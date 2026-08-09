你正在 /home/jjqin/CGRA-SoC 中工作。请完成 CGRA DMA 从 Rocket io.mem/DCache adapter 迁移到独立 128-bit
  TileLink master 的重构，并将 VectorCGRA 的修改缩减到已经确认的最小范围。

  第一条强制要求：在修改任何源代码之前，先把本 prompt 完整、原样保存到：

  docs/plans/cgra-dma-tilelink-refactor-prompt.md

  如果后续发生上下文压缩、恢复或任务续跑，必须先重新完整阅读该文档，再检查当前 diff 和任务状态，确保实现没有漂
  移。

  全过程禁止：

  - fallback 到非 DMA CGRA。
  - fallback 到 io.mem/DCache。
  - CPU 代替 DMA 搬运数据。
  - SPM peek/readback 或调试旁路。
  - 通过后处理文本硬改生成 Verilog。
  - hardcode 协议 command ID、packet field offset 或 descriptor field。
  - 逐 bit 在运行时构造六个 DMA packet。
  - 根据地址数值猜测虚拟地址或物理地址。
  - 遇到 TileLink 错误后伪造 DMA completion。
  - 为了通过测试而降低传输宽度或把一个 128-bit beat 拆成多个 32/64-bit事务。
  - 修改、清理或回滚与本任务无关的 dirty/untracked 文件。
  - commit、push 或修改远端。

  使用 apply_patch 进行人工代码修改。先检查三个仓库的 status/diff，识别已有用户修改。本任务已明确允许回滚下文
  列出的 VectorCGRA 本轮修改，但不要影响其他改动。

  ## 一、最终架构

  目标数据通路：

  CPU RoCC command
    -> 原子六包 DMA command sequencer
    -> IntegratedCgraWithDmaRTL
    -> VectorCGRA 原生 128-bit DRAM val/rdy interface
    -> Chipyard CGRATileLinkDmaAdapter
    -> dedicated LazyRoCC tlNode
    -> 128-bit TileLink/system bus

  不再使用 Rocket HellaCache io.mem 搬运 DMA 数据。LazyRoCC 固有的 io.mem 端口保留，但必须明确 tie-off，不能存
  在任何 DMA 请求路径。

  Phase 1 是明确的 bare-metal physical-address-only 设计：

  - C pointer 数值直接作为物理地址。
  - 不实例化 TLB。
  - 不申请 PTW port。
  - 不检查或猜测 satp。
  - 不提供虚拟地址 fallback。
  - API/文档明确说明仅支持裸机物理地址。
  - 虚拟地址支持属于后续独立 Phase。

  ## 二、VectorCGRA 最小修改

  VectorCGRA 只允许保留以下两个 tracked 文件的修改：

  1. VectorCGRA/mem/dma/DmaEngineRTL.py
     - 保留当前显式 beat_word0/1/2/3 和 beat_with_spm_resp 修改。
     - 该修改用于避免当前 PyMTL fork 生成 lane 2/3 slice 时常量宽度溢出。
     - 不改变 DMA 协议、状态机语义、外部端口或 128-bit beat 与四个 32-bit SPM word 的既有架构。

  2. VectorCGRA/mem/dma/test/DmaEngineRTL_test.py
     - 保留生成 Verilog 后检查精确 lane slice 的 regression test。
     - 测试必须验证 31:0、63:32、95:64、127:96。

  回滚本轮对以下 VectorCGRA 文件的修改：

  - VectorCGRA/cgra/IntegratedCgraWithDmaRTL.py
  - VectorCGRA/cgra/test/CgraTemplateRTL_single_test.py
  - VectorCGRA/cgra/test/IntegratedCgraWithDmaRTL_test.py
  - VectorCGRA/mem/data/test/DataMemControllerRTL_dma_test.py

  删除本轮生成在 VectorCGRA 工作区中的这些 untracked artifact，但不要删除任何无法确认来源的其他文件：

  - CgraTemplateRTL_single__pickled.v
  - IntegratedCgraWithDmaRTL_single__pickled.v
  - MeshMultiCgraTemplateRTL_multi__pickled.v

  完成后，VectorCGRA 应只剩上述两个预期 tracked 文件发生变化，且不再承载 SoC 生成入口、SoC packet helper 或生
  成 RTL artifact。

  ## 三、根仓库生成流程重构

  当前 scripts/generate_single_cgra.py 依赖修改后的：

  VectorCGRA/cgra/test/CgraTemplateRTL_single_test.py

  必须解除该依赖。

  将 IntegratedCgraWithDmaRTL 的 YAML 构建和 PyMTL translation 入口移动到根仓库 scripts 下。可以迁移当前已验证
  的构建逻辑，但不得继续要求修改 VectorCGRA test 文件。

  要求：

  - 默认单 CGRA 仍生成 IntegratedCgraWithDmaRTL_single。
  - DMA top 或完整 DMA ports 缺失时立即失败。
  - 不 fallback 到 CgraTemplateRTL。
  - num_fu_outports 当前使用上游 IntegratedCgraWithDmaRTL 的默认值 2；配置不匹配时明确失败，不再修改
  VectorCGRA 构造函数。
  - 保持生成结果确定性，继续清理仅出现在注释中的 Python object address。
  - 中间 RTL 输出到根仓库拥有的生成目录或临时目录，不能写入 VectorCGRA。
  - 最终 RTL 和 Verilog wrapper 同步到 Chipyard resources/vsrc。
  - tests/test_cgra_dma_generation.py 不再 import 新增的 VectorCGRA test helper。
  - 六包 packet 的参考构造逻辑放在根仓库测试或根仓库协议 helper 中。
  - command ID 继续从 VectorCGRA/lib/cmd_type.py AST 提取。
  - RoCC funct、Scala/C 常量和 packet layout 继续来自现有 YAML/生成元数据。
  - 不复制 magic number 形成第二协议源。

  ## 四、128-bit TileLink DMA Adapter

  在 Chipyard CGRA 集成侧实现独立、职责清晰的 CGRATileLinkDmaAdapter。可以放在 CGRA.scala 或相邻专用 Scala 文
  件中，遵循现有 Chipyard/Gemmini风格。

  连接方式参考 Gemmini：

  - 使用 dedicated LazyRoCC tlNode。
  - 使用 TLClientNode。
  - sourceId 只需要 IdRange(0, 1)。
  - 不使用 atlNode。
  - 不使用 io.mem。
  - single outstanding。
  - 不实例化 TLB/PTW。
  - 正确连接或 tie-off TileLink B/C/E 等未使用 channel。

  CGRARocketConfig 增加 WithSystemBusWidth(128)，旁边加简短注释：

  “CGRA DMA currently supports only a 128-bit system-bus beat.”

  CGRA+Gemmini 配置继续保持 128-bit system bus。

  不要写大量宽度判断。只保留一个清晰的 elaboration-time require/assert，确认 negotiated TileLink data width 等
  于 VectorCGRA DMA data width，也就是 128 bit。配置不匹配立即失败。

  Adapter 事务语义：

  - 一个 VectorCGRA 128-bit read request严格对应一个 16-byte TileLink Get。
  - 一个 VectorCGRA 128-bit write request严格对应一个 16-byte TileLink PutFull 或 PutPartial。
  - TileLink lgSize 固定表示 16 bytes。
  - VectorCGRA 的 16-bit byte mask直接映射为 TileLink mask。
  - 全 1 mask 使用 PutFull；其他合法 mask 使用 PutPartial。
  - 不能拆成四次 32-bit或两次 64-bit请求。
  - 不能通过 TLWidthWidget 静默掩盖窄 system bus。
  - read response 必须来自真实 AccessAckData。
  - write completion 必须来自真实 AccessAck。
  - 检查 source、opcode、denied、corrupt 和事务结束条件。
  - denied/corrupt/opcode mismatch 立即 assertion failure，不返回成功。
  - 接收 VectorCGRA request 时锁存 address/data/mask。
  - TileLink A channel backpressure 期间 bits 必须稳定。
  - TileLink D response 到达后，如果 VectorCGRA 暂时不 ready，response 必须稳定保留。
  - 同时出现 DMA read/write request 应 assertion failure。
  - 地址超出 TileLink physical address width时 assertion failure，不能静默截断。

  删除 CGRA.scala 中当前 strict single-outstanding DCache adapter：

  - 删除四次 32-bit lane scan/read/write状态机。
  - 删除 M_XRD/M_XWR 依赖。
  - 删除 dmaDprv/dmaDv，因为 Phase 1 不翻译虚拟地址。
  - 恢复 io.mem 为明确 unused/tie-off。
  - 保留六包 DMA sequencer、dmaInFlight、done latch、tag 检查和 DMA_WAIT completion-early语义。

  ## 五、地址和长度契约

  Phase 1 强制：

  - DRAM address 16-byte aligned。
  - nbytes 非零。
  - nbytes 是 16 的整数倍。
  - SPM 范围合法。
  - address + nbytes 不溢出。
  - 一次只允许一个 DMA command in flight。

  同时更新硬件检查、C API 和测试。不要保留旧的“nbytes 只需是 4 的整数倍”公共 API 语义。

  C API 应继续优先使用编译期可确定的 descriptor：

  - descriptor 使用 compile-time macro/constant construction。
  - 编译期可知的 nbytes 必须静态检查为 16-byte multiple。
  - tag 必须精确为 8 bit。
  - buffer 在 demo 中使用 aligned(16)。
  - 不逐 bit 构造 packet。
  - 不增加 runtime fallback。
  - 普通 pointer 明确解释为 bare-metal physical address。

  ## 六、六包 sequencer 与完成语义

  保留当前每个 DMA command 的固定六包：

  1. DRAM address low
  2. DRAM address high
  3. SPM address
  4. nbytes
  5. tag
  6. MVIN 或 MVOUT command

  要求：

  - 六包由 RoCC/Chipyard sequencer 自动生成。
  - 软件只发送语义 DMA command。
  - 六包必须原子连续，不允许普通 CGRA packet 插入中间。
  - command 接收时一次性锁存 rs1、descriptor 和所需状态。
  - packet FIFO backpressure 下当前 packet 保持稳定。
  - 真正的 CMD_DMA_DONE 才能清除 dmaInFlight。
  - completion 可以早于 DMA_WAIT。
  - DMA_WAIT 返回实际 tag，并检查 expected tag。
  - 不伪造完成，不通过 memory response 直接代替 CMD_DMA_DONE。

  ## 七、C Demo 顺序

  更新 tests/cgra-dma-relu4x4.c，保持完整路径：

  普通内存 input
    -> DMA MVIN
    -> CGRA 配置和执行 ReLU
    -> DMA MVOUT
    -> CPU 普通 load 验证 output

  顺序要求：

  1. CPU 初始化 aligned(16) input/output。
  2. 执行 memory fence。
  3. 发出 DMA MVIN async。
  4. 在 MVIN DMA_WAIT 之前执行 CGRA_SET_EXPECTED_COMPLETES，使其与 DMA 搬运重叠。
  5. 配置 CGRA可以在 DMA 进行时发送，但必须维持六包 DMA command 原子性。
  6. 等待 MVIN 真实完成并验证 tag。
  7. 只有 MVIN 完成后才能 launch CGRA。
  8. 等待并验证 CGRA completion/result。
  9. 发出 DMA MVOUT async。
  10. 等待 MVOUT 真实完成并验证 tag。
  11. 执行 memory fence。
  12. CPU 通过普通 load 读取 output 并逐项验证 ReLU。

  禁止：

  - CPU 写 SPM。
  - CPU 从 SPM 读取结果。
  - debug peek。
  - cache/debug bypass。
  - DMA 失败后 CPU 搬运。
  - 在 MVIN 完成前 launch。

  如果 dedicated TileLink 路径出现 CPU cache一致性问题，应修正 TileLink 接入和协议，不得添加软件复制、缓存旁路
  或隐藏 fallback。

  ## 八、测试与验收

  先运行快速单元测试，再完整重建 SoC。不要因为已有 simulator 而跳过 rebuild。

  至少完成：

  1. VectorCGRA DMA engine functional tests。
  2. VectorCGRA Verilog lane translation regression。
  3. 根仓库 DMA generation/protocol tests。
  4. 生成两次 RTL并比较 hash，确认确定性。
  5. Chipyard CGRARocketConfig 完整 rebuild。
  6. ./run-chipyard-cgra-test.sh --rebuild cgra-dma-relu4x4
  7. ./run-chipyard-cgra-test.sh cgra-relu4x4
  8. ./run-chipyard-cgra-test.sh cgra-fir-yaml-4x4
  9. 根仓库、VectorCGRA、Chipyard 分别执行 git diff --check。

  还必须通过代码或生成结果证明：

  - negotiated TileLink beat 是 128 bit。
  - 每个 VectorCGRA DMA beat 只产生一个 TileLink A transaction。
  - 不存在 32/64-bit lane拆分状态机。
  - io.mem 不产生请求。
  - DMA read 使用 Get。
  - full write 使用 PutFull。
  - mask write 使用 PutPartial。
  - D response 之前不产生 DMA memory completion。
  - VectorCGRA 只保留两个允许修改的 tracked 文件。
  - VectorCGRA 中没有本轮生成 RTL artifact。
  - 旧的非 DMA ReLU/FIR 测试没有回归。

  ## 九、过程要求

  开始时先给出简短执行计划，然后持续更新进度。每完成一个阶段就检查 diff，避免最后才发现职责放错仓库。

  不要停在设计或部分实现。完成代码、生成、完整 rebuild、测试和最终自审后再汇报。

  如果遇到不确定问题：

  - 先阅读 Gemmini、LazyRoCC 和现有 VectorCGRA DMA 接口代码。
  - 优先遵循 Gemmini dedicated tlNode 的既有模式。
  - 不得自行加入 fallback、magic number 或协议旁路。
  - 只有涉及无法从代码确定的外部语义决策时才询问用户。

  最终报告必须包括：

  - 三个仓库分别修改了哪些文件。
  - VectorCGRA 最终保留的两个修改及原因。
  - io.mem adapter 删除情况。
  - TileLink node、beat、Get/Put语义。
  - 地址/长度限制。
  - C demo 的最终执行顺序。
  - 所有实际运行的测试命令和结果。
  - 尚存限制。
  - 明确说明没有 commit、push 或修改远端。
