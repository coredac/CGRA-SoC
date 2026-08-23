#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CHIPYARD_DIR="$ROOT_DIR/chipyard"
GEMMINI_SW="$CHIPYARD_DIR/generators/gemmini/software/gemmini-rocc-tests"
CGRA_SOC_YAML="$ROOT_DIR/configs/soc/cgra_gemmini_soc.yaml"
SHARED_SPM_GENERATOR="$ROOT_DIR/scripts/generate_shared_spm.py"
CONTROL_GENERATOR="$ROOT_DIR/scripts/generate_cgra_spm_control.py"
CONFIG="${CONFIG:-CGRAMinimalGemminiRocketConfig}"
REBUILD=0
TEST_SRC="${TEST_SRC:-$ROOT_DIR/tests/cgra-gemmini/relu_dma.c}"
TEST_NAME="$(basename "$TEST_SRC" .c)"

uses_spm_dma() {
  case "$CONFIG" in
    CGRAMinimalGemminiRocketConfig)
      return 0
      ;;
    *)
      return 1
      ;;
  esac
}

usage() {
  echo "usage: $0 [--rebuild] [--fast] [test-source.c]" >&2
  echo "       CONFIG=$CONFIG TEST_SRC=$TEST_SRC $0 --rebuild" >&2
}

while (($# > 0)); do
  case "$1" in
    --rebuild)
      REBUILD=1
      ;;
    --fast)
      # Fast simulation is the only supported mode.
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    -*)
      echo "error: unknown option: $1" >&2
      usage
      exit 1
      ;;
    *)
      TEST_SRC="$1"
      TEST_NAME="$(basename "$TEST_SRC" .c)"
      ;;
  esac
  shift
done

if [[ ! -f "$TEST_SRC" ]]; then
  echo "error: test source not found: $TEST_SRC" >&2
  usage
  exit 1
fi
TEST_SRC="$(realpath "$TEST_SRC")"

if (( ! REBUILD )); then
  echo "note: first run should use --rebuild so elaboration regenerates matching gemmini_params.h" >&2
fi

OUT_DIR="${TMPDIR:-/tmp}/chipyard-cgra-gemmini"
BIN_PATH="$OUT_DIR/${TEST_NAME}.${CONFIG}.riscv"

mkdir -p "$OUT_DIR" "$(dirname "$BIN_PATH")"

cd "$CHIPYARD_DIR"
set +u
source env.sh >/dev/null 2>&1
set -u

# The prebuilt simulator needs the Conda C++ runtime ahead of the system one.
export LD_LIBRARY_PATH="$CHIPYARD_DIR/.conda-env/lib:${LD_LIBRARY_PATH:-}"

if uses_spm_dma; then
  if ((REBUILD)); then
    echo "[generate] CGRA + Gemmini SPM DMA design"
    python3 "$ROOT_DIR/scripts/generate_single_cgra.py" --soc-yaml "$CGRA_SOC_YAML"
    python3 "$SHARED_SPM_GENERATOR" --soc-yaml "$CGRA_SOC_YAML"
    python3 "$CONTROL_GENERATOR" --soc-yaml "$CGRA_SOC_YAML"
  else
    python3 "$SHARED_SPM_GENERATOR" --soc-yaml "$CGRA_SOC_YAML" --check
    python3 "$CONTROL_GENERATOR" --soc-yaml "$CGRA_SOC_YAML" --check
  fi
fi

if ((REBUILD)); then
  echo "[1/3] Rebuilding $CONFIG simulator"
  make -C sims/verilator CONFIG="$CONFIG"
  BUILD_STEP="[2/3]"
  RUN_STEP="[3/3]"
else
  BUILD_STEP="[1/2]"
  RUN_STEP="[2/2]"
fi

echo "$BUILD_STEP Building $TEST_NAME -> $BIN_PATH"
riscv64-unknown-elf-gcc \
  -std=gnu99 -O2 -Wall -Wextra -fno-common -fno-builtin-printf \
  -march=rv64imafd -mabi=lp64d -mcmodel=medany \
  -I "$ROOT_DIR/tests/include" \
  -I "$ROOT_DIR/tests" \
  -I "$ROOT_DIR/tests/cgra-gemmini" \
  -I "$CHIPYARD_DIR/tests" \
  -I "$GEMMINI_SW" \
  -I "$GEMMINI_SW/include" \
  -I "$GEMMINI_SW/rocc-software/src" \
  -I "$GEMMINI_SW/riscv-tests/env" \
  -I "$GEMMINI_SW/riscv-tests/benchmarks/common" \
  -specs="$CHIPYARD_DIR/toolchains/libgloss/util/htif_nano.specs" \
  -static -T "$CHIPYARD_DIR/tests/htif.ld" \
  "$TEST_SRC" \
  -o "$BIN_PATH"

# The combined generated model requires more host stack than the common
# 8 MiB shell default. Keep this local to the simulator process tree.
if [[ "$(ulimit -s)" != unlimited ]] && (( $(ulimit -s) < 32768 )); then
  ulimit -s 32768
fi

echo "$RUN_STEP Running $TEST_NAME on $CONFIG (fast)"
make -C sims/verilator \
  CONFIG="$CONFIG" \
  BINARY="$BIN_PATH" \
  BREAK_SIM_PREREQ=1 \
  run-binary-fast
