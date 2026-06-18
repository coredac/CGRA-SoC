#!/usr/bin/env python3
"""Sync generated OpenFPGA RTL into Chipyard resources and emit wrappers."""

from __future__ import annotations

import re
import shutil
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional

from config import CELL_LIBRARY_FILES, DemoConfig, UserInterfaceSpec, ensure_file
from flow import GeneratedPaths
from pinmap import PinMap


INV_BUF_PASSGATE_INCLUDE = "SRC/sub_module/inv_buf_passgate.v"
VERILATOR_FRIENDLY_INV_BUF_INCLUDES = (
    "SRC/cell_library/inv.v",
    "SRC/cell_library/buf4.v",
    "SRC/cell_library/tap_buf4.v",
)
GENERATED_DATE_RE = re.compile(r"^//\s*Date:.*$")


@dataclass(frozen=True)
class PortRange:
    left: int
    right: int

    @property
    def width(self) -> int:
        return abs(self.left - self.right) + 1

    def select(self, signal: str) -> str:
        return f"{signal}[{self.left}:{self.right}]"


@dataclass(frozen=True)
class PortDecl:
    direction: str
    name: str
    width_range: Optional[PortRange]

    @property
    def width(self) -> int:
        return self.width_range.width if self.width_range is not None else 1


def parse_include_order(fabric_netlists: Path) -> List[str]:
    text = fabric_netlists.read_text(encoding="utf-8")
    includes: List[str] = []
    for include_path in re.findall(r'^\s*`include\s+"([^"]+)"', text, flags=re.MULTILINE):
        base = Path(include_path).name
        normalized = include_path.replace("\\", "/")
        if "/openfpga_cell_library/verilog/" in normalized:
            includes.append(f"SRC/cell_library/{base}")
            continue
        if base in CELL_LIBRARY_FILES:
            includes.append(f"SRC/cell_library/{base}")
            continue
        if normalized.startswith("./"):
            normalized = normalized[2:]
        if normalized.startswith("SRC/"):
            includes.append(normalized)
            continue
        raise ValueError(f"unexpected include in {fabric_netlists}: {include_path}")
    return includes


def _rtl_uses_const_cells(src_dir: Path) -> bool:
    inst_re = re.compile(r"^\s*(const0|const1)\s+\w+\s*\(", flags=re.MULTILINE)
    for path in src_dir.rglob("*.v"):
        if path.name == "inv_buf_passgate.v" or "/cell_library/" in path.as_posix():
            continue
        if inst_re.search(path.read_text(encoding="utf-8")):
            return True
    return False


def _use_verilator_friendly_inv_buf_cells(includes: List[str], src_dir: Path) -> List[str]:
    if INV_BUF_PASSGATE_INCLUDE not in includes:
        return _dedupe_includes(includes)
    if _rtl_uses_const_cells(src_dir):
        return _dedupe_includes(includes)

    rewritten: List[str] = []
    seen = set()
    for include in includes:
        if include == INV_BUF_PASSGATE_INCLUDE:
            for replacement in VERILATOR_FRIENDLY_INV_BUF_INCLUDES:
                if replacement not in seen:
                    rewritten.append(replacement)
                    seen.add(replacement)
        else:
            if include not in seen:
                rewritten.append(include)
                seen.add(include)
    return rewritten


def _dedupe_includes(includes: List[str]) -> List[str]:
    rewritten: List[str] = []
    seen = set()
    for include in includes:
        if include not in seen:
            rewritten.append(include)
            seen.add(include)
    return rewritten


def sync_rtl(demo: DemoConfig, paths: GeneratedPaths, user_interface: UserInterfaceSpec, pin_map: PinMap) -> None:
    wrapper_root = paths.vsrc_dir
    fabric_root = demo.fabric_vsrc_dir

    if wrapper_root.exists():
        shutil.rmtree(wrapper_root)
    wrapper_root.mkdir(parents=True)

    staging_root = paths.workdir / "chipyard_vsrc_staging"
    if staging_root.exists():
        shutil.rmtree(staging_root)

    includes = parse_include_order(paths.src_dir / "fabric_netlists.v")
    includes = _prepare_fabric_src(demo, paths.src_dir, staging_root, includes)
    _sync_fabric_src(staging_root / "SRC", fabric_root / "SRC", demo)
    shutil.rmtree(staging_root)

    fpga_ports = _parse_fpga_top_ports(fabric_root / "SRC" / "fpga_top.v")
    _validate_fpga_top_ports(demo, fpga_ports, pin_map)
    _write_manifest(wrapper_root / demo.manifest_filename, demo, includes, fabric_root)
    _write_wrapper(demo.wrapper_path, demo, user_interface, pin_map, fpga_ports)


