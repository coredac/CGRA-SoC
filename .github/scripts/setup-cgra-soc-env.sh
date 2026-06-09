#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CHIPYARD_DIR="$ROOT_DIR/chipyard"
VECTORCGRA_DIR="$ROOT_DIR/VectorCGRA"
OPENFPGA_DIR="$ROOT_DIR/OpenFPGA"
VENV_DIR="$ROOT_DIR/.venv"
OPENFPGA_VENV_DIR="$OPENFPGA_DIR/.venv"
OPENFPGA_LOCAL_DIR="$OPENFPGA_DIR/.local"
OPENFPGA_DEPS_DIR="$OPENFPGA_DIR/.deps"
CONDA_PREFIX_DEFAULT="${CONDA_PREFIX_DEFAULT:-$HOME/conda}"

have_cmd() {
  command -v "$1" >/dev/null 2>&1
}

ensure_root_submodules() {
  (
    cd "$ROOT_DIR"
    echo "Initializing root submodules: chipyard, VectorCGRA, OpenFPGA."
    git submodule sync -- chipyard VectorCGRA OpenFPGA
    git submodule update --init -- chipyard VectorCGRA OpenFPGA
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

ensure_openfpga_submodules() {
  (
    cd "$OPENFPGA_DIR"
    echo "Initializing OpenFPGA submodules needed for the AND2 fabric demo."
    git submodule sync -- vtr-verilog-to-routing
    git submodule update --init --recursive -- vtr-verilog-to-routing
  )
}

ensure_chipyard_submodules() {
  (
    cd "$CHIPYARD_DIR"
    echo "Initializing Chipyard submodules needed for CGRA and Gemmini tests."

    local -a recursive_submodules=(
      generators/hardfloat
      generators/constellation
    )
    local -a leaf_submodules=(
      generators/bar-fetchers
      generators/boom
      generators/diplomacy
      generators/gemmini
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

ensure_gemmini_submodules() {
  (
    cd "$CHIPYARD_DIR/generators/gemmini"
    echo "Initializing Gemmini software submodules needed for the CGRA demo."
    local -a gemmini_submodules=(
      software/gemmini-rocc-tests
    )
    git submodule sync -- "${gemmini_submodules[@]}"
    git submodule update --init -- "${gemmini_submodules[@]}"
  )

  (
    cd "$CHIPYARD_DIR/generators/gemmini/software/gemmini-rocc-tests"
    echo "Initializing Gemmini RoCC test support submodules."
    local -a gemmini_test_submodules=(
      rocc-software
      riscv-tests
    )
    git submodule sync -- "${gemmini_test_submodules[@]}"
    git submodule update --init -- "${gemmini_test_submodules[@]}"
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

ensure_openfpga_python() {
  if [[ ! -x "$OPENFPGA_VENV_DIR/bin/python" ]]; then
    rm -rf "$OPENFPGA_VENV_DIR"
    python3 -m venv "$OPENFPGA_VENV_DIR"
  fi

  PIP_CACHE_DIR="$OPENFPGA_DIR/.pip-cache" "$OPENFPGA_VENV_DIR/bin/python" -m pip install --upgrade pip wheel
  PIP_CACHE_DIR="$OPENFPGA_DIR/.pip-cache" "$OPENFPGA_VENV_DIR/bin/python" -m pip install -r "$OPENFPGA_DIR/requirements.txt"
}

ensure_openfpga_pkgconf() {
  if have_cmd pkg-config || [[ -x "$OPENFPGA_LOCAL_DIR/bin/pkg-config" ]]; then
    return
  fi

  echo "Building pkgconf locally for OpenFPGA."
  mkdir -p "$OPENFPGA_DEPS_DIR" "$OPENFPGA_LOCAL_DIR/bin"
  if [[ ! -d "$OPENFPGA_DEPS_DIR/pkgconf-src" ]]; then
    git clone --depth 1 --branch pkgconf-2.5.1 https://github.com/pkgconf/pkgconf.git "$OPENFPGA_DEPS_DIR/pkgconf-src"
  fi
  (
    cd "$OPENFPGA_DEPS_DIR/pkgconf-src"
    make -f Makefile.lite clean
    make -f Makefile.lite \
      SYSTEM_LIBDIR=/usr/lib \
      SYSTEM_INCLUDEDIR=/usr/include \
      PKG_DEFAULT_PATH="$OPENFPGA_LOCAL_DIR/lib/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig"
  )
  install -m 755 "$OPENFPGA_DEPS_DIR/pkgconf-src/pkgconf-lite" "$OPENFPGA_LOCAL_DIR/bin/pkgconf"
  ln -sf pkgconf "$OPENFPGA_LOCAL_DIR/bin/pkg-config"
}

system_tcl_headers_ready() {
  [[ -f /usr/include/tcl.h ]] || [[ -f /usr/include/tcl8.6/tcl.h ]]
}

ensure_openfpga_tcl() {
  if [[ -f "$OPENFPGA_LOCAL_DIR/include/tcl.h" ]] || system_tcl_headers_ready; then
    return
  fi

  echo "Building Tcl locally for OpenFPGA."
  mkdir -p "$OPENFPGA_DEPS_DIR" "$OPENFPGA_LOCAL_DIR"
  local tarball="$OPENFPGA_DEPS_DIR/tcl8.6.14-src.tar.gz"
  if [[ ! -f "$tarball" ]]; then
    curl -L --retry 3 -o "$tarball" "https://prdownloads.sourceforge.net/tcl/tcl8.6.14-src.tar.gz"
  fi
  if [[ ! -d "$OPENFPGA_DEPS_DIR/tcl8.6.14" ]]; then
    tar -xzf "$tarball" -C "$OPENFPGA_DEPS_DIR"
  fi
  (
    cd "$OPENFPGA_DEPS_DIR/tcl8.6.14/unix"
    ./configure --prefix="$OPENFPGA_LOCAL_DIR"
    make -j"${OPENFPGA_DEP_JOBS:-2}"
    make install
  )
}

ensure_openfpga_swig() {
  if have_cmd swig || [[ -x "$OPENFPGA_LOCAL_DIR/bin/swig" ]]; then
    return
  fi

  echo "Building SWIG locally for OpenFPGA."
  mkdir -p "$OPENFPGA_DEPS_DIR" "$OPENFPGA_LOCAL_DIR"
  local tarball="$OPENFPGA_DEPS_DIR/swig-4.2.0.tar.gz"
  if [[ ! -f "$tarball" ]]; then
    curl -L --retry 3 -o "$tarball" "https://prdownloads.sourceforge.net/swig/swig-4.2.0.tar.gz"
  fi
  if [[ ! -d "$OPENFPGA_DEPS_DIR/swig-4.2.0" ]]; then
    tar -xzf "$tarball" -C "$OPENFPGA_DEPS_DIR"
  fi
  (
    cd "$OPENFPGA_DEPS_DIR/swig-4.2.0"
    ./configure --prefix="$OPENFPGA_LOCAL_DIR"
    make -j"${OPENFPGA_DEP_JOBS:-2}"
    make install
  )
}

openfpga_env_ready() {
  [[ -x "$OPENFPGA_VENV_DIR/bin/python" ]] || return 1
  [[ -x "$OPENFPGA_DIR/build/openfpga/openfpga" ]] || return 1

  "$OPENFPGA_VENV_DIR/bin/python" - <<'PY' >/dev/null
import coloredlogs
import envyaml
import humanize
import pyverilog
PY
  "$OPENFPGA_DIR/build/openfpga/openfpga" --version >/dev/null
}

ensure_openfpga_env() {
  if openfpga_env_ready; then
    echo "Reusing existing OpenFPGA Python/build environment."
    return
  fi

  ensure_openfpga_python
  ensure_openfpga_pkgconf
  ensure_openfpga_tcl
  ensure_openfpga_swig

  echo "Building minimal OpenFPGA shell for the AND2 fabric demo."
  mkdir -p "$OPENFPGA_DIR/build"
  (
    cd "$ROOT_DIR"
    PATH="$OPENFPGA_LOCAL_DIR/bin:$PATH" \
    LD_LIBRARY_PATH="$OPENFPGA_LOCAL_DIR/lib:${LD_LIBRARY_PATH:-}" \
    BUILD_USING_CCACHE=off \
    cmake -S "$OPENFPGA_DIR" -B "$OPENFPGA_DIR/build" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH="$OPENFPGA_LOCAL_DIR" \
      -DOPENFPGA_IPO_BUILD=off \
      -DOPENFPGA_WITH_YOSYS=OFF \
      -DOPENFPGA_WITH_SLANG=OFF \
      -DOPENFPGA_WITH_SWIG=OFF \
      -DOPENFPGA_WITH_TEST=OFF \
      -DOPENFPGA_WITH_INSTALLER=OFF \
      -DOPENFPGA_INSTALL_DOC=OFF \
      -DOPENFPGA_READLINE_MODE=standard
    cmake --build "$OPENFPGA_DIR/build" --target openfpga -j"${OPENFPGA_BUILD_JOBS:-2}"
  )
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

  "$OPENFPGA_VENV_DIR/bin/python" - <<'PY'
import coloredlogs
import envyaml
import humanize
import pyverilog

print("openfpga_python_deps=ok")
PY
  "$OPENFPGA_DIR/build/openfpga/openfpga" --version
}

ensure_root_submodules
ensure_vectorcgra_submodules
ensure_openfpga_submodules
ensure_chipyard_submodules
ensure_gemmini_submodules
ensure_chipyard_env
ensure_top_venv
ensure_openfpga_env
check_environment
