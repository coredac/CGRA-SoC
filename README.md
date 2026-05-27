# CGRA-SoC

This repository integrates VectorCGRA-generated RTL with Chipyard RoCC. VectorCGRA generates CGRA RTL and control packets; Chipyard wraps the generated RTL and runs baremetal CPU+CGRA tests.

## **Repository layout**

- `configs/arch/`: CGRA architecture YAMLs. `arch.yaml` is the canonical single-CGRA architecture config.
- `configs/soc/`: SoC interface and memory YAMLs. `cgra_soc.yaml` is the canonical single-CGRA SoC config.
- `configs/kernels/`: Kernel metadata and execution counts. Kernel configs should not redefine hardware, interface, or memory fields.
- `scripts/generate_single_cgra.py`: Generates single-CGRA RTL and syncs it into Chipyard.
- `scripts/generate_multi_cgra.py`: Generates multi-CGRA RTL and syncs it into the same Chipyard BlackBox wrapper shape.
- `scripts/cgra_fast_api.py`: Generates fast-only C headers in `tests/generated/`.
- `tests/`: Baremetal CPU+CGRA tests.
- `VectorCGRA/`: CGRA generator and reference from-yaml tests.
- `chipyard/`: SoC integration and Verilator simulator.

## **Setup**

See [docs/Setup.md](./docs/Setup.md).

## **Single-CGRA flow**

Generate the canonical single-CGRA RTL:

```shell
$ python scripts/generate_single_cgra.py --arch-yaml configs/arch/arch.yaml --soc-yaml configs/soc/cgra_soc.yaml
```

Generate a fast C API for the target kernel, then rebuild and run the matching Chipyard test:

```shell
$ python scripts/cgra_fast_api.py --arch-yaml configs/arch/arch.yaml --soc-yaml configs/soc/cgra_soc.yaml configs/kernels/kernel_<kernel>_4x4.yaml --output-dir tests/generated
$ ./run-chipyard-cgra-test.sh --rebuild <c-test-name>
```

After rebuilding for the same generated RTL, rerun another matching C test without `--rebuild`:

```shell
$ ./run-chipyard-cgra-test.sh <c-test-name>
```

### **Supported single-CGRA kernels**

- FIR: `configs/kernels/kernel_fir4x4_4x4.yaml`, test `cgra-fir-yaml-4x4`
- ReLU: `configs/kernels/kernel_relu4x4_4x4.yaml`, test `cgra-relu4x4`
- GEMV: `configs/kernels/kernel_gemv_4x4.yaml`, test `cgra-gemv-4x4`
- Histogram: `configs/kernels/kernel_histogram_4x4.yaml`, test `cgra-histogram-4x4`
- AXPY: `configs/kernels/kernel_axpy_4x4.yaml`, test `cgra-axpy-4x4`

GEMM and SAD are unsupported and skipped by the fast API generator. **NOTE**: ReLU, GEMV, and histogram have some issues (the output of addr 0 is incorrect).

### **Generated fast APIs**

`scripts/cgra_fast_api.py` loads the layered arch/soc/kernel YAMLs, invokes VectorCGRA `ScriptFactory`, orders packets, and emits `tests/generated/cgra_<kernel>_fast_api.h`.

- `load_<kernel>_config_fast()`: sends non-launch packets.
- `launch_<kernel>_fast()`: sends launch packets.
- `configure_<kernel>_fast()`: sends config plus launch packets.
- `<kernel>_store_fast(addr, data)`: writes CGRA data memory.
- `<kernel>_read_mem_fast(addr)`: reads CGRA data memory through the RoCC wrapper.

Generated fast APIs are local single-CGRA only and are precomputed for `cgra_target_local()`. Target-aware and hand-written multi-CGRA tests continue to use `tests/include/cgra_runtime.h`.

### **Gemmini + CGRA demo**

The supported combined accelerator demo uses `CGRAMinimalGemminiRocketConfig`: CGRA uses `custom0`, Gemmini uses `custom3`, and data moves through DRAM and CPU-mediated CGRA stores.