def _prepare_fabric_src(demo: DemoConfig, src_dir: Path, target: Path, includes: List[str]) -> List[str]:
    (target / "SRC").mkdir(parents=True)

    for filename in ("fpga_defines.v", "fpga_top.v"):
        shutil.copy2(src_dir / filename, target / "SRC" / filename)

    for dirname in ("sub_module", "lb", "routing", "tile"):
        src_subdir = src_dir / dirname
        if src_subdir.is_dir():
            shutil.copytree(src_subdir, target / "SRC" / dirname)

    includes = _use_verilator_friendly_inv_buf_cells(includes, target / "SRC")
    _copy_cell_library_files(demo, target, includes)
    return includes


def _sync_fabric_src(staged_src: Path, fabric_src: Path, demo: DemoConfig) -> None:
    if fabric_src.exists():
        if not _src_trees_match_ignoring_dates(staged_src, fabric_src):
            raise ValueError(
                f"generated fabric SRC for {demo.name} differs from shared fabric "
                f"{demo.fabric_name}; use a distinct top-level multi-benchmark config"
            )
        return

    fabric_src.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(staged_src, fabric_src)


def _src_trees_match_ignoring_dates(left: Path, right: Path) -> bool:
    left_files = sorted(path.relative_to(left) for path in left.rglob("*") if path.is_file())
    right_files = sorted(path.relative_to(right) for path in right.rglob("*") if path.is_file())
    if left_files != right_files:
        return False
    return all(
        _read_verilog_without_generated_dates(left / relpath)
        == _read_verilog_without_generated_dates(right / relpath)
        for relpath in left_files
    )


def _read_verilog_without_generated_dates(path: Path) -> List[str]:
    return [
        line
        for line in path.read_text(encoding="utf-8").splitlines()
        if GENERATED_DATE_RE.fullmatch(line) is None
    ]


def _copy_cell_library_files(demo: DemoConfig, target: Path, includes: List[str]) -> None:
    cell_files = sorted(
        {Path(include).name for include in includes if include.startswith("SRC/cell_library/")}
    )
    cell_src = demo.openfpga_root / "openfpga_flow" / "openfpga_cell_library" / "verilog"
    cell_dst = target / "SRC" / "cell_library"
    cell_dst.mkdir()
    for filename in cell_files:
        shutil.copy2(ensure_file(cell_src / filename, f"OpenFPGA cell library {filename}"), cell_dst / filename)


def _parse_fpga_top_ports(path: Path) -> Dict[str, PortDecl]:
    text = path.read_text(encoding="utf-8")
    ports: Dict[str, PortDecl] = {}
    decl_re = re.compile(
        r"^\s*(input|output|inout)\s+(?:wire\s+|reg\s+)?"
        r"(?:(\[\s*\d+\s*:\s*\d+\s*\])\s+)?(\w+)\s*;",
        flags=re.MULTILINE,
    )
    for direction, raw_range, name in decl_re.findall(text):
        width_range: Optional[PortRange] = None
        if raw_range:
            left, right = [int(part) for part in re.findall(r"\d+", raw_range)]
            width_range = PortRange(left=left, right=right)
        if name in ports:
            raise ValueError(f"duplicate fpga_top port declaration {name!r} in {path}")
        ports[name] = PortDecl(direction=direction, name=name, width_range=width_range)
    return ports


def _require_port(ports: Dict[str, PortDecl], name: str, direction: str) -> PortDecl:
    port = ports.get(name)
    if port is None:
        raise ValueError(f"fpga_top is missing required port {name!r}")
    if port.direction != direction:
        raise ValueError(f"fpga_top port {name!r} direction is {port.direction}, expected {direction}")
    return port


def _require_scalar_port(ports: Dict[str, PortDecl], name: str, direction: str) -> PortDecl:
    port = _require_port(ports, name, direction)
    if port.width != 1:
        raise ValueError(f"fpga_top port {name!r} must be scalar")
    return port


def _validate_indexed_zero_based(port: PortDecl) -> None:
    if port.width_range is None:
        if port.width != 1:
            raise ValueError(f"internal error: scalar port {port.name} has non-1 width")
        return
    indices = set(range(min(port.width_range.left, port.width_range.right), max(port.width_range.left, port.width_range.right) + 1))
    expected = set(range(port.width))
    if indices != expected:
        raise ValueError(
            f"fpga_top port {port.name!r} range [{port.width_range.left}:{port.width_range.right}] "
            f"is not zero-based width {port.width}"
        )


