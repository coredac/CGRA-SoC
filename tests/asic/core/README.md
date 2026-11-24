# ASIC local-memory bridge

Run `python3 scripts/asic/core.py` from the repository root. The runner builds real SystemVerilog and checks the existing GCN ring16, small projected ResNet and GPT N2/H128 fixtures. It writes results, source hashes and build/run commands under `build/experiments/asic/core/`.

`AsicCore` contains 32768 32-bit words with one synchronous read and one write port. Read/write collisions return the old word. Reset clears execution state, not memory contents. The external memory interface accepts reads and writes only while idle; `rvalid` indicates the read response one clock edge after the request. The system adapter must serialize DMA and core execution. Descriptor inputs latch when `start` is accepted while idle. `done` pulses once after final local output storage, with `busy` low.

All addresses are local 32-bit word offsets. The complete input/output ranges must fit memory. In-place elementwise operation is supported when source and destination start at the same address; arbitrary partial overlap is outside the contract. Counts are positive, divisors nonzero, GCN has at most 16 nodes and 256 edges with positive-length CSR rows, and all kernels follow the existing integer arithmetic contracts.

| Unit | Function | Inputs and parameters | Output |
| --- | --- | --- | --- |
| 0 | GCN aggregation | `src0`: row pointers; `src1`: columns; `src2`: edge weights; `src3`: projected features; `rows`: node count; `edges`: edge count; `fused`: output ReLU | `rows * 128` words |
| 1 | Vector | `src0`: left; `src1`: right for binary operations; `operation`: 0 ReLU, 1 Add, 2 Add+ReLU; `count`: elements | `count` words |
| 2 | H128 LayerNorm | `src0`: input; `src1`: gamma; `src2`: beta; `fused` or `phase` 0–6 | 128 words for fused execution or phases 5/6 |
| 3 | Two-column causal Softmax | `src0`: scores; `rows[0]`: causal row 0/1; `fused` or `phase` 0–2 | Two words for fused execution or phase 2 |
| 4 | GPT vector | `src0`: input; `src1`: residual operand; `operation`: 0 GELU, 1 requantization, 2 saturated residual; `count`: 1–1024; `divisor`: requantization scale | `count` words |

Norm and Softmax retain intermediate native state across phase commands. Do not interleave a different row's phases on the same unit. Reduction-only phases do not copy uninitialized result arrays to local memory. Other input arrays are reloaded from local memory before each execution; this load cost is included in core cycles. Local chaining can use the previous job's destination directly, avoiding a host copy, while still paying real local-memory accesses and native preload costs.

The tests execute 159 descriptors without resetting between jobs and check 24,412 outputs. They exercise all five units, fused and phased kernels, local GCN aggregation→ReLU and ResNet Add→ReLU, descriptor capture, the final physical memory word and read-first behavior. The separate restoring-divider test covers signed/unsigned boundaries, deterministic varied operands, input capture and reset during an unfinished division.

Run the divider check from the repository root:

```sh
chipyard/.conda-env/bin/verilator --cc --exe --build -j 4 -Wall -Wno-DECLFILENAME --top-module GptDivider --Mdir build/experiments/asic/core/divider rtl/asic/gpt/div.sv "$PWD/tests/asic/core/div.cpp"
build/experiments/asic/core/divider/VGptDivider
```

This is a component test, not a CPU, AutoLink or TileLink performance test. Technology timing remains to be qualified after integration. The GPT kernels use a real 32-step divider for variable division; constant divide-by-four GELU and residual arithmetic retain their direct datapaths.
