# Chipyard 中 CPU / Accelerator / Accelerator 之间的通信方式

这份笔记只讨论 **Chipyard 框架本身** 能提供哪些通信路径，以及这些路径分别用了什么协议、谁是主方/从方、协议里实际传什么内容。

目标不是一次讲完所有细节，而是给后续逐条学习提供一份稳定地图。

## 目录

1. 总览图
2. 先记住的几个概念
3. CPU 和 accelerator 之间有哪些通信方式
4. accelerator 和 accelerator 之间有哪些通信方式
5. 每种协议具体是什么
6. 在这份代码里该看哪里
7. 建议的学习顺序

---

## 1. 总览图

### 1.1 大图

```text
CPU(core)
  |
  +-- RoCC ------------------------------------------+
  |                                                  |
  |                                            accelerator
  |                                                  |
  |                                                  +-- io.mem -> L1 D$ -> TileLink -> memory/MMIO
  |                                                  |
  |                                                  +-- atlNode/tlNode -> TileLink fabric
  |                                                  |
  |                                                  +-- stlNode <- TileLink fabric
  |
  +-- MMIO read/write over TileLink/AXI4 ------------> accelerator(regs)
  |
  +-- ReRoCC(client side looks like RoCC) ----------> remote accelerator manager


accelerator A
  |
  +-- TileLink/AXI4 master ---> shared memory <--- TileLink/AXI4 master -- accelerator B
  |
  +-- TileLink/AXI4 master ---> accelerator B 的 slave/MMIO 寄存器
  |
  +-- ReRoCC packet protocol ---> ReRoCC manager ---> remote accelerator


Constellation NoC
  不是新的功能协议
  而是承载 TileLink / AXI4 的 transport layer
```

### 1.2 一句话总结

