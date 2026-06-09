#!/usr/bin/env python3
"""YAML schema and naming helpers for OpenFPGA Chipyard demos."""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List

import yaml


ROOT = Path(__file__).resolve().parents[1]
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


def parse_int(value: Any, label: str = "value") -> int:
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        return int(value, 0)
    raise TypeError(f"{label} must be int-like, got {value!r}")


def require_mapping(data: Dict[str, Any], key: str) -> Dict[str, Any]:
    value = data.get(key)
    if not isinstance(value, dict):
        raise ValueError(f"missing mapping: {key}")
    return value


def require_string(data: Dict[str, Any], key: str) -> str:
    value = data.get(key)
    if not isinstance(value, str) or not value:
        raise ValueError(f"missing string: {key}")
    return value


def resolve_under(base: Path, rel: str) -> Path:
    path = Path(rel)
    return path if path.is_absolute() else base / path


def ensure_file(path: Path, label: str) -> Path:
    if not path.is_file():
        raise FileNotFoundError(f"{label} not found: {path}")
    return path


def ensure_dir(path: Path, label: str) -> Path:
    if not path.is_dir():
        raise FileNotFoundError(f"{label} not found: {path}")
    return path


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
    pin_map: str
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
    user_interface: UserInterfaceSpec
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

    @property
    def input_width(self) -> int:
        return self.user_interface.input_register.width

    @property
    def output_width(self) -> int:
        return self.user_interface.output_register.width


def _parse_field(data: Dict[str, Any], context: str) -> FieldSpec:
    name = _require_identifier(require_string(data, "name"), f"{context}.name")
    width = parse_int(data.get("width", 1), f"{context}.width")
    if width <= 0:
        raise ValueError(f"{context}.width must be positive")
    has_bit = "bit" in data
    has_lsb = "lsb" in data
    if has_bit == has_lsb:
        raise ValueError(f"{context} must specify exactly one of bit or lsb")
    lsb = parse_int(data["bit"] if has_bit else data["lsb"], f"{context}.lsb")
    if lsb < 0:
        raise ValueError(f"{context}.lsb must be non-negative")
    return FieldSpec(name=name, width=width, lsb=lsb)


def _parse_register(data: Dict[str, Any], context: str) -> RegisterSpec:
    name = _require_identifier(require_string(data, "name"), f"{context}.name")
    raw_fields = data.get("fields")
    if not isinstance(raw_fields, list) or not raw_fields:
        raise ValueError(f"{context}.fields must be a non-empty list")

    fields: List[FieldSpec] = []
    used_bits: Dict[int, str] = {}
    seen_names: set[str] = set()
    for index, raw_field in enumerate(raw_fields):
        if not isinstance(raw_field, dict):
            raise ValueError(f"{context}.fields[{index}] must be a mapping")
        field = _parse_field(raw_field, f"{context}.fields[{index}]")
        if field.name in seen_names:
            raise ValueError(f"{context} has duplicate field name {field.name!r}")
        seen_names.add(field.name)
        for bit in field.bit_indices:
            if bit in used_bits:
                raise ValueError(
                    f"{context} fields {used_bits[bit]!r} and {field.name!r} overlap at bit {bit}"
                )
            used_bits[bit] = field.name
        fields.append(field)

    width = max(field.msb for field in fields) + 1
    return RegisterSpec(name=name, fields=fields, width=width)


def _parse_protocol(data: Dict[str, Any]) -> ConfigProtocol:
    kind = require_string(data, "type")
    if kind != "frame_based":
        raise ValueError(f"unsupported config_protocol.type {kind!r}; only frame_based is supported")
    protocol = ConfigProtocol(
        kind=kind,
        address_width=parse_int(data["address_width"], "architecture.config_protocol.address_width"),
        data_width=parse_int(data["data_width"], "architecture.config_protocol.data_width"),
    )
    if protocol.address_width <= 0 or protocol.data_width <= 0:
        raise ValueError("config protocol widths must be positive")
    if protocol.word_width > 16:
        raise ValueError("generated C header uses uint16_t cfg words; config word width must be <= 16")
    return protocol


def _parse_architecture(data: Dict[str, Any]) -> ArchitectureSpec:
    return ArchitectureSpec(
        vpr_arch=require_string(data, "vpr_arch"),
        openfpga_arch=require_string(data, "openfpga_arch"),
        shell_template=require_string(data, "shell_template"),
        simulation_setting=require_string(data, "simulation_setting"),
        config_protocol=_parse_protocol(require_mapping(data, "config_protocol")),
    )


def _parse_application(data: Dict[str, Any]) -> ApplicationSpec:
    return ApplicationSpec(
        name=_require_identifier(require_string(data, "name"), "application.name"),
        top_module=_require_identifier(require_string(data, "top_module"), "application.top_module"),
        benchmark_blif=require_string(data, "benchmark_blif"),
        benchmark_verilog=require_string(data, "benchmark_verilog"),
        benchmark_act=require_string(data, "benchmark_act"),
        chan_width=parse_int(data.get("chan_width", 300), "application.chan_width"),
    )


def _parse_user_interface(data: Dict[str, Any]) -> UserInterfaceSpec:
    pin_map = require_string(data, "pin_map")
    if pin_map != "auto":
        raise ValueError("only user_interface.pin_map: auto is supported; pad indices must be extracted")
    input_register = _parse_register(require_mapping(data, "input_register"), "user_interface.input_register")
    output_register = _parse_register(require_mapping(data, "output_register"), "user_interface.output_register")
    if input_register.width > 32 or output_register.width > 32:
        raise ValueError("packed USER_INPUT/USER_OUTPUT MMIO registers currently support widths <= 32")
    names = [field.name for field in input_register.fields + output_register.fields]
    duplicates = sorted({name for name in names if names.count(name) > 1})
    if duplicates:
        raise ValueError(f"field names must be unique across input/output registers: {duplicates}")
    return UserInterfaceSpec(
        pin_map=pin_map,
        input_register=input_register,
        output_register=output_register,
    )


def _parse_chipyard(data: Dict[str, Any], demo_name: str) -> ChipyardSpec:
    wrapper_default = f"{_pascal_case(demo_name)}Wrapper"
    return ChipyardSpec(
        config_name=_require_identifier(
            require_string(data, "config_name") if "config_name" in data else "OpenFPGADemoRocketConfig",
            "chipyard.config_name",
        ),
        peripheral_name=require_string(data, "peripheral_name") if "peripheral_name" in data else demo_name,
        wrapper_module=_require_identifier(
            require_string(data, "wrapper_module") if "wrapper_module" in data else wrapper_default,
            "chipyard.wrapper_module",
        ),
        scala_object=_require_identifier(
            require_string(data, "scala_object") if "scala_object" in data else "OpenFPGAGenerated",
            "chipyard.scala_object",
        ),
    )


def load_demo_config(path: Path) -> DemoConfig:
    config_path = path if path.is_absolute() else ROOT / path
    with config_path.open("r", encoding="utf-8") as fp:
        raw = yaml.safe_load(fp)
    if not isinstance(raw, dict):
        raise ValueError(f"expected YAML mapping in {config_path}")

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
        user_interface=_parse_user_interface(require_mapping(raw, "user_interface")),
        soc=SocSpec(
            base_address=parse_int(require_mapping(raw, "soc")["base_address"], "soc.base_address"),
            size=parse_int(require_mapping(raw, "soc")["size"], "soc.size"),
        ),
        chipyard=_parse_chipyard(require_mapping(raw, "chipyard") if "chipyard" in raw else {}, name),
    )
