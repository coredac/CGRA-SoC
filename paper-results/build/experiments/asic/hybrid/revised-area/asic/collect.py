"""Collect mapped full-core reports and subtract only named storage cells."""
from collections import Counter, defaultdict
import csv
from decimal import Decimal
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parent
JOB = json.loads((ROOT / 'job.json').read_text())
NAMES = ['add', 'relu', 'gcn', 'norm', 'softmax', 'gpt_vector']
FILES = ['area.rpt', 'units.rpt', 'setup.rpt', 'constraints.rpt', 'qor.rpt', 'check.rpt', 'timing_check.rpt', 'design.rpt', 'leaves.tsv', 'SUCCESS', 'exit', 'started', 'finished', 'dc.log']
PREFIX = {'gcn': 'gcn.kernel/', 'norm': 'gpt.norm.kernel/', 'softmax': 'gpt.softmax.kernel/', 'gpt_vector': 'gpt.vector_unit.kernel/'}
ARRAY = {
    'gcn': re.compile(r'(?:rowptr|columns)_reg\[|(?:lanes|weight_banks)\[\d+\]\.memory/words_reg\['),
    'norm': re.compile(r'(?:source|gain|offset|result)_reg\['),
    'softmax': re.compile(r'(?:source|result|exponent)_reg\['),
    'gpt_vector': re.compile(r'(?:source|other|result)_store/banks\[\d+\]\.memory/words_reg\['),
}

def fetch(name):
    folder = ROOT / name
    remote = f"{JOB['remote']}/{name}"
    check = subprocess.run(['ssh', JOB['host'], f'test -f {remote}/exit'], capture_output=True)
    if check.returncode:
        return False
    subprocess.run(['scp', *[f"{JOB['host']}:{remote}/{file}" for file in FILES], str(folder)], check=True, stdout=subprocess.DEVNULL)
    hashes = subprocess.check_output(['ssh', JOB['host'], f'cd {remote} && sha256sum all.v synth.tcl'], text=True)
    (folder/'mapped-inputs.sha256').write_text(hashes)
    return True


