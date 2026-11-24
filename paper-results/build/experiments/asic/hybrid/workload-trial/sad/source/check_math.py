"""Arithmetic identity checks; this does not simulate RTL or establish throughput."""
import json
from pathlib import Path
import random
import sys

WORD = (1 << 32) - 1


def reference(a, b, mask):
    total = 0
    for i, (left, right) in enumerate(zip(a, b)):
        if (mask >> i) & 1:
            difference = (right - left) & WORD
            signed = difference - (1 << 32) if difference & (1 << 31) else difference
            total += abs(signed)
    return total & WORD


def scheduled(a, b, mask):
    values = [(right - left) & WORD if (mask >> i) & 1 else 0 for i, (left, right) in enumerate(zip(a, b))]
    values = [(-value) & WORD if value >> 31 else value for value in values]
    while len(values) != 1:
        values = [(values[i] + values[i + 1]) & WORD for i in range(0, len(values), 2)]
    return values[0]


def main():
    rng = random.Random(3216)
    edges = [0, 1, 0x7fffffff, 0x80000000, 0xffffffff]
    cases = []
    for left in edges:
        for right in edges:
            for mask in [0, 1, 0x00ff, 0xffff, 0xaaaa]:
                cases.append(([left] * 16, [right] * 16, mask))
    cases += [([rng.getrandbits(32) for _ in range(16)], [rng.getrandbits(32) for _ in range(16)], rng.getrandbits(16)) for _ in range(4096)]
    cases.append((list(range(1, 17)), list(range(4, 20)), 0xff))
    for number, (a, b, mask) in enumerate(cases):
        if scheduled(a, b, mask) != reference(a, b, mask):
            print(f"Arithmetic mismatch in case {number}")
            return 1
    expected_fixture = scheduled(list(range(1, 17)), list(range(4, 20)), 0xff)
    if expected_fixture != 24:
        return 1
    result = {"passed": True, "cases": len(cases), "fixture_result": expected_fixture, "semantics": "B-A wraps to 32 bits before signed absolute value; all sums wrap to 32 bits", "limit": "Software arithmetic identities only; no RTL or workload simulation"}
    Path(__file__).with_name("arithmetic.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
