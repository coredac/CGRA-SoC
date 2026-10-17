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
- Supported: CPU-loaded dependency graphs within generated IP and table capacities
- Unsupported: in-flight graph changes and concurrent kernels on one IP

In the three-stage AES path, Gemmini publishes to its external SPM, CGRA pulls the data and computes into its local SPM, and AES reads that SPM directly before writing ciphertext to DRAM. That demo remains sequential with one 128-byte chunk. AutoLink carries control and TileLink carries payload; tiled CNN demos reuse cached jobs across multiple chunks. See [hardware contracts](./docs/contracts.md) for supported interfaces.

CPU-issued Gemmini producers finish their command sequence with `gemmini_commands_end()` so the wrapper can publish after computation and DMA drain. Captured automatic jobs already carry a command count and do not need this marker. Plain manual execution is unchanged.

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

Use `--soc-yaml` to select the hardware and default graph; changing this YAML requires `--rebuild`. The AES configuration defaults to `gca.yaml` for the full AES → Gemmini → CGRA → AES path.

#### Runtime dependency graphs

CPU-started graphs can change without rebuilding RTL, within the selected hardware's IP, stage, dependency and job capacities. A runtime graph YAML names stages, their endpoint instances and dependencies; it does not instantiate new hardware. Generate its C descriptor with `--graph-yaml`, then call `auto_link_load(&GRAPH, addresses)` before the existing tile configuration and `auto_link_input_ready()`. Wait for all results and `RUNNING=0` before replacing the graph. Legacy externally triggered configurations retain their original startup and do not support runtime graph replacement.

Named `buffers` declare addresses, sizes and optional formats once. A stage's `output.buffer` and its consumers' dependency `buffer` refer to the same storage. Fixed addresses can name SPM; symbolic addresses receive software-owned pointers through `auto_link_load`. Pool/AES automatic destinations use the graph binding; Gemmini uses a captured native output command with an `OutputAddress` patch. CGRA does not accept an explicit output buffer: its results stay in local SPM for consumers to pull. Software can explicitly issue a manual DMA write after the graph drains. Buffers are passive storage, not extra stages or DMA engines. Manual APIs are unchanged.

`graph.c` changes graphs on one five-IP system and checks SPM and DRAM transfers. It rebinds Gemmini/Pool DRAM outputs without reconfiguring the IP jobs, while Pool pulls CGRA results directly from SPM. Separate raw/packed input graphs finish in CGRA SPM, then the CPU explicitly requests raw INT32 or packed INT8 DMA writeout. Generate its descriptors and ReLU API before running:

```shell
$ for graph in local dram raw packed; do chipyard/.conda-env/bin/python scripts/generate_auto_links.py --soc-yaml configs/soc/autolink/graph.yaml --graph-yaml configs/graphs/$graph.yaml --header-out tests/generated/graph_$graph.h; done
$ chipyard/.conda-env/bin/python scripts/cgra_fast_api.py --soc-yaml configs/soc/autolink/graph.yaml configs/kernels/kernel_relu_runtime_4x4.yaml
$ CONFIG=MultiAccelRocketConfig TEST_SRC=tests/cgra-gemmini/graph.c LOADMEM=1 timeout_cycles=350000 ./run-chipyard-cgra-gemmini-demo.sh --soc-yaml configs/soc/autolink/graph.yaml --rebuild
```

Later edits to runtime graph YAML require regenerating its header and rebuilding the C binary, not the simulator. A physical address selects SPM or DRAM through TileLink; the graph does not carry a memory-type flag. Manual CGRA writeout handles a contiguous region, not a two-dimensional tensor scatter.

The GitHub workflow runs this test in the CGRA and Gemmini E2E job, generating all four graph descriptors and the matching ReLU API before rebuilding the simulator. Its log is included in the existing test-log artifact.

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

`resnet.c` follows the MLPerf Tiny v1.1 ResNet-8 dimensions: `32×32×3 → stem (16 channels) → ordinary residual block (16) → projected block (32) → projected block (64) → global average → FC → Softmax (10 probabilities)`. It uses synthetic INT8 inputs and weights, saturation-based quantization and TensorFlow SAME padding, not trained-model accuracy evaluation. The CGRA Softmax interprets the INT32 logits with scale `1/256` and leaves Q15 probabilities in its SPM; `32768` represents one.

Both Gemmini and both CGRA SPMs are 128 KiB each; Gemmini accumulators remain 32 KiB. CGRAs compute in INT32 and expose INT8 outputs. Intermediate tensors remain in SPM; weights and reference values reside in DRAM. FC publishes ten INT32 logits to Gemmini0 SPM, then CGRA0 DMA reads them directly for Softmax. The existing Pool performs signed AveragePool through its unchanged RoCC interface.

`BINARY_ARGS=0` runs the whole network manually. `1` uses the same manual stem, ordinary block and tail, but the CPU starts an AutoLink tiled pipeline for each projected block. This Hybrid mode does not make the whole network autonomous. Both modes use two-row full-width strips and check the stem, every residual block, global average, logits and probabilities against host-generated reference values.

After FC, both modes use the CPU to load and start three CGRA0 phases: maximum, approximate exponential/sum, and normalization. Each phase replaces the previous control configuration without expanding control memory. Native RoCC reads preserve the INT32/Q15 probabilities instead of passing through the INT8 TileLink window. A subsequent inference must reload the ReLU configuration.

