"""Map the modified complete CGRA compute boundary using the ASIC constraints."""

import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent
HOST = "jjqin25@hpc6.cse.cuhk.edu.hk"
source = ROOT / "source/CgraCompute.v"
remote = subprocess.check_output(
    ["ssh", HOST, "mktemp -d /tmp/cgra-revised-core-XXXXXXXX"], text=True
).strip()
subprocess.run(
    ["scp", str(source), str(ROOT / "synth.tcl"), f"{HOST}:{remote}/"],
    check=True,
)
runner = """#!/bin/bash
cd "$1" || exit 1
/tmp/cgra-area-20261030/dc.sh -f synth.tcl >dc.log 2>&1
status=$?
echo "$status" >exit
exit "$status"
"""
(ROOT / "run.sh").write_text(runner)
subprocess.run(["scp", str(ROOT / "run.sh"), f"{HOST}:{remote}/"], check=True)
subprocess.run(
    ["ssh", HOST, f"nohup bash {remote}/run.sh {remote} >{remote}/run.log 2>&1 </dev/null &"],
    check=True,
)
job = {
    "host": HOST,
    "remote": remote,
    "top": "CgraCompute",
    "cores": 2,
    "clock_ns": 2,
    "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
    "constraints_sha256": hashlib.sha256((ROOT / "synth.tcl").read_bytes()).hexdigest(),
    "boundary": json.loads((ROOT / "source/boundary.json").read_text())["boundary"],
}
(ROOT / "job.json").write_text(json.dumps(job, indent=2) + "\n")
print(remote)
