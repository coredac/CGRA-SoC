# ASIC adapter benchmark

Generate fixtures with `python3 scripts/asic/fixtures.py`. The test includes the default output at `build/experiments/asic/system/fixtures.h`; define `ASIC_FIXTURES` to select another generated header. The test also requires the generated accelerator identities and the ASIC runtime API in `tests/include/asic.h`.

`main.c` runs actual native configuration, DMA and accelerator commands on Rocket. Argument one selects `-1` for both modes, `0` for separate operator submissions, or `1` for a cached job chain. Argument two selects `0` for the five-unit smoke subset or `1` for all fixtures. Each case executes input zero with cold configuration, then input one with the same cached jobs. Both modes use identical arithmetic jobs and local memory placement; only cached next-job links and CPU submission count differ.

The fixtures contain GCN CSR aggregation followed by ReLU, ResNet ReLU and Add followed by ReLU, both GPT LayerNorms, both causal Softmax heads, every existing projection/PV requantization, requantization followed by GELU, and both saturated residual additions. They use two existing reference inputs. Matrix and convolution outputs are supplied as reference intermediates, so this test measures non-matrix kernel chains with real system interfaces, not whole-network latency or Gemmini integration.

One CPU `rdcycle` interval includes configuration when cold, input staging and DMA, execution and waits, final output DMA, and the visibility fence. Checking and printing occur afterward. Transfers are padded to 32-byte boundaries, and reported DMA bytes include that padding. These byte totals count successfully completed software DMA requests, not DRAM controller transactions. Intermediate local memory accesses remain inside the measured execution interval. No reference data is embedded in the hardware.

Passing host fixture generation or C compilation does not establish system validation. The test requires a matching ASIC-enabled SoC rebuild and successful numerical results. It must not be run on an older simulator that lacks the ASIC endpoint.

## Reproduce the primary comparisons

Generate the inputs and matching APIs, then rebuild once for the shared SoC. The remaining C tests reuse that simulator. These commands preserve both backends in the same hardware configuration and run 16 primary cold/warm cases.

```sh
chipyard/.conda-env/bin/python scripts/asic/fixtures.py
chipyard/.conda-env/bin/python scripts/asic/resnet_system.py
chipyard/.conda-env/bin/python scripts/asic/gpt_system.py
chipyard/.conda-env/bin/python scripts/cgra_fast_api.py \
  --arch-yaml configs/arch/attention.yaml --soc-yaml configs/soc/autolink/asic.yaml \
  --output-dir build/experiments/asic/system \
  configs/kernels/kernel_gcn_4x4.yaml \
  configs/kernels/kernel_{relu_runtime,add_relu_runtime}_4x4.yaml \
  configs/kernels/kernel_block_ln_{sum,mean,squares,variance,sqrt,norm}_4x4.yaml \
  configs/kernels/kernel_block_{requant,activation,residual}_4x4.yaml \
  configs/kernels/kernel_attention_{max,exp,norm}_4x4.yaml
LOADMEM=1 CONFIG=MultiAccelRocketConfig ./run-chipyard-cgra-gemmini-demo.sh --rebuild --soc-yaml configs/soc/autolink/asic.yaml --arch-yaml configs/arch/attention.yaml tests/asic/system/compare.c
LOADMEM=1 CONFIG=MultiAccelRocketConfig ./run-chipyard-cgra-gemmini-demo.sh --soc-yaml configs/soc/autolink/asic.yaml --arch-yaml configs/arch/attention.yaml tests/asic/system/resnet_compare.c
LOADMEM=1 BINARY_ARGS='1 0' CONFIG=MultiAccelRocketConfig ./run-chipyard-cgra-gemmini-demo.sh --soc-yaml configs/soc/autolink/asic.yaml --arch-yaml configs/arch/attention.yaml tests/asic/system/gpt_compare.c
LOADMEM=1 BINARY_ARGS='1 1' CONFIG=MultiAccelRocketConfig ./run-chipyard-cgra-gemmini-demo.sh --soc-yaml configs/soc/autolink/asic.yaml --arch-yaml configs/arch/attention.yaml tests/asic/system/gpt_compare.c
```

## Gemmini GCN pipeline