`setup_cycles` includes input preparation, preloading and initial configuration. `execution_cycles` sums the stem, residual blocks, Pool, FC and Softmax intervals, including CPU control within those stages but excluding progress printing and output checking. `softmax_cycles` includes the SPM-to-SPM DMA and three configuration/execution phases. Hybrid fabric counters describe each projected block, not the whole network. These are simulated CPU cycles, not host simulation time.

```shell
$ chipyard/.conda-env/bin/python scripts/resnet_data.py
$ chipyard/.conda-env/bin/python scripts/cgra_fast_api.py --soc-yaml configs/soc/autolink/resnet.yaml configs/kernels/kernel_relu_runtime_4x4.yaml configs/kernels/kernel_add_relu_runtime_4x4.yaml configs/kernels/kernel_softmax_max_4x4.yaml configs/kernels/kernel_softmax_exp_4x4.yaml configs/kernels/kernel_softmax_norm_4x4.yaml
$ CONFIG=MultiAccelRocketConfig TEST_SRC=tests/cgra-gemmini/resnet.c BINARY_ARGS=0 LOADMEM=1 timeout_cycles=8000000 ./run-chipyard-cgra-gemmini-demo.sh --soc-yaml configs/soc/autolink/resnet.yaml --rebuild
$ CONFIG=MultiAccelRocketConfig TEST_SRC=tests/cgra-gemmini/resnet.c BINARY_ARGS=1 LOADMEM=1 timeout_cycles=8000000 ./run-chipyard-cgra-gemmini-demo.sh --soc-yaml configs/soc/autolink/resnet.yaml
```

#### Causal Attention prefill

`attention.c` runs one attention head with 128 tokens and head dimension 64: `Gemmini0 QKᵀ → CGRA scale/mask/Softmax → Gemmini1 PV`. Q, K and V are synthetic INT8 tensors; scores and accumulators are INT32. Softmax produces values from 0 to 127 in CGRA SPM, read as INT8 through its packed window. This is the attention core, not a full GPT-2 block or a trained-model accuracy evaluation.

Each tile contains 16 query rows and attends to all permitted preceding keys. K and V remain preloaded. The three Softmax phases occupy disjoint resident control ranges; automatic execution follows a software-configured cached-job sequence and publishes only after normalization. Two slots per IP support inter-IP overlap without an intermediate DRAM copy. Manual mode uses the same native commands and kernels, starting each phase from the CPU. Final INT32 output remains in Gemmini1 SPM and all 8192 elements are checked against the host-generated integer reference.

```shell
$ chipyard/.conda-env/bin/python scripts/attention_data.py
$ chipyard/.conda-env/bin/python scripts/cgra_fast_api.py --arch-yaml configs/arch/attention.yaml --soc-yaml configs/soc/autolink/attention.yaml configs/kernels/kernel_attention_max_4x4.yaml configs/kernels/kernel_attention_exp_4x4.yaml configs/kernels/kernel_attention_norm_4x4.yaml
$ CONFIG=MultiAccelRocketConfig BINARY_ARGS=manual LOADMEM=1 timeout_cycles=4000000 ./run-chipyard-cgra-gemmini-demo.sh --arch-yaml configs/arch/attention.yaml --soc-yaml configs/soc/autolink/attention.yaml --rebuild tests/cgra-gemmini/attention.c
$ CONFIG=MultiAccelRocketConfig BINARY_ARGS=auto LOADMEM=1 timeout_cycles=4000000 ./run-chipyard-cgra-gemmini-demo.sh --arch-yaml configs/arch/attention.yaml --soc-yaml configs/soc/autolink/attention.yaml tests/cgra-gemmini/attention.c
```

`setup_cycles` covers preloading and configuration; `execution_cycles` covers the tiled attention calculation, excluding final checking. Automatic mode also reports fabric cycles and cross-IP overlap. Size changes must fit the native GEMM blocks, command cache and reserved SPM regions; reducing tile rows increases padded Q storage, so smaller tiles do not always use less SPM.

The default configuration passed both modes with all 8192 outputs matching. With Softmax II 12/18/13, manual execution took 1,229,474 CPU cycles and automatic execution took 878,320 CPU cycles, down 14.8% and 19.5% from the previous II 16/24/16 schedules. The automatic fabric recorded 867,527 cycles and 49,966 overlap cycles. Its peak counter includes scheduling activity and is not a count of simultaneously computing IPs.

#### Multi-head Attention

`mha.c` extends the attention core with Q/K/V projections, head concatenation and an output projection. Hybrid mode (`BINARY_ARGS=0`, the default) lets the CPU issue projections and advance between heads. Automatic mode (`BINARY_ARGS=1`) captures shared jobs once and starts the whole layer with one command: one interval repeats projection plus Attention across heads, then another repeats output-projection column blocks. Each head retains the automatic tiled `QKᵀ → Softmax → PV` pipeline. Both modes reuse the two Gemminis, one CGRA and Attention configuration above. They do not include LayerNorm, residual addition or the MLP.

Inputs and weights are synthetic INT8, with INT32 accumulation and INT8 saturation after each projection and PV. Current-head Q/K/V and Softmax intermediates stay in SPM. Gemmini writes each head into its columns of the DRAM concatenation buffer using a row stride, then reads that buffer for the output projection. The host generates exact integer references; the test checks both concatenation and final output without doing model arithmetic on the CPU.

Generate the two-head case (`hidden=128`, `N=128`, `d=64`, 16 query rows per tile) and the loadable single-stage projection graphs. Use the Attention fast APIs above, and rebuild once after changing the hardware or YAML:

