#!/usr/bin/env python3
"""Generate complete elaborated AutoLink parameters."""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Mapping

import yaml

from generate_gemmini_ext_spm import require_int, require_mapping, write

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOC = ROOT / "configs" / "soc" / "autolink" / "gc.yaml"
GENERATED_DIR = (
    ROOT
    / "chipyard"
    / "generators"
    / "chipyard"
    / "src"
    / "main"
    / "scala"
    / "socgen"
    / "generated"
)
DEFAULT_OUTPUT = GENERATED_DIR / "AutoLinkGenerated.scala"
DEFAULT_HEADER = ROOT / "tests" / "include" / "auto_link_generated.h"
DEFAULT_CGRA_SCALA = (
    ROOT
    / "chipyard"
    / "generators"
    / "chipyard"
    / "src"
    / "main"
    / "scala"
    / "example"
    / "CGRAGenerated.scala"
)
DEFAULT_CACHE_BLOCK_SCALA = (
    ROOT
    / "chipyard"
    / "generators"
    / "rocket-chip"
    / "src"
    / "main"
    / "scala"
    / "subsystem"
    / "BankedCoherenceParams.scala"
)
INPUT_READY = "input_ready"
STAGE_NAME = re.compile(r"[a-z][a-z0-9_]{0,31}\Z")
CAPABILITIES = {
    "gemmini": {"source", "destination"},
    "cgra": {"source", "destination"},
    "aes": {"source", "destination"},
    "pool": {"destination"},
}
STAGE_KEYS = {"name", "endpoint"}
DEPENDENCY_KEYS = {
    "source",
    "destination",
    "size_bytes",
    "source_offset",
    "destination_offset",
    "format",
}
INT8_FORMAT = "int8"
CGRA_WORD_BYTES = 4


@dataclass(frozen=True)
class Memory:
    base: int
    size: int


@dataclass(frozen=True)
class Stage:
    name: str
    endpoint: str
    job: int


@dataclass(frozen=True)
class Copy:
    source_offset: int
    destination_offset: int
    size: int
    format: str | None


@dataclass(frozen=True)
class Dependency:
    source: str | None
    destination: str
    copy: Copy | None


@dataclass(frozen=True)
class CgraHardware:
    dma_beat_bytes: int
    cache_block_bytes: int


@dataclass(frozen=True)
class CgraBridge:
    packed_base: int
    packed_bytes: int
    packed_window_bytes: int
    outbound_word: int
    outbound_words: int
    inbound_word: int
    inbound_words: int


@dataclass(frozen=True)
class AutoLinkConfig:
    gemmini: Memory
    cgra: Memory
    stages: tuple[Stage, ...]
    dependencies: tuple[Dependency, ...]
    bridge: CgraBridge | None


def load_cgra_hardware(cgra_path: Path, cache_block_path: Path) -> CgraHardware:
    cgra_text = cgra_path.read_text(encoding="utf-8")
    dma = re.search(r"dma\s*=\s*CGRADMAParams\((?P<body>.*?)\n\s*\)", cgra_text, re.S)
    if dma is None:
        raise ValueError(f"{cgra_path}: missing generated CGRA DMA metadata")
    data_width = re.search(r"dramDataWidth\s*=\s*(\d+)", dma.group("body"))
    if (
        data_width is None
        or int(data_width.group(1)) <= 0
        or int(data_width.group(1)) % 8 != 0
    ):
        raise ValueError(f"{cgra_path}: invalid generated CGRA DMA data width")
    dma_beat_bytes = int(data_width.group(1)) // 8
    if dma_beat_bytes & (dma_beat_bytes - 1):
        raise ValueError(f"{cgra_path}: CGRA DMA beat size must be a power of two")

    cache_text = cache_block_path.read_text(encoding="utf-8")
    cache_block = re.search(
        r"case object CacheBlockBytes extends Field\[Int\]\((\d+)\)", cache_text
    )
    if cache_block is None:
        raise ValueError(f"{cache_block_path}: missing cache-block size")
    cache_block_bytes = int(cache_block.group(1))
    if cache_block_bytes <= 0 or cache_block_bytes & (cache_block_bytes - 1):
        raise ValueError(f"{cache_block_path}: cache-block size must be a power of two")
    return CgraHardware(dma_beat_bytes, cache_block_bytes)


