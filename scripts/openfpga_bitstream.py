#!/usr/bin/env python3
"""Parse OpenFPGA fabric bitstreams and prepack MMIO CFG_WORD values."""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List

from openfpga_demo_config import ConfigProtocol, DemoConfig


@dataclass(frozen=True)
class PackedBitstream:
    words: List[int]
    declared_length: int | None

    @property
    def parsed_length(self) -> int:
        return len(self.words)

    def to_json_dict(self) -> Dict[str, int | None]:
        return {
            "declared_length": self.declared_length,
            "parsed_length": self.parsed_length,
        }


def parse_bitstream(bitstream: Path, demo: DemoConfig) -> PackedBitstream:
    cfg = demo.architecture.config_protocol
    lines = bitstream.read_text(encoding="utf-8").splitlines()
    header = "\n".join(line for line in lines if line.startswith("//"))
    if "Bitstream width (LSB -> MSB)" not in header:
        raise ValueError(f"unsupported bitstream bit order in {bitstream}")

    expected_header = f"<address {cfg.address_width} bits><data input {cfg.data_width} bits>"
    if expected_header not in header:
        raise ValueError(
            f"bitstream header does not match CFG_WORD contract; expected {expected_header}"
        )

    data_lines = [line.strip() for line in lines if re.fullmatch(r"[01]+", line.strip())]
    if not data_lines:
        raise ValueError(f"no bitstream records found in {bitstream}")
    if any(len(line) != cfg.word_width for line in data_lines):
        widths = sorted({len(line) for line in data_lines})
        raise ValueError(f"unexpected bitstream record widths {widths}; expected {cfg.word_width}")

    words = [_pack_cfg_word(line, cfg) for line in data_lines]
    length_match = re.search(r"Bitstream length:\s+(\d+)", header)
    declared_len = int(length_match.group(1)) if length_match else None
    if declared_len is not None and declared_len != len(words):
        raise ValueError(f"bitstream length mismatch: header={declared_len} parsed={len(words)}")

    return PackedBitstream(words=words, declared_length=declared_len)


def _pack_cfg_word(record: str, cfg: ConfigProtocol) -> int:
    addr_bits = record[: cfg.address_width]
    data_bits = record[cfg.address_width :]
    address = sum((1 << index) for index, bit in enumerate(addr_bits) if bit == "1")
    data = sum((1 << index) for index, bit in enumerate(data_bits) if bit == "1")
    return (address << cfg.data_width) | data
