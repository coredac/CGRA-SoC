"""Map the additional SAD endpoint using the frozen six-family flow."""

import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent
HOST = "jjqin25@hpc6.cse.cuhk.edu.hk"


def main():
    job_path = ROOT / "job.json"
    if job_path.exists():
        print("Existing job recorded; collect it before launching another revision.")
        return
    folder = ROOT / "sad"
    source = folder / "source/all.v"
    template = (ROOT.parent / "revised-area/asic/add/synth.tcl").read_text()
    script = template.replace("ExpandedAsic", "ExpandedSad")
    (folder / "synth.tcl").write_text(script)
    remote = subprocess.check_output(
        ["ssh", HOST, "mktemp -d /tmp/cgra-sad-area-XXXXXXXX"], text=True
    ).strip()
    subprocess.run(["scp", str(source), str(folder / "synth.tcl"), f"{HOST}:{remote}/"], check=True)
    runner = '''#!/bin/bash
cd "$1" || exit 1
sha256sum all.v synth.tcl > inputs.sha256
date -u +%FT%TZ > started
/tmp/cgra-area-20261030/dc.sh -f synth.tcl > dc.log 2>&1
status=$?
echo "$status" > exit
date -u +%FT%TZ > finished
'''
    (ROOT / "run.sh").write_text(runner)
    subprocess.run(["scp", str(ROOT / "run.sh"), f"{HOST}:{remote}/run.sh"], check=True)
    job = {
        "host": HOST,
        "remote": remote,
        "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "constraints_sha256": hashlib.sha256(script.encode()).hexdigest(),
        "base_constraints_sha256": hashlib.sha256(template.encode()).hexdigest(),
        "flow_difference": "Only top-level design name changes from ExpandedAsic to ExpandedSad.",
    }
    job_path.write_text(json.dumps(job, indent=2) + "\n")
    subprocess.run(["ssh", HOST, f"nohup bash {remote}/run.sh {remote} >{remote}/runner.log 2>&1 </dev/null &"], check=True)
    print(json.dumps(job, indent=2))


if __name__ == "__main__":
    main()
