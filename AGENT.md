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
| FIR | `configs/kernels/kernel_fir4x4_4x4.yaml` | `cgra-fir-yaml-4x4` | `tests/generated/cgra_fir4x4_api.h` | PASS, checks completion/result. |
| ReLU | `configs/kernels/kernel_relu4x4_4x4.yaml` | `cgra-relu4x4` | `tests/generated/cgra_relu4x4_api.h` | PASS, checks all output addresses by readback. |
| GEMV | `configs/kernels/kernel_gemv_4x4.yaml` | `cgra-gemv-4x4` | `tests/generated/cgra_gemv_api.h` | PASS with known row-0 skip. |
| Histogram | `configs/kernels/kernel_histogram_4x4.yaml` | `cgra-histogram-4x4` | `tests/generated/cgra_histogram_api.h` | PASS with known bin-0 skip. |
| AXPY | `configs/kernels/kernel_axpy_4x4.yaml` | `cgra-axpy-4x4` | `tests/generated/cgra_axpy_api.h` | PASS with known addr0 skip. |

Do not treat GEMM or SAD as supported just because local configs/tests may
exist. They are not in the current supported CPU+CGRA set unless explicitly
promoted.

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
because the generated C API sends them through:

- `CGRA_CMD_CONFIG_COUNT_PER_ITER`
- `CGRA_CMD_CONFIG_TOTAL_CTRL_COUNT`

## Kernel Bring-Up Rule

For every kernel, use this order:

1. Run the VectorCGRA from-yaml reference test.
2. Generate single-CGRA RTL from `configs/arch/arch.yaml` and `configs/soc/cgra_soc.yaml`.
3. Generate the semantic C API header from arch/soc plus the kernel YAML.
4. Rebuild the Chipyard simulator and run the matching C test.

Changing only the kernel YAML execution counts or generated C test headers does
not change the CGRA RTL. Changing generated RTL, the RoCC wrapper, or generated
Chipyard Scala resources requires `--rebuild`.

## Main Commands

From the top-level repository:

```bash
python scripts/generate_single_cgra.py --arch-yaml configs/arch/arch.yaml --soc-yaml configs/soc/cgra_soc.yaml
python scripts/generate_multi_cgra.py --arch-yaml configs/arch/multi_cgra_arch.yaml --soc-yaml configs/soc/multi_cgra_soc.yaml
python scripts/generate_cgra_c_api.py --arch-yaml configs/arch/arch.yaml --soc-yaml configs/soc/cgra_soc.yaml configs/kernels/kernel_<kernel>_4x4.yaml --output-dir tests/generated
./run-chipyard-cgra-test.sh --rebuild <c-test-name>
```

Concrete examples:

```bash
python scripts/generate_cgra_c_api.py --arch-yaml configs/arch/arch.yaml --soc-yaml configs/soc/cgra_soc.yaml configs/kernels/kernel_fir4x4_4x4.yaml --output-dir tests/generated
./run-chipyard-cgra-test.sh --rebuild cgra-fir-yaml-4x4
```

```bash
python scripts/generate_cgra_c_api.py --arch-yaml configs/arch/arch.yaml --soc-yaml configs/soc/cgra_soc.yaml configs/kernels/kernel_relu4x4_4x4.yaml --output-dir tests/generated
./run-chipyard-cgra-test.sh --rebuild cgra-relu4x4
```

After rebuilding for the same generated RTL, rerun compatible C tests without
`--rebuild`.

## Multi-CGRA Flow

Single and multi flows share the CPU/RoCC raw packet interface. The CPU sends
raw `IntraCgraPkt` packets in both modes. In single-CGRA mode, existing APIs
default to `dst_cgra_id = 0`. In multi-CGRA mode, software selects the target
CGRA by setting the packet target fields through the C runtime helpers.

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

The C runtime has target-aware helpers while preserving old signatures:

```c
configure_fir4x4_to(cgra_target_id(2));
```

`configure_<kernel>()`, `send_basic`, `send_config`, `send_prologue`, and
`read_mem(addr)` keep local single-CGRA behavior. `read_mem(addr)` remains
target-local; remote memory routing is selected by `data_addr` in RTL.

