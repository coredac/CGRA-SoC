# P1：27 组性能数据

已核对 27 组、54 条记录及 52 个去重归档证据文件。每组保留 cold/warm，全部完成且退出码为 0。

[27 组原始周期 CSV](pairs.csv) · [54 条长表 CSV](samples.csv) · [比较与加速比 CSV](comparison.csv) · [完整来源 JSON](results.json)

## Workload

- gcn-star64: star64, 190 edges, K64/N128。
- resnet-large: 32x32x16 -> 16x16x32 projected block。
- gpt-rows: N64/H128/heads2/FF512 block。

## Warm 周期

| Workload | CGRAs | Manual | Hybrid | Auto | Auto 相对 Manual 加速 | Auto 相对 Hybrid 延迟下降 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| gcn-star64 | 1 | 608,860 | 402,709 | 402,353 | 1.513× | 0.088% |
| gcn-star64 | 2 | 526,122 | 313,568 | 286,515 | 1.836× | 8.627% |
| gcn-star64 | 4 | 520,148 | 311,370 | 230,602 | 2.256× | 25.940% |
| resnet-large | 1 | 622,431 | 309,863 | 265,643 | 2.343× | 14.271% |
| resnet-large | 2 | 501,672 | 193,915 | 173,087 | 2.898× | 10.741% |
| resnet-large | 4 | 487,617 | 134,590 | 125,708 | 3.879× | 6.599% |
| gpt-rows | 1 | 18,270,931 | 16,341,700 | 16,262,298 | 1.124× | 0.486% |
| gpt-rows | 2 | 12,578,157 | 8,466,036 | 8,372,251 | 1.502× | 1.108% |
| gpt-rows | 4 | 12,689,077 | 4,561,151 | 4,434,498 | 2.861× | 2.777% |

## Cold 周期

| Workload | CGRAs | Manual | Hybrid | Auto | Auto 相对 Manual 加速 | Auto 相对 Hybrid 延迟下降 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| gcn-star64 | 1 | 619,742 | 1,467,526 | 949,319 | 0.653× | 35.312% |
| gcn-star64 | 2 | 538,551 | 1,382,468 | 836,382 | 0.644× | 39.501% |
| gcn-star64 | 4 | 536,021 | 1,390,099 | 786,322 | 0.682× | 43.434% |
| resnet-large | 1 | 642,243 | 660,798 | 620,618 | 1.035× | 6.081% |
| resnet-large | 2 | 524,812 | 547,802 | 529,699 | 0.991× | 3.305% |
| resnet-large | 4 | 515,635 | 507,371 | 502,677 | 1.026× | 0.925% |
| gpt-rows | 1 | 18,448,100 | 16,675,497 | 16,598,945 | 1.111× | 0.459% |
| gpt-rows | 2 | 12,887,567 | 9,042,143 | 8,951,640 | 1.440× | 1.001% |
| gpt-rows | 4 | 13,188,682 | 5,534,000 | 5,414,871 | 2.436× | 2.153% |

加速比 = 基线周期 / Auto 周期；延迟下降 = (基线周期 − Auto 周期) / 基线周期。负值代表 Auto 更慢。

## 结论与边界

9 个设置的 warm 样本均为 Auto 最快。GCN 的 cold 样本均为 Manual 最快；ResNet 两 CGRA 的 cold 也是 Manual 最快，因此不能概括为 Auto 在所有场景都最快。

- 范围为三个主 workload × 1/2/4 个 CGRA × Manual/Hybrid/Auto，共 27 组、54 条周期记录；不含 GPT wide、旧 ResNet v001 和新的 ASIC 对照实验。
- 单位为仿真 CPU cycles，越小越好；不是仿真墙钟时间，也不证明物理时序收敛或真实频率。
- 每组仅一条 cold 和一条 warm，没有重复样本方差或统计显著性结论。cold 包含配置与任务捕获，不表示硬件复位或 CPU cache 冷启动；warm 仍包含必要的数据搬运和完成处理。
- 54 条大规模性能记录均 checked=0；PASS、零退出码和 status=0 只说明执行完成，不能据此声称大规模输出数值检查通过。小规模数值验证是另行保留的证据。
- 同一 workload/CGRA 数量下三种策略使用相同硬件；不同 workload 和 CGRA 数量的容量配置不同，不能视作固定总资源的扩展性对照，也不能由这些周期推导面积或功耗优势。
- ResNet 1-CGRA 选择 rotated v002，4-CGRA 使用 v002-resume 完成记录；GPT 4-CGRA Manual 使用完成后的重试记录，Hybrid/Auto 使用此前已完成记录。来源与 SHA-256 保留在 results.json。
- 数据来自原有合成 workload/block，不代表完整预训练网络、数据集精度或端到端应用性能。

## 来源与复现

运行 `python3 build/experiments/p1/summary27/collect.py` 可重新核对归档哈希、逐条对照日志并生成本目录结果；不启动仿真，也不修改原始证据。JSON 中 evidence.path 相对于 evidence_base；CSV 保留原始来源与归档日志路径。