```shell
$ chipyard/.conda-env/bin/python scripts/mha_data.py --heads 2
$ for ip in g0 g1; do chipyard/.conda-env/bin/python scripts/generate_auto_links.py --soc-yaml configs/soc/autolink/attention.yaml --graph-yaml configs/graphs/mha_$ip.yaml --header-out tests/generated/mha_$ip.h; done
$ CONFIG=MultiAccelRocketConfig BINARY_ARGS=1 LOADMEM=1 timeout_cycles=12000000 ./run-chipyard-cgra-gemmini-demo.sh --arch-yaml configs/arch/attention.yaml --soc-yaml configs/soc/autolink/attention.yaml --rebuild tests/cgra-gemmini/mha.c
$ CONFIG=MultiAccelRocketConfig BINARY_ARGS=0 LOADMEM=1 timeout_cycles=12000000 ./run-chipyard-cgra-gemmini-demo.sh --arch-yaml configs/arch/attention.yaml --soc-yaml configs/soc/autolink/attention.yaml tests/cgra-gemmini/mha.c
```

For the GPT-2 Small attention dimensions, generate with `--heads 12` (`hidden=768`) and run the same C test with `timeout_cycles=60000000`. No RTL rebuild is needed when changing only this generated data. The YAML reserves nine run profiles, seven Gemmini0 jobs, three Gemmini1 jobs, the three existing CGRA jobs, and 132 completion reports. Gemmini uses native compute loops and runtime address patches instead of duplicating commands for every head. These are synthetic attention tests, not trained-model inference or a complete GPT-2 block.

`setup_cycles` includes initial job and run configuration. `execution_cycles` includes projections and head pipelines but excludes setup and final checking. Both concatenation and final output are checked: 16,384 elements each for two heads and 98,304 each for twelve heads.

Both two-head modes passed: hybrid took 1,844,011 execution cycles and automatic took 1,834,488. Twelve-head automatic execution also passed, with 132 completion reports, zero mismatches in both tensors, and 12,502,648 execution cycles.

#### Synthetic GPT-2 block

`gpt2.c` connects two LayerNorms, Q/K/V and output projections, causal attention, the 4x-expanded GELU MLP and two residual additions on the same Attention hardware. `BINARY_ARGS=0` (the default) selects Manual: the CPU starts and waits for every attention tile's QK, score DMA, three Softmax phases and PV. `BINARY_ARGS=1` selects Hybrid: each head uses the automatic three-IP attention pipeline, with the CPU advancing between heads. Experimental `BINARY_ARGS=2` selects HeadAuto: one launch repeats a QK/V load profile and the Attention profile across heads. All three modes use CPU software to sequence the large block phases; none is whole-block Auto. Gemmini computes matrix products with INT32 partial sums, while CGRA computes LayerNorm, GELU, residuals and INT8 requantization. The default storage policy, `dram`, holds stage tensors in DRAM. The test checks twelve intermediate/final tensors and repeats with a different input without resetting the accelerators.

This is a synthetic fixed-point block, not pretrained GPT-2 inference. Weights have sixteen signed nonzero taps per output but are sent to hardware as dense matrices. The data generator reports clipping and error against a separate floating reference; exact integer agreement does not establish model accuracy. Numerical scales and the synthetic LayerNorm epsilon are specified in [hardware contracts](docs/contracts.md).

Generate the small two-head case and kernel APIs after the matching `VectorCGRA/cgra/test/CgraRTL_block_test_from_yaml.py --test-verilog` reference test passes:

```shell
$ chipyard/.conda-env/bin/python scripts/gpt2_data.py --heads 2 --length 64 --runs 2 --report /tmp/gpt2-reference.json
$ chipyard/.conda-env/bin/python scripts/cgra_fast_api.py --arch-yaml configs/arch/attention.yaml --soc-yaml configs/soc/autolink/attention.yaml configs/kernels/kernel_attention_*_4x4.yaml configs/kernels/kernel_block_*_4x4.yaml
$ chipyard/.conda-env/bin/python scripts/generate_auto_links.py --soc-yaml configs/soc/autolink/attention.yaml --graph-yaml configs/graphs/block_load.yaml --header-out tests/generated/block_load.h
$ CONFIG=MultiAccelRocketConfig BINARY_ARGS=0 LOADMEM=1 timeout_cycles=200000000 ./run-chipyard-cgra-gemmini-demo.sh --arch-yaml configs/arch/attention.yaml --soc-yaml configs/soc/autolink/attention.yaml --rebuild tests/cgra-gemmini/gpt2.c
```

Use `--heads 12 --length 128` for hidden width 768 and MLP width 3072. Data changes alone do not require another hardware rebuild. `mlp.c` separately exercises both Gemminis with one and four K chunks, signed extreme inputs, bias-once accumulation and full INT32 output. Generate its fixtures with `python scripts/mlp_data.py`, then run it with the same runner and Attention configuration. Its independent integer reference runs on the host; the simulated CPU checks all 6,144 hardware outputs.

For the two-head N=64 case, add `spm` as the second program argument: `BINARY_ARGS='0 spm'` for Manual, `BINARY_ARGS='1 spm'` for Hybrid or `BINARY_ARGS='2 spm'` for experimental HeadAuto. These keep intermediate activations in the existing Gemmini/CGRA SPMs. Each GEMM tile publishes INT32 results locally, then CGRA requantizes and scatters packed rows into the retained tensor. The block loads its input once and returns only its final activation to DRAM. Weights, biases, LayerNorm parameters and CPU reference checks still read DRAM. This policy also supports the twelve-head N=16 shape documented below; other shapes are rejected without silently spilling to DRAM. Use `BINARY_ARGS='1 dram'` to select the original Hybrid storage explicitly. No hardware rebuild is needed when changing only these arguments.

