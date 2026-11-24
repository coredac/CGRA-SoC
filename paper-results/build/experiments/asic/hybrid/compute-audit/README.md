# Compute and control area audit

This audit reuses the original mapped CGRA and general five-unit ASIC endpoints. It does not use the split Add/ReLU endpoint or the new Softmax pipeline. Area is in SAED14 library units. Both original endpoints fail 2 ns; this is a structural area accounting result, not matched-frequency PPA.

The comparison removes only SRAM macros and named bulk configuration/data array flip-flops. It retains storage read/write muxes, decode, address logic, operand register files, FIFO/queue registers, prologue counters, FSMs, pipeline registers, validity state, interconnect and other control. Thus it is **compute plus control excluding identified bulk storage cells**, not a newly synthesized accelerator with memories removed.

| Boundary | CGRA | ASIC | CGRA/ASIC |
| --- | ---: | ---: | ---: |
| Complete endpoint | 1,264,551.83 | 402,725.45 | 3.140× |
| Core only | 1,178,924.43 | 385,209.52 | 3.060× |

The core boundary is `cgra/cgra_inst/cgra` versus `AsicCore`. The endpoint additionally includes the respective adapters, DMA and interface shell. Their retained contributions are 85,627.40 for CGRA and 17,515.93 for ASIC. The common Gemmini is outside both endpoints. This implementation does not show a CGRA area advantage under this boundary.

## Retained core breakdown

| CGRA component | Area |
| --- | ---: |
| FU subtrees, including internal control | 148,887.54 |
| FU selection logic outside FU subtrees | 46,025.00 |
| Routing crossbars | 134,361.02 |
| FU crossbars | 32,434.64 |
| Operand register clusters | 70,099.87 |
| PE configuration access and local control | 264,385.39 |
| Constant-store access logic | 71,800.04 |
| Control ring, including operational packet buffers | 303,709.63 |
| Data-SPM access logic | 69,080.32 |
| Other core control and channels | 38,141.98 |
| Total | 1,178,924.43 |

The control ring contains 53,856 packet-buffer flip-flops with area 62,169.59; the remaining 241,540.04 includes buffer access, routing, arbitration and control. These queues carry operational control traffic and remain in the compute/control boundary. Subtracting the entire control ring would remove required implementation logic.

| ASIC component, retaining local-array access logic | Area |
| --- | ---: |
| GCN | 128,104.97 |
| LayerNorm | 31,718.34 |
| Softmax | 2,593.36 |
| GPT vector | 169,213.24 |
| Vector | 416.87 |
| Data-SPM access logic | 52,112.90 |
| Core local control | 1,049.84 |
| Total | 385,209.52 |

The ASIC unit rows retain substantial read/write mux and decode logic around their local arrays. They are not arithmetic-only unit areas. Removing entire memory submodules would use a different boundary; the flattened GCN and LayerNorm arrays also prevent a symmetric whole-memory-module subtraction from these reports alone.

Each CGRA FU type below has 16 instances. These subtotals include internal FU control and sum to 148,887.54; they exclude the separately counted selection logic and crossbars.

| FU type | Installed area |
| --- | ---: |
| Add | 8,330.37 |
| Multiply | 30,289.32 |
| Divide | 11,829.05 |
| Logic | 5,058.67 |
| Shift | 9,110.13 |
| Compare | 6,305.11 |
| Select | 5,654.12 |
| Grant | 10,922.53 |
| Phi | 8,600.01 |
| Memory interface | 7,205.01 |
| Return | 7,541.52 |
| GEP address generation | 37,099.88 |
| Nah | 941.81 |

## Configuration storage

The adapter packet/patch cache and the PE control store are distinct. CGRA adapter caches use SRAM macros, area 2,225,288.81. PE control stores are implemented with standard-cell flip-flops: the `ctrl_mem/reg_file` payload contains 85,248 mapped flip-flops, area 98,410.29. Their complete register-file subtrees occupy 251,806.12, so removing just the storage bits retains 153,395.83 of access logic. The rest of the `ctrl_mem` subtrees contributes another 110,989.56 of local control, queues, counters and state. Hence the previous 362,795.68 `ctrl_mem` figure included both storage and control.

CGRA constant storage contributes another 32,480 flip-flops, area 37,494.69. Its adapter job-descriptor payload arrays contribute 2,240 flip-flops, area 2,585.86. ASIC also has configuration: its resident descriptor/next-job arrays contribute 5,440 flip-flops, area 6,279.94. Its identified local data arrays contribute 190,176 flip-flops, area 197,656.01. Validity flags, prologue counters and live command/staging registers remain classified as control on both sides.

## Reproduction and validation

`inventory.tcl` reads the original mapped DDC checkpoints and exports leaf instance name, library reference, area and sequential status. It does not compile or change the design. `classify.py` applies the explicit regex rules in `results.json` and reconciles the leaf-area sums with the original hierarchy reports. Both inventories have unique instance paths and reconcile within 0.003 library area units, reflecting printed precision. `remote.sha256` and the JSON hashes bind the inventories to the original DDCs. The DC process completed successfully; `inventory.log` and `SUCCESS` retain its exit evidence.

Run `python3 build/experiments/asic/hybrid/compute-audit/classify.py` from the repository root. Raw leaf inventories, hierarchy selections, per-FU areas and both boundary subtotals are retained alongside this note. The current FU ordering has add, mul, div, logic, shift, compare, select, grant, phi, memory, return, GEP and the appended Nah unit on each of 16 PEs.
