# CGRA-SoC Agent Notes

This repository is the top-level integration workspace for VectorCGRA RTL
generation and Chipyard SoC simulation.

It currently contains:

- `configs/`: top-level CGRA architecture and SoC/interface YAML files.
- `tests/`: top-level baremetal C tests for the Chipyard CGRA RoCC wrapper.
- `VectorCGRA/`: CGRA architecture parser, PyMTL3 generator, reference tests,
  and generated RTL output.
- `chipyard/`: active SoC integration point. It consumes generated CGRA RTL
  through a Chisel BlackBox wrapper and runs RoCC baremetal tests.
- `firesim/`: existing submodule from the upstream layout.
- `scripts/`: glue scripts that translate VectorCGRA output into Chipyard
  resources.

## Current Flow

The important change is that Chipyard is no longer tied to a hand-maintained
`CgraRTL_2x2` blackbox. The current flow supports generating RTL from
VectorCGRA and then letting Chipyard stitch the generated RTL into the SoC and
test it.

Typical flow:

```bash
cd /mnt/public/qjj/CGRA-SoC
python scripts/generate_single_cgra.py
./run-chipyard-cgra-test.sh --rebuild
```

`scripts/generate_single_cgra.py` does two steps:

1. Runs `VectorCGRA/cgra/test/CgraTemplateRTL_single_test.py` to elaborate a
   YAML-configured single-CGRA `CgraTemplateRTL_single` and emit PyMTL3 Verilog.
2. Runs `scripts/sync_cgra_blackbox.py` to copy that RTL into Chipyard, generate
   a flat SystemVerilog wrapper, and update Chipyard's generated Scala params.

## Main Entry Points

- VectorCGRA-to-Chipyard generation:
  [scripts/generate_single_cgra.py](/mnt/public/sichuan_a/qjj/CGRA-SoC/scripts/generate_single_cgra.py:1)

- Generic RTL sync/wrapper generator:
  [scripts/sync_cgra_blackbox.py](/mnt/public/sichuan_a/qjj/CGRA-SoC/scripts/sync_cgra_blackbox.py:1)

- Chipyard CGRA RoCC wrapper:
  [chipyard/generators/chipyard/src/main/scala/example/CGRA.scala](/mnt/public/sichuan_a/qjj/CGRA-SoC/chipyard/generators/chipyard/src/main/scala/example/CGRA.scala:1)

- Generated Chipyard CGRA parameters:
  [chipyard/generators/chipyard/src/main/scala/example/CGRAGenerated.scala](/mnt/public/sichuan_a/qjj/CGRA-SoC/chipyard/generators/chipyard/src/main/scala/example/CGRAGenerated.scala:1)

- Top-level Chipyard test runner:
  [run-chipyard-cgra-test.sh](/mnt/public/sichuan_a/qjj/CGRA-SoC/run-chipyard-cgra-test.sh:1)

- Current baremetal tests:
  [tests/cgra-fir-2x2.c](/mnt/public/sichuan_a/qjj/CGRA-SoC/tests/cgra-fir-2x2.c:1)

## Chipyard Integration State

Chipyard now takes CGRA shape and packet metadata from `CGRAGenerated.params`.
`CGRA.scala` should stay generic over:

- `intraPktWidth`
- `interPktWidth`
- `payloadWidth`
- `dataWidth` / `dataPayloadWidth`
- `cmdWidth`
- `idWidth`
- `addrWidth`
- `xTiles`, `yTiles`, and `numTiles`
- `addressLower` / `addressUpper`
- optional boundary data ports via `hasBoundaryPorts`
- generated RTL and wrapper resource names

The generated wrapper flattens PyMTL3 struct and array ports into Chisel
BlackBox-compatible scalar/vector ports. For single-CGRA `CgraTemplateRTL`
builds, boundary data ports can be absent; `CGRA.scala` handles that with
optional Chisel IO.

## RoCC Host Interface

The CGRA is attached as a RoCC accelerator via `custom0`. The current wrapper is
a raw packet injector plus completion tracker. It does not try to understand the
full CGRA programming model.

Current funct encodings:

- `2`: `STATUS`
- `4`: `WAIT`
- `5`: `RAW_PKT_LO`
- `6`: `RAW_PKT_MID`
- `7`: `RAW_PKT_HI`
- `8`: `SET_EXPECTED_COMPLETES`
- `9`: `RESULT`
- `10`: `RAW_PKT_TOP`

`RAW_PKT_TOP` exists because generated `CgraTemplateRTL_single` packets can be
wider than 192 bits. After changing YAML or regenerating RTL, check
`CGRAGenerated.scala` and make sure any hand-written C packet layout constants
still match the generated packet type.

## Test Guidance

Use the top-level runner with an explicit Chipyard test name:

```bash
./run-chipyard-cgra-test.sh --rebuild cgra-fir-2x2
```

The script compiles `tests/<test-name>.c` and runs it on
`CGRARocketConfig`. Use `--rebuild` after changing any generated RTL, wrapper
Verilog, `CGRAGenerated.scala`, `CGRA.scala`, or Chipyard config fragments.

Expected success strings include:

```text
CGRA RoCC FIR 2x2: PASS
```

## Compatibility Notes

The older fixed `CgraRTL_2x2` path still exists as generated/vendored resources:

- `chipyard/generators/chipyard/src/main/resources/vsrc/CgraRTL_2x2__pickled.v`
- `chipyard/generators/chipyard/src/main/resources/vsrc/CgraRTL_2x2_wrapper.v`

Do not assume the active integration is using those files. The active hardware
is selected by `CGRAGenerated.scala`, which currently points at
`CgraTemplateRTL_single`.

## Repository Status Expectations

- The top-level repo may contain local workspace files such as `.venv/`,
  `.vscode/`, `.metals/`, generated RTL, and Python caches.
- `configs/` and `tests/` are the canonical places for CGRA SoC input YAML and
  Chipyard baremetal CGRA tests.
- `VectorCGRA/` may be dirty when generator changes, YAML files, or generated
  RTL are being developed.
- `chipyard/` may contain generated Scala/Verilog resources. Regenerate them
  rather than editing generated files by hand.

## Practical Guidance

- For architecture changes, start with top-level `configs/` and
  `VectorCGRA/cgra/test/CgraTemplateRTL_single_test.py`.
- For SoC-facing metadata and BlackBox stitching, start with
  `scripts/sync_cgra_blackbox.py` and `CGRA.scala`.
- For host packet programming, inspect the generated packet typedefs in the
  emitted PyMTL3 Verilog and keep the C packet builder constants in sync.
- Avoid hard-coding old packet widths such as `182` unless you are deliberately
  targeting the old `CgraRTL_2x2` wrapper.
