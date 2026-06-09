#!/usr/bin/env python3
"""Extract OpenFPGA user-port to GPIO pad mappings from formal netlists."""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Optional

from openfpga_demo_config import DemoConfig, FieldSpec


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


def _format_pads(pads: List[int]) -> str:
    return str(pads[0]) if len(pads) == 1 else "[" + ",".join(str(pad) for pad in pads) + "]"


def _expected_field_map(fields: Iterable[FieldSpec]) -> Dict[str, FieldSpec]:
    return {field.name: field for field in fields}


def _bit_index(field: FieldSpec, raw_bit: Optional[str], context: str) -> int:
    if raw_bit is None:
        if field.width == 1:
            return 0
        raise ValueError(f"{context}: field {field.name!r} has width {field.width}; netlist must name a bit")
    bit = int(raw_bit)
    if bit < 0 or bit >= field.width:
        raise ValueError(f"{context}: bit {bit} is outside field {field.name!r} width {field.width}")
    return bit


def _record(mapping: Dict[str, List[Optional[int]]], field: FieldSpec, bit: int, pad: int, context: str) -> None:
    current = mapping[field.name][bit]
    if current is not None and current != pad:
        raise ValueError(
            f"{context}: duplicate mapping for {field.name}[{bit}] disagrees: pad {current} vs {pad}"
        )
    mapping[field.name][bit] = pad


def _finalize(mapping: Dict[str, List[Optional[int]]], fields: Iterable[FieldSpec], direction: str) -> Dict[str, List[int]]:
    result: Dict[str, List[int]] = {}
    for field in fields:
        values = mapping[field.name]
        missing = [index for index, pad in enumerate(values) if pad is None]
        if missing:
            raise ValueError(f"could not extract {direction} pin map for {field.name} bits {missing}")
        result[field.name] = [int(pad) for pad in values if pad is not None]
    return result


def extract_pin_map(demo: DemoConfig, formal_verification: Path) -> PinMap:
    text = formal_verification.read_text(encoding="utf-8")
    input_fields = _expected_field_map(demo.user_interface.input_register.fields)
    output_fields = _expected_field_map(demo.user_interface.output_register.fields)

    input_map: Dict[str, List[Optional[int]]] = {
        field.name: [None] * field.width for field in input_fields.values()
    }
    output_map: Dict[str, List[Optional[int]]] = {
        field.name: [None] * field.width for field in output_fields.values()
    }

    comment_re = re.compile(
        r"Blif Benchmark\s+(input|output)\s+(\w+)\s+is mapped to FPGA IOPAD "
        r"gfpga_pad_GPIO_PAD_fm\[(\d+)\]"
    )
    for direction, name, raw_pad in comment_re.findall(text):
        pad = int(raw_pad)
        fields = input_fields if direction == "input" else output_fields
        mapping = input_map if direction == "input" else output_map
        field = fields.get(name)
        if field is None:
            continue
        if field.width == 1:
            _record(mapping, field, 0, pad, f"{formal_verification} comment")

    input_assign_re = re.compile(
        r"assign\s+gfpga_pad_GPIO_PAD_fm\[(\d+)\]\s*=\s*(\w+)(?:\[(\d+)\])?\s*;"
    )
    for raw_pad, name, raw_bit in input_assign_re.findall(text):
        field = input_fields.get(name)
        if field is None:
            continue
        bit = _bit_index(field, raw_bit if raw_bit != "" else None, f"{formal_verification} input assign")
        _record(input_map, field, bit, int(raw_pad), f"{formal_verification} input assign")

    output_assign_re = re.compile(
        r"assign\s+(\w+)(?:\[(\d+)\])?\s*=\s*gfpga_pad_GPIO_PAD_fm\[(\d+)\]\s*;"
    )
    for name, raw_bit, raw_pad in output_assign_re.findall(text):
        field = output_fields.get(name)
        if field is None:
            continue
        bit = _bit_index(field, raw_bit if raw_bit != "" else None, f"{formal_verification} output assign")
        _record(output_map, field, bit, int(raw_pad), f"{formal_verification} output assign")

    range_match = re.search(r"wire\s+\[(\d+)\s*:\s*(\d+)\]\s+gfpga_pad_GPIO_PAD_fm\s*;", text)
    if not range_match:
        raise ValueError(f"could not parse gfpga_pad_GPIO_PAD_fm range from {formal_verification}")
    pad_left, pad_right = int(range_match.group(1)), int(range_match.group(2))
    pad_min, pad_max = min(pad_left, pad_right), max(pad_left, pad_right)

    pin_map = PinMap(
        inputs=_finalize(input_map, input_fields.values(), "input"),
        outputs=_finalize(output_map, output_fields.values(), "output"),
        pad_left=pad_left,
        pad_right=pad_right,
    )

    pads = pin_map.all_pads
    if len(pads) != len(set(pads)):
        raise ValueError(f"pin map has duplicate FPGA pads: {pin_map.to_json_dict()}")
    for pad in pads:
        if pad < pad_min or pad > pad_max:
            raise ValueError(f"pin map pad {pad} is outside GPIO pad range [{pad_left}:{pad_right}]")

    return pin_map