`VectorCGRA/validation/script_generator.py` is unchanged. The current automatic
kernel-YAML-to-C-control-API path is intended for single-CGRA or for sending the
same tile configuration to one selected `dst_cgra_id`. True automatic control
signal generation across multiple CGRAs is still TODO. For hand-written
multi-CGRA control signal tests, use
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

ReLU verifies every address `0..31` by `read_mem(addr)` and expects
`max(addr - 16, 0)`. FIR currently checks completion count and scalar result
rather than memory readback.

If one of these skipped addresses is fixed upstream, update the C test to check
it and remove the explanatory comment in the test.

## Key Entry Points

- RTL generation:
  [scripts/generate_single_cgra.py](/mnt/public/sichuan_a/qjj/CGRA-SoC/scripts/generate_single_cgra.py:1)
- Multi RTL generation:
  [scripts/generate_multi_cgra.py](/mnt/public/sichuan_a/qjj/CGRA-SoC/scripts/generate_multi_cgra.py:1)
- VectorCGRA multi arch/soc RTL loader:
  [VectorCGRA/multi_cgra/test/MeshMultiCgraTemplateRTL_multi_test.py](/mnt/public/sichuan_a/qjj/CGRA-SoC/VectorCGRA/multi_cgra/test/MeshMultiCgraTemplateRTL_multi_test.py:1)
- Semantic C API generation:
  [scripts/generate_cgra_c_api.py](/mnt/public/sichuan_a/qjj/CGRA-SoC/scripts/generate_cgra_c_api.py:1)
- VectorCGRA arch/soc RTL loader:
  [VectorCGRA/cgra/test/CgraTemplateRTL_single_test.py](/mnt/public/sichuan_a/qjj/CGRA-SoC/VectorCGRA/cgra/test/CgraTemplateRTL_single_test.py:1)
- Chipyard CGRA RoCC wrapper:
  [chipyard/generators/chipyard/src/main/scala/example/CGRA.scala](/mnt/public/sichuan_a/qjj/CGRA-SoC/chipyard/generators/chipyard/src/main/scala/example/CGRA.scala:1)
- Generated Chipyard parameters:
  [chipyard/generators/chipyard/src/main/scala/example/CGRAGenerated.scala](/mnt/public/sichuan_a/qjj/CGRA-SoC/chipyard/generators/chipyard/src/main/scala/example/CGRAGenerated.scala:1)
- Top-level runner:
  [run-chipyard-cgra-test.sh](/mnt/public/sichuan_a/qjj/CGRA-SoC/run-chipyard-cgra-test.sh:1)

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

`tests/include/cgra_runtime.h` provides `send_basic`, `send_config`,
`send_prologue`, target-aware `_to` variants, and `read_mem`. `read_mem(addr)`
sends a CGRA `CMD_LOAD_REQUEST`; `CGRA.scala` captures the matching
CPU-destined `CMD_LOAD_RESPONSE`.

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

`scripts/generate_cgra_c_api.py` updates:

- `tests/generated/cgra_<kernel>_api.h`

Do not hand-edit generated headers or generated RTL/Scala resources unless you
are deliberately debugging generator output. Regenerate from the YAML instead.

## Practical Guidance

- Keep `configs/arch/arch.yaml` and `configs/soc/cgra_soc.yaml` as the hardware/SoC
  source of truth.
- Keep `configs/kernels/kernel_*_4x4.yaml` limited to kernel metadata and execution
  counts.
- When a CPU+CGRA test fails, first confirm the matching VectorCGRA from-yaml
  test behavior and whether the first-output skip applies.
- If generated RTL or Chipyard `CGRA.scala` changes, use `--rebuild`.
- If only C test code or generated C API headers change and the active RTL is
  unchanged, rerun without `--rebuild`.
- Avoid fixed packet-width assumptions. Use `tests/include/cgra_layout.h` and
  `tests/include/cgra_runtime.h`.
- Remember that generated RTL/layout/Chipyard resources describe whichever
  single or multi generator ran last. Regenerate the intended top before
  rebuilding or running tests.
