# Hardware contracts

Read the sections relevant to the hardware being changed. Repository workflow and generated-file policy live in [AGENTS.md](../AGENTS.md).

## Configuration sources

The single-CGRA configuration is layered:

- `configs/arch/arch.yaml` owns CGRA structure and functional-unit choices.
- `configs/soc/cgra_soc.yaml` owns the SoC interface and memory settings.
- `configs/soc/autolink/gc.yaml` owns the two-IP CGRA and Gemmini SoC settings and automatic task.
- `configs/soc/autolink/gca.yaml` owns the three-IP CGRA, Gemmini, and AES SoC settings and automatic tasks.
- `configs/soc/autolink/gca_short.yaml` owns the shorter Gemmini → CGRA → AES automatic graph.
- `configs/soc/autolink/gcp.yaml` owns the Gemmini, CGRA, and Pool settings.
- `configs/soc/autolink/res.yaml` owns the residual block settings and automatic graph.
- `configs/kernels/kernel_*_4x4.yaml` owns kernel metadata and execution counts.

Do not restore the deprecated mixed kernel schema or add fallback reads for its old fields. Keep multi-CGRA architecture and SoC settings in their matching files under `configs/arch/` and `configs/soc/`.

The main generation entry points are:

- `scripts/generate_single_cgra.py`
- `scripts/generate_multi_cgra.py`
- `scripts/cgra_fast_api.py`
- `scripts/generate_auto_links.py`
- `scripts/generate_cgra_link_control.py`
- `scripts/generate_gemmini_ext_spm.py`
- `scripts/generate_cgra_spm_window.py`
- `scripts/openfpga/generate.py`

Use the scripts' `--help` output and the current YAML schema instead of copying arguments from old plans or logs.

## Supported systems

### CGRA

Single-CGRA fast APIs support FIR, ReLU, Add+ReLU, GEMV, Histogram, and AXPY. GEMM and SAD are unsupported. Track kernel bugs and validation limitations in GitHub Issues rather than recording them here.

Multi-CGRA tests support homogeneous mesh, 2x2 and 4x4 systolic, scalar FIR, and vector FIR configurations. Their packet headers are fixed, preencoded test inputs. Automatic multi-CGRA control-packet generation is unsupported; do not replace those headers with an ad hoc generator.

The generated fast API is local single-CGRA only. Do not silently extend it to multi-CGRA targets without first defining the packet-encoding contract.

### CGRA + FPGA

The current OpenFPGA integration is a TileLink MMIO fabric flow. It supports AND2, AND2/OR2, bin2bcd, and gcd6 demos with frame-based k4 fabrics. A Chipyard configuration enabling CGRA and OpenFPGA together is not currently supported. Do not describe the existing OpenFPGA-only configs as concurrent CGRA + FPGA systems.

### CGRA + Gemmini

`CGRAMinimalGemminiRocketConfig` combines CGRA and Gemmini. `CGRAMinimalGemminiAESRocketConfig` and `CGRAMinimalGemminiAESAutoLinkRocketConfig` also add AES. CGRA uses `custom0`, AES uses `custom1`, and Gemmini uses `custom3`. Keep the opcodes distinct.

The CPU-mediated Gemmini GEMM to CGRA ReLU demos remain supported. Manual and automatic configurations use the same Gemmini external SPM. `CGRAMinimalGemminiAutoLinkRocketConfig` adds one automatic 128-byte transfer from that SPM to CGRA local SPM. The CGRA DMA reads the Gemmini SPM through TileLink and the system bus; there is no shared staging buffer or intermediate DRAM copy.

The three-IP Manual and Automatic configurations validate one strict sequential pipeline: AES decrypts 256 bytes into the Gemmini shared external SPM, Gemmini runs GEMM with B preloaded by the CPU and publishes 128 bytes at the SPM tail, CGRA pulls the data into its local SPM and runs ReLU, and AES reads the CGRA read-only SPM window and encrypts 128 bytes to DRAM. Manual mode has the CPU start each IP in order. Automatic mode has the CPU preload B, capture the native Gemmini command sequence, and configure the CGRA and both AES jobs; AutoLink uses fixed `aes -> gemmini`, `gemmini -> cgra`, and `cgra -> aes` routes and returns Gemmini, CGRA, and AES destination results. The older Gemmini-to-CGRA and Gemmini-to-CGRA-to-AES demos remain supported.

`CGRAMinimalGemminiPoolRocketConfig` and `CGRAMinimalGemminiPoolAutoLinkRocketConfig` add a `custom2` streaming Pool accelerator. The verified pipeline runs Gemmini INT8 Conv, CGRA INT32 ReLU, and INT32 MaxPool; Pool element width is selected at elaboration from 8, 16, or 32 bits, while mode, shape, kernel, stride, padding, and addresses are runtime fields. Average mode is reserved but unsupported.

