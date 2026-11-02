#!/usr/bin/env python3
"""Generate the existing synthetic GPT-2 block as a native-kernel DAG."""

import argparse
import json
from pathlib import Path

import block_math as bm
import gpt2_data as data
from kernel_graph import Graph


def validate(model, dma_bytes):
    if dma_bytes < 4 or dma_bytes & (dma_bytes - 1):
        return "DMA alignment must be a power of two, at least four bytes"
    if type(model["length"]) is not int or model["length"] <= 0:
        return "length must be a positive integer"
    if type(model["heads"]) is not int or model["heads"] <= 0:
        return "heads must be a positive integer"
    if type(model.get("head_dim", 64)) is not int or model.get("head_dim", 64) != 64:
        return "the existing native reference supports head_dim = 64"
    if type(model["seed"]) is not int:
        return "seed must be an integer"
    return None


def workload(model):
    graph = Graph("gpt2")
    head_dim = model.get("head_dim", 64)
    rows, hidden = model["length"], model["heads"] * head_dim
    expanded = hidden * 4
    source = graph.tensor("x", rows, hidden, 8)

    def requant(name, source, divisor, result=None):
        if result is None:
            result = graph.tensor(name, source.rows, source.columns, 8)
        graph.call(
            name, "block_requant", [source], [result], {"divisor": divisor}, narrow=True
        )
        return result

    def norm(stage, source):
        name = f"ln{stage}"
        gamma = graph.tensor(f"gamma{stage}", 1, hidden, 32)
        beta = graph.tensor(f"beta{stage}", 1, hidden, 32)
        total = graph.tensor(f"{name}_sum", rows, 1, 32)
        graph.call(
            f"{name}_sum", "block_ln_sum", [source], [total], {"columns": hidden}
        )
        mean = graph.tensor(f"{name}_mean", rows, 1, 32)
        graph.call(
            f"{name}_mean", "block_ln_mean", [total], [mean], {"columns": hidden}
        )
        squares = graph.tensor(f"{name}_squares", rows, 1, 32)
        graph.call(
            f"{name}_squares",
            "block_ln_squares",
            [source, mean],
            [squares],
            {"columns": hidden},
        )
        variance = graph.tensor(f"{name}_variance", rows, 1, 32)
        graph.call(
            f"{name}_variance",
            "block_ln_variance",
            [squares],
            [variance],
            {"columns": hidden},
        )
        std = graph.tensor(f"{name}_std", rows, 1, 32)
        graph.call(f"{name}_sqrt", "block_ln_sqrt", [variance], [std])
        affine = graph.tensor(f"{name}_affine", rows, hidden, 32)
        graph.call(
            f"{name}_norm",
            "block_ln_norm",
            [source, gamma, beta, mean, std],
            [affine],
            {"columns": hidden},
        )
        return requant(name, affine, 4)

    def matrix(name, source, weight, bias, inner, columns, transpose_b=0):
        result = graph.tensor(name, source.rows, columns, 32)
        graph.call(
            name,
            "gemmini_matrix",
            [source, weight, bias],
            [result],
            {
                "m": source.rows,
                "k": inner,
                "n": columns,
                "transpose_a": 0,
                "transpose_b": transpose_b,
            },
        )
        return result

    def linear(name, source, columns):
        suffix = data.NAMES[name]
        weight = graph.tensor(f"w{suffix}", source.columns, columns, 8)
        bias = graph.tensor(f"b{suffix}", 1, columns, 32)
        accum = matrix(f"{name}_accum", source, weight, bias, source.columns, columns)
        return requant(name, accum, bm.DIVISORS[name])

    def residual(name, left, right):
        result = graph.tensor(name, rows, hidden, 8)
        graph.call(name, "block_residual", [left, right], [result], narrow=True)
        return result

    normalized = norm(1, source)
    query, key, value = [linear(name, normalized, hidden) for name in ("q", "k", "v")]
    score_bias = graph.tensor("score_bias", 1, rows, 32)
    pv_bias = graph.tensor("pv_bias", 1, head_dim, 32)
    concat = graph.tensor("concat", rows, hidden, 8)
    slices = []
    for head in range(model["heads"]):
        prefix = f"h{head}"
        q, k, v = [
            graph.view(
                f"{prefix}_{tensor.name}",
                tensor,
                0,
                head * head_dim,
                rows,
                head_dim,
            )
            for tensor in (query, key, value)
        ]
        scores = matrix(
            f"{prefix}_scores", q, k, score_bias, head_dim, rows, transpose_b=1
        )
        maximum = graph.tensor(f"{prefix}_maximum", rows, 1, 32)
        graph.call(
            f"{prefix}_maximum",
            "attention_max",
            [scores],
            [maximum],
            {"columns": rows, "row": 0},
        )
        exponent = graph.tensor(f"{prefix}_exponent", rows, rows, 32)
        total = graph.tensor(f"{prefix}_sum", rows, 1, 32)
        graph.call(
            f"{prefix}_exponent",
            "attention_exp",
            [scores, maximum],
            [exponent, total],
            {"columns": rows, "row": 0},
        )
        probability = graph.tensor(f"{prefix}_probability", rows, rows, 8)
        graph.call(
            f"{prefix}_probability",
            "attention_norm",
            [exponent, total],
            [probability],
            {"columns": rows},
            narrow=True,
        )
        pv = matrix(f"{prefix}_pv", probability, v, pv_bias, rows, head_dim)
        output = graph.view(
            f"{prefix}_concat", concat, 0, head * head_dim, rows, head_dim
        )
        requant(f"{prefix}_requant", pv, 127, output)
        slices.append(output)
    graph.join("concat", concat, slices)
    attention = linear("attention", concat, hidden)
    skip = residual("u", source, attention)
    normalized = norm(2, skip)
    fc1 = linear("fc1", normalized, expanded)
    gelu = graph.tensor("gelu", rows, expanded, 8)
    graph.call("gelu", "block_activation", [fc1], [gelu], narrow=True)
    fc2 = linear("fc2", gelu, hidden)
    output = residual("y", skip, fc2)
    return graph, output