def _validate_fpga_top_ports(demo: DemoConfig, ports: Dict[str, PortDecl], pin_map: PinMap) -> None:
    cfg = demo.architecture.config_protocol
    _validate_pad_port(ports, pin_map.input_pad_port, pin_map.input_pad_count)
    if pin_map.output_pad_port != pin_map.input_pad_port:
        _validate_pad_port(ports, pin_map.output_pad_port, pin_map.output_pad_count)

    _validate_frame_based_fpga_top_ports(ports, cfg)


def _validate_pad_port(ports: Dict[str, PortDecl], name: str, expected_width: int) -> None:
    port = _require_port(ports, name, "inout")
    if port.width != expected_width:
        raise ValueError(f"fpga_top pad port {name} width {port.width} != extracted pin_map {expected_width}")
    _validate_indexed_zero_based(port)


def _validate_frame_based_fpga_top_ports(ports: Dict[str, PortDecl], cfg) -> None:
    for scalar in ("set", "reset", "clk", "enable"):
        _require_scalar_port(ports, scalar, "input")

    address = _require_port(ports, "address", "input")
    data_in = _require_port(ports, "data_in", "input")
    if address.width != cfg.address_width:
        raise ValueError(f"fpga_top address width {address.width} != YAML {cfg.address_width}")
    if data_in.width != cfg.data_width:
        raise ValueError(f"fpga_top data_in width {data_in.width} != YAML {cfg.data_width}")
    _validate_indexed_zero_based(address)
    _validate_indexed_zero_based(data_in)


def _write_manifest(path: Path, demo: DemoConfig, includes: List[str], fabric_root: Path) -> None:
    required = [
        "SRC/fpga_defines.v",
        "SRC/fpga_top.v",
    ]
    missing = [item for item in required if item not in includes]
    if missing:
        raise ValueError(f"manifest include list is missing required files: {missing}")
    missing_files = [item for item in includes if not (fabric_root / item).is_file()]
    if missing_files:
        raise ValueError(f"manifest include list references missing files: {missing_files}")

    guard = f"{demo.macro_prefix}_MANIFEST_V"
    lines = [
        "// Auto-generated by scripts/openfpga/rtl.py.",
        f"// Chipyard include manifest for {demo.name}.",
        f"`ifndef {guard}",
        f"`define {guard}",
        "",
    ]
    for include in includes:
        include_path = (fabric_root / include).resolve()
        lines.append(f'`include "{include_path}"')
    lines.extend(["", "`endif"])
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def _signal_ref(signal: str, port: PortDecl) -> str:
    if port.width_range is None:
        return f"{signal}[0]"
    return port.width_range.select(signal)


def _append_pad_wire_declarations(lines: List[str], pin_map: PinMap) -> None:
    lines.append(
        f"  wire [{pin_map.input_pad_left}:{pin_map.input_pad_right}] {pin_map.input_pad_port};"
    )
    if pin_map.output_pad_port != pin_map.input_pad_port:
        lines.append(
            f"  wire [{pin_map.output_pad_left}:{pin_map.output_pad_right}] {pin_map.output_pad_port};"
        )
    lines.append("")


def _append_fpga_pad_connections(lines: List[str], pin_map: PinMap) -> None:
    lines.append(
        f"    .{pin_map.input_pad_port}({pin_map.input_pad_port}"
        f"[{pin_map.input_pad_left}:{pin_map.input_pad_right}]),"
    )
    if pin_map.output_pad_port != pin_map.input_pad_port:
        lines.append(
            f"    .{pin_map.output_pad_port}({pin_map.output_pad_port}"
            f"[{pin_map.output_pad_left}:{pin_map.output_pad_right}]),"
        )


def _append_user_pad_wiring(
    lines: List[str],
    user_interface: UserInterfaceSpec,
    pin_map: PinMap,
) -> None:
    input_reg = user_interface.input_register
    output_reg = user_interface.output_register

    for field in input_reg.fields:
        for bit, pad in enumerate(pin_map.inputs[field.name]):
            source = f"user_input[{field.lsb + bit}]"
            lines.append(f"  assign {pin_map.input_pad_port}[{pad}] = {source};")
    for field in output_reg.fields:
        for bit, pad in enumerate(pin_map.outputs[field.name]):
            source = f"{pin_map.output_pad_port}[{pad}]"
            lines.append(f"  assign user_output[{field.lsb + bit}] = {source};")

    output_field_bits = {bit for field in output_reg.fields for bit in field.bit_indices}
    for bit in range(output_reg.width):
        if bit not in output_field_bits:
            lines.append(f"  assign user_output[{bit}] = 1'b0;")
    lines.append("")


