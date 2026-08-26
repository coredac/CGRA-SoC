# CGRA-SoC

CGRA-SoC is the top-level integration and runtime repository for VectorCGRA, OpenFPGA, Gemmini, and Chipyard.

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

We follow LLVM style with LF line endings for C sources and use Black for the Python utilities under `scripts/`. The format configurations are in `.clang-format` and `pyproject.toml` at the repository root.

```shell
$ git ls-files -z '*.c' '*.h' | xargs -0 -r clang-format-18 -i
$ black scripts
```

Generated files are excluded by `.clang-format-ignore`. VectorCGRA, Chipyard, and OpenFPGA keep their own formatting rules.

## Supported Flows

The repository supports standalone CGRA systems and integrations with OpenFPGA and Gemmini at different levels of completeness.

### CGRA

The CGRA is attached to the processor through RoCC. Single- and multi-CGRA systems share the same Chipyard test runner.

#### Single CGRA

- Supported: FIR, ReLU, GEMV, Histogram, AXPY
- Unsupported: GEMM, SAD

Generate the RTL with `scripts/generate_single_cgra.py` and the kernel API with `scripts/cgra_fast_api.py`, then run:

```shell
$ ./run-chipyard-cgra-test.sh --rebuild <test-name>
```

The rebuild flag can be omitted when the generated RTL is unchanged.

#### Multi-CGRA

- Supported: homogeneous mesh, 2x2 and 4x4 systolic arrays, scalar FIR, vector FIR
- Unsupported: automatic multi-CGRA control-packet generation

Generate the RTL with `scripts/generate_multi_cgra.py`, then run:

```shell
$ ./run-chipyard-cgra-test.sh --rebuild <test-name>
```

### CGRA + FPGA

The current FPGA flow generates an OpenFPGA fabric and exposes it as a TileLink MMIO peripheral.

- Supported: fabric generation, runtime bitstream programming, and MMIO I/O
- Supported demos: AND2, AND2/OR2, bin2bcd, gcd6
- Unsupported: a system configuration that enables CGRA and FPGA together

Generate the selected fabric with `scripts/openfpga/generate.py`, then run:

```shell
$ CONFIG=<chipyard-config> ./run-chipyard-openfpga-demo.sh --rebuild <test-name>
```

### CGRA + Gemmini

The combined configuration runs Gemmini and CGRA in the same Chipyard system.

- Supported: CPU-controlled Gemmini GEMM followed by CGRA ReLU through DRAM
- Supported: CPU-controlled Gemmini external SPM to CGRA SPM transfer followed by CGRA ReLU
- Supported: automatic 128-byte Gemmini external SPM to CGRA SPM transfer followed by CGRA ReLU
- Unsupported: runtime programming of AutoLink routes and copy descriptors

The automatic path uses an elaboration-time task table. Its control protocol carries dependency and completion messages, while the SoC memory interconnect carries payload data. See [docs/accelerator-modes.md](./docs/accelerator-modes.md) for the Manual and Automatic mode contracts.

Generate the single-CGRA ReLU RTL and API, then run the automatic SPM transfer:

```shell
$ ./run-chipyard-cgra-gemmini-demo.sh --rebuild
```

Run the CPU-controlled DRAM path with:

```shell
$ CONFIG=CGRAMinimalGemminiRocketConfig ./run-chipyard-cgra-gemmini-demo.sh --rebuild tests/cgra-gemmini/relu_dma.c
```

Run the CPU-controlled External-SPM path with:

```shell
$ CONFIG=CGRAMinimalGemminiRocketConfig ./run-chipyard-cgra-gemmini-demo.sh --rebuild tests/cgra-gemmini/relu_spm_manual.c
```