`heads.c` isolates Hybrid (`BINARY_ARGS=1`) and HeadAuto (`BINARY_ARGS=2`) with the two-head N=64 fixture and the full block's retained Q/K/V/PV SPM addresses. It checks two inputs without reset and native CGRA execution before and after Attention. Its `setup`, `execute` and `total` values are CPU cycles; setup includes command capture on every call, including per-head PV capture in Hybrid. Total excludes fixture upload and output requantization/checking. Run it with the Attention configuration above and `timeout_cycles=12000000`. This focused test does not validate the complete block or twelve-head execution.

Both focused modes passed 16,384 concatenated values plus four native warmup values and exited successfully on the same simulator. The results below include per-call configuration costs. HeadAuto used 0.74% and 0.55% more total cycles; this trial demonstrates autonomous head scheduling, not a speedup. It remains experimental and does not replace Hybrid.

| Mode | Input | Setup cycles | Execute cycles | Total cycles |
| --- | --- | ---: | ---: | ---: |
| Hybrid | 0 | 22,926 | 444,235 | 467,651 |
| HeadAuto | 0 | 25,322 | 445,543 | 471,092 |
| Hybrid | 1 | 14,897 | 443,981 | 459,081 |
| HeadAuto | 1 | 16,263 | 445,244 | 461,611 |

`block_spm_test.c` checks both Gemminis' local input DMA, INT32 tile publication, CGRA packed row writeback and subsequent native DMA consumption. Its 4,096 output checks passed. Manual and Hybrid each passed two consecutive full-block inputs without reset: 24 stage checks and 294,912 values per mode, with zero mismatches and successful simulator exits. CGRA helper DMA counters measure requested payload bytes, not total system-bus traffic; they exclude Attention's direct score DMA, native Gemmini traffic and CPU checks. Both runs in both modes matched exactly 10,240 DRAM-read bytes (8,192 input plus 2,048 LayerNorm parameter bytes) and 8,192 final-output bytes from these helpers, as required by the test. Initial/final activation transfers are included in the first LayerNorm and final residual stage intervals.

Before multirow LayerNorm batching, this two-head SPM case used 16,720,230 and 16,689,313 CPU execution cycles in Manual and 16,419,326 and 16,388,239 in Hybrid, about 1.8% fewer cycles for each input. The two LayerNorms accounted for about 53% of Hybrid execution time. This comparison is between scheduling modes with SPM storage, not a measured DRAM-to-SPM speedup.

`norm.c` checks the maximum LayerNorm batch layout: sixteen 768-element rows, packed DMA and high SPM addresses, repeated with different inputs. Generate its fixtures with `python scripts/norm_data.py` and use the same runner and Attention configuration. It checks all 24,576 outputs against host-generated expectations. Every test requires its `PASS` message and a successful simulator exit.

`block_switch.c` checks a LayerNorm batch, Attention and another LayerNorm batch in Manual mode, then in Hybrid mode with a different input and no accelerator reset. Run it with the same Attention configuration and `tests/cgra-gemmini/block_switch.c` as the runner's source argument. After the LayerNorm remap, the two-head N=64 fixture passed all 24,576 checks and exited successfully, including native CGRA execution after Hybrid Attention. Projections and residual inputs are precomputed, so this is a control-state switching regression, not full-block validation.

`timeout_cycles` counts testbench clocks, not the CPU `rdcycle` values printed by these tests. In this Attention configuration, the testbench runs at 1 GHz and the CPU at 500 MHz: allow at least twice the expected CPU cycles, plus startup and checking time. Use `timeout_cycles=30000000` for the small-fixture switching regression. A timeout alone does not establish an accelerator stall.

GPT-2 reports per-stage `elapsed` cycles and their sum, `execution_cycles`, excluding result checking and progress printing. `norm.c` measures the LayerNorm call before checking its output. Both report CPU-observed block-CGRA helper intervals for configuration, execution waits and DMA, plus LayerNorm phase totals. Configuration includes CPU copying, patching and command submission; execution waits can include pending configuration drain. These helper counters exclude Gemmini and Attention scheduling, and phase totals overlap them. They are not pure FU timings or host simulation durations. Instrumentation and cache effects from intermediate checks remain in these tests.

Before the LayerNorm remap, the two-head N=64 block passed two consecutive runs in Manual and in the original Hybrid implementation, with 294,912 checked values per mode. After sharing the native QK/PV commands, a focused Hybrid regression also passed both inputs with 16,384 concatenated values checked, including restoration of the Softmax programs after a block kernel. The 768-wide LayerNorm, both-endpoint K=3072 probes and existing twelve-head MHA regression also passed at that stage. The later N=16 experiment below validates the twelve-head full block end to end on the SoC simulator.

The initial single-row multi-PE LayerNorm remap passed the from-YAML Verilog reference, including 768-wide signed and extreme-value cases, and the repeated 16x768 SoC test with all 24,576 outputs matching. On unchanged hardware with the same instrumented harness, its two runs took 1,831,560 and 1,816,660 CPU cycles versus 2,064,142 and 2,049,479 for the scalar mapping, reductions of 11.3% and 11.4%. A two-row 768-wide probe also passed both inputs, reducing cycles by 8.9% and 9.5%. In the full-batch test, execution-wait cycles fell about 28%, but configuration cycles grew about 7% and accounted for about half the total. These are isolated LayerNorm results, not a full-block speedup.

