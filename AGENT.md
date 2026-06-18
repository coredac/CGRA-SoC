# CGRA-SoC Agent Notes

This repository is the top-level integration workspace for VectorCGRA RTL
generation and Chipyard CPU+CGRA simulation.

## Current State

The active flow uses layered configuration files, not mixed per-kernel hardware
YAMLs:

- `configs/arch/arch.yaml`: CGRA architecture source of truth.
- `configs/soc/cgra_soc.yaml`: SoC interface and memory source of truth.
- `configs/kernels/kernel_*_4x4.yaml`: kernel metadata plus execution counts only.

The old `cgra-fir-2x2` path and the old mixed/unified kernel YAML schema are
deprecated and should not be used as defaults or fallback paths.

Multi-CGRA RTL/Chipyard generation is available through a separate flow. It
uses `configs/arch/multi_cgra_arch.yaml` and
`configs/soc/multi_cgra_soc.yaml`; do not fold those fields into the single-CGRA
arch/soc files unless the task explicitly asks for a shared schema change.

Currently supported CPU+CGRA kernels:

| Kernel | Kernel config | C test | Generated API | Validation status |
| --- | --- | --- | --- | --- |
| FIR | `configs/kernels/kernel_fir4x4_4x4.yaml` | `cgra-fir-yaml-4x4` | `tests/generated/cgra_fir4x4_fast_api.h` | PASS, checks completion/result. |
| ReLU | `configs/kernels/kernel_relu4x4_4x4.yaml` | `cgra-relu4x4` | `tests/generated/cgra_relu4x4_fast_api.h` | PASS, checks all output addresses by readback and reports fast timing. |
| GEMV | `configs/kernels/kernel_gemv_4x4.yaml` | `cgra-gemv-4x4` | `tests/generated/cgra_gemv_fast_api.h` | PASS with known row-0 skip. |
| Histogram | `configs/kernels/kernel_histogram_4x4.yaml` | `cgra-histogram-4x4` | `tests/generated/cgra_histogram_fast_api.h` | PASS with known bin-0 skip. |
| AXPY | `configs/kernels/kernel_axpy_4x4.yaml` | `cgra-axpy-4x4` | `tests/generated/cgra_axpy_fast_api.h` | PASS with known addr0 skip. |

Do not treat GEMM or SAD as supported just because local configs/tests may
exist. They are unsupported, skipped by `scripts/cgra_fast_api.py`, and should
not receive fast API headers unless explicitly promoted.

Current Gemmini support is a combined demo, not a general Gemmini benchmark
suite:

| Demo | Config | C test | Status |
| --- | --- | --- | --- |
| Gemmini GEMM + single-CGRA ReLU | `CGRAMinimalGemminiRocketConfig` | `tests/cgra-gemmini/gemmini-gemm-cgra-relu.c` | PASS in the saved timing run. |

Gemmini uses `OpcodeSet.custom3`; CGRA uses `OpcodeSet.custom0`. Keep those
opcodes distinct. The demo data path is:

```text
DRAM A/B -> Gemmini scratchpad/accumulator -> DRAM C
  -> CPU -> CGRA fast STORE_REQUEST -> CGRA data memory
  -> CGRA ReLU -> CGRA data memory -> CPU fast readback
```

Do not assume direct Gemmini buffer to CGRA data-memory connectivity.

Current OpenFPGA support is an MMIO fabric demo, not a general FPGA management
stack:

| Demo | Config | C test | Status |
| --- | --- | --- | --- |
| OpenFPGA AND2 | `OpenFPGADemoRocketConfig` | `tests/fpga/openfpga-and2.c` | PASS in Verilator. |
| OpenFPGA AND2_OR2 | `OpenFPGAAnd2Or2K4FrameRocketConfig` | `tests/fpga/openfpga-and2-or2-k4-frame.c` | PASS in Verilator. |
| OpenFPGA bin2bcd | `OpenFPGABin2BcdK4FrameRocketConfig` | `tests/fpga/openfpga-bin2bcd-k4-frame.c` | PASS in Verilator. |
| OpenFPGA gcd6 | `OpenFPGAGcd6K4FrameRocketConfig` | `tests/fpga/openfpga-gcd6-k4-frame.c` | PASS in Verilator. |

