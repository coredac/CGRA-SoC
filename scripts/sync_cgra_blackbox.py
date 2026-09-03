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
import ast
import re
import shutil
from dataclasses import dataclass
from pathlib import Path
from string import Template
from typing import Dict, Iterable, List, Optional, Tuple

import yaml

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RTL = ROOT / "VectorCGRA" / "CgraRTL_2x2__pickled.v"
DEFAULT_VSRC = (
    ROOT
    / "chipyard"
    / "generators"
    / "chipyard"
    / "src"
    / "main"
    / "resources"
    / "vsrc"
)
DEFAULT_SCALA = (
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
DEFAULT_C_LAYOUT = ROOT / "tests" / "include" / "cgra_layout.h"
DEFAULT_C_PROTOCOL = ROOT / "tests" / "generated" / "cgra_protocol_generated.h"
DEFAULT_TEMPLATE_DIR = ROOT / "scripts" / "templates"
ROCC_PROTOCOL = ROOT / "configs" / "protocol" / "cgra_rocc_functs.yaml"
CMD_TYPE_SOURCE = ROOT / "VectorCGRA" / "lib" / "cmd_type.py"
ROCC_XLEN = 64
SIDES = ("south", "north", "east", "west")


@dataclass(frozen=True)
class Port:
    direction: str
    name: str
    width: int
    sv_type: Optional[str] = None
    array_len: Optional[int] = None


@dataclass(frozen=True)
class DmaMetadata:
    enabled: bool
    write_req_type: str = ""
    dram_addr_width: int = 0
    dram_data_width: int = 0
    dram_mask_width: int = 0
    spm_addr_width: int = 0
    nbytes_width: int = 0
    tag_width: int = 0
    spm_words: int = 0
    write_req_addr_lsb: int = 0
    write_req_data_lsb: int = 0
    write_req_mask_lsb: int = 0
    descriptor_spm_lsb: int = 0
    descriptor_nbytes_lsb: int = 0
    descriptor_tag_lsb: int = 0
    descriptor_width: int = 0
    cmd_config_dram_addr_lo: int = 0
    cmd_config_dram_addr_hi: int = 0
    cmd_config_spm_addr: int = 0
    cmd_config_bytes: int = 0
    cmd_config_tag: int = 0
    cmd_mvin: int = 0
    cmd_mvout: int = 0
    cmd_done: int = 0
    packet_templates: Tuple[int, ...] = ()


@dataclass(frozen=True)
class SpmReadMetadata:
    enabled: bool
    req_type: str = ""
    resp_type: str = ""
    addr_width: int = 0
    data_width: int = 0
    words: int = 0


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
    has_inter_cgra_noc_ports: bool
    has_cgra_id_port: bool
    has_address_ports: bool
    pkt_cmd_lsb: int
    pkt_data_payload_lsb: int
    pkt_data_predicate_lsb: int
    pkt_data_addr_lsb: int
    pkt_opaque_lsb: int
    pkt_dst_tile_lsb: int
    dma: DmaMetadata
    spm_read: SpmReadMetadata


def load_command_ids(path: Path = CMD_TYPE_SOURCE) -> Dict[str, int]:
    tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
    commands: Dict[str, int] = {}
    for node in tree.body:
        if not isinstance(node, ast.Assign) or len(node.targets) != 1:
            continue
        target = node.targets[0]
        if (
            isinstance(target, ast.Name)
            and target.id.startswith("CMD_")
            and isinstance(node.value, ast.Constant)
            and isinstance(node.value.value, int)
        ):
            commands[target.id] = node.value.value
    if not commands:
        raise ValueError(f"no CMD_* integer assignments found in {path}")
    return commands


def load_rocc_functs(path: Path = ROCC_PROTOCOL) -> Dict[str, int]:
    data = yaml.safe_load(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict) or not isinstance(data.get("functs"), dict):
        raise ValueError(f"{path}: expected a 'functs' mapping")
    functs: Dict[str, int] = {}
    used_values = set()
    for name, value in data["functs"].items():
        if not isinstance(name, str) or not isinstance(value, int):
            raise ValueError(f"{path}: funct names must map to integers")
        if not 0 <= value < 128:
            raise ValueError(f"{path}: funct {name}={value} does not fit funct7")
        if value in used_values:
            raise ValueError(f"{path}: duplicate funct value {value}")
        functs[name] = value
        used_values.add(value)
    required = {
        "STATUS",
        "WAIT",
        "RAW_PKT_LO",
        "RAW_PKT_MID",
        "RAW_PKT_HI",
        "SET_EXPECTED_COMPLETES",
        "RESULT",
        "RAW_PKT_TOP",
        "LOAD_RESULT",
        "DMA_MVIN_ASYNC",
        "DMA_MVOUT_ASYNC",
        "DMA_WAIT",
        "SPM_PKT_HI",
        "SPM_PKT_TOP",
    }
    missing = required - functs.keys()
    if missing:
        raise ValueError(f"{path}: missing required functs: {sorted(missing)}")
    return functs


def range_width(msb: int, lsb: int) -> int:
    return abs(msb - lsb) + 1


def packed_dims(text: str) -> List[int]:
    return [
        range_width(int(msb), int(lsb))
        for msb, lsb in re.findall(r"\[(\d+)\s*:\s*(\d+)\]", text)
    ]


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
        logic_match = re.match(
            r"logic\s+(?P<dims>(?:\[[^\]]+\]\s*)*)\s*(?P<name>\w+)$", line
        )
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
        logic_match = re.match(
            r"logic\s+(?P<dims>(?:\[[^\]]+\]\s*)*)\s*(?P<name>\w+)$", line
        )
        if logic_match:
            dims_by_name[logic_match.group("name")] = packed_dims(
                logic_match.group("dims")
            )
    return dims_by_name


def struct_width(type_name: str, typedefs: Dict[str, str], memo: Dict[str, int]) -> int:
    if type_name in memo:
        return memo[type_name]
    if type_name not in typedefs:
        raise ValueError(f"unknown struct typedef: {type_name}")
    total = 0
    for field_type, _field_name, field_width in parse_struct_fields(
        typedefs[type_name]
    ):
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


def field_width(
    type_name: str, field_name: str, typedefs: Dict[str, str], memo: Dict[str, int]
) -> int:
    for field_type, name, width in parse_struct_fields(typedefs[type_name]):
        if name != field_name:
            continue
        if field_type == "logic":
            return width
        return struct_width(field_type, typedefs, memo)
    raise ValueError(f"{type_name} has no field named {field_name}")


def field_offsets(
    type_name: str, typedefs: Dict[str, str], memo: Dict[str, int]
) -> Dict[str, Tuple[int, int]]:
    fields = parse_struct_fields(typedefs[type_name])
    cursor = struct_width(type_name, typedefs, memo)
    offsets: Dict[str, Tuple[int, int]] = {}
    for field_type, name, width in fields:
        field_nbits = (
            width if field_type == "logic" else struct_width(field_type, typedefs, memo)
        )
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
        name
        for name in modules
        if name.startswith(("CgraRTL", "CgraTemplateRTL", "MeshMultiCgraTemplateRTL"))
        and not name.endswith("_wrapper")
    ]
    if not candidates:
        raise ValueError("could not infer top module; pass --top-module")
    return candidates[-1]


