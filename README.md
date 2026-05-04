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
BlackBox wrapper, generated Scala parameters, and C packet layout header into
Chipyard/top-level tests.

## Generate YAML Control Packets

The 4x4 FIR YAML test uses VectorCGRA's `ScriptFactory` to generate raw control
packets for the C test:

```bash
python scripts/generate_cgra_control_signals.py
```

By default this consumes:

```text
configs/arch_fir_yaml_4x4.yaml
configs/cgra_soc_fir_yaml_4x4.yaml
VectorCGRA/validation/test/fir_acceptance_test.yaml
```

and writes:

```text
tests/generated/cgra_fir_yaml_4x4_packets.h
```

The generated header includes preload packets and control packets. User C tests
can include `tests/include/cgra_runtime.h` and send the generated packet array
with `cgra_send_packets`.

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

To run the YAML-generated 4x4 FIR test, first generate matching 4x4 RTL and
refresh the generated packet header:

```bash
python scripts/generate_single_cgra.py \
  --arch-yaml configs/arch_fir_yaml_4x4.yaml \
  --soc-yaml configs/cgra_soc_fir_yaml_4x4.yaml
python scripts/generate_cgra_control_signals.py
./run-chipyard-cgra-test.sh --rebuild cgra-fir-yaml-4x4
```

Expected output includes:

```text
CGRA RoCC FIR YAML 4x4: PASS
```

After the simulator is already rebuilt for the current RTL, omit `--rebuild`
for faster reruns:

```bash
./run-chipyard-cgra-test.sh cgra-fir-2x2
```

The C tests include `tests/include/cgra_protocol.h` for stable RoCC/CGRA
protocol constants, `tests/include/cgra_runtime.h` for packet builders and
send helpers, and the generated `tests/include/cgra_layout.h` for packet bit
offsets that depend on the current generated RTL.
