#!/usr/bin/env python3
"""YAML schema and naming helpers for OpenFPGA Chipyard demos."""

from __future__ import annotations

import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

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
    address_width: int = 0
    data_width: int = 0

    @property
    def word_width(self) -> int:
        return self.address_width + self.data_width

    @property
    def is_frame_based(self) -> bool:
        return self.kind == "frame_based"


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
class FlowSpec:
    fpga_flow: str = "vpr_blif"
    inject_route_chan_width: bool = True
    explicit_port_mapping: bool = False
    yosys_tmpl: Optional[str] = None
    ys_rewrite_tmpl: Tuple[str, ...] = ()
    extra_template_vars: Dict[str, str] = field(default_factory=dict)
    extra_flags: Tuple[str, ...] = ()


@dataclass(frozen=True)
class ApplicationSpec:
    name: str
    top_module: str
    benchmark_blif: Optional[str]
    benchmark_verilog: str
    benchmark_act: Optional[str]
    chan_width: int
    clock_ports: Tuple[str, ...] = ()
    reset_ports: Tuple[str, ...] = ()


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
    fabric_name: str
    openfpga_root: Path
    architecture: ArchitectureSpec
    flow: FlowSpec
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
    def fabric_vsrc_dir(self) -> Path:
        return CHIPYARD_VSRC / self.fabric_name

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
        raise ValueError("frame_based config protocol address_width and data_width must be positive")

    if protocol.word_width > 32:
        raise ValueError(
            f"frame_based config word width {protocol.word_width} exceeds the current "
            "MMIO config-word backend limit of 32 bits"
        )
    return protocol


def _parse_architecture(data: Dict[str, Any]) -> ArchitectureSpec:
    return ArchitectureSpec(
        vpr_arch=require_string(data, "vpr_arch", "architecture"),
        openfpga_arch=require_string(data, "openfpga_arch", "architecture"),
        shell_template=require_string(data, "shell_template", "architecture"),
        simulation_setting=require_string(data, "simulation_setting", "architecture"),
        config_protocol=_parse_protocol(require_mapping(data, "config_protocol", "architecture")),
    )


def _optional_string(data: Dict[str, Any], key: str, context: str) -> Optional[str]:
    if key not in data or data[key] is None:
        return None
    return require_string(data, key, context)


def _require_bool(data: Dict[str, Any], key: str, context: str, *, default: bool) -> bool:
    value = data.get(key, default)
    if type(value) is not bool:
        raise TypeError(f"{context}.{key} must be a boolean, got {value!r}")
    return value


def _string_tuple(value: Any, label: str) -> Tuple[str, ...]:
    if value is None:
        return ()
    if isinstance(value, str):
        return (value,)
    if isinstance(value, list) and all(isinstance(item, str) and item for item in value):
        return tuple(value)
    raise TypeError(f"{label} must be a non-empty string or a list of non-empty strings")


def _identifier_tuple(value: Any, label: str) -> Tuple[str, ...]:
    values = _string_tuple(value, label)
    result = tuple(_require_identifier(item, f"{label} entry") for item in values)
    duplicates = sorted({item for item in result if result.count(item) > 1})
    if duplicates:
        raise ValueError(f"{label} contains duplicate entries: {duplicates}")
    return result


def _parse_extra_template_vars(data: Dict[str, Any]) -> Dict[str, str]:
    raw = data.get("extra_template_vars", {})
    if not isinstance(raw, dict):
        raise TypeError("flow.extra_template_vars must be a mapping")

    result: Dict[str, str] = {}
    for key, value in raw.items():
        if not isinstance(key, str) or not _IDENT_RE.fullmatch(key):
            raise ValueError(f"flow.extra_template_vars key must be an identifier, got {key!r}")
        if not isinstance(value, (str, int)):
            raise TypeError(f"flow.extra_template_vars.{key} must be a string or integer, got {value!r}")
        result[key] = str(value)
    return result


def _parse_flow(data: Dict[str, Any]) -> FlowSpec:
    kind = require_string(data, "fpga_flow", "flow", default="vpr_blif")
    if kind not in ("vpr_blif", "yosys_vpr"):
        raise ValueError(f"unsupported flow.fpga_flow {kind!r}; supported flows are vpr_blif and yosys_vpr")

    extra_flags = _string_tuple(data.get("extra_flags"), "flow.extra_flags")
    for flag in extra_flags:
        if not _IDENT_RE.fullmatch(flag):
            raise ValueError(f"flow.extra_flags entries must be parser flag names, got {flag!r}")

    return FlowSpec(
        fpga_flow=kind,
        inject_route_chan_width=_require_bool(data, "inject_route_chan_width", "flow", default=True),
        explicit_port_mapping=_require_bool(data, "explicit_port_mapping", "flow", default=False),
        yosys_tmpl=_optional_string(data, "yosys_tmpl", "flow"),
        ys_rewrite_tmpl=_string_tuple(data.get("ys_rewrite_tmpl"), "flow.ys_rewrite_tmpl"),
        extra_template_vars=_parse_extra_template_vars(data),
        extra_flags=extra_flags,
    )


