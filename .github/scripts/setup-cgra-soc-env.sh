#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CHIPYARD_DIR="$ROOT_DIR/chipyard"
VECTORCGRA_DIR="$ROOT_DIR/VectorCGRA"
VENV_DIR="$ROOT_DIR/.venv"
CONDA_PREFIX_DEFAULT="${CONDA_PREFIX_DEFAULT:-$HOME/conda}"

have_cmd() {
  command -v "$1" >/dev/null 2>&1
}

ensure_root_submodules() {
  (
    cd "$ROOT_DIR"
    echo "Initializing root submodules: chipyard, VectorCGRA."
    git submodule sync -- chipyard VectorCGRA
    git submodule update --init -- chipyard VectorCGRA
  )
}

ensure_vectorcgra_submodules() {
  (
    cd "$VECTORCGRA_DIR"
    echo "Initializing VectorCGRA submodules needed for CGRA translation."
    local -a vectorcgra_submodules=(
      noc/PyOCN
      fu/pymtl3_hardfloat
      fu/fused_alu_fixedp/dp_fpfma
    )
    git submodule sync -- "${vectorcgra_submodules[@]}"
    git submodule update --init -- "${vectorcgra_submodules[@]}"
  )
}

ensure_chipyard_submodules() {
  (
    cd "$CHIPYARD_DIR"
    echo "Initializing Chipyard submodules needed for CGRARocketConfig."

    local -a recursive_submodules=(
      generators/hardfloat
      generators/constellation
    )
    local -a leaf_submodules=(
      generators/bar-fetchers
      generators/boom
      generators/diplomacy
      generators/icenet
      generators/rerocc
      generators/rocc-acc-utils
      generators/rocket-chip
      generators/rocket-chip-blocks
      generators/rocket-chip-inclusive-cache
      generators/shuttle
      generators/testchipip
      sims/firesim
      toolchains/libgloss
      tools/cde
      tools/dsptools
      tools/firrtl2
      tools/fixedpoint
      tools/install-circt
      tools/rocket-dsp-utils
    )

    git submodule sync -- "${recursive_submodules[@]}" "${leaf_submodules[@]}"
    git submodule update --init --recursive -- "${recursive_submodules[@]}"
    git submodule update --init -- "${leaf_submodules[@]}"
  )
}

ensure_conda() {
  if have_cmd conda; then
    if [[ -z "${CONDA_EXE:-}" ]]; then
      CONDA_EXE="$(conda info --base)/bin/conda"
      export CONDA_EXE
    fi
    return
  fi

  if [[ -x "$CONDA_PREFIX_DEFAULT/bin/conda" ]]; then
    export PATH="$CONDA_PREFIX_DEFAULT/bin:$PATH"
    export CONDA_EXE="$CONDA_PREFIX_DEFAULT/bin/conda"
    return
  fi

  "$CHIPYARD_DIR/.github/scripts/install-conda.sh" \
    --prefix "$CONDA_PREFIX_DEFAULT"
  export PATH="$CONDA_PREFIX_DEFAULT/bin:$PATH"
  export CONDA_EXE="$CONDA_PREFIX_DEFAULT/bin/conda"
}

chipyard_env_ready() {
  [[ -d "$CHIPYARD_DIR/.conda-env" ]] || return 1
  [[ -f "$CHIPYARD_DIR/env.sh" ]] || return 1
  [[ -f "$CHIPYARD_DIR/toolchains/libgloss/util/htif_nano.specs" ]] || return 1

  (
    set +u
    source "$CHIPYARD_DIR/env.sh" >/dev/null 2>&1
    set -u
    have_cmd riscv64-unknown-elf-gcc &&
      have_cmd verilator &&
      have_cmd firtool &&
      [[ -f "$(riscv64-unknown-elf-gcc -print-file-name=libgloss_htif.a)" ]] &&
      [[ -f "$(riscv64-unknown-elf-gcc -print-file-name=htif.ld)" ]] &&
      [[ -f "$(riscv64-unknown-elf-gcc -print-file-name=htif_nano.specs)" ]]
  )
}

ensure_chipyard_env() {
  ensure_conda

  if chipyard_env_ready; then
    echo "Reusing existing Chipyard lean conda/toolchain environment."
    return
  fi

  echo "Creating Chipyard lean conda/toolchain environment."
  rm -rf "$CHIPYARD_DIR/.conda-lock-env" "$CHIPYARD_DIR/.conda-env"
  (
    cd "$CHIPYARD_DIR"
    ./scripts/build-setup.sh \
      --use-lean-conda \
      --skip-submodules \
      --skip-ctags \
      --skip-precompile \
      --skip-firesim \
      --skip-marshal \
      --skip-clean \
      --github-token "${GITHUB_TOKEN:-null}"
  )
}

ensure_top_venv() {
  if [[ ! -x "$VENV_DIR/bin/python" ]]; then
    rm -rf "$VENV_DIR"
    if have_cmd python3.9 && python3.9 -m venv "$VENV_DIR"; then
      :
    elif have_cmd python3 && python3 -m venv "$VENV_DIR"; then
      :
    else
      (
        set +u
        source "$CHIPYARD_DIR/env.sh" >/dev/null
        set -u
        python -m venv "$VENV_DIR"
      )
    fi
  fi

  "$VENV_DIR/bin/python" -m pip install --upgrade pip wheel
  "$VENV_DIR/bin/python" -m pip install \
    py==1.11.0 \
    "git+https://github.com/tancheng/pymtl3.1@yo-struct-list-fix" \
    hypothesis \
    pytest \
    py-markdown-table \
    PyYAML
}

check_environment() {
  (
    set +u
    source "$CHIPYARD_DIR/env.sh" >/dev/null
    set -u
    riscv64-unknown-elf-gcc --version >/dev/null
    test -f "$(riscv64-unknown-elf-gcc -print-file-name=libgloss_htif.a)"
    test -f "$(riscv64-unknown-elf-gcc -print-file-name=htif.ld)"
    test -f "$(riscv64-unknown-elf-gcc -print-file-name=htif_nano.specs)"
    verilator --version
    firtool --version
  )

  "$VENV_DIR/bin/python" - <<'PY'
import pymtl3
import yaml

print(f"pymtl3={pymtl3.__file__}")
print(f"yaml={yaml.__version__}")
PY
}

ensure_root_submodules
ensure_vectorcgra_submodules
ensure_chipyard_submodules
ensure_chipyard_env
ensure_top_venv
check_environment
