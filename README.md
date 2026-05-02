# CGRA-SoC

This workspace exposes the CGRA integration flow from the top level while
keeping VectorCGRA and Chipyard as backend projects.

## Layout

- `configs/`: user-facing CGRA architecture and SoC interface YAML files.
- `tests/`: user-facing baremetal C tests for the Chipyard RoCC CGRA wrapper.
- `scripts/`: glue scripts that generate VectorCGRA RTL and sync it into
  Chipyard.
- `VectorCGRA/`: backend CGRA generator and PyMTL3 translation code.
- `chipyard/`: backend SoC integration, Verilator simulator, and toolchain.

## Generate RTL and Sync Chipyard

From the repository root:

```bash
python scripts/generate_single_cgra.py
```

By default this uses:

```text
configs/arch_fir_2x2.yaml
configs/cgra_soc_fir_2x2.yaml
```

To use another configuration:

```bash
python scripts/generate_single_cgra.py \
  --arch-yaml configs/arch_fir_2x2.yaml \
  --soc-yaml configs/cgra_soc_fir_2x2.yaml
```

The script runs the VectorCGRA PyMTL3 translator, then syncs the generated RTL,
BlackBox wrapper, and generated Scala parameters into Chipyard.

## Run Tests

Rebuild the Chipyard simulator after regenerating RTL or changing the Chipyard
CGRA integration. The default top-level YAML generates the FIR 2x2 RTL, so the
default test is `cgra-fir-2x2`:

```bash
./run-chipyard-cgra-test.sh --rebuild
```

Expected output includes:

```text
CGRA RoCC FIR 2x2: PASS
```

You can pass the test name explicitly:

```bash
./run-chipyard-cgra-test.sh --rebuild cgra-fir-2x2
```

After the simulator is already rebuilt for the current RTL, omit `--rebuild`
for faster reruns:

```bash
./run-chipyard-cgra-test.sh cgra-fir-2x2
```