def next_power_of_two(value: int) -> int:
    return 1 << (value - 1).bit_length()


def load_memory(data: Mapping[str, object], key: str, path: Path) -> Memory:
    value = require_mapping(data, key, path)
    memory = Memory(
        base=require_int(value, "base_address", path),
        size=require_int(value, "size_bytes", path),
    )
    if memory.base < 0 or memory.size <= 0:
        raise ValueError(f"{path}: '{key}' needs a nonnegative base and positive size")
    return memory


def load_stages(values: object, path: Path) -> tuple[Stage, ...]:
    if not isinstance(values, list) or not values:
        raise ValueError(f"{path}: 'stages' must be a non-empty list")
    stages = []
    jobs: dict[str, int] = {}
    names = set()
    for index, value in enumerate(values):
        if not isinstance(value, Mapping):
            raise ValueError(f"{path}: stages[{index}] must be a mapping")
        if set(value) - STAGE_KEYS:
            raise ValueError(f"{path}: stages[{index}] has unsupported fields")
        name = value.get("name")
        endpoint = value.get("endpoint")
        if (
            not isinstance(name, str)
            or STAGE_NAME.fullmatch(name) is None
            or name == INPUT_READY
        ):
            raise ValueError(f"{path}: stages[{index}].name is invalid")
        if not isinstance(endpoint, str) or endpoint not in CAPABILITIES:
            raise ValueError(f"{path}: stages[{index}] has an unknown endpoint")
        if name in names:
            raise ValueError(f"{path}: duplicate stage '{name}'")
        job = jobs.get(endpoint, 0)
        stages.append(Stage(name, endpoint, job))
        jobs[endpoint] = job + 1
        names.add(name)
    return tuple(stages)


def load_offset(value: object, field: str, path: Path, index: int) -> int | None:
    if value is None:
        return None
    if not isinstance(value, int) or isinstance(value, bool) or value < 0:
        raise ValueError(f"{path}: dependencies[{index}].{field} must be nonnegative")
    return value


def load_dependencies(
    values: object,
    stages: tuple[Stage, ...],
    memories: Mapping[str, Memory],
    path: Path,
) -> tuple[Dependency, ...]:
    if not isinstance(values, list) or not values:
        raise ValueError(f"{path}: 'dependencies' must be a non-empty list")
    stage_map = {stage.name: stage for stage in stages}
    dependencies = []
    edges = set()
    for index, value in enumerate(values):
        if not isinstance(value, Mapping):
            raise ValueError(f"{path}: dependencies[{index}] must be a mapping")
        if set(value) - DEPENDENCY_KEYS:
            raise ValueError(f"{path}: dependencies[{index}] has unsupported fields")
        source = value.get("source")
        destination = value.get("destination")
        if (
            not isinstance(source, str)
            or source not in stage_map
            and source != INPUT_READY
        ):
            raise ValueError(f"{path}: dependencies[{index}] has an unknown source")
        if not isinstance(destination, str) or destination not in stage_map:
            raise ValueError(
                f"{path}: dependencies[{index}] has an unknown destination"
            )
        if source == destination:
            raise ValueError(f"{path}: stage '{source}' cannot depend on itself")
        edge = (source, destination)
        if edge in edges:
            raise ValueError(f"{path}: duplicate dependency {source} -> {destination}")
        edges.add(edge)
        destination_stage = stage_map[destination]
        if "destination" not in CAPABILITIES[destination_stage.endpoint]:
            raise ValueError(
                f"{path}: endpoint '{destination_stage.endpoint}' cannot be a destination"
            )

        source_offset = load_offset(
            value.get("source_offset"), "source_offset", path, index
        )
        destination_offset = load_offset(
            value.get("destination_offset"), "destination_offset", path, index
        )
        size = value.get("size_bytes")
        tensor_format = value.get("format")
        if tensor_format is not None and tensor_format != INT8_FORMAT:
            raise ValueError(
                f"{path}: dependencies[{index}].format must be '{INT8_FORMAT}'"
            )
        if size is None:
            if (
                source_offset is not None
                or destination_offset is not None
                or tensor_format is not None
            ):
                raise ValueError(
                    f"{path}: dependencies[{index}] copy fields require size_bytes"
                )
            copy = None
        else:
            if source == INPUT_READY:
                raise ValueError(f"{path}: input_ready cannot carry data")
            if not isinstance(size, int) or isinstance(size, bool) or size <= 0:
                raise ValueError(
                    f"{path}: dependencies[{index}].size_bytes must be positive"
                )
            source_stage = stage_map[source]
            if "source" not in CAPABILITIES[source_stage.endpoint]:
                raise ValueError(
                    f"{path}: endpoint '{source_stage.endpoint}' cannot be a source"
                )
            source_memory = memories.get(source_stage.endpoint)
            if source_offset is None:
                source_offset = (
                    source_memory.size - size
                    if source_stage.endpoint == "gemmini"
                    else 0
                )
            if source_offset < 0:
                raise ValueError(f"{path}: dependencies[{index}] exceeds source memory")
            if destination_offset is None:
                destination_offset = 0
            copy = Copy(source_offset, destination_offset, size, tensor_format)
        dependencies.append(
            Dependency(None if source == INPUT_READY else source, destination, copy)
        )
    return tuple(dependencies)


