# CGRA-SoC

This workspace drives the CPU+CGRA integration flow from unified per-kernel
YAML files. VectorCGRA generates the CGRA RTL and semantic control packets;
Chipyard hosts the generated RTL behind the RoCC wrapper and runs baremetal C
tests.

## Layout

- `configs/kernel_*_4x4.yaml`: unified per-kernel hardware and execution
  configs.
- `scripts/generate_single_cgra.py`: generates kernel-specific
  `CgraTemplateRTL_single` RTL and syncs it into Chipyard.
- `scripts/generate_cgra_c_api.py`: generates semantic C configuration APIs in
  `tests/generated/cgra_<kernel>_api.h`.
- `tests/`: baremetal CPU+CGRA tests.
- `tests/include/`: CGRA protocol constants, packet layout, and runtime helpers.
- `VectorCGRA/`: backend CGRA generator and reference from-yaml tests.
- `chipyard/`: SoC integration and Verilator simulator.

The older `cgra-fir-2x2` flow is deprecated and is not the main validation path.
Use the unified 4x4 kernel YAML flow below.

## Supported Kernels

| Kernel | Config | C test | Notes |
| --- | --- | --- | --- |
| FIR | `configs/kernel_fir4x4_4x4.yaml` | `cgra-fir-yaml-4x4` | Checks completion/result. |
| ReLU | `configs/kernel_relu4x4_4x4.yaml` | `cgra-relu4x4` | Checks all output addresses. |
| GEMV | `configs/kernel_gemv_4x4.yaml` | `cgra-gemv-4x4` | Checks output rows 1..3; skips known row 0 issue. |
| Histogram | `configs/kernel_histogram_4x4.yaml` | `cgra-histogram-4x4` | Checks bins 1..3; skips known bin 0 issue. |
| AXPY | `configs/kernel_axpy_4x4.yaml` | `cgra-axpy-4x4` | Checks addr 1..15; skips known addr 0 issue. |

GEMM and SAD are not part of the current supported CPU+CGRA set.

## Run A Kernel

Switching kernels requires regenerating RTL and rebuilding the simulator because
different kernels can use different RTL parameters, control memories, and FU
lists. Running a new test on a simulator built for a previous kernel gives
invalid results.

Use this flow from the repository root:

```bash
python scripts/generate_single_cgra.py --kernel-yaml configs/kernel_<kernel>_4x4.yaml
python scripts/generate_cgra_c_api.py configs/kernel_<kernel>_4x4.yaml
./run-chipyard-cgra-test.sh --rebuild <c-test-name>
```

Examples:

```bash
python scripts/generate_single_cgra.py --kernel-yaml configs/kernel_histogram_4x4.yaml
python scripts/generate_cgra_c_api.py configs/kernel_histogram_4x4.yaml
./run-chipyard-cgra-test.sh --rebuild cgra-histogram-4x4
```

```bash
python scripts/generate_single_cgra.py --kernel-yaml configs/kernel_relu4x4_4x4.yaml
python scripts/generate_cgra_c_api.py configs/kernel_relu4x4_4x4.yaml
./run-chipyard-cgra-test.sh --rebuild cgra-relu4x4
```

After rebuilding for the same generated RTL, rerun the same C test without
`--rebuild`:

```bash
./run-chipyard-cgra-test.sh cgra-relu4x4
```

## Reference First

When bringing up a kernel, first check the matching VectorCGRA from-yaml test.
For FIR, use `CgraRTL_fir4x4_test_from_yaml.py`; the older
`CgraRTL_fir_test_from_yaml.py` is not the reference for this flow.

```bash
cd VectorCGRA
python -m pytest cgra/test/CgraRTL_fir4x4_test_from_yaml.py -q
python -m pytest cgra/test/CgraRTL_relu4x4_test_from_yaml.py -q
python -m pytest cgra/test/CgraRTL_gemv_test_from_yaml.py -q
python -m pytest cgra/test/CgraRTL_histogram_test_from_yaml.py -q
python -m pytest cgra/test/CgraRTL_axpy_test_from_yaml.py -q
```

Then return to the top level and run the CPU+CGRA flow.

## Runtime Notes

The C tests use `tests/include/cgra_protocol.h` for protocol constants,
`tests/include/cgra_runtime.h` for packet send/readback helpers, and the
generated `tests/include/cgra_layout.h` for packet field offsets.

`read_mem(addr)` sends a CGRA load request and reads back CGRA-side data memory
through the RoCC wrapper. This is used by ReLU, GEMV, Histogram, and AXPY
result checks.

Known first-output issues are preserved in the C tests with comments:
GEMV leaves logical row 0 (`addr20`) incorrect, Histogram leaves logical bin 0
(`addr20`) incorrect, and AXPY leaves physical `addr0` incorrect. Those tests
still require the remaining checked addresses to match.
