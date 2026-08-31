# Accelerator execution modes

The SoC generator supports two ways to coordinate accelerators: Manual and Automatic. The selected hardware configuration fixes the mode during elaboration. There is no runtime mode bit.

Both modes keep accelerator-local interfaces and the SoC data interconnect. Automatic mode adds a control protocol and endpoint adapters. It does not replace TileLink, a NoC, or another payload network.

## Modes

### Manual mode

Manual mode instantiates no automatic control fabric, task table, or automatic endpoint adapters. The CPU coordinates the full pipeline through each accelerator's native control interface.

1. The CPU configures and starts a producer.
2. The CPU polls or waits for producer completion.
3. The CPU explicitly starts the required data transfer.
4. The existing memory interconnect routes the payload from the selected source address to the consumer's local memory.
5. The CPU starts the consumer and waits for its result.
6. The CPU repeats these steps for later stages.

The source can be an accelerator SPM, DRAM, or another addressable memory. Manual mode does not require a specific staging location.

### Automatic mode

Automatic mode adds the control fabric, one adapter per participating accelerator, and an elaborated dependency table. The CPU still configures the accelerators and starts the entry stage. Hardware then manages dependencies, transfers, downstream launches, and the final result.

1. The fabric arms each producer output required by the task table.
2. The CPU configures the participating accelerators and starts the entry producer.
3. The producer adapter reports when its watched output is committed.
4. The fabric issues copy requests for every dependent consumer.
5. Each consumer adapter pulls its input through the existing data interconnect and reports completion.
6. The fabric launches a consumer after all of its input copies succeed.
7. The same sequence continues through later stages, and the final completion is returned to the CPU.

The CPU does not poll intermediate stages or start intermediate transfers. A failed publication or copy prevents the dependent compute from starting and returns an error.

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
| `watchOutput` | Fabric to producer | Address, bytes | Arm one output range. |
| `reportOutput` | Producer to fabric | Status, detail, data | Report publication success or failure. |
| `requestCopy` | Fabric to consumer | Task, source address, destination offset, bytes | Ask the consumer to pull one input. |
| `reportCopy` | Consumer to fabric | Task, status, detail | Report transfer completion. |
| `requestCompute` | Fabric to consumer | Start | Start after all dependencies succeed. |
| `reportCompute` | Consumer to fabric | Status, detail, data | Return compute completion. |

`AutoEndpointAsyncLink` wraps the same channels with asynchronous queues when the fabric and accelerator use different clock domains.

The copy request uses a global source address and a destination-local offset. The destination adapter owns its local memory map and translates the offset for its DMA or SPM interface. The request therefore does not need a destination physical address.

### Task and routing model

`AutoLinkParams` describes endpoints, physical links, copy tasks, interface widths, and descriptor queue depth. Each copy task selects one physical link and provides a source offset, destination offset, and byte count.

`AutoCopyTask` waits for a producer publication, sends one copy request, waits for the copy result, and reports a dependency event. `AutoJoin` collects dependency events for one consumer and requests compute only when every input succeeds.

The fabric buffers copy descriptors only. It never buffers payload data and has no TileLink port. The destination adapter initiates the transfer, and the configured memory interconnect resolves the global source address. The payload network can change without changing the endpoint protocol.

### Mode selection and generation

Manual configurations omit `AutoLinkKey` and the automatic adapters. Automatic configurations provide `AutoLinkParams` and attach each configured endpoint to `AutoLinkFabric`.

The SoC YAML lists each automatic task's source, destination, and byte count. `scripts/generate_auto_links.py` infers the links, endpoints, offsets, and task table, then emits complete `AutoLinkParams` for elaboration. Runtime task programming is not implemented.

The generic implementation is under `chipyard.socgen.link`. Accelerator-specific adapters live in their matching `chipyard.socgen` subpackage and contain only the translation between `AutoEndpointIO` and the IP's existing interfaces. Integration configuration instantiates and connects these pieces but must not duplicate protocol or routing logic.

Each IP keeps its native configuration interface. Its adapter maps only the control behavior required by the selected mode.

### AES streaming behavior

AES starts reading when its adapter accepts `requestCopy`. The adapter holds `reportCopy` until AES has read all input data. The later `requestCompute` is a continuation barrier rather than a second launch, and `reportCompute` waits until the running job has completed and its ciphertext and completion writes have drained.

Payload moves directly over TileLink. AutoLink has no payload staging buffer.

## Current validation

- Two-IP Manual: the CPU runs Gemmini, starts the CGRA transfer, and launches CGRA.
- Two-IP Automatic: AutoLink coordinates one 128-byte Gemmini external SPM to CGRA SPM transfer and CGRA launch.
- Three-IP Manual: the CPU runs Gemmini, CGRA, and AES in order for one 128-byte chunk.
- Three-IP Automatic: AutoLink coordinates the fixed sequential Gemmini to CGRA to AES pipeline for one 128-byte chunk.

## TODO

- Generate endpoint attachment parameters from the SoC configuration.
- Add runtime task programming when its software contract is defined.
- Add Hybrid execution.
- Support overlap, multiple publication ranges, chunks, kernels, and concurrent task graphs.
- Add adapters for more accelerators and transfer directions.