- **最紧耦合的控制通路** 是 `RoCC`
- **最常见的数据通路** 是 `TileLink`
- **AXI4` 主要用于外设、片外接口、或某些专门的总线接入**
- **MMIO** 本质上通常还是跑在 `TileLink` 或 `AXI4` 之上
- **ReRoCC** 是“远程 RoCC”
- **Constellation** 是把 `TileLink/AXI4` 包到 NoC 上跑，不是另起一套加速器功能协议

---

## 2. 先记住的几个概念

### 2.1 master/slave 和 client/manager

在 Chipyard / Rocket Chip 里：

- `AXI4` 常说 `master / slave`
- `TileLink` 常说 `client / manager`

大体上可以这样对应理解：

- `master/client`：主动发起请求的一侧
- `slave/manager`：拥有地址空间、接收并响应请求的一侧

但要注意：

- **一个模块完全可以同时拥有多个接口**
- 它可以在一个接口上是 `master/client`
- 在另一个接口上是 `slave/manager`

例如一个 accelerator 可以：

- 用 `RoCC` 接收 CPU 命令
- 用 `TileLink master` 去读 DRAM
- 再额外暴露一个 `TileLink slave/MMIO` 让别的模块配置它

### 2.2 控制面和数据面

可以把很多设计都拆成两部分：

- **控制面**：启动、暂停、查询状态、发参数
- **数据面**：真正搬数据、读写内存、发送结果

在 Chipyard 里非常常见的组合是：

- 控制面走 `RoCC` 或 `MMIO`
- 数据面走 `HellaCacheIO`、`TileLink`、`AXI4`

---

## 3. CPU 和 accelerator 之间有哪些通信方式

## 3.1 RoCC

### 3.1.1 它是什么

RoCC 是 Rocket tile 内部给 accelerator 预留的一套协处理器接口。

CPU 通过执行 `custom0~custom3` 指令，把命令送进 accelerator。

### 3.1.2 谁是主方

- CPU 是命令发起方
- accelerator 是命令接收方
- accelerator 可以回写结果给 CPU

### 3.1.3 协议内容

RoCC 的核心接口定义在：

- [LazyRoCC.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rocket-chip/src/main/scala/tile/LazyRoCC.scala:24)

关键结构：

- `RoCCInstruction`
  - `funct`
  - `rs1`
  - `rs2`
  - `rd`
  - `opcode`
  - `xd/xs1/xs2`
- `RoCCCommand`
  - `inst`
  - `rs1` 的值
  - `rs2` 的值
  - `status`
- `RoCCResponse`
  - `rd`
  - `data`

接口本体：

- `io.cmd`
- `io.resp`
- `io.mem`
- `io.busy`
- `io.interrupt`

对应源码：

- [LazyRoCC.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rocket-chip/src/main/scala/tile/LazyRoCC.scala:47)

### 3.1.4 它适合做什么

适合：

- 用指令直接启动 accelerator
- 小量参数传递
- 小量结果返回
- 与 CPU pipeline 紧耦合

不适合：

- 大带宽数据流本身直接走 RoCC 指令

那种情况下通常会让 accelerator 再去自己访存。

### 3.1.5 该看哪里

- 协议入口：
  - [LazyRoCC.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rocket-chip/src/main/scala/tile/LazyRoCC.scala:47)
- 官方说明：
  - [RoCC-Accelerators.rst](/mnt/public/qjj/CGRA-SoC/chipyard/docs/Customization/RoCC-Accelerators.rst:3)

---

## 3.2 RoCC + `io.mem` 通过 L1 D$ 访存

### 3.2.1 它是什么

RoCC accelerator 可以不自己实现 TileLink，而是通过 `io.mem` 复用挂靠 CPU 的 L1 DCache 接口。

这条路本质上是：

```text
CPU --RoCC--> accelerator --HellaCacheIO--> D$ --TileLink--> memory/MMIO
```

### 3.2.2 谁是主方

- accelerator 是 `HellaCacheIO` 请求发起方
- DCache/cache hierarchy 是响应方

### 3.2.3 协议内容

这不是 AXI 或 TL 的原生 endpoint，而是 Rocket 内部的 cache request interface。

定义在：

- [HellaCache.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rocket-chip/src/main/scala/rocket/HellaCache.scala:173)

请求字段来自 `HasCoreMemOp` 和 `HasCoreData`：

- `addr`
- `tag`
- `cmd`
- `size`
- `signed`
- `dprv`
- `dv`
- `data`
- `mask`
- `phys`
- `no_resp`
- `no_alloc`
- `no_xcpt`

定义在：

- [HellaCache.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rocket-chip/src/main/scala/rocket/HellaCache.scala:110)

响应字段：

- `replay`
- `has_data`
- `data_word_bypass`
- `data_raw`
- `store_data`

定义在：

- [HellaCache.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rocket-chip/src/main/scala/rocket/HellaCache.scala:135)

### 3.2.4 重要语义

- 返回可能乱序
- accelerator 需要用 `tag` 区分返回
- 最终底层仍会被 DCache 转成 TileLink 事务

文档说明：

- [RoCC-Accelerators.rst](/mnt/public/qjj/CGRA-SoC/chipyard/docs/Customization/RoCC-Accelerators.rst:56)

### 3.2.5 该看哪里

- 接口定义：
  - [HellaCache.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rocket-chip/src/main/scala/rocket/HellaCache.scala:173)
- 示例：
  - [AccumulatorExample in LazyRoCC.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rocket-chip/src/main/scala/tile/LazyRoCC.scala:118)

---

## 3.3 RoCC + TileLink master (`atlNode` / `tlNode`)

### 3.3.1 它是什么

RoCC accelerator 不一定只能经 `io.mem` 访存。

`LazyRoCC` 还给 accelerator 两个 TileLink client/master 口：

- `atlNode`
- `tlNode`

定义在：

- [LazyRoCC.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rocket-chip/src/main/scala/tile/LazyRoCC.scala:72)

这些口会被 tile 接到 SoC 的 TileLink 互连上：

- [LazyRoCC.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rocket-chip/src/main/scala/tile/LazyRoCC.scala:89)

### 3.3.2 谁是主方

- accelerator 是 `TileLink client/master`
- 内存、MMIO 外设、别的 TL slave 是 `manager/slave`

### 3.3.3 它和 `io.mem` 的区别

`io.mem`：

- 像在借 CPU 的 DCache/LSU
- 实现简单
- 带宽和行为受 cache 路径约束

`atlNode/tlNode`：

- accelerator 自己就是总线 client/master
- 更接近 DMA master
- 更灵活，通常吞吐更高

### 3.3.4 具体例子

`CharacterCountExample` 直接通过 `atlNode` 发 TileLink `Get`：

- [LazyRoCC.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rocket-chip/src/main/scala/tile/LazyRoCC.scala:237)

### 3.3.5 该看哪里

- 节点定义：
  - [LazyRoCC.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rocket-chip/src/main/scala/tile/LazyRoCC.scala:63)
- 节点接入：
  - [LazyRoCC.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rocket-chip/src/main/scala/tile/LazyRoCC.scala:84)
- 示例：
  - [CharacterCountExample](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rocket-chip/src/main/scala/tile/LazyRoCC.scala:237)

---

## 3.4 MMIO 外设式 accelerator

### 3.4.1 它是什么

这类 accelerator 不挂在 RoCC 指令口上，而是像普通设备一样，有一段可读写的寄存器地址空间。

CPU 通过 load/store 访问这些寄存器。

### 3.4.2 谁是主方

- CPU / 总线 master 发请求
- accelerator 作为 MMIO slave/manager 响应

### 3.4.3 它底层用什么协议

常见是：

- `TileLink` MMIO
- `AXI4` MMIO

文档入口：

- [MMIO-Peripherals.rst](/mnt/public/qjj/CGRA-SoC/chipyard/docs/Customization/MMIO-Peripherals.rst:3)

### 3.4.4 协议内容

这一层的“功能协议”通常不是复杂包格式，而是寄存器语义：

- 写参数寄存器
- 写启动寄存器
- 读状态寄存器
- 读结果寄存器
- 或者拉中断

`RegField` 还可以把 MMIO 读写映射成硬件 `ready/valid`：

- [MMIO-Peripherals.rst](/mnt/public/qjj/CGRA-SoC/chipyard/docs/Customization/MMIO-Peripherals.rst:30)

### 3.4.5 具体例子：GCD

TileLink 版本：

- `GCDTL` 用 `TLRegisterNode`
- [GCD.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/chipyard/src/main/scala/example/GCD.scala:128)

AXI4 版本：

- `GCDAXI4` 用 `AXI4RegisterNode`
- [GCD.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/chipyard/src/main/scala/example/GCD.scala:180)

寄存器映射：

- `0x00` 状态
- `0x04` 参数 `x`
- `0x08` 参数 `y` / 触发
- `0x0C` 结果

位置：

- [GCD.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/chipyard/src/main/scala/example/GCD.scala:165)

真正接到 SoC 总线：

- [CanHavePeripheryGCD](/mnt/public/qjj/CGRA-SoC/chipyard/generators/chipyard/src/main/scala/example/GCD.scala:287)

---

## 3.5 RoCC + `stlNode`

### 3.5.1 它是什么

`LazyRoCC` 不只给 master/client 口，也给了一个 slave/manager 口：

- `stlNode`

定义在：

- [LazyRoCC.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rocket-chip/src/main/scala/tile/LazyRoCC.scala:74)

它会接到：

- [LazyRoCC.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rocket-chip/src/main/scala/tile/LazyRoCC.scala:91)

### 3.5.2 意义

这意味着一个 RoCC accelerator 可以同时：

- 用 RoCC 接收 CPU 指令
- 用 TL master 主动去访存
- 再暴露一个 TL slave/MMIO 区域给别人访问

这是构造复杂 accelerator 常见的混合模式。

---

## 4. accelerator 和 accelerator 之间有哪些通信方式

## 4.1 共享内存

### 4.1.1 它是什么

这是最通用、也最常见的 accelerator-to-accelerator 通信方式。

```text
accel A --master/client--> memory <--master/client-- accel B
```

### 4.1.2 用什么协议

- `TileLink`
- `AXI4`
- 或 RoCC 的 `io.mem` 间接走到 TileLink

### 4.1.3 谁是主方

- 两边 accelerator 都可以是 master/client
- 内存系统是 slave/manager

### 4.1.4 协议内容

这里通常没有单独“加速器间消息协议”。

通信内容由软件/硬件自己定义，比如：

- ring buffer
- descriptor queue
- producer-consumer buffer
- doorbell word

### 4.1.5 什么时候适合

适合：

- 数据量大
- 生产者消费者模式
- 不要求非常强的点对点低时延控制

---

## 4.2 一方直接访问另一方的寄存器

### 4.2.1 它是什么

```text
accel A --TL/AXI master--> accel B 的 MMIO / slave endpoint
```

### 4.2.2 用什么协议

- `TileLink`
- `AXI4`

### 4.2.3 谁是主方

- accelerator A 是 master/client
- accelerator B 是 slave/manager

### 4.2.4 协议内容

仍然通常是“寄存器协议”：

- 写命令
- 写参数
- 读状态
- 读结果

本质上和 CPU 访问 MMIO 没区别，只是 master 不是 CPU 而是另一个 accelerator。

---

## 4.3 ReRoCC：远程 accelerator

### 4.3.1 它是什么

ReRoCC 是 Chipyard 里“把 accelerator 从 CPU 边上拆出去”的方案。

从 CPU 看，它还是像 RoCC；
但中间多了一层远程消息协议。

```text
CPU --RoCC--> ReRoCC client ==packet protocol== ReRoCC manager --RoCC--> remote accelerator
```

### 4.3.2 谁是主方

- CPU 发 RoCC 指令给 `client`
- `client` 是 ReRoCC request 发起方
- `manager` 是 ReRoCC response 返回方
- `manager` 再把指令重组后送给远端 accelerator

### 4.3.3 协议内容

文档定义：

- [rerocc/README.md](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rerocc/README.md:45)

消息字段：

- `opcode`
- `client_id`
- `manager_id`
- `data[63:0]`

request channel opcode：

- `mAcquire`
- `mInst`
- `mUStatus`
- `mUPtbr`
- `mRelease`
- `mUnbusy`

response channel opcode：

- `sAcqResp`
- `sInstAck`
- `sWrite`
- `sRelResp`
- `sUnbusyAck`

它是：

- 双通道
- in-order
- non-blocking
- packetized

### 4.3.4 代码里怎么实现

client 侧：

- 把 RoCC 指令拆成消息 beat
- [Client.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rerocc/src/main/scala/client/Client.scala:28)

manager 侧：

- 收消息
- 重组回 `RoCCCommand`
- 再喂给真实 accelerator
- [Manager.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rerocc/src/main/scala/manager/Manager.scala:97)

中间互连：

- [Xbar.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rerocc/src/main/scala/bus/Xbar.scala:32)

### 4.3.5 什么时候适合

适合：

- many-accelerator
- accelerator virtualization
- accelerator 与 CPU 物理上不放一起

---

## 4.4 Constellation NoC

### 4.4.1 它是什么

Constellation 是 Chipyard 里的 NoC 生成器。

但要强调：

> Constellation 不是新的 accelerator 功能协议；
> 它是承载上层协议的传输层。

文档：

- [Constellation.rst](/mnt/public/qjj/CGRA-SoC/chipyard/docs/Generators/Constellation.rst:1)

### 4.4.2 它承载什么

它可以承载：

- `TileLink`
- `AXI4`

文档：

- [Protocols/index.rst](/mnt/public/qjj/CGRA-SoC/chipyard/generators/constellation/docs/source/Protocols/index.rst:4)

### 4.4.3 自己内部是什么样的传输协议

Constellation 的 transport 特征：

- packet-switched
- wormhole-routed
- virtual networks
- credit-based flow control

文档：

- [Constellation.rst](/mnt/public/qjj/CGRA-SoC/chipyard/docs/Generators/Constellation.rst:6)

### 4.4.4 谁是主方

Constellation 本身不重新定义 master/slave 语义。

真正的功能角色仍然是：

- 上层 `TileLink client / manager`
- 或 `AXI4 master / slave`

NoC 只负责把这些 endpoint 之间的多通道协议安全搬运过去。

### 4.4.5 在 SoC 里怎么接

它可以替换 SoC 里的交叉开关，比如：

- `SystemBus`
- `MemoryBus`
- `PeripheryBus`

实现入口：

- [Buses.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/constellation/src/main/scala/soc/Buses.scala:18)

---

## 5. 每种协议到底是什么

## 5.1 RoCC 协议

### 5.1.1 方向

- `cmd`：CPU -> accelerator
- `resp`：accelerator -> CPU
- `busy`：accelerator -> CPU
- `interrupt`：accelerator -> CPU

### 5.1.2 内容

- `cmd` 带 instruction 和寄存器值
- `resp` 带 `rd` 和 `data`

### 5.1.3 它不是总线协议

RoCC 不负责大规模系统互连；
它更像 CPU 内部“协处理器命令接口”。

---

## 5.2 HellaCacheIO 协议

### 5.2.1 方向

- accelerator -> D$：请求
- D$ -> accelerator：响应

### 5.2.2 内容

请求：

- 地址 `addr`
- 标签 `tag`
- 读写/原子操作种类 `cmd`
- 访问大小 `size`
- 数据 `data`
- 写掩码 `mask`

响应：

- 数据
- replay
- 是否带数据

### 5.2.3 它不是 TileLink endpoint

它比 TileLink 更靠近 core/cache 内部。
真正对外仍然会被缓存层翻译成 TileLink。

---

## 5.3 TileLink 协议

### 5.3.1 角色

- `client` 发起事务
- `manager` 响应事务

### 5.3.2 通道

普通不带一致性时常只看到：

- `A`
- `D`

带一致性 `TL-C` 时还有：

- `B`
- `C`
- `E`

定义：

- [tilelink/Bundles.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rocket-chip/src/main/scala/tilelink/Bundles.scala:173)

### 5.3.3 每个通道干什么

`A` channel: client -> manager  
请求通道。

字段：

- `opcode`
- `param`
- `size`
- `source`
- `address`
- `mask`
- `data`
- `corrupt`

`D` channel: manager -> client  
响应通道。

字段：

- `opcode`
- `param`
- `size`
- `source`
- `sink`
- `denied`
- `data`
- `corrupt`

`B` channel: manager -> client  
probe/coherence 请求。

`C` channel: client -> manager  
probe ack / release。

`E` channel: client -> manager  
grant ack。

### 5.3.4 常见消息类型

`A` 上常见：

- `Get`
- `PutFullData`
- `PutPartialData`
- `ArithmeticData`
- `LogicalData`
- `Hint`
- `AcquireBlock`
- `AcquirePerm`

`D` 上常见：

- `AccessAck`
- `AccessAckData`
- `Grant`
- `GrantData`
- `ReleaseAck`

消息定义：

- [tilelink/Bundles.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rocket-chip/src/main/scala/tilelink/Bundles.scala:18)

### 5.3.5 TileLink 适合做什么

适合：

- 片上 SoC 主互连
- MMIO
- DMA master
- 一致性访存

---

## 5.4 AXI4 协议

### 5.4.1 角色

- `master` 发起读写
- `slave` 响应读写

### 5.4.2 五通道

定义：

- [axi4/Bundles.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rocket-chip/src/main/scala/amba/axi4/Bundles.scala:94)

`AW`: master -> slave  
写地址通道。

字段：

- `id`
- `addr`
- `len`
- `size`
- `burst`
- `lock`
- `cache`
- `prot`
- `qos`

`W`: master -> slave  
写数据通道。

字段：

- `data`
- `strb`
- `last`

`B`: slave -> master  
写响应通道。

字段：

- `id`
- `resp`

`AR`: master -> slave  
读地址通道。

字段和 `AW` 类似。

`R`: slave -> master  
读数据通道。

字段：

- `id`
- `data`
- `resp`
- `last`

### 5.4.3 AXI4 适合做什么

适合：

- 外部内存控制器
- 片外接口
- 某些 IP block 的标准接入
- 可作为 MMIO 总线

---

## 5.5 ReRoCC 协议

### 5.5.1 角色

- `client`：靠近 CPU
- `manager`：靠近远端 accelerator

### 5.5.2 通道

- request channel
- response channel

### 5.5.3 消息

统一字段：

- `opcode`
- `client_id`
- `manager_id`
- `data`

请求里最重要的是：

- `mInst`

它可以跨多个 beat 发送：

- 指令本体
- `rs1`
- `rs2`

这本质上就是把 RoCC 命令序列化后，通过 interconnect 发给远端 manager。

---

## 5.6 Constellation transport

### 5.6.1 它不是 TL/AXI 的替代品

它不改变 TL/AXI endpoint 的协议含义。

### 5.6.2 它提供什么

- flit 化传输
- wormhole routing
- virtual networks
- credit-based flow control

### 5.6.3 你该怎么理解它

最好的理解方式是：

```text
TileLink / AXI4 = 你在“说什么”
Constellation   = 你在“怎么运过去”
```

---

## 6. 在这份代码里该看哪里

## 6.1 如果你现在只想先学 CPU -> accelerator

按顺序看：

1. [RoCC-Accelerators.rst](/mnt/public/qjj/CGRA-SoC/chipyard/docs/Customization/RoCC-Accelerators.rst:3)
2. [LazyRoCC.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rocket-chip/src/main/scala/tile/LazyRoCC.scala:24)
3. [AccumulatorExample](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rocket-chip/src/main/scala/tile/LazyRoCC.scala:118)
4. [CharacterCountExample](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rocket-chip/src/main/scala/tile/LazyRoCC.scala:237)

## 6.2 如果你想学 MMIO accelerator

按顺序看：

1. [MMIO-Peripherals.rst](/mnt/public/qjj/CGRA-SoC/chipyard/docs/Customization/MMIO-Peripherals.rst:3)
2. [GCD.scala: GCDTL/GCDAXI4](/mnt/public/qjj/CGRA-SoC/chipyard/generators/chipyard/src/main/scala/example/GCD.scala:127)
3. [GCD.scala: CanHavePeripheryGCD](/mnt/public/qjj/CGRA-SoC/chipyard/generators/chipyard/src/main/scala/example/GCD.scala:287)

## 6.3 如果你想学数据面协议

先看：

1. [HellaCache.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rocket-chip/src/main/scala/rocket/HellaCache.scala:110)
2. [tilelink/Bundles.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rocket-chip/src/main/scala/tilelink/Bundles.scala:18)
3. [axi4/Bundles.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rocket-chip/src/main/scala/amba/axi4/Bundles.scala:11)

## 6.4 如果你想学 accelerator -> accelerator / many-accelerator

先看：

1. [rerocc/README.md](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rerocc/README.md:1)
2. [Client.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rerocc/src/main/scala/client/Client.scala:67)
3. [Manager.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rerocc/src/main/scala/manager/Manager.scala:34)
4. [Xbar.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/rerocc/src/main/scala/bus/Xbar.scala:12)
5. [Constellation.rst](/mnt/public/qjj/CGRA-SoC/chipyard/docs/Generators/Constellation.rst:1)
6. [Buses.scala](/mnt/public/qjj/CGRA-SoC/chipyard/generators/constellation/src/main/scala/soc/Buses.scala:18)

---

## 7. 建议的学习顺序

推荐按下面顺序学，不要一开始就冲 NoC：

1. **RoCC 基本接口**
   - 搞清楚 CPU 是怎么发命令给 accelerator 的
2. **HellaCacheIO**
   - 搞清楚 RoCC accelerator 怎么通过本核 cache 访存
3. **TileLink**
   - 搞清楚 Chipyard 片上标准互连是什么
4. **MMIO accelerator**
   - 搞清楚不用 RoCC 时，设备是怎么接到总线上的
5. **AXI4**
   - 搞清楚片外/外设场景下的主流协议
6. **ReRoCC**
   - 搞清楚远程 accelerator 的抽象
7. **Constellation**
   - 搞清楚 many-accelerator 时传输层怎么替换成 NoC

---

## 附：一句话对照表

| 场景 | 常用方式 | 功能协议 | 传输角色 |
|---|---|---|---|
| CPU 启动本地 accelerator | RoCC | RoCC | CPU 发命令，acc 返回结果 |
| RoCC accelerator 访存 | `io.mem` | HellaCacheIO | acc 发 cache req，D$ 响应 |
| accelerator 主动 DMA / 高带宽访存 | `atlNode/tlNode` | TileLink | acc 是 client/master |
| CPU 像访问设备一样控制 accelerator | MMIO | TileLink 或 AXI4 | CPU 是 master/client |
| accelerator A 和 B 共享 buffer | shared memory | TileLink / AXI4 | A/B 都可做 master |
| accelerator A 配置 accelerator B | slave/MMIO access | TileLink / AXI4 | A 是 master，B 是 slave |
| 远程 accelerator | ReRoCC | ReRoCC packet | client <-> manager |
| many-accelerator 互连 | Constellation | 上层仍是 TL/AXI | NoC 负责 transport |

