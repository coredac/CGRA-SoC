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
    input_pad_port: str
    input_pad_left: int
    input_pad_right: int
    output_pad_port: str
    output_pad_left: int
    output_pad_right: int

    @property
    def has_split_pads(self) -> bool:
        return self.input_pad_port != self.output_pad_port

    @property
    def input_pad_count(self) -> int:
        return abs(self.input_pad_left - self.input_pad_right) + 1

    @property
    def output_pad_count(self) -> int:
        return abs(self.output_pad_left - self.output_pad_right) + 1

    @property
    def pad_left(self) -> int:
        return self.input_pad_left

    @property
    def pad_right(self) -> int:
        return self.input_pad_right

    @property
    def pad_count(self) -> int:
        return max(self.input_pad_count, self.output_pad_count)

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
            "input_pad_port": self.input_pad_port,
            "input_pad_left": self.input_pad_left,
            "input_pad_right": self.input_pad_right,
            "input_pad_count": self.input_pad_count,
            "output_pad_port": self.output_pad_port,
            "output_pad_left": self.output_pad_left,
            "output_pad_right": self.output_pad_right,
            "output_pad_count": self.output_pad_count,
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


def _derive_user_interface(
    ports: List[FormalPort],
    formal_verification: Path,
    excluded_input_ports: set[str],
) -> UserInterfaceSpec:
    input_ports = [
        port for port in ports if port.direction == "input" and port.name not in excluded_input_ports
    ]
    output_ports = [port for port in ports if port.direction == "output"]
    if not input_ports:
        raise ValueError(
            f"{formal_verification}: formal module declares no software-visible benchmark input ports"
        )
    if not output_ports:
        raise ValueError(f"{formal_verification}: formal module declares no benchmark output ports")

    input_register = _build_register("USER_INPUT", input_ports)
    output_register = _build_register("USER_OUTPUT", output_ports)
    if input_register.width > 32 or output_register.width > 32:
        raise ValueError(
            "current single-register MMIO USER_INPUT/USER_OUTPUT backend supports widths <= 32; "
            f"got input_width={input_register.width}, output_width={output_register.width}"
        )
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


def extract_interface_and_pin_map(
    formal_verification: Path,
    *,
    excluded_input_ports: Iterable[str] = (),
    known_input_ports: Iterable[str] = (),
) -> ExtractedInterface:
    text = _strip_comments(formal_verification.read_text(encoding="utf-8"))
    ports = _parse_formal_ports(text, formal_verification)
    excluded_inputs = set(excluded_input_ports)
    known_inputs = set(known_input_ports)
    input_port_names = {port.name for port in ports if port.direction == "input"}
    output_port_names = {port.name for port in ports if port.direction == "output"}
    for name in sorted(excluded_inputs | known_inputs):
        if name in output_port_names:
            raise ValueError(f"{formal_verification}: configured input port {name!r} is an output")
        if name not in input_port_names:
            raise ValueError(f"{formal_verification}: configured input port {name!r} is not declared")

    user_interface = _derive_user_interface(ports, formal_verification, excluded_inputs)
    input_ports = [port for port in ports if port.direction == "input"]
    output_ports = [port for port in ports if port.direction == "output"]
    input_ports_by_name, output_ports_by_name = _port_maps(ports)
    pad_ranges = _parse_pad_ranges(text, formal_verification)
    input_pad_port, output_pad_port = _select_pad_ports(pad_ranges, formal_verification)

    input_map: Dict[str, List[Optional[int]]] = {
        port.name: [None] * port.width for port in input_ports
    }
    output_map: Dict[str, List[Optional[int]]] = {
        port.name: [None] * port.width for port in output_ports
    }

    input_assign_re = re.compile(
        rf"\bassign\s+(gfpga_pad_(?:GPIO|GPIN|GPOUT)_PAD)_fm\[(\d+)\]\s*=\s*"
        rf"({_VERILOG_IDENT})(?:\[(\d+)\])?\s*;"
    )
    for pad_port, raw_pad, name, raw_bit in input_assign_re.findall(text):
        if pad_port != input_pad_port:
            raise ValueError(
                f"{formal_verification}: input pin map uses {pad_port}, expected {input_pad_port}"
            )
        port = input_ports_by_name.get(name)
        if port is None:
            raise ValueError(f"{formal_verification}: GPIO input assign references non-input port {name!r}")
        bit = _bit_index(port, raw_bit if raw_bit != "" else None, f"{formal_verification} input assign")
        _record(input_map, port, bit, int(raw_pad), f"{formal_verification} input assign")

    output_assign_re = re.compile(
        rf"\bassign\s+({_VERILOG_IDENT})(?:\[(\d+)\])?\s*=\s*"
        rf"(gfpga_pad_(?:GPIO|GPIN|GPOUT)_PAD)_fm\[(\d+)\]\s*;"
    )
    for name, raw_bit, pad_port, raw_pad in output_assign_re.findall(text):
        if pad_port != output_pad_port:
            raise ValueError(
                f"{formal_verification}: output pin map uses {pad_port}, expected {output_pad_port}"
            )
        port = output_ports_by_name.get(name)
        if port is None:
            raise ValueError(f"{formal_verification}: GPIO output assign references non-output port {name!r}")
        bit = _bit_index(port, raw_bit if raw_bit != "" else None, f"{formal_verification} output assign")
        _record(output_map, port, bit, int(raw_pad), f"{formal_verification} output assign")

    pin_map = PinMap(
        inputs=_finalize(input_map, input_ports, "input"),
        outputs=_finalize(output_map, output_ports, "output"),
        input_pad_port=input_pad_port,
        input_pad_left=pad_ranges[input_pad_port][0],
        input_pad_right=pad_ranges[input_pad_port][1],
        output_pad_port=output_pad_port,
        output_pad_left=pad_ranges[output_pad_port][0],
        output_pad_right=pad_ranges[output_pad_port][1],
    )

    _validate_pin_map_pads(pin_map)

    return ExtractedInterface(user_interface=user_interface, pin_map=pin_map)


