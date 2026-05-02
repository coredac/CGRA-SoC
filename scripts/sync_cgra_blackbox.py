#!/usr/bin/env python3
"""
Sync PyMTL3-generated CGRA RTL into the Chipyard Chisel BlackBox wrapper.

The PyMTL3 Verilog is the source of truth. This script parses its top-level
module, extracts packet/data widths and boundary array sizes, then emits:

  1. a flat SystemVerilog wrapper for Chisel BlackBox compatibility
  2. a generated Scala object containing the matching CGRAParams
  3. a copy of the PyMTL3 RTL under Chipyard's vsrc directory
"""

from __future__ import annotations

import argparse
import re
import shutil
from dataclasses import dataclass
from pathlib import Path
from string import Template
from typing import Dict, Iterable, List, Optional, Tuple


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RTL = ROOT / "VectorCGRA" / "CgraRTL_2x2__pickled.v"
DEFAULT_VSRC = ROOT / "chipyard" / "generators" / "chipyard" / "src" / "main" / "resources" / "vsrc"
DEFAULT_SCALA = ROOT / "chipyard" / "generators" / "chipyard" / "src" / "main" / "scala" / "example" / "CGRAGenerated.scala"
DEFAULT_C_LAYOUT = ROOT / "tests" / "include" / "cgra_layout.h"
DEFAULT_TEMPLATE_DIR = ROOT / "scripts" / "templates"
SIDES = ("south", "north", "east", "west")


@dataclass(frozen=True)
class Port:
    direction: str
    name: str
    width: int
    sv_type: Optional[str] = None
    array_len: Optional[int] = None


@dataclass(frozen=True)
class CgraMetadata:
    top_module: str
    wrapper_module: str
    intra_type: str
    inter_type: str
    data_type: str
    payload_type: str
    ctrl_type: str
    intra_width: int
    inter_width: int
    data_width: int
    data_payload_width: int
    payload_width: int
    ctrl_width: int
    ctrl_hi_width: int
    cmd_width: int
    data_addr_width: int
    ctrl_addr_width: int
    id_width: int
    addr_width: int
    x_tiles: int
    y_tiles: int
    num_tiles: int
    address_lower: int
    address_upper: int
    rtl_resource: str
    wrapper_resource: str
    has_boundary_ports: bool


def range_width(msb: int, lsb: int) -> int:
    return abs(msb - lsb) + 1


def packed_dims(text: str) -> List[int]:
    return [range_width(int(msb), int(lsb)) for msb, lsb in re.findall(r"\[(\d+)\s*:\s*(\d+)\]", text)]


def packed_dims_width(text: str) -> int:
    width = 1
    for dim_width in packed_dims(text):
        width *= dim_width
    return width


def array_len(lo: int, hi: int) -> int:
    return abs(hi - lo) + 1


def extract_typedefs(text: str) -> Dict[str, str]:
    typedefs: Dict[str, str] = {}
    pattern = re.compile(
        r"typedef\s+struct\s+packed\s*\{(?P<body>.*?)\}\s*(?P<name>\w+)\s*;",
        re.S,
    )
    for match in pattern.finditer(text):
        typedefs[match.group("name")] = match.group("body")
    return typedefs


def strip_line(line: str) -> str:
    return line.split("//", 1)[0].strip().rstrip(",;").strip()


def parse_struct_fields(body: str) -> List[Tuple[str, str, int]]:
    fields: List[Tuple[str, str, int]] = []
    for raw_line in body.splitlines():
        line = strip_line(raw_line)
        if not line:
            continue
        logic_match = re.match(r"logic\s+(?P<dims>(?:\[[^\]]+\]\s*)*)\s*(?P<name>\w+)$", line)
        if logic_match:
            dims = logic_match.group("dims")
            width = packed_dims_width(dims) if dims else 1
            fields.append(("logic", logic_match.group("name"), width))
            continue
        type_match = re.match(r"(?P<type>\w+)\s+(?P<name>\w+)$", line)
        if type_match:
            fields.append((type_match.group("type"), type_match.group("name"), 0))
            continue
        raise ValueError(f"cannot parse typedef field: {raw_line}")
    return fields


