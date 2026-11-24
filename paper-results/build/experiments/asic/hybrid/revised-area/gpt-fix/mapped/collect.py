"""Audit the repaired full core with the original six-family area collector."""
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
BASE = ROOT.parents[1] / 'asic'
spec = importlib.util.spec_from_file_location('area', BASE/'collect.py')
area = importlib.util.module_from_spec(spec)
spec.loader.exec_module(area)
area.ROOT = ROOT
area.JOB = json.loads((ROOT/'job.json').read_text())
if area.fetch('gpt_vector'):
    row = area.collect('gpt_vector')
    row.update({'original_source_sha': area.JOB['original_source_sha'], 'new_source_sha': area.JOB['specialized_verilog_hashes']['gpt_vector'], 'fixed_from': area.JOB['fixed_from'], 'scope': 'Whole expanded family5 core with isolated native SQUARE/BIAS timing repair; original six-family report retained separately.', 'native_latency_change': 'SQUARE +2 cycles; BIAS +1 cycle for paths using those states.', 'performance_rerun': False, 'simulation_run': False})
    (ROOT/'results.json').write_text(json.dumps(row, indent=2)+'\n')
    print(json.dumps(row, indent=2))
else:
    print('Mapping has not completed.')
