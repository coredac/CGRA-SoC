#!/usr/bin/env python3
"""Export the generated SoC resources and kernel metadata for Amoeba."""

import argparse
import re
from pathlib import Path

import yaml

from cgra_fast_api import (
    build_packet_types,
    load_kernel_config,
    make_vector_cgra_packets,
    ordered_packets,
    split_packets,
)
from cgra_rtl_generator import load_soc_config
from generate_accels import instance_layout
from generate_auto_links import (
    DEFAULT_CACHE_BLOCK_SCALA,
    DEFAULT_CGRA_SCALA,
    load_cgra_hardware,
    load_config,
)
from generate_gemmini_ext_spm import write
from util.schema import require_int

# The runtime runner selects this config; its SystemBus width sets Pool DMA lanes.
ROCC_CONFIG = (
    DEFAULT_CGRA_SCALA.parent.parent / "config" / "RoCCAcceleratorConfigs.scala"
)


def pool_lanes(element_bits, config_path=ROCC_CONFIG):
    config = re.search(
        r"^class MultiAccelRocketConfig extends Config\((.*?)(?=^(?:class|object) |\Z)",
        config_path.read_text(),
        re.M | re.S,
    )
    widths = (
        re.findall(r"WithSystemBusWidth\(([^)]*)\)", config.group(1)) if config else []
    )
    if len(widths) != 1 or not widths[0].strip().isdigit():
        raise ValueError(
            f"{config_path}: MultiAccelRocketConfig requires one literal SystemBus width"
        )
    width = int(widths[0])
    if width <= 0 or element_bits <= 0 or width % element_bits:
        raise ValueError("Pool element width must divide the SystemBus width")
    return width // element_bits


def kernel_metadata(path, arch_path, soc_path):
    source = yaml.safe_load(path.read_text())
    kind = source["kind"]
    runtime = source["runtime"]
    if "flat" in runtime and (
        not isinstance(runtime["flat"], bool) or runtime["flat"] and kind != "cgra"
    ):
        raise ValueError(f"{path}: flat tiling requires a CGRA boolean capability")
    metadata = {
        "name": source["kernel"]["name"],
        "kind": kind,
        **{
            field: runtime[field]
            for field in (
                "input_bits",
                "output_bits",
                "input_shapes",
                "output_shapes",
                "tile_rows",
                "max_tile_rows",
                "shape_limits",
                "whole_inputs",
            )
        },
    }
    if "query" in runtime:
        query = runtime["query"]
        metadata["query"] = {
            "library": str((path.parent / query["library"]).resolve()),
            "symbol": query["symbol"],
        }
    if kind == "pool":
        return {
            **metadata,
            "complete": runtime["complete"],
            "parameters": runtime["parameters"],
        }
    if kind in ("gemmini", "gemmini_conv", "gemmini_matrix", "gemmini_copy"):
        return {
            **metadata,
            "commands": runtime["commands"],
            "patches": runtime["patches"],
            **{
                field: runtime[field]
                for field in ("parameters", "complete", "workspace_divisor")
                if field in runtime
            },
        }
    if kind != "cgra":
        raise ValueError(f"unsupported kernel interface: {kind}")
    if "flat" in runtime:
        flat = runtime["flat"]
        shape = runtime.get("iteration_shape", runtime["output_shapes"][0])
        if flat and (
            len(shape) != 2
            or runtime["whole_inputs"]
            or any(item != shape for item in runtime["input_shapes"])
            or any(item != shape for item in runtime["output_shapes"])
            or any("row_stride" in item for item in runtime["symbols"])
        ):
            raise ValueError(
                f"{path}: flat tiling requires matching elementwise shapes without whole inputs or row strides"
            )
        metadata["flat"] = flat
    config = load_kernel_config(path, arch_path, soc_path)
    parameters = [
        item.role
        for item in config.runtime_symbols
        if item.kind == "count" and item.role != "elements"
    ]
    if parameters:
        metadata["parameters"] = parameters
    steps = {}
    for item in runtime["symbols"]:
        if "row_stride" not in item:
            continue
        stride = require_int(item, "row_stride", path)
        if item["role"] not in parameters or not 0 <= stride <= 0xFFFFFFFF:
            raise ValueError(
                f"{path}: row_stride requires a scalar parameter and uint32 step"
            )
        steps[item["role"]] = stride
    if steps:
        metadata["parameter_steps"] = steps
    types = build_packet_types(config)
    packets = ordered_packets(config, make_vector_cgra_packets(config, types))
    static, captured, _, _, _ = split_packets(config, packets, types)
    return {
        **metadata,
        "header": f"generated/cgra_{config.name}_fast_api.h",
        "symbol": config.name.upper(),
        "iteration_shape": list(
            runtime.get("iteration_shape", runtime["output_shapes"][0])
        ),
        "symbols": [item.role for item in config.runtime_symbols],
        "packets": len(captured),
        "static_packets": len(static),
        "count_scale": config.runtime_count.scale,
        "count_offset": config.runtime_count.offset,
    }