def collect(name):
    folder = ROOT / name
    rows = list(csv.DictReader((folder / 'leaves.tsv').open(), delimiter='\t'))
    groups = defaultdict(Decimal)
    counts = Counter()
    references = Counter()
    matched = []
    array_counts = Counter()
    bank_access_area = Decimal(0)
    for row in rows:
        path = row['path']
        native = path.startswith(PREFIX.get(name, '\0'))
        local = path.removeprefix(PREFIX[name]) if native else ''
        storage = native and row['sequential'] == 'true' and ARRAY[name].match(local)
        group = 'top_arbitration_and_control'
        if native:
            group = 'native_datapath_control_and_access'
        if path.startswith('primitives/'):
            group = 'primitive_bank'
        if storage:
            group = 'local_array_storage'
            matched.append(row)
            references[row['ref']] += 1
            array_counts[path.split('_reg[')[0]] += 1
        bank_module = (name == 'gcn' and re.match(r'(?:lanes|weight_banks)\[\d+\]\.memory/', local)) or (name == 'gpt_vector' and re.match(r'(?:source|other|result)_store/', local))
        if bank_module and not storage:
            bank_access_area += Decimal(row['area'])
        groups[group] += Decimal(row['area'])
        counts[group] += 1
    with (folder / 'storage.tsv').open('w') as stream:
        writer = csv.DictWriter(stream, fieldnames=['path', 'ref', 'area', 'sequential'], delimiter='\t')
        writer.writeheader()
        writer.writerows(matched)
    qor = (folder / 'qor.rpt').read_text()
    setup = (folder / 'setup.rpt').read_text()
    total = Decimal(re.search(r'^\s*Cell Area:\s*([\d.]+)', qor, re.M)[1])
    leaf = sum(groups.values())
    storage_area = groups['local_array_storage']
    evidence_hashes = dict(line.split()[::-1] for line in (folder/'mapped-inputs.sha256').read_text().splitlines())
    source_hash = hashlib.sha256((folder/'all.v').read_bytes()).hexdigest()
    constraint_hash = hashlib.sha256((folder/'synth.tcl').read_bytes()).hexdigest()
    expected_arrays = {'add': 0, 'relu': 0, 'gcn': 20, 'norm': 4, 'softmax': 3, 'gpt_vector': 24}[name]
    tolerance = max(Decimal('0.001'), total * Decimal('0.0000001'))
    audit = {
        'source_hash_matches': source_hash == JOB['specialized_verilog_hashes'][name] == evidence_hashes['all.v'],
        'timing_check_has_no_warnings': not re.search(r'Warning:|Error:', (folder/'timing_check.rpt').read_text()),
        'constraint_hash_matches': constraint_hash == JOB['synth_tcl_hashes'][name] == evidence_hashes['synth.tcl'],
        'leaf_area_reconciles': abs(total - leaf) <= tolerance,
        'leaf_area_tolerance': float(tolerance),
        'storage_arrays_match_expected_count': len(array_counts) == expected_arrays,
        'expected_storage_arrays': expected_arrays,
        'matched_storage_arrays': len(array_counts),
    }
    audit['passed'] = all(audit[key] for key in ['source_hash_matches', 'constraint_hash_matches', 'leaf_area_reconciles', 'storage_arrays_match_expected_count', 'timing_check_has_no_warnings'])
    paths = [{'startpoint': start, 'endpoint': end, 'status': status, 'slack_ns': float(slack)} for start, end, status, slack in re.findall(r'Startpoint:\s*(.*?)\n.*?Endpoint:\s*(.*?)\n.*?slack \((MET|VIOLATED)\)\s+(-?[\d.]+)', setup, re.S)]
    violations = int(float(re.search(r'No. of Violating Paths:\s*([\d.]+)', qor)[1]))
    return {
        'complete': (folder/'SUCCESS').is_file() and (folder/'exit').read_text().strip() == '0' and audit['passed'],
        'evidence_audit': audit,
        'storage_array_cell_counts': dict(array_counts),
        'total_cell_area': float(total),
        'local_array_storage_area': float(storage_area),
        'area_excluding_local_array_storage': float(total - storage_area),
        'leaf_area_sum': float(leaf),
        'report_minus_leaf_area': float(total - leaf),
        'group_area': {key: float(value) for key, value in groups.items()},
        'group_cell_counts': dict(counts),
        'storage_cell_references': dict(references),
        'bank_module_access_area_included': float(bank_access_area),
        'bank_module_access_hierarchy_available': name in ['gcn', 'gpt_vector'],
        'sequential_cell_count': sum(row['sequential'] == 'true' for row in rows),
        'sequential_cell_area': float(sum(Decimal(row['area']) for row in rows if row['sequential'] == 'true')),
        'design_warning_codes': dict(Counter(re.findall(r'^Warning:.*\((LINT-\d+)\)', (folder/'check.rpt').read_text(), re.M))),
        'storage_rule': {'prefix': PREFIX.get(name), 'relative_path_pattern': ARRAY[name].pattern if name in ARRAY else None, 'sequential': True},
        'worst_setup_slack_ns': min(map(float, re.findall(r'slack \((?:MET|VIOLATED)\)\s+(-?[\d.]+)', setup))),
        'total_negative_setup_slack_ns_report_precision': float(re.search(r'Total Negative Slack:\s*(-?[\d.]+)', qor)[1]),
        'violating_setup_paths': int(float(re.search(r'No. of Violating Paths:\s*([\d.]+)', qor)[1])),
        'no_violated_constraints': 'no violated constraints' in (folder/'constraints.rpt').read_text(),
        'reported_setup_paths': paths,
        'listed_setup_paths_cover_all_violations': sum(p['slack_ns'] < 0 for p in paths) == violations,
        'total_negative_slack_ns_from_listed_paths': sum(min(p['slack_ns'], 0) for p in paths),
        'worst_startpoint': re.search(r'Startpoint:\s*(.*)', setup)[1],
        'worst_endpoint': re.search(r'Endpoint:\s*(.*)', setup)[1],
        'started_utc': (folder/'started').read_text().strip(),
        'finished_utc': (folder/'finished').read_text().strip(),
        'source_sha256': hashlib.sha256((folder/'all.v').read_bytes()).hexdigest(),
    }


def main():
    results = {}
    for name in NAMES:
        folder = ROOT/name
        ready = (folder/'SUCCESS').exists() and (folder/'exit').exists() and (folder/'mapped-inputs.sha256').exists()
        if ready or fetch(name):
            results[name] = collect(name)
    totals = {key: sum(result[key] for result in results.values()) for key in ['total_cell_area', 'local_array_storage_area', 'area_excluding_local_array_storage']}
    result = {'scope': 'Measured whole expanded ASIC cores, one standalone core per family. Native arrays remain in synthesis; only named sequential storage cells are subtracted afterward.', 'area_unit': 'SAED14 library area units', 'clock_ns': 2, 'complete_family_count': sum(v['complete'] and v['evidence_audit']['passed'] for v in results.values()), 'families': results, 'sum_completed_families': totals, 'limits': ['Six independent 16-lane ASIC endpoints install 96 primitive lanes; the CGRA has 16 PEs. Native throughput alignment is not established.', 'Includes compute, native control, primitive bank, command arbitration, local array access and pipeline/runtime state.', 'Excludes DMA, external SPM, endpoint adapters and SoC interconnect.', 'Storage subtraction retains array read/write selection and access logic.', 'Ideal-clock synthesis with wire-load estimates is not physical timing signoff.', 'No simulation or performance rerun; historical performance results do not validate modified schedules.']}
    (ROOT/'results.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))

if __name__ == '__main__':
    main()
