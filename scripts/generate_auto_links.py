#!/usr/bin/env python3
"""Generate AutoLink hardware capacities and loadable YAML task graphs."""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass, replace
from pathlib import Path
from typing import Mapping

import yaml

from generate_accels import Accel, instance_layout
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
    "pool": {"source", "destination"},
}
STAGE_KEYS = {"name", "endpoint", "jobs", "output"}
DEPENDENCY_KEYS = {
    "source",
    "destination",
    "size_bytes",
    "source_offset",
    "destination_offset",
    "format",
    "source_format",
    "buffer",
}
INT8_FORMAT = "int8"
CGRA_WORD_BYTES = 4


@dataclass(frozen=True)
class Memory:
    base: int
    size: int


@dataclass(frozen=True)
class Buffer:
    address: int | str
    size: int
    format: str | None = None
    stride: int = 0
    pixel_bytes: int = 0


@dataclass(frozen=True)
class Output:
    buffer: str
    address: int | str
    stride: int
    size: int
    pixel_bytes: int


@dataclass(frozen=True)
class Stage:
    name: str
    endpoint: str
    job: int
    jobs: int = 1
    output: Output | None = None


@dataclass(frozen=True)
class Copy:
    source_offset: int
    destination_offset: int
    size: int
    format: str | None
    source_format: str | None = None
    source_address: int | str | None = None
    buffer: str | None = None
    source_stride: int = 0
    pixel_bytes: int = 0

    @property
    def destination_size(self) -> int:
        return self.size * (CGRA_WORD_BYTES if self.source_format == INT8_FORMAT else 1)


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
    window_bytes: int
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
    buffer_slots: dict[str, int]
    instances: dict[str, Accel]
    bridges: dict[str, CgraBridge | None]
    run_capacity: int = 0
    stage_capacity: int = 0
    dependency_capacity: int = 0
    jobs: dict[str, int] | None = None


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


def load_buffers(values: Mapping[str, object], path: Path) -> dict[str, Buffer]:
    buffers = {}
    for name, fields in values.items():
        tensor_format = fields.get("format")
        if tensor_format not in (None, "int8", "int32"):
            raise ValueError(f"{path}: buffer '{name}' format must be int8 or int32")
        size = require_int(fields, "size_bytes", path)
        if size <= 0:
            raise ValueError(f"{path}: buffer '{name}' size_bytes must be positive")
        buffers[name] = Buffer(
            load_address(fields["address"], path),
            size,
            tensor_format,
            fields.get("stride", 0),
            fields.get("bytes_per_pixel", 0),
        )
    return buffers


def load_output(
    fields: Mapping[str, object], buffers: Mapping[str, Buffer], kind: str, path: Path
) -> Output:
    if kind == "cgra":
        raise ValueError(
            f"{path}: CGRA outputs stay in local SPM; automatic writeback is unsupported"
        )
    if set(fields) != {"buffer"}:
        raise ValueError(f"{path}: output accepts only a buffer reference")
    name = fields["buffer"]
    buffer = buffers[name]
    return Output(
        name,
        buffer.address,
        buffer.stride,
        buffer.size,
        buffer.pixel_bytes,
    )


def load_stages(
    values: object,
    instances: Mapping[str, Accel],
    buffers: Mapping[str, Buffer],
    path: Path,
) -> tuple[Stage, ...]:
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
        if not isinstance(endpoint, str) or endpoint not in instances:
            raise ValueError(f"{path}: stages[{index}] has an unknown endpoint")
        if name in names:
            raise ValueError(f"{path}: duplicate stage '{name}'")
        job = jobs.get(endpoint, 0)
        count = value.get("jobs", 1)
        output = None
        if "output" in value:
            output = load_output(
                require_mapping(value, "output", path),
                buffers,
                instances[endpoint].kind,
                path,
            )
        stages.append(Stage(name, endpoint, job, count, output))
        jobs[endpoint] = job + count
        names.add(name)
    return tuple(stages)


