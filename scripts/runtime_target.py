#!/usr/bin/env python3
"""Check the hardware assumptions of Amoeba's fixed v1 SoC plan."""

import argparse
from pathlib import Path

import yaml

from generate_accels import instance_layout
from generate_auto_links import load_config


def check_target(soc_path, arch_path):
    soc = yaml.safe_load(soc_path.read_text())
    arch = yaml.safe_load(arch_path.read_text())
    layout = instance_layout(soc)
    link = load_config(soc_path)
    memory = soc["memory"]
    instances = soc["communication"]["instances"]
    errors = []
    if any(
        arch.get(name)
        for name in ("cgra_overrides", "tile_overrides", "link_overrides")
    ):
        errors.append("the fixed ReLU mapping does not support architecture overrides")
    if soc["execution"] != {"ctrl_count_bits": 16} or soc["interface"] != {
        "num_tile_inports": 4,
        "num_tile_outports": 4,
        "num_fu_inports": 4,
        "num_fu_outports": 2,
        "data_nbits": 32,
        "predicate_nbits": 1,
    }:
        errors.append("expected the attention execution counter and packet interface")
    if [(item.name, item.kind, item.id, item.size) for item in layout.instances] != [
        ("gemmini0", "gemmini", 0, 131072),
        ("gemmini1", "gemmini", 1, 131072),
        ("cgra0", "cgra", 2, 131072),
    ]:
        errors.append("expected Gemmini0/Gemmini1/CGRA0 with 128-KiB SPMs")
    if (
        arch["multi_cgra_defaults"] != {"rows": 1, "columns": 1}
        or arch["cgra_defaults"] != {"rows": 4, "columns": 4, "configMemSize": 64}
        or arch["tile_defaults"]["num_registers"] != 8
        or arch["tile_defaults"]["fu_types"]
        != [
            "add",
            "mul",
            "div",
            "logic",
            "shift",
            "cmp",
            "sel",
            "grant",
            "phi",
            "mem",
            "return",
            "gep",
        ]
        or memory["data_mem_size_global"] != 32768
        or memory["data_mem_size_per_bank"] != 4096
        or memory["num_banks_per_cgra"] != 8
        or memory["num_registers_per_reg_bank"] != 8
        or not memory["mem_access_is_combinational"]
    ):
        errors.append("expected the 4x4 attention CGRA with 32768 INT32 words")
    if link.buffer_slots != {"gemmini0": 2, "gemmini1": 2, "cgra0": 2}:
        errors.append("the v1 layout reserves exactly two slots per endpoint")
    bridge = link.bridges.get("cgra0")
    if bridge is None or bridge.outbound_word != 0 or bridge.outbound_words < 320:
        errors.append("CGRA must expose at least 320 packed INT8 words from word 0")
    if (
        link.stage_capacity < 3
        or link.dependency_capacity < 3
        or link.run_capacity < 1
        or link.result_capacity < 3
        or any(link.jobs.get(name, 0) < 1 for name in instances)
    ):
        errors.append("insufficient graph, run, job or result capacity")
    for name, commands in (("gemmini0", 3), ("gemmini1", 4)):
        limits = layout.gemminis.get(name, {})
        if (
            limits.get("command_capacity", 16) < commands
            or limits.get("patch_capacity", 32) < 2
        ):
            errors.append(f"{name} cannot capture the fixed GEMM template")
    if layout.cgra_packets.get("cgra0", 0) < 76:
        errors.append("CGRA needs 76 captured ReLU packets")
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--soc-yaml", type=Path, required=True)
    parser.add_argument("--arch-yaml", type=Path, required=True)
    args = parser.parse_args()
    for error in check_target(args.soc_yaml, args.arch_yaml):
        print(f"runtime target: {error}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