def infer_bridge(
    stages: tuple[Stage, ...],
    dependencies: tuple[Dependency, ...],
    cgra: Memory,
    gemmini: Memory,
    hardware: CgraHardware,
    path: Path,
) -> CgraBridge | None:
    stage_map = {stage.name: stage for stage in stages}
    transformed = [
        dependency
        for dependency in dependencies
        if dependency.copy is not None and dependency.copy.format == INT8_FORMAT
    ]
    if not transformed:
        return None
    outbound = [
        dependency
        for dependency in transformed
        if dependency.source is not None
        and stage_map[dependency.source].endpoint == "cgra"
        and stage_map[dependency.destination].endpoint != "cgra"
    ]
    inbound = [
        dependency
        for dependency in transformed
        if dependency.source is not None
        and stage_map[dependency.source].endpoint != "cgra"
        and stage_map[dependency.destination].endpoint == "cgra"
    ]
    if len(transformed) != 2 or len(outbound) != 1 or len(inbound) != 1:
        raise ValueError(
            f"{path}: int8 bridge needs one CGRA input and one CGRA output"
        )

    out_copy = outbound[0].copy
    in_copy = inbound[0].copy
    if out_copy.source_offset % CGRA_WORD_BYTES != 0:
        raise ValueError(f"{path}: int8 output source offset must be word aligned")
    if in_copy.destination_offset % CGRA_WORD_BYTES != 0:
        raise ValueError(f"{path}: int8 input destination offset must be word aligned")
    if in_copy.size % CGRA_WORD_BYTES != 0:
        raise ValueError(f"{path}: int8 input size must contain whole INT32 words")

    outbound_word = out_copy.source_offset // CGRA_WORD_BYTES
    outbound_words = out_copy.size
    inbound_word = in_copy.destination_offset // CGRA_WORD_BYTES
    inbound_words = in_copy.size // CGRA_WORD_BYTES
    if (outbound_word + outbound_words) * CGRA_WORD_BYTES > cgra.size:
        raise ValueError(f"{path}: int8 output exceeds CGRA SPM")
    if (inbound_word + inbound_words) * CGRA_WORD_BYTES > cgra.size:
        raise ValueError(f"{path}: int8 input exceeds CGRA SPM")
    if (
        outbound_word < inbound_word + inbound_words
        and inbound_word < outbound_word + outbound_words
    ):
        raise ValueError(f"{path}: int8 input and output SPM ranges overlap")

    packed_base = cgra.base + cgra.size
    packed_bytes = outbound_words
    packed_window_bytes = next_power_of_two(
        max(packed_bytes, hardware.cache_block_bytes)
    )
    if packed_base % packed_window_bytes != 0:
        raise ValueError(f"{path}: packed CGRA alias base is not window aligned")
    if (
        packed_base < gemmini.base + gemmini.size
        and gemmini.base < packed_base + packed_window_bytes
    ):
        raise ValueError(f"{path}: packed CGRA alias overlaps Gemmini SPM")
    return CgraBridge(
        packed_base,
        packed_bytes,
        packed_window_bytes,
        outbound_word,
        outbound_words,
        inbound_word,
        inbound_words,
    )


