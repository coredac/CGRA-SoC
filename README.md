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
- Supported: two Gemmini, two independent CGRA instances and Pool through one shared CPU command interface
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

#### Multiple accelerator instances

`multi.yaml` names each instance separately. The generated descriptors select the target while retaining native IP APIs:

```c
accel_commands(GEMMINI0, {
  gemmini_mvin(input, 0);
});
```

Use separate, non-nested command blocks for different instances. MMIO configuration uses the matching descriptor's `.control` address. In this demo, CGRA0's MMIO endpoint collects all AutoLink results.

Both tests run eight tiles through `gemmini0 → cgra0 → gemmini1 → cgra1 → pool`. Each tile has two 16-element GEMM rows and produces eight MaxPool outputs. Gemmini1 reads CGRA0's packed INT8 SPM window with native `mvin`; CGRA1 exposes raw INT32 results to Pool. The second GEMM uses different weights. Manual mode runs tiles sequentially and checks each intermediate result; automatic mode reuses captured jobs, checks all final outputs and the last tile's intermediate results, and reports cross-IP overlap. Each Gemmini/CGRA has two 128-byte tile regions in its existing SPM, configured by `communication.buffer_slots`; Pool streams without a publication SPM. AutoLink assigns slots independently, and existing field updates relocate input, computation and publication addresses without CPU intervention between tiles. Both tests use the same relocatable ReLU kernel and hardware:

```shell
$ CONFIG=MultiAccelRocketConfig TEST_SRC=tests/cgra-gemmini/multi_manual.c ./run-chipyard-cgra-gemmini-demo.sh --rebuild
$ CONFIG=MultiAccelRocketConfig TEST_SRC=tests/cgra-gemmini/multi_auto.c ./run-chipyard-cgra-gemmini-demo.sh
```

Automatic `cycles` and `overlap` count fabric cycles; `peak` counts distinct active tile IDs, not arithmetic units. Both tests report `cpu_cycles`: Manual sums each tile's command-to-Pool-completion interval, while Auto measures input-ready through graph drain. Initial configuration, preloading and output verification are excluded.

#### Projected residual block

`block_manual.c` and `block_auto.c` use two Gemmini and two CGRA instances. Gemmini0 executes both the main 3×3 stride-2 Conv1 and the 1×1 stride-2 projection. CGRA0 applies ReLU, Gemmini1 executes 3×3 Conv2, and CGRA1 joins Conv2 with the projection for Add+ReLU. Automatic execution uses the existing ready-job round-robin arbitration, not a fixed software schedule.

The input is 32×32×16 and the output is 16×16×32. Nine 6×6 output tiles include clipped right and bottom edges; Conv1 and ReLU produce the halo needed by Conv2. Input and weights are preloaded into Gemmini SPM, all intermediate transfers stay on-chip, and all 8192 final INT32 values remain in CGRA1 SPM for CPU checking. Both Gemmini instances retain 64 KiB SPM and 32 KiB accumulator; each CGRA has 64 KiB SPM. Gemmini0 shares four publication slots between its two jobs; the other instances use two slots each. The configured copy capacities support up to 6×6 output tiles; changing the C tests' `TILE_H` and `TILE_W` to 4 reuses the same hardware.

```shell
$ chipyard/.conda-env/bin/python scripts/block_data.py
$ chipyard/.conda-env/bin/python scripts/cgra_fast_api.py --arch-yaml configs/arch/arch.yaml --soc-yaml configs/soc/autolink/block.yaml configs/kernels/kernel_relu_runtime_4x4.yaml configs/kernels/kernel_add_relu_runtime_4x4.yaml --output-dir tests/generated
$ CONFIG=MultiAccelRocketConfig TEST_SRC=tests/cgra-gemmini/block_manual.c LOADMEM=1 timeout_cycles=4000000 ./run-chipyard-cgra-gemmini-demo.sh --soc-yaml configs/soc/autolink/block.yaml --rebuild
$ CONFIG=MultiAccelRocketConfig TEST_SRC=tests/cgra-gemmini/block_auto.c LOADMEM=1 timeout_cycles=4000000 ./run-chipyard-cgra-gemmini-demo.sh --soc-yaml configs/soc/autolink/block.yaml
```