That single-row remap was not faster at every size. A cold 16x128 batch passed all 2,048 output checks but took 1,136,551 cycles versus 1,122,785 with the original scalar packets, about 1.2% more. Execution waits fell from 181,040 to 132,865 cycles, while configuration rose from 878,128 to 940,053. The old packets were recovered byte-for-byte from the preserved baseline executable; both tests used the same instrumented C helper and input. This single cold-batch comparison does not establish the performance of a complete four-batch N=64 block. There is no size-based mapping selector in this implementation.

Multirow LayerNorm now processes a batch per phase, splitting at row boundaries only when required by the 16-bit execution counter. It retains the same hardware and integer rules. The from-YAML Verilog reference passed 46 phase jobs and 13,351 value checks across widths 8/128/768, including repeated programs with different addresses and row counts. The same cold 16x128 SoC harness passed all 2,048 outputs and exited successfully in 253,112 CPU cycles versus 1,136,551 for the single-row remap: 4.49x faster, or 77.7% fewer cycles. Launches fell from 112 to seven and configuration cycles from 940,053 to 95,759; execution waits were 134,397 versus 132,865. This isolates the benefit of amortizing configuration rather than faster arithmetic; it is not a full-block speedup.

The multirow 16x768 SoC test also passed both inputs and all 24,576 outputs with a successful exit. Its two calls used 1,092,589 and 1,075,887 CPU cycles versus 1,831,560 and 1,816,660 for the single-row remap: 1.68x and 1.69x faster, or 40.3% and 40.8% fewer cycles. Each call used sixteen launches, including row-aligned REPEAT chunks that fit the existing execution counter. The RTL and test fixtures were unchanged for these comparisons.

With multirow LayerNorm, the full two-head N=64 SPM block passed both consecutive inputs in Manual and Hybrid, with 24 stage checks, 294,912 matching values and a successful simulator exit per mode. Manual used 9,863,347 and 9,830,513 CPU execution cycles, about 1.70x faster than its single-row baseline. Hybrid used 9,560,556 and 9,527,817 cycles, about 1.72x faster. The two LayerNorms now account for about 19.4% of Manual and 20.0% of Hybrid execution time. CGRA helper launches fell from 1,056 to 216 per input, with the same measured DMA payloads. In Hybrid's first input, helper configuration fell from 8,937,549 to 2,192,393 cycles while execution waits stayed near 5.55 million cycles; the gain comes mainly from fewer configurations, not faster arithmetic or less DRAM traffic. These full-block measurements use H=128 and two heads; the H=768 result above is isolated LayerNorm, not a twelve-head full-block result.

`gelu.c` isolates block activation with exhaustive INT8 values, a 2048-element chunk plus a five-element tail, and a changed input routed through Gemmini SPM after a residual kernel. It also checks in-place activation, as used by the full block. The fused GELU/requant program uses the same Attention hardware. Run its matching `VectorCGRA/cgra/test/CgraRTL_activation_test_from_yaml.py --test-verilog` reference before generating the API and running the SoC test:

```shell
$ chipyard/.conda-env/bin/python scripts/gelu_data.py
$ chipyard/.conda-env/bin/python scripts/cgra_fast_api.py --arch-yaml configs/arch/attention.yaml --soc-yaml configs/soc/autolink/attention.yaml configs/kernels/kernel_block_activation_4x4.yaml
$ CONFIG=MultiAccelRocketConfig LOADMEM=1 timeout_cycles=3000000 ./run-chipyard-cgra-gemmini-demo.sh --arch-yaml configs/arch/attention.yaml --soc-yaml configs/soc/autolink/attention.yaml tests/cgra-gemmini/gelu.c
```

The fused activation passed the from-YAML Verilog reference (256 exhaustive values plus nine relocated REPEAT values) and all 2,567 SoC checks, including kernel switching and in-place SPM execution. Both simulators exited successfully. The 2,053-element DRAM case took 116,544 CPU cycles versus 201,837 for separate GELU/requant kernels (1.73x faster), reducing launches from six to two. The 257-element SPM case took 25,463 versus 37,577 cycles (1.48x faster), with one launch instead of two. Requested DMA bytes were unchanged; the gain combines two-PE pipelining, larger chunks and configuration reuse. The separate in-place SPM check took 25,129 cycles. These are isolated activation measurements on unchanged hardware; the full-block results above predate this GELU change.

#### Whole-block Auto

`auto_block.c` adds an experimental whole-block Auto path for the N=64, hidden=128, two-head fixture above. After CPU configuration, one launch runs both LayerNorms, all projections, Attention, GELU, residuals and final output writeback. Intermediate activations stay in the existing SPM layout; weight, bias and affine-parameter reads still use DRAM. This is an autonomy test, not a performance target. The existing `gpt2.c` Manual, Hybrid and HeadAuto modes are unchanged.

Use the generated small-block data, Attention/block kernel APIs and `block_load.h` described above. The new `gpt2.yaml` expands control caches without increasing the CGRA array, per-PE control slots or data SPM sizes. Rebuild when switching from `attention.yaml`:

```shell
$ CONFIG=MultiAccelRocketConfig LOADMEM=1 timeout_cycles=12000000 ./run-chipyard-cgra-gemmini-demo.sh --rebuild --arch-yaml configs/arch/attention.yaml --soc-yaml configs/soc/autolink/gpt2.yaml tests/cgra-gemmini/auto_switch.c
$ CONFIG=MultiAccelRocketConfig BINARY_ARGS=0 LOADMEM=1 timeout_cycles=100000000 ./run-chipyard-cgra-gemmini-demo.sh --arch-yaml configs/arch/attention.yaml --soc-yaml configs/soc/autolink/gpt2.yaml tests/cgra-gemmini/auto_block.c
```