def validate_copy_alignment(
    dependencies: tuple[Dependency, ...], beat_bytes: int, path: Path
) -> None:
    for index, dependency in enumerate(dependencies):
        if dependency.copy is None:
            continue
        copy = dependency.copy
        fields = {
            "source_offset": copy.source_offset,
            "destination_offset": copy.destination_offset,
            "size_bytes": copy.size,
        }
        for name, value in fields.items():
            if value % beat_bytes != 0:
                raise ValueError(
                    f"{path}: dependencies[{index}].{name} must align to the {beat_bytes}-byte CGRA DMA beat"
                )


def validate_graph(
    stages: tuple[Stage, ...], dependencies: tuple[Dependency, ...], path: Path
) -> None:
    incoming = {stage.name: [] for stage in stages}
    outgoing = {stage.name: [] for stage in stages}
    for dependency in dependencies:
        incoming[dependency.destination].append(dependency.source)
        if dependency.source is not None:
            outgoing[dependency.source].append(dependency)

    external = [stage.name for stage in stages if not incoming[stage.name]]
    for name in external:
        if not any(dependency.copy is not None for dependency in outgoing[name]):
            raise ValueError(f"{path}: external stage '{name}' needs a copied output")

    publications = {stage.name: set() for stage in stages}
    for dependency in dependencies:
        if dependency.source is not None and dependency.copy is not None:
            publications[dependency.source].add(
                (dependency.copy.source_offset, dependency.copy.size)
            )
    for name, ranges in publications.items():
        if len(ranges) > 1:
            raise ValueError(f"{path}: stage '{name}' has ambiguous output ranges")

    degree = {
        stage.name: sum(source is not None for source in incoming[stage.name])
        for stage in stages
    }
    ready = [name for name, count in degree.items() if count == 0]
    visited = set()
    while ready:
        name = ready.pop()
        visited.add(name)
        for dependency in outgoing[name]:
            degree[dependency.destination] -= 1
            if degree[dependency.destination] == 0:
                ready.append(dependency.destination)
    if len(visited) != len(stages):
        names = ", ".join(sorted(set(degree) - visited))
        raise ValueError(f"{path}: cyclic stages: {names}")


def validate_destinations(
    stages: tuple[Stage, ...], dependencies: tuple[Dependency, ...], path: Path
) -> None:
    for stage in stages:
        ranges = sorted(
            (
                dependency.copy.destination_offset,
                dependency.copy.destination_offset + dependency.copy.size,
            )
            for dependency in dependencies
            if dependency.destination == stage.name and dependency.copy is not None
        )
        for first, second in zip(ranges, ranges[1:]):
            if second[0] < first[1]:
                raise ValueError(
                    f"{path}: stage '{stage.name}' has overlapping input ranges"
                )


def validate_memory(config: AutoLinkConfig, path: Path) -> None:
    memories = {"gemmini": config.gemmini, "cgra": config.cgra}
    stage_map = {stage.name: stage for stage in config.stages}
    for dependency in config.dependencies:
        if dependency.copy is None or dependency.source is None:
            continue
        copy = dependency.copy
        source = stage_map[dependency.source].endpoint
        destination = stage_map[dependency.destination].endpoint
        source_memory = memories.get(source)
        if (
            source_memory is not None
            and copy.source_offset + copy.size > source_memory.size
        ):
            raise ValueError(
                f"{path}: {dependency.source} output exceeds source memory"
            )
        destination_memory = memories.get(destination)
        if (
            destination_memory is not None
            and copy.destination_offset + copy.size > destination_memory.size
        ):
            raise ValueError(
                f"{path}: {dependency.destination} input exceeds destination memory"
            )


