---
name: cgra-validate
description: Validate CGRA-SoC kernel, generator, runtime, or hardware integration changes with the matching reference test and simulator. Use for hardware validation tasks, not documentation-only edits.
---

# Validate CGRA-SoC changes

Run from the repository root. Read the relevant [hardware contracts](../../../docs/contracts.md) and use the current runner or generator `--help` when choosing arguments. Preserve the user's selected architecture, SoC YAML, kernel, and configuration.

## Choose the validation path

- For a CGRA kernel change, first run its matching `VectorCGRA/cgra/test/*_test_from_yaml.py` reference test with its current parameters. Multi-CGRA cases live in `VectorCGRA/multi_cgra/test/MeshMultiCgraRTL_test.py`. The `CgraTemplateRTL_single_test.py` and `MeshMultiCgraTemplateRTL_multi_test.py` files are translation helpers, not simulation reference tests.
- Regenerate affected RTL and API from the intended configuration. For standalone CGRA or OpenFPGA, generation precedes the runner; Gemmini's `--rebuild` runs its integration generators.
- Use `run-chipyard-cgra-test.sh` for single/multi-CGRA, `run-chipyard-openfpga-demo.sh` for OpenFPGA, and `run-chipyard-cgra-gemmini-demo.sh` for combined accelerator paths.
- Use `--rebuild` after changing generated RTL, Scala integration, a RoCC wrapper, Chipyard configuration, or SoC YAML. The first Gemmini run after a configuration change must rebuild to regenerate matching Gemmini parameters. C-only changes can reuse a simulator built for the same design.

## Common Gemmini paths

These commands include a rebuild; omit it only under the reuse rule above.

| Path | Command |
| --- | --- |
| Automatic Gemmini → CGRA | `./run-chipyard-cgra-gemmini-demo.sh --rebuild` |
| Manual AES → Gemmini → CGRA → AES | `CONFIG=CGRAMinimalGemminiAESRocketConfig TEST_SRC=tests/cgra-gemmini/aes_gemm_relu_manual.c ./run-chipyard-cgra-gemmini-demo.sh --rebuild` |
| Automatic AES → Gemmini → CGRA → AES | `CONFIG=CGRAMinimalGemminiAESAutoLinkRocketConfig TEST_SRC=tests/cgra-gemmini/aes_gemm_relu_auto.c ./run-chipyard-cgra-gemmini-demo.sh --rebuild` |

The default combined runner selects `CGRAMinimalGemminiAutoLinkRocketConfig` and `relu_spm_auto.c`. For Pool, residual, and other supported cases, select the matching configuration and C test from the current runner and tests. YAML graph changes require a rebuild even if the Chipyard configuration name is unchanged.

## Completion evidence

Check process exit status and the test's expected result. When piping output through `tee`, retain `pipefail`. Report the chosen configuration, exact test command, outcome, and log location; identify blocked checks without claiming they passed. Stop validation after the relevant checks pass unless new evidence requires another run.
