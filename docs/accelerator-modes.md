# Accelerator execution modes

Accelerators can run under CPU control (Manual), AutoLink control (Automatic), or a combination (Hybrid). The hardware configuration determines whether AutoLink is present; software selects the supported execution flow without a global mode bit.

All modes keep accelerator-local interfaces and the SoC data interconnect. Automatic mode adds a control protocol and endpoint adapters. It does not replace TileLink, a NoC, or another payload network.

## Modes

### Manual mode

The CPU coordinates the full pipeline through each accelerator's native control interface. Manual-only configurations omit AutoLink; configurations with AutoLink retain native manual control. Software must not issue competing manual and automatic work to the same IP.

1. The CPU configures and starts a producer.
2. The CPU polls or waits for producer completion.
3. The CPU explicitly starts the required data transfer.
4. The existing memory interconnect routes the payload from the selected source address to the consumer's local memory.
5. The CPU starts the consumer and waits for its result.
6. The CPU repeats these steps for later stages.

The source can be an accelerator SPM, DRAM, or another addressable memory. Manual mode does not require a specific staging location.

### Automatic mode

AutoLink adds a control fabric and endpoint adapters. Its dependency table starts from the generated default graph; CPU-rooted configurations also accept runtime graphs within the generated capacities. Both use the same scheduler and ownership tracking. The CPU configures IP jobs and the graph before hardware manages dependencies, transfers and execution.

1. The CPU configures native jobs, graph bindings and tile parameters, then signals `auto_link_input_ready()`.
2. The fabric schedules ready stages and arms their required output publications.
3. Producer adapters report output readiness after their writes complete.
4. Consumer adapters pull data through the existing memory interconnect and report completion.
5. The fabric requests computation once the stage's dependencies succeed.
6. Tiles reuse the configured jobs and free storage slots; stage results summarize the completed run.

The CPU does not poll intermediate stages or start intermediate transfers. Publication and copy failures propagate through dependency results rather than being treated as successful completion.

Legacy externally triggered graphs still let the CPU launch their producer after output monitoring is armed. They do not support runtime graph replacement.

### Hybrid mode

The CPU configures or starts separate automatic runs at workload boundaries and may execute other stages manually. `blocks.c` compares CPU configuration, CPU-triggered cached runs and a hardware-controlled cached sequence. `resnet.c` uses automatic tiled execution inside projected residual blocks while its stem, ordinary block and tail remain CPU-controlled. These flows do not overlap different blocks.

## Implementation

The implementation separates four responsibilities:

- The protocol defines common dependency, copy, compute, and result messages.
- The fabric owns the task table, routes control messages, and joins dependencies.
- Each IP adapter converts common messages to that accelerator's native DMA, launch, and completion signals.
- The SoC memory interconnect carries payload data independently of the control fabric.

### Endpoint interface

An endpoint represents one accelerator instance at the automatic control boundary. It is not a memory port. All channels use ready/valid flow control.

| Channel | Direction | Fields | Meaning |
| --- | --- | --- | --- |
| `watchOutput` | Fabric to producer | Job, slot, tile, address, bytes, output binding | Arm one output publication. |
| `reportOutput` | Producer to fabric | Stage, job, status, detail, data | Report publication success or failure. |
| `requestCopy` | Fabric to consumer | Task, job, tiles, slots, source address, destination offset, byte lengths | Ask the consumer to consume one input. |
| `reportCopy` | Consumer to fabric | Task, status, detail | Report transfer completion. |
| `requestCompute` | Fabric to consumer | Job, slot, tile, start, input presence | Start after all dependencies succeed. |
| `reportCompute` | Consumer to fabric | Stage, job, status, detail, data | Return compute completion. |

`AutoEndpointAsyncLink` wraps the same channels with asynchronous queues when the fabric and accelerator use different clock domains.

The copy request uses a global source address and a destination-local offset. The destination adapter owns its local memory map. CGRA copies into local SPM, Gemmini retains an input description for its captured native commands, and streaming endpoints start consuming data directly.

### Task and routing model

`AutoLinkParams` describes physical endpoints, default stages and dependencies, interface widths and table capacities. `AutoRun` holds a runtime snapshot of graph bindings, transfers, output descriptions and tile regions. `AutoLinkRoot` exposes MMIO configuration, launches and optional cached-run sequencing.

`AutoLinkFabric` connects per-stage `AutoStage` controllers and the shared `AutoScheduler`. They track dependencies and storage ownership, arbitrate physical IPs and route endpoint requests. A producer slot remains live until its computation and dependent reads finish. Independent IPs can process different tiles, but one IP executes only one computation at a time.

The fabric stores control descriptions, never tensor payloads. Named graph buffers lower to existing address and transfer registers; they do not instantiate memory or a DMA engine. TileLink resolves SPM or DRAM from the physical address. CGRA results stay in local SPM for downstream readers; explicit CPU DMA writeout remains available.

### Mode selection and generation

Manual-only configurations omit `AutoLinkKey` and the automatic adapters. Automatic configurations provide `AutoLinkParams` and attach the available endpoints to `AutoLinkFabric`.

The SoC YAML selects hardware and its default graph. `scripts/generate_auto_links.py` emits the elaboration parameters; `--graph-yaml` generates a runtime C descriptor against that hardware without changing RTL. Software calls `auto_link_load` with any symbolic address bindings, configures tile parameters and starts through `input_ready`. Runtime graphs must be DAGs and fit the generated IP, stage, dependency and job capacities. Replace a graph only after all results and the active run have drained.

The generic implementation is under `chipyard.socgen.link`. Accelerator-specific adapters live in their matching `chipyard.socgen` subpackage and contain only the translation between `AutoEndpointIO` and the IP's existing interfaces. Integration configuration instantiates and connects these pieces but must not duplicate protocol or routing logic.

Each IP keeps its native configuration interface. Its adapter maps only the control behavior required by the selected mode.

### AES streaming behavior

AES starts reading when its adapter accepts `requestCopy`. The adapter holds `reportCopy` until AES has read all input data. The later `requestCompute` is a continuation barrier rather than a second launch, and `reportCompute` waits until the running job has completed and its ciphertext and completion writes have drained.

Payload moves directly over TileLink. AutoLink has no payload staging buffer.

## Current validation

- Sequential Gemmini → CGRA and AES → Gemmini → CGRA → AES paths under Manual and Automatic control.
- Tiled Conv → ReLU → Pool and multi-instance pipelines with independent buffer slots.
- Projected residual blocks, cached block sequences and synthetic ResNet-8 Manual/Hybrid execution, including CGRA Softmax.
- Runtime graph replacement and address rebinding on one five-IP system, including SPM/DRAM transfers and explicit CPU raw/packed CGRA writeout.

See [README](../README.md) for commands and workload boundaries, and [hardware contracts](contracts.md) for supported interfaces and limitations.