def logic_field_dims(type_name: str, typedefs: Dict[str, str]) -> Dict[str, List[int]]:
    dims_by_name: Dict[str, List[int]] = {}
    for raw_line in typedefs[type_name].splitlines():
        line = strip_line(raw_line)
        if not line:
            continue
        logic_match = re.match(r"logic\s+(?P<dims>(?:\[[^\]]+\]\s*)*)\s*(?P<name>\w+)$", line)
        if logic_match:
            dims_by_name[logic_match.group("name")] = packed_dims(logic_match.group("dims"))
    return dims_by_name


def struct_width(type_name: str, typedefs: Dict[str, str], memo: Dict[str, int]) -> int:
    if type_name in memo:
        return memo[type_name]
    if type_name not in typedefs:
        raise ValueError(f"unknown struct typedef: {type_name}")
    total = 0
    for field_type, _field_name, field_width in parse_struct_fields(typedefs[type_name]):
        if field_type == "logic":
            total += field_width
        else:
            total += struct_width(field_type, typedefs, memo)
    memo[type_name] = total
    return total


def field_type(type_name: str, field_name: str, typedefs: Dict[str, str]) -> str:
    for field_type, name, _width in parse_struct_fields(typedefs[type_name]):
        if name == field_name:
            return field_type
    raise ValueError(f"{type_name} has no field named {field_name}")


def field_width(type_name: str, field_name: str, typedefs: Dict[str, str], memo: Dict[str, int]) -> int:
    for field_type, name, width in parse_struct_fields(typedefs[type_name]):
        if name != field_name:
            continue
        if field_type == "logic":
            return width
        return struct_width(field_type, typedefs, memo)
    raise ValueError(f"{type_name} has no field named {field_name}")


def field_offsets(type_name: str, typedefs: Dict[str, str], memo: Dict[str, int]) -> Dict[str, Tuple[int, int]]:
    fields = parse_struct_fields(typedefs[type_name])
    cursor = struct_width(type_name, typedefs, memo)
    offsets: Dict[str, Tuple[int, int]] = {}
    for field_type, name, width in fields:
        field_nbits = width if field_type == "logic" else struct_width(field_type, typedefs, memo)
        cursor -= field_nbits
        offsets[name] = (cursor, field_nbits)
    return offsets


def find_top_module(text: str, requested: Optional[str]) -> str:
    if requested:
        if not re.search(rf"^module\s+{re.escape(requested)}\s*\(", text, re.M):
            raise ValueError(f"requested top module not found: {requested}")
        return requested

    modules = re.findall(r"^module\s+(\w+)\s*\(", text, re.M)
    candidates = [
        name for name in modules
        if name.startswith(("CgraRTL", "CgraTemplateRTL")) and not name.endswith("_wrapper")
    ]
    if not candidates:
        raise ValueError("could not infer top module; pass --top-module")
    return candidates[-1]


def module_port_block(text: str, module_name: str) -> str:
    match = re.search(rf"^module\s+{re.escape(module_name)}\s*\((?P<body>.*?)^\);", text, re.M | re.S)
    if not match:
        raise ValueError(f"could not parse module port block for {module_name}")
    return match.group("body")


def parse_port_line(line: str, typedefs: Dict[str, str], memo: Dict[str, int]) -> Optional[Port]:
    line = strip_line(line)
    if not line:
        return None
    match = re.match(r"(?P<dir>input|output)\s+(?P<rest>.+)$", line)
    if not match:
        return None
    direction = match.group("dir")
    rest = match.group("rest").strip()
    port_match = re.match(
        r"(?P<prefix>.+?)\s+(?P<name>\w+)(?:\s+\[(?P<alo>\d+)\s*:\s*(?P<ahi>\d+)\])?$",
        rest,
    )
    if not port_match:
        raise ValueError(f"cannot parse module port: {line}")
    prefix = port_match.group("prefix").strip()
    name = port_match.group("name")
    alo = port_match.group("alo")
    ahi = port_match.group("ahi")
    arr_len = array_len(int(alo), int(ahi)) if alo is not None and ahi is not None else None

    if prefix.startswith("logic"):
        dims = prefix[len("logic"):].strip()
        width = packed_dims_width(dims) if dims else 1
        return Port(direction, name, width, None, arr_len)

    sv_type = prefix
    return Port(direction, name, struct_width(sv_type, typedefs, memo), sv_type, arr_len)


