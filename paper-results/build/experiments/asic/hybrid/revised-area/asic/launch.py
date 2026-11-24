"""Map each frozen expanded ASIC core with the common two-nanosecond flow."""
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent
SOURCE = ROOT.parents[1] / 'capability-trial'
HOST = 'jjqin25@hpc6.cse.cuhk.edu.hk'
NAMES = ['add', 'relu', 'gcn', 'norm', 'softmax', 'gpt_vector']
text = (SOURCE / 'all.v').read_text()
template = (SOURCE / 'synth.tcl').read_text()
template = template.replace('max_cores 2', 'max_cores 1').replace('bank.v', 'all.v').replace('PrimitiveBank', 'ExpandedAsic')
start = template.index('  set leaves [get_cells')
end = template.index('  set f [open SUCCESS', start)
template = template[:start] + '''  redirect -file check.rpt {check_design}
  redirect -file timing_check.rpt {check_timing -include {unconstrained_endpoints no_input_delay partial_input_delay}}
  redirect -file design.rpt {report_design}
  set leaves [get_cells -hierarchical -filter {is_hierarchical == false}]
  foreach ref [lsort -unique [get_attribute $leaves ref_name]] {
    if {[string match GTECH* $ref] || [sizeof_collection [get_lib_cells -quiet */$ref]] == 0} {error "unmapped $ref"}
  }
  set names [get_attribute $leaves full_name]
  set refs [get_attribute $leaves ref_name]
  set areas [get_attribute $leaves area]
  set seqs [get_attribute $leaves is_sequential]
  set f [open leaves.tsv w]
  puts $f "path\\tref\\tarea\\tsequential"
  foreach name $names ref $refs area $areas seq $seqs {puts $f "$name\\t$ref\\t$area\\t$seq"}
  close $f
''' + template[end:]
source_hashes = {str(p.relative_to(SOURCE)): hashlib.sha256(p.read_bytes()).hexdigest() for p in [SOURCE/'all.v', *sorted((SOURCE/'rtl').glob('*.sv')), *sorted((SOURCE/'native').glob('*.sv'))]}
hashes = {}
for family, name in enumerate(NAMES):
    folder = ROOT / name
    folder.mkdir(exist_ok=True)
    specialized = text.replace('parameter integer FAMILY = 0;', f'parameter integer FAMILY = {family};', 1)
    (folder / 'all.v').write_text(specialized)
    (folder / 'synth.tcl').write_text(template)
    hashes[name] = hashlib.sha256(specialized.encode()).hexdigest()
remote = subprocess.check_output(['ssh', HOST, 'mktemp -d /tmp/cgra-revised-asic-XXXXXXXX'], text=True).strip()
for name in NAMES:
    subprocess.run(['ssh', HOST, f'mkdir -p {remote}/{name}'], check=True)
    subprocess.run(['scp', str(ROOT/name/'all.v'), str(ROOT/name/'synth.tcl'), f'{HOST}:{remote}/{name}/'], check=True)
runner = '''#!/bin/bash
root="$1"
shift
for family in "$@"; do
  cd "$root/$family" || exit 1
  date -u +%FT%TZ >started
  /tmp/cgra-area-20261030/dc.sh -f synth.tcl >dc.log 2>&1
  status=$?
  echo "$status" >exit
  date -u +%FT%TZ >finished
done
'''
(ROOT/'run.sh').write_text(runner)
subprocess.run(['scp', str(ROOT/'run.sh'), f'{HOST}:{remote}/run.sh'], check=True)
queues = [['add','gcn','norm'], ['relu','gpt_vector','softmax']]
for index, queue in enumerate(queues):
    subprocess.run(['ssh', HOST, f'nohup bash {remote}/run.sh {remote} {" ".join(queue)} >{remote}/queue{index}.log 2>&1 </dev/null &'], check=True)
job = {'host': HOST, 'remote': remote, 'queues': queues, 'cores_per_job': 1, 'max_concurrent_jobs': 2, 'clock_ns': 2, 'source_hashes': source_hashes, 'synth_tcl_hashes': {name: hashlib.sha256((ROOT/name/'synth.tcl').read_bytes()).hexdigest() for name in NAMES}, 'specialized_verilog_hashes': hashes, 'specialization': 'Only ExpandedAsic FAMILY parameter default changed; all implementation text unchanged.'}
(ROOT/'job.json').write_text(json.dumps(job, indent=2)+'\n')
print(json.dumps(job, indent=2))
