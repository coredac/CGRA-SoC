# ORION 论文实验归档

本归档对应 ORION 论文终稿。[figures](figures/) 中的 5 张图和 1 张表均直接从终稿裁出，保留了原图中的文字、排版和数值。单独的制图文件用于追溯数据；引用论文原图时使用 figures 目录。

| 终稿位置 | 论文原图或表 | 对应数据与说明 |
| --- | --- | --- |
| 第 1 页，图 1 | [设计空间和 GPT-2 面积效率](figures/fig1.pdf) | [9 个配置及策略组合的数据](build/experiments/policy-check/gpt-three-way-v2/cases.csv) · [归一化核对](performance-source/README.md) |
| 第 2 页，表 I | [框架对比](figures/table1.pdf) | 文献对照表，引用见终稿参考文献 |
| 第 4 页，图 2 | [SoC 架构](figures/fig2.pdf) | 架构示意图 |
| 第 5 页，图 3 | [软件栈](figures/fig3.pdf) | 编译和调度示意图 |
| 第 6 页，图 4 | [性能、面积效率和 SoC 面积分解](figures/fig4.pdf) | [27 组绘图数值](mainres-1c.csv) · [原始周期记录](build/experiments/p1/summary27/results.json) · [面积分解](build/experiments/p1/summary27/figures/review-v20/area-relative.csv) · [综合报告与核验说明](performance-source/README.md) |
| 第 6 页，图 5 | [ASIC 对比和芯片版图](figures/fig5.pdf) | [ASIC 与 CGRA 的原始证据](ASIC.md) · [版图数据库、运行记录和几何数据](layout-source/README.md) |

历史实验文件在 [build/experiments](build/experiments/) 下沿用原相对路径，以便对照日志中的配置名称。CGRA/ASIC 的速度与面积柱来自不同版本。主结果 CSV 逐行区分了实测周期、沿用值与模型估计，两份核验说明保留了各自口径。

图 1(b) 有两行的归一化与图注不一致，重算值见 [性能核验说明](performance-source/README.md)。图 5(b) 的图注写作 post-route，实际底图与全芯片 global route 的输出一致。后续运行完成了 track assignment，留存的详细布线尝试覆盖 8 条选定网络。正文所写的 Innovus 与该次布局的 Fusion Compiler 日志也有出入。见[后端记录](layout-source/README.md)。

[SHA256SUMS](SHA256SUMS) 覆盖归档内每个文件（校验清单自身除外）。在本目录运行 `sha256sum --quiet -c SHA256SUMS` 可校验整份归档。本归档不需要启动性能仿真或后端流程。
