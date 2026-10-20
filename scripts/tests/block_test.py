"""Protect fixed-point interfaces and the sparse fixture's dense GEMM semantics."""

import math
import random
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import gpt2_data as data
import block_math as bm


class BlockTest(unittest.TestCase):
    def test_rounding_and_saturation(self):
        values = [-515, -514, -512, -510, -2, -1, 0, 1, 2, 506, 508, 510]
        actual, count = bm.quantize([values], 4)
        expected = [[-128, -128, -128, -128, -1, 0, 0, 0, 1, 127, 127, 127]]
        if actual != expected or count != 3:
            self.fail(f"INT8 rounding/saturation: {actual}, clipped={count}")

    def test_dense_chunked_projection(self):
        rng = random.Random(9)
        inputs, outputs = 3072, 16
        dense, taps = data.weight(rng, inputs, outputs)
        rows = [[rng.randrange(-128, 128) for _ in range(inputs)] for _ in range(3)]
        bias = [rng.randrange(-100, 101) for _ in range(outputs)]
        actual = bm.project(rows, taps, bias)
        for row, result in zip(rows, actual):
            for column, value in enumerate(result):
                expected = bias[column]
                for start in range(0, inputs, 768):
                    expected += sum(
                        row[k] * dense[k * outputs + column]
                        for k in range(start, start + 768)
                    )
                    if not any(
                        dense[k * outputs + column] for k in range(start, start + 768)
                    ):
                        self.fail("Synthetic FC2 does not exercise every K chunk")
                if value != expected:
                    self.fail(f"Dense/chunked mismatch: {value} != {expected}")

    def test_layernorm_extreme_and_constant(self):
        rows = [[-128, 127] * 384, [-37] * 768]
        gamma, beta = [64] * 768, [8] * 768
        output, clipped, stats = bm.layernorm(rows, gamma, beta)
        if stats[0][2] != 3196108800 or stats[0][4] != math.isqrt(66589696):
            self.fail(f"Extreme statistics changed: {stats[0]}")
        if stats[1][4] != 64 or output[1] != [2] * 768 or clipped:
            self.fail("Constant LayerNorm must produce beta with nonzero epsilon")
        if any(not 0 <= row[2] < 2**32 for row in stats):
            self.fail("LayerNorm squares do not fit uint32")

    def test_gelu_interface(self):
        source = [list(range(-128, 128))]
        output, clipped = bm.gelu(source)
        floating = bm.float_gelu([[value / 16 for value in source[0]]])
        metrics = bm.error(output, floating)
        if clipped or metrics["max_abs"] > 0.065:
            self.fail(f"GELU Q4 domain error: {metrics}, clipped={clipped}")

    def test_run_data_and_scales(self):
        first, report = data.reference(heads=2, length=8, seed=3, run=0)
        second, _ = data.reference(heads=2, length=8, seed=3, run=1)
        if first["w2"] != second["w2"] or first["x"] == second["x"]:
            self.fail("Repeated runs must change inputs but retain weights")
        for residual, left, right in (
            ("u", "x", "expected_attention"),
            ("y", "expected_u", "expected_fc2"),
        ):
            expected, _ = bm.quantize(
                [[a + b for a, b in zip(first[left][1], first[right][1])]]
            )
            if first[f"expected_{residual}"][1] != expected[0]:
                self.fail("Residual branches are not in the same Q4 scale")
        if report["epsilon"] != 1 / 256 or not math.isfinite(
            report["float_error"]["y"]["rmse"]
        ):
            self.fail("Invalid floating reference metrics")


if __name__ == "__main__":
    unittest.main()
