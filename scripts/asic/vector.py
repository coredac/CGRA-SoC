#!/usr/bin/env python3
"""Build and check standalone GCN and ResNet ASIC kernels on small fixtures."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import gcn_data
import p1_resnet_data
from block_data import conv


def fixtures():
    graph = gcn_data.graphs(0)[0]
    weights = gcn_data.weights(0)
    resnet = p1_resnet_data.reference("small")
    for sign in (1, -1):
        gcn, report = gcn_data.reference(graph, weights, sign)
        source = [sign * value for value in resnet["p1_input"]]
        first = conv(source, resnet["p1_weights1"], 8, 8, 4, 8, 3, 2, 1)
        activated = [max(0, value) for value in first]
        main = conv(activated, resnet["p1_weights2"], 4, 4, 8, 8, 3, 1, 1)
        skip = conv(source, resnet["p1_projection"], 8, 8, 4, 8, 1, 2, 0)
        residual = [max(0, a + b) for a, b in zip(main, skip)]
        yield {
            "graph": report,
            "arrays": [
                gcn[name][1]
                for name in (
                    "rowptr",
                    "col",
                    "edge_weight",
                    "expected_z",
                    "expected_sum",
                    "expected_y",
                )
            ]
            + [first, activated, main, skip, residual],
        }


def run(args):
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    cases = list(fixtures())
    (output / "fixtures.json").write_text(json.dumps(cases, indent=2) + "\n")
    data = output / "fixtures.txt"
    data.write_text(
        "".join(
            str(len(values)) + " " + " ".join(map(str, values)) + "\n"
            for case in cases
            for values in case["arrays"]
        )
    )
    sources = [
        ROOT / "rtl/asic/vector/Gcn.sv",
        ROOT / "rtl/asic/vector/Vector.sv",
        ROOT / "rtl/asic/vector/GcnVector.sv",
        ROOT / "tests/asic/vector/Bench.sv",
        ROOT / "tests/asic/vector/run.cpp",
    ]
    build = [
        str(args.verilator.resolve()),
        "--cc",
        "--exe",
        "--build",
        "-j",
        "4",
        "-Wall",
        "--top-module",
        "Bench",
        "--Mdir",
        str(output / "obj"),
        "-o",
        "kernel-test",
        *map(str, sources),
    ]
    with (output / "build.log").open("w") as log:
        code = subprocess.run(build, stdout=log, stderr=subprocess.STDOUT).returncode
    if code:
        print(f"Build failed; see {output / 'build.log'}", file=sys.stderr)
        return code
    simulation = [str(output / "obj/kernel-test"), str(data)]
    with (output / "run.log").open("w") as log:
        code = subprocess.run(
            simulation, stdout=log, stderr=subprocess.STDOUT, timeout=120
        ).returncode
    if code:
        print(f"Simulation failed; see {output / 'run.log'}", file=sys.stderr)
        return code
    rows = [json.loads(line) for line in (output / "run.log").read_text().splitlines()]
    result = {
        "passed": len(rows) == 28,
        "scope": "Standalone integer kernels; excludes Gemmini, CPU, DMA, configuration and SoC interconnect.",
        "cycle_scope": "Sum of cycles per kernel, including both accepted start edge and final accepted output edge; preloads and host dispatch gaps excluded.",
        "traffic_scope": "32-bit RTL port transfers. Materialized intermediate counts a producer output plus consumer input. These are not measured DRAM transfers.",
        "sharing": "One GCN MAC datapath and one reusable ReLU/Add/Add+ReLU vector unit. Streamed GcnVector directly connects those two units with ready/valid and one outer command; two internal starts remain. Simulation instantiates alternatives only for isolated comparison, not as one proposed system.",
        "gcn_shape": {"nodes": 16, "channels": 128},
        "resnet_shape": "8x8x4 -> 4x4x8 projected block; only post-convolution vector kernels are timed.",
        "build_command": build,
        "simulation_command": simulation,
        "sha256": {
            str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sources
        },
        "fixture_sha256": hashlib.sha256(data.read_bytes()).hexdigest(),
        "records": rows,
    }
    (output / "results.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        f"{'PASS' if result['passed'] else 'FAIL'}: {len(rows)} kernel records; {output / 'results.json'}"
    )
    return 0 if result["passed"] else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=ROOT / "build/experiments/asic/vector"
    )
    parser.add_argument(
        "--verilator", type=Path, default=ROOT / "chipyard/.conda-env/bin/verilator"
    )
    return run(parser.parse_args())


if __name__ == "__main__":
    sys.exit(main())
