#!/usr/bin/env python3
"""Extract OpenFPGA user-port to GPIO pad mappings from formal netlists."""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Optional

from config import FieldSpec, RegisterSpec, UserInterfaceSpec


_VERILOG_IDENT = r"[A-Za-z_][A-Za-z0-9_]*"


@dataclass(frozen=True)
class PinMap:
    inputs: Dict[str, List[int]]
    outputs: Dict[str, List[int]]
    pad_left: int
    pad_right: int

    @property
    def pad_count(self) -> int:
        return abs(self.pad_left - self.pad_right) + 1

    @property
    def all_pads(self) -> List[int]:
        pads: List[int] = []
        for group in (self.inputs, self.outputs):
            for values in group.values():
                pads.extend(values)
        return pads

    def to_json_dict(self) -> Dict[str, object]:
        return {
            "inputs": self.inputs,
            "outputs": self.outputs,
            "pad_left": self.pad_left,
            "pad_right": self.pad_right,
            "pad_count": self.pad_count,
        }

    def summary(self) -> str:
        pieces = []
        for name, pads in self.inputs.items():
            pieces.append(f"{name}={_format_pads(pads)}")
        for name, pads in self.outputs.items():
            pieces.append(f"{name}={_format_pads(pads)}")
        return " ".join(pieces)


@dataclass(frozen=True)
class ExtractedInterface:
    user_interface: UserInterfaceSpec
    pin_map: PinMap


@dataclass(frozen=True)
class FormalPort:
    direction: str
    name: str
    bit_indices: List[int]

    @property
    def width(self) -> int:
        return len(self.bit_indices)


def _format_pads(pads: List[int]) -> str:
    return str(pads[0]) if len(pads) == 1 else "[" + ",".join(str(pad) for pad in pads) + "]"


def _strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return "\n".join(line.split("//", 1)[0] for line in text.splitlines())


def _parse_range(raw_range: Optional[str], context: str) -> List[int]:
    if raw_range is None:
        return [0]
    match = re.fullmatch(r"\[\s*(\d+)\s*:\s*(\d+)\s*\]", raw_range)
    if not match:
        raise ValueError(f"{context}: unsupported port range {raw_range!r}")
    left, right = int(match.group(1)), int(match.group(2))
    return list(range(min(left, right), max(left, right) + 1))


def _split_port_items(header: str) -> List[str]:
    items = [item.strip() for item in header.split(",")]
    if not items or any(not item for item in items):
        raise ValueError("formal module port declaration has an empty port item")
    return items


def _parse_formal_ports(text: str, formal_verification: Path) -> List[FormalPort]:
    module_re = re.compile(
        rf"\bmodule\s+(?P<module>{_VERILOG_IDENT})\s*\((?P<ports>.*?)\)\s*;",
        flags=re.DOTALL,
    )
    module_match = module_re.search(text)
    if module_match is None:
        raise ValueError(f"could not parse formal module port declaration from {formal_verification}")

    decl_re = re.compile(
        rf"^(?:(?P<direction>input|output)\s+)?"
        rf"(?:(?:wire|reg|logic|signed)\s+)*"
        rf"(?:(?P<range>\[\s*\d+\s*:\s*\d+\s*\])\s+)?"
        rf"(?P<name>{_VERILOG_IDENT})$"
    )

    ports: List[FormalPort] = []
    seen_names: set[str] = set()
    current_direction: Optional[str] = None
    current_range: Optional[str] = None
    for index, item in enumerate(_split_port_items(module_match.group("ports"))):
        match = decl_re.fullmatch(" ".join(item.split()))
        if match is None:
            raise ValueError(
                f"{formal_verification}: unsupported formal module port declaration item {index}: {item!r}"
            )

        direction = match.group("direction")
        raw_range = match.group("range")
        if direction is None:
            if current_direction is None:
                raise ValueError(
                    f"{formal_verification}: port declaration item {index} omits input/output direction"
                )
            direction = current_direction
            raw_range = raw_range if raw_range is not None else current_range
        else:
            current_direction = direction
            current_range = raw_range

        name = match.group("name")
        if name in seen_names:
            raise ValueError(f"{formal_verification}: duplicate formal module port {name!r}")
        seen_names.add(name)
        ports.append(
            FormalPort(
                direction=direction,
                name=name,
                bit_indices=_parse_range(raw_range, f"{formal_verification} port {name}"),
            )
        )

    if not ports:
        raise ValueError(f"{formal_verification}: formal module declares no benchmark ports")
    return ports


def _build_register(name: str, ports: Iterable[FormalPort]) -> RegisterSpec:
    fields: List[FieldSpec] = []
    next_lsb = 0
    for port in ports:
        fields.append(FieldSpec(name=port.name, width=port.width, lsb=next_lsb))
        next_lsb += port.width
    return RegisterSpec(name=name, fields=fields, width=next_lsb)


