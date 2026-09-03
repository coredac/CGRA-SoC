#!/usr/bin/env python3
"""Generate complete elaborated AutoLink parameters."""

from __future__ import annotations

import argparse
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
CAPABILITIES = {
    "gemmini": {"source", "destination"},
    "cgra": {"source", "destination"},
    "aes": {"source", "destination"},
}
TASK_KEYS = {"source", "destination", "size_bytes"}


@dataclass(frozen=True)
class Memory:
    base: int
    size: int


@dataclass(frozen=True)
class Task:
    source: str
    destination: str
    size: int


@dataclass(frozen=True)
class AutoLinkConfig:
    gemmini: Memory
    cgra: Memory
    tasks: tuple[Task, ...]


def load_memory(data: Mapping[str, object], key: str, path: Path) -> Memory:
    value = require_mapping(data, key, path)
    memory = Memory(
        base=require_int(value, "base_address", path),
        size=require_int(value, "size_bytes", path),
    )
    if memory.base < 0 or memory.size <= 0:
        raise ValueError(f"{path}: '{key}' needs a nonnegative base and positive size")
    return memory


def load_config(path: Path) -> AutoLinkConfig:
    document = yaml.safe_load(path.read_text(encoding="utf-8"))
    if not isinstance(document, Mapping):
        raise ValueError(f"{path}: expected a mapping")
    memory = require_mapping(document, "memory", path)
    communication = require_mapping(document, "communication", path)
    values = communication.get("auto_tasks")
    if not isinstance(values, list) or not values:
        raise ValueError(f"{path}: 'auto_tasks' must be a non-empty list")

    tasks = []
    routes = set()
    for index, value in enumerate(values):
        if not isinstance(value, Mapping):
            raise ValueError(f"{path}: auto_tasks[{index}] must be a mapping")
        extra = set(value) - TASK_KEYS
        if extra:
            raise ValueError(f"{path}: auto_tasks[{index}] has unsupported fields")
        source = value.get("source")
        destination = value.get("destination")
        size = value.get("size_bytes")
        if not isinstance(source, str) or not isinstance(destination, str):
            raise ValueError(
                f"{path}: auto_tasks[{index}] needs source and destination"
            )
        if not isinstance(size, int) or isinstance(size, bool) or size <= 0:
            raise ValueError(f"{path}: auto_tasks[{index}].size_bytes must be positive")
        if source not in CAPABILITIES:
            raise ValueError(f"{path}: unknown AutoLink source '{source}'")
        if destination not in CAPABILITIES:
            raise ValueError(f"{path}: unknown AutoLink destination '{destination}'")
        if source == destination:
            raise ValueError(f"{path}: AutoLink self-link '{source}' is unsupported")
        if "source" not in CAPABILITIES[source]:
            raise ValueError(f"{path}: AutoLink endpoint '{source}' cannot be a source")
        if "destination" not in CAPABILITIES[destination]:
            raise ValueError(
                f"{path}: AutoLink endpoint '{destination}' cannot be a destination"
            )
        if destination == "aes" and size != AES_JOB_BYTES:
            raise ValueError(f"{path}: AES AutoLink task must be {AES_JOB_BYTES} bytes")
        route = (source, destination)
        if route in routes:
            raise ValueError(
                f"{path}: duplicate AutoLink route {source} -> {destination}"
            )
        routes.add(route)
        tasks.append(Task(source, destination, size))

    for endpoint in CAPABILITIES:
        incoming = sum(task.destination == endpoint for task in tasks)
        outgoing = sum(task.source == endpoint for task in tasks)
        if incoming > 1:
            raise ValueError(
                f"{path}: AutoLink endpoint '{endpoint}' has multiple incoming tasks"
            )
        if outgoing > 1:
            raise ValueError(
                f"{path}: AutoLink endpoint '{endpoint}' has multiple outgoing tasks"
            )
    if any(task.source == "cgra" for task in tasks) and not any(
        task.destination == "cgra" for task in tasks
    ):
        raise ValueError(f"{path}: CGRA source task requires an incoming CGRA task")

    gemmini = load_memory(memory, "gemmini_external_spm", path)
    cgra = load_memory(memory, "cgra_spm_window", path)
    for task in tasks:
        source_size = cgra.size if task.source == "cgra" else gemmini.size
        if task.size > source_size:
            raise ValueError(
                f"{path}: {task.source} -> {task.destination} exceeds source memory"
            )
    return AutoLinkConfig(gemmini, cgra, tuple(tasks))


def buffer_text(memory: Memory) -> str:
    return f'Some(AutoBuffer(BigInt("{memory.base:x}", 16), {memory.size}))'


def endpoint_text(config: AutoLinkConfig, name: str) -> str:
    if name == "gemmini":
        return (
            '      AutoEndpointSpec(name = "gemmini", buffer = '
            f"{buffer_text(config.gemmini)}, localBytes = {config.gemmini.size})"
        )
    if name == "cgra":
        is_source = any(task.source == "cgra" for task in config.tasks)
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
    aes_source = next((task for task in config.tasks if task.source == "aes"), None)
    buffer = "None"
    if aes_source is not None:
        output = Memory(config.gemmini.base, aes_source.size)
        buffer = buffer_text(output)
    aes_bytes = next(task.size for task in config.tasks if task.destination == "aes")
    return (
        f'      AutoEndpointSpec(name = "aes", buffer = {buffer}, localBytes = {aes_bytes})'
    )


def task_text(config: AutoLinkConfig, index: int, task: Task) -> str:
    source_offset = config.gemmini.size - task.size if task.source == "gemmini" else 0
    return (
        f"      AutoCopySpec(route = {index}, sourceOffset = {source_offset}, "
        f"destinationOffset = 0, bytes = {task.size})"
    )


def scala_text(config: AutoLinkConfig) -> str:
    names = []
    for task in config.tasks:
        for name in (task.source, task.destination):
            if name not in names:
                names.append(name)
    links = ",\n".join(
        f'      AutoLinkSpec(source = "{task.source}", destination = "{task.destination}")'
        for task in config.tasks
    )
    endpoints = ",\n".join(endpoint_text(config, name) for name in names)
    table = ",\n".join(
        task_text(config, index, task) for index, task in enumerate(config.tasks)
    )
    return f"""package chipyard.socgen.generated

import chipyard.example.CGRAGenerated
import chipyard.socgen.link._

// Generated by scripts/generate_auto_links.py. Do not edit.
object AutoLinkGenerated {{
  val params = AutoLinkParams(
    links = Seq(
{links}),
    endpoints = Seq(
{endpoints}),
    table = Seq(
{table}),
    beatBytes = CGRAGenerated.params.dma.dramDataWidth / 8,
    copyDepth = 2)
}}
"""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--soc-yaml", type=Path, default=DEFAULT_SOC)
    parser.add_argument("--scala-out", type=Path)
    parser.add_argument("--check", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    path = args.soc_yaml.resolve()
    output = args.scala_out or DEFAULT_OUTPUT
    write(output, scala_text(load_config(path)), args.check)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