`block_data.py` computes reference values on the host and emits the tensor dimensions and expected output into `tests/generated/block_data.h`. The RTL tests still initialize the synthetic inputs, execute every accelerator stage and check every output. Use the generator's size arguments to change the workload within the configured memory capacities. Manual reports sequential per-tile CPU cycles. Auto reports CPU and fabric cycles, overlap cycles and peak distinct active tiles. Setup and final output checking are outside the reported execution intervals. This is one synthetic residual block, not a full ResNet inference or accuracy benchmark.

#### Sequential projected residual blocks

`blocks.c` reuses the same four IPs for `32×32×16 → 16×16×32 → 8×8×64`. AutoLink pipelines full-width strips within each block; the next block starts after the current block drains. Output tiles are initially 2×16 and 2×8. Gemmini SPMs are 128 KiB each, accumulators remain 32 KiB, and CGRA SPMs remain 64 KiB each. Both CGRAs expose packed INT8 windows while computing with INT32 locally.

Native convolution DMA loads weights from DRAM and reads the preceding block's packed CGRA1 output directly for the next Conv1 and projection. There is no separate boundary copy or intermediate DRAM writeback. Both blocks' outputs remain in separate CGRA1 SPM regions, starting at local words 4096 and 12288, for checking all 12288 values. Static CGRA kernels stay resident; Add job variants select each output region. The host generates reference outputs and constant synthetic weights, while the CPU initializes the first input tensor.

`BINARY_ARGS` selects the configuration policy: `0` configures and starts each block from the CPU, `1` preconfigures both blocks but lets the CPU trigger each cached run, and `2` preconfigures both and starts one hardware-controlled sequence. The graph and data path are identical in all modes; the full chain does not overlap different blocks.

```shell
$ chipyard/.conda-env/bin/python scripts/blocks_data.py
$ chipyard/.conda-env/bin/python scripts/cgra_fast_api.py --soc-yaml configs/soc/autolink/blocks.yaml configs/kernels/kernel_relu_runtime_4x4.yaml configs/kernels/kernel_add_relu_runtime_4x4.yaml
$ CONFIG=MultiAccelRocketConfig TEST_SRC=tests/cgra-gemmini/blocks.c BINARY_ARGS=0 LOADMEM=1 timeout_cycles=4000000 ./run-chipyard-cgra-gemmini-demo.sh --soc-yaml configs/soc/autolink/blocks.yaml --rebuild
$ CONFIG=MultiAccelRocketConfig TEST_SRC=tests/cgra-gemmini/blocks.c BINARY_ARGS=1 LOADMEM=1 timeout_cycles=4000000 ./run-chipyard-cgra-gemmini-demo.sh --soc-yaml configs/soc/autolink/blocks.yaml
$ CONFIG=MultiAccelRocketConfig TEST_SRC=tests/cgra-gemmini/blocks.c BINARY_ARGS=2 LOADMEM=1 timeout_cycles=4000000 ./run-chipyard-cgra-gemmini-demo.sh --soc-yaml configs/soc/autolink/blocks.yaml
```

`initial_setup_cycles` measures configuration before launch, excluding common input preparation. `chain_cycles` covers the whole chain, including CPU-controlled boundary configuration or triggering where applicable. CPU/Cached boundary counters are parts of this interval, not additional costs. Auto reports only the last block's fabric counters and does not claim to measure its hardware boundary cost separately. Output checking and result printing occur afterward. This is a synthetic two-block workload, not full ResNet inference.

#### Synthetic ResNet-8

`resnet.c` follows the MLPerf Tiny v1.1 ResNet-8 dimensions: `32×32×3 → stem (16 channels) → ordinary residual block (16) → projected block (32) → projected block (64) → global average → FC (10 logits)`. It uses synthetic INT8 inputs and weights, saturation-based quantization and TensorFlow SAME padding, not trained-model accuracy evaluation. Softmax is not included.