def _parse_application(data: Dict[str, Any]) -> ApplicationSpec:
    clock_ports = _identifier_tuple(data.get("clock_ports"), "application.clock_ports")
    reset_ports = _identifier_tuple(data.get("reset_ports"), "application.reset_ports")
    overlap = sorted(set(clock_ports) & set(reset_ports))
    if overlap:
        raise ValueError(f"application clock_ports and reset_ports overlap: {overlap}")

    return ApplicationSpec(
        name=_require_identifier(require_string(data, "name", "application"), "application.name"),
        top_module=_require_identifier(require_string(data, "top_module", "application"), "application.top_module"),
        benchmark_blif=_optional_string(data, "benchmark_blif", "application"),
        benchmark_verilog=require_string(data, "benchmark_verilog", "application"),
        benchmark_act=_optional_string(data, "benchmark_act", "application"),
        chan_width=require_int(data, "chan_width", "application", default=300),
        clock_ports=clock_ports,
        reset_ports=reset_ports,
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


def _merge_mapping(base: Dict[str, Any], override: Dict[str, Any]) -> Dict[str, Any]:
    result = dict(base)
    for key, value in override.items():
        if isinstance(value, dict) and isinstance(result.get(key), dict):
            result[key] = _merge_mapping(result[key], value)
        else:
            result[key] = value
    return result


def _select_benchmark(raw: Dict[str, Any], benchmark: Optional[str]) -> Tuple[str, Dict[str, Any]]:
    benchmarks = require_mapping(raw, "benchmarks")
    if benchmark is None:
        choices = ", ".join(sorted(benchmarks))
        raise ValueError(f"{raw['name']} defines multiple benchmarks; pass --benchmark <name>. Choices: {choices}")
    if benchmark in benchmarks:
        selected = require_mapping(benchmarks, benchmark, "benchmarks")
        return benchmark, dict(selected)

    for key, value in benchmarks.items():
        selected = require_mapping(benchmarks, key, "benchmarks")
        names = [key]
        if "name" in selected:
            names.append(require_string(selected, "name", f"benchmarks.{key}"))
        if "application" in selected:
            app = require_mapping(selected, "application", f"benchmarks.{key}")
            if "name" in app:
                names.append(require_string(app, "name", f"benchmarks.{key}.application"))
        if benchmark in names:
            return key, dict(selected)

    choices = ", ".join(sorted(benchmarks))
    raise ValueError(f"unknown benchmark {benchmark!r} in {raw['name']}; choices: {choices}")


def _load_multi_benchmark_config(
    config_path: Path,
    raw: Dict[str, Any],
    *,
    benchmark: Optional[str],
) -> DemoConfig:
    fabric_name = require_string(raw, "name")
    if not _FILE_STEM_RE.fullmatch(fabric_name):
        raise ValueError(f"name must be a stable fabric file/directory stem, got {fabric_name!r}")

    benchmark_key, selected = _select_benchmark(raw, benchmark)
    demo_name = require_string(selected, "name", f"benchmarks.{benchmark_key}")
    if not _FILE_STEM_RE.fullmatch(demo_name):
        raise ValueError(f"benchmarks.{benchmark_key}.name must be a stable file/directory stem, got {demo_name!r}")
    _require_identifier(_c_identifier(demo_name), "derived C identifier")

    openfpga_root = resolve_under(ROOT, require_string(raw, "openfpga_root"))
    ensure_dir(openfpga_root, "OpenFPGA root")

    architecture_raw = _merge_mapping(
        dict(require_mapping(raw, "architecture")),
        dict(require_mapping(selected, "architecture", default={})),
    )
    flow_raw = _merge_mapping(
        dict(require_mapping(raw, "flow", default={})),
        dict(require_mapping(selected, "flow", default={})),
    )
    soc_raw = _merge_mapping(
        dict(require_mapping(raw, "soc")),
        dict(require_mapping(selected, "soc", default={})),
    )

    return DemoConfig(
        path=config_path,
        raw=raw,
        name=demo_name,
        fabric_name=fabric_name,
        openfpga_root=openfpga_root,
        architecture=_parse_architecture(architecture_raw),
        flow=_parse_flow(flow_raw),
        application=_parse_application(require_mapping(selected, "application", f"benchmarks.{benchmark_key}")),
        soc=SocSpec(
            base_address=require_int(soc_raw, "base_address", "soc"),
            size=require_int(soc_raw, "size", "soc"),
        ),
        chipyard=_parse_chipyard(
            require_mapping(selected, "chipyard", f"benchmarks.{benchmark_key}") if "chipyard" in selected else {},
            demo_name,
        ),
    )


def load_demo_config(path: Path, *, benchmark: Optional[str] = None) -> DemoConfig:
    config_path = path if path.is_absolute() else ROOT / path
    raw = load_yaml_mapping(config_path)

    if "benchmarks" in raw:
        return _load_multi_benchmark_config(config_path, raw, benchmark=benchmark)
    if benchmark is not None:
        raise ValueError(f"{config_path} is a single-benchmark config; do not pass --benchmark")

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
        fabric_name=name,
        openfpga_root=openfpga_root,
        architecture=_parse_architecture(require_mapping(raw, "architecture")),
        flow=_parse_flow(dict(require_mapping(raw, "flow", default={}))),
        application=_parse_application(require_mapping(raw, "application")),
        soc=SocSpec(
            base_address=require_int(require_mapping(raw, "soc"), "base_address", "soc"),
            size=require_int(require_mapping(raw, "soc"), "size", "soc"),
        ),
        chipyard=_parse_chipyard(require_mapping(raw, "chipyard") if "chipyard" in raw else {}, name),
    )
