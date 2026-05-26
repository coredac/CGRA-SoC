# CGRA-SoC

This workspace drives the CPU+CGRA integration flow with layered CGRA
configuration files. VectorCGRA generates the CGRA RTL and semantic control
packets; Chipyard hosts the generated RTL behind the RoCC wrapper and runs
baremetal C tests.

## Layout

- `configs/arch/arch.yaml`: CGRA architecture source of truth.
- `configs/soc/cgra_soc.yaml`: SoC interface and memory parameters.
- `configs/kernels/kernel_*_4x4.yaml`: kernel metadata and execution counts only.
- `scripts/generate_single_cgra.py`: generates `CgraTemplateRTL_single` RTL
  from `arch.yaml` and `cgra_soc.yaml`, then syncs it into Chipyard.
- `scripts/generate_multi_cgra.py`: generates `MeshMultiCgraTemplateRTL_multi`
  RTL from `multi_cgra_arch.yaml` and `multi_cgra_soc.yaml`, then syncs it into
  the same Chipyard BlackBox shape.
- `scripts/cgra_fast_api.py`: generates fast-only local single-CGRA headers in
  `tests/generated/cgra_<kernel>_fast_api.h` from arch/soc and kernel YAMLs.
- `tests/`: baremetal CPU+CGRA tests.
- `tests/cgra-gemmini/`: Gemmini + single-CGRA combined demo.
- `tests/include/`: CGRA protocol constants, packet layout, and runtime helpers.
- `VectorCGRA/`: backend CGRA generator and reference from-yaml tests.
- `chipyard/`: SoC integration and Verilator simulator.
- `run-chipyard-cgra-gemmini-demo.sh`: builds/runs the Gemmini + CGRA demo.

The old `cgra-fir-2x2` manual flow and mixed per-kernel hardware YAML flow are
deprecated and are not the validation path.

## Configuration Schema

`configs/arch/arch.yaml` owns CGRA hardware structure:

- `multi_cgra_defaults.rows/columns`
- `cgra_defaults.rows/columns/configMemSize`
- `tile_defaults.num_registers/fu_types`

`configs/soc/cgra_soc.yaml` owns SoC/interface/memory:

- `interface.num_tile_inports`
- `interface.num_tile_outports`
- `interface.num_fu_inports`
- `interface.num_fu_outports`
- `interface.data_nbits`
- `interface.predicate_nbits`
- `memory.data_mem_size_global`
- `memory.data_mem_size_per_bank`
- `memory.num_banks_per_cgra`
- `memory.num_registers_per_reg_bank`
- `memory.mem_access_is_combinational`

Each `configs/kernels/kernel_*_4x4.yaml` only owns kernel-specific metadata and
execution counts:

```yaml
kernel:
  name: fir4x4
  kernel_yaml: VectorCGRA/validation/test/fir4x4.yaml

execution:
  compiled_ii: 5
  loop_times: 170
```

Kernel YAMLs must not contain `cgra`, `interface`, `memory`, `hardware`,
`fu_list`, or `dfg_yaml`. `compiled_ii` and `loop_times` stay in the kernel YAML
because the generated fast packets encode `CGRA_CMD_CONFIG_COUNT_PER_ITER` and
`CGRA_CMD_CONFIG_TOTAL_CTRL_COUNT`.

## Supported Kernels

| Kernel | Config | C test | Notes |
| --- | --- | --- | --- |
| FIR | `configs/kernels/kernel_fir4x4_4x4.yaml` | `cgra-fir-yaml-4x4` | Checks completion/result. |
| ReLU | `configs/kernels/kernel_relu4x4_4x4.yaml` | `cgra-relu4x4` | Checks all output addresses by readback. |
| GEMV | `configs/kernels/kernel_gemv_4x4.yaml` | `cgra-gemv-4x4` | Checks output rows 1..3; skips known row 0 issue. |
| Histogram | `configs/kernels/kernel_histogram_4x4.yaml` | `cgra-histogram-4x4` | Checks bins 1..3; skips known bin 0 issue. |
| AXPY | `configs/kernels/kernel_axpy_4x4.yaml` | `cgra-axpy-4x4` | Checks addr 1..15; skips known addr 0 issue. |

All supported single-CGRA tests use generated fast-only headers. GEMM and SAD
may have local configs/tests or legacy generated files, but they are unsupported
and skipped by the fast API generator.

The supported combined accelerator demo is:

| Demo | Config | C test | Notes |
| --- | --- | --- | --- |
| Gemmini GEMM + CGRA ReLU | `CGRAMinimalGemminiRocketConfig` | `tests/cgra-gemmini/gemmini-gemm-cgra-relu.c` | Gemmini uses `custom3`; CGRA uses `custom0`; data moves through DRAM and CPU-mediated CGRA stores. |