def parse_ports(text: str, module_name: str, typedefs: Dict[str, str], memo: Dict[str, int]) -> Dict[str, Port]:
    ports: Dict[str, Port] = {}
    for line in module_port_block(text, module_name).splitlines():
        port = parse_port_line(line, typedefs, memo)
        if port:
            ports[port.name] = port
    return ports


def require_port(ports: Dict[str, Port], name: str) -> Port:
    if name not in ports:
        raise ValueError(f"top module is missing required port {name}")
    return ports[name]


def infer_address_bounds(text: str, addr_width: int) -> Tuple[int, int]:
    match = re.search(r"controller2addr_map_\{0:\s*[\[\(](\d+),\s*(\d+)[\]\)]", text)
    if match:
        return int(match.group(1)), int(match.group(2))
    return 0, (1 << addr_width) - 1


def infer_tile_shape(text: str, intra_type: str) -> Tuple[int, int, int]:
    shape_patterns = [
        r"// Full name: .*?__per_cgra_rows_(\d+)__per_cgra_columns_(\d+)",
        r"// Full name: .*?__width_(\d+)__height_(\d+)",
    ]
    for pattern in shape_patterns:
        matches = re.findall(pattern, text)
        if matches:
            rows, columns = matches[-1]
            break
    else:
        rows, columns = "1", "1"

    packet_match = re.match(r"IntraCgraPacket_\d+_\d+x\d+_(\d+)_", intra_type)
    num_tiles = int(packet_match.group(1)) if packet_match else int(rows) * int(columns)
    return int(rows), int(columns), num_tiles


def infer_metadata(text: str, rtl_name: str, top_module: Optional[str]) -> CgraMetadata:
    typedefs = extract_typedefs(text)
    memo: Dict[str, int] = {}
    top = find_top_module(text, top_module)
    ports = parse_ports(text, top, typedefs, memo)

    intra = require_port(ports, "recv_from_cpu_pkt__msg")
    inter = require_port(ports, "recv_from_inter_cgra_noc__msg")
    cgra_id = require_port(ports, "cgra_id")
    address_lower = require_port(ports, "address_lower")

    if intra.sv_type is None or inter.sv_type is None:
        raise ValueError("packet ports must use generated struct typedefs")

    payload_type = field_type(intra.sv_type, "payload", typedefs)
    data_type = field_type(payload_type, "data", typedefs)
    ctrl_type = field_type(payload_type, "ctrl", typedefs)
    data_width = struct_width(data_type, typedefs, memo)
    data_payload_width = field_width(data_type, "payload", typedefs, memo)
    payload_width = struct_width(payload_type, typedefs, memo)
    ctrl_width = struct_width(ctrl_type, typedefs, memo)
    cmd_width = field_width(payload_type, "cmd", typedefs, memo)
    data_addr_width = field_width(payload_type, "data_addr", typedefs, memo)
    ctrl_addr_width = field_width(payload_type, "ctrl_addr", typedefs, memo)
    address_lo, address_hi = infer_address_bounds(text, address_lower.width)

    has_boundary_ports = "recv_data_on_boundary_south__msg" in ports
    if has_boundary_ports:
        side_counts = {}
        for side in SIDES:
            msg_port = require_port(ports, f"recv_data_on_boundary_{side}__msg")
            if msg_port.array_len is None:
                raise ValueError(f"boundary port for {side} is not an unpacked array")
            side_counts[side] = msg_port.array_len

        if side_counts["south"] != side_counts["north"]:
            raise ValueError("south/north boundary counts differ")
        if side_counts["east"] != side_counts["west"]:
            raise ValueError("east/west boundary counts differ")

        x_tiles = side_counts["south"]
        y_tiles = side_counts["east"]
        num_tiles = x_tiles * y_tiles
    else:
        x_tiles, y_tiles, num_tiles = infer_tile_shape(text, intra.sv_type)

    wrapper_name = f"{top}_wrapper"
    return CgraMetadata(
        top_module=top,
        wrapper_module=wrapper_name,
        intra_type=intra.sv_type,
        inter_type=inter.sv_type,
        data_type=data_type,
        payload_type=payload_type,
        ctrl_type=ctrl_type,
        intra_width=intra.width,
        inter_width=inter.width,
        data_width=data_width,
        data_payload_width=data_payload_width,
        payload_width=payload_width,
        ctrl_width=ctrl_width,
        ctrl_hi_width=max(ctrl_width - 128, 0),
        cmd_width=cmd_width,
        data_addr_width=data_addr_width,
        ctrl_addr_width=ctrl_addr_width,
        id_width=cgra_id.width,
        addr_width=address_lower.width,
        x_tiles=x_tiles,
        y_tiles=y_tiles,
        num_tiles=num_tiles,
        address_lower=address_lo,
        address_upper=address_hi,
        rtl_resource=f"/vsrc/{rtl_name}",
        wrapper_resource=f"/vsrc/{wrapper_name}.v",
        has_boundary_ports=has_boundary_ports,
    )


