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
- `scripts/generate_cgra_c_api.py`: generates semantic C configuration APIs in
  `tests/generated/cgra_<kernel>_api.h`.
- `tests/`: baremetal CPU+CGRA tests.
- `tests/include/`: CGRA protocol constants, packet layout, and runtime helpers.
- `VectorCGRA/`: backend CGRA generator and reference from-yaml tests.
- `chipyard/`: SoC integration and Verilator simulator.

The old `cgra-fir-2x2` flow and mixed per-kernel hardware YAML flow are
deprecated and are not the main validation path.

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
because the generated C API sends them through `CGRA_CMD_CONFIG_COUNT_PER_ITER`
and `CGRA_CMD_CONFIG_TOTAL_CTRL_COUNT`.

## Supported Kernels

| Kernel | Config | C test | Notes |
| --- | --- | --- | --- |
| FIR | `configs/kernels/kernel_fir4x4_4x4.yaml` | `cgra-fir-yaml-4x4` | Checks completion/result. |
| ReLU | `configs/kernels/kernel_relu4x4_4x4.yaml` | `cgra-relu4x4` | Checks all output addresses by readback. |
| GEMV | `configs/kernels/kernel_gemv_4x4.yaml` | `cgra-gemv-4x4` | Checks output rows 1..3; skips known row 0 issue. |
| Histogram | `configs/kernels/kernel_histogram_4x4.yaml` | `cgra-histogram-4x4` | Checks bins 1..3; skips known bin 0 issue. |
| AXPY | `configs/kernels/kernel_axpy_4x4.yaml` | `cgra-axpy-4x4` | Checks addr 1..15; skips known addr 0 issue. |

GEMM and SAD may have local configs/tests, but they are not part of the current
supported CPU+CGRA set unless explicitly promoted.

## Run Kernels

Generate the canonical CGRA RTL from the layered arch/soc configs, generate C
APIs for the kernels being tested, then rebuild the Chipyard simulator:

```bash
python scripts/generate_single_cgra.py --arch-yaml configs/arch/arch.yaml --soc-yaml configs/soc/cgra_soc.yaml
python scripts/generate_cgra_c_api.py --arch-yaml configs/arch/arch.yaml --soc-yaml configs/soc/cgra_soc.yaml configs/kernels/kernel_<kernel>_4x4.yaml --output-dir tests/generated
./run-chipyard-cgra-test.sh --rebuild <c-test-name>
```

Examples:

```bash
python scripts/generate_cgra_c_api.py --arch-yaml configs/arch/arch.yaml --soc-yaml configs/soc/cgra_soc.yaml configs/kernels/kernel_fir4x4_4x4.yaml --output-dir tests/generated
./run-chipyard-cgra-test.sh --rebuild cgra-fir-yaml-4x4
```

```bash
python scripts/generate_cgra_c_api.py --arch-yaml configs/arch/arch.yaml --soc-yaml configs/soc/cgra_soc.yaml configs/kernels/kernel_relu4x4_4x4.yaml --output-dir tests/generated
./run-chipyard-cgra-test.sh --rebuild cgra-relu4x4
```

After rebuilding for the same generated RTL, rerun another C test that targets
that same RTL without `--rebuild`:

```bash
./run-chipyard-cgra-test.sh cgra-relu4x4
```

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

The C runtime has target-aware helpers. Existing `configure_<kernel>()` calls
target CGRA 0. To send the same generated tile configuration to another CGRA,
call the target-aware API, for example:

```c
configure_fir4x4_to(cgra_target_id(2));
```

`VectorCGRA/validation/script_generator.py` is unchanged. The current automatic
path from kernel YAML through `scripts/generate_cgra_c_api.py` is intended for
single-CGRA use, or for sending the same tile configuration to one
`dst_cgra_id`. Automatic control-signal generation for kernels partitioned
across multiple CGRAs is still TODO. Hand-written multi-CGRA control-signal
tests are in `VectorCGRA/multi_cgra/test/MeshMultiCgraTemplateRTL_test.py`.

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
`tests/include/cgra_runtime.h` for packet send/readback helpers, and the
generated `tests/include/cgra_layout.h` for packet field offsets.

`read_mem(addr)` sends a CGRA load request and reads back CGRA-side data memory
through the RoCC wrapper. This is used by ReLU, GEMV, Histogram, and AXPY
result checks. ReLU currently verifies every address `0..31` and expects
`max(addr - 16, 0)`.

Known first-output issues are preserved in the C tests with comments: GEMV
leaves logical row 0 (`addr20`) incorrect, Histogram leaves logical bin 0
(`addr20`) incorrect, and AXPY leaves physical `addr0` incorrect. Those tests
still require the remaining checked addresses to match.