OpenFPGA source configs live under `configs/openfpga/`. `openfpga_and2.yaml`
and `openfpga_and2_or2_k4_frame.yaml` are single-benchmark configs.
`openfpga_fabric_k4_frame_4x4_w40.yaml` is a shared k4 frame-based fabric config
with selectable `bin2bcd` and `gcd6` benchmarks. Current support is frame-based
k4 fabric only; scan-chain configs are not supported in the Chipyard backend.

`scripts/openfpga/generate.py` runs the local OpenFPGA flow, extracts the
formal netlist user-interface layout and pin map, parses and prepackages the
bitstream, syncs generated RTL into Chipyard, and emits generated Scala/C
collateral. Do not hand-edit the generated OpenFPGA wrapper, generated Scala
metadata, or generated C headers as a lasting fix; regenerate them from the YAML
instead.

The Chipyard-side OpenFPGA peripheral is TileLink MMIO, not RoCC. The current
register contract is:

- `0x00`: control, write bit 0 to clear programming state.
- `0x08`: status, bit 0 is programmed, bit 1 is config-active, and the high bits hold `cfgCount`.
- `0x10`: `CFG_WORD`, a prepacked frame-based config word.
- `0x20`: packed `USER_INPUT`.
- `0x28`: packed `USER_OUTPUT`.

OpenFPGA bitstream words must remain prepacked by the generator. Runtime tests
should only stream the generated word array to `CFG_WORD`; do not rebuild address
and data packets in the C test hot path.

The current Verilator-compatible OpenFPGA path uses the standard-cell mux
architecture and the OpenFPGA cell-library `inv.v`, `buf4.v`, and `tap_buf4.v`
models in the Chipyard manifest. Do not restore the removed Verilator shim
backend or a generated Verilator replacement tree unless the task explicitly
changes this policy.

## Configuration Rules

`configs/arch/arch.yaml` owns CGRA hardware structure:

- `multi_cgra_defaults.rows/columns`
- `cgra_defaults.rows/columns/configMemSize`
- `tile_defaults.num_registers/fu_types`

`configs/soc/cgra_soc.yaml` owns SoC/interface/memory:

- `interface.*`
- `memory.*`

Kernel YAMLs own only:

- `kernel.name`
- `kernel.kernel_yaml`
- `execution.compiled_ii`
- `execution.loop_times`

Do not reintroduce `cgra`, `interface`, `memory`, `hardware`, `fu_list`, or
`dfg_yaml` into cleaned kernel YAMLs. Do not add hidden fallback logic that
reads old mixed schema fields. FU availability comes from
`arch.yaml.tile_defaults.fu_types`.

`execution.compiled_ii` and `execution.loop_times` remain in the kernel YAML
because the generated fast packets encode:

- `CGRA_CMD_CONFIG_COUNT_PER_ITER`
- `CGRA_CMD_CONFIG_TOTAL_CTRL_COUNT`

## Kernel Bring-Up Rule

For every kernel, use this order:

1. Run the VectorCGRA from-yaml reference test.
2. Generate single-CGRA RTL from `configs/arch/arch.yaml` and `configs/soc/cgra_soc.yaml`.
3. Generate the fast-only C API header from arch/soc plus the kernel YAML.
4. Rebuild the Chipyard simulator and run the matching C test.

Changing only the kernel YAML execution counts or generated C test headers does
not change the CGRA RTL. Changing generated RTL, the RoCC wrapper, or generated
Chipyard Scala resources requires `--rebuild`.

## Main Commands

From the top-level repository:

```bash
python scripts/generate_single_cgra.py --arch-yaml configs/arch/arch.yaml --soc-yaml configs/soc/cgra_soc.yaml
python scripts/generate_multi_cgra.py --arch-yaml configs/arch/multi_cgra_arch.yaml --soc-yaml configs/soc/multi_cgra_soc.yaml
python scripts/cgra_fast_api.py --arch-yaml configs/arch/arch.yaml --soc-yaml configs/soc/cgra_soc.yaml configs/kernels/kernel_<kernel>_4x4.yaml --output-dir tests/generated
./run-chipyard-cgra-test.sh --rebuild <c-test-name>
python scripts/openfpga/generate.py --config configs/openfpga/openfpga_fabric_k4_frame_4x4_w40.yaml --benchmark gcd6
CONFIG=OpenFPGAGcd6K4FrameRocketConfig ./run-chipyard-openfpga-demo.sh --rebuild openfpga-gcd6-k4-frame
```

Concrete examples:

```bash
python scripts/cgra_fast_api.py --arch-yaml configs/arch/arch.yaml --soc-yaml configs/soc/cgra_soc.yaml configs/kernels/kernel_fir4x4_4x4.yaml --output-dir tests/generated
./run-chipyard-cgra-test.sh --rebuild cgra-fir-yaml-4x4
```

```bash
python scripts/cgra_fast_api.py --arch-yaml configs/arch/arch.yaml --soc-yaml configs/soc/cgra_soc.yaml configs/kernels/kernel_relu4x4_4x4.yaml --output-dir tests/generated
./run-chipyard-cgra-test.sh --rebuild cgra-relu4x4
```

After rebuilding for the same generated RTL, rerun compatible C tests without
`--rebuild`.

Gemmini + CGRA demo flow:

```bash
.venv/bin/python scripts/generate_single_cgra.py \
  --kernel-yaml configs/kernels/kernel_relu4x4_4x4.yaml \
  --arch-yaml configs/arch/arch.yaml \
  --soc-yaml configs/soc/cgra_soc.yaml
.venv/bin/python scripts/cgra_fast_api.py \
  --arch-yaml configs/arch/arch.yaml \
  --soc-yaml configs/soc/cgra_soc.yaml \
  configs/kernels/kernel_relu4x4_4x4.yaml \
  --output-dir tests/generated
./run-chipyard-cgra-gemmini-demo.sh --rebuild
```

`--rebuild` is required for the first Gemmini run after config changes because
Chipyard elaboration regenerates the matching
`chipyard/generators/gemmini/software/gemmini-rocc-tests/include/gemmini_params.h`.
The demo uses low-level Gemmini commands from `gemmini.h`; do not replace them
with `gemmini_loop_ws`, `tiled_matmul_auto`, or other loop/CISC helpers unless
the task explicitly changes the demo scope.

The Gemmini config lives in
`chipyard/generators/chipyard/src/main/scala/config/RoCCAcceleratorConfigs.scala`
as `CGRAMinimalGemminiRocketConfig`. It combines `WithCGRA()` with
`gemmini.DefaultGemminiConfig(...)`, uses weight-stationary dataflow, disables
training convs, max pool, nonlinear activations, depthwise convs,
normalizations, first-layer optimizations, loop conv, and scale units, and sets
64 KiB scratchpad, 32 KiB accumulator, `dma_maxbytes = 64`, and
`dma_buswidth = 128`.

The top-level CI setup must initialize Gemmini dependencies in addition to the
usual Chipyard submodules: `generators/gemmini`, Gemmini's
`software/gemmini-rocc-tests`, and that repository's `rocc-software` and
`riscv-tests` submodules. If `chipyard/generators/gemmini` already exists and
is a valid submodule, do not reclone it.

## Generated C API Mode

`scripts/cgra_fast_api.py` is the only generated single-CGRA C API entry point.
It owns YAML loading, packet type construction, VectorCGRA `ScriptFactory`
invocation, packet ordering, and header emission. It only supports FIR, ReLU,
GEMV, Histogram, and AXPY; GEMM and SAD are unsupported/skipped.

