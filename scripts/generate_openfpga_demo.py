#!/usr/bin/env python3
"""Generate OpenFPGA fabric demo collateral and sync it into Chipyard."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from openfpga_bitstream import parse_bitstream
from openfpga_chipyard_codegen import write_chipyard_config, write_scala_generated
from openfpga_demo_config import ROOT, DemoConfig, ensure_file, load_demo_config
from openfpga_flow_runner import run_openfpga_flow
from openfpga_pinmap import extract_pin_map
from openfpga_rtl_sync import sync_rtl
from openfpga_test_codegen import write_c_header, write_pin_map_header, write_run_env


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--config",
        required=True,
        help="OpenFPGA demo YAML, e.g. configs/openfpga/openfpga_and2.yaml",
    )
    return parser.parse_args()


def write_metadata(demo: DemoConfig, paths, pin_map, bitstream) -> None:
    metadata = {
        "name": demo.name,
        "workdir": str(paths.workdir.relative_to(ROOT)),
        "vsrc_dir": str(paths.vsrc_dir.relative_to(ROOT)),
        "config_protocol": {
            "type": demo.architecture.config_protocol.kind,
            "address_width": demo.architecture.config_protocol.address_width,
            "data_width": demo.architecture.config_protocol.data_width,
            "word_width": demo.architecture.config_protocol.word_width,
        },
        "user_interface": {
            "input_width": demo.input_width,
            "output_width": demo.output_width,
        },
        "chipyard": {
            "config_name": demo.chipyard.config_name,
            "peripheral_name": demo.chipyard.peripheral_name,
            "wrapper_module": demo.chipyard.wrapper_module,
            "wrapper_path": str(demo.wrapper_path.resolve()),
            "scala_object": demo.chipyard.scala_object,
        },
        "pin_map": pin_map.to_json_dict(),
        "bitstream": bitstream.to_json_dict(),
    }
    paths.workdir.mkdir(parents=True, exist_ok=True)
    (paths.workdir / "pin_map.json").write_text(
        json.dumps(pin_map.to_json_dict(), indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    (paths.workdir / "metadata.json").write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def print_summary(demo: DemoConfig, paths, pin_map, bitstream) -> None:
    cfg = demo.architecture.config_protocol
    data_slice = "0" if cfg.data_width == 1 else f"{cfg.data_width - 1}:0"
    print(f"Generated OpenFPGA demo in {paths.workdir.relative_to(ROOT)}")
    print(f"Synced RTL to {paths.vsrc_dir.relative_to(ROOT)}")
    print(f"Wrapper module: {demo.chipyard.wrapper_module}")
    print(f"Pin map: {pin_map.summary()}")
    print(f"Bitstream length: {bitstream.parsed_length}")
    print(
        "CFG words are prepacked for "
        f"CFG_WORD[{cfg.word_width - 1}:{cfg.data_width}]=address[{cfg.address_width - 1}:0], "
        f"CFG_WORD[{data_slice}]=data[{cfg.data_width - 1}:0]"
    )


def main() -> int:
    args = parse_args()
    demo = load_demo_config(Path(args.config))

    paths = run_openfpga_flow(demo)
    formal = ensure_file(
        paths.src_dir / f"{demo.application.top_module}_top_formal_verification.v",
        "OpenFPGA formal verification netlist",
    )
    pin_map = extract_pin_map(demo, formal)
    bitstream = parse_bitstream(paths.workdir / "fabric_bitstream.bit", demo)

    sync_rtl(demo, paths, pin_map)
    write_scala_generated(demo, bitstream)
    write_chipyard_config(demo)
    write_c_header(demo, bitstream)
    write_pin_map_header(demo, pin_map)
    write_run_env(demo)
    write_metadata(demo, paths, pin_map, bitstream)
    print_summary(demo, paths, pin_map, bitstream)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
