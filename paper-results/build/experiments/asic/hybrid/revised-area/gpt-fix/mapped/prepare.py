"""Stage the isolated timing repair with the frozen whole-core mapping flow."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parent
BASE = ROOT.parents[1] / 'asic'
SOURCE = ROOT.parent / 'source'
HOST = 'jjqin25@hpc6.cse.cuhk.edu.hk'
folder = ROOT / 'gpt_vector'
folder.mkdir(exist_ok=True)
shutil.copyfile(SOURCE/'all.v', folder/'all.v')
shutil.copyfile(BASE/'gpt_vector/synth.tcl', folder/'synth.tcl')
shutil.copyfile(BASE/'run.sh', ROOT/'run.sh')
source_hash = hashlib.sha256((folder/'all.v').read_bytes()).hexdigest()
constraint_hash = hashlib.sha256((folder/'synth.tcl').read_bytes()).hexdigest()
remote = subprocess.check_output(['ssh', HOST, 'mktemp -d /tmp/cgra-revised-gpt-fix-XXXXXXXX'], text=True).strip()
subprocess.run(['ssh', HOST, f'mkdir -p {remote}/gpt_vector'], check=True)
subprocess.run(['scp', str(folder/'all.v'), str(folder/'synth.tcl'), f'{HOST}:{remote}/gpt_vector/'], check=True)
subprocess.run(['scp', str(ROOT/'run.sh'), f'{HOST}:{remote}/run.sh'], check=True)
original = json.loads((BASE/'job.json').read_text())
job = {'host': HOST, 'remote': remote, 'cores_per_job': 1, 'clock_ns': 2, 'started': False, 'source_directory': str(SOURCE), 'original_source_sha': original['specialized_verilog_hashes']['gpt_vector'], 'specialized_verilog_hashes': {'gpt_vector': source_hash}, 'synth_tcl_hashes': {'gpt_vector': constraint_hash}, 'fixed_from': str(BASE/'gpt_vector'), 'constraint_flow_identical_to_original': constraint_hash == original['synth_tcl_hashes']['gpt_vector']}
(ROOT/'job.json').write_text(json.dumps(job, indent=2)+'\n')
print(json.dumps(job, indent=2))