`gemmini.c` computes the complete small GCN layer using actual Gemmini DIM64 projection followed by ASIC CSR aggregation and ReLU. The generator also emits its `gcn.h`: a 16-node graph, 64 input features, 128 output channels, two runtime input tensors, dense weights, and final reference outputs. Projection runs as two 64-column tiles with padded matrix rows. Gemmini publishes the signed 32-bit results directly into the last 8 KiB of its external SPM, and the ASIC DMA reads those physical addresses into its local feature region. The CPU does not supply reference projection values or materialize the intermediate tensor in DRAM.

Argument one selects both modes (`-1`, default), separate ASIC aggregation/ReLU submissions (`0`), or their local job chain (`1`). Both modes execute identical Gemmini commands, ASIC arithmetic and transfers. Cold execution includes descriptor configuration and graph metadata loading; warm execution reuses those descriptors and graph arrays with changed input features. Total CPU cycles include input staging, native setup, projection, the real SPM-to-SPM copy, ASIC execution and final output DMA. Numerical checking occurs afterward. The benchmark prints the copy cost explicitly: being on chip does not make that transfer free.

The generated accelerator identities must provide `GEMMINI0` and `ASIC0`, with sufficient external SPM and the expected DIM64 configuration. The test source has been prepared for integration; only an actual matching simulator run can establish its numerical or performance result.

## AutoLink interface smoke

`auto.c` exercises two real root-to-ASIC DMA edges followed by a cached Add/ReLU chain. It checks both the DMA result returned to host memory and the ASIC publication window, then repeats with changed inputs while retaining configuration. One root submission triggers two native unit starts. The printed cycle interval includes input generation, so this is an interface smoke test rather than a matched performance comparison.

## Matched GCN comparison

`compare.c` runs the same complete small GCN on CGRA and ASIC backends in one SoC. Both use the same Rocket clock, Gemmini projection, input tensors, final reference outputs, physical Gemmini SPM publication, backend input DMA, and final host output boundary. Both submit through the same AutoLink root and drain completion reports before proceeding. Argument one selects both backends (`-1`, default), CGRA (`0`), or ASIC (`1`); argument two selects both modes (`-1`, default), separate jobs (`0`), or a cached chain (`1`).

The CGRA implements aggregation and ReLU with 16 row jobs; the ASIC uses one graph aggregation job and one ReLU job. Separate-job mode therefore submits 16 or two root runs, while chain mode submits one root run for either backend. These are different computation granularities. A root submission is distinct from a native job start: chaining preserves the 16 CGRA row executions or two ASIC operator executions. It does not turn them into one hardware operation. The comparison reports submission counts and configured jobs separately from elapsed cycles.

Cold means configuration-cold: the measured interval includes native setup, descriptor capture, graph loading, and graph metadata DMA. It does not imply a processor cache flush or a reset of the entire SoC. The next input reuses the graph, metadata, and job cache. CGRA rows share one resident static program; different kernels require their own configuration policy. Total timing includes input staging, Gemmini projection, actual SPM-to-SPM transfer, backend execution, final DMA, and visibility fences. Numerical checking and output printing occur afterward. No performance ordering is established until both backends pass on the matching simulator.

## Complete small ResNet and GPT workloads

`resnet.c` implements a complete projected residual block: an 8×8×4 input, first stride-two convolution, ReLU, second convolution, skip projection, and residual Add/ReLU, producing 4×4×8 outputs. Gemmini performs all three convolutions, and the ASIC performs the three vector operations. Separate mode issues three native ASIC starts from the CPU; chain mode issues two CPU submissions while still executing three native jobs. Only the final Add/ReLU pair is chained, since the intervening convolution consumes the first activation. These submissions use the native ASIC API, not the AutoLink root.

ResNet intermediates are materialized in host memory. The CPU converts Gemmini's packed signed bytes to ASIC signed words and converts the first activation back for the next convolution; those conversions and transfers are timed. Final checking uses the full block reference, and no reference intermediate replaces a computed value. This path establishes a whole-block functional and timing boundary; it does not demonstrate an optimized on-chip pipeline.

`gpt.c` implements a complete small GPT block with two tokens, hidden width 128, two attention heads, and feed-forward width 512. Gemmini computes the matrix products; the ASIC computes both LayerNorms, causal Softmax, requantization, GELU, and residual additions. It uses actual computed intermediates throughout. Separate mode submits each native phase, while chain mode groups LayerNorm and Softmax phases and keeps the first feed-forward requantization/GELU intermediate local. Root submissions are not used in this test. Chaining reduces CPU submissions without removing the underlying native phase executions.