| 组 | 归档原始日志 |
| --- | --- |
| gcn-star64-cgra1-Manual | [log](../gpt2/manual-retry-20261109/summary/provenance/local/gcn/cgra1/v002/large-results.log) |
| gcn-star64-cgra1-Hybrid | [log](../gpt2/manual-retry-20261109/summary/provenance/local/gcn/cgra1/v002/large-results.log) |
| gcn-star64-cgra1-Auto | [log](../gpt2/manual-retry-20261109/summary/provenance/local/gcn/cgra1/v002/large-results.log) |
| gcn-star64-cgra2-Manual | [log](../gpt2/manual-retry-20261109/summary/provenance/local/gcn/cgra2/v001/large-results.log) |
| gcn-star64-cgra2-Hybrid | [log](../gpt2/manual-retry-20261109/summary/provenance/local/gcn/cgra2/v001/large-results.log) |
| gcn-star64-cgra2-Auto | [log](../gpt2/manual-retry-20261109/summary/provenance/local/gcn/cgra2/v001/large-results.log) |
| gcn-star64-cgra4-Manual | [log](../gpt2/manual-retry-20261109/summary/provenance/local/gcn/cgra4/v001/large-results.log) |
| gcn-star64-cgra4-Hybrid | [log](../gpt2/manual-retry-20261109/summary/provenance/local/gcn/cgra4/v001/large-results.log) |
| gcn-star64-cgra4-Auto | [log](../gpt2/manual-retry-20261109/summary/provenance/local/gcn/cgra4/v001/large-results.log) |
| resnet-large-cgra1-Manual | [log](../gpt2/manual-retry-20261109/summary/provenance/local/resnet/cgra1/v002/large-results.log) |
| resnet-large-cgra1-Hybrid | [log](../gpt2/manual-retry-20261109/summary/provenance/local/resnet/cgra1/v002/large-results.log) |
| resnet-large-cgra1-Auto | [log](../gpt2/manual-retry-20261109/summary/provenance/local/resnet/cgra1/v002/large-results.log) |
| resnet-large-cgra2-Manual | [log](../gpt2/manual-retry-20261109/summary/provenance/local/resnet/cgra2/v001/large-results.log) |
| resnet-large-cgra2-Hybrid | [log](../gpt2/manual-retry-20261109/summary/provenance/local/resnet/cgra2/v001/large-results.log) |
| resnet-large-cgra2-Auto | [log](../gpt2/manual-retry-20261109/summary/provenance/local/resnet/cgra2/v001/large-results.log) |
| resnet-large-cgra4-Manual | [log](../gpt2/manual-retry-20261109/summary/provenance/local/resnet/cgra4/v002-resume/results-20261109/jobs/timing-Manual/run.log) |
| resnet-large-cgra4-Hybrid | [log](../gpt2/manual-retry-20261109/summary/provenance/local/resnet/cgra4/v002-resume/results-20261109/jobs/timing-Hybrid/run.log) |
| resnet-large-cgra4-Auto | [log](../gpt2/manual-retry-20261109/summary/provenance/local/resnet/cgra4/v002-resume/results-20261109/jobs/timing-Auto/run.log) |
| gpt-rows-cgra1-Manual | [log](../gpt2/manual-retry-20261109/summary/provenance/local/gpt2/cgra1/v002/large/rows-manual.log) |
| gpt-rows-cgra1-Hybrid | [log](../gpt2/manual-retry-20261109/summary/provenance/local/gpt2/cgra1/v002/large/rows-hybrid.log) |
| gpt-rows-cgra1-Auto | [log](../gpt2/manual-retry-20261109/summary/provenance/local/gpt2/cgra1/v002/large/rows-auto.log) |
| gpt-rows-cgra2-Manual | [log](../gpt2/manual-retry-20261109/summary/provenance/local/gpt2/cgra2/v001/large/rows-manual.log) |
| gpt-rows-cgra2-Hybrid | [log](../gpt2/manual-retry-20261109/summary/provenance/local/gpt2/cgra2/v001/large/rows-hybrid.log) |
| gpt-rows-cgra2-Auto | [log](../gpt2/manual-retry-20261109/summary/provenance/local/gpt2/cgra2/v001/large/rows-auto.log) |
| gpt-rows-cgra4-Manual | [log](../gpt2/manual-retry-20261109/summary/provenance/local/gpt2/resume-20261107/results-20261109/jobs/cgra4-rows-manual/run.log) |
| gpt-rows-cgra4-Hybrid | [log](../gpt2/manual-retry-20261109/summary/provenance/local/gpt2/cgra4/v001/large/rows-hybrid.log) |
| gpt-rows-cgra4-Auto | [log](../gpt2/manual-retry-20261109/summary/provenance/local/gpt2/cgra4/v001/large/rows-auto.log) |
