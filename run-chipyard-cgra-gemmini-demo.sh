#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CHIPYARD_DIR="$ROOT_DIR/chipyard"
GEMMINI_SW="$CHIPYARD_DIR/generators/gemmini/software/gemmini-rocc-tests"
CONFIG="${CONFIG:-CGRAMinimalGemminiRocketConfig}"
REBUILD=0
TEST_SRC="${TEST_SRC:-$ROOT_DIR/tests/cgra-gemmini/gemmini-gemm-cgra-relu.c}"
TEST_NAME="$(basename "$TEST_SRC" .c)"

usage() {
  echo "usage: $0 [--rebuild] [test-source.c]" >&2
  echo "       CONFIG=$CONFIG TEST_SRC=$TEST_SRC $0 --rebuild" >&2
}

while (($# > 0)); do
  case "$1" in
    --rebuild)
      REBUILD=1
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

echo "$RUN_STEP Running $TEST_NAME on $CONFIG"
make -C sims/verilator \
  CONFIG="$CONFIG" \
  BINARY="$BIN_PATH" \
  BREAK_SIM_PREREQ=1 \
  run-binary-fast