This demo implements a GEMM kernel (16×16) on Gemmini and then a ReLU kernel (1×16) on the CGRA. First, data is loaded from DRAM to Gemmini, and the GEMM result is then stored back to DRAM. The CGRA then loads the GEMM output from DRAM (via the CPU sending them one by one), applies ReLU, and sends the results back to the CPU.

```shell
$ python scripts/generate_single_cgra.py \
      --kernel-yaml configs/kernels/kernel_relu4x4_4x4.yaml \
      --arch-yaml configs/arch/arch.yaml \
      --soc-yaml configs/soc/cgra_soc.yaml
$ python scripts/cgra_fast_api.py \
      --arch-yaml configs/arch/arch.yaml \
      --soc-yaml configs/soc/cgra_soc.yaml \
      configs/kernels/kernel_relu4x4_4x4.yaml \
      --output-dir tests/generated
$ ./run-chipyard-cgra-gemmini-demo.sh --rebuild
```

Use `--rebuild` on the first run because Gemmini elaboration must generate a matching `gemmini_params.h`.

## **Multi-CGRA flow**

Generate multi-CGRA RTL with the matching arch and SoC YAMLs:

```shell
$ python scripts/generate_multi_cgra.py --arch-yaml configs/arch/multi_cgra_arch.yaml --soc-yaml configs/soc/multi_cgra_soc.yaml
```

Supported multi-CGRA CPU+CGRA tests:

- `multi-cgra/cgra-multi-homo`: generate with `configs/arch/multi_cgra_homo_meshrtl.yaml` and `configs/soc/multi_cgra_homo_meshrtl.yaml`; run `./run-chipyard-cgra-test.sh --rebuild multi-cgra/cgra-multi-homo`.
- `multi-cgra/cgra-multi-systolic-2x2`: generate with `configs/arch/multi_cgra_homo_meshrtl.yaml` and `configs/soc/multi_cgra_systolic_2x2.yaml`; run `./run-chipyard-cgra-test.sh --rebuild multi-cgra/cgra-multi-systolic-2x2`.
- `multi-cgra/cgra-multi-systolic-4x4`: generate with `configs/arch/multi_cgra_4x4_meshrtl.yaml` and `configs/soc/multi_cgra_systolic_4x4_2x2.yaml`; run `./run-chipyard-cgra-test.sh --rebuild multi-cgra/cgra-multi-systolic-4x4`.

Multi-CGRA FIR tests live under `tests/multi-cgra/fir/`. Their C control packets are hand-written from `VectorCGRA/multi_cgra/test/MeshMultiCgraRTL_test.py`; do not regenerate those control signals with a script.

- `multi-cgra/fir/cgra-multi-fir-scalar`: generate with `configs/arch/multi_cgra_fir_2x2_4x4_scalar.yaml` and `configs/soc/multi_cgra_fir_scalar.yaml`; run `./run-chipyard-cgra-test.sh --rebuild multi-cgra/fir/cgra-multi-fir-scalar`.
- `multi-cgra/fir/cgra-multi-fir-scalar-2x2-2x2`: generate with `configs/arch/multi_cgra_homo_meshrtl.yaml` and `configs/soc/multi_cgra_fir_scalar.yaml`; run `./run-chipyard-cgra-test.sh --rebuild multi-cgra/fir/cgra-multi-fir-scalar-2x2-2x2`.
- `multi-cgra/fir/cgra-multi-fir-vector`: generate with `configs/arch/multi_cgra_fir_2x2_4x4_vector.yaml` and `configs/soc/multi_cgra_fir_vector.yaml`; run `./run-chipyard-cgra-test.sh --rebuild multi-cgra/fir/cgra-multi-fir-vector`.

**Reference tests**

When bringing up a kernel, first check the matching VectorCGRA from-yaml test. For FIR, use `CgraRTL_fir4x4_test_from_yaml.py`; the older `CgraRTL_fir_test_from_yaml.py` is not the reference for this flow.

```shell
$ cd VectorCGRA
$ python -m pytest cgra/test/CgraRTL_fir4x4_test_from_yaml.py -q
```