def module_port_block(text: str, module_name: str) -> str:
    match = re.search(
        rf"^module\s+{re.escape(module_name)}\s*\((?P<body>.*?)^\);", text, re.M | re.S
    )
    if not match:
        raise ValueError(f"could not parse module port block for {module_name}")
    return match.group("body")


def parse_port_line(
    line: str, typedefs: Dict[str, str], memo: Dict[str, int]
) -> Optional[Port]:
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
    arr_len = (
        array_len(int(alo), int(ahi)) if alo is not None and ahi is not None else None
    )

    if prefix.startswith("logic"):
        dims = prefix[len("logic") :].strip()
        width = packed_dims_width(dims) if dims else 1
        return Port(direction, name, width, None, arr_len)

    sv_type = prefix
    return Port(
        direction, name, struct_width(sv_type, typedefs, memo), sv_type, arr_len
    )


def parse_ports(
    text: str, module_name: str, typedefs: Dict[str, str], memo: Dict[str, int]
) -> Dict[str, Port]:
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


def optional_port(ports: Dict[str, Port], name: str) -> Optional[Port]:
    return ports.get(name)


def same_port_type(left: Port, right: Port) -> bool:
    return left.sv_type is not None and left.sv_type == right.sv_type


def find_inter_packet_type(
    typedefs: Dict[str, str],
    memo: Dict[str, int],
    intra_type: str,
    payload_type: str,
) -> str:
    intra_width = struct_width(intra_type, typedefs, memo)
    candidates: List[Tuple[int, str]] = []
    for type_name in typedefs:
        if not type_name.startswith("InterCgraPacket"):
            continue
        try:
            if field_type(type_name, "payload", typedefs) != payload_type:
                continue
            width = struct_width(type_name, typedefs, memo)
        except ValueError:
            continue
        candidates.append((width, type_name))

    if not candidates:
        raise ValueError(
            "top module has no inter-CGRA NoC ports and no matching "
            "InterCgraPacket typedef was found"
        )

    candidates.sort(key=lambda item: (item[0] < intra_width, item[0], item[1]))
    return candidates[-1][1]


def find_dma_cmd_type(typedefs: Dict[str, str]) -> str:
    required_fields = {"opcode", "dram_addr", "spm_addr", "nbytes", "dma_tag"}
    candidates = []
    for type_name in typedefs:
        if not type_name.startswith("DmaCmd_"):
            continue
        fields = {
            name for _kind, name, _width in parse_struct_fields(typedefs[type_name])
        }
        if required_fields.issubset(fields):
            candidates.append(type_name)
    if len(candidates) != 1:
        raise ValueError(
            f"expected exactly one generated DmaCmd typedef, found {candidates}"
        )
    return candidates[0]


def insert_packet_field(
    packet: int, value: int, lsb: int, width: int, field_name: str
) -> int:
    if value < 0 or value >= (1 << width):
        raise ValueError(f"{field_name}={value} does not fit {width} bits")
    mask = ((1 << width) - 1) << lsb
    if packet & mask:
        raise ValueError(f"packet template field {field_name} is not zero")
    return packet | (value << lsb)


def build_dma_packet_words(
    meta: CgraMetadata,
    dram_addr: int,
    spm_addr: int,
    nbytes: int,
    tag: int,
    dma_cmd: int,
) -> Tuple[int, ...]:
    dma = meta.dma
    if not dma.enabled:
        raise ValueError("generated CGRA top has no DMA interface")
    if dram_addr < 0 or dram_addr >= (1 << dma.dram_addr_width):
        raise ValueError("DRAM address does not fit generated DMA layout")
    if spm_addr < 0 or spm_addr >= (1 << dma.spm_addr_width):
        raise ValueError("SPM address does not fit generated DMA layout")
    if nbytes <= 0 or nbytes >= (1 << dma.nbytes_width):
        raise ValueError("DMA byte count does not fit generated DMA layout")
    if tag < 0 or tag >= (1 << dma.tag_width):
        raise ValueError("DMA tag does not fit generated DMA layout")
    if dma_cmd not in (dma.cmd_mvin, dma.cmd_mvout):
        raise ValueError(f"invalid DMA issue command {dma_cmd}")
    word_bytes = meta.data_payload_width // 8
    beat_bytes = dma.dram_data_width // 8
    if dma.dram_data_width != 128 or dma.dram_data_width % 8:
        raise ValueError("Phase-1 CGRA DMA requires a 128-bit DRAM beat")
    if dram_addr % beat_bytes:
        raise ValueError("DMA DRAM address must be 16-byte aligned")
    if nbytes % beat_bytes:
        raise ValueError("DMA byte count must be a multiple of 16 bytes")
    if dram_addr + nbytes > (1 << dma.dram_addr_width):
        raise ValueError("DMA address plus byte count overflows the DRAM address width")
    if spm_addr + nbytes // word_bytes > dma.spm_words:
        raise ValueError("DMA transfer exceeds the generated SPM range")

    templates = list(dma.packet_templates[:5])
    templates.append(dma.packet_templates[5 if dma_cmd == dma.cmd_mvin else 6])
    data_width = meta.data_payload_width
    packets = [
        insert_packet_field(
            templates[0],
            dram_addr & ((1 << data_width) - 1),
            meta.pkt_data_payload_lsb,
            data_width,
            "dram_addr_lo",
        ),
        insert_packet_field(
            templates[1],
            dram_addr >> data_width,
            meta.pkt_data_payload_lsb,
            data_width,
            "dram_addr_hi",
        ),
        insert_packet_field(
            templates[2],
            spm_addr,
            meta.pkt_data_addr_lsb,
            dma.spm_addr_width,
            "spm_addr",
        ),
        insert_packet_field(
            templates[3], nbytes, meta.pkt_data_payload_lsb, dma.nbytes_width, "nbytes"
        ),
        insert_packet_field(
            templates[4], tag, meta.pkt_data_payload_lsb, dma.tag_width, "tag"
        ),
        templates[5],
    ]
    packet_limit = 1 << meta.intra_width
    if any(packet >= packet_limit for packet in packets):
        raise ValueError("generated DMA packet exceeds the intra-CGRA packet width")
    return tuple(packets)


