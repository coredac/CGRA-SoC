#!/usr/bin/env python3
"""Generate deterministic synthetic GPT-2 block data and stage references.

Weights have sixteen nonzero taps per output, distributed over the input axis.
Hardware receives dense matrices; sparsity keeps the dependency-free host
reference practical. This fixture is not a trained model or performance claim.
"""

import argparse
import json
import random
from array import array
from pathlib import Path

import block_math as bm

ROOT = Path(__file__).resolve().parents[1]
NAMES = {"q": "q", "k": "k", "v": "v", "attention": "o", "fc1": "1", "fc2": "2"}


def weight(rng, inputs, outputs):
    dense = array("b", [0]) * (inputs * outputs)
    taps = []
    for out in range(outputs):
        column = []
        for part in range(16):
            lo, hi = part * inputs // 16, (part + 1) * inputs // 16
            index = rng.randrange(lo, hi)
            value = rng.choice((-32, 32))
            dense[index * outputs + out] = value
            column.append((index, value))
        taps.append(column)
    return dense, taps


def flatten(rows):
    return [value for row in rows for value in row]


def reference(heads=2, length=128, seed=0, run=0):
    hidden, expanded = heads * 64, heads * 256
    rng = random.Random(seed)
    input_rng = random.Random(seed + 1000 + run)
    source = [
        [input_rng.randrange(-32, 33) for _ in range(hidden)] for _ in range(length)
    ]
    tensors = {"x": ("int8_t", flatten(source))}
    weights, biases = {}, {}
    for name, inputs, outputs in (
        ("q", hidden, hidden),
        ("k", hidden, hidden),
        ("v", hidden, hidden),
        ("attention", hidden, hidden),
        ("fc1", hidden, expanded),
        ("fc2", expanded, hidden),
    ):
        dense, weights[name] = weight(rng, inputs, outputs)
        biases[name] = [rng.randrange(-128, 129) for _ in range(outputs)]
        tensors[f"w{NAMES[name]}"] = ("int8_t", dense)
        tensors[f"b{NAMES[name]}"] = ("int32_t", biases[name])
    params = []
    for stage in (1, 2):
        gamma = [rng.randrange(48, 81) for _ in range(hidden)]
        beta = [rng.randrange(-8, 9) for _ in range(hidden)]
        params.append((gamma, beta))
        tensors[f"gamma{stage}"] = ("int32_t", gamma)
        tensors[f"beta{stage}"] = ("int32_t", beta)

    integer, floating, clipping = {}, {}, {}
    float_source = [[value / 16 for value in row] for row in source]

    def norm(name, rows, floats, param):
        output, count, stats = bm.layernorm(rows, *param)
        integer[name], clipping[name] = output, count
        floating[name] = bm.float_layernorm(floats, *param)
        for index, field in enumerate(("sum", "mean", "squares", "variance", "std")):
            kind = "int32_t" if field == "mean" else "uint32_t"
            tensors[f"{name}_{field}"] = (kind, [row[index] for row in stats])

    def linear(name, rows, floats, input_scale=16):
        accum = bm.project(rows, weights[name], biases[name])
        tensors[f"{name}_accum"] = ("int32_t", flatten(accum))
        integer[name], clipping[name] = bm.quantize(accum, bm.DIVISORS[name])
        floating[name] = bm.float_project(
            floats, weights[name], biases[name], input_scale
        )

    norm("ln1", source, float_source, params[0])
    for name in ("q", "k", "v"):
        linear(name, integer["ln1"], floating["ln1"])
    concat, count, scores, probability, pv = bm.attention(
        integer["q"], integer["k"], integer["v"], heads
    )
    integer["concat"], clipping["concat"] = concat, count
    floating["concat"] = bm.float_attention(
        floating["q"], floating["k"], floating["v"], heads
    )
    tensors["scores"] = ("int32_t", flatten(scores))
    tensors["probability"] = ("int8_t", flatten(probability))
    tensors["pv_accum"] = ("int32_t", flatten(pv))
    linear("attention", concat, floating["concat"], 8)
    integer["u"], clipping["u"] = bm.quantize(bm.add(source, integer["attention"]))
    floating["u"] = bm.add(float_source, floating["attention"])
    norm("ln2", integer["u"], floating["u"], params[1])
    linear("fc1", integer["ln2"], floating["ln2"])
    integer["gelu"], clipping["gelu"] = bm.gelu(integer["fc1"])
    floating["gelu"] = bm.float_gelu(floating["fc1"])
    linear("fc2", integer["gelu"], floating["gelu"])
    integer["y"], clipping["y"] = bm.quantize(bm.add(integer["u"], integer["fc2"]))
    floating["y"] = bm.add(floating["u"], floating["fc2"])
    for name, rows in integer.items():
        tensors[f"expected_{name}"] = ("int8_t", flatten(rows))
    report = {
        "heads": heads,
        "length": length,
        "hidden": hidden,
        "expanded": expanded,
        "seed": seed,
        "run": run,
        "epsilon": bm.EPSILON,
        "weights": "16 signed taps/output; dense row-major hardware matrices",
        "rounding": "nearest ties away, except LN internal floor/truncate",
        "gelu_input": "INT8 Q4 expanded to Q6 [-512,508]; FC1 clipping counted",
        "clipped": clipping,
        "float_error": {
            name: bm.error(rows, floating[name], bm.SCALES.get(name, 16))
            for name, rows in integer.items()
        },
        "accum_max_abs": {
            name: max(map(abs, values))
            for name, (_, values) in tensors.items()
            if name.endswith("_accum") or name == "scores"
        },
    }
    return tensors, report


