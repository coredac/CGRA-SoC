# Six-family arithmetic capability trial

This isolated, uncommitted trial expands all six independent ASIC families. Every elaborated endpoint has a runtime-accessible sixteen-lane arithmetic bank. Native kernel commands and primitive commands share hardware and are mutually exclusive. Active `rtl/asic` sources, SoC integration, cached descriptors and historical performance are unchanged. This is an expanded arithmetic-capability baseline, not an equal-throughput result or a fully CGRA-equivalent implementation.

| Family parameter | Native operation retained | Native use of shared bank | Other native arithmetic retained | Public primitive lanes |
| --- | --- | --- | --- | ---: |
| 0 Add | Fixed INT32 streaming addition | Lane 0 performs each scalar addition; no separate native add datapath | Stream length and completion control | 16 |
| 1 ReLU | Fixed signed INT32 streaming ReLU | Lane 0 performs each scalar ReLU; no separate native ReLU datapath | Stream length and completion control | 16 |
| 2 GCN | CSR aggregation, optional final ReLU, sixteen-channel groups | All sixteen lanes perform native multiply batches and accumulate batches; no duplicate native multipliers or accumulation adders | CSR traversal, arrays, channel registers and final ReLU selection | 16 |
| 3 Norm | Existing H128 normalization phases and fused sequence | Lane 0 restoring unit replaces the original signed divider | Wide square, product, reductions, native integer-sqrt state machine, quantization and array access | 16 |
| 4 Softmax | Existing two-column causal Softmax phases | Lane 0 restoring unit replaces the original unsigned divider | Specialized exponent polynomial, maxima, reductions and local state | 16 |
| 5 GPT-vector | Existing GELU, requantization and residual-add operations | Lane 0 restoring unit replaces the original unsigned divider | Specialized constant multipliers, rounding, saturation and array access | 16 |

The six-family system would install **96 physical primitive lanes**, compared with one sixteen-PE CGRA. This is a scenario in which each independent endpoint must expose the expanded capability separately. It is not an equal total resource budget. The three GPT families retain their other specialized datapaths, so some operations overlap with the public primitive bank; this is an implementation choice to preserve those native schedules, not an unavoidable lower bound on capability cost. Norm still has its native sqrt state machine in addition to the bank's restoring units. The three original `GptDivider` instances are removed from the generated native copies and replaced by lane-zero service, so they are not counted a second time.

## Primitive contract

`cmd_valid && cmd_ready` accepts one packet with a sixteen-bit lane mask, sixteen independent five-bit opcodes, and four thirty-two-bit operands (`a`, `b`, `c`, `d`) per lane. Lane `i` uses bits `[i*32 +: 32]` and opcode bits `[i*5 +: 5]`. `rsp_valid` becomes true when all enabled lanes finish and remains true with stable results until `rsp_ready`. Disabled result lanes are unspecified; an empty mask completes as a no-op. Unsupported opcodes 24 through 31 return zero. The bank cannot accept a new packet while a response is pending.

| Opcode | Operation | Result |
| --- | --- | --- |
| 0 | Add | `a + b`, low 32 bits |
| 1 | Subtract | `a - b`, low 32 bits |
| 2 | Multiply | `a * b`, low 32 bits |
| 3 | Unsigned divide | `a / b`; zero divisor gives all-one quotient |
| 4 | Unsigned remainder | `a % b`; zero divisor returns `a` |
| 5 | Integer square root | `floor(sqrt(a))` for unsigned `a` |
| 6, 7, 8 | Bitwise AND, OR, XOR | Applied to `a`, `b` |
| 9 | Bitwise NOT | `~a` |
| 10 | Logical NOT | One if `a == 0`, otherwise zero |
| 11, 12, 13 | Left, logical-right, arithmetic-right shift | Shift `a` by the full unsigned `b` amount; opcode 13 treats `a` as signed |
| 14, 15 | Equality, inequality | Thirty-two-bit zero or one |
| 16, 17, 18 | Signed less-than, greater-or-equal, greater-than | Thirty-two-bit zero or one |
| 19 | Select | `c == 1 ? a : b`; CGRA selector input maps to `c`, its data inputs to `a/b` |
| 20 | ReLU | Signed negative `a` becomes zero |
| 21 | Pass | `a` |
| 22 | Increment | `a + 1`, low 32 bits |
| 23 | Two-dimensional GEP | `a + b*d + c`, low 32 bits |

Constant variants use the corresponding operand field with a literal value. One-dimensional GEP uses addition. GEP2D reuses the lane multiplier, then performs two registered additions. Division/remainder use thirty-two restoring steps; sqrt uses sixteen. Fast operations have registered input and result stages. A packet containing a slow operation waits for that lane before retiring the whole batch. These initiation intervals and barrier semantics do not reproduce independent CGRA instruction scheduling.