def load_config(
    path: Path,
    cgra_path: Path = DEFAULT_CGRA_SCALA,
    cache_block_path: Path = DEFAULT_CACHE_BLOCK_SCALA,
) -> AutoLinkConfig:
    hardware = load_cgra_hardware(cgra_path, cache_block_path)
    document = yaml.safe_load(path.read_text(encoding="utf-8"))
    if not isinstance(document, Mapping):
        raise ValueError(f"{path}: expected a mapping")
    memory = require_mapping(document, "memory", path)
    communication = require_mapping(document, "communication", path)
    gemmini = load_memory(memory, "gemmini_external_spm", path)
    cgra = load_memory(memory, "cgra_spm_window", path)
    stages = load_stages(communication.get("stages"), path)
    memories = {"gemmini": gemmini, "cgra": cgra}
    dependencies = load_dependencies(
        communication.get("dependencies"), stages, memories, path
    )
    validate_copy_alignment(dependencies, hardware.dma_beat_bytes, path)
    validate_graph(stages, dependencies, path)
    validate_destinations(stages, dependencies, path)
    bridge = infer_bridge(stages, dependencies, cgra, gemmini, hardware, path)
    config = AutoLinkConfig(gemmini, cgra, stages, dependencies, bridge)
    validate_memory(config, path)
    return config


def buffer_text(memory: Memory) -> str:
    return f'Some(AutoBuffer(BigInt("{memory.base:x}", 16), {memory.size}))'


def copied_dependencies(config: AutoLinkConfig) -> list[Dependency]:
    return [dependency for dependency in config.dependencies if dependency.copy]


def stage_map(config: AutoLinkConfig) -> dict[str, Stage]:
    return {stage.name: stage for stage in config.stages}


def gemmini_endpoint(config: AutoLinkConfig) -> str:
    return (
        '      AutoEndpointSpec(name = "gemmini", buffer = '
        f"{buffer_text(config.gemmini)}, localBytes = {config.gemmini.size})"
    )


def cgra_endpoint(config: AutoLinkConfig) -> str:
    copies = [dependency for dependency in config.dependencies if dependency.copy]
    stages = stage_map(config)
    is_source = any(
        dependency.source is not None and stages[dependency.source].endpoint == "cgra"
        for dependency in copies
    )
    if is_source:
        size = config.cgra.size + (
            config.bridge.packed_window_bytes if config.bridge is not None else 0
        )
        return (
            '      AutoEndpointSpec(name = "cgra", buffer = '
            f"{buffer_text(Memory(config.cgra.base, size))}, localBytes = {config.cgra.size})"
        )
    local_bytes = (
        "CGRAGenerated.params.dma.spmWords * "
        "CGRAGenerated.params.dataPayloadWidth / 8"
    )
    return f'      AutoEndpointSpec(name = "cgra", buffer = None, localBytes = {local_bytes})'


def aes_endpoint(config: AutoLinkConfig) -> str:
    copies = copied_dependencies(config)
    stages = stage_map(config)
    source_spans = [
        dependency.copy.source_offset + dependency.copy.size
        for dependency in copies
        if dependency.source is not None and stages[dependency.source].endpoint == "aes"
    ]
    input_sizes = [
        dependency.copy.destination_offset + dependency.copy.size
        for dependency in copies
        if stages[dependency.destination].endpoint == "aes"
    ]
    buffer = (
        buffer_text(Memory(config.gemmini.base, max(source_spans)))
        if source_spans
        else "None"
    )
    local_bytes = max(input_sizes, default=0)
    return f'      AutoEndpointSpec(name = "aes", buffer = {buffer}, localBytes = {local_bytes})'