## Set up the environment
See [Setup documentation](./docs/Setup.md) for more details.

## Run Kernels

Generate the canonical CGRA RTL from the layered arch/soc configs, generate
fast-only C APIs for the kernels being tested, then rebuild the Chipyard
simulator:

```bash
python scripts/generate_single_cgra.py --arch-yaml configs/arch/arch.yaml --soc-yaml configs/soc/cgra_soc.yaml
python scripts/cgra_fast_api.py --arch-yaml configs/arch/arch.yaml --soc-yaml configs/soc/cgra_soc.yaml configs/kernels/kernel_<kernel>_4x4.yaml --output-dir tests/generated
./run-chipyard-cgra-test.sh --rebuild <c-test-name>
```

Examples:

```bash
python scripts/cgra_fast_api.py --arch-yaml configs/arch/arch.yaml --soc-yaml configs/soc/cgra_soc.yaml configs/kernels/kernel_fir4x4_4x4.yaml --output-dir tests/generated
./run-chipyard-cgra-test.sh --rebuild cgra-fir-yaml-4x4
```

```bash
python scripts/cgra_fast_api.py --arch-yaml configs/arch/arch.yaml --soc-yaml configs/soc/cgra_soc.yaml configs/kernels/kernel_relu4x4_4x4.yaml --output-dir tests/generated
./run-chipyard-cgra-test.sh --rebuild cgra-relu4x4
```

After rebuilding for the same generated RTL, rerun another C test that targets
that same RTL without `--rebuild`:

```bash
./run-chipyard-cgra-test.sh cgra-relu4x4
```

## Generated C APIs

`scripts/cgra_fast_api.py` is the single-CGRA C API generator. It loads the
layered arch/soc/kernel YAMLs, invokes VectorCGRA `ScriptFactory`, orders the
packets, and emits fast-only headers named
`tests/generated/cgra_<kernel>_fast_api.h`.

The fast API encodes deterministic control/config/launch packets into
`cgra_packet_t` constant arrays at generation time and sends them with
`cgra_send_packets_fast()`, avoiding runtime `build_ctrl()` /
`cgra_build_intra_pkt_to()` / per-bit packet construction for supported
single-CGRA tests.

The generated APIs are local single-CGRA only and are precomputed for
`cgra_target_local()`. They do not generate a target-aware fast variant.
Target-aware and hand-written multi-CGRA tests continue to use
`tests/include/cgra_runtime.h`.

Generated fast API functions follow this split:

```c
load_<kernel>_config_fast();  // non-launch packets only
launch_<kernel>_fast();       // launch packets only
configure_<kernel>_fast();    // config plus launch
<kernel>_store_fast(addr, data);
<kernel>_read_mem_fast(addr);
<kernel>_basic_fast_templates_match_runtime();
```

The old generated semantic functions `configure_<kernel>()` and
`configure_<kernel>_to()` are no longer emitted for supported single-CGRA
kernels. ReLU fast timing lives in the canonical `tests/cgra-relu4x4.c`.

## Gemmini + CGRA Demo

Gemmini support is currently exposed through
`CGRAMinimalGemminiRocketConfig` in
`chipyard/generators/chipyard/src/main/scala/config/RoCCAcceleratorConfigs.scala`.
That config combines `WithCGRA()` and a minimal Gemmini config. The CGRA RoCC
uses `OpcodeSet.custom0`; Gemmini keeps its default `OpcodeSet.custom3`.

The minimal Gemmini config keeps the GEMM path and removes nonessential
features such as training convolutions, max pool, nonlinear activations,
normalizations, depthwise convolutions, loop conv, and scale units. It uses
weight-stationary dataflow, 64 KiB scratchpad, 32 KiB accumulator, 64-byte DMA
max request size, and a 128-bit DMA/system bus width.

The combined demo uses single-CGRA ReLU fast APIs, not the current multi-CGRA
generated top. Regenerate the matching single-CGRA RTL/API before rebuilding:

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

Use `--rebuild` on the first run because Gemmini elaboration must generate a
matching `gemmini_params.h`. Later runs can omit `--rebuild` if the RTL/config
has not changed.

The demo data path is intentionally memory-mediated:

```text
DRAM A/B -> Gemmini scratchpad/accumulator -> DRAM C
  -> CPU -> CGRA fast STORE_REQUEST -> CGRA data memory
  -> CGRA ReLU -> CGRA data memory -> CPU fast readback
```

There is no direct Gemmini-buffer-to-CGRA-buffer connection. The C code uses
low-level Gemmini commands from `gemmini.h`, not `gemmini_loop_ws` or
`tiled_matmul_auto`.

CI initializes the additional Gemmini submodules needed by the demo:
`chipyard/generators/gemmini`,
`chipyard/generators/gemmini/software/gemmini-rocc-tests`, and that test
repository's `rocc-software` and `riscv-tests` submodules.