`CGRAMinimalGemminiResidualRocketConfig` and `CGRAMinimalGemminiResidualAutoLinkRocketConfig` validate one residual block with two Gemmini jobs and two CGRA jobs. AutoLink models logical stages with fan-out and a two-input join. Each cached CGRA job contains its complete configuration and launch packets. The wrapper resets CGRA execution state between jobs while preserving its data SPM; this is a workaround for missing native VectorCGRA task switching.

AutoLink carries control only. TileLink carries payload data. Routes and copy tasks are fixed during elaboration; runtime programming is unsupported. Hybrid mode, overlap, multiple chunks, arbitrary runtime graphs, and concurrent producers are unsupported.

## Interface contracts

- Single- and multi-CGRA systems share the raw CPU/RoCC packet interface through `custom0`. `tests/include/cgra_runtime.h` is the minimal packet-send layer; multi-CGRA hot paths send their preencoded packets directly.
- The system bus can read and write Gemmini's four-bank shared external SPM. CGRA's TileLink DMA master pulls from it into CGRA local SPM.
- Manual `cgra_dma_mvin_i8_async` reads packed signed INT8 and expands each byte into one INT32 CGRA SPM word. `CGRA_DMA_I8_DESC_CONST` takes the element count; the encoded DMA descriptor counts destination bytes, so N source bytes occupy 4N SPM bytes. The wrapper supports unaligned source addresses and exact byte tails using sub-beat TileLink reads, and reports completion only after native DMA finishes the SPM writes. Packed input bypasses requantization; the existing raw INT32 DMA path remains unchanged. This wrapper-only command does not change VectorCGRA's native packet protocol.
- CGRA exposes one read-only TileLink SPM window at `memory.cgra_spm_window.base_address`. Its manager selects raw data or INT8 quantization at elaboration from the graph's output format; the modes do not coexist. Quantized byte offset `i` reads element `outboundSpmWord + i`, while raw offsets address native SPM bytes. The YAML size describes physical SPM capacity; generated `CGRA_SPM_WINDOW_SIZE_BYTES` describes the visible window, rounded up to a cache block for INT8 with zero padding beyond the tensor. There is no separate packed alias or additional data SPM. Input DMA requantization remains independent, and CGRA computation stays INT32.
- Automatic Gemmini execution captures CPU-issued native RoCC commands in a parameterized wrapper buffer and replays them after `requestCompute`; it does not synthesize a fixed job or modify Gemmini.
- Gemmini and CGRA configuration uses begin followed by native command or packet capture. Hardware completes capture using the declared count; `end()` is an optional query for capture validation, not a commit or execution barrier. MMIO `CONFIG_READY`, `CONFIG_DONE`, `CONFIG_STATUS`, and `CONFIG_DETAIL` describe configuration separately from execution results. Hardware execution does not wait for the CPU to read these fields.
- MMIO configuration APIs use ordered accesses to the same endpoint and poll its acknowledgement; they do not issue a hardware fence, because Rocket fences also wait for RoCC execution. Callers still synchronize payload memory before making data ready or submitting a streaming job.
- AES uses MMIO to select a job and atomically submit its complete descriptor to the adapter cache. Cache depth follows the number of AES stages. Keys, modes, output addresses, and completion addresses are runtime values, not generated hardware constants. Configure jobs before releasing upstream; execution is sequential and active configurations must not be overwritten.
- `AES_SUBMIT` bit 0 commits the selected descriptor; bit 1 also requests root launch. Cache-only configuration does not start AES. The root job starts only after its matching output watch is armed and its destination and byte count match the watched range. It reports output only after output and completion writes drain.
- AES submission returns configuration acceptance status through the same MMIO status fields without an extra `end()` operation. Its complete descriptor sets ready and done together; these fields do not indicate execution completion. The root API combines configuration and launch in one call.
- Downstream AES selects cached configuration by `requestCopy.job`; source address and length come from that request. Generated `AUTO_LINK_JOB_*` constants identify per-IP jobs, distinct from global `AUTO_LINK_STAGE_*` indices.
- Downstream AES starts streaming on `requestCopy`, reports copy completion after all input is read, treats `requestCompute` as a continuation barrier, and reports compute completion after output and completion writes drain.
- Pool configuration writes its parameter registers through RoCC without begin/end commands. Software completes configuration before starting the upstream producer; AutoLink `requestCopy` launches the streaming job, with parameter validation at launch. Manual configuration, start, and wait remain unchanged.
- `CgraLinkEndpoint` uses MMIO for AutoLink configuration and results. CGRA launch packet contents use RoCC.
- Regenerate after switching between scalar and vector layouts; packet width depends on the layout.