This covers the inspected CGRA arithmetic families: add/subtract/pass/increment, multiply, divide/remainder/sqrt, logic, shifts, comparisons, selection and address generation. It does not reproduce token predicates, grant/phi scheduling, control-slot programs, inter-PE routing or memory/load-store services. Memory and return/control operations are not silently charged as arithmetic units. ReLU and arithmetic shift additionally support native-family behavior. Thus arithmetic coverage must not be presented as complete CGRA programmability or protocol equivalence.

## Native interface and changed timing

`native_start && native_ready` launches the selected family. A simultaneously presented primitive packet has priority; hold or retry a native request until accepted. Native preload writes are blocked while the primitive bank is busy or a primitive packet is presented. Native operand ranges, legal shapes and nonempty-CSR requirements remain those of the original family. Add/ReLU use `length`, scalar `left/right`, `in_valid/in_ready`, and `out_valid/out_ready`. GCN uses the existing load-bank convention, `nodes`, `fused` and scalar result handshake. GPT families use their original phase/operation and local-array interfaces, exposed through the common wrapper.

Add/ReLU compatibility streams still execute one scalar value through lane zero and now take multiple bank cycles per value. GCN keeps sixteen native channels but its multiply and accumulate states now wait for shared-bank responses, changing latency. Norm, Softmax and GPT-vector retain scalar native kernel schedules; exposing sixteen primitive lanes does not make those kernels sixteen-wide. Their native division busy/done/result timing is preserved by the proved restoring-unit replacement. No old performance measurement is valid for the newly scheduled kernels. No simulator or performance experiment was run.

## Validation and reproduction

`prepare.py` snapshots the native source files and exposes shared arithmetic interfaces in the copies. `check.py` translates all sources and runs Verilator lint plus Yosys structural synthesis for each of the six `FAMILY` values. Every variant passes lint with no warnings. The resource inventory finds sixteen `ArithmeticLane` and sixteen `Restoring` instances in each variant; multiplication operators including retained native specializations are 16, 16, 16, 18, 20 and 19 respectively. Array capacities are unchanged by the arithmetic extension. `resources.json` records source hashes, counts and limitations.

The first direct thirty-five-cycle divider SAT attempt timed out at 120 seconds. A relational invariant then proved native division equivalence by temporal induction: after reset, arbitrary 32-bit operands, signedness and start patterns produce the same busy, done and result as the original `GptDivider`. The invariant relates shifted numerator/partial quotient, remainder bounds, sign and iteration count. This proof covers division mode, including the original zero-divisor and wrap behavior; it does not prove sqrt, remainder, the new primitive interface, whole kernels or SoC integration.

From the repository root, the retained proof command is:

```sh
OpenFPGA/yosys/yosys -s build/experiments/asic/hybrid/capability-trial/checks/divider-invariant.ys
```

The successful proof log is `checks/divider-invariant.log`; the timed-out attempt remains `checks/divider-formal.log` with its script `checks/divider-bounded.ys`. A root-agent source review checked native/primitive arbitration and GCN multiply/accumulate handshakes and corrected SELECT from nonzero to exactly one before final checks. The abandoned synthesis inputs/log are retained under `aborted-select/`; they have no reported area result. Source-level review and a divider proof do not establish whole-design functional correctness. Functional simulation remains outstanding under the user's no-simulator instruction.

## Technology mapping scope

Only the corrected standalone `PrimitiveBank` is technology-mapped. Its top retains the native signed-division service interface, so integrated families may simplify unused frontend logic. The six expanded endpoints and their new integration logic have not been separately technology-mapped. Do not multiply the standalone bank area by six and label that as measured total ASIC area, or combine it with historical cycle counts as a new PPA result.

`job.json` records the remote job and exact Verilog hash. Mapping uses SAED14 RVT TT 0.8 V 25 C, a two-nanosecond ideal clock, 0.1 ns I/O delay and input transition, output load 1 in library units, maximum transition 0.2 ns, and maximum fanout 16. `synth.tcl` is retained. Final mapping results belong in `results.json`; a success marker proves mapping completion only, and timing/constraint reports determine whether two nanoseconds was met. Ideal-clock wire-load synthesis is not physical signoff.

The completed corrected bank maps to **33,673.447781 library cell-area units**, including 28,600.259445 combinational and 5,073.188336 sequential area. It has no memory macros. At the two-nanosecond target, worst setup slack is **+0.013344 ns**, with zero violating setup paths and no reported constraint violations. The worst path runs from `lane[1].unit/divider/square_root_reg` to `lane[1].unit/divider/result_reg[31]`. The report uses wire-load estimation, including an assumed fanout of 1000 for two high-fanout nets; this meets the stated synthesis constraints, not post-layout signoff. Exact numbers and source hash are in [results.json](results.json), with [area](area.rpt), [setup](setup.rpt), and [constraint](constraints.rpt) reports retained.