GPT timing includes configuration when cold, representation conversion, head packing, DMA, all kernels, and final output materialization; final numerical checking is outside the interval. Other intermediates still pass through host memory, so this is a DRAM-materialized whole-block baseline. Both `resnet.c` and `gpt.c` accept `-1` for both modes, `0` for separate submissions, or `1` for chains, and execute cold and warm inputs.

`resnet_compare.c` pairs the complete CGRA and ASIC block with identical Gemmini work, raw INT32 DMA and CPU representation conversion. Both use fused AddReLU and two native submissions. Argument one selects both backends (`-1`), CGRA (`0`), or ASIC (`1`). CGRA keeps its ReLU and AddReLU programs resident in separate control slots; ASIC retains its two descriptors. This comparison holds fusion constant rather than comparing CGRA fusion against an unfused ASIC.

`gpt_compare.c` pairs both backends using the same AutoLink root submissions, actual Gemmini work and DRAM boundaries. Argument one selects both modes (`-1`), separate phases (`0`), or chains (`1`); argument two selects both backends (`-1`), CGRA (`0`), or ASIC (`1`). CGRA captures complete controls for each phase because its different kernels overlap control slots. Chained execution retains compatible phase results and the FC1 requantization/GELU intermediate locally on either backend. Hardware ASIC counter deltas are sampled outside the timed CPU interval and distinguish actual starts, configuration commands and DMA payload bytes from software submission counts.

Use `LOADMEM=1` with the top-level runner to preload the executable into simulated DRAM before CPU execution. This shortens simulator startup without replacing any timed accelerator data transfers. The comparison programs still require numerical success on the matching SoC before their cycles are treated as performance results.

## Separate Add and ReLU ASICs

`split.c` uses [split.yaml](../../../configs/soc/autolink/split.yaml): `ADD0` and `RELU0` are independent fixed-function ASIC endpoints, each with its own 64 KiB SPM, descriptor configuration, DMA interface, completion handling, and counters. `CGRA0` has 128 KiB SPM and maps AddReLU as one fused kernel. Equal total SPM capacity does not establish equal hardware area. This baseline measures the chosen separate-endpoint architecture; it does not establish that ASICs generally cannot fuse. `resnet_compare.c` retains the fused-ASIC comparison.

The residual pair takes two native CPU submissions on the ASIC side and one on CGRA. After Add completes, RELU0 performs a real DMA read from ADD0's SPM publication window into its own SPM. This peer transfer incurs measured work and moves four bytes per element; it does not require a host-memory intermediate. The full small ResNet uses the same three actual Gemmini convolutions and inputs as `resnet_compare.c`, with three ASIC submissions versus two CGRA submissions, and checks all 128 final outputs. Its other convolution and activation boundaries retain the existing host-memory materialization and representation conversions.

The default fixture is the 128-output small block. `scripts/asic/resnet_system.py --case medium --output <header>` generates the 16×16×8 → 8×8×16 block with 1024 outputs. A C wrapper can define `RESNET_DATA` as the quoted absolute header path before including `split.c`. The `spatial` fixture increases spatial dimensions to 32×32×8 → 16×16×16 (4096 outputs). The current buffers and 4096-word region spacing support up to 4096 outputs. FIRST/ACTIVE storage is reused as LEFT/RIGHT only after the host consumes the first activation; every operation retains distinct input and output regions. Each convolution selects the largest output-row strip fitting native Gemmini scratch and accumulator limits, validates its window, and fences before reusing workspace. The spatial case uses two eight-row strips per convolution. The 8192-output large fixture still needs a different endpoint storage schedule and is rejected by this test. Changing only the fixture and runtime counts reuses the matching simulator.

