"""Integer arithmetic for the synthetic GPT-2 block, separate from MHA fixtures.

Stage tensors use signed Q4 except Q/K/V/concat (Q3) and probabilities (1/127).
Weights are Q7; gamma and beta are Q6. Bias uses the corresponding accumulator
scale. LayerNorm epsilon is one squared input code, or 1/256 in real units.
This synthetic epsilon is not the pretrained GPT-2 epsilon.
"""

import math

from attention_data import probabilities

EPSILON = 1 / 256
DIVISORS = {"q": 256, "k": 256, "v": 256, "attention": 64, "fc1": 128, "fc2": 128}
SCALES = {"q": 8, "k": 8, "v": 8, "concat": 8}


def round_div(value, divisor):
    """Round to nearest, with ties away from zero."""
    magnitude = (abs(value) + divisor // 2) // divisor
    return -magnitude if value < 0 else magnitude


def trunc_div(value, divisor):
    magnitude = abs(value) // divisor
    return -magnitude if value < 0 else magnitude


def quantize(rows, divisor=1):
    rounded = [[round_div(value, divisor) for value in row] for row in rows]
    clipped = sum(value < -128 or value > 127 for row in rounded for value in row)
    return [[max(-128, min(127, value)) for value in row] for row in rounded], clipped


def project(rows, taps, bias):
    """Exact unbounded integer reference, using the fixture's nonzero weights."""
    return [
        [
            offset + sum(row[index] * weight for index, weight in column)
            for column, offset in zip(taps, bias)
        ]
        for row in rows
    ]


def float_project(rows, taps, bias, input_scale):
    """Floating path does not requantize intermediate tensors."""
    return [
        [
            offset / (input_scale * 128)
            + math.fsum(row[index] * (weight / 128) for index, weight in column)
            for column, offset in zip(taps, bias)
        ]
        for row in rows
    ]


def layernorm(rows, gamma, beta):
    """Return Q4 output, clipping count, and the six-phase kernel's statistics.

    Mean is Q6 in input-code units. Squares and variance are unsigned Q8;
    std is Q6. The inner signed divide truncates; output Q6 to Q4 rounds.
    At width 768 the squares bound is below 2**32, not below 2**31.
    """
    result, stats = [], []
    for row in rows:
        shifted_sum = sum(value + 128 for value in row)
        mean = sum(row) * 64 // len(row)
        squares = sum((value * 64 - mean) ** 2 // 16 for value in row)
        variance = squares // len(row)
        std = math.isqrt(16 * variance + 4096)
        result.append(
            [
                trunc_div((value * 64 - mean) * gain, std) + offset
                for value, gain, offset in zip(row, gamma, beta)
            ]
        )
        stats.append((shifted_sum, mean, squares, variance, std))
    output, clipped = quantize(result, 4)
    return output, clipped, stats


def float_layernorm(rows, gamma, beta):
    result = []
    for row in rows:
        mean = math.fsum(row) / len(row)
        variance = math.fsum((value - mean) ** 2 for value in row) / len(row)
        std = math.sqrt(variance + EPSILON)
        result.append(
            [
                (value - mean) / std * gain / 64 + offset / 64
                for value, gain, offset in zip(row, gamma, beta)
            ]
        )
    return result


def gelu_code(value):
    """The tested Q6 polynomial; callers bound its input to [-512,512]."""
    magnitude = abs(value)
    delta = min(magnitude, 160) - 160
    correction = (591 * delta * delta) >> 12
    gate = correction if value < 0 else 8192 - correction
    output = (magnitude * gate + 4096) >> 13
    return -output if value < 0 else output


def gelu(rows):
    # Every Q4 INT8 code expands into the tested Q6 interval [-512,508].
    return quantize([[gelu_code(value * 4) for value in row] for row in rows], 4)


def float_gelu(rows):
    return [
        [
            0.5
            * value
            * (1 + math.tanh(math.sqrt(2 / math.pi) * (value + 0.044715 * value**3)))
            for value in row
        ]
        for row in rows
    ]


def attention(query, key, value, heads):
    """Q3 inputs, existing causal integer softmax, and Q3 rounded PV output."""
    width = 64
    concat = [[] for _ in query]
    all_scores, all_probability, all_accum = [], [], []
    clipped = 0
    for head in range(heads):
        start, end = head * width, (head + 1) * width
        scores = [
            [sum(a * b for a, b in zip(q[start:end], k[start:end])) for k in key]
            for q in query
        ]
        probability = [probabilities(row, index) for index, row in enumerate(scores)]
        accum = [
            [
                sum(p * v[column] for p, v in zip(row, value))
                for column in range(start, end)
            ]
            for row in probability
        ]
        output, count = quantize(accum, 127)
        clipped += count
        for row, part in zip(concat, output):
            row.extend(part)
        all_scores.extend(scores)
        all_probability.extend(probability)
        all_accum.extend(accum)
    return concat, clipped, all_scores, all_probability, all_accum


def float_attention(query, key, value, heads):
    concat = [[] for _ in query]
    for head in range(heads):
        start, end = head * 64, (head + 1) * 64
        for index, q in enumerate(query):
            scores = [
                math.fsum(a * b for a, b in zip(q[start:end], k[start:end])) / 8
                for k in key[: index + 1]
            ]
            maximum = max(scores)
            exponents = [math.exp(score - maximum) for score in scores]
            total = math.fsum(exponents)
            concat[index].extend(
                math.fsum(p / total * v[column] for p, v in zip(exponents, value))
                for column in range(start, end)
            )
    return concat


def add(left, right):
    return [[a + b for a, b in zip(x, y)] for x, y in zip(left, right)]


def error(integer, floating, scale=16):
    delta = [a / scale - b for x, y in zip(integer, floating) for a, b in zip(x, y)]
    return {
        "max_abs": max(map(abs, delta)),
        "rmse": math.sqrt(math.fsum(value * value for value in delta) / len(delta)),
    }
