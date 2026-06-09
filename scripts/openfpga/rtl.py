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
        if base in CELL_LIBRARY_FILES:
            includes.append(f"SRC/cell_library/{base}")
            continue
        normalized = include_path.replace("\\", "/")
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
        return includes
    if _rtl_uses_const_cells(src_dir):
        raise ValueError(
            "generated RTL instantiates const0/const1 from inv_buf_passgate.v; "
            "cannot replace inv_buf_passgate.v with OpenFPGA cell-library inv/buf models"
        )

    rewritten: List[str] = []
    for include in includes:
        if include == INV_BUF_PASSGATE_INCLUDE:
            rewritten.extend(VERILATOR_FRIENDLY_INV_BUF_INCLUDES)
        else:
            rewritten.append(include)
    return rewritten


def sync_rtl(demo: DemoConfig, paths: GeneratedPaths, user_interface: UserInterfaceSpec, pin_map: PinMap) -> None:
    target = paths.vsrc_dir
    if target.exists():
        shutil.rmtree(target)
    (target / "SRC").mkdir(parents=True)

    for filename in ("fpga_defines.v", "fpga_top.v"):
        shutil.copy2(paths.src_dir / filename, target / "SRC" / filename)

    for dirname in ("sub_module", "lb", "routing"):
        shutil.copytree(paths.src_dir / dirname, target / "SRC" / dirname)

    cell_src = demo.openfpga_root / "openfpga_flow" / "openfpga_cell_library" / "verilog"
    cell_dst = target / "SRC" / "cell_library"
    cell_dst.mkdir()
    for filename in CELL_LIBRARY_FILES:
        shutil.copy2(ensure_file(cell_src / filename, f"OpenFPGA cell library {filename}"), cell_dst / filename)

    includes = parse_include_order(paths.src_dir / "fabric_netlists.v")
    includes = _use_verilator_friendly_inv_buf_cells(includes, target / "SRC")

    fpga_ports = _parse_fpga_top_ports(target / "SRC" / "fpga_top.v")
    _validate_fpga_top_ports(demo, fpga_ports, pin_map)
    _write_manifest(target / demo.manifest_filename, demo, includes)
    _write_wrapper(demo.wrapper_path, demo, user_interface, pin_map, fpga_ports)


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
    for scalar in ("set", "reset", "clk", "enable"):
        port = _require_port(ports, scalar, "input")
        if port.width != 1:
            raise ValueError(f"fpga_top port {scalar!r} must be scalar")

    address = _require_port(ports, "address", "input")
    data_in = _require_port(ports, "data_in", "input")
    gpio = _require_port(ports, "gfpga_pad_GPIO_PAD", "inout")
    if address.width != cfg.address_width:
        raise ValueError(f"fpga_top address width {address.width} != YAML {cfg.address_width}")
    if data_in.width != cfg.data_width:
        raise ValueError(f"fpga_top data_in width {data_in.width} != YAML {cfg.data_width}")
    if gpio.width != pin_map.pad_count:
        raise ValueError(f"fpga_top GPIO pad width {gpio.width} != extracted pin_map {pin_map.pad_count}")
    _validate_indexed_zero_based(address)
    _validate_indexed_zero_based(data_in)
    _validate_indexed_zero_based(gpio)


def _write_manifest(path: Path, demo: DemoConfig, includes: List[str]) -> None:
    required = [
        "SRC/fpga_defines.v",
        "SRC/cell_library/dff.v",
        "SRC/cell_library/latch.v",
        "SRC/cell_library/gpio.v",
        "SRC/cell_library/mux2.v",
        "SRC/fpga_top.v",
    ]
    missing = [item for item in required if item not in includes]
    if missing:
        raise ValueError(f"manifest include list is missing required files: {missing}")

    guard = f"{demo.macro_prefix}_MANIFEST_V"
    lines = [
        "// Auto-generated by scripts/openfpga/rtl.py.",
        f"// Chipyard include manifest for {demo.name}.",
        f"`ifndef {guard}",
        f"`define {guard}",
        "",
    ]
    for include in includes:
        include_path = (path.parent / include).resolve()
        lines.append(f'`include "{include_path}"')
    lines.extend(["", "`endif"])
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def _signal_ref(signal: str, port: PortDecl) -> str:
    if port.width_range is None:
        return f"{signal}[0]"
    return port.width_range.select(signal)


def _write_wrapper(
    path: Path,
    demo: DemoConfig,
    user_interface: UserInterfaceSpec,
    pin_map: PinMap,
    fpga_ports: Dict[str, PortDecl],
) -> None:
    cfg = demo.architecture.config_protocol
    input_reg = user_interface.input_register
    output_reg = user_interface.output_register
    pad_left = pin_map.pad_left
    pad_right = pin_map.pad_right
    pad_min = min(pad_left, pad_right)
    pad_max = max(pad_left, pad_right)
    driven_pads = {pad for pads in pin_map.inputs.values() for pad in pads}
    output_pads = {pad for pads in pin_map.outputs.values() for pad in pads}
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
        f"  wire [{pad_left}:{pad_right}] gfpga_pad_GPIO_PAD;",
        f"  wire [0:{cfg.address_width - 1}] fabric_cfg_address;",
        f"  wire [0:{cfg.data_width - 1}] fabric_cfg_data;",
        "",
    ]

    for index in range(cfg.address_width):
        lines.append(f"  assign fabric_cfg_address[{index}] = cfg_address[{index}];")
    for index in range(cfg.data_width):
        lines.append(f"  assign fabric_cfg_data[{index}] = cfg_data[{index}];")
    lines.append("")

    for field in input_reg.fields:
        for bit, pad in enumerate(pin_map.inputs[field.name]):
            lines.append(f"  assign gfpga_pad_GPIO_PAD[{pad}] = user_input[{field.lsb + bit}];")
    for field in output_reg.fields:
        for bit, pad in enumerate(pin_map.outputs[field.name]):
            lines.append(f"  assign user_output[{field.lsb + bit}] = gfpga_pad_GPIO_PAD[{pad}];")

    output_field_bits = {bit for field in output_reg.fields for bit in field.bit_indices}
    for bit in range(output_reg.width):
        if bit not in output_field_bits:
            lines.append(f"  assign user_output[{bit}] = 1'b0;")
    lines.append("")

    lines.append("  // Tie unused pads low. Pads used as fabric outputs are left undriven here.")
    for pad in range(pad_min, pad_max + 1):
        if pad in driven_pads or pad in output_pads:
            continue
        lines.append(f"  assign gfpga_pad_GPIO_PAD[{pad}] = 1'b0;")

    lines.extend(
        [
            "",
            "  fpga_top fabric (",
            "    .set(1'b0),",
            "    .reset(reset),",
            "    .clk(clock),",
            f"    .gfpga_pad_GPIO_PAD(gfpga_pad_GPIO_PAD[{pad_left}:{pad_right}]),",
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
