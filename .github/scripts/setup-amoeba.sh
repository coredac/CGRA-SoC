#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LLVM_COMMIT=6146a88f60492b520a36f8f8f3231e15f3cc6082
LLVM_SOURCE="$ROOT_DIR/build/llvm-project"
AMOEBA_BUILD="${AMOEBA_BUILD:-$ROOT_DIR/amoeba/build}"
BUILD_JOBS="${BUILD_JOBS:-2}"

# An explicit LLVM_BUILD reuses an existing build without modifying it.
if [[ -z "${LLVM_BUILD:-}" ]]; then
  LLVM_BUILD="$LLVM_SOURCE/build"
  if [[ ! -d "$LLVM_SOURCE/.git" ]]; then
    git init "$LLVM_SOURCE"
    git -C "$LLVM_SOURCE" remote add origin https://github.com/llvm/llvm-project.git
    git -C "$LLVM_SOURCE" fetch --depth=1 --filter=blob:none origin "$LLVM_COMMIT"
    git -C "$LLVM_SOURCE" checkout --detach FETCH_HEAD
  fi
  if [[ "$(git -C "$LLVM_SOURCE" rev-parse HEAD)" != "$LLVM_COMMIT" ]]; then
    echo "LLVM checkout must be at $LLVM_COMMIT: $LLVM_SOURCE" >&2
    exit 1
  fi

  cmake -G Ninja -S "$LLVM_SOURCE/llvm" -B "$LLVM_BUILD" \
    -DLLVM_ENABLE_PROJECTS="mlir;clang" \
    -DLLVM_TARGETS_TO_BUILD=Native \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -DLLVM_ENABLE_ASSERTIONS=ON \
    -DLLVM_ENABLE_RTTI=ON \
    -DLLVM_ENABLE_LLD=ON \
    -DLLVM_PARALLEL_LINK_JOBS=1 \
    -DLLVM_BUILD_EXAMPLES=OFF \
    -DMLIR_INCLUDE_TESTS=OFF \
    -DMLIR_INSTALL_AGGREGATE_OBJECTS=ON
  # mlir-opt also builds the dialect and conversion libraries linked by Amoeba.
  cmake --build "$LLVM_BUILD" --parallel "$BUILD_JOBS" \
    --target mlir-opt mlir-translate clang FileCheck not
fi

cmake -G Ninja -S "$ROOT_DIR/amoeba" -B "$AMOEBA_BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="$LLVM_BUILD/bin/clang" \
  -DCMAKE_CXX_COMPILER="$LLVM_BUILD/bin/clang++" \
  -DLLVM_DIR="$LLVM_BUILD/lib/cmake/llvm" \
  -DMLIR_DIR="$LLVM_BUILD/lib/cmake/mlir" \
  -DAMOEBA_ENABLE_NEURA=OFF
cmake --build "$AMOEBA_BUILD" --target mlir-amoeba-opt --parallel "$BUILD_JOBS"