The generator writes `tests/generated/cgra_<kernel>_fast_api.h`. It precomputes
deterministic control/config/launch packets into `cgra_packet_t` constants and
emits:

```c
load_<kernel>_config_fast();
launch_<kernel>_fast();
configure_<kernel>_fast();
<kernel>_store_fast(addr, data);
<kernel>_read_mem_fast(addr);
```

`configure_<kernel>_fast()` is just config plus launch. Fast APIs are local
single-CGRA only and precomputed for `cgra_target_local()`; do not add a
target-aware fast variant until its encoding contract is explicitly designed.

The fast runtime helpers use the `_fast` suffix, for example
`cgra_send_packet_fast()` and `cgra_send_packets_fast()`. For the current
189-bit intra-CGRA packet layout, the fast helper sends LO/MID/HI and skips
TOP; the old `cgra_send_packet()` still sends all four chunks for compatibility.

`tests/include/cgra_runtime.h` is now the minimal direct packet send API. It
should contain only `cgra_packet_t`, `cgra_send_packet()`,
`cgra_send_packets()`, `cgra_send_packet_fast()`, `cgra_send_packets_fast()`,
the required includes, and the packet-width guard. Do not re-add semantic
packet builders to this header.

The old per-kernel semantic generator and generated functions
`configure_<kernel>()` / `configure_<kernel>_to()` are removed for supported
single-CGRA kernels. Supported single-CGRA fast tests should include only
`tests/include/cgra_runtime.h` and generated fast headers.

## Multi-CGRA Flow

Single and multi flows share the CPU/RoCC raw packet interface. The CPU sends
raw `IntraCgraPkt` packets in both modes. Generated fast APIs are local
single-CGRA only. Supported hand-written multi-CGRA tests now use preencoded
direct packet headers and send `cgra_packet_t` constants; target CGRA fields
are already encoded in those constants.

Important RTL contract:

- Single `CgraTemplateRTL` instantiation must explicitly pass
  `is_multi_cgra=False` and `cgra_id=0`.
- Multi `MeshMultiCgraTemplateRTL` instantiation must explicitly pass
  `is_multi_cgra=True`.
- The CPU-to-`cgra[0]` gateway, `dst_cgra_id` decoding, inter-CGRA routing, and
  remote memory routing are VectorCGRA RTL capabilities. Do not reimplement
  those mechanisms in Chipyard or the Python orchestration scripts.

The multi generation entry point is:

```bash
python scripts/generate_multi_cgra.py --arch-yaml configs/arch/multi_cgra_arch.yaml --soc-yaml configs/soc/multi_cgra_soc.yaml
```

That script only orchestrates generation. It calls the VectorCGRA-level entry
`VectorCGRA/multi_cgra/test/MeshMultiCgraTemplateRTL_multi_test.py`, which
parses multi-CGRA arch/soc YAMLs, constructs the `id2*` parameter maps, and
instantiates `MeshMultiCgraTemplateRTL` with `is_multi_cgra=True`.

Chipyard `CGRABlackBox` intentionally remains a superset interface with CPU
packet ports, inter-CGRA NoC ports, `cgra_id`, address range ports, and optional
boundary ports. `scripts/sync_cgra_blackbox.py` generates a wrapper with that
same external shape for both single and multi tops. If an internal top does not
expose a superset port, the wrapper leaves the input unused or ties the external
output low.

Do not put supported multi-CGRA test hot paths back on the semantic runtime.
The direct multi test `.c` files should include `tests/include/cgra_runtime.h`
and their matching packet header. They should call only
`cgra_send_packet_fast()` / `cgra_send_packets_fast()` for CGRA packets. For
readback, send the preencoded `CGRA_CMD_LOAD_REQUEST` packet and then call
`CGRA_LOAD_RESULT(result)`.

Forbidden hot-path helper names in supported multi-CGRA direct tests:

```bash
build_ctrl
cgra_build_ctrl
cgra_build_intra_pkt
cgra_pkt_set_bits
cgra_ctrl_set_bits
send_basic
send_basic_to
send_config
send_config_to
send_prologue
send_prologue_to
read_mem
cgra_read_mem
```

