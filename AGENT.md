# CGRA-SoC Agent Notes

This repository is the top-level integration workspace for VectorCGRA RTL
generation and Chipyard CPU+CGRA simulation.

## Current State

The active flow is the unified per-kernel 4x4 YAML flow. The old
`cgra-fir-2x2` path is deprecated and should not be used as the main reference
or default bring-up path.

Currently supported CPU+CGRA kernels:

| Kernel | Unified config | C test | Generated API | Validation status |
| --- | --- | --- | --- | --- |
| FIR | `configs/kernel_fir4x4_4x4.yaml` | `cgra-fir-yaml-4x4` | `tests/generated/cgra_fir4x4_api.h` | PASS, checks completion/result. |
| ReLU | `configs/kernel_relu4x4_4x4.yaml` | `cgra-relu4x4` | `tests/generated/cgra_relu4x4_api.h` | PASS, checks all output addresses. |
| GEMV | `configs/kernel_gemv_4x4.yaml` | `cgra-gemv-4x4` | `tests/generated/cgra_gemv_api.h` | PASS with known row-0 skip. |
| Histogram | `configs/kernel_histogram_4x4.yaml` | `cgra-histogram-4x4` | `tests/generated/cgra_histogram_api.h` | PASS with known bin-0 skip. |
| AXPY | `configs/kernel_axpy_4x4.yaml` | `cgra-axpy-4x4` | `tests/generated/cgra_axpy_api.h` | PASS with known addr0 skip. |

Do not treat GEMM or SAD as supported just because local configs/tests may
exist. They are not in the current supported CPU+CGRA set.

## Kernel Bring-Up Rule

For every kernel, use this order:

1. Run the VectorCGRA from-yaml reference test.
2. Generate matching single-CGRA RTL from the unified kernel YAML.
3. Generate the semantic C API header from the same YAML.
4. Rebuild the Chipyard simulator and run the matching C test.

Changing kernels requires a new RTL generation and simulator rebuild. This is
not just a stale build-system issue: different kernels can change generated RTL
parameters, control memory size, data memory size, and `FuList`. Running a C
test on a simulator built for a different kernel gives invalid results.

## Main Commands

From the top-level repository:

```bash
python scripts/generate_single_cgra.py --kernel-yaml configs/kernel_<kernel>_4x4.yaml
python scripts/generate_cgra_c_api.py configs/kernel_<kernel>_4x4.yaml
./run-chipyard-cgra-test.sh --rebuild <c-test-name>
```

Concrete examples:

```bash
python scripts/generate_single_cgra.py --kernel-yaml configs/kernel_fir4x4_4x4.yaml
python scripts/generate_cgra_c_api.py configs/kernel_fir4x4_4x4.yaml
./run-chipyard-cgra-test.sh --rebuild cgra-fir-yaml-4x4
```

```bash
python scripts/generate_single_cgra.py --kernel-yaml configs/kernel_histogram_4x4.yaml
python scripts/generate_cgra_c_api.py configs/kernel_histogram_4x4.yaml
./run-chipyard-cgra-test.sh --rebuild cgra-histogram-4x4
```

After rebuilding for the same generated RTL, rerun that same test without
`--rebuild`.

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

ReLU verifies every address. FIR currently checks completion count and scalar
result rather than memory readback.

If one of these skipped addresses is fixed upstream, update the C test to check
it and remove the explanatory comment in the test.

## Key Entry Points

- RTL generation:
  [scripts/generate_single_cgra.py](/mnt/public/sichuan_a/qjj/CGRA-SoC/scripts/generate_single_cgra.py:1)
- Semantic C API generation:
  [scripts/generate_cgra_c_api.py](/mnt/public/sichuan_a/qjj/CGRA-SoC/scripts/generate_cgra_c_api.py:1)
- VectorCGRA unified YAML loader / translator:
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
`send_prologue`, and `read_mem`. `read_mem(addr)` sends a CGRA
`CMD_LOAD_REQUEST`; `CGRA.scala` captures the matching CPU-destined
`CMD_LOAD_RESPONSE`.

## Generated Files

`scripts/generate_single_cgra.py` updates:

- `VectorCGRA/CgraTemplateRTL_single__pickled.v`
- `chipyard/generators/chipyard/src/main/resources/vsrc/CgraTemplateRTL_single__pickled.v`
- `chipyard/generators/chipyard/src/main/resources/vsrc/CgraTemplateRTL_single_wrapper.v`
- `chipyard/generators/chipyard/src/main/scala/example/CGRAGenerated.scala`
- `tests/include/cgra_layout.h`

`scripts/generate_cgra_c_api.py` updates:

- `tests/generated/cgra_<kernel>_api.h`

Do not hand-edit generated headers or generated RTL/Scala resources unless you
are deliberately debugging generator output. Regenerate from the YAML instead.

## Divider Notes

Histogram uses `OPT_DIV_CONST`. The translatable divider path maps YAML
`DivRTL` to `ExclusiveDivRTL`, and `ExclusiveDivRTL` must support
`OPT_DIV_CONST`. Keep its default latency at 4 cycles; do not force a global
latency-1 override.

## Practical Guidance

- Keep `configs/kernel_*_4x4.yaml` as the source of truth for supported kernel
  hardware parameters, execution counts, and `fu_list`.
- When a CPU+CGRA test fails, first confirm the matching VectorCGRA from-yaml
  test behavior and whether the first-output skip applies.
- If generated RTL or Chipyard `CGRA.scala` changes, use `--rebuild`.
- If only C test code changes and the active RTL is unchanged, rerun without
  `--rebuild`.
- Avoid old fixed packet-width assumptions. Use `tests/include/cgra_layout.h`
  and `tests/include/cgra_runtime.h`.