def load_address(value: object, path: Path) -> int | str:
    if isinstance(value, int) and not isinstance(value, bool) and value >= 0:
        return value
    if isinstance(value, str) and STAGE_NAME.fullmatch(value):
        return value
    raise ValueError(f"{path}: address must be a physical integer or binding name")


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
    instances: Mapping[str, Accel],
    buffers: Mapping[str, Buffer],
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
        if (
            "destination"
            not in CAPABILITIES[instances[destination_stage.endpoint].kind]
        ):
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
        source_format = value.get("source_format")
        buffer_name = value.get("buffer")
        buffer = buffers[buffer_name] if buffer_name is not None else None
        source_address = buffer.address if buffer else None
        if buffer is not None:
            if tensor_format is not None or source_format is not None:
                raise ValueError(f"{path}: buffer dependencies inherit their format")
            size = buffer.size if size is None else size
            if instances[destination_stage.endpoint].kind == "cgra":
                source_format = INT8_FORMAT if buffer.format == INT8_FORMAT else None
            if (source_offset or 0) + size > buffer.size:
                raise ValueError(f"{path}: dependency exceeds buffer '{buffer_name}'")
        if tensor_format is not None and tensor_format != INT8_FORMAT:
            raise ValueError(
                f"{path}: dependencies[{index}].format must be '{INT8_FORMAT}'"
            )
        if source_format is not None:
            if source_format != INT8_FORMAT:
                raise ValueError(
                    f"{path}: dependencies[{index}].source_format must be '{INT8_FORMAT}'"
                )
            if instances[destination_stage.endpoint].kind != "cgra":
                raise ValueError(
                    f"{path}: dependencies[{index}].source_format requires a CGRA destination"
                )
            if tensor_format is not None:
                raise ValueError(
                    f"{path}: dependencies[{index}] cannot combine format and source_format"
                )
        if size is None:
            if (
                source_offset is not None
                or destination_offset is not None
                or tensor_format is not None
                or source_format is not None
                or source_address is not None
            ):
                raise ValueError(
                    f"{path}: dependencies[{index}] copy fields require size_bytes"
                )
            copy = None
        else:
            if source == INPUT_READY and source_address is None:
                raise ValueError(f"{path}: input_ready data requires a buffer")
            if not isinstance(size, int) or isinstance(size, bool) or size <= 0:
                raise ValueError(
                    f"{path}: dependencies[{index}].size_bytes must be positive"
                )
            source_stage = stage_map.get(source)
            source_memory = (
                memories.get(source_stage.endpoint) if source_stage else None
            )
            if (
                source_stage
                and instances[source_stage.endpoint].kind == "pool"
                and source_address is None
            ):
                raise ValueError(f"{path}: Pool output requires a buffer")
            if source_offset is None:
                source_offset = (
                    source_memory.size - size
                    if source_address is None
                    and source_stage
                    and instances[source_stage.endpoint].kind == "gemmini"
                    else 0
                )
            if source_offset < 0:
                raise ValueError(f"{path}: dependencies[{index}] exceeds source memory")
            if destination_offset is None:
                destination_offset = 0
            copy = Copy(
                source_offset,
                destination_offset,
                size,
                tensor_format,
                source_format,
                source_address,
                buffer_name,
                buffer.stride if buffer else 0,
                buffer.pixel_bytes if buffer and "size_bytes" not in value else 0,
            )
        dependencies.append(
            Dependency(None if source == INPUT_READY else source, destination, copy)
        )
    return tuple(dependencies)


