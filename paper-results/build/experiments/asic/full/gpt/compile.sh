#!/usr/bin/env bash
set -euo pipefail
cd /home/jjqin/CGRA-SoC
chipyard/.conda-env/riscv-tools/bin/riscv64-unknown-elf-gcc \
  -MMD -MF build/experiments/asic/full/gpt/dependencies.d -std=gnu99 -O2 -fno-common -fno-builtin-printf \
  -march=rv64imafd -mabi=lp64d -mcmodel=medany \
  -include build/experiments/asic/full/gpt/include/accel_generated.h \
  -include build/experiments/asic/full/gpt/include/auto_link_generated.h \
  -include build/experiments/asic/full/gpt/include/cgra_link_control_generated.h \
  -I tests/include -I tests -I runtime/include -I tests/cgra-gemmini/common \
  -I chipyard/tests \
  -I chipyard/generators/gemmini/software/gemmini-rocc-tests \
  -I chipyard/generators/gemmini/software/gemmini-rocc-tests/include \
  -I chipyard/generators/gemmini/software/gemmini-rocc-tests/rocc-software/src \
  -I chipyard/generators/gemmini/software/gemmini-rocc-tests/riscv-tests/env \
  -I chipyard/generators/gemmini/software/gemmini-rocc-tests/riscv-tests/benchmarks/common \
  -specs=chipyard/toolchains/libgloss/util/htif_nano.specs \
  -specs=chipyard/toolchains/libgloss/util/htif_argv.specs \
  -static -T chipyard/tests/htif.ld \
  "build/experiments/asic/full/gpt/gpt.c" \
  -o "build/experiments/asic/full/gpt/gpt.riscv"