def flat_range(width: int) -> str:
    return "" if width == 1 else f" [{width - 1}:0]"


def wrapper_ports(meta: CgraMetadata) -> List[str]:
    ports = [
        "input  logic        clk",
        "input  logic        reset",
        "input  logic        recv_from_cpu_pkt_val",
        f"input  logic{flat_range(meta.intra_width)} recv_from_cpu_pkt_msg",
        "output logic        recv_from_cpu_pkt_rdy",
        "output logic        send_to_cpu_pkt_val",
        f"output logic{flat_range(meta.intra_width)} send_to_cpu_pkt_msg",
        "input  logic        send_to_cpu_pkt_rdy",
        "input  logic        recv_from_inter_cgra_noc_val",
        f"input  logic{flat_range(meta.inter_width)} recv_from_inter_cgra_noc_msg",
        "output logic        recv_from_inter_cgra_noc_rdy",
        "output logic        send_to_inter_cgra_noc_val",
        f"output logic{flat_range(meta.inter_width)} send_to_inter_cgra_noc_msg",
        "input  logic        send_to_inter_cgra_noc_rdy",
    ]

    if meta.has_boundary_ports:
        side_sizes = {
            "south": meta.x_tiles,
            "north": meta.x_tiles,
            "east": meta.y_tiles,
            "west": meta.y_tiles,
        }
        for side in SIDES:
            for idx in range(side_sizes[side]):
                ports.extend([
                    f"input  logic        recv_data_on_boundary_{side}_{idx}_val",
                    f"input  logic{flat_range(meta.data_width)} recv_data_on_boundary_{side}_{idx}_msg",
                    f"output logic        recv_data_on_boundary_{side}_{idx}_rdy",
                ])
            for idx in range(side_sizes[side]):
                ports.extend([
                    f"output logic        send_data_on_boundary_{side}_{idx}_val",
                    f"output logic{flat_range(meta.data_width)} send_data_on_boundary_{side}_{idx}_msg",
                    f"input  logic        send_data_on_boundary_{side}_{idx}_rdy",
                ])

    ports.extend([
        f"input  logic{flat_range(meta.id_width)} cgra_id",
        f"input  logic{flat_range(meta.addr_width)} address_lower",
        f"input  logic{flat_range(meta.addr_width)} address_upper",
    ])
    return ports


def comma_join(lines: Iterable[str], indent: str = "  ") -> str:
    items = list(lines)
    return "\n".join(f"{indent}{line}{',' if idx != len(items) - 1 else ''}" for idx, line in enumerate(items))


def gen_boundary_wires(meta: CgraMetadata, side: str, count: int) -> str:
    return f"""
  {meta.data_type} w_recv_{side}_msg [0:{count - 1}];
  logic [0:0] w_recv_{side}_rdy [0:{count - 1}];
  logic [0:0] w_recv_{side}_val [0:{count - 1}];
  {meta.data_type} w_send_{side}_msg [0:{count - 1}];
  logic [0:0] w_send_{side}_rdy [0:{count - 1}];
  logic [0:0] w_send_{side}_val [0:{count - 1}];
"""


