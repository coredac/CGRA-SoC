#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CHIPYARD_DIR="$ROOT_DIR/chipyard"
GEMMINI_SW="$CHIPYARD_DIR/generators/gemmini/software/gemmini-rocc-tests"
AES_SW="$CHIPYARD_DIR/generators/caliptra-aes-acc/software"
GC_SOC_YAML="$ROOT_DIR/configs/soc/autolink/gc.yaml"
GCA_SOC_YAML="$ROOT_DIR/configs/soc/autolink/gca.yaml"
EXTERNAL_SPM_GENERATOR="$ROOT_DIR/scripts/generate_gemmini_ext_spm.py"
CGRA_SPM_GENERATOR="$ROOT_DIR/scripts/generate_cgra_spm_window.py"
CONTROL_GENERATOR="$ROOT_DIR/scripts/generate_cgra_link_control.py"
AUTO_LINK_GENERATOR="$ROOT_DIR/scripts/generate_auto_links.py"
AES_JOB_GENERATOR="$ROOT_DIR/scripts/generate_aes_auto_job.py"
CONFIG="${CONFIG:-CGRAMinimalGemminiAutoLinkRocketConfig}"
REBUILD=0
TEST_SRC="${TEST_SRC:-$ROOT_DIR/tests/cgra-gemmini/relu_spm_auto.c}"
TEST_NAME="$(basename "$TEST_SRC" .c)"

uses_auto_link() {
  case "$CONFIG" in
    CGRAMinimalGemminiAutoLinkRocketConfig|CGRAMinimalGemminiAESAutoLinkRocketConfig)
      return 0
      ;;
    *)
      return 1
      ;;
  esac
}

uses_cgra_spm() {
  case "$CONFIG" in
    CGRAMinimalGemminiAESRocketConfig|CGRAMinimalGemminiAESAutoLinkRocketConfig)
      return 0
      ;;
    *)
      return 1
      ;;
  esac
}

uses_aes_manual() {
  [[ "$CONFIG" == CGRAMinimalGemminiAESRocketConfig ]]
}

uses_aes_auto() {
  [[ "$CONFIG" == CGRAMinimalGemminiAESAutoLinkRocketConfig ]]
}

CGRA_SOC_YAML="$GC_SOC_YAML"
if uses_cgra_spm; then
  CGRA_SOC_YAML="$GCA_SOC_YAML"
fi

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

if ((REBUILD)); then
  echo "[generate] CGRA design"
  python3 "$ROOT_DIR/scripts/generate_single_cgra.py" --soc-yaml "$CGRA_SOC_YAML"
fi

if ((REBUILD)); then
  echo "[generate] Gemmini external SPM"
  python3 "$EXTERNAL_SPM_GENERATOR" --soc-yaml "$CGRA_SOC_YAML"
else
  python3 "$EXTERNAL_SPM_GENERATOR" --soc-yaml "$CGRA_SOC_YAML" --check
fi

if uses_auto_link; then
  if ((REBUILD)); then
    echo "[generate] CGRA + Gemmini AutoLink"
    python3 "$CONTROL_GENERATOR" --soc-yaml "$CGRA_SOC_YAML"
    python3 "$AUTO_LINK_GENERATOR" --soc-yaml "$GC_SOC_YAML"
    python3 "$AUTO_LINK_GENERATOR" --soc-yaml "$GCA_SOC_YAML"
  else
    python3 "$CONTROL_GENERATOR" --soc-yaml "$CGRA_SOC_YAML" --check
    python3 "$AUTO_LINK_GENERATOR" --soc-yaml "$GC_SOC_YAML" --check
    python3 "$AUTO_LINK_GENERATOR" --soc-yaml "$GCA_SOC_YAML" --check
  fi
fi

if uses_cgra_spm; then
  if ((REBUILD)); then
    echo "[generate] CGRA SPM window"
    python3 "$CGRA_SPM_GENERATOR" --soc-yaml "$CGRA_SOC_YAML"
  else
    python3 "$CGRA_SPM_GENERATOR" --soc-yaml "$CGRA_SOC_YAML" --check
  fi
fi

if uses_aes_auto; then
  if ((REBUILD)); then
    echo "[generate] AES automatic job"
    python3 "$AES_JOB_GENERATOR" --soc-yaml "$CGRA_SOC_YAML"
  else
    python3 "$AES_JOB_GENERATOR" --soc-yaml "$CGRA_SOC_YAML" --check
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
SOURCES=("$TEST_SRC")
if uses_aes_manual; then
  SOURCES+=("$AES_SW/accellib.c")
fi
riscv64-unknown-elf-gcc \
  -std=gnu99 -O2 -Wall -Wextra -fno-common -fno-builtin-printf \
  -march=rv64imafd -mabi=lp64d -mcmodel=medany \
  -I "$ROOT_DIR/tests/include" \
  -I "$ROOT_DIR/tests" \
  -I "$ROOT_DIR/tests/cgra-gemmini" \
  -I "$AES_SW" \
  -I "$CHIPYARD_DIR/tests" \
  -I "$GEMMINI_SW" \
  -I "$GEMMINI_SW/include" \
  -I "$GEMMINI_SW/rocc-software/src" \
  -I "$GEMMINI_SW/riscv-tests/env" \
  -I "$GEMMINI_SW/riscv-tests/benchmarks/common" \
  -specs="$CHIPYARD_DIR/toolchains/libgloss/util/htif_nano.specs" \
  -static -T "$CHIPYARD_DIR/tests/htif.ld" \
  "${SOURCES[@]}" \
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