def _parse_pad_ranges(text: str, formal_verification: Path) -> Dict[str, tuple[int, int]]:
    range_re = re.compile(
        r"\bwire\s+\[\s*(\d+)\s*:\s*(\d+)\s*\]\s+(gfpga_pad_(?:GPIO|GPIN|GPOUT)_PAD)_fm\s*;"
    )
    ranges: Dict[str, tuple[int, int]] = {}
    for raw_left, raw_right, port in range_re.findall(text):
        if port in ranges:
            raise ValueError(f"{formal_verification}: duplicate pad range for {port}")
        ranges[port] = (int(raw_left), int(raw_right))
    return ranges


def _select_pad_ports(
    pad_ranges: Dict[str, tuple[int, int]], formal_verification: Path
) -> tuple[str, str]:
    if "gfpga_pad_GPIO_PAD" in pad_ranges:
        return "gfpga_pad_GPIO_PAD", "gfpga_pad_GPIO_PAD"
    if "gfpga_pad_GPIN_PAD" in pad_ranges and "gfpga_pad_GPOUT_PAD" in pad_ranges:
        return "gfpga_pad_GPIN_PAD", "gfpga_pad_GPOUT_PAD"
    raise ValueError(
        f"could not parse supported FPGA GPIO pad ranges from {formal_verification}; "
        "expected gfpga_pad_GPIO_PAD_fm or both gfpga_pad_GPIN_PAD_fm/gfpga_pad_GPOUT_PAD_fm"
    )


def _validate_port_pads(
    pin_map: PinMap,
    port_name: str,
    pad_left: int,
    pad_right: int,
    pads: List[int],
) -> None:
    if len(pads) != len(set(pads)):
        raise ValueError(f"pin map has duplicate FPGA pads on {port_name}: {pin_map.to_json_dict()}")
    pad_min, pad_max = min(pad_left, pad_right), max(pad_left, pad_right)
    for pad in pads:
        if pad < pad_min or pad > pad_max:
            raise ValueError(f"pin map pad {pad} is outside {port_name} range [{pad_left}:{pad_right}]")


def _validate_pin_map_pads(pin_map: PinMap) -> None:
    by_port: Dict[str, List[int]] = {}
    by_port.setdefault(pin_map.input_pad_port, [])
    by_port[pin_map.input_pad_port].extend(pad for pads in pin_map.inputs.values() for pad in pads)
    by_port.setdefault(pin_map.output_pad_port, [])
    by_port[pin_map.output_pad_port].extend(pad for pads in pin_map.outputs.values() for pad in pads)

    _validate_port_pads(
        pin_map,
        pin_map.input_pad_port,
        pin_map.input_pad_left,
        pin_map.input_pad_right,
        by_port[pin_map.input_pad_port],
    )
    if pin_map.output_pad_port != pin_map.input_pad_port:
        _validate_port_pads(
            pin_map,
            pin_map.output_pad_port,
            pin_map.output_pad_left,
            pin_map.output_pad_right,
            by_port[pin_map.output_pad_port],
        )