def infer_bridge(
    stages: tuple[Stage, ...],
    dependencies: tuple[Dependency, ...],
    cgra: Memory,
    memories: Mapping[str, Memory],
    hardware: CgraHardware,
    path: Path,
    packed_words: int | None = None,
    name: str = "cgra",
    output_format: str | None = None,
) -> CgraBridge | None:
    stage_map = {stage.name: stage for stage in stages}
    transformed = [
        dependency
        for dependency in dependencies
        if dependency.copy is not None
        and dependency.copy.format == INT8_FORMAT
        and (
            stage_map[dependency.destination].endpoint == name
            or dependency.source is not None
            and stage_map[dependency.source].endpoint == name
        )
    ]
    if not transformed and output_format != INT8_FORMAT:
        if packed_words is not None:
            raise ValueError(f"{path}: packed_words requires a CGRA INT8 output")
        return None
    outbound = [
        dependency
        for dependency in transformed
        if dependency.source is not None
        and stage_map[dependency.source].endpoint == name
        and stage_map[dependency.destination].endpoint != name
    ]
    inbound = [
        dependency
        for dependency in transformed
        if dependency.source is not None
        and stage_map[dependency.source].endpoint != name
        and stage_map[dependency.destination].endpoint == name
    ]
    if (not outbound and output_format != INT8_FORMAT) or len(transformed) != len(
        outbound
    ) + len(inbound):
        raise ValueError(
            f"{path}: int8 transformations require a CGRA output and CGRA input/output edges"
        )

    if any(dependency.copy.source_offset % CGRA_WORD_BYTES for dependency in outbound):
        raise ValueError(f"{path}: int8 output source offset must be word aligned")
    if any(
        dependency.copy.destination_offset % CGRA_WORD_BYTES for dependency in inbound
    ):
        raise ValueError(f"{path}: int8 input destination offset must be word aligned")
    if any(dependency.copy.size % CGRA_WORD_BYTES for dependency in inbound):
        raise ValueError(f"{path}: int8 input size must contain whole INT32 words")

    output_ranges = [
        (dependency.copy.source_offset // CGRA_WORD_BYTES, dependency.copy.size)
        for dependency in outbound
    ]
    outbound_word = min((start for start, _ in output_ranges), default=0)
    output_span = (
        max((start + size for start, size in output_ranges), default=0) - outbound_word
    )
    outbound_words = (
        output_span or cgra.size // CGRA_WORD_BYTES
        if packed_words is None
        else packed_words
    )
    if outbound_words < output_span:
        raise ValueError(f"{path}: packed_words does not cover declared CGRA outputs")
    input_ranges = {
        (
            dependency.copy.destination_offset // CGRA_WORD_BYTES,
            dependency.copy.size // CGRA_WORD_BYTES,
        )
        for dependency in inbound
    }
    if len(input_ranges) > 1:
        raise ValueError(
            f"{path}: raw INT8 requantization supports one CGRA input region"
        )
    inbound_word, inbound_words = next(iter(input_ranges), (0, 0))
    if (outbound_word + outbound_words) * CGRA_WORD_BYTES > cgra.size:
        raise ValueError(f"{path}: int8 output exceeds CGRA SPM")
    if (inbound_word + inbound_words) * CGRA_WORD_BYTES > cgra.size:
        raise ValueError(f"{path}: int8 input exceeds CGRA SPM")
    if (
        inbound_words != 0
        and outbound_word < inbound_word + inbound_words
        and inbound_word < outbound_word + outbound_words
    ):
        raise ValueError(f"{path}: int8 input and output SPM ranges overlap")

    if any(
        dependency.source is not None
        and stage_map[dependency.source].endpoint == name
        and dependency.copy is not None
        and dependency.copy.format != INT8_FORMAT
        and dependency.copy.source_address is None
        for dependency in dependencies
    ):
        raise ValueError(
            f"{path}: CGRA has one output window; int8 and raw outputs cannot be mixed"
        )

    window_bytes = next_power_of_two(max(outbound_words, hardware.cache_block_bytes))
    if cgra.base % window_bytes != 0:
        raise ValueError(f"{path}: CGRA window base is not window aligned")
    for other, memory in memories.items():
        if (
            other != name
            and cgra.base < memory.base + memory.size
            and memory.base < cgra.base + window_bytes
        ):
            raise ValueError(f"{path}: CGRA window overlaps {other} SPM")
    return CgraBridge(
        window_bytes,
        outbound_word,
        outbound_words,
        inbound_word,
        inbound_words,
    )


def validate_copy_alignment(
    stages: tuple[Stage, ...],
    dependencies: tuple[Dependency, ...],
    beat_bytes: int,
    instances: Mapping[str, Accel],
    path: Path,
) -> None:
    stage_map = {stage.name: stage for stage in stages}
    for index, dependency in enumerate(dependencies):
        if dependency.copy is None:
            continue
        copy = dependency.copy
        if (
            copy.format == INT8_FORMAT
            and instances[stage_map[dependency.source].endpoint].kind == "cgra"
        ):
            continue
        if copy.source_format == INT8_FORMAT:
            if copy.destination_offset % CGRA_WORD_BYTES != 0:
                raise ValueError(
                    f"{path}: dependencies[{index}].destination_offset must be word aligned"
                )
            continue
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
    endpoints = {stage.name: stage.endpoint for stage in stages}
    incoming = {stage.name: [] for stage in stages}
    outgoing = {stage.name: [] for stage in stages}
    for dependency in dependencies:
        incoming[dependency.destination].append(dependency.source)
        if dependency.source is not None:
            if (
                dependency.copy is not None
                and endpoints[dependency.source] == endpoints[dependency.destination]
            ):
                raise ValueError(
                    f"{path}: data dependencies between stages on the same physical endpoint are unsupported"
                )
            outgoing[dependency.source].append(dependency)

    external = [stage.name for stage in stages if not incoming[stage.name]]
    for name in external:
        if not any(dependency.copy is not None for dependency in outgoing[name]):
            raise ValueError(f"{path}: external stage '{name}' needs a copied output")

    publications = {stage.name: set() for stage in stages}
    outputs = {stage.name: stage.output for stage in stages}
    writers = [stage.output.buffer for stage in stages if stage.output is not None]
    if len(writers) != len(set(writers)):
        raise ValueError(f"{path}: a buffer must have only one producer")
    for dependency in dependencies:
        if dependency.source is not None and dependency.copy is not None:
            publications[dependency.source].add(
                (
                    dependency.copy.source_address,
                    dependency.copy.source_offset,
                    dependency.copy.size,
                )
            )
    for name, ranges in publications.items():
        if outputs[name] is None and len(ranges) > 1:
            raise ValueError(f"{path}: stage '{name}' has ambiguous output ranges")

    for dependency in dependencies:
        if dependency.source is None or dependency.copy is None:
            continue
        output = outputs[dependency.source]
        buffer = dependency.copy.buffer
        if (output is not None and buffer != output.buffer) or (
            buffer is not None and output is None
        ):
            raise ValueError(
                f"{path}: '{dependency.source}' consumers must read its published output"
            )

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
    stages: tuple[Stage, ...],
    dependencies: tuple[Dependency, ...],
    instances: Mapping[str, Accel],
    path: Path,
) -> None:
    for stage in stages:
        incoming = [
            dependency
            for dependency in dependencies
            if dependency.destination == stage.name
        ]
        if (
            instances[stage.endpoint].kind in ("aes", "pool")
            and incoming
            and sum(dependency.copy is not None for dependency in incoming) != 1
        ):
            raise ValueError(
                f"{path}: streaming stage '{stage.name}' requires exactly one data input"
            )
        ranges = sorted(
            (
                dependency.copy.destination_offset,
                dependency.copy.destination_offset + dependency.copy.destination_size,
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
    memories = {
        name: Memory(accel.base, accel.size)
        for name, accel in config.instances.items()
        if accel.size
    }
    stage_map = {stage.name: stage for stage in config.stages}
    for dependency in config.dependencies:
        if dependency.copy is None:
            continue
        copy = dependency.copy
        destination = stage_map[dependency.destination].endpoint
        source = stage_map[dependency.source].endpoint if dependency.source else None
        source_memory = memories.get(source)
        if (
            source_memory is not None
            and copy.source_address is None
            and copy.source_offset + copy.size > source_memory.size
        ):
            raise ValueError(
                f"{path}: {dependency.source} output exceeds source memory"
            )
        destination_memory = memories.get(destination)
        if (
            destination_memory is not None
            and copy.destination_offset + copy.destination_size
            > destination_memory.size
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
    buffer_slots = communication.get("buffer_slots", {})
    if not isinstance(buffer_slots, Mapping):
        raise ValueError(f"{path}: buffer_slots must map endpoint names to counts")
    gemmini = load_memory(memory, "gemmini_external_spm", path)
    cgra = load_memory(memory, "cgra_spm_window", path)
    window = require_mapping(memory, "cgra_spm_window", path)
    packed_words = (
        require_int(window, "packed_words", path) if "packed_words" in window else None
    )
    if packed_words is not None and packed_words <= 0:
        raise ValueError(f"{path}: packed_words must be positive")
    packed_by_instance = {
        name: require_int(value, "packed_words", path)
        for name, value in communication.get("instances", {}).items()
        if "packed_words" in value
    }
    instances = {accel.name: accel for accel in instance_layout(document).instances}
    buffers = load_buffers(communication.get("buffers", {}), path)
    stages = load_stages(communication.get("stages"), instances, buffers, path)
    if any(name not in instances for name in buffer_slots):
        raise ValueError(f"{path}: buffer_slots names an unknown endpoint")
    if any(not isinstance(count, int) or count <= 0 for count in buffer_slots.values()):
        raise ValueError(f"{path}: buffer slot counts must be positive integers")
    memories = {
        name: Memory(accel.base, accel.size)
        for name, accel in instances.items()
        if accel.size
    }
    dependencies = load_dependencies(
        communication.get("dependencies"), stages, memories, instances, buffers, path
    )
    validate_copy_alignment(
        stages, dependencies, hardware.dma_beat_bytes, instances, path
    )
    validate_graph(stages, dependencies, path)
    validate_destinations(stages, dependencies, instances, path)
    bridges = {
        name: infer_bridge(
            stages,
            dependencies,
            memories[name],
            memories,
            hardware,
            path,
            packed_by_instance.get(name, packed_words),
            name,
            communication.get("instances", {}).get(name, {}).get("output_format"),
        )
        for name, accel in instances.items()
        if accel.kind == "cgra"
    }
    bridge = next(iter(bridges.values()), None)
    capacity = communication.get("capacity", {})
    job_counts = {
        name: max(
            (stage.job + stage.jobs for stage in stages if stage.endpoint == name),
            default=1,
        )
        for name in instances
    }
    job_counts.update(capacity.get("jobs", {}))
    config = AutoLinkConfig(
        gemmini,
        cgra,
        stages,
        dependencies,
        bridge,
        buffer_slots,
        instances,
        bridges,
        communication.get("run_capacity", 0),
        capacity.get("stages", len(stages)),
        capacity.get("dependencies", len(dependencies)),
        job_counts,
    )
    validate_memory(config, path)
    validate_capacity(config, path)
    return config


def validate_capacity(config: AutoLinkConfig, path: Path) -> None:
    if (
        len(config.stages) > config.stage_capacity
        or len(config.dependencies) > config.dependency_capacity
    ):
        raise ValueError(f"{path}: graph exceeds hardware stage/dependency capacity")
    for name, instance in config.instances.items():
        if config.jobs[name] <= 0:
            raise ValueError(f"{path}: '{name}' job capacity must be positive")
        if instance.kind == "pool" and config.jobs[name] != 1:
            raise ValueError(
                f"{path}: Pool has one configuration and requires job capacity 1"
            )
        if instance.kind in ("aes", "pool") and config.buffer_slots.get(name, 1) != 1:
            raise ValueError(
                f"{path}: streaming endpoint '{name}' requires one buffer slot"
            )
    for stage in config.stages:
        if stage.jobs <= 0:
            raise ValueError(f"{path}: '{stage.name}' job count must be positive")
        if stage.job + stage.jobs > config.jobs[stage.endpoint]:
            raise ValueError(
                f"{path}: '{stage.endpoint}' jobs exceed hardware capacity"
            )


def load_graph(
    path: Path, hardware: AutoLinkConfig, beat_bytes: int
) -> tuple[str, AutoLinkConfig]:
    validate_runtime_entry(hardware, path)
    document = yaml.safe_load(path.read_text(encoding="utf-8"))
    name = document.get("name", path.stem)
    if STAGE_NAME.fullmatch(name) is None:
        raise ValueError(f"{path}: graph name must be a lowercase identifier")
    buffers = load_buffers(document.get("buffers", {}), path)
    stages = load_stages(document["stages"], hardware.instances, buffers, path)
    memories = {
        name: Memory(accel.base, accel.size)
        for name, accel in hardware.instances.items()
        if accel.size
    }
    dependencies = load_dependencies(
        document["dependencies"], stages, memories, hardware.instances, buffers, path
    )
    config = replace(hardware, stages=stages, dependencies=dependencies)
    validate_graph(stages, dependencies, path)
    validate_destinations(stages, dependencies, hardware.instances, path)
    validate_copy_alignment(stages, dependencies, beat_bytes, hardware.instances, path)
    validate_memory(config, path)
    validate_capacity(config, path)
    validate_runtime_entry(config, path)
    for dependency in dependencies:
        copy = dependency.copy
        if copy is None or copy.source_address is not None:
            continue
        source = stage_map(config)[dependency.source].endpoint
        destination = stage_map(config)[dependency.destination].endpoint
        if hardware.instances[source].kind == "cgra":
            packed = hardware.bridges[source] is not None
            if packed != (copy.format == INT8_FORMAT):
                raise ValueError(
                    f"{path}: '{source}' copy format must match its configured SPM window"
                )
        if copy.format != INT8_FORMAT:
            continue
        for endpoint in (source, destination):
            if (
                hardware.instances[endpoint].kind == "cgra"
                and hardware.bridges[endpoint] is None
            ):
                raise ValueError(f"{path}: '{endpoint}' has no configured INT8 window")
        if hardware.instances[source].kind == "cgra":
            bridge = hardware.bridges[source]
            word = copy.source_offset // CGRA_WORD_BYTES
            if (
                word < bridge.outbound_word
                or word + copy.size > bridge.outbound_word + bridge.outbound_words
            ):
                raise ValueError(f"{path}: '{source}' output exceeds its INT8 window")
        if hardware.instances[destination].kind == "cgra":
            bridge = hardware.bridges[destination]
            if (
                copy.destination_offset != bridge.inbound_word * CGRA_WORD_BYTES
                or copy.size != bridge.inbound_words * CGRA_WORD_BYTES
            ):
                raise ValueError(
                    f"{path}: '{destination}' requantization requires its configured input region"
                )
    return name, config


def validate_runtime_entry(config: AutoLinkConfig, path: Path) -> None:
    # In a validated DAG, every stage having an incoming edge makes input_ready its only root.
    destinations = {dependency.destination for dependency in config.dependencies}
    if any(stage.name not in destinations for stage in config.stages):
        raise ValueError(
            f"{path}: runtime graphs and their hardware default graph must start at input_ready"
        )


def buffer_text(memory: Memory) -> str:
    return f'Some(AutoBuffer(BigInt("{memory.base:x}", 16), {memory.size}))'


def copied_dependencies(config: AutoLinkConfig) -> list[Dependency]:
    return [dependency for dependency in config.dependencies if dependency.copy]


def stage_map(config: AutoLinkConfig) -> dict[str, Stage]:
    return {stage.name: stage for stage in config.stages}


def gemmini_endpoint(config: AutoLinkConfig, name: str) -> str:
    accel = config.instances[name]
    return (
        f'      AutoEndpointSpec(name = "{name}", buffer = '
        f"{buffer_text(Memory(accel.base, accel.size))}, localBytes = {accel.size}, "
        f"bufferSlots = {config.buffer_slots.get(name, 1)})"
    )


def cgra_endpoint(config: AutoLinkConfig, name: str) -> str:
    accel = config.instances[name]
    bridge = config.bridges[name]
    size = bridge.window_bytes if bridge is not None else accel.size
    return (
        f'      AutoEndpointSpec(name = "{name}", buffer = '
        f"{buffer_text(Memory(accel.base, size))}, localBytes = {accel.size}, "
        "bufferedInput = true, inputAlignment = CGRAGenerated.params.dataPayloadWidth / 8, "
        f"bufferSlots = {config.buffer_slots.get(name, 1)}, releaseOnCopy = true)"
    )


def aes_endpoint(config: AutoLinkConfig, name: str) -> str:
    copies = copied_dependencies(config)
    stages = stage_map(config)
    source_spans = [
        dependency.copy.source_offset + dependency.copy.size
        for dependency in copies
        if dependency.source is not None and stages[dependency.source].endpoint == name
    ]
    input_sizes = [
        dependency.copy.destination_offset + dependency.copy.size
        for dependency in copies
        if stages[dependency.destination].endpoint == name
    ]
    buffer = (
        buffer_text(Memory(config.gemmini.base, max(source_spans)))
        if source_spans
        else "None"
    )
    local_bytes = max(input_sizes, default=0)
    return (
        f'      AutoEndpointSpec(name = "{name}", buffer = {buffer}, localBytes = {local_bytes}, '
        f"bufferSlots = {config.buffer_slots.get(name, 1)}, releaseOnCopy = true)"
    )


def pool_endpoint(config: AutoLinkConfig, name: str) -> str:
    copies = copied_dependencies(config)
    stages = stage_map(config)
    sizes = [
        dependency.copy.destination_offset + dependency.copy.size
        for dependency in copies
        if stages[dependency.destination].endpoint == name
    ]
    return f'      AutoEndpointSpec(name = "{name}", buffer = None, localBytes = {max(sizes, default=0)}, releaseOnCopy = true)'


ENDPOINT_TEXT = {
    "gemmini": gemmini_endpoint,
    "cgra": cgra_endpoint,
    "aes": aes_endpoint,
    "pool": pool_endpoint,
}


def endpoint_text(config: AutoLinkConfig, name: str) -> str:
    text = ENDPOINT_TEXT[config.instances[name].kind](config, name)
    return text[:-1] + f", jobs = {config.jobs[name]})"


def copy_text(config: AutoLinkConfig, dependency: Dependency) -> str:
    copy = dependency.copy
    if copy is None:
        return "None"
    source_offset = copy.source_offset
    if copy.format == INT8_FORMAT:
        source = stage_map(config)[dependency.source]
        if config.instances[source.endpoint].kind == "cgra":
            source_offset = (
                copy.source_offset // CGRA_WORD_BYTES
                - config.bridges[source.endpoint].outbound_word
            )
    expansion = (
        f", expansion = {CGRA_WORD_BYTES}" if copy.source_format == INT8_FORMAT else ""
    )
    address = ""
    if copy.source_address is not None:
        value = copy.source_address if isinstance(copy.source_address, int) else 0
        address = f', sourceAddress = Some(BigInt("{value:x}", 16))'
    return (
        "Some(AutoCopySpec("
        f"sourceOffset = {source_offset}, destinationOffset = {copy.destination_offset}, "
        f"bytes = {copy.size}{expansion}{address}, sourceStride = {copy.source_stride}, "
        f"bytesPerPixel = {copy.pixel_bytes}))"
    )


def output_text(output: Output | None) -> str:
    if output is None:
        return ""
    address = output.address if isinstance(output.address, int) else 0
    return (
        f', output = Some(AutoOutputSpec(address = BigInt("{address:x}", 16), '
        f"bytes = {output.size}, stride = {output.stride}, "
        f"bytesPerPixel = {output.pixel_bytes}))"
    )


def scala_text(config: AutoLinkConfig) -> str:
    stage_index = {stage.name: index for index, stage in enumerate(config.stages)}
    names = config.instances
    stages = ",\n".join(
        f'      AutoStageSpec(name = "{stage.name}", endpoint = "{stage.endpoint}", job = {stage.job}'
        + (f", jobs = {stage.jobs}" if stage.jobs != 1 else "")
        + output_text(stage.output)
        + ")"
        for stage in config.stages
    )
    dependencies = ",\n".join(
        "      AutoDependencySpec("
        f"source = {'None' if dependency.source is None else f'Some({stage_index[dependency.source]})'}, "
        f"destination = {stage_index[dependency.destination]}, copy = {copy_text(config, dependency)})"
        for dependency in config.dependencies
    )
    endpoints = ",\n".join(endpoint_text(config, name) for name in names)
    capacity = (
        f",\n    runCapacity = {config.run_capacity}" if config.run_capacity else ""
    )
    capacity += f",\n    stageCapacity = {config.stage_capacity},\n    dependencyCapacity = {config.dependency_capacity}"
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
    controlBytes = CgraLinkControlGenerated.pageSizeBytes{capacity})
}}
"""


def address_text(value: int | str, bindings: Mapping[str, int]) -> str:
    if isinstance(value, str):
        return f"{{.value = 0, .binding = {bindings[value]}}}"
    return f"{{.value = UINT64_C(0x{value:x}), .binding = -1}}"


def graph_tables(config: AutoLinkConfig, prefix: str) -> str:
    symbols = [stage.output.address for stage in config.stages if stage.output]
    symbols += [
        dependency.copy.source_address
        for dependency in config.dependencies
        if dependency.copy
    ]
    bindings = {
        name: index
        for index, name in enumerate(
            dict.fromkeys(value for value in symbols if isinstance(value, str))
        )
    }
    constants = "\n".join(
        f"#define {prefix}_ADDRESS_{name.upper()} {index}u"
        for name, index in bindings.items()
    )
    stages = []
    for stage in config.stages:
        output = ".address = {.binding = -1}"
        if stage.output is not None:
            value = stage.output
            output = (
                f".flags = 1u, .pixel_bytes = {value.pixel_bytes}u, "
                f".address = {address_text(value.address, bindings)}, .stride = {value.stride}u, .bytes = {value.size}u"
            )
        stages.append(
            f"  {{.endpoint = {config.instances[stage.endpoint].id}u, .job = {stage.job}u, .output = {{{output}}}}}"
        )
    indices = {stage.name: index for index, stage in enumerate(config.stages)}
    names = stage_map(config)
    edges = []
    for dependency in config.dependencies:
        source = indices[dependency.source] if dependency.source else 0
        destination = indices[dependency.destination]
        flags = (
            1
            | (int(dependency.source is None) << 1)
            | (int(dependency.copy is not None) << 2)
        )
        fields = (
            f".source = {source}u, .destination = {destination}u, .flags = {flags}u"
        )
        copy = dependency.copy
        if copy is None:
            fields += ", .address = {.binding = -1}"
        else:
            address = copy.source_address
            offset = copy.source_offset
            if address is None:
                endpoint = names[dependency.source].endpoint
                accel = config.instances[endpoint]
                address = config.gemmini.base if accel.kind == "aes" else accel.base
                if copy.format == INT8_FORMAT and accel.kind == "cgra":
                    offset = (
                        offset // CGRA_WORD_BYTES
                        - config.bridges[endpoint].outbound_word
                    )
            expansion = 2 if copy.source_format == INT8_FORMAT else 0
            fields += (
                f", .bytes = {copy.size}u, .expansion = {expansion}u, .address = {address_text(address, bindings)}, "
                f".source_offset = {offset}u, .destination_offset = {copy.destination_offset}u, "
                f".source_stride = {copy.source_stride}u, .pixel_bytes = {copy.pixel_bytes}u"
            )
        edges.append(f"  {{{fields}}}")
    stage_text = ",\n".join(stages)
    edge_text = ",\n".join(edges)
    return f"""{constants}