Use this check after editing multi-CGRA tests:

```bash
rg -n "build_ctrl|cgra_build_ctrl|cgra_build_intra_pkt|cgra_pkt_set_bits|cgra_ctrl_set_bits|send_basic|send_basic_to|send_config|send_config_to|send_prologue|send_prologue_to|read_mem|cgra_read_mem" tests/multi-cgra
```

This check should not report hits in supported direct multi-CGRA tests or their
packet headers.

`VectorCGRA/validation/script_generator.py` is unchanged. The current automatic
`scripts/cgra_fast_api.py` path is intended for local single-CGRA fast headers.
True automatic control signal generation across multiple CGRAs is still TODO.
For hand-written multi-CGRA control signal tests, use
`VectorCGRA/multi_cgra/test/MeshMultiCgraTemplateRTL_test.py` as the reference.

Current supported multi-CGRA CPU+CGRA tests:

- `multi-cgra/cgra-multi-homo`
  - generate: `python scripts/generate_multi_cgra.py --arch-yaml configs/arch/multi_cgra_homo_meshrtl.yaml --soc-yaml configs/soc/multi_cgra_homo_meshrtl.yaml`
  - run: `./run-chipyard-cgra-test.sh --rebuild multi-cgra/cgra-multi-homo`
- `multi-cgra/cgra-multi-systolic-2x2`
  - generate: `python scripts/generate_multi_cgra.py --arch-yaml configs/arch/multi_cgra_homo_meshrtl.yaml --soc-yaml configs/soc/multi_cgra_systolic_2x2.yaml`
  - run: `./run-chipyard-cgra-test.sh --rebuild multi-cgra/cgra-multi-systolic-2x2`
- `multi-cgra/cgra-multi-systolic-4x4`
  - generate: `python scripts/generate_multi_cgra.py --arch-yaml configs/arch/multi_cgra_4x4_meshrtl.yaml --soc-yaml configs/soc/multi_cgra_systolic_4x4_2x2.yaml`
  - run: `./run-chipyard-cgra-test.sh --rebuild multi-cgra/cgra-multi-systolic-4x4`

Multi-CGRA FIR tests:

- `multi-cgra/fir/cgra-multi-fir-scalar`
  - generate: `.venv/bin/python scripts/generate_multi_cgra.py --arch-yaml configs/arch/multi_cgra_fir_2x2_4x4_scalar.yaml --soc-yaml configs/soc/multi_cgra_fir_scalar.yaml`
  - run: `./run-chipyard-cgra-test.sh --rebuild multi-cgra/fir/cgra-multi-fir-scalar`
- `multi-cgra/fir/cgra-multi-fir-scalar-2x2-2x2`
  - generate: `.venv/bin/python scripts/generate_multi_cgra.py --arch-yaml configs/arch/multi_cgra_homo_meshrtl.yaml --soc-yaml configs/soc/multi_cgra_fir_scalar.yaml`
  - run: `./run-chipyard-cgra-test.sh --rebuild multi-cgra/fir/cgra-multi-fir-scalar-2x2-2x2`
- `multi-cgra/fir/cgra-multi-fir-vector`
  - generate: `.venv/bin/python scripts/generate_multi_cgra.py --arch-yaml configs/arch/multi_cgra_fir_2x2_4x4_vector.yaml --soc-yaml configs/soc/multi_cgra_fir_vector.yaml`
  - run: `./run-chipyard-cgra-test.sh --rebuild multi-cgra/fir/cgra-multi-fir-vector`

The multi-CGRA direct packet headers are hand-written/frozen packet sources,
translated from `VectorCGRA/multi_cgra/test/MeshMultiCgraRTL_test.py`. Do not
overwrite them with an ad hoc script unless the task explicitly says to redo the
migration. If adding a supported multi-CGRA test, add a preencoded packet header
and send those packets directly. Always regenerate the intended FIR RTL/layout
before rebuilding, especially before switching between scalar `data_nbits: 32`
and vector `data_nbits: 64`. All three FIR tests currently PASS with expected
result `0x8a7` and one completion.