def gen_boundary_assigns(side: str, count: int) -> str:
    lines: List[str] = []
    for idx in range(count):
        lines.extend([
            f"  assign w_recv_{side}_val[{idx}] = recv_data_on_boundary_{side}_{idx}_val;",
            f"  assign w_recv_{side}_msg[{idx}] = recv_data_on_boundary_{side}_{idx}_msg;",
            f"  assign recv_data_on_boundary_{side}_{idx}_rdy = w_recv_{side}_rdy[{idx}];",
        ])
    for idx in range(count):
        lines.extend([
            f"  assign send_data_on_boundary_{side}_{idx}_val = w_send_{side}_val[{idx}];",
            f"  assign send_data_on_boundary_{side}_{idx}_msg = w_send_{side}_msg[{idx}];",
            f"  assign w_send_{side}_rdy[{idx}] = send_data_on_boundary_{side}_{idx}_rdy;",
        ])
    return "\n".join(lines)


def render_template(template_name: str, **values: object) -> str:
    template_path = DEFAULT_TEMPLATE_DIR / template_name
    template = Template(template_path.read_text(encoding="utf-8"))
    return template.substitute({key: str(value) for key, value in values.items()})


def gen_wrapper(meta: CgraMetadata) -> str:
    side_sizes = {
        "south": meta.x_tiles,
        "north": meta.x_tiles,
        "east": meta.y_tiles,
        "west": meta.y_tiles,
    }
    boundary_wires = ""
    boundary_assigns = ""
    if meta.has_boundary_ports:
        boundary_wires = "".join(gen_boundary_wires(meta, side, side_sizes[side]) for side in SIDES).rstrip()
        boundary_assigns = "\n\n".join(gen_boundary_assigns(side, side_sizes[side]) for side in SIDES).rstrip()

    inst_ports = [
        ".clk                                ( clk )",
        ".reset                              ( reset )",
        ".recv_from_cpu_pkt__val             ( recv_from_cpu_pkt_val )",
        ".recv_from_cpu_pkt__msg             ( w_recv_from_cpu_pkt_msg )",
        ".recv_from_cpu_pkt__rdy             ( recv_from_cpu_pkt_rdy )",
        ".send_to_cpu_pkt__val               ( send_to_cpu_pkt_val )",
        ".send_to_cpu_pkt__msg               ( w_send_to_cpu_pkt_msg )",
        ".send_to_cpu_pkt__rdy               ( send_to_cpu_pkt_rdy )",
        ".recv_from_inter_cgra_noc__val      ( recv_from_inter_cgra_noc_val )",
        ".recv_from_inter_cgra_noc__msg      ( w_recv_from_inter_cgra_noc_msg )",
        ".recv_from_inter_cgra_noc__rdy      ( recv_from_inter_cgra_noc_rdy )",
        ".send_to_inter_cgra_noc__val        ( send_to_inter_cgra_noc_val )",
        ".send_to_inter_cgra_noc__msg        ( w_send_to_inter_cgra_noc_msg )",
        ".send_to_inter_cgra_noc__rdy        ( send_to_inter_cgra_noc_rdy )",
    ]
    if meta.has_boundary_ports:
        for side in SIDES:
            inst_ports.extend([
                f".recv_data_on_boundary_{side}__val   ( w_recv_{side}_val )",
                f".recv_data_on_boundary_{side}__msg   ( w_recv_{side}_msg )",
                f".recv_data_on_boundary_{side}__rdy   ( w_recv_{side}_rdy )",
                f".send_data_on_boundary_{side}__val   ( w_send_{side}_val )",
                f".send_data_on_boundary_{side}__msg   ( w_send_{side}_msg )",
                f".send_data_on_boundary_{side}__rdy   ( w_send_{side}_rdy )",
            ])
    inst_ports.extend([
        ".cgra_id                            ( cgra_id )",
        ".address_lower                      ( address_lower )",
        ".address_upper                      ( address_upper )",
    ])

    return render_template(
        "cgra_wrapper.v.tpl",
        wrapper_module=meta.wrapper_module,
        top_module=meta.top_module,
        port_list=comma_join(wrapper_ports(meta)),
        intra_type=meta.intra_type,
        inter_type=meta.inter_type,
        boundary_wires=boundary_wires,
        boundary_assigns=boundary_assigns,
        inst_port_list=comma_join(inst_ports, indent="    "),
    )


