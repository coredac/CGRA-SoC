#!/usr/bin/env python3
"""YAML schema and naming helpers for OpenFPGA Chipyard demos."""

from __future__ import annotations

import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List

SCRIPTS_DIR = Path(__file__).resolve().parents[1]
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

from util.schema import (  # noqa: E402
    ensure_dir,
    ensure_file,
    load_yaml_mapping,
    require_int,
    require_mapping,
    require_string,
)


ROOT = Path(__file__).resolve().parents[2]
CHIPYARD_VSRC = (
    ROOT / "chipyard" / "generators" / "chipyard" / "src" / "main" / "resources" / "vsrc"
)
CHIPYARD_SCALA = (
    ROOT / "chipyard" / "generators" / "chipyard" / "src" / "main" / "scala" / "example"
)
TESTS_GENERATED = ROOT / "tests" / "generated"
CELL_LIBRARY_FILES = ("dff.v", "latch.v", "gpio.v", "mux2.v", "inv.v", "buf4.v", "tap_buf4.v")

_IDENT_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
_FILE_STEM_RE = re.compile(r"^[A-Za-z0-9_][A-Za-z0-9_.-]*$")


def resolve_under(base: Path, rel: str) -> Path:
    path = Path(rel)
    return path if path.is_absolute() else base / path


def _require_identifier(name: str, label: str) -> str:
    if not _IDENT_RE.fullmatch(name):
        raise ValueError(f"{label} must be a Verilog/C identifier, got {name!r}")
    return name


def _macro_prefix(name: str) -> str:
    text = re.sub(r"[^A-Za-z0-9]+", "_", name).strip("_").upper()
    if not text or text[0].isdigit():
        text = f"_{text}"
    return text


def _c_identifier(name: str) -> str:
    text = re.sub(r"[^A-Za-z0-9_]+", "_", name).strip("_").lower()
    if not text or text[0].isdigit():
        text = f"_{text}"
    return text


def _pascal_case(name: str) -> str:
    parts = [part for part in re.split(r"[^A-Za-z0-9]+", name) if part]
    if not parts:
        raise ValueError(f"cannot derive PascalCase identifier from {name!r}")
    return "".join(part[:1].upper() + part[1:] for part in parts)


@dataclass(frozen=True)
class ConfigProtocol:
    kind: str
    address_width: int
    data_width: int

    @property
    def word_width(self) -> int:
        return self.address_width + self.data_width


@dataclass(frozen=True)
class FieldSpec:
    name: str
    width: int
    lsb: int

    @property
    def msb(self) -> int:
        return self.lsb + self.width - 1

    @property
    def bit_indices(self) -> range:
        return range(self.lsb, self.lsb + self.width)

    @property
    def macro_name(self) -> str:
        return _macro_prefix(self.name)


@dataclass(frozen=True)
class RegisterSpec:
    name: str
    fields: List[FieldSpec]
    width: int


@dataclass(frozen=True)
class ArchitectureSpec:
    vpr_arch: str
    openfpga_arch: str
    shell_template: str
    simulation_setting: str
    config_protocol: ConfigProtocol


@dataclass(frozen=True)
class ApplicationSpec:
    name: str
    top_module: str
    benchmark_blif: str
    benchmark_verilog: str
    benchmark_act: str
    chan_width: int


@dataclass(frozen=True)
class UserInterfaceSpec:
    input_register: RegisterSpec
    output_register: RegisterSpec

    @property
    def all_fields(self) -> List[FieldSpec]:
        return self.input_register.fields + self.output_register.fields


@dataclass(frozen=True)
class SocSpec:
    base_address: int
    size: int


@dataclass(frozen=True)
class ChipyardSpec:
    config_name: str
    peripheral_name: str
    wrapper_module: str
    scala_object: str


@dataclass(frozen=True)
class DemoConfig:
    path: Path
    raw: Dict[str, Any]
    name: str
    openfpga_root: Path
    architecture: ArchitectureSpec
    application: ApplicationSpec
    soc: SocSpec
    chipyard: ChipyardSpec

    @property
    def macro_prefix(self) -> str:
        return _macro_prefix(self.name)

    @property
    def c_identifier(self) -> str:
        return _c_identifier(self.name)

    @property
    def manifest_filename(self) -> str:
        return f"{self.name}_manifest.v"

    @property
    def bitstream_header_filename(self) -> str:
        return f"{self.name}_bitstream.h"

    @property
    def pin_map_header_filename(self) -> str:
        return f"{self.name}_pin_map.h"

    @property
    def vsrc_dir(self) -> Path:
        return CHIPYARD_VSRC / self.name

    @property
    def workdir(self) -> Path:
        return ROOT / "build" / "openfpga" / self.name

    @property
    def bitstream_header_path(self) -> Path:
        return TESTS_GENERATED / self.bitstream_header_filename

    @property
    def pin_map_header_path(self) -> Path:
        return TESTS_GENERATED / self.pin_map_header_filename

    @property
    def scala_generated_path(self) -> Path:
        return CHIPYARD_SCALA / f"{self.chipyard.scala_object}.scala"

    @property
    def wrapper_path(self) -> Path:
        return self.vsrc_dir / f"{self.chipyard.wrapper_module}.v"