def samples(model):
    references = []
    for sample in range(2):
        tensors, _ = data.reference(
            model["heads"], model["length"], model["seed"], sample
        )
        tensors["score_bias"] = ("int32_t", [0] * model["length"])
        tensors["pv_bias"] = ("int32_t", [0] * model.get("head_dim", 64))
        references.append(tensors)
    return references


def header(graph, output, references, dma_bytes):
    elements = output.rows * output.columns
    padded = (elements + dma_bytes - 1) // dma_bytes * dma_bytes
    text = [
        "/* Generated by scripts/gpt2_plan.py. Do not edit. */",
        "#ifndef RUNTIME_GPT2_DATA_H",
        "#define RUNTIME_GPT2_DATA_H",
        "#include <stdint.h>",
        f"enum {{ GPT2_SAMPLES = {len(references)}, GPT2_ARGUMENTS = {len(graph.tensors)}, GPT2_OUTPUTS = {elements}, GPT2_OUTPUT_BYTES = {padded}, GPT2_DMA_BYTES = {dma_bytes} }};",
    ]
    for tensor in graph.tensors:
        if tensor.state:
            continue
        count = tensor.rows * tensor.columns
        aligned = (
            (count * tensor.bits // 8 + dma_bytes - 1)
            // dma_bytes
            * dma_bytes
            * 8
            // tensor.bits
        )
        dynamic = tensor.name == "x"
        shape = f"[GPT2_SAMPLES][{aligned}]" if dynamic else f"[{aligned}]"
        text.append(
            f"static const int{tensor.bits}_t gpt2_{tensor.name}{shape} __attribute__((aligned({dma_bytes}))) = {{"
        )
        for reference in references if dynamic else references[:1]:
            values = list(reference[tensor.name][1]) + [0] * (aligned - count)
            if dynamic:
                text.append(" {")
            for start in range(0, len(values), 32):
                text.append(
                    "  " + ", ".join(map(str, values[start : start + 32])) + ","
                )
            if dynamic:
                text.append(" },")
        text.append("};")
    text.append("static const int8_t gpt2_expected[GPT2_SAMPLES][GPT2_OUTPUTS] = {")
    for reference in references:
        text.append(" {" + ", ".join(map(str, reference["expected_y"][1])) + "},")
    text.extend(
        [
            "};",
            "static void gpt2_arguments(uintptr_t *arguments, unsigned sample, int8_t *output) {",
        ]
    )
    for index, tensor in enumerate(graph.tensors):
        pointer = "0"
        if not tensor.state:
            pointer = f"(uintptr_t)gpt2_{tensor.name}" + (
                "[sample]" if tensor.name == "x" else ""
            )
        elif tensor is output:
            pointer = "(uintptr_t)output"
        text.append(f"  arguments[{index}] = {pointer};")
    text.extend(["}", "#endif", ""])
    return "\n".join(text)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("model", type=Path)
    parser.add_argument("--dma-bytes", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--header", type=Path, required=True)
    args = parser.parse_args()
    try:
        model = json.loads(args.model.read_text())
        error = validate(model, args.dma_bytes)
        if error:
            parser.error(error)
        graph, output = workload(model)
        references = samples(model)
        args.output.write_text(graph.emit([output]))
        args.header.write_text(header(graph, output, references, args.dma_bytes))
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