Both Gemmini and both CGRA SPMs are 128 KiB each; Gemmini accumulators remain 32 KiB. CGRAs compute in INT32 and expose INT8 outputs. Intermediate tensors remain in SPM, weights and reference values reside in DRAM, and the ten final INT32 logits are written to DRAM. The existing Pool performs signed AveragePool through its unchanged RoCC interface.

`BINARY_ARGS=0` runs the whole network manually. `1` uses the same manual stem, ordinary block and tail, but the CPU starts an AutoLink tiled pipeline for each projected block. This Hybrid mode does not make the whole network autonomous. Both modes use two-row full-width strips and check the stem, every residual block, global average and final logits against host-generated reference values.

`setup_cycles` includes input preparation, preloading and initial configuration. `execution_cycles` sums the stem, residual blocks, Pool and FC intervals, including CPU control within those stages but excluding progress printing and output checking. Hybrid fabric counters describe each projected block, not the whole network. These are simulated CPU cycles, not host simulation time.

```shell
$ chipyard/.conda-env/bin/python scripts/resnet_data.py
$ chipyard/.conda-env/bin/python scripts/cgra_fast_api.py --soc-yaml configs/soc/autolink/resnet.yaml configs/kernels/kernel_relu_runtime_4x4.yaml configs/kernels/kernel_add_relu_runtime_4x4.yaml
$ CONFIG=MultiAccelRocketConfig TEST_SRC=tests/cgra-gemmini/resnet.c BINARY_ARGS=0 LOADMEM=1 timeout_cycles=8000000 ./run-chipyard-cgra-gemmini-demo.sh --soc-yaml configs/soc/autolink/resnet.yaml --rebuild
$ CONFIG=MultiAccelRocketConfig TEST_SRC=tests/cgra-gemmini/resnet.c BINARY_ARGS=1 LOADMEM=1 timeout_cycles=8000000 ./run-chipyard-cgra-gemmini-demo.sh --soc-yaml configs/soc/autolink/resnet.yaml
```

#### Tiled Conv → ReLU → Pool

`pool_tiles.c` runs a 4×6×3 input through a 3×3 convolution with eight output channels, ReLU, and 2×2 stride-2 MaxPool, producing a 2×3×8 output. One captured configuration runs both 1×2 and 2×1 output-tile partitions, including smaller boundary tiles. Gemmini and Pool use INT8; the CGRA wrapper expands input to INT32 and packs its INT32 output back to INT8.

Gemmini and CGRA each use two regions in their own SPM. AutoLink manages their lifetimes and can keep all three IPs active on different tiles. CGRA input DMA can also overlap its current computation in another slot. Pool streams from CGRA SPM and writes each tile directly into its final DRAM position; it has no publication SPM.

```shell
$ .venv/bin/python scripts/cgra_fast_api.py --arch-yaml configs/arch/arch.yaml --soc-yaml configs/soc/autolink/tiles.yaml configs/kernels/kernel_relu_runtime_4x4.yaml --output-dir tests/generated
$ CONFIG=CgraPoolTileRocketConfig TEST_SRC=tests/cgra-gemmini/pool_tiles.c LOADMEM=1 timeout_cycles=350000 ./run-chipyard-cgra-gemmini-demo.sh --soc-yaml configs/soc/autolink/tiles.yaml --rebuild
```

The cycle budget includes software reference computation and configuration. Printed `cycles` measure the CPU-observed pipeline interval; `overlap` and `peak` measure cross-IP task overlap, not arithmetic-unit utilization. CI runs this test alongside the existing manual, automatic, AES, Pool and non-tiled residual tests, while retaining separate CGRA and OpenFPGA jobs.

The tiled residual test (`residual_tiles.c`, `res_tiles.yaml`) is retained but currently deferred; its single-Gemmini/single-CGRA reentrant pipeline remains outside the validated scope. It describes Conv1 → ReLU → Conv2 → Add+ReLU with a skip dependency, two tile partitions and preloaded skip data. Final output stays in separate per-tile SPM regions for CPU validation through the INT8 window after the run. Output checks skip each tile's first element because of [the known VectorCGRA store bug](https://github.com/coredac/CGRA-SoC/issues/3). The reported overlap counts active task intervals on different IPs and tiles, not arithmetic-unit utilization.