def gen_scala(meta: CgraMetadata) -> str:
    return render_template(
        "cgra_generated.scala.tpl",
        top_module=meta.top_module,
        intra_width=meta.intra_width,
        inter_width=meta.inter_width,
        data_payload_width=meta.data_payload_width,
        data_width=meta.data_width,
        payload_width=meta.payload_width,
        id_width=meta.id_width,
        addr_width=meta.addr_width,
        x_tiles=meta.x_tiles,
        y_tiles=meta.y_tiles,
        cmd_width=meta.cmd_width,
        num_tiles=meta.num_tiles,
        address_lower=meta.address_lower,
        address_upper=meta.address_upper,
        has_boundary_ports=str(meta.has_boundary_ports).lower(),
        wrapper_module=meta.wrapper_module,
        rtl_resource=meta.rtl_resource,
        wrapper_resource=meta.wrapper_resource,
    )


def c_define(name: str, value: int) -> str:
    return f"#define {name} {value}"


def require_offset(offsets: Dict[str, Tuple[int, int]], field: str) -> Tuple[int, int]:
    if field not in offsets:
        raise ValueError(f"missing generated layout field: {field}")
    return offsets[field]


def gen_c_layout(meta: CgraMetadata, typedefs: Dict[str, str]) -> str:
    memo: Dict[str, int] = {}
    data_offsets = field_offsets(meta.data_type, typedefs, memo)
    ctrl_offsets = field_offsets(meta.ctrl_type, typedefs, memo)
    ctrl_dims = logic_field_dims(meta.ctrl_type, typedefs)
    payload_offsets = field_offsets(meta.payload_type, typedefs, memo)
    pkt_offsets = field_offsets(meta.intra_type, typedefs, memo)

    def append_lsb_nbits(lines: List[str], define_base: str, offsets: Dict[str, Tuple[int, int]], field: str) -> None:
        lsb, nbits = require_offset(offsets, field)
        lines.append(c_define(f"{define_base}_LSB", lsb))
        lines.append(c_define(f"{define_base}_NBITS", nbits))

    def append_packed_array_shape(lines: List[str], define_base: str, field: str) -> None:
        dims = ctrl_dims.get(field, [])
        if len(dims) > 1:
            elem_width = 1
            for dim in dims[1:]:
                elem_width *= dim
            lines.append(c_define(f"{define_base}_COUNT", dims[0]))
            lines.append(c_define(f"{define_base}_ELEM_NBITS", elem_width))
        elif len(dims) == 1:
            lines.append(c_define(f"{define_base}_COUNT", 1))
            lines.append(c_define(f"{define_base}_ELEM_NBITS", dims[0]))

    lines = [
        "/*",
        " * Auto-generated by scripts/sync_cgra_blackbox.py.",
        f" * Source top module: {meta.top_module}",
        f" * Source packet type: {meta.intra_type}",
        " * Do not edit by hand; regenerate after CGRA RTL/YAML changes.",
        " */",
        "#ifndef CGRA_LAYOUT_H",
        "#define CGRA_LAYOUT_H",
        "",
        c_define("CGRA_INTRA_PKT_NBITS", meta.intra_width),
        c_define("CGRA_INTER_PKT_NBITS", meta.inter_width),
        c_define("CGRA_PAYLOAD_NBITS", meta.payload_width),
        c_define("CGRA_CMD_NBITS", meta.cmd_width),
        c_define("CGRA_DATA_NBITS", meta.data_width),
        c_define("CGRA_DATA_PAYLOAD_NBITS", meta.data_payload_width),
        c_define("CGRA_CTRL_NBITS", meta.ctrl_width),
        c_define("DATA_ADDR_NBITS", meta.data_addr_width),
        c_define("CTRL_ADDR_NBITS", meta.ctrl_addr_width),
        c_define("CTRL_LO_NBITS", min(meta.ctrl_width, 64)),
        c_define("CTRL_MID_NBITS", max(min(meta.ctrl_width - 64, 64), 0)),
        c_define("CTRL_HI_NBITS", meta.ctrl_hi_width),
        "",
    ]

    data_names = {
        "payload": "DATA_PAYLOAD",
        "predicate": "DATA_PREDICATE",
        "bypass": "DATA_BYPASS",
        "delay": "DATA_DELAY",
    }
    for field, define_base in data_names.items():
        append_lsb_nbits(lines, define_base, data_offsets, field)

    lines.append("")

    ctrl_names = {
        "operation": "CTRL_OPERATION",
        "fu_in": "CTRL_FU_IN",
        "routing_xbar_outport": "CTRL_ROUTING_XBAR_OUTPORT",
        "fu_xbar_outport": "CTRL_FU_XBAR_OUTPORT",
        "vector_factor_power": "CTRL_VECTOR_FACTOR_POWER",
        "is_last_ctrl": "CTRL_IS_LAST_CTRL",
        "write_reg_from": "CTRL_WRITE_REG_FROM",
        "write_reg_idx": "CTRL_WRITE_REG_IDX",
        "read_reg_from": "CTRL_READ_REG_FROM",
        "read_reg_idx": "CTRL_READ_REG_IDX",
    }
    for field, define_base in ctrl_names.items():
        append_lsb_nbits(lines, define_base, ctrl_offsets, field)
        append_packed_array_shape(lines, define_base, field)

    lines.append("")

    payload_lsb, _payload_width = require_offset(pkt_offsets, "payload")
    pkt_names = {
        "ctrl_addr": "PKT_CTRL_ADDR",
        "ctrl": "PKT_CTRL",
        "data_addr": "PKT_DATA_ADDR",
        "data": "PKT_DATA",
        "cmd": "PKT_CMD",
    }
    for field, define_base in pkt_names.items():
        lsb, nbits = require_offset(payload_offsets, field)
        lines.append(c_define(f"{define_base}_LSB", payload_lsb + lsb))
        lines.append(c_define(f"{define_base}_NBITS", nbits))

    top_pkt_names = {
        "vc_id": "PKT_VC_ID",
        "opaque": "PKT_OPAQUE",
        "dst_cgra_y": "PKT_DST_CGRA_Y",
        "dst_cgra_x": "PKT_DST_CGRA_X",
        "src_cgra_y": "PKT_SRC_CGRA_Y",
        "src_cgra_x": "PKT_SRC_CGRA_X",
        "dst_cgra_id": "PKT_DST_CGRA_ID",
        "src_cgra_id": "PKT_SRC_CGRA_ID",
        "dst": "PKT_DST_TILE",
        "src": "PKT_SRC_TILE",
    }
    for field, define_base in top_pkt_names.items():
        append_lsb_nbits(lines, define_base, pkt_offsets, field)

    lines.extend(["", "#endif", ""])
    return "\n".join(lines)


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rtl", type=Path, default=DEFAULT_RTL, help="PyMTL3-generated CGRA Verilog")
    parser.add_argument("--top-module", help="Top module name to wrap; inferred if omitted")
    parser.add_argument("--chipyard-vsrc", type=Path, default=DEFAULT_VSRC, help="Chipyard vsrc output directory")
    parser.add_argument("--scala-out", type=Path, default=DEFAULT_SCALA, help="Generated Scala params output")
    parser.add_argument("--c-layout-out", type=Path, default=DEFAULT_C_LAYOUT, help="Generated C layout header output")
    parser.add_argument("--dry-run", action="store_true", help="Parse and print metadata without writing files")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    rtl_path = args.rtl.resolve()
    text = rtl_path.read_text(encoding="utf-8")
    typedefs = extract_typedefs(text)
    meta = infer_metadata(text, rtl_path.name, args.top_module)

    wrapper_path = args.chipyard_vsrc / f"{meta.wrapper_module}.v"
    rtl_dst = args.chipyard_vsrc / rtl_path.name

    print(f"top_module={meta.top_module}")
    print(f"wrapper_module={meta.wrapper_module}")
    print(f"intra_width={meta.intra_width} inter_width={meta.inter_width} data_width={meta.data_width}")
    print(f"x_tiles={meta.x_tiles} y_tiles={meta.y_tiles} num_tiles={meta.num_tiles}")
    print(f"has_boundary_ports={meta.has_boundary_ports}")
    print(f"scala_out={args.scala_out}")
    print(f"c_layout_out={args.c_layout_out}")
    print(f"wrapper_out={wrapper_path}")
    print(f"rtl_out={rtl_dst}")

    if args.dry_run:
        return 0

    args.chipyard_vsrc.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(rtl_path, rtl_dst)
    write_text(wrapper_path, gen_wrapper(meta))
    write_text(args.scala_out, gen_scala(meta))
    write_text(args.c_layout_out, gen_c_layout(meta, typedefs))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