#define {prefix}_ADDRESS_COUNT {len(bindings)}u

static const auto_link_stage_t {prefix}_STAGES[] = {{
{stage_text}
}};
static const auto_link_edge_t {prefix}_EDGES[] = {{
{edge_text}
}};
static const auto_link_graph_t {prefix}_GRAPH = {{
  .stage_count = {len(config.stages)}u, .edge_count = {len(config.dependencies)}u,
  .stage_capacity = {config.stage_capacity}u, .edge_capacity = {config.dependency_capacity}u,
  .stages = {prefix}_STAGES, .edges = {prefix}_EDGES
}};
"""


def header_text(config: AutoLinkConfig, prefix: str = "AUTO_LINK") -> str:
    slots = "\n".join(
        f"#define {prefix}_{name.upper()}_BUFFER_SLOTS {config.buffer_slots.get(name, 1)}u"
        for name in config.instances
    )
    constants = "\n".join(
        f"#define {prefix}_STAGE_{stage.name.upper()} {index}u"
        for index, stage in enumerate(config.stages)
    )
    jobs = "\n".join(
        f"#define {prefix}_JOB_{stage.name.upper()} {stage.job}u\n"
        f"#define {prefix}_JOB_{stage.name.upper()}_COUNT {stage.jobs}u"
        for stage in config.stages
    )
    copies = "\n".join(
        f"#define {prefix}_COPY_{(dependency.source or INPUT_READY).upper()}_{dependency.destination.upper()} {index}u"
        for index, dependency in enumerate(config.dependencies)
        if dependency.copy is not None
    )
    return f"""/* Generated by scripts/generate_auto_links.py. Do not edit. */
#ifndef {prefix}_GENERATED_H
#define {prefix}_GENERATED_H

#include "auto_link_types.h"

{slots}

{constants}

{jobs}

{copies}

{graph_tables(config, prefix)}

#endif
"""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--soc-yaml", type=Path, default=DEFAULT_SOC)
    parser.add_argument(
        "--graph-yaml",
        type=Path,
        help="Emit a loadable graph header against --soc-yaml without changing hardware",
    )
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
    if args.graph_yaml is not None:
        if args.header_out is None or args.scala_out is not None:
            raise ValueError(
                "--graph-yaml requires --header-out and does not emit Scala"
            )
        hardware = load_cgra_hardware(args.cgra_scala, args.cache_block_scala)
        name, graph = load_graph(args.graph_yaml, config, hardware.dma_beat_bytes)
        write(args.header_out, header_text(graph, name.upper()), args.check)
        return 0
    write(args.scala_out or DEFAULT_OUTPUT, scala_text(config), args.check)
    if args.header_out is not None or args.scala_out is None:
        write(args.header_out or DEFAULT_HEADER, header_text(config), args.check)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