def pool_endpoint(config: AutoLinkConfig) -> str:
    copies = copied_dependencies(config)
    stages = stage_map(config)
    sizes = [
        dependency.copy.destination_offset + dependency.copy.size
        for dependency in copies
        if stages[dependency.destination].endpoint == "pool"
    ]
    return f'      AutoEndpointSpec(name = "pool", buffer = None, localBytes = {max(sizes)})'


ENDPOINT_TEXT = {
    "gemmini": gemmini_endpoint,
    "cgra": cgra_endpoint,
    "aes": aes_endpoint,
    "pool": pool_endpoint,
}


def endpoint_text(config: AutoLinkConfig, name: str) -> str:
    return ENDPOINT_TEXT[name](config)


def copy_text(config: AutoLinkConfig, dependency: Dependency) -> str:
    copy = dependency.copy
    if copy is None:
        return "None"
    source_offset = copy.source_offset
    if copy.format == INT8_FORMAT:
        source = stage_map(config)[dependency.source]
        if source.endpoint == "cgra":
            source_offset = config.bridge.packed_base - config.cgra.base
    return (
        "Some(AutoCopySpec("
        f"sourceOffset = {source_offset}, destinationOffset = {copy.destination_offset}, "
        f"bytes = {copy.size}))"
    )


def scala_text(config: AutoLinkConfig) -> str:
    stage_index = {stage.name: index for index, stage in enumerate(config.stages)}
    names = []
    for stage in config.stages:
        if stage.endpoint not in names:
            names.append(stage.endpoint)
    stages = ",\n".join(
        f'      AutoStageSpec(name = "{stage.name}", endpoint = "{stage.endpoint}", job = {stage.job})'
        for stage in config.stages
    )
    dependencies = ",\n".join(
        "      AutoDependencySpec("
        f"source = {'None' if dependency.source is None else f'Some({stage_index[dependency.source]})'}, "
        f"destination = {stage_index[dependency.destination]}, copy = {copy_text(config, dependency)})"
        for dependency in config.dependencies
    )
    endpoints = ",\n".join(endpoint_text(config, name) for name in names)
    return f"""package chipyard.socgen.generated

import chipyard.example.CGRAGenerated
import chipyard.socgen.link._

// Generated by scripts/generate_auto_links.py. Do not edit.
object AutoLinkGenerated {{
  val params = AutoLinkParams(
    stages = Seq(
{stages}),
    dependencies = Seq(
{dependencies}),
    endpoints = Seq(
{endpoints}),
    beatBytes = CGRAGenerated.params.dma.dramDataWidth / 8,
    controlAddress = CgraLinkControlGenerated.autoLinkAddress,
    controlBytes = CgraLinkControlGenerated.pageSizeBytes)
}}
"""


def header_text(config: AutoLinkConfig) -> str:
    constants = "\n".join(
        f"#define AUTO_LINK_STAGE_{stage.name.upper()} {index}u"
        for index, stage in enumerate(config.stages)
    )
    jobs = "\n".join(
        f"#define AUTO_LINK_JOB_{stage.name.upper()} {stage.job}u"
        for stage in config.stages
    )
    return f"""/* Generated by scripts/generate_auto_links.py. Do not edit. */
#ifndef AUTO_LINK_GENERATED_H
#define AUTO_LINK_GENERATED_H

{constants}

{jobs}

#endif
"""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--soc-yaml", type=Path, default=DEFAULT_SOC)
    parser.add_argument("--cgra-scala", type=Path, default=DEFAULT_CGRA_SCALA)
    parser.add_argument(
        "--cache-block-scala", type=Path, default=DEFAULT_CACHE_BLOCK_SCALA
    )
    parser.add_argument("--scala-out", type=Path)
    parser.add_argument("--header-out", type=Path)
    parser.add_argument("--check", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    path = args.soc_yaml.resolve()
    config = load_config(
        path, args.cgra_scala.resolve(), args.cache_block_scala.resolve()
    )
    write(args.scala_out or DEFAULT_OUTPUT, scala_text(config), args.check)
    if args.header_out is not None or args.scala_out is None:
        write(args.header_out or DEFAULT_HEADER, header_text(config), args.check)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