def infer_address_bounds(text: str, addr_width: int) -> Tuple[int, int]:
    match = re.search(r"controller2addr_map_\{0:\s*[\[\(](\d+),\s*(\d+)[\]\)]", text)
    if match:
        return int(match.group(1)), int(match.group(2))
    return 0, (1 << addr_width) - 1


def infer_tile_shape(text: str, intra_type: str) -> Tuple[int, int, int]:
    shape_patterns = [
        r"// Full name: .*?__per_cgra_rows_(\d+)__per_cgra_columns_(\d+)",
        r"// Full name: .*?__id2cgraSize_map_\{0:\s*\[(\d+),\s*(\d+)\]",
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
    for cpu_port_name in (
        "recv_from_cpu_pkt__val",
        "recv_from_cpu_pkt__rdy",
        "send_to_cpu_pkt__val",
        "send_to_cpu_pkt__msg",
        "send_to_cpu_pkt__rdy",
    ):
        require_port(ports, cpu_port_name)

    recv_inter = optional_port(ports, "recv_from_inter_cgra_noc__msg")
    send_inter = optional_port(ports, "send_to_inter_cgra_noc__msg")
    inter_ports = [
        optional_port(ports, name)
        for name in (
            "recv_from_inter_cgra_noc__val",
            "recv_from_inter_cgra_noc__msg",
            "recv_from_inter_cgra_noc__rdy",
            "send_to_inter_cgra_noc__val",
            "send_to_inter_cgra_noc__msg",
            "send_to_inter_cgra_noc__rdy",
        )
    ]
    has_inter_cgra_noc_ports = any(port is not None for port in inter_ports)
    if has_inter_cgra_noc_ports and not all(port is not None for port in inter_ports):
        raise ValueError("top module has only a partial inter-CGRA NoC interface")

    cgra_id = optional_port(ports, "cgra_id")
    address_lower = optional_port(ports, "address_lower")
    address_upper = optional_port(ports, "address_upper")
    has_cgra_id_port = cgra_id is not None
    has_address_ports = any(port is not None for port in (address_lower, address_upper))
    if has_address_ports and not all(
        port is not None for port in (address_lower, address_upper)
    ):
        raise ValueError("top module has only a partial address bound interface")

    if intra.sv_type is None:
        raise ValueError("CPU packet ports must use generated struct typedefs")

    payload_type = field_type(intra.sv_type, "payload", typedefs)
    if recv_inter is not None:
        if (
            recv_inter.sv_type is None
            or send_inter is None
            or send_inter.sv_type is None
        ):
            raise ValueError(
                "inter-CGRA packet ports must use generated struct typedefs"
            )
        if not same_port_type(recv_inter, send_inter):
            raise ValueError("inter-CGRA recv/send packet typedefs differ")
        inter_type = recv_inter.sv_type
    else:
        inter_type = find_inter_packet_type(typedefs, memo, intra.sv_type, payload_type)

    data_type = field_type(payload_type, "data", typedefs)
    ctrl_type = field_type(payload_type, "ctrl", typedefs)
    data_width = struct_width(data_type, typedefs, memo)
    data_payload_width = field_width(data_type, "payload", typedefs, memo)
    payload_width = struct_width(payload_type, typedefs, memo)
    ctrl_width = struct_width(ctrl_type, typedefs, memo)
    cmd_width = field_width(payload_type, "cmd", typedefs, memo)
    data_addr_width = field_width(payload_type, "data_addr", typedefs, memo)
    ctrl_addr_width = field_width(payload_type, "ctrl_addr", typedefs, memo)
    id_width = (
        cgra_id.width
        if cgra_id is not None
        else field_width(intra.sv_type, "dst_cgra_id", typedefs, memo)
    )
    addr_width = address_lower.width if address_lower is not None else data_addr_width
    address_lo, address_hi = infer_address_bounds(text, addr_width)

    pkt_offsets = field_offsets(intra.sv_type, typedefs, memo)
    payload_offsets = field_offsets(payload_type, typedefs, memo)
    data_offsets = field_offsets(data_type, typedefs, memo)
    payload_lsb, _ = require_offset(pkt_offsets, "payload")
    payload_data_lsb, _ = require_offset(payload_offsets, "data")
    pkt_cmd_lsb = payload_lsb + require_offset(payload_offsets, "cmd")[0]
    pkt_data_addr_lsb = payload_lsb + require_offset(payload_offsets, "data_addr")[0]
    pkt_data_lsb = payload_lsb + payload_data_lsb
    pkt_data_payload_lsb = pkt_data_lsb + require_offset(data_offsets, "payload")[0]
    pkt_data_predicate_lsb = pkt_data_lsb + require_offset(data_offsets, "predicate")[0]
    pkt_opaque_lsb, pkt_opaque_width = require_offset(pkt_offsets, "opaque")
    pkt_dst_tile_lsb, _ = require_offset(pkt_offsets, "dst")

    dma_port_names = (
        "send_to_dram_rd_req__val",
        "send_to_dram_rd_req__msg",
        "send_to_dram_rd_req__rdy",
        "recv_from_dram_rd_resp__val",
        "recv_from_dram_rd_resp__msg",
        "recv_from_dram_rd_resp__rdy",
        "send_to_dram_wr_req__val",
        "send_to_dram_wr_req__msg",
        "send_to_dram_wr_req__rdy",
        "recv_from_dram_wr_resp__val",
        "recv_from_dram_wr_resp__msg",
        "recv_from_dram_wr_resp__rdy",
    )
    present_dma_ports = [name for name in dma_port_names if name in ports]
    if present_dma_ports and len(present_dma_ports) != len(dma_port_names):
        missing = sorted(set(dma_port_names) - set(present_dma_ports))
        raise ValueError(f"top module has a partial DMA interface; missing {missing}")

    dma_meta = DmaMetadata(enabled=False)
    if present_dma_ports:
        expected_directions = {
            "send_to_dram_rd_req__val": "output",
            "send_to_dram_rd_req__msg": "output",
            "send_to_dram_rd_req__rdy": "input",
            "recv_from_dram_rd_resp__val": "input",
            "recv_from_dram_rd_resp__msg": "input",
            "recv_from_dram_rd_resp__rdy": "output",
            "send_to_dram_wr_req__val": "output",
            "send_to_dram_wr_req__msg": "output",
            "send_to_dram_wr_req__rdy": "input",
            "recv_from_dram_wr_resp__val": "input",
            "recv_from_dram_wr_resp__msg": "input",
            "recv_from_dram_wr_resp__rdy": "output",
        }
        for port_name, direction in expected_directions.items():
            if ports[port_name].direction != direction:
                raise ValueError(
                    f"DMA port {port_name} is {ports[port_name].direction}, expected {direction}"
                )

        rd_req = require_port(ports, "send_to_dram_rd_req__msg")
        rd_resp = require_port(ports, "recv_from_dram_rd_resp__msg")
        wr_req = require_port(ports, "send_to_dram_wr_req__msg")
        if wr_req.sv_type is None:
            raise ValueError(
                "DMA DRAM write request must use a generated struct typedef"
            )
        wr_offsets = field_offsets(wr_req.sv_type, typedefs, memo)
        wr_addr_lsb, wr_addr_width = require_offset(wr_offsets, "addr")
        wr_data_lsb, wr_data_width = require_offset(wr_offsets, "data")
        wr_mask_lsb, wr_mask_width = require_offset(wr_offsets, "mask")
        if rd_req.width != wr_addr_width:
            raise ValueError("DMA read and write DRAM address widths differ")
        if rd_resp.width != wr_data_width:
            raise ValueError("DMA read and write DRAM data widths differ")

        dma_cmd_type = find_dma_cmd_type(typedefs)
        dma_cmd_offsets = field_offsets(dma_cmd_type, typedefs, memo)
        dma_dram_width = require_offset(dma_cmd_offsets, "dram_addr")[1]
        dma_nbytes_width = require_offset(dma_cmd_offsets, "nbytes")[1]
        dma_tag_width = require_offset(dma_cmd_offsets, "dma_tag")[1]
        if dma_dram_width != rd_req.width:
            raise ValueError("DMA command and external DRAM address widths differ")
        if dma_dram_width != 2 * data_payload_width:
            raise ValueError(
                "current six-packet DMA protocol requires DRAM address width "
                "to equal two packet data payloads"
            )
        if dma_nbytes_width > data_payload_width:
            raise ValueError("DMA nbytes field does not fit one packet data payload")
        if dma_tag_width > data_payload_width or dma_tag_width > pkt_opaque_width:
            raise ValueError("DMA tag does not fit packet data payload/opaque fields")
        if data_payload_width % 8:
            raise ValueError("CGRA data payload width must be byte aligned for DMA")

        descriptor_spm_lsb = 0
        descriptor_nbytes_lsb = descriptor_spm_lsb + data_addr_width
        descriptor_tag_lsb = descriptor_nbytes_lsb + dma_nbytes_width
        descriptor_width = descriptor_tag_lsb + dma_tag_width
        if descriptor_width > ROCC_XLEN:
            raise ValueError(
                f"DMA descriptor needs {descriptor_width} bits, exceeding xLen={ROCC_XLEN}"
            )

        commands = load_command_ids()
        required_dma_commands = (
            "CMD_DMA_CONFIG_DRAM_ADDR_LO",
            "CMD_DMA_CONFIG_DRAM_ADDR_HI",
            "CMD_DMA_CONFIG_SPM_ADDR",
            "CMD_DMA_CONFIG_BYTES",
            "CMD_DMA_CONFIG_TAG",
            "CMD_DMA_MVIN",
            "CMD_DMA_MVOUT",
            "CMD_DMA_DONE",
        )
        missing_commands = [
            name for name in required_dma_commands if name not in commands
        ]
        if missing_commands:
            raise ValueError(f"{CMD_TYPE_SOURCE}: missing {missing_commands}")

        def command_template(command_name: str, predicate: bool = False) -> int:
            packet = commands[command_name] << pkt_cmd_lsb
            if predicate:
                packet |= 1 << pkt_data_predicate_lsb
            if packet >= (1 << intra.width):
                raise ValueError(
                    f"DMA packet template {command_name} exceeds packet width"
                )
            return packet

        packet_templates = (
            command_template("CMD_DMA_CONFIG_DRAM_ADDR_LO", predicate=True),
            command_template("CMD_DMA_CONFIG_DRAM_ADDR_HI", predicate=True),
            command_template("CMD_DMA_CONFIG_SPM_ADDR"),
            command_template("CMD_DMA_CONFIG_BYTES", predicate=True),
            command_template("CMD_DMA_CONFIG_TAG", predicate=True),
            command_template("CMD_DMA_MVIN"),
            command_template("CMD_DMA_MVOUT"),
        )
        spm_words = address_hi + 1
        if address_lo != 0 or spm_words > (1 << data_addr_width):
            raise ValueError(
                "DMA descriptor currently requires a zero-based software-visible SPM range"
            )
        dma_meta = DmaMetadata(
            enabled=True,
            write_req_type=wr_req.sv_type,
            dram_addr_width=rd_req.width,
            dram_data_width=rd_resp.width,
            dram_mask_width=wr_mask_width,
            spm_addr_width=data_addr_width,
            nbytes_width=dma_nbytes_width,
            tag_width=dma_tag_width,
            spm_words=spm_words,
            write_req_addr_lsb=wr_addr_lsb,
            write_req_data_lsb=wr_data_lsb,
            write_req_mask_lsb=wr_mask_lsb,
            descriptor_spm_lsb=descriptor_spm_lsb,
            descriptor_nbytes_lsb=descriptor_nbytes_lsb,
            descriptor_tag_lsb=descriptor_tag_lsb,
            descriptor_width=descriptor_width,
            cmd_config_dram_addr_lo=commands["CMD_DMA_CONFIG_DRAM_ADDR_LO"],
            cmd_config_dram_addr_hi=commands["CMD_DMA_CONFIG_DRAM_ADDR_HI"],
            cmd_config_spm_addr=commands["CMD_DMA_CONFIG_SPM_ADDR"],
            cmd_config_bytes=commands["CMD_DMA_CONFIG_BYTES"],
            cmd_config_tag=commands["CMD_DMA_CONFIG_TAG"],
            cmd_mvin=commands["CMD_DMA_MVIN"],
            cmd_mvout=commands["CMD_DMA_MVOUT"],
            cmd_done=commands["CMD_DMA_DONE"],
            packet_templates=packet_templates,
        )

    spm_read_port_names = (
        "recv_from_ext_spm_rd_req__val",
        "recv_from_ext_spm_rd_req__msg",
        "recv_from_ext_spm_rd_req__rdy",
        "send_to_ext_spm_rd_resp__val",
        "send_to_ext_spm_rd_resp__msg",
        "send_to_ext_spm_rd_resp__rdy",
    )
    present_spm_read_ports = [
        name for name in spm_read_port_names if name in ports
    ]
    if present_spm_read_ports and len(present_spm_read_ports) != len(
        spm_read_port_names
    ):
        missing = sorted(set(spm_read_port_names) - set(present_spm_read_ports))
        raise ValueError(
            f"top module has a partial external SPM read interface; missing {missing}"
        )

    spm_read_meta = SpmReadMetadata(enabled=False)
    if present_spm_read_ports:
        expected_directions = {
            "recv_from_ext_spm_rd_req__val": "input",
            "recv_from_ext_spm_rd_req__msg": "input",
            "recv_from_ext_spm_rd_req__rdy": "output",
            "send_to_ext_spm_rd_resp__val": "output",
            "send_to_ext_spm_rd_resp__msg": "output",
            "send_to_ext_spm_rd_resp__rdy": "input",
        }
        for port_name, direction in expected_directions.items():
            if ports[port_name].direction != direction:
                raise ValueError(
                    f"external SPM read port {port_name} is "
                    f"{ports[port_name].direction}, expected {direction}"
                )

        req = require_port(ports, "recv_from_ext_spm_rd_req__msg")
        resp = require_port(ports, "send_to_ext_spm_rd_resp__msg")
        if req.sv_type is None or resp.sv_type is None:
            raise ValueError(
                "external SPM read messages must use generated struct typedefs"
            )
        addr_width = field_width(req.sv_type, "addr", typedefs, memo)
        spm_data_width = field_width(resp.sv_type, "data", typedefs, memo)
        if req.width != addr_width or resp.width != spm_data_width:
            raise ValueError("external SPM read message types must contain one field")
        spm_read_meta = SpmReadMetadata(
            enabled=True,
            req_type=req.sv_type,
            resp_type=resp.sv_type,
            addr_width=addr_width,
            data_width=spm_data_width,
            words=address_hi + 1,
        )

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
        inter_type=inter_type,
        data_type=data_type,
        payload_type=payload_type,
        ctrl_type=ctrl_type,
        intra_width=intra.width,
        inter_width=struct_width(inter_type, typedefs, memo),
        data_width=data_width,
        data_payload_width=data_payload_width,
        payload_width=payload_width,
        ctrl_width=ctrl_width,
        ctrl_hi_width=max(ctrl_width - 128, 0),
        cmd_width=cmd_width,
        data_addr_width=data_addr_width,
        ctrl_addr_width=ctrl_addr_width,
        id_width=id_width,
        addr_width=addr_width,
        x_tiles=x_tiles,
        y_tiles=y_tiles,
        num_tiles=num_tiles,
        address_lower=address_lo,
        address_upper=address_hi,
        rtl_resource=f"/vsrc/{rtl_name}",
        wrapper_resource=f"/vsrc/{wrapper_name}.v",
        has_boundary_ports=has_boundary_ports,
        has_inter_cgra_noc_ports=has_inter_cgra_noc_ports,
        has_cgra_id_port=has_cgra_id_port,
        has_address_ports=has_address_ports,
        pkt_cmd_lsb=pkt_cmd_lsb,
        pkt_data_payload_lsb=pkt_data_payload_lsb,
        pkt_data_predicate_lsb=pkt_data_predicate_lsb,
        pkt_data_addr_lsb=pkt_data_addr_lsb,
        pkt_opaque_lsb=pkt_opaque_lsb,
        pkt_dst_tile_lsb=pkt_dst_tile_lsb,
        dma=dma_meta,
        spm_read=spm_read_meta,
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

    if meta.dma.enabled:
        ports.extend(
            [
                "output logic        send_to_dram_rd_req_val",
                f"output logic{flat_range(meta.dma.dram_addr_width)} send_to_dram_rd_req_addr",
                "input  logic        send_to_dram_rd_req_rdy",
                "input  logic        recv_from_dram_rd_resp_val",
                f"input  logic{flat_range(meta.dma.dram_data_width)} recv_from_dram_rd_resp_data",
                "output logic        recv_from_dram_rd_resp_rdy",
                "output logic        send_to_dram_wr_req_val",
                f"output logic{flat_range(meta.dma.dram_addr_width)} send_to_dram_wr_req_addr",
                f"output logic{flat_range(meta.dma.dram_data_width)} send_to_dram_wr_req_data",
                f"output logic{flat_range(meta.dma.dram_mask_width)} send_to_dram_wr_req_mask",
                "input  logic        send_to_dram_wr_req_rdy",
                "input  logic        recv_from_dram_wr_resp_val",
                "input  logic        recv_from_dram_wr_resp_msg",
                "output logic        recv_from_dram_wr_resp_rdy",
            ]
        )

    if meta.spm_read.enabled:
        ports.extend(
            [
                "input  logic        recv_from_ext_spm_rd_req_val",
                f"input  logic{flat_range(meta.spm_read.addr_width)} recv_from_ext_spm_rd_req_addr",
                "output logic        recv_from_ext_spm_rd_req_rdy",
                "output logic        send_to_ext_spm_rd_resp_val",
                f"output logic{flat_range(meta.spm_read.data_width)} send_to_ext_spm_rd_resp_data",
                "input  logic        send_to_ext_spm_rd_resp_rdy",
            ]
        )

    if meta.has_boundary_ports:
        side_sizes = {
            "south": meta.x_tiles,
            "north": meta.x_tiles,
            "east": meta.y_tiles,
            "west": meta.y_tiles,
        }
        for side in SIDES:
            for idx in range(side_sizes[side]):
                ports.extend(
                    [
                        f"input  logic        recv_data_on_boundary_{side}_{idx}_val",
                        f"input  logic{flat_range(meta.data_width)} recv_data_on_boundary_{side}_{idx}_msg",
                        f"output logic        recv_data_on_boundary_{side}_{idx}_rdy",
                    ]
                )
            for idx in range(side_sizes[side]):
                ports.extend(
                    [
                        f"output logic        send_data_on_boundary_{side}_{idx}_val",
                        f"output logic{flat_range(meta.data_width)} send_data_on_boundary_{side}_{idx}_msg",
                        f"input  logic        send_data_on_boundary_{side}_{idx}_rdy",
                    ]
                )

    ports.extend(
        [
            f"input  logic{flat_range(meta.id_width)} cgra_id",
            f"input  logic{flat_range(meta.addr_width)} address_lower",
            f"input  logic{flat_range(meta.addr_width)} address_upper",
        ]
    )
    return ports


def comma_join(lines: Iterable[str], indent: str = "  ") -> str:
    items = list(lines)
    return "\n".join(
        f"{indent}{line}{',' if idx != len(items) - 1 else ''}"
        for idx, line in enumerate(items)
    )


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
        lines.extend(
            [
                f"  assign w_recv_{side}_val[{idx}] = recv_data_on_boundary_{side}_{idx}_val;",
                f"  assign w_recv_{side}_msg[{idx}] = recv_data_on_boundary_{side}_{idx}_msg;",
                f"  assign recv_data_on_boundary_{side}_{idx}_rdy = w_recv_{side}_rdy[{idx}];",
            ]
        )
    for idx in range(count):
        lines.extend(
            [
                f"  assign send_data_on_boundary_{side}_{idx}_val = w_send_{side}_val[{idx}];",
                f"  assign send_data_on_boundary_{side}_{idx}_msg = w_send_{side}_msg[{idx}];",
                f"  assign w_send_{side}_rdy[{idx}] = send_data_on_boundary_{side}_{idx}_rdy;",
            ]
        )
    return "\n".join(lines)


def gen_tieoff_assigns(meta: CgraMetadata) -> str:
    lines: List[str] = []
    if not meta.has_inter_cgra_noc_ports:
        lines.extend(
            [
                "  assign recv_from_inter_cgra_noc_rdy = 1'b0;",
                "  assign send_to_inter_cgra_noc_val = 1'b0;",
                "  assign w_send_to_inter_cgra_noc_msg = '0;",
            ]
        )
    return "\n".join(lines)


def gen_dma_wires(meta: CgraMetadata) -> str:
    if not meta.dma.enabled:
        return ""
    return f"  {meta.dma.write_req_type} w_send_to_dram_wr_req_msg;"


def gen_dma_assigns(meta: CgraMetadata) -> str:
    if not meta.dma.enabled:
        return ""
    return "\n".join(
        [
            "  assign send_to_dram_wr_req_addr = w_send_to_dram_wr_req_msg.addr;",
            "  assign send_to_dram_wr_req_data = w_send_to_dram_wr_req_msg.data;",
            "  assign send_to_dram_wr_req_mask = w_send_to_dram_wr_req_msg.mask;",
        ]
    )


def gen_spm_read_wires(meta: CgraMetadata) -> str:
    if not meta.spm_read.enabled:
        return ""
    return "\n".join(
        [
            f"  {meta.spm_read.req_type} w_recv_from_ext_spm_rd_req_msg;",
            f"  {meta.spm_read.resp_type} w_send_to_ext_spm_rd_resp_msg;",
        ]
    )


def gen_spm_read_assigns(meta: CgraMetadata) -> str:
    if not meta.spm_read.enabled:
        return ""
    return "\n".join(
        [
            "  assign w_recv_from_ext_spm_rd_req_msg.addr = recv_from_ext_spm_rd_req_addr;",
            "  assign send_to_ext_spm_rd_resp_data = w_send_to_ext_spm_rd_resp_msg.data;",
        ]
    )


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
        boundary_wires = "".join(
            gen_boundary_wires(meta, side, side_sizes[side]) for side in SIDES
        ).rstrip()
        boundary_assigns = "\n\n".join(
            gen_boundary_assigns(side, side_sizes[side]) for side in SIDES
        ).rstrip()

    inst_ports = [
        ".clk                                ( clk )",
        ".reset                              ( reset )",
        ".recv_from_cpu_pkt__val             ( recv_from_cpu_pkt_val )",
        ".recv_from_cpu_pkt__msg             ( w_recv_from_cpu_pkt_msg )",
        ".recv_from_cpu_pkt__rdy             ( recv_from_cpu_pkt_rdy )",
        ".send_to_cpu_pkt__val               ( send_to_cpu_pkt_val )",
        ".send_to_cpu_pkt__msg               ( w_send_to_cpu_pkt_msg )",
        ".send_to_cpu_pkt__rdy               ( send_to_cpu_pkt_rdy )",
    ]
    if meta.dma.enabled:
        inst_ports.extend(
            [
                ".send_to_dram_rd_req__val          ( send_to_dram_rd_req_val )",
                ".send_to_dram_rd_req__msg          ( send_to_dram_rd_req_addr )",
                ".send_to_dram_rd_req__rdy          ( send_to_dram_rd_req_rdy )",
                ".recv_from_dram_rd_resp__val       ( recv_from_dram_rd_resp_val )",
                ".recv_from_dram_rd_resp__msg       ( recv_from_dram_rd_resp_data )",
                ".recv_from_dram_rd_resp__rdy       ( recv_from_dram_rd_resp_rdy )",
                ".send_to_dram_wr_req__val          ( send_to_dram_wr_req_val )",
                ".send_to_dram_wr_req__msg          ( w_send_to_dram_wr_req_msg )",
                ".send_to_dram_wr_req__rdy          ( send_to_dram_wr_req_rdy )",
                ".recv_from_dram_wr_resp__val       ( recv_from_dram_wr_resp_val )",
                ".recv_from_dram_wr_resp__msg       ( recv_from_dram_wr_resp_msg )",
                ".recv_from_dram_wr_resp__rdy       ( recv_from_dram_wr_resp_rdy )",
            ]
        )
    if meta.spm_read.enabled:
        inst_ports.extend(
            [
                ".recv_from_ext_spm_rd_req__val    ( recv_from_ext_spm_rd_req_val )",
                ".recv_from_ext_spm_rd_req__msg    ( w_recv_from_ext_spm_rd_req_msg )",
                ".recv_from_ext_spm_rd_req__rdy    ( recv_from_ext_spm_rd_req_rdy )",
                ".send_to_ext_spm_rd_resp__val     ( send_to_ext_spm_rd_resp_val )",
                ".send_to_ext_spm_rd_resp__msg     ( w_send_to_ext_spm_rd_resp_msg )",
                ".send_to_ext_spm_rd_resp__rdy     ( send_to_ext_spm_rd_resp_rdy )",
            ]
        )
    if meta.has_inter_cgra_noc_ports:
        inst_ports.extend(
            [
                ".recv_from_inter_cgra_noc__val      ( recv_from_inter_cgra_noc_val )",
                ".recv_from_inter_cgra_noc__msg      ( w_recv_from_inter_cgra_noc_msg )",
                ".recv_from_inter_cgra_noc__rdy      ( recv_from_inter_cgra_noc_rdy )",
                ".send_to_inter_cgra_noc__val        ( send_to_inter_cgra_noc_val )",
                ".send_to_inter_cgra_noc__msg        ( w_send_to_inter_cgra_noc_msg )",
                ".send_to_inter_cgra_noc__rdy        ( send_to_inter_cgra_noc_rdy )",
            ]
        )
    if meta.has_boundary_ports:
        for side in SIDES:
            inst_ports.extend(
                [
                    f".recv_data_on_boundary_{side}__val   ( w_recv_{side}_val )",
                    f".recv_data_on_boundary_{side}__msg   ( w_recv_{side}_msg )",
                    f".recv_data_on_boundary_{side}__rdy   ( w_recv_{side}_rdy )",
                    f".send_data_on_boundary_{side}__val   ( w_send_{side}_val )",
                    f".send_data_on_boundary_{side}__msg   ( w_send_{side}_msg )",
                    f".send_data_on_boundary_{side}__rdy   ( w_send_{side}_rdy )",
                ]
            )
    if meta.has_cgra_id_port:
        inst_ports.append(".cgra_id                            ( cgra_id )")
    if meta.has_address_ports:
        inst_ports.extend(
            [
                ".address_lower                      ( address_lower )",
                ".address_upper                      ( address_upper )",
            ]
        )

    return render_template(
        "cgra_wrapper.v.tpl",
        wrapper_module=meta.wrapper_module,
        top_module=meta.top_module,
        port_list=comma_join(wrapper_ports(meta)),
        intra_type=meta.intra_type,
        inter_type=meta.inter_type,
        dma_wires=gen_dma_wires(meta),
        dma_assigns=gen_dma_assigns(meta),
        spm_read_wires=gen_spm_read_wires(meta),
        spm_read_assigns=gen_spm_read_assigns(meta),
        boundary_wires=boundary_wires,
        boundary_assigns=boundary_assigns,
        tieoff_assigns=gen_tieoff_assigns(meta),
        inst_port_list=comma_join(inst_ports, indent="    "),
    )


def gen_scala(meta: CgraMetadata) -> str:
    commands = load_command_ids()
    functs = load_rocc_functs()
    rocc_funct_object = "\n".join(
        [
            "object CGRARoCCGenerated {",
            *(f"  val {name} = {value}" for name, value in functs.items()),
            "}",
        ]
    )
    cgra_cmd_object = "\n".join(
        [
            "object CGRACmdGenerated {",
            *(
                f"  val {name} = {value}"
                for name, value in sorted(
                    commands.items(), key=lambda item: (item[1], item[0])
                )
            ),
            "}",
        ]
    )
    dma_templates = ", ".join(
        f'BigInt("{packet:x}", 16)' for packet in meta.dma.packet_templates
    )
    return render_template(
        "cgra_generated.scala.tpl",
        top_module=meta.top_module,
        rocc_funct_object=rocc_funct_object,
        cgra_cmd_object=cgra_cmd_object,
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
        pkt_cmd_lsb=meta.pkt_cmd_lsb,
        pkt_data_payload_lsb=meta.pkt_data_payload_lsb,
        pkt_data_predicate_lsb=meta.pkt_data_predicate_lsb,
        pkt_data_addr_lsb=meta.pkt_data_addr_lsb,
        pkt_opaque_lsb=meta.pkt_opaque_lsb,
        pkt_dst_tile_lsb=meta.pkt_dst_tile_lsb,
        dma_enabled=str(meta.dma.enabled).lower(),
        dma_dram_addr_width=meta.dma.dram_addr_width,
        dma_dram_data_width=meta.dma.dram_data_width,
        dma_dram_mask_width=meta.dma.dram_mask_width,
        dma_spm_addr_width=meta.dma.spm_addr_width,
        dma_nbytes_width=meta.dma.nbytes_width,
        dma_tag_width=meta.dma.tag_width,
        dma_spm_words=meta.dma.spm_words,
        dma_write_req_addr_lsb=meta.dma.write_req_addr_lsb,
        dma_write_req_data_lsb=meta.dma.write_req_data_lsb,
        dma_write_req_mask_lsb=meta.dma.write_req_mask_lsb,
        dma_descriptor_spm_lsb=meta.dma.descriptor_spm_lsb,
        dma_descriptor_nbytes_lsb=meta.dma.descriptor_nbytes_lsb,
        dma_descriptor_tag_lsb=meta.dma.descriptor_tag_lsb,
        dma_descriptor_width=meta.dma.descriptor_width,
        dma_cmd_config_dram_addr_lo=meta.dma.cmd_config_dram_addr_lo,
        dma_cmd_config_dram_addr_hi=meta.dma.cmd_config_dram_addr_hi,
        dma_cmd_config_spm_addr=meta.dma.cmd_config_spm_addr,
        dma_cmd_config_bytes=meta.dma.cmd_config_bytes,
        dma_cmd_config_tag=meta.dma.cmd_config_tag,
        dma_cmd_mvin=meta.dma.cmd_mvin,
        dma_cmd_mvout=meta.dma.cmd_mvout,
        dma_cmd_done=meta.dma.cmd_done,
        dma_packet_templates=dma_templates,
        spm_read_enabled=str(meta.spm_read.enabled).lower(),
        spm_read_addr_width=meta.spm_read.addr_width,
        spm_read_data_width=meta.spm_read.data_width,
        spm_read_words=meta.spm_read.words,
        wrapper_module=meta.wrapper_module,
        rtl_resource=meta.rtl_resource,
        wrapper_resource=meta.wrapper_resource,
    )


def c_define(name: str, value: int) -> str:
    return f"#define {name} {value}"


def gen_c_protocol() -> str:
    commands = load_command_ids()
    functs = load_rocc_functs()
    lines = [
        "/*",
        " * Auto-generated by scripts/sync_cgra_blackbox.py.",
        f" * Command source: {CMD_TYPE_SOURCE.relative_to(ROOT)}",
        f" * RoCC funct source: {ROCC_PROTOCOL.relative_to(ROOT)}",
        " * Do not edit by hand.",
        " */",
        "#ifndef CGRA_PROTOCOL_GENERATED_H",
        "#define CGRA_PROTOCOL_GENERATED_H",
        "",
    ]
    for name, value in functs.items():
        lines.append(c_define(f"CGRA_FUNCT_{name}", value))
    lines.append("")
    for name, value in sorted(commands.items(), key=lambda item: (item[1], item[0])):
        lines.append(c_define(f"CGRA_{name}", value))
    lines.extend(["", "#endif", ""])
    return "\n".join(lines)


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

    def append_lsb_nbits(
        lines: List[str],
        define_base: str,
        offsets: Dict[str, Tuple[int, int]],
        field: str,
    ) -> None:
        lsb, nbits = require_offset(offsets, field)
        lines.append(c_define(f"{define_base}_LSB", lsb))
        lines.append(c_define(f"{define_base}_NBITS", nbits))

    def append_packed_array_shape(
        lines: List[str], define_base: str, field: str
    ) -> None:
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
        c_define("CGRA_HAS_DMA", int(meta.dma.enabled)),
        c_define("CGRA_DMA_DRAM_ADDR_NBITS", meta.dma.dram_addr_width),
        c_define("CGRA_DMA_DRAM_DATA_NBITS", meta.dma.dram_data_width),
        c_define("CGRA_DMA_DRAM_MASK_NBITS", meta.dma.dram_mask_width),
        c_define("CGRA_DMA_SPM_ADDR_NBITS", meta.dma.spm_addr_width),
        c_define("CGRA_DMA_NBYTES_NBITS", meta.dma.nbytes_width),
        c_define("CGRA_DMA_TAG_NBITS", meta.dma.tag_width),
        c_define("CGRA_DMA_SPM_WORDS", meta.dma.spm_words),
        c_define("CGRA_DMA_WR_REQ_ADDR_LSB", meta.dma.write_req_addr_lsb),
        c_define("CGRA_DMA_WR_REQ_DATA_LSB", meta.dma.write_req_data_lsb),
        c_define("CGRA_DMA_WR_REQ_MASK_LSB", meta.dma.write_req_mask_lsb),
        c_define("CGRA_DMA_DESC_SPM_ADDR_LSB", meta.dma.descriptor_spm_lsb),
        c_define("CGRA_DMA_DESC_SPM_ADDR_NBITS", meta.dma.spm_addr_width),
        c_define("CGRA_DMA_DESC_NBYTES_LSB", meta.dma.descriptor_nbytes_lsb),
        c_define("CGRA_DMA_DESC_NBYTES_NBITS", meta.dma.nbytes_width),
        c_define("CGRA_DMA_DESC_TAG_LSB", meta.dma.descriptor_tag_lsb),
        c_define("CGRA_DMA_DESC_TAG_NBITS", meta.dma.tag_width),
        c_define("CGRA_DMA_DESC_NBITS", meta.dma.descriptor_width),
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
        "read_reg_towards": "CTRL_READ_REG_FROM",
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
    parser.add_argument(
        "--rtl", type=Path, default=DEFAULT_RTL, help="PyMTL3-generated CGRA Verilog"
    )
    parser.add_argument(
        "--top-module", help="Top module name to wrap; inferred if omitted"
    )
    parser.add_argument(
        "--chipyard-vsrc",
        type=Path,
        default=DEFAULT_VSRC,
        help="Chipyard vsrc output directory",
    )
    parser.add_argument(
        "--scala-out",
        type=Path,
        default=DEFAULT_SCALA,
        help="Generated Scala params output",
    )
    parser.add_argument(
        "--c-layout-out",
        type=Path,
        default=DEFAULT_C_LAYOUT,
        help="Generated C layout header output",
    )
    parser.add_argument(
        "--c-protocol-out",
        type=Path,
        default=DEFAULT_C_PROTOCOL,
        help="Generated C command/funct protocol header output",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Parse and print metadata without writing files",
    )
    parser.add_argument(
        "--require-dma",
        action="store_true",
        help="fail unless the complete generated DMA interface is present",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    rtl_path = args.rtl.resolve()
    text = rtl_path.read_text(encoding="utf-8")
    typedefs = extract_typedefs(text)
    meta = infer_metadata(text, rtl_path.name, args.top_module)
    if args.require_dma and not meta.dma.enabled:
        raise ValueError("generated top has no DMA interface")

    wrapper_path = args.chipyard_vsrc / f"{meta.wrapper_module}.v"
    rtl_dst = args.chipyard_vsrc / rtl_path.name

    print(f"top_module={meta.top_module}")
    print(f"wrapper_module={meta.wrapper_module}")
    print(
        f"intra_width={meta.intra_width} inter_width={meta.inter_width} data_width={meta.data_width}"
    )
    print(f"x_tiles={meta.x_tiles} y_tiles={meta.y_tiles} num_tiles={meta.num_tiles}")
    print(f"has_boundary_ports={meta.has_boundary_ports}")
    print(f"has_inter_cgra_noc_ports={meta.has_inter_cgra_noc_ports}")
    print(
        f"has_cgra_id_port={meta.has_cgra_id_port} has_address_ports={meta.has_address_ports}"
    )
    print(
        f"has_dma={meta.dma.enabled} dram_addr_width={meta.dma.dram_addr_width} "
        f"dram_data_width={meta.dma.dram_data_width} dram_mask_width={meta.dma.dram_mask_width}"
    )
    print(
        f"dma_spm_addr_width={meta.dma.spm_addr_width} nbytes_width={meta.dma.nbytes_width} "
        f"tag_width={meta.dma.tag_width} descriptor_width={meta.dma.descriptor_width}"
    )
    print(
        f"has_spm_read={meta.spm_read.enabled} "
        f"spm_read_addr_width={meta.spm_read.addr_width} "
        f"spm_read_data_width={meta.spm_read.data_width} "
        f"spm_read_words={meta.spm_read.words}"
    )
    print(f"scala_out={args.scala_out}")
    print(f"c_layout_out={args.c_layout_out}")
    print(f"c_protocol_out={args.c_protocol_out}")
    print(f"wrapper_out={wrapper_path}")
    print(f"rtl_out={rtl_dst}")

    if args.dry_run:
        return 0

    args.chipyard_vsrc.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(rtl_path, rtl_dst)
    write_text(wrapper_path, gen_wrapper(meta))
    write_text(args.scala_out, gen_scala(meta))
    write_text(args.c_layout_out, gen_c_layout(meta, typedefs))
    write_text(args.c_protocol_out, gen_c_protocol())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