def _append_clock_pad_wiring(
    lines: List[str],
    demo: DemoConfig,
    pin_map: PinMap,
    clock_source: str,
) -> None:
    for port_name in demo.application.clock_ports:
        pads = pin_map.inputs.get(port_name)
        if pads is None:
            raise ValueError(f"configured clock port {port_name!r} is missing from extracted pin map")
        if len(pads) != 1:
            raise ValueError(
                f"configured clock port {port_name!r} must be scalar; extracted pads are {pads}"
            )
        lines.append(f"  assign {pin_map.input_pad_port}[{pads[0]}] = {clock_source};")
    if demo.application.clock_ports:
        lines.append("")


def _append_unused_pad_tieoffs(lines: List[str], user_interface: UserInterfaceSpec, pin_map: PinMap) -> None:
    output_reg = user_interface.output_register
    pad_min = min(pin_map.input_pad_left, pin_map.input_pad_right)
    pad_max = max(pin_map.input_pad_left, pin_map.input_pad_right)
    driven_pads = {pad for pads in pin_map.inputs.values() for pad in pads}
    output_pads = (
        {pad for field in output_reg.fields for pad in pin_map.outputs[field.name]}
        if pin_map.output_pad_port == pin_map.input_pad_port
        else set()
    )

    lines.append(
        f"  // Tie unused {pin_map.input_pad_port} pads low. "
        "Pads used as fabric outputs are left undriven here."
    )
    for pad in range(pad_min, pad_max + 1):
        if pad in driven_pads or pad in output_pads:
            continue
        lines.append(f"  assign {pin_map.input_pad_port}[{pad}] = 1'b0;")
    lines.append("")


def _write_wrapper(
    path: Path,
    demo: DemoConfig,
    user_interface: UserInterfaceSpec,
    pin_map: PinMap,
    fpga_ports: Dict[str, PortDecl],
) -> None:
    _write_frame_based_wrapper(path, demo, user_interface, pin_map, fpga_ports)


def _write_frame_based_wrapper(
    path: Path,
    demo: DemoConfig,
    user_interface: UserInterfaceSpec,
    pin_map: PinMap,
    fpga_ports: Dict[str, PortDecl],
) -> None:
    cfg = demo.architecture.config_protocol
    input_reg = user_interface.input_register
    output_reg = user_interface.output_register
    manifest = (path.parent / demo.manifest_filename).resolve()
    address_port = fpga_ports["address"]
    data_port = fpga_ports["data_in"]

    lines = [
        "// Auto-generated by scripts/openfpga/rtl.py.",
        f"`include \"{manifest}\"",
        "",
        "`default_nettype none",
        f"module {demo.chipyard.wrapper_module}(",
        "  input        clock,",
        "  input        reset,",
        "  input        cfg_we,",
        f"  input [{cfg.address_width - 1}:0] cfg_address,",
        f"  input [{cfg.data_width - 1}:0] cfg_data,",
        f"  input [{input_reg.width - 1}:0] user_input,",
        f"  output [{output_reg.width - 1}:0] user_output",
        ");",
        "",
        f"  wire [0:{cfg.address_width - 1}] fabric_cfg_address;",
        f"  wire [0:{cfg.data_width - 1}] fabric_cfg_data;",
        "",
    ]
    _append_pad_wire_declarations(lines, pin_map)

    for index in range(cfg.address_width):
        lines.append(f"  assign fabric_cfg_address[{index}] = cfg_address[{index}];")
    for index in range(cfg.data_width):
        lines.append(f"  assign fabric_cfg_data[{index}] = cfg_data[{index}];")
    lines.append("")

    _append_clock_pad_wiring(lines, demo, pin_map, "clock")
    _append_user_pad_wiring(lines, user_interface, pin_map)
    _append_unused_pad_tieoffs(lines, user_interface, pin_map)

    lines.extend(
        [
            "",
            "  fpga_top fabric (",
            "    .set(1'b0),",
            "    .reset(reset),",
            "    .clk(clock),",
        ]
    )
    _append_fpga_pad_connections(lines, pin_map)
    lines.extend(
        [
            "    .enable(cfg_we),",
            f"    .address({_signal_ref('fabric_cfg_address', address_port)}),",
            f"    .data_in({_signal_ref('fabric_cfg_data', data_port)})",
            "  );",
            "",
            "endmodule",
            "`default_nettype wire",
        ]
    )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