def _derive_user_interface(ports: List[FormalPort], formal_verification: Path) -> UserInterfaceSpec:
    input_ports = [port for port in ports if port.direction == "input"]
    output_ports = [port for port in ports if port.direction == "output"]
    if not input_ports:
        raise ValueError(f"{formal_verification}: formal module declares no benchmark input ports")
    if not output_ports:
        raise ValueError(f"{formal_verification}: formal module declares no benchmark output ports")

    input_register = _build_register("USER_INPUT", input_ports)
    output_register = _build_register("USER_OUTPUT", output_ports)
    if input_register.width > 32 or output_register.width > 32:
        raise ValueError("packed USER_INPUT/USER_OUTPUT MMIO registers currently support widths <= 32")
    return UserInterfaceSpec(input_register=input_register, output_register=output_register)


def _port_maps(ports: List[FormalPort]) -> tuple[Dict[str, FormalPort], Dict[str, FormalPort]]:
    inputs = {port.name: port for port in ports if port.direction == "input"}
    outputs = {port.name: port for port in ports if port.direction == "output"}
    return inputs, outputs


def _bit_index(port: FormalPort, raw_bit: Optional[str], context: str) -> int:
    if raw_bit is None:
        if port.width == 1:
            return 0
        raise ValueError(f"{context}: port {port.name!r} has width {port.width}; netlist must name a bit")
    bit = int(raw_bit)
    try:
        return port.bit_indices.index(bit)
    except ValueError as exc:
        raise ValueError(
            f"{context}: bit {bit} is outside port {port.name!r} range {port.bit_indices}"
        ) from exc


def _record(mapping: Dict[str, List[Optional[int]]], port: FormalPort, bit: int, pad: int, context: str) -> None:
    current = mapping[port.name][bit]
    port_bit = port.bit_indices[bit]
    if current is not None:
        raise ValueError(f"{context}: duplicate mapping for {port.name}[{port_bit}]")
    mapping[port.name][bit] = pad


def _finalize(mapping: Dict[str, List[Optional[int]]], ports: Iterable[FormalPort], direction: str) -> Dict[str, List[int]]:
    result: Dict[str, List[int]] = {}
    for port in ports:
        values = mapping[port.name]
        missing = [port.bit_indices[index] for index, pad in enumerate(values) if pad is None]
        if missing:
            raise ValueError(f"could not extract {direction} pin map for {port.name} bits {missing}")
        result[port.name] = [int(pad) for pad in values if pad is not None]
    return result


def extract_interface_and_pin_map(formal_verification: Path) -> ExtractedInterface:
    text = _strip_comments(formal_verification.read_text(encoding="utf-8"))
    ports = _parse_formal_ports(text, formal_verification)
    user_interface = _derive_user_interface(ports, formal_verification)
    input_ports = [port for port in ports if port.direction == "input"]
    output_ports = [port for port in ports if port.direction == "output"]
    input_ports_by_name, output_ports_by_name = _port_maps(ports)

    input_map: Dict[str, List[Optional[int]]] = {
        port.name: [None] * port.width for port in input_ports
    }
    output_map: Dict[str, List[Optional[int]]] = {
        port.name: [None] * port.width for port in output_ports
    }

    input_assign_re = re.compile(
        rf"\bassign\s+gfpga_pad_GPIO_PAD_fm\[(\d+)\]\s*=\s*({_VERILOG_IDENT})(?:\[(\d+)\])?\s*;"
    )
    for raw_pad, name, raw_bit in input_assign_re.findall(text):
        port = input_ports_by_name.get(name)
        if port is None:
            raise ValueError(f"{formal_verification}: GPIO input assign references non-input port {name!r}")
        bit = _bit_index(port, raw_bit if raw_bit != "" else None, f"{formal_verification} input assign")
        _record(input_map, port, bit, int(raw_pad), f"{formal_verification} input assign")

    output_assign_re = re.compile(
        rf"\bassign\s+({_VERILOG_IDENT})(?:\[(\d+)\])?\s*=\s*gfpga_pad_GPIO_PAD_fm\[(\d+)\]\s*;"
    )
    for name, raw_bit, raw_pad in output_assign_re.findall(text):
        port = output_ports_by_name.get(name)
        if port is None:
            raise ValueError(f"{formal_verification}: GPIO output assign references non-output port {name!r}")
        bit = _bit_index(port, raw_bit if raw_bit != "" else None, f"{formal_verification} output assign")
        _record(output_map, port, bit, int(raw_pad), f"{formal_verification} output assign")

    range_match = re.search(r"\bwire\s+\[\s*(\d+)\s*:\s*(\d+)\s*\]\s+gfpga_pad_GPIO_PAD_fm\s*;", text)
    if not range_match:
        raise ValueError(f"could not parse gfpga_pad_GPIO_PAD_fm range from {formal_verification}")
    pad_left, pad_right = int(range_match.group(1)), int(range_match.group(2))
    pad_min, pad_max = min(pad_left, pad_right), max(pad_left, pad_right)

    pin_map = PinMap(
        inputs=_finalize(input_map, input_ports, "input"),
        outputs=_finalize(output_map, output_ports, "output"),
        pad_left=pad_left,
        pad_right=pad_right,
    )

    pads = pin_map.all_pads
    if len(pads) != len(set(pads)):
        raise ValueError(f"pin map has duplicate FPGA pads: {pin_map.to_json_dict()}")
    for pad in pads:
        if pad < pad_min or pad > pad_max:
            raise ValueError(f"pin map pad {pad} is outside GPIO pad range [{pad_left}:{pad_right}]")

    return ExtractedInterface(user_interface=user_interface, pin_map=pin_map)
