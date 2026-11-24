"""Check pipeline arithmetic independently of simulator and workload execution."""

import json
import random
from pathlib import Path


MASK16 = (1 << 16) - 1
MASK32 = (1 << 32) - 1


def square(coefficient, delta):
    low = (coefficient & MASK16) * (delta & MASK16)
    cross_a = ((coefficient >> 16) * (delta & MASK16)) & MASK16
    cross_b = ((coefficient & MASK16) * (delta >> 16)) & MASK16
    upper = ((low >> 16) + cross_a) & MASK16
    return (((upper + cross_b) & MASK16) << 16) | (low & MASK16)


def bias(product):
    low = (product & MASK16) + 4096
    upper = ((product >> 16) + (low >> 16)) & MASK16
    return (upper << 16) | (low & MASK16)


def main():
    values = {0, 1, MASK32, 0x80000000, 0x7fffffff, 4095, 4096, 65535, 65536, 0xfffff000}
    for bit in range(32):
        values.update({1 << bit, ((1 << bit) - 1) & MASK32, (-(1 << bit)) & MASK32})
    pairs = [(left, right) for left in sorted(values) for right in sorted(values)]
    rng = random.Random(0x475054)
    pairs.extend((rng.getrandbits(32), rng.getrandbits(32)) for _ in range(65536))
    for coefficient, delta in pairs:
        signed_coefficient = coefficient if coefficient < 1 << 31 else coefficient - (1 << 32)
        signed_delta = delta if delta < 1 << 31 else delta - (1 << 32)
        if square(coefficient, delta) != (signed_coefficient * signed_delta) & MASK32:
            return f"Square mismatch for {coefficient}, {delta}"
        if bias(coefficient) != (coefficient + 4096) & MASK32:
            return f"Bias mismatch for {coefficient}"
    # Expand (a0 + 2^16*a1)*(b0 + 2^16*b1) as a polynomial modulo 2^32.
    expanded = {(i, j): (1 << (16 * (i + j))) & MASK32 for i in range(2) for j in range(2)}
    implemented = {(0, 0): 1, (0, 1): 1 << 16, (1, 0): 1 << 16, (1, 1): 0}
    if expanded != implemented:
        return "Partial-product polynomial identity failed"
    result = {
        "arithmetic_pairs_checked": len(pairs),
        "random_seed": "0x475054",
        "all_checks_passed": True,
        "square_identity": "For B=2^16, (a0+B*a1)(b0+B*b1) mod B^2 = a0*b0+B*((a1*b0+a0*b1) mod B) mod B^2. Splitting a0*b0 into high/low words and truncating each high-word addition modulo B preserves this identity. Signed and unsigned 32-bit products have the same low32 bits.",
        "polynomial_coefficients_equal_modulo_2_32": expanded == implemented,
        "limits": "Arithmetic identity and deterministic software checks cover combinational expressions; this is not whole-FSM formal equivalence or RTL simulation. Busy/done preservation relies on unchanged FSM actions outside added pipeline states.",
    }
    (Path(__file__).resolve().parent / "arithmetic.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    import sys

    sys.exit(main())
