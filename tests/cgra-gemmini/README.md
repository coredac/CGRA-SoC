# CGRA and Gemmini tests

Tests are grouped by workload. Each `.c` file is a runner entry point; adjacent headers contain workload-private implementation. `common/` holds helpers shared across workloads. Generated APIs and fixtures remain in `tests/generated/`, and device/runtime interfaces remain in `tests/include/`.

| Directory | Contents |
| --- | --- |
| `gcn/` | GCN Manual, Auto, Hybrid and grouped task reuse |
| `gcn/experiments/` | Channel, two-input and node-readiness pipeline implementations selected by `gcn.c` |
| `gpt2/` | GPT-2 block modes, Attention, MHA and component regressions |
| `resnet/` | ResNet and projected residual-block tests |
| `integration/` | Small Gemmini/CGRA/AES/Pool, DMA and runtime-graph tests |
| `common/` | Shared native GEMM helper |

The runner compiles one test entry point with its included implementation headers. Headers are not independent executables; GCN experiment headers share the state defined by `gcn/gcn.c`. Directory organization does not change mode numbers or scheduling. `resnet/block_auto.c` tests a projected residual block; `gpt2/auto_block.c` runs the whole GPT-2 block automatically.

Pass the source path to the root runner, or set `TEST_SRC`. For example, with matching hardware and generated APIs already available:

```shell
CONFIG=MultiAccelRocketConfig BINARY_ARGS='2 10 1' RANDOM_SEED=1 LOADMEM=1 timeout_cycles=8000000 ./run-chipyard-cgra-gemmini-demo.sh --arch-yaml configs/arch/attention.yaml --soc-yaml configs/soc/autolink/gpt2.yaml tests/cgra-gemmini/gcn/gcn.c
```

See the [root README](../../README.md) for fixture generation, hardware selection and other test commands. Changing hardware configuration still requires `--rebuild`; moving test sources does not.