Argument one selects full ResNet (`0`, default) or an AddReLU micro sweep (`1`). Argument two selects both backends (`-1`, default), CGRA (`0`), or the two ASICs (`1`). For the micro sweep, argument three selects all sizes (`0`, default) or 8, 32, 128, 512, or 1024 elements. Argument four selects CGRA CPU packet submission (`0`, default) or the existing hardware configuration cache (`1`). Cached mode captures the ReLU and AddReLU jobs and a one-stage AutoLink compute graph during cold setup. Each execution selects its job, signals input readiness, consumes completion, and drains the graph. Payload transfers remain identical manual DMA requests in both modes. ASIC uses its existing cached descriptors regardless of this argument. Each case checks two changed inputs. Cold execution includes descriptor or static-program configuration; warm execution reuses it. Cold does not mean CPU-cache-cold or a reset SoC. CGRA uses `CGRA_COLD` for its first physical run, `CGRA_REPEAT` for the warm micro input with the same resident kernel and shape, and `CGRA_SWITCH` after a kernel change or configuration. In CPU packet mode, full ResNet alternates ReLU and AddReLU, so its later executions use `CGRA_SWITCH`. Cached mode replays the captured jobs through the existing adapter. Both backends use matching host staging, input DMA payloads, and final host-output boundaries.

Argument five selects host-materialized intermediates (`0`, default) or on-chip intermediates (`1`, full ResNet only). The latter publishes each native INT8 convolution output into Gemmini's upper SPM half. Both backends use the same Gemmini accumulator conversion to expand signed INT8 to INT32, followed by the same raw endpoint DMA. After the first ReLU, endpoint DMA returns INT32 to Gemmini staging and Gemmini narrows it into the next convolution's INT8 input. This narrowing preserves the reference because that ReLU output lies in 0–127. The convolution working half, four INT8 publication regions and reused INT32 staging region do not overlap. Conversion and DMA completion precede reuse. Inputs and weights still originate in host memory; only intermediate tensor materialization moves on chip. No additional RTL is required.

`host_read` and `host_write` count endpoint DMA payloads to/from host memory, not all Gemmini traffic. `onchip_bytes` counts the four intermediate endpoint DMA payloads (16 bytes per output element); `conversion_bytes` counts three expansions and one narrowing (20 bytes per output element). The separate `peer_bytes` field still counts Add-to-ReLU ASIC traffic. These are software-requested payload counts, not measured total fabric or DRAM transactions. Native convolution publication, input reads and working-memory traffic are additional.

CPU timing includes configuration when cold, staging, computation, waits, transfers, and the final visibility fence. The output separates the full-block macro stages and the residual pair's Add, peer transfer, and ReLU intervals. For CGRA, the `add` field contains the entire fused-kernel interval. `submissions` counts non-matrix endpoint starts only; the identical Gemmini convolution calls are timed in `conv1` and `branches`. Hardware counter snapshots are outside CPU timing and report each ASIC's active cycles, native starts, CPU starts, DMA bytes, and configuration commands. Numerical checking and printing are also outside timing. Results require successful execution on the matching rebuilt SoC.

```sh
chipyard/.conda-env/bin/python scripts/asic/resnet_system.py
chipyard/.conda-env/bin/python scripts/cgra_fast_api.py \
  --arch-yaml configs/arch/attention.yaml --soc-yaml configs/soc/autolink/split.yaml \
  --output-dir build/experiments/asic/system \
  configs/kernels/kernel_{relu_runtime,add_relu_runtime}_4x4.yaml
LOADMEM=1 CONFIG=MultiAccelRocketConfig ./run-chipyard-cgra-gemmini-demo.sh --rebuild --soc-yaml configs/soc/autolink/split.yaml --arch-yaml configs/arch/attention.yaml tests/asic/system/split.c
LOADMEM=1 BINARY_ARGS='1' CONFIG=MultiAccelRocketConfig ./run-chipyard-cgra-gemmini-demo.sh --soc-yaml configs/soc/autolink/split.yaml --arch-yaml configs/arch/attention.yaml tests/asic/system/split.c
LOADMEM=1 BINARY_ARGS='0 -1 0 1' CONFIG=MultiAccelRocketConfig ./run-chipyard-cgra-gemmini-demo.sh --soc-yaml configs/soc/autolink/split.yaml --arch-yaml configs/arch/attention.yaml tests/asic/system/split.c
LOADMEM=1 BINARY_ARGS='0 -1 0 1 1' CONFIG=MultiAccelRocketConfig ./run-chipyard-cgra-gemmini-demo.sh --soc-yaml configs/soc/autolink/split.yaml --arch-yaml configs/arch/attention.yaml tests/asic/system/split.c
```