def target_metadata(soc_path, arch_path, kernels):
    layout = instance_layout(yaml.safe_load(soc_path.read_text()))
    link = load_config(soc_path)
    soc = load_soc_config(soc_path)
    hardware = load_cgra_hardware(DEFAULT_CGRA_SCALA, DEFAULT_CACHE_BLOCK_SCALA)
    devices = []
    for device in layout.instances:
        bridge = link.bridges.get(device.name)
        gemmini = layout.gemminis.get(device.name, {})
        devices.append(
            {
                **(
                    {
                        "pool_lanes": pool_lanes(
                            layout.pools[device.name]["element_bits"]
                        ),
                        "pool_line_entries": layout.pools[device.name][
                            "line_buffer_entries"
                        ],
                    }
                    if device.kind == "pool"
                    else {}
                ),
                "name": device.name,
                "kind": device.kind,
                "id": device.id,
                "spm_bytes": device.size,
                "slots": link.buffer_slots.get(device.name, 1),
                "jobs": link.jobs[device.name],
                "commands": (
                    gemmini.get("command_capacity", 16)
                    if device.kind == "gemmini"
                    else 0
                ),
                "patches": (
                    gemmini.get("patch_capacity", 32) if device.kind == "gemmini" else 0
                ),
                "packets": layout.cgra_packets.get(device.name, 0),
                "packed_word": bridge.outbound_word if bridge else 0,
                "packed_words": bridge.outbound_words if bridge else 0,
            }
        )
    collector = next(
        (device.id for device in layout.instances if device.kind == "cgra"), None
    )
    if collector is None:
        raise ValueError("target requires a CGRA result collector")
    return {
        "devices": devices,
        "fabric": {
            "stages": link.stage_capacity,
            "edges": link.dependency_capacity,
            "runs": link.run_capacity,
            "results": link.result_capacity
            or max(2, max(1, link.run_capacity) * link.stage_capacity),
            "collector": collector,
            "alignment": hardware.dma_beat_bytes,
        },
        "cgra": {
            "word_bits": soc.data_nbits,
            "counter_bits": soc.ctrl_count_bits,
            "words": soc.data_mem_size_per_bank * soc.num_banks_per_cgra,
        },
        "kernel_sources": [str(path.resolve()) for path in kernels],
        "kernels": [kernel_metadata(path, arch_path, soc_path) for path in kernels],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--soc-yaml", type=Path, required=True)
    parser.add_argument("--arch-yaml", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("kernels", nargs="*", type=Path)
    args = parser.parse_args()
    try:
        kernels = args.kernels
        if args.check and not kernels:
            sources = yaml.safe_load(args.output.read_text())["kernel_sources"]
            kernels = [Path(path) for path in sources]
        target = target_metadata(args.soc_yaml, args.arch_yaml, kernels)
        content = "# Generated by scripts/runtime_target.py. Do not edit.\n"
        content += yaml.safe_dump(target, sort_keys=False)
        write(args.output, content, args.check)
    except (OSError, KeyError, TypeError, ValueError) as error:
        parser.exit(1, f"runtime target: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
