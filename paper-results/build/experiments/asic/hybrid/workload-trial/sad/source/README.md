# Optional expanded SAD endpoint

`ExpandedSad` adds a native absolute-difference reduction controller to one unchanged sixteen-lane `PrimitiveBank`. Native work and public primitive commands share the same arithmetic units. The controller does not duplicate the bank's adders or add a separate sixteen-lane SAD datapath. The frozen Verilog top is `ExpandedSad` in `all.v`; `manifest.json` records source identities.

This is an independently provisioned endpoint under the existing expanded-ASIC experiment's per-family bank policy. All required primitive operations were already available in the earlier six cores. Installing this endpoint buys an independent native sequencer and its own resources; it is not necessary merely to express SAD through software-issued primitive commands. Its area must not be described as an unavoidable cost of adding SAD support to a shared ASIC.

The repository workload is `configs/kernels/kernel_sad_4x4.yaml`, backed by `VectorCGRA/validation/test/sad.yaml` and `tests/cgra-sad-4x4.c`. Its reference case uses eight pairs, `A[i]=i+1`, `B[i]=i+4`, and returns 24. The endpoint accepts up to sixteen pairs per block, with `native_mask=16'h00ff` for that reference case. A wider input packet does not establish equal bandwidth, operator count or useful throughput to sixteen CGRA PEs, and the new endpoint has no measured workload performance.

## Native protocol and arithmetic

A native block is accepted when `native_valid && native_ready`. The first accepted block in idle clears the accumulator. Later blocks continue the same operation; `native_last` marks the final block. Each block's mask, last flag and bank operands are captured on acceptance. While waiting for additional blocks, native ownership remains active and public primitive commands are blocked. An all-zero mask contributes zero and is legal, including as the final block. A one-block operation sets `native_last=1` on its only accepted packet.

For each enabled lane, subtraction first computes `d=(B-A) mod 2^32`. If bit 31 is set, the absolute-value phase computes `0-d mod 2^32`; otherwise it passes `d`. Pairwise sums reduce sixteen lanes to eight, four, two and one, followed by addition to the accumulated result. Every arithmetic step uses the shared bank and wraps to 32 bits. `0x80000000` therefore remains `0x80000000` through absolute value. There is no saturation or widened signed difference.

The final `native_result` remains stable with `native_rsp_valid=1` until `native_rsp_ready` accepts it. Public commands regain access on the following idle cycle. A public command has priority if public and native command valid are both asserted while idle. Pending public responses drain before native acceptance. Synchronous reset cancels work, clears native state/result and resets the shared bank. Inputs outside their acceptance handshake do not update a native block.

There are no bulk local arrays or configuration stores to subtract. The block mask, reduction values, accumulator, pipeline state and bank registers are runtime state and remain charged. DMA, endpoint adapters and external SPM are outside this standalone boundary.

## Validation

`validation.json` records successful sv2v translation, warning-free Verilator lint, and Yosys structural checking with zero reported problems or retained latches. Structural inventory retains one `PrimitiveBank`, sixteen `ArithmeticLane` instances and sixteen `Restoring` instances. The copied `Primitives.sv` is byte-identical to the earlier capability trial. `check_math.py` checks 4,222 software arithmetic cases, including signed overflow boundaries, masked lanes, random 32-bit inputs and the reference result 24. These checks are not RTL simulation.

`ControlProof.sv` proves seven assertions for 108 bounded steps on the actual `ExpandedSad.sv` controller with `BankModel.sv`, a two-cycle ADD/SUB/PASS transaction model. The concrete three-block sequence contributes 24, 9 and zero and returns 33. A conflicting public ADD command returns 30 before native work. Properties cover public/native exclusion, completion, final-result hold under arbitrary output readiness, and cancellation/restart after reset at step 18. This modeled-bank proof does not establish equivalence to the full frozen primitive bank, arbitrary-data whole-endpoint correctness, or exact bank latency; notably the real bank completes a zero-mask request earlier than the model. The proof script reads SystemVerilog directly, lowers edge-triggered checks with `async2sync` and `chformal`, and reports SAT SUCCESS. An initial sv2v formal translation discarded assertions and was not used as evidence; the initial direct-parser attempt required check lowering.

No simulator, performance workload, active hardware generation or remote synthesis was run by this source preparation. No active ASIC or VectorCGRA file was changed and no commit was made. Full system functionality and performance of the added endpoint remain unverified.

Reproduce the static checks from the repository root:

```sh
sv2v build/experiments/asic/hybrid/workload-trial/sad/source/ExpandedSad.sv build/experiments/asic/hybrid/workload-trial/sad/source/Primitives.sv > build/experiments/asic/hybrid/workload-trial/sad/source/all.v
chipyard/.conda-env/bin/verilator --lint-only --top-module ExpandedSad -Wno-fatal build/experiments/asic/hybrid/workload-trial/sad/source/ExpandedSad.sv build/experiments/asic/hybrid/workload-trial/sad/source/Primitives.sv
python build/experiments/asic/hybrid/workload-trial/sad/source/check_math.py
```

Run `proof.ys` with Yosys from this source directory to reproduce the bounded modeled-bank proof. `all.v` contains only the endpoint and complete frozen primitive implementation; proof modules are excluded from synthesis.
