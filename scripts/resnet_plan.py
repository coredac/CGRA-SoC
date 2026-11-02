#!/usr/bin/env python3
"""Generate a complete synthetic ResNet-8 native-kernel DAG and two references."""

import argparse
import json
from pathlib import Path

from kernel_graph import Graph
import resnet_data as data


def validate(model, dma_bytes):
    if dma_bytes < 4 or dma_bytes & (dma_bytes - 1):
        return "DMA alignment must be a power of two, at least four bytes"
    for name in ("rows", "columns", "inputs", "classes"):
        if type(model[name]) is not int or model[name] <= 0:
            return f"{name} must be a positive integer"
    channels = model["channels"]
    if not isinstance(channels, list) or len(channels) != 3:
        return "ResNet-8 requires three residual-block channel widths"
    if any(type(value) is not int or value <= 0 for value in channels):
        return "channel widths must be positive integers"
    if model["classes"] != 10:
        return "the existing noncausal Softmax kernels require ten classes"
    return None


def workload(model):
    graph = Graph("resnet")
    rows, columns, inputs = model["rows"], model["columns"], model["inputs"]
    source = graph.tensor("input", rows * columns, inputs, 8)

    def conv(name, tensor, height, width, channels, outputs, kernel, stride, weights):
        out_rows, out_columns = (height + stride - 1) // stride, (
            width + stride - 1
        ) // stride
        weight = graph.tensor(weights, kernel * kernel * channels, outputs, 8)
        result = graph.tensor(name, out_rows * out_columns, outputs, 8)
        graph.call(
            name,
            "gemmini_conv_i8",
            [tensor, weight],
            [result],
            parameters={
                "input_rows": height,
                "input_columns": width,
                "input_channels": channels,
                "output_rows": out_rows,
                "output_columns": out_columns,
                "output_channels": outputs,
                "kernel": kernel,
                "stride": stride,
                "dilation": 1,
                "padding_top": max((out_rows - 1) * stride + kernel - height, 0) // 2,
                "padding_left": max((out_columns - 1) * stride + kernel - width, 0)
                // 2,
            },
            narrow=True,
        )
        return result, out_rows, out_columns

    def relu(name, tensor):
        result = graph.tensor(name, tensor.rows, tensor.columns, 8)
        graph.call(name, "relu_runtime", [tensor], [result], narrow=True)
        return result

    stem, _, _ = conv(
        "stem_conv",
        source,
        rows,
        columns,
        inputs,
        model["channels"][0],
        data.KERNEL,
        1,
        "stem_weights",
    )
    source = relu("stem_relu", stem)
    inputs = model["channels"][0]
    for index, outputs in enumerate(model["channels"]):
        stride = 1 if index == 0 else 2
        first, out_rows, out_columns = conv(
            f"block{index}_conv1",
            source,
            rows,
            columns,
            inputs,
            outputs,
            data.KERNEL,
            stride,
            f"weights1_{index}",
        )
        first = relu(f"block{index}_relu", first)
        main, _, _ = conv(
            f"block{index}_conv2",
            first,
            out_rows,
            out_columns,
            outputs,
            outputs,
            data.KERNEL,
            1,
            f"weights2_{index}",
        )
        skip = source
        if index:
            skip, _, _ = conv(
                f"block{index}_projection",
                source,
                rows,
                columns,
                inputs,
                outputs,
                1,
                stride,
                f"projection_{index}",
            )
        result = graph.tensor(
            f"block{index}_output", out_rows * out_columns, outputs, 8
        )
        graph.call(
            f"block{index}_add", "add_relu_runtime", [main, skip], [result], narrow=True
        )
        source = result
        rows, columns, inputs = out_rows, out_columns, outputs
    pooled = graph.tensor("pool", 1, inputs, 8)
    graph.call(
        "pool",
        "pool_average",
        [source],
        [pooled],
        parameters={"rows": rows, "columns": columns, "channels": inputs},
        narrow=True,
    )
    classes = model["classes"]
    weight = graph.tensor("fc_weights", inputs, classes, 8)
    bias = graph.tensor("fc_bias", 1, classes, 32)
    logits = graph.tensor("logits", 1, classes, 32)
    graph.call(
        "fc",
        "gemmini_matrix",
        [pooled, weight, bias],
        [logits],
        parameters={
            "m": 1,
            "k": inputs,
            "n": classes,
            "transpose_a": 0,
            "transpose_b": 0,
        },
    )
    maximum = graph.tensor("maximum", 1, 1, 32)
    graph.call("maximum", "softmax_max", [logits], [maximum])
    exponent = graph.tensor("exponent", 1, classes, 32)
    total = graph.tensor("sum", 1, 1, 32)
    graph.call("exponent", "softmax_exp", [logits, maximum], [exponent, total])
    output = graph.tensor("probabilities", 1, classes, 32)
    graph.call("normalize", "softmax_norm", [exponent, total], [output])
    return graph, output


def samples(model):
    result = []
    for sample in range(2):
        _, tensors = data.reference(
            model["rows"],
            model["columns"],
            model["inputs"],
            model["channels"],
            model["classes"],
            sample,
        )
        tensors["input"] = data.input_data(
            model["rows"], model["columns"], model["inputs"], sample
        )
        result.append(tensors)
    return result


def header(graph, output, references, dma_bytes):
    text = [
        "/* Generated by scripts/resnet_plan.py. Do not edit. */",
        "#ifndef RUNTIME_RESNET_DATA_H",
        "#define RUNTIME_RESNET_DATA_H",
        "#include <stdint.h>",
        f"enum {{ RESNET_ALIGNMENT = {dma_bytes}, RESNET_SAMPLES = {len(references)}, RESNET_ARGUMENTS = {len(graph.tensors)}, RESNET_OUTPUTS = {output.columns}, RESNET_OUTPUT_WORDS = {(output.columns * 4 + dma_bytes - 1) // dma_bytes * dma_bytes // 4} }};",
    ]
    for tensor in graph.tensors:
        if tensor.state:
            continue
        elements = tensor.rows * tensor.columns
        aligned = (
            (elements * tensor.bits // 8 + dma_bytes - 1)
            // dma_bytes
            * dma_bytes
            * 8
            // tensor.bits
        )
        text.append(
            f"static const int{tensor.bits}_t resnet_{tensor.name}[RESNET_SAMPLES][{aligned}] __attribute__((aligned({dma_bytes}))) = {{"
        )
        for reference in references:
            values = reference[tensor.name] + [0] * (aligned - elements)
            text.append("  {" + ", ".join(map(str, values)) + "},")
        text.append("};")
    text.append(
        "static const int32_t resnet_expected[RESNET_SAMPLES][RESNET_OUTPUTS] = {"
    )
    for reference in references:
        text.append(
            "  {" + ", ".join(map(str, reference["expected_probabilities"])) + "},"
        )
    text.extend(
        [
            "};",
            "static void resnet_arguments(uintptr_t *arguments, unsigned sample, int32_t *output) {",
        ]
    )
    for index, tensor in enumerate(graph.tensors):
        pointer = "0"
        if not tensor.state:
            pointer = f"(uintptr_t)resnet_{tensor.name}[sample]"
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