Vector FIR needs the YAML FU names `vector_mul_combo`, `vector_adder_combo`,
and `vector_all_reduce` mapped in `VectorCGRA/cgra/CgraTemplateRTL.py`. Keep
`vfmul` mapped to `None`; do not add class-name aliases unless the YAML schema
explicitly changes.

## VectorCGRA Reference Tests

Run these before debugging the CPU+CGRA integration:

```bash
cd VectorCGRA
python -m pytest cgra/test/CgraRTL_fir4x4_test_from_yaml.py -q
python -m pytest cgra/test/CgraRTL_relu4x4_test_from_yaml.py -q
python -m pytest cgra/test/CgraRTL_gemv_test_from_yaml.py -q
python -m pytest cgra/test/CgraRTL_histogram_test_from_yaml.py -q
python -m pytest cgra/test/CgraRTL_axpy_test_from_yaml.py -q
```

Important FIR note: use `CgraRTL_fir4x4_test_from_yaml.py`. The older
`CgraRTL_fir_test_from_yaml.py` is known not to be the reference for this flow.

## First-Output Known Issues

Some VectorCGRA from-yaml kernels complete but leave the first logical output
incorrect. The CPU+CGRA C tests intentionally skip only that known first output
and still verify the remaining outputs:

- GEMV: skips logical `y[0]` at `addr20`, checks `addr21..23`.
- Histogram: skips logical bin 0 at `addr20`, checks `addr21..23`.
- AXPY: skips physical `addr0`, checks `addr1..15`.

ReLU verifies every address `0..31` by `relu4x4_read_mem_fast(addr)` and
expects `max(addr - 16, 0)`. FIR currently checks completion count and scalar
result rather than memory readback.

If one of these skipped addresses is fixed upstream, update the C test to check
it and remove the explanatory comment in the test.

## Key Entry Points

- RTL generation:
  [scripts/generate_single_cgra.py](/mnt/public/sichuan_a/qjj/CGRA-SoC/scripts/generate_single_cgra.py:1)
- Multi RTL generation:
  [scripts/generate_multi_cgra.py](/mnt/public/sichuan_a/qjj/CGRA-SoC/scripts/generate_multi_cgra.py:1)
- VectorCGRA multi arch/soc RTL loader:
  [VectorCGRA/multi_cgra/test/MeshMultiCgraTemplateRTL_multi_test.py](/mnt/public/sichuan_a/qjj/CGRA-SoC/VectorCGRA/multi_cgra/test/MeshMultiCgraTemplateRTL_multi_test.py:1)
- Fast single-CGRA C API generation:
  [scripts/cgra_fast_api.py](/mnt/public/sichuan_a/qjj/CGRA-SoC/scripts/cgra_fast_api.py:1)
- VectorCGRA arch/soc RTL loader:
  [VectorCGRA/cgra/test/CgraTemplateRTL_single_test.py](/mnt/public/sichuan_a/qjj/CGRA-SoC/VectorCGRA/cgra/test/CgraTemplateRTL_single_test.py:1)
- Chipyard CGRA RoCC wrapper:
  [chipyard/generators/chipyard/src/main/scala/example/CGRA.scala](/mnt/public/sichuan_a/qjj/CGRA-SoC/chipyard/generators/chipyard/src/main/scala/example/CGRA.scala:1)
- Generated Chipyard parameters:
  [chipyard/generators/chipyard/src/main/scala/example/CGRAGenerated.scala](/mnt/public/sichuan_a/qjj/CGRA-SoC/chipyard/generators/chipyard/src/main/scala/example/CGRAGenerated.scala:1)
- Top-level runner:
  [run-chipyard-cgra-test.sh](/mnt/public/sichuan_a/qjj/CGRA-SoC/run-chipyard-cgra-test.sh:1)
