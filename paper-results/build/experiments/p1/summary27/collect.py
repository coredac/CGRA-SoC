"""Organize the completed primary P1 matrix without rerunning simulations."""

import csv
import hashlib
import json
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent
ROOT = OUT.parents[3]
BASE = OUT.parent / 'gpt2/manual-retry-20261109/summary'
WORKLOADS = ('gcn-star64', 'resnet-large', 'gpt-rows')
MODES = ('Manual', 'Hybrid', 'Auto')
TITLES = dict(zip(WORKLOADS, ('GCN', 'ResNet', 'GPT2')))
SHAPES = {
    'gcn-star64': 'star64, 190 edges, K64/N128',
    'resnet-large': '32x32x16 -> 16x16x32 projected block',
    'gpt-rows': 'N64/H128/heads2/FF512 block',
}
NOTES = [
    '范围为三个主 workload × 1/2/4 个 CGRA × Manual/Hybrid/Auto，共 27 组、54 条周期记录；不含 GPT wide、旧 ResNet v001 和新的 ASIC 对照实验。',
    '单位为仿真 CPU cycles，越小越好；不是仿真墙钟时间，也不证明物理时序收敛或真实频率。',
    '每组仅一条 cold 和一条 warm，没有重复样本方差或统计显著性结论。cold 包含配置与任务捕获，不表示硬件复位或 CPU cache 冷启动；warm 仍包含必要的数据搬运和完成处理。',
    '54 条大规模性能记录均 checked=0；PASS、零退出码和 status=0 只说明执行完成，不能据此声称大规模输出数值检查通过。小规模数值验证是另行保留的证据。',
    '同一 workload/CGRA 数量下三种策略使用相同硬件；不同 workload 和 CGRA 数量的容量配置不同，不能视作固定总资源的扩展性对照，也不能由这些周期推导面积或功耗优势。',
    'ResNet 1-CGRA 选择 rotated v002，4-CGRA 使用 v002-resume 完成记录；GPT 4-CGRA Manual 使用完成后的重试记录，Hybrid/Auto 使用此前已完成记录。来源与 SHA-256 保留在 results.json。',
    '数据来自原有合成 workload/block，不代表完整预训练网络、数据集精度或端到端应用性能。',
]


