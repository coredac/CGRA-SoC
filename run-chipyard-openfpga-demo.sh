#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CHIPYARD_DIR="$ROOT_DIR/chipyard"
ENV_FILE="$ROOT_DIR/tests/generated/openfpga_demo.env"

if [[ ! -f "$ENV_FILE" ]]; then
  echo "error: generated env not found: $ENV_FILE" >&2
  echo "run: python scripts/generate_openfpga_demo.py --config configs/openfpga/openfpga_and2.yaml" >&2
  exit 1
fi

source "$ENV_FILE"

CONFIG="${CONFIG:-$OPENFPGA_CHIPYARD_CONFIG}"
REBUILD=0
TEST_NAME="${TEST_NAME:-$OPENFPGA_DEFAULT_TEST}"
SEEN_TEST_NAME=0

usage() {
  echo "usage: $0 [--rebuild] [$OPENFPGA_DEFAULT_TEST]" >&2
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
      if ((SEEN_TEST_NAME)); then
        echo "error: only one test name is supported" >&2
        usage
        exit 1
      fi
      TEST_NAME="$1"
      SEEN_TEST_NAME=1
      ;;
  esac
  shift
done

TEST_SRC="$ROOT_DIR/tests/${TEST_NAME}.c"
OUT_DIR="${TMPDIR:-/tmp}/chipyard-openfpga"
BIN_PATH="$OUT_DIR/${TEST_NAME}.riscv"

if [[ ! -f "$TEST_SRC" ]]; then
  echo "error: test source not found: $TEST_SRC" >&2
  usage
  exit 1
fi

mkdir -p "$OUT_DIR" "$(dirname "$BIN_PATH")"

cd "$CHIPYARD_DIR"
set +u
source env.sh >/dev/null 2>&1
set -u

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
  -I "$ROOT_DIR/tests" \
  -I "$ROOT_DIR/tests/include" \
  -I "$CHIPYARD_DIR/tests" \
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