- Gemmini + CGRA runner:
  [run-chipyard-cgra-gemmini-demo.sh](/mnt/public/sichuan_a/qjj/CGRA-SoC/run-chipyard-cgra-gemmini-demo.sh:1)
- Gemmini + CGRA demo source:
  [tests/cgra-gemmini/gemmini-gemm-cgra-relu.c](/mnt/public/sichuan_a/qjj/CGRA-SoC/tests/cgra-gemmini/gemmini-gemm-cgra-relu.c:1)

## RoCC Host Interface

The CGRA is attached as a RoCC accelerator via `custom0`. The wrapper is a raw
packet injector plus completion/readback tracker.

Current funct encodings:

- `2`: `STATUS`
- `4`: `WAIT`
- `5`: `RAW_PKT_LO`
- `6`: `RAW_PKT_MID`
- `7`: `RAW_PKT_HI`
- `8`: `SET_EXPECTED_COMPLETES`
- `9`: `RESULT`
- `10`: `RAW_PKT_TOP`

`tests/include/cgra_runtime.h` provides only direct raw-packet send helpers.
Generated supported single-CGRA tests use the per-kernel fast `*_store_fast` and
`*_read_mem_fast` helpers. Supported multi-CGRA tests use fixed packet arrays in
their direct packet headers. `CGRA.scala` captures the matching CPU-destined
`CMD_LOAD_RESPONSE`.

The RoCC raw packet interface still sends up to four 64-bit chunks: LO, MID,
HI, and TOP. Do not use a 64-bit generated layout for 32-bit scalar tests;
regenerate the matching layout first.

## Generated Files

`scripts/generate_single_cgra.py` updates:

- `VectorCGRA/CgraTemplateRTL_single__pickled.v`
- `chipyard/generators/chipyard/src/main/resources/vsrc/CgraTemplateRTL_single__pickled.v`
- `chipyard/generators/chipyard/src/main/resources/vsrc/CgraTemplateRTL_single_wrapper.v`
- `chipyard/generators/chipyard/src/main/scala/example/CGRAGenerated.scala`
- `tests/include/cgra_layout.h`

`scripts/generate_multi_cgra.py` updates:

- `VectorCGRA/MeshMultiCgraTemplateRTL_multi__pickled.v`
- `chipyard/generators/chipyard/src/main/resources/vsrc/MeshMultiCgraTemplateRTL_multi__pickled.v`
- `chipyard/generators/chipyard/src/main/resources/vsrc/MeshMultiCgraTemplateRTL_multi_wrapper.v`
- `chipyard/generators/chipyard/src/main/scala/example/CGRAGenerated.scala`
- `tests/include/cgra_layout.h`

`scripts/cgra_fast_api.py` updates:

- `tests/generated/cgra_<kernel>_fast_api.h`

Generated supported kernel headers contain local single-CGRA fast packet APIs
only. Do not hand-edit generated headers or generated RTL/Scala resources
unless you are deliberately debugging generator output. Regenerate from the YAML
instead.

## Practical Guidance

- Keep `configs/arch/arch.yaml` and `configs/soc/cgra_soc.yaml` as the hardware/SoC
  source of truth.
- Keep `configs/kernels/kernel_*_4x4.yaml` limited to kernel metadata and execution
  counts.
- When a CPU+CGRA test fails, first confirm the matching VectorCGRA from-yaml
  test behavior and whether the first-output skip applies.
- If generated RTL or Chipyard `CGRA.scala` changes, use `--rebuild`.
- If only C test code or generated fast C API headers change and the active RTL is
  unchanged, rerun without `--rebuild`.
- Avoid fixed packet-width assumptions in runtime code. Use
  `tests/include/cgra_layout.h` and the minimal direct
  `tests/include/cgra_runtime.h`. For supported multi-CGRA tests, keep packet
  widths encoded in their matching direct packet headers and regenerate the
  intended layout before running.
- Remember that generated RTL/layout/Chipyard resources describe whichever
  single or multi generator ran last. Regenerate the intended top before
  rebuilding or running tests.