`auto_switch.c` checks cached kernel replacement, repetition and native/automatic transitions, including return to the same cached job after native configuration. `auto_block.c` repeats two different inputs without accelerator reset. Mode `0` runs the full block; modes `1`–`7` isolate copy, Q projection, LayerNorm, GELU, residual, raw INT32 PV requantization and Attention plus PV requantization. Each focused mode also checks both inputs. Full mode checks final output after execution; focused modes check intermediate stages independently without introducing CPU intervention into the complete run. Full mode passed both inputs in the rebuilt SoC simulator: all 16,384 output values matched, completion reports passed and the process exited successfully. Each input used one automatic launch; execution took 6,405,729 and 6,396,674 target cycles. These results use asynchronous Gemmini command-cache storage with unchanged capacity and replay timing; the two-input HeadAuto regression also passed.

To compare current Manual, Hybrid and whole-block Auto, use the same generated fixture and rebuilt `gpt2.yaml` simulator for all three runs. The `spm bench` option skips intermediate stage checks in `gpt2.c` and checks final output after timing. Each `total` interval includes configuration, input loading, computation and final output writeback; Auto also includes completion-report collection. Initial Gemmini flush and final output checks are outside timing. Both inputs run consecutively without reset; report their results separately. Default `gpt2.c` runs retain intermediate stage validation.

```shell
$ CONFIG=MultiAccelRocketConfig BINARY_ARGS='0 spm bench' RANDOM_SEED=1 LOADMEM=1 timeout_cycles=100000000 ./run-chipyard-cgra-gemmini-demo.sh --arch-yaml configs/arch/attention.yaml --soc-yaml configs/soc/autolink/gpt2.yaml tests/cgra-gemmini/gpt2.c
$ CONFIG=MultiAccelRocketConfig BINARY_ARGS='1 spm bench' RANDOM_SEED=1 LOADMEM=1 timeout_cycles=100000000 ./run-chipyard-cgra-gemmini-demo.sh --arch-yaml configs/arch/attention.yaml --soc-yaml configs/soc/autolink/gpt2.yaml tests/cgra-gemmini/gpt2.c
$ CONFIG=MultiAccelRocketConfig BINARY_ARGS=0 RANDOM_SEED=1 LOADMEM=1 timeout_cycles=100000000 ./run-chipyard-cgra-gemmini-demo.sh --arch-yaml configs/arch/attention.yaml --soc-yaml configs/soc/autolink/gpt2.yaml tests/cgra-gemmini/auto_block.c
```

These runs compare complete implementations, not scheduling alone: Auto uses Gemmini gather/collect and a different projection traversal order, while Manual/Hybrid use direct CGRA DMA and retain helper timing counters. Simulator host wall time is separate from target-cycle performance.

The same-input comparison passed both inputs in all three modes on the same eight-thread simulator, with seed 1, current multirow LayerNorm and fused GELU. Each mode checked 16,384 output values with no mismatch and exited successfully. These uninterrupted totals include configuration and replace the earlier cross-version performance comparison:

| Mode | Input 0 total cycles | Input 1 total cycles | Mean latency at 500 MHz | Host wall time for both inputs |
| --- | ---: | ---: | ---: | ---: |
| Manual | 8,584,827 | 8,543,830 | 17.129 ms | 3371.49 s |
| Hybrid | 8,288,668 | 8,246,231 | 16.535 ms | 3267.17 s |
| Auto | 6,755,110 | 6,722,451 | 13.478 ms | 2718.61 s |

Across these two inputs, Auto used 21.32% fewer cycles than Manual (1.27x speedup) and 18.49% fewer than Hybrid (1.23x). Auto setup took 349,477 and 325,531 cycles; execution plus completion handling took 6,405,633 and 6,396,920 cycles. Host wall time includes test compilation, boot, output checks and printing, but excludes the reused hardware-model build. These results apply to this synthetic N64 H128 two-head FF512 fixture, not arbitrary shapes or a pretrained model.

#### Operator-level Hybrid experiment

`hybrid_block.c` tests a smaller software working set on the existing `gpt2.yaml` simulator. The CPU captures and launches each operator separately, then checks and drains its reports before reusing cached job/profile slots. Hardware still executes the operator's internal tiles and Attention heads. Unlike `gpt2.c`'s original head-level Hybrid, this path shares whole-block Auto's gather/collect and projection traversal. It limits usage to 8 profiles, 8 jobs per endpoint and 128 pending reports; the physical hardware capacities are unchanged.

Reuse the matching whole-block Auto simulator and generated fixture; no rebuild is needed for this C-only experiment:

```shell
$ CONFIG=MultiAccelRocketConfig RANDOM_SEED=1 LOADMEM=1 timeout_cycles=100000000 ./run-chipyard-cgra-gemmini-demo.sh --arch-yaml configs/arch/attention.yaml --soc-yaml configs/soc/autolink/gpt2.yaml tests/cgra-gemmini/hybrid_block.c
```

Two different inputs run consecutively without reset. Total target cycles include recapture, CPU scheduling, execution and report handling. Per-operator statistics and final output checks are printed after timing. This experiment measures functional correctness, cycle cost and software capacity requirements, not reduced-hardware area or timing.