def _parse_protocol(data: Dict[str, Any]) -> ConfigProtocol:
    kind = require_string(data, "type", "architecture.config_protocol")
    if kind != "frame_based":
        raise ValueError(f"unsupported config_protocol.type {kind!r}; only frame_based is supported")
    protocol = ConfigProtocol(
        kind=kind,
        address_width=require_int(data, "address_width", "architecture.config_protocol"),
        data_width=require_int(data, "data_width", "architecture.config_protocol"),
    )
    if protocol.address_width <= 0 or protocol.data_width <= 0:
        raise ValueError("config protocol widths must be positive")
    if protocol.word_width > 16:
        raise ValueError("generated C header uses uint16_t cfg words; config word width must be <= 16")
    return protocol


def _parse_architecture(data: Dict[str, Any]) -> ArchitectureSpec:
    return ArchitectureSpec(
        vpr_arch=require_string(data, "vpr_arch", "architecture"),
        openfpga_arch=require_string(data, "openfpga_arch", "architecture"),
        shell_template=require_string(data, "shell_template", "architecture"),
        simulation_setting=require_string(data, "simulation_setting", "architecture"),
        config_protocol=_parse_protocol(require_mapping(data, "config_protocol", "architecture")),
    )


def _parse_application(data: Dict[str, Any]) -> ApplicationSpec:
    return ApplicationSpec(
        name=_require_identifier(require_string(data, "name", "application"), "application.name"),
        top_module=_require_identifier(require_string(data, "top_module", "application"), "application.top_module"),
        benchmark_blif=require_string(data, "benchmark_blif", "application"),
        benchmark_verilog=require_string(data, "benchmark_verilog", "application"),
        benchmark_act=require_string(data, "benchmark_act", "application"),
        chan_width=require_int(data, "chan_width", "application", default=300),
    )


def _parse_chipyard(data: Dict[str, Any], demo_name: str) -> ChipyardSpec:
    wrapper_default = f"{_pascal_case(demo_name)}Wrapper"
    return ChipyardSpec(
        config_name=_require_identifier(
            require_string(data, "config_name", "chipyard") if "config_name" in data else "OpenFPGADemoRocketConfig",
            "chipyard.config_name",
        ),
        peripheral_name=require_string(data, "peripheral_name", "chipyard") if "peripheral_name" in data else demo_name,
        wrapper_module=_require_identifier(
            require_string(data, "wrapper_module", "chipyard") if "wrapper_module" in data else wrapper_default,
            "chipyard.wrapper_module",
        ),
        scala_object=_require_identifier(
            require_string(data, "scala_object", "chipyard") if "scala_object" in data else "OpenFPGAGenerated",
            "chipyard.scala_object",
        ),
    )


def load_demo_config(path: Path) -> DemoConfig:
    config_path = path if path.is_absolute() else ROOT / path
    raw = load_yaml_mapping(config_path)

    name = require_string(raw, "name")
    if not _FILE_STEM_RE.fullmatch(name):
        raise ValueError(f"name must be a stable file/directory stem, got {name!r}")
    _require_identifier(_c_identifier(name), "derived C identifier")

    openfpga_root = resolve_under(ROOT, require_string(raw, "openfpga_root"))
    ensure_dir(openfpga_root, "OpenFPGA root")

    return DemoConfig(
        path=config_path,
        raw=raw,
        name=name,
        openfpga_root=openfpga_root,
        architecture=_parse_architecture(require_mapping(raw, "architecture")),
        application=_parse_application(require_mapping(raw, "application")),
        soc=SocSpec(
            base_address=require_int(require_mapping(raw, "soc"), "base_address", "soc"),
            size=require_int(require_mapping(raw, "soc"), "size", "soc"),
        ),
        chipyard=_parse_chipyard(require_mapping(raw, "chipyard") if "chipyard" in raw else {}, name),
    )
