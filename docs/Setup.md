### Project Setup Documentation

**Clone the repository**

```shell
$ git clone https://github.com/coredac/CGRA-SoC.git
$ cd /path/to/CGRA-SoC
# Initialize the submodules: chipyard, VectorCGRA
$ git submodule sync -- chipyard VectorCGRA
$ git submodule update --init -- chipyard VectorCGRA
```

**Update VectorCGRA submodules**

```shell
$ cd VectorCGRA/
$ git submodule update --init --recursive
```

**Update chipyard submodules**

```shell
$ cd chipyard
$ git submodule update --init --recursive generators/hardfloat generators/constellation
$ git submodule update --init generators/bar-fetchers generators/boom \
      generators/diplomacy \
      generators/icenet \
      generators/rerocc \
      generators/rocc-acc-utils \
      generators/rocket-chip \
      generators/rocket-chip-blocks \
      generators/rocket-chip-inclusive-cache \
      generators/shuttle \
      generators/testchipip \
      sims/firesim \
      toolchains/libgloss \
      tools/cde \
      tools/dsptools \
      tools/firrtl2 \
      tools/fixedpoint \
      tools/install-circt \
      tools/rocket-dsp-utils
```

**Install conda/miniconda** — conda is required for the chipyard build process.

**Build chipyard**

```shell
$ cd chipyard/
$ ./scripts/build-setup.sh \
      --use-lean-conda \
      --skip-submodules \
      --skip-ctags \
      --skip-precompile \
      --skip-firesim \
      --skip-marshal \
      --skip-clean
```

> [!IMPORTANT]
> During the build process, chipyard automatically checks for an existing Verilator installation. If Verilator is present, chipyard will reuse it. The version required by chipyard is >5.022. You have two options:
>
> 1. Install a high-version Verilator (>5.022) in advance.
> 2. Mask the current Verilator environment variables and let chipyard install Verilator automatically.

**Verify the chipyard installation**

```shell
$ verilator --version   # Should be >5.022
$ firtool --version     # Should be 1.75
$ riscv64-unknown-elf-gcc --version   # Should be 13.2
```

**Conda environment**

After chipyard is successfully built, you can find its conda environment:

```shell
$ conda env list
# conda environments:
/path/to/CGRA-SoC/chipyard/.conda-env
```

For convenience, you can create a symbolic link to easily activate the environment, e.g.,

```shell
$ ln -s /path/to/CGRA-SoC/chipyard/.conda-env /path/to/miniconda3/envs/cgra-soc
```

This allows you to activate the chipyard environment using `conda activate cgra-soc`.

**Install Python packages (pymtl, etc.)**

```shell
$ conda activate cgra-soc
$ pip install py==1.11.0 \
    "git+https://github.com/tancheng/pymtl3.1@yo-struct-list-fix" \
    hypothesis \
    pytest \
    py-markdown-table \
    PyYAML
```
