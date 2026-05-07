#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CHIPYARD_DIR="$ROOT_DIR/chipyard"
VENV_DIR="$ROOT_DIR/.venv"
CONDA_PREFIX_DEFAULT="${CONDA_PREFIX_DEFAULT:-$HOME/conda}"

have_cmd() {
  command -v "$1" >/dev/null 2>&1
}

ensure_conda() {
  if have_cmd conda; then
    if [[ -z "${CONDA_EXE:-}" ]]; then
      export CONDA_EXE="$(conda info --base)/bin/conda"
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
  (
    set +u
    source "$CHIPYARD_DIR/env.sh" >/dev/null 2>&1
    set -u
    have_cmd riscv64-unknown-elf-gcc && have_cmd verilator
  )
}

ensure_chipyard_env() {
  ensure_conda

  if chipyard_env_ready; then
    echo "Reusing existing Chipyard environment."
    return
  fi

  echo "Creating Chipyard lean conda/toolchain environment."
  rm -rf "$CHIPYARD_DIR/.conda-lock-env" "$CHIPYARD_DIR/.conda-env"
  (
    cd "$CHIPYARD_DIR"
    ./scripts/build-setup.sh \
      --use-lean-conda \
      --skip-ctags \
      --skip-precompile \
      --skip-circt \
      --skip-firesim \
      --skip-marshal \
      --skip-clean \
      --github-token "${GITHUB_TOKEN:-null}"
  )
}

ensure_vectorcgra_venv() {
  if [[ ! -x "$VENV_DIR/bin/python" ]]; then
    rm -rf "$VENV_DIR"
    if command -v python3.9 >/dev/null 2>&1 && python3.9 -m venv "$VENV_DIR"; then
      :
    elif command -v python3 >/dev/null 2>&1 && python3 -m venv "$VENV_DIR"; then
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
    verilator --version
  )

  "$VENV_DIR/bin/python" - <<'PY'
import pymtl3
import yaml

print(f"pymtl3={pymtl3.__file__}")
print(f"yaml={yaml.__version__}")
PY
}

ensure_chipyard_env
ensure_vectorcgra_venv
check_environment
