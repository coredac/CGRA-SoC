#!/usr/bin/env python3
"""Generate complete elaborated AutoLink parameters."""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Mapping

import yaml

from generate_aes_auto_job import OUTPUT_BYTES as AES_JOB_BYTES
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
}


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


@dataclass(frozen=True)
class Dependency:
    source: str | None
    destination: str
    copy: Copy | None


@dataclass(frozen=True)
class AutoLinkConfig:
    gemmini: Memory
    cgra: Memory
    stages: tuple[Stage, ...]
    dependencies: tuple[Dependency, ...]


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
        if size is None:
            if source_offset is not None or destination_offset is not None:
                raise ValueError(
                    f"{path}: dependencies[{index}] offsets require size_bytes"
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
            if destination_stage.endpoint == "aes" and size != AES_JOB_BYTES:
                raise ValueError(
                    f"{path}: AES dependency must be {AES_JOB_BYTES} bytes"
                )
            copy = Copy(source_offset, destination_offset, size)
        dependencies.append(
            Dependency(None if source == INPUT_READY else source, destination, copy)
        )
    return tuple(dependencies)


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


def load_config(path: Path) -> AutoLinkConfig:
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
    validate_graph(stages, dependencies, path)
    validate_destinations(stages, dependencies, path)
    config = AutoLinkConfig(gemmini, cgra, stages, dependencies)
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
        return (
            '      AutoEndpointSpec(name = "cgra", buffer = '
            f"{buffer_text(config.cgra)}, localBytes = {config.cgra.size})"
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


def copy_text(copy: Copy | None) -> str:
    if copy is None:
        return "None"
    return (
        "Some(AutoCopySpec("
        f"sourceOffset = {copy.source_offset}, destinationOffset = {copy.destination_offset}, "
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
        f"destination = {stage_index[dependency.destination]}, copy = {copy_text(dependency.copy)})"
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
    return f"""/* Generated by scripts/generate_auto_links.py. Do not edit. */
#ifndef AUTO_LINK_GENERATED_H
#define AUTO_LINK_GENERATED_H

{constants}

#endif
"""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--soc-yaml", type=Path, default=DEFAULT_SOC)
    parser.add_argument("--scala-out", type=Path)
    parser.add_argument("--header-out", type=Path)
    parser.add_argument("--check", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    path = args.soc_yaml.resolve()
    config = load_config(path)
    write(args.scala_out or DEFAULT_OUTPUT, scala_text(config), args.check)
    if args.header_out is not None or args.scala_out is None:
        write(args.header_out or DEFAULT_HEADER, header_text(config), args.check)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
