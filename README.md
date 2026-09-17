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

Generated headers expose kernel descriptors such as `RELU4X4`. After hardware reset, manual execution uses `cgra_config(&RELU4X4, CGRA_COLD)` followed by `cgra_start(&RELU4X4)` after input DMA completes. Repeat the same resident kernel with `cgra_prepare(&RELU4X4, CGRA_REPEAT)`. Use `CGRA_SWITCH` when another kernel has executed, including the first use of a newly loaded kernel: `cgra_config` loads its static configuration, while `cgra_prepare` reuses an already resident configuration. Manage DMA and completion separately.

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

The combined flow runs Gemmini and CGRA in the same Chipyard system and can also instantiate AES or Pool.

- Supported: CPU-controlled Gemmini GEMM followed by CGRA ReLU through DRAM
- Supported: CPU-controlled Gemmini external SPM to CGRA SPM transfer followed by CGRA ReLU
- Supported: automatic 128-byte Gemmini external SPM to CGRA SPM transfer followed by CGRA ReLU
- Supported: CPU-controlled and automatic 128-byte Gemmini to CGRA to AES pipelines
- Supported: tiled Conv → ReLU → Pool with cached configuration and cross-IP overlap
- Supported: non-tiled residual blocks that reuse two Gemmini jobs and two resident CGRA kernels
- Supported: runtime tile shapes, transfer offsets and buffer-slot strides within the generated graph
- Supported: CGRA input-copy/compute overlap using separate SPM slots
- Unsupported: runtime graph changes and concurrent kernels on one IP

In the three-stage AES path, Gemmini publishes to its external SPM, CGRA pulls the data and computes into its local SPM, and AES reads that SPM directly before writing ciphertext to DRAM. That demo remains sequential with one 128-byte chunk. AutoLink carries control and TileLink carries payload; tiled CNN demos reuse cached jobs across multiple chunks. See [hardware contracts](./docs/contracts.md) for supported interfaces.

For automatic CGRA execution, call `cgra_job_config(job, &RELU4X4, NULL)` before releasing dependencies. This loads static controls and captures runtime initialization plus launch packets; it does not start computation. Tiled kernels pass their symbol bindings instead of `NULL`. AutoLink starts the configured job when its inputs are ready, and hardware applies the configured field updates on each tile without per-tile CPU configuration.

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

Run the CPU-controlled three-stage path with:

```shell
$ CONFIG=CGRAMinimalGemminiAESRocketConfig TEST_SRC=tests/cgra-gemmini/relu_spm_aes_manual.c ./run-chipyard-cgra-gemmini-demo.sh --rebuild
```

Run the automatic three-stage path with:

```shell
$ CONFIG=CGRAMinimalGemminiAESAutoLinkRocketConfig TEST_SRC=tests/cgra-gemmini/relu_spm_aes_auto.c ./run-chipyard-cgra-gemmini-demo.sh --soc-yaml configs/soc/autolink/gca_short.yaml --rebuild
```

Use `--soc-yaml` to select the graph; changing YAML requires `--rebuild`. The AES configuration defaults to `gca.yaml` for the full AES → Gemmini → CGRA → AES path.

#### Tiled Conv → ReLU → Pool

`pool_tiles.c` runs a 4×6×3 input through a 3×3 convolution with eight output channels, ReLU, and 2×2 stride-2 MaxPool, producing a 2×3×8 output. One captured configuration runs both 1×2 and 2×1 output-tile partitions, including smaller boundary tiles. Gemmini and Pool use INT8; the CGRA wrapper expands input to INT32 and packs its INT32 output back to INT8.

Gemmini and CGRA each use two regions in their own SPM. AutoLink manages their lifetimes and can keep all three IPs active on different tiles. CGRA input DMA can also overlap its current computation in another slot. Pool streams from CGRA SPM and writes each tile directly into its final DRAM position; it has no publication SPM.

```shell
$ .venv/bin/python scripts/cgra_fast_api.py --arch-yaml configs/arch/arch.yaml --soc-yaml configs/soc/autolink/tiles.yaml configs/kernels/kernel_relu_runtime_4x4.yaml --output-dir tests/generated
$ CONFIG=CgraPoolTileRocketConfig TEST_SRC=tests/cgra-gemmini/pool_tiles.c LOADMEM=1 timeout_cycles=350000 ./run-chipyard-cgra-gemmini-demo.sh --soc-yaml configs/soc/autolink/tiles.yaml --rebuild
```

The cycle budget includes software reference computation and configuration. Printed `cycles` measure the CPU-observed pipeline interval; `overlap` and `peak` measure cross-IP task overlap, not arithmetic-unit utilization. CI runs this test alongside the existing manual, automatic, AES, Pool and non-tiled residual tests, while retaining separate CGRA and OpenFPGA jobs.

The tiled residual test (`residual_tiles.c`, `res_tiles.yaml`) is retained but currently deferred; repeated-IP tiled execution is outside the supported validation scope. It describes Conv1 → ReLU → Conv2 → Add+ReLU with a skip dependency, two tile partitions and preloaded skip data. Final output stays in separate per-tile SPM regions for CPU validation through the INT8 window after the run. Output checks skip each tile's first element because of [the known VectorCGRA store bug](https://github.com/coredac/CGRA-SoC/issues/3). The reported overlap counts active task intervals on different IPs and tiles, not arithmetic-unit utilization.
