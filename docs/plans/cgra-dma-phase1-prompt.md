第一条强制要求：开始任何调查、修改或命令执行之前，先将本 prompt 原样保存到
  `docs/plans/cgra-dma-phase1-prompt.md`。如果发生上下文压缩、进程恢复或长时间中断，
  必须先重新读取该文件，再继续工作，确保目标、顺序和约束不漂移。不得用后续计划覆盖该文件。

  你位于 `/home/jjqin/CGRA-SoC`。VectorCGRA 当前应位于 `kernel-submit`，基线提交为
  `854d75d`，该提交已将 `tancheng/master` 语义合并进来并包含 DMA。先核对状态，但不要
  fetch、pull、reset、checkout 或回退已有改动。工作树已有无关修改和未跟踪文件，全部保留。

  ## 目标

  完成 Phase 1：让默认的单 CGRA SoC 使用 `IntegratedCgraWithDmaRTL`，并实现一个真正的
  C-level SoC demo：

  1. CPU 在普通内存中初始化输入。
  2. CPU 用一条语义化 RoCC 指令异步启动 DMA MVIN。
  3. DMA 将输入从 CPU 内存搬到 CGRA SPM。
  4. CGRA 执行现有 4x4 ReLU kernel。
  5. CPU 用一条语义化 RoCC 指令异步启动 DMA MVOUT。
  6. DMA 将结果从 SPM 写回普通内存。
  7. CPU 普通加载输出数组并验证全部结果。

  Phase 1 成功并完成检查后立即停止。不要实现、修改或运行 CGRA+Gemmini demo；那是用户
  审查本阶段后才开始的 Phase 2。

  ## 不可违反的约束

  - DMA 在默认单 CGRA 生成流程中启用；运行 `scripts/generate_single_cgra.py` 不需要额外
  DMA 开关。DMA 生成失败时直接失败，绝不能静默退回普通 `CgraTemplateRTL_single`。
  - demo 的数据搬入和搬出只能经过 DMA。不得调用 `relu4x4_store_fast()` 预载 SPM，不得调用
  `relu4x4_read_mem_fast()` 读取结果，不得 peek 内部 SPM，不得用 CPU memcpy 冒充 DMA。
  - 不得添加 fallback、兼容旁路、假内存、MMIO 替代路径、测试专用 bypass 或“超时也算成功”。
  - 不得手写 packet bit offset、packet word、DMA command ID 或重复的 RoCC funct magic number。
  每个协议常量只能有一个权威来源，其余 Scala/C/Verilog 内容必须生成。
  - 不得在 C 中逐 bit 构造 DMA packet。固定配置在编译期生成；CGRA kernel 配置继续直接加载
  生成的 packet 数组；DMA 描述符必须是编译期可计算的常量。
  - 不得静默截断地址、长度、tag 或 mask。布局无法容纳时应在生成期、编译期或 elaboration
  阶段明确失败。
  - 保持修改聚焦，不做无关重构，不改变 ReLU kernel 语义，不覆盖用户已有改动。

  ## 已确认的 DMA 协议

  VectorCGRA 内部一次 DMA 启动仍使用以下六个 packet，顺序固定：

  1. `CMD_DMA_CONFIG_DRAM_ADDR_LO`
  2. `CMD_DMA_CONFIG_DRAM_ADDR_HI`
  3. `CMD_DMA_CONFIG_SPM_ADDR`
  4. `CMD_DMA_CONFIG_BYTES`
  5. `CMD_DMA_CONFIG_TAG`
  6. `CMD_DMA_MVIN` 或 `CMD_DMA_MVOUT`

  `CMD_DMA_DONE` 是完成响应，不属于上述六个发送 packet。命令值必须取自
  `VectorCGRA/lib/cmd_type.py`，不能在 Chisel/C 中写入 44～51 等字面量。

  这六个 packet 只作为 VectorCGRA 内部协议和 PyMTL 单元测试接口。SoC C API 不显式发送它们。
  Chisel RoCC wrapper 接收一条高层 DMA 指令后，用硬件 packet sequencer 展开六个 packet。

  ## 生成链路

  将默认 4x4 YAML 单 CGRA生成顶层改为 `IntegratedCgraWithDmaRTL_single`，参数仍来自
  `configs/arch/arch.yaml` 和 `configs/soc/cgra_soc.yaml`，不能使用现有固定 2x2 DMA 测试 DUT。

  扩展 `scripts/generate_single_cgra.py`、`scripts/sync_cgra_blackbox.py` 及模板，使其完整处理：
  CPU packet 端口、CGRA 配置端口、DMA DRAM read request/response、DMA DRAM write
  request/response，以及 write request 中的 addr/data/mask 字段。DMA 端口只能是完整存在或
  明确报错，不能部分连接或 tie-off。

  生成的 Scala/C 元数据至少包含：DMA 是否存在、DRAM 地址/数据/mask 宽度、SPM 软件可见地址
  宽度、nbytes/tag 宽度、packet 字段 offset、六个 DMA command ID、`CMD_DMA_DONE` ID、
  DMA descriptor 各字段 offset/width。SPM 软件地址宽度取实际 packet `data_addr` 宽度，当前
  应为 7 bit，而不是照抄内部 `DmaCmdType` 的 32 bit。

  为 RoCC funct 建立单一权威协议表，并生成 Scala 和 C 定义。保留现有 funct 2～11，新增：
  12=`DMA_MVIN_ASYNC`、13=`DMA_MVOUT_ASYNC`、14=`DMA_WAIT`。不得在多处复制这些数字。

  生成器必须验证当前 DMA descriptor：
  `spm_addr + nbytes + tag` 的总 bit 数不超过 `xLen`，当前预期为 7+32+8=47 bit。
  若未来配置超过 64 bit，生成直接失败，不得拆包或使用旧六-packet 软件路径兜底。

  ## Hardware packet sequencer

  在 `CGRA.scala` 中增加独立的 DMA packet sequencer。高层 issue 指令语义为：

  - `rs1`：64-bit CPU 虚拟 DRAM byte address。
  - `rs2`：生成布局定义的 `{tag, nbytes, spm_word_addr}`。
  - funct 12/13：决定 MVIN/MVOUT。

  sequencer 在接受命令时原子锁存 `rs1`、`rs2` 和 `cmd.bits.status.dprv/dv`，然后逐拍向现有
  packet FIFO 发送六个生成模板。只有 `packetFifo.enq.fire` 才能进入下一 phase；backpressure
  期间 packet 必须稳定；六个 packet 之间不允许 raw packet 插入。模板和字段插入位置必须来自
  生成元数据，并用测试与 PyMTL `issue_dma_cmd()` 生成的 packet 做 bit-exact 对比。

  一次只允许一个 DMA in flight。sequencer 完成六包入队后应释放普通 RoCC 命令，使
  `load_relu4x4_config_fast()` 能在 MVIN 执行期间发送配置。`io.busy` 不能阻止普通 RoCC 命令，
  但必须包含 sequencer、DMA in-flight 和 memory adapter 活动状态，使 RISC-V fence 可以全局
  drain DMA。

  ## DCache memory adapter

  将 Integrated CGRA 的抽象 128-bit DRAM beat 接到 RoCC `io.mem`，所有宽度从生成参数
  推导。Phase 1 使用简单、严格、单 outstanding adapter，不实现 Gemmini 式多请求并发。

  按生成的 CGRA word 宽度逐 word 访问 DCache；当前是 32 bit，因此一个 128-bit read beat
  串行执行四次 32-bit `M_XRD`，地址依次 `addr+0,+4,+8,+12`，收到每个 `io.mem.resp` 后按
  little-endian 顺序组装，最后才向 CGRA 返回一个 128-bit response。

  MVOUT 将 16-bit byte mask 按 32-bit word lane 解释。每个 nibble 只能是 `0x0` 或 `0xf`：
  `0xf` 发一次 32-bit `M_XWR`，`0x0` 跳过；其他值明确 assert，不得取整。每个 store 都必须
  设置 `no_resp=false` 并等待对应 `io.mem.resp`；全部有效 word store 收到响应后，才向 CGRA
  产生 write response。绝不能在 `io.mem.req.fire` 时提前报告完成。

  所有请求使用 `phys=false` 和 issue 时锁存的 `dprv/dv`，完整初始化 HellaCache request 字段。
  依赖现有 `SimpleHellaCacheIF` 处理 nack/replay，不重复实现 replay。地址必须按 CGRA word
  对齐；非法地址、非法 descriptor、SPM 越界、非 word 倍数长度和 response tag 错误均明确
  assert，不能截断或调整。

  ## DMA completion 与 wait

  监控 CGRA `send_to_cpu_pkt` 中的 `CMD_DMA_DONE`，锁存完成 tag，并检查 packet 的 `opaque`
  和 data payload tag 一致。DMA issue 接受后设置 `dmaInFlight`，只有观察到真正的
  `CMD_DMA_DONE` 才清除；packet FIFO 为空或内存请求已发出均不代表完成。

  `DMA_WAIT` 使用 `rs1` 传入 expected tag，并返回 observed tag。完成可能早于 wait，因此需要
  一个不会丢失的 done latch。若 tag 不匹配，返回实际 tag 让 C 测试失败，不能等待“正确 tag”
  而死锁。消费 done latch 后才允许下一次 DMA。`doneValid` 本身不要让 `io.busy` 永久保持，
  否则先 fence 后读取 completion 会死锁。

  ## C API

  新增生成布局驱动的 DMA C API，例如 `tests/include/cgra_dma.h`：

  - `CGRA_DMA_DESC_CONST(spm_addr, nbytes, tag)`
  - `cgra_dma_mvin_async(const void *dram, cgra_dma_desc_t desc)`
  - `cgra_dma_mvout_async(void *dram, cgra_dma_desc_t desc)`
  - `cgra_dma_wait(uint8_t expected_tag)`，返回 observed tag
  - `cgra_dma_memory_fence()`

  descriptor 宏必须要求参数是编译期常量并进行范围、对齐、长度和 SPM 边界检查；不得用 mask
  静默截断。issue 每次只执行一条双源 RoCC 指令。inline asm 必须带 `"memory"` clobber；
  memory fence 使用 `fence rw, rw` 并带 `"memory"` clobber。不要照抄 Gemmini 缺少 compiler
  memory barrier 的宏。

  ## Demo 顺序

  新增 `tests/cgra-dma-relu4x4.c`。使用两个独立、至少 16-byte 对齐的 32-element `int32_t`
  静态数组。input[i]=i-16，output 先填 sentinel，DMA 长度为 `sizeof(input)`，SPM 起始 word
  地址为 0，MVIN/MVOUT 使用不同非零 tag。

  执行顺序必须严格为：

  1. CPU 初始化 input/output，执行 `cgra_dma_memory_fence()`。
  7. `CGRA_WAIT()`，检查 wait/status/result/completion count。
  8. `cgra_dma_mvout_async(output, MVOUT_DESC)`，不得在 CGRA 完成前启动。
  9. `cgra_dma_wait(MVOUT_TAG)`，检查 observed tag。
  10. `cgra_dma_memory_fence()`，CPU 普通读取 output 并逐元素验证 ReLU。

  demo 中不得调用 `configure_relu4x4_fast()`，因为配置和 launch 必须分开以实现上述 overlap。

  ## 验证与停止条件

  运行并通过现有 VectorCGRA DMA engine 和 Integrated wrapper pytest；增加生成器/packet
  模板的 bit-exact 测试。重新生成默认 DMA RTL、wrapper、Scala/C 元数据和 ReLU fast API，
  重复生成一次并确认结果确定且无额外 diff。

  必须使用重建后的 SoC simulator 运行：
  `./run-chipyard-cgra-test.sh --rebuild cgra-dma-relu4x4`
  随后运行现有回归：
  `./run-chipyard-cgra-test.sh cgra-relu4x4`
  `./run-chipyard-cgra-test.sh cgra-fir-yaml-4x4`

  不得使用旧 simulator 的 PASS 作为证据。所有命令必须等待结束；失败时定位根因并修复，不能
  绕过。完成后执行 `git diff --check`，检查工作树，做一次针对协议原子性、backpressure、
  memory ordering、completion 时机、tag 和 generated metadata 漂移的自审。

  最终只报告：关键设计、修改文件、生成出的实际宽度/descriptor 布局、完整测试命令与结果、
  自审发现和仍然明确存在的限制（例如单 outstanding）。然后停止，等待用户检查；不要开始
  CGRA+Gemmini Phase 2，也不要自行 commit、push 或修改远端。