The operator-level Hybrid test passed both inputs on the same eight-thread simulator and fixture used above: all 16,384 output values matched, each input's 378 completion reports passed, and the process exited successfully. Peak usage was 8 profiles, 8 jobs on each Gemmini, 7 CGRA jobs and 96 pending reports. No hardware configuration or shared Auto helper changed for this experiment.

| Path | Input 0 total cycles | Input 1 total cycles |
| --- | ---: | ---: |
| Manual baseline | 8,584,827 | 8,543,830 |
| Original head-level Hybrid baseline | 8,288,668 | 8,246,231 |
| Operator-level Hybrid | 6,823,656 | 6,784,020 |
| Whole-block Auto baseline | 6,755,110 | 6,722,451 |

Operator-level Hybrid used 1.01% and 0.92% more cycles than Auto, respectively. Across both inputs, it used 0.97% more cycles than Auto, 17.70% fewer than the original Hybrid and 20.56% fewer than Manual. Setup cost was 412,411/382,302 cycles; execution plus report handling cost was 6,410,528/6,401,293 cycles. Total also includes a small amount of inter-operator accounting. Host wall time was 2979.09 s, excluding the reused hardware build. The baseline rows are the completed same-input measurements above, not new reruns. Physical cache reduction and its area/timing effects have not been validated.

#### GPT-2 Small-width prefill experiment

The same three entrypoints also define a short-sequence fixture with N=16, hidden=768, twelve 64-wide heads and FF=3072. Generate its two inputs below, then use `gpt2.c` with `BINARY_ARGS='0 spm bench'`, `auto_block.c` with mode 0, and `hybrid_block.c` with the same `gpt2.yaml` simulator. This changes data and software plans only; a matching existing hardware model can be reused.

```shell
$ chipyard/.conda-env/bin/python scripts/gpt2_data.py --heads 12 --length 16 --runs 2 --report /tmp/gpt2-small-reference.json
```

This remains one synthetic prefill block, not the complete twelve-layer pretrained model or KV-cache decode. Wide Auto/Hybrid use two-row LayerNorm groups and compact column-iteration matrix jobs; Hybrid splits FC1 into two launches to retain its smaller working-set limit. Manual retains its own batching and command organization, so totals compare complete implementations. The N64 performance tables above do not describe this wider fixture.

All three modes passed both identical inputs without accelerator reset, each checking 24,576 output values and exiting successfully. Total CPU-observed target cycles include configuration, input/output movement, execution and completion handling; output checking and result printing are outside the interval.

| Mode | Input 0 cycles | Input 1 cycles |
| --- | ---: | ---: |
| Manual | 13,210,345 | 13,211,806 |
| Whole-block Auto | 10,351,409 | 10,353,004 |
| Operator-level Hybrid | 10,475,146 | 10,471,186 |

Hybrid used 1.20% and 1.14% more cycles than Auto, and 20.70% and 20.74% fewer than Manual. Both Hybrid inputs had peaks of 2 profiles, 2/2/7 Gemmini0/Gemmini1/CGRA jobs and 72 pending reports. Auto retained 16 profiles, 15/15 Gemmini job IDs and 23 CGRA jobs. Auto and Hybrid each checked 624 completion reports per input; Auto drains them during execution because the total exceeds the 512-entry queue. These are software working-set measurements on unchanged hardware, not measured area reductions.

Host runner times were 7,365.33 s for Auto, 8,636.46 s for Hybrid and 9,487.37 s for Manual, with the same eight-thread hardware model reused. Simulations ran concurrently after Auto's earlier start, so these are waiting-time records, not a fair host-performance comparison. The focused K=768/K=3072 column-job test also passed all 24,576 outputs before the full runs.

#### GPT-2 CI

The [SoC workflow](.github/workflows/cgra-soc-tests.yml) runs Manual (`gpt2.c`, `0 spm bench`), whole-block Auto (`auto_block.c`, `0`) and operator-level Hybrid (`hybrid_block.c`) in independent jobs for code changes. PR and push runs use N=64, hidden=128, two heads and FF=512. To test N=16, hidden=768, twelve heads and FF=3072, manually dispatch the workflow with `gpt2_shape=wide`. The default dispatch shape is `small`.

Each job checks the host references and translated CGRA kernels, generates the same seed-0 pair of inputs, regenerates the Attention/block APIs and rebuilds `MultiAccelRocketConfig` from `attention.yaml` and `gpt2.yaml`. Both inputs run without accelerator reset. A successful exit and the final PASS marker are required. The job summary records target-cycle totals, output checks and the fixture checksum; artifacts retain simulator/reference logs and the numerical reference report. Compare target cycles, not CI host times, which include setup/build costs and depend on runner load. Neither shape measures reduced-capacity hardware area or pretrained-model accuracy.

#### Synthetic single-layer GCN

`gcn.c` runs `ReLU(D^-1/2 (A+I) D^-1/2 XW)` in Manual, Auto and Hybrid modes on the same hardware. Gemmini computes the dense projection; CGRA follows CSR neighbor indices and accumulates normalized weighted features. Manual schedules each node/channel chunk on the CPU. Auto submits the projection, input copies and complete aggregation chain once; Hybrid caches at most eight aggregation tasks and reuses those job IDs between completed batches. All modes finish with one CPU-issued raw DMA to preserve INT32 Q15 output. Graphs, degree one-hot features and seeded weights are generated locally; no dataset download or trained-model accuracy claim is involved.

Generate the data and kernel API, then use the existing GPT-2 hardware configuration:

```shell
$ chipyard/.conda-env/bin/python scripts/gcn_data.py --report build/gcn-reference.json
$ chipyard/.conda-env/bin/python scripts/cgra_fast_api.py --arch-yaml configs/arch/attention.yaml --soc-yaml configs/soc/autolink/gpt2.yaml configs/kernels/kernel_gcn_4x4.yaml
$ CONFIG=MultiAccelRocketConfig RANDOM_SEED=1 LOADMEM=1 timeout_cycles=30000000 ./run-chipyard-cgra-gemmini-demo.sh --arch-yaml configs/arch/attention.yaml --soc-yaml configs/soc/autolink/gpt2.yaml tests/cgra-gemmini/gcn.c
```

The runner command assumes the same hardware has already been built; add `--rebuild` when switching configurations. New graph data and kernel packets alone do not require rebuilding RTL. The test alternates all three modes on each graph and repeats twice without accelerator reset, checking every projected and final value. Host and device outputs are poisoned before each timed inference to detect missing writes. Total CPU-observed cycles include task preparation, configuration capture, DMA, computation, waits and final output DMA; poisoning, printing and numerical checks are excluded. Auto/Hybrid also report setup, execution and writeback separately; Manual includes command configuration in execution. Compare total cycles, not execution alone. Peak cached jobs and profiles describe software requirements, not measured hardware area. Require `GCN: PASS` and exit status zero. Run the host reference tests with `python -m unittest discover -s scripts/tests -p gcn_test.py`; the independent kernel reference is `VectorCGRA/cgra/test/CgraRTL_gcn_test_from_yaml.py --test-verilog`.

The seed-0 three-mode regression passed all 18 graph/mode/repeat combinations: 61,440 projected and 52,224 final values, zero mismatches, `GCN: PASS` and exit status zero. The star exercises a 126+2 channel split at its degree-20 center; the irregular graph includes an isolated node. Three host tests passed. The unchanged kernel retains its earlier four-case Verilog reference validation of 209 outputs, including degree 64 and relocated channel slices. The complete regression uses a 30-million simulator-cycle budget; the previous Manual-only 20-million budget truncated the final case and was increased before the successful full rerun.

| Synthetic graph | Nodes | Repeat | Manual cycles | Auto cycles | Hybrid cycles |
| --- | ---: | ---: | ---: | ---: | ---: |
| Ring with chords | 16 | 0 | 325,993 | 335,393 | 335,279 |
| Ring with chords | 16 | 1 | 320,295 | 333,005 | 335,115 |
| Star | 20 | 0 | 270,898 | 287,305 | 291,438 |
| Star | 20 | 1 | 270,434 | 287,415 | 291,526 |
| Irregular with isolated node | 32 | 0 | 619,076 | 639,607 | 646,066 |
| Irregular with isolated node | 32 | 1 | 618,086 | 639,647 | 645,783 |

Auto uses 16/21/32 CGRA jobs for these graphs. Hybrid uses at most eight, in two/three/four batches, with total cycles within 1.5% of Auto in both repeats. Both use two/four/four Gemmini jobs and at most three/five/five profiles. Manual remains faster in these configuration-inclusive small-graph measurements; cached execution reduces CPU scheduling but adds capture/setup cost. These scalar-kernel results do not establish a throughput advantage or reduced hardware area.

#### Tiled Conv → ReLU → Pool

`pool_tiles.c` runs a 4×6×3 input through a 3×3 convolution with eight output channels, ReLU, and 2×2 stride-2 MaxPool, producing a 2×3×8 output. One captured configuration runs both 1×2 and 2×1 output-tile partitions, including smaller boundary tiles. Gemmini and Pool use INT8; the CGRA wrapper expands input to INT32 and packs its INT32 output back to INT8.

Gemmini and CGRA each use two regions in their own SPM. AutoLink manages their lifetimes and can keep all three IPs active on different tiles. CGRA input DMA can also overlap its current computation in another slot. Pool streams from CGRA SPM and writes each tile directly into its final DRAM position; it has no publication SPM.

```shell
$ .venv/bin/python scripts/cgra_fast_api.py --arch-yaml configs/arch/arch.yaml --soc-yaml configs/soc/autolink/tiles.yaml configs/kernels/kernel_relu_runtime_4x4.yaml --output-dir tests/generated
$ CONFIG=CgraPoolTileRocketConfig TEST_SRC=tests/cgra-gemmini/pool_tiles.c LOADMEM=1 timeout_cycles=350000 ./run-chipyard-cgra-gemmini-demo.sh --soc-yaml configs/soc/autolink/tiles.yaml --rebuild
```

The cycle budget includes software reference computation and configuration. Printed `cycles` measure the CPU-observed pipeline interval; `overlap` and `peak` measure cross-IP task overlap, not arithmetic-unit utilization. CI runs this test alongside the existing manual, automatic, AES, Pool and non-tiled residual tests, while retaining separate CGRA and OpenFPGA jobs.

The tiled residual test (`residual_tiles.c`, `res_tiles.yaml`) is retained but currently deferred; its single-Gemmini/single-CGRA reentrant pipeline remains outside the validated scope. It describes Conv1 → ReLU → Conv2 → Add+ReLU with a skip dependency, two tile partitions and preloaded skip data. Final output stays in separate per-tile SPM regions for CPU validation through the INT8 window after the run. Output checks skip each tile's first element because of [the known VectorCGRA store bug](https://github.com/coredac/CGRA-SoC/issues/3). The reported overlap counts active task intervals on different IPs and tiles, not arithmetic-unit utilization.
