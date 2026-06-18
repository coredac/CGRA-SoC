#!/usr/bin/env python3
"""Generate C headers consumed by baremetal OpenFPGA demo tests."""

from __future__ import annotations

from typing import List

from bitstream import PackedBitstream
from config import DemoConfig, FieldSpec, UserInterfaceSpec
from pinmap import PinMap


def _format_array(values: List[int], hex_digits: int) -> str:
    formatted = [f"0x{word:0{hex_digits}x}" for word in values]
    rows = [", ".join(formatted[i : i + 8]) for i in range(0, len(formatted), 8)]
    return ",\n  ".join(rows)


def _cfg_array_type(width: int) -> str:
    if width <= 8:
        return "uint8_t"
    if width <= 16:
        return "uint16_t"
    if width <= 32:
        return "uint32_t"
    raise ValueError(
        f"config word width {width} exceeds the current MMIO config-word backend limit of 32 bits"
    )


def _field_mask(field: FieldSpec) -> int:
    return ((1 << field.width) - 1) << field.lsb


def _field_macros(prefix: str, register: str, fields: List[FieldSpec]) -> List[str]:
    lines: List[str] = []
    for field in fields:
        name = field.macro_name
        mask = _field_mask(field)
        lines.extend(
            [
                f"#define {prefix}_{register}_FIELD_{name}_WIDTH {field.width}",
                f"#define {prefix}_{register}_FIELD_{name}_LSB {field.lsb}",
                f"#define {prefix}_{register}_FIELD_{name}_MASK 0x{mask:08x}u",
                (
                    f"#define {prefix}_{register}_FIELD_{name}_PACK(value) "
                    f"((((uint32_t)(value)) << {prefix}_{register}_FIELD_{name}_LSB) & "
                    f"{prefix}_{register}_FIELD_{name}_MASK)"
                ),
                (
                    f"#define {prefix}_{register}_FIELD_{name}_GET(value) "
                    f"((((uint32_t)(value)) & {prefix}_{register}_FIELD_{name}_MASK) >> "
                    f"{prefix}_{register}_FIELD_{name}_LSB)"
                ),
                "",
            ]
        )
    return lines


def write_c_header(demo: DemoConfig, user_interface: UserInterfaceSpec, bitstream: PackedBitstream) -> None:
    cfg = demo.architecture.config_protocol
    input_register = user_interface.input_register
    output_register = user_interface.output_register
    prefix = demo.macro_prefix
    array_name = f"{demo.c_identifier}_cfg_words"
    getter_name = f"{demo.c_identifier}_cfg_word"
    array_type = _cfg_array_type(cfg.word_width)
    hex_digits = max(1, (cfg.word_width + 3) // 4)
    array_body = _format_array(bitstream.words, hex_digits)

    lines = [
        f"#ifndef {prefix}_BITSTREAM_H",
        f"#define {prefix}_BITSTREAM_H",
        "",
        "#include <stdint.h>",
        "",
        f"#define {prefix}_BASE 0x{demo.soc.base_address:x}UL",
        f"#define {prefix}_SIZE 0x{demo.soc.size:x}UL",
        f"#define {prefix}_BITSTREAM_LEN {bitstream.parsed_length}",
        f"#define {prefix}_CONFIG_PROTOCOL_{cfg.kind.upper()} 1",
        f"#define {prefix}_CFG_WORD_WIDTH {cfg.word_width}",
        f"#define {prefix}_CFG_ADDR_WIDTH {cfg.address_width}",
        f"#define {prefix}_CFG_DATA_WIDTH {cfg.data_width}",
        f"#define {prefix}_INPUT_WIDTH {input_register.width}",
        f"#define {prefix}_OUTPUT_WIDTH {output_register.width}",
        "",
    ]
    lines.extend(_field_macros(prefix, "INPUT", input_register.fields))
    lines.extend(_field_macros(prefix, "OUTPUT", output_register.fields))
    lines.extend(
        [
            f"static const {array_type} {array_name}[{prefix}_BITSTREAM_LEN] = {{",
            f"  {array_body}",
            "};",
            "",
            f"static inline uint32_t {getter_name}(uint32_t index) {{",
            f"  return {array_name}[index];",
            "}",
            "",
            "#endif",
            "",
        ]
    )
    demo.bitstream_header_path.parent.mkdir(parents=True, exist_ok=True)
    demo.bitstream_header_path.write_text("\n".join(lines), encoding="utf-8")


def write_pin_map_header(demo: DemoConfig, user_interface: UserInterfaceSpec, pin_map: PinMap) -> None:
    prefix = demo.macro_prefix
    lines = [
        f"#ifndef {prefix}_PIN_MAP_H",
        f"#define {prefix}_PIN_MAP_H",
        "",
    ]
    for field in user_interface.input_register.fields:
        pads = pin_map.inputs[field.name]
        for bit, pad in enumerate(pads):
            lines.append(f"#define {prefix}_PAD_{field.macro_name}_{bit} {pad}")
        if field.width == 1:
            lines.append(f"#define {prefix}_PAD_{field.macro_name} {pads[0]}")
    for field in user_interface.output_register.fields:
        pads = pin_map.outputs[field.name]
        for bit, pad in enumerate(pads):
            lines.append(f"#define {prefix}_PAD_{field.macro_name}_{bit} {pad}")
        if field.width == 1:
            lines.append(f"#define {prefix}_PAD_{field.macro_name} {pads[0]}")
    lines.extend(
        [
            f"#define {prefix}_GPIO_PAD_COUNT {pin_map.pad_count}",
            "",
            "#endif",
            "",
        ]
    )
    demo.pin_map_header_path.parent.mkdir(parents=True, exist_ok=True)
    demo.pin_map_header_path.write_text("\n".join(lines), encoding="utf-8")
