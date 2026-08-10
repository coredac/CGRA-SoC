# CGRA-SoC

CGRA-SoC integrates VectorCGRA, OpenFPGA, Gemmini, and Chipyard for
accelerator generation and bare-metal simulation.

## Contents

- [Setup](#setup)
- [Repository Layout](#repository-layout)
- [Code Format Style](#code-format-style)
- [Supported Flows](#supported-flows)
  - [CGRA](#cgra)
  - [CGRA + FPGA](#cgra--fpga)
  - [CGRA + Gemmini](#cgra--gemmini)

## Setup

See [docs/Setup.md](./docs/Setup.md).

## Repository Layout

- `configs/`
- `scripts/`
- `tests/`
- `docs/`
- `VectorCGRA/`
- `OpenFPGA/`
- `chipyard/`

## Code Format Style

We follow LLVM style with LF line endings for C sources and use Black for the
Python utilities under `scripts/`. The format configurations are in
`.clang-format` and `pyproject.toml` at the repository root.

```shell
$ git ls-files -z '*.c' '*.h' | xargs -0 -r clang-format-18 -i
$ black scripts
```

Generated files are excluded by `.clang-format-ignore`. VectorCGRA, Chipyard,
and OpenFPGA keep their own formatting rules.

## Supported Flows

The repository supports standalone CGRA systems and integrations with OpenFPGA
and Gemmini at different levels of completeness.

### CGRA

The CGRA is attached to the processor through RoCC. Single- and multi-CGRA
systems share the same Chipyard test runner.

#### Single CGRA

- Supported: FIR, ReLU
- Supported with known limitations: GEMV, Histogram, AXPY
- Unsupported: GEMM, SAD

Generate the RTL with `scripts/generate_single_cgra.py` and the kernel API with
`scripts/cgra_fast_api.py`, then run:

```shell
$ ./run-chipyard-cgra-test.sh --rebuild <test-name>
```

The rebuild flag can be omitted when the generated RTL is unchanged.

#### Multi-CGRA

- Supported: homogeneous mesh, 2x2 and 4x4 systolic arrays, scalar FIR, vector
  FIR
- Unsupported: automatic multi-CGRA control-packet generation

Generate the RTL with `scripts/generate_multi_cgra.py`, then run:

```shell
$ ./run-chipyard-cgra-test.sh --rebuild <test-name>
```

### CGRA + FPGA

The current FPGA flow generates an OpenFPGA fabric and exposes it as a
TileLink MMIO peripheral.

- Supported: fabric generation, runtime bitstream programming, and MMIO I/O
- Supported demos: AND2, AND2/OR2, bin2bcd, gcd6
- Unsupported: a system configuration that enables CGRA and FPGA together

Generate the selected fabric with `scripts/openfpga/generate.py`, then run:

```shell
$ CONFIG=<chipyard-config> ./run-chipyard-openfpga-demo.sh --rebuild <test-name>
```

### CGRA + Gemmini

The combined configuration runs Gemmini and CGRA in the same Chipyard system.

- Supported: Gemmini GEMM followed by CGRA ReLU
- Supported: CGRA DMA ReLU chunk-0 smoke test

Generate the single-CGRA ReLU RTL and API, then run:

```shell
$ ./run-chipyard-cgra-gemmini-demo.sh --rebuild [test-source.c]
```