def require(ok, message):
    if not ok:
        sys.exit(message)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def csv_file(name, rows):
    with (OUT / name).open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def main():
    baseline = json.loads((BASE / 'baseline.json').read_text())
    pairs = [p for p in baseline['pairs'] if p['workload'] in WORKLOADS]
    expected = {(w, c, m) for w in WORKLOADS for c in (1, 2, 4) for m in MODES}
    require(len(pairs) == 27 and {(p['workload'], p['cgras'], p['mode']) for p in pairs} == expected, 'Incomplete matrix')
    pairs.sort(key=lambda p: (WORKLOADS.index(p['workload']), p['cgras'], MODES.index(p['mode'])))
    evidence = {e['path']: e for p in pairs for e in p['evidence']}
    for item in evidence.values():
        require(sha(BASE / item['path']) == item['sha256'], 'Evidence hash mismatch: ' + item['path'])
    rows, samples = [], []
    for pair in pairs:
        logs = [e for e in pair['evidence'] if e['path'].endswith('.log')]
        require(len(logs) == 1 and pair['exit'] == 0, pair['id'] + ': missing completion')
        log = (BASE / logs[0]['path']).read_text().splitlines()
        title = TITLES[pair['workload']]
        require(f'P1 {title}: PASS' in log and not any(f'P1 {title}: FAIL' in line for line in log), pair['id'] + ': invalid PASS')
        parsed = [dict(token.split('=', 1) for token in line.split()[2:]) for line in log if line.startswith(f'P1 {title} mode={pair["mode"]} ')]
        require(len(parsed) == 2 and [r['phase'] for r in pair['records']] == ['cold', 'warm'], pair['id'] + ': expected cold/warm')
        for record, logged in zip(pair['records'], parsed):
            require(all(str(value) == logged[key] for key, value in record.items()), pair['id'] + ': raw log mismatch')
            require(record['cycles'] > 0 and all(record[k] == 0 for k in ('checked', 'mismatches', 'status')), pair['id'] + ': invalid timing record')
            samples.append(dict(id=pair['id'], workload=pair['workload'], cgras=pair['cgras'], **record, exit=pair['exit'], source_log=logs[0]['origin']))
        cold, warm = [r['cycles'] for r in pair['records']]
        rows.append(dict(id=pair['id'], workload=pair['workload'], shape=SHAPES[pair['workload']], cgras=pair['cgras'], mode=pair['mode'], cold_cycles=cold, warm_cycles=warm, exit=0, checked=0, numerical_verified=False, source_log=logs[0]['origin'], archived_log=str((BASE / logs[0]['path']).relative_to(ROOT)), log_sha256=logs[0]['sha256']))
    # GPT records include an input ID; the other runners do not report that field.
    samples = [{**{k: r.get(k, '') for k in ('id', 'workload', 'cgras', 'mode', 'phase', 'repeat', 'input', 'cycles', 'checked', 'mismatches', 'status', 'exit', 'source_log')}} for r in samples]
    comparisons = []
    for workload in WORKLOADS:
        for count in (1, 2, 4):
            setting = {r['mode']: r for r in rows if r['workload'] == workload and r['cgras'] == count}
            comparison = dict(workload=workload, cgras=count)
            for phase in ('cold', 'warm'):
                for mode in MODES:
                    comparison[f'{phase}_{mode.lower()}'] = setting[mode][phase + '_cycles']
                auto = setting['Auto'][phase + '_cycles']
                for mode in ('Manual', 'Hybrid'):
                    base = setting[mode][phase + '_cycles']
                    comparison[f'{phase}_auto_speedup_vs_{mode.lower()}'] = base / auto
                    comparison[f'{phase}_auto_latency_reduction_vs_{mode.lower()}_pct'] = 100 * (1 - auto / base)
            comparisons.append(comparison)
    csv_file('pairs.csv', rows)
    csv_file('samples.csv', samples)
    csv_file('comparison.csv', comparisons)
    result = dict(scope='Primary P1 27 mode pairs', unit='CPU cycles', baseline=str((BASE / 'baseline.json').relative_to(ROOT)), baseline_sha256=sha(BASE / 'baseline.json'), verified_evidence_files=len(evidence), notes=NOTES, shapes=SHAPES, pairs=pairs, comparisons=comparisons, evidence_base=str(BASE.relative_to(ROOT)))
    (OUT / 'results.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    lines = ['# P1：27 组性能数据', '', '已核对 27 组、54 条记录及 52 个去重归档证据文件。每组保留 cold/warm，全部完成且退出码为 0。', '', '[27 组原始周期 CSV](pairs.csv) · [54 条长表 CSV](samples.csv) · [比较与加速比 CSV](comparison.csv) · [完整来源 JSON](results.json)', '', '## Workload', '']
    lines += [f'- {w}: {SHAPES[w]}。' for w in WORKLOADS]
    for phase in ('warm', 'cold'):
        lines += ['', f'## {phase.capitalize()} 周期', '', '| Workload | CGRAs | Manual | Hybrid | Auto | Auto 相对 Manual 加速 | Auto 相对 Hybrid 延迟下降 |', '| --- | ---: | ---: | ---: | ---: | ---: | ---: |']
        for row in comparisons:
            values = [f'{row[f"{phase}_{m.lower()}"]:,}' for m in MODES]
            lines.append(f'| {row["workload"]} | {row["cgras"]} | ' + ' | '.join(values) + f' | {row[f"{phase}_auto_speedup_vs_manual"]:.3f}× | {row[f"{phase}_auto_latency_reduction_vs_hybrid_pct"]:.3f}% |')
    lines += ['', '加速比 = 基线周期 / Auto 周期；延迟下降 = (基线周期 − Auto 周期) / 基线周期。负值代表 Auto 更慢。', '', '## 结论与边界', '', '9 个设置的 warm 样本均为 Auto 最快。GCN 的 cold 样本均为 Manual 最快；ResNet 两 CGRA 的 cold 也是 Manual 最快，因此不能概括为 Auto 在所有场景都最快。', '']
    lines += ['- ' + note for note in NOTES]
    lines += ['', '## 来源与复现', '', '运行 `python3 build/experiments/p1/summary27/collect.py` 可重新核对归档哈希、逐条对照日志并生成本目录结果；不启动仿真，也不修改原始证据。JSON 中 evidence.path 相对于 evidence_base；CSV 保留原始来源与归档日志路径。', '', '| 组 | 归档原始日志 |', '| --- | --- |']
    import os
    lines += [f'| {r["id"]} | [log]({os.path.relpath(ROOT / r["archived_log"], OUT)}) |' for r in rows]
    (OUT / 'README.md').write_text('\n'.join(lines) + '\n')
    print(f'Verified {len(rows)} pairs, {len(samples)} records, {len(evidence)} evidence files; wrote {OUT}')


if __name__ == '__main__':
    main()