## Multi-CGRA Flow

The multi-CGRA Chipyard flow uses:

```bash
python scripts/generate_multi_cgra.py --arch-yaml configs/arch/multi_cgra_arch.yaml --soc-yaml configs/soc/multi_cgra_soc.yaml
```

The generated wrapper keeps the same external superset port shape used by
`CGRABlackBox`. For `MeshMultiCgraTemplateRTL_multi`, CPU packets enter through
the single raw `IntraCgraPkt` CPU interface. Inter-CGRA NoC, CGRA ID, and memory
address bounds are handled inside the multi top, so the generated wrapper
absorbs the corresponding external Chipyard ports.

The C runtime still has semantic and target-aware helpers such as
`send_basic_to`, `send_config_to`, `send_prologue_to`, `build_ctrl`, and
`read_mem`. These helpers are kept for hand-written multi-CGRA tests; they are
not a generated per-kernel semantic API.

`VectorCGRA/validation/script_generator.py` is unchanged. The automatic
`scripts/cgra_fast_api.py` path is local single-CGRA only. Automatic
control-signal generation for kernels partitioned across multiple CGRAs is
still TODO. Hand-written multi-CGRA control-signal tests are in
`VectorCGRA/multi_cgra/test/MeshMultiCgraTemplateRTL_test.py`.

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

Multi-CGRA FIR CPU+CGRA tests live under `tests/multi-cgra/fir/`. Their C
control packets are hand-written from
`VectorCGRA/multi_cgra/test/MeshMultiCgraRTL_test.py`; do not regenerate those
control signals with a script. Regenerate the matching RTL/layout before each
test because the scalar and vector FIR layouts use different `data_nbits`.

- `multi-cgra/fir/cgra-multi-fir-scalar`
  - generate: `.venv/bin/python scripts/generate_multi_cgra.py --arch-yaml configs/arch/multi_cgra_fir_2x2_4x4_scalar.yaml --soc-yaml configs/soc/multi_cgra_fir_scalar.yaml`
  - run: `./run-chipyard-cgra-test.sh --rebuild multi-cgra/fir/cgra-multi-fir-scalar`
- `multi-cgra/fir/cgra-multi-fir-scalar-2x2-2x2`
  - generate: `.venv/bin/python scripts/generate_multi_cgra.py --arch-yaml configs/arch/multi_cgra_homo_meshrtl.yaml --soc-yaml configs/soc/multi_cgra_fir_scalar.yaml`
  - run: `./run-chipyard-cgra-test.sh --rebuild multi-cgra/fir/cgra-multi-fir-scalar-2x2-2x2`
- `multi-cgra/fir/cgra-multi-fir-vector`
  - generate: `.venv/bin/python scripts/generate_multi_cgra.py --arch-yaml configs/arch/multi_cgra_fir_2x2_4x4_vector.yaml --soc-yaml configs/soc/multi_cgra_fir_vector.yaml`
  - run: `./run-chipyard-cgra-test.sh --rebuild multi-cgra/fir/cgra-multi-fir-vector`

All three FIR tests check one completion and result `0x8a7` (`2215`). The
vector test uses `data_nbits: 64`, so `tests/include/cgra_runtime.h` encodes
CGRA data payloads with an internal 128-bit word while still sending the same
four 64-bit RoCC raw packet chunks.

## Reference First

When bringing up a kernel, first check the matching VectorCGRA from-yaml test.
For FIR, use `CgraRTL_fir4x4_test_from_yaml.py`; the older
`CgraRTL_fir_test_from_yaml.py` is not the reference for this flow.

```bash
cd VectorCGRA
python -m pytest cgra/test/CgraRTL_fir4x4_test_from_yaml.py -q
```

Then return to the top level and run the CPU+CGRA flow.

## Runtime Notes

The C tests use `tests/include/cgra_protocol.h` for protocol constants,
`tests/include/cgra_runtime.h` for shared packet helpers, generated
`tests/include/cgra_layout.h` for packet field offsets, and generated
`tests/generated/cgra_<kernel>_fast_api.h` headers for supported single-CGRA
kernels.

`<kernel>_read_mem_fast(addr)` sends a fast CGRA load request and reads back
CGRA-side data memory through the RoCC wrapper. This is used by ReLU, GEMV,
Histogram, and AXPY result checks. ReLU currently verifies every address
`0..31` and expects `max(addr - 16, 0)`.

Known first-output issues are preserved in the C tests with comments: GEMV
leaves logical row 0 (`addr20`) incorrect, Histogram leaves logical bin 0
(`addr20`) incorrect, and AXPY leaves physical `addr0` incorrect. Those tests
still require the remaining checked addresses to match.
