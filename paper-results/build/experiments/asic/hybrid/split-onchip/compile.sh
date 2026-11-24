#!/usr/bin/env bash
set -euo pipefail
cd /home/jjqin/CGRA-SoC
name=$1
chipyard/.conda-env/riscv-tools/bin/riscv64-unknown-elf-gcc \
  -MD -MF "build/experiments/asic/hybrid/split-onchip/$name.d" -std=gnu99 -O2 -fno-common -fno-builtin-printf \
  -march=rv64imafd -mabi=lp64d -mcmodel=medany \
  -include /home/jjqin/CGRA-SoC/build/experiments/asic/hybrid/split-onchip/include/accel_generated.h \
  -include /home/jjqin/CGRA-SoC/build/experiments/asic/hybrid/split-onchip/include/auto_link_generated.h \
  -include /home/jjqin/CGRA-SoC/build/experiments/asic/hybrid/split-onchip/include/cgra_link_control_generated.h \
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
  "/home/jjqin/CGRA-SoC/build/experiments/asic/hybrid/split-onchip/$name.c" \
  -o "/home/jjqin/CGRA-SoC/build/experiments/asic/hybrid/split-onchip/$name.riscv"
