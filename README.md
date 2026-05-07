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

By default this uses the current validated Neura 4x4 flow:

```text
configs/architectures/neura_architecture.yaml
configs/cgra_soc_neura_4x4.yaml
```

To use another configuration:

```bash
python scripts/generate_single_cgra.py \
  --arch-yaml configs/architectures/neura_architecture.yaml \
  --soc-yaml configs/cgra_soc_neura_4x4.yaml
```

The script runs the VectorCGRA PyMTL3 translator, then syncs the generated RTL,
BlackBox wrapper, generated Scala parameters, and C packet layout header into
Chipyard/top-level tests.

## Generate YAML Control Packets

The current compiler-format FIR and AXPY tests use VectorCGRA's
`ScriptFactory` to generate raw control packets for the C tests:

```bash
python scripts/generate_cgra_control_signals.py
```

By default this consumes:

```text
configs/architectures/neura_architecture.yaml
configs/cgra_soc_neura_4x4.yaml
configs/kernels/fir.yaml
```

and writes:

```text
tests/generated/cgra_fir_packets.h
```

The generated header includes preload packets and control packets. User C tests
can include `tests/include/cgra_runtime.h` and send the generated packet array
with `cgra_send_packets`.

## Run Tests

Rebuild the Chipyard simulator after regenerating RTL or changing the Chipyard
CGRA integration. The current validated top-level flow runs `cgra-fir` and
`cgra-axpy` on the Neura 4x4 RTL:

```bash
./run-chipyard-cgra-test.sh --rebuild
```

Expected output includes:

```text
CGRA RoCC FIR: PASS
```

You can pass the test name explicitly:

```bash
./run-chipyard-cgra-test.sh --rebuild cgra-fir
./run-chipyard-cgra-test.sh --rebuild cgra-axpy
```

To regenerate the current Neura 4x4 RTL and packet header, use:

```bash
python scripts/generate_single_cgra.py \
  --arch-yaml configs/architectures/neura_architecture.yaml \
  --soc-yaml configs/cgra_soc_neura_4x4.yaml
python scripts/generate_cgra_control_signals.py \
  --arch-yaml configs/architectures/neura_architecture.yaml \
  --soc-yaml configs/cgra_soc_neura_4x4.yaml \
  --control-yaml configs/kernels/fir.yaml \
  --output tests/generated/cgra_fir_packets.h \
  --expected-completes 1 \
  --expected-result 528
./run-chipyard-cgra-test.sh --rebuild cgra-fir
```

Expected output includes:

```text
CGRA RoCC AXPY: PASS
```

After the simulator is already rebuilt for the current RTL, omit `--rebuild`
for faster reruns:

```bash
./run-chipyard-cgra-test.sh cgra-fir
```

The C tests include `tests/include/cgra_protocol.h` for stable RoCC/CGRA
protocol constants, `tests/include/cgra_runtime.h` for packet builders and
send helpers, and the generated `tests/include/cgra_layout.h` for packet bit
offsets that depend on the current generated RTL.