def header(heads, length, fixtures):
    yield "// Generated by scripts/gpt2_data.py. Do not edit.\n"
    yield "#ifndef GPT2_DATA_H\n#define GPT2_DATA_H\n#include <stdint.h>\n"
    yield f"enum {{ BLOCK_N = {length}, BLOCK_D = 64, BLOCK_HEADS = {heads}, BLOCK_HIDDEN = {heads * 64}, BLOCK_FF = {heads * 256}, BLOCK_TILE_ROWS = 16, BLOCK_RUNS = {len(fixtures)} }};\n"
    yield "enum { BLOCK_Q4_SCALE = 16, BLOCK_Q3_SCALE = 8, BLOCK_WEIGHT_SCALE = 128, BLOCK_LN_SCALE = 64, BLOCK_LN_EPSILON_Q12 = 4096, BLOCK_PV_DIV = 127 };\n"
    for name, divisor in bm.DIVISORS.items():
        yield f"enum {{ BLOCK_{name.upper()}_DIV = {divisor} }};\n"
    static = {f"{prefix}{name}" for prefix in ("w", "b") for name in NAMES.values()}
    static.update(("gamma1", "beta1", "gamma2", "beta2"))
    for name, (kind, values) in fixtures[0].items():
        dimension = "" if name in static else f"[{len(fixtures)}]"
        yield f"static const {kind} block_{name}{dimension}[{len(values)}] __attribute__((aligned(32))) = {{\n"
        rows = [values] if name in static else [item[name][1] for item in fixtures]
        for row in rows:
            if name not in static:
                yield " {\n"
            for start in range(0, len(row), 32):
                yield "  " + ", ".join(map(str, row[start : start + 32])) + ",\n"
            if name not in static:
                yield " },\n"
        yield "};\n"
    yield "#endif\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--heads", type=int, choices=(2, 12), default=2)
    parser.add_argument("--length", type=int, choices=(16, 64, 128), default=128)
    parser.add_argument("--seed", type=int, default=0)
    parser.add_argument("--runs", type=int, choices=(1, 2), default=2)
    parser.add_argument(
        "--output", type=Path, default=ROOT / "tests/generated/gpt2_data.h"
    )
    parser.add_argument(
        "--report", type=Path, help="Write float error and clipping as JSON"
    )
    args = parser.parse_args()
    fixtures, reports = [], []
    for run in range(args.runs):
        tensors, report = reference(args.heads, args.length, args.seed, run)
        fixtures.append(tensors)
        reports.append(report)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as stream:
        stream.writelines(header(args.heads, args.length, fixtures))
    text = json.dumps(reports, indent=2)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(text + "\n", encoding="utf-8")
    print(text)
    print(f"Generated {args.output}")


if __name__ == "__main__":
    main()
