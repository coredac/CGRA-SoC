"""Run one isolated GPT backend/mode on the archived validated SoC."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import subprocess
import time

ROOT = Path(__file__).resolve().parents[5]
OUT = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("mode", type=int, choices=(0, 1))
parser.add_argument("backend", type=int, choices=(0, 1))
args = parser.parse_args()
name = f'{("operator", "chain")[args.mode]}-{("cgra", "asic")[args.backend]}'
folder = OUT / "runs" / name
folder.mkdir(parents=True, exist_ok=True)
elf = OUT / "gpt.riscv"
simulator = ROOT / "build/experiments/asic/system/simulator-validated"
command = [
    str(simulator),
    "+permissive",
    "+dramsim",
    f"+dramsim_ini_dir={ROOT}/chipyard/generators/testchipip/src/main/resources/dramsim2_ini",
    "+max-cycles=10000000",
    f"+loadmem={elf}",
    "+permissive-off",
    str(elf),
    str(args.mode),
    str(args.backend),
]
env = dict(os.environ)
env["LD_LIBRARY_PATH"] = str(ROOT / "chipyard/.conda-env/lib")
soft, hard = resource.getrlimit(resource.RLIMIT_STACK)
resource.setrlimit(resource.RLIMIT_STACK, (32 * 1024 * 1024, hard))
record = {
    "name": name,
    "mode": args.mode,
    "backend": args.backend,
    "status": "running",
    "command": command,
    "cwd": str(folder),
    "stack_bytes": 32 * 1024 * 1024,
    "environment": {"LD_LIBRARY_PATH": env["LD_LIBRARY_PATH"]},
    "sha256": {
        str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in (elf, simulator)
    },
    "start_epoch": time.time(),
}
(folder / "run.json").write_text(json.dumps(record, indent=2) + "\n")
print(f'START {name} log={folder / "run.log"}', flush=True)
with (folder / "run.log").open("w") as log:
    process = subprocess.Popen(
        command, stdout=log, stderr=subprocess.STDOUT, cwd=folder, env=env
    )
    record["pid"] = process.pid
    (folder / "run.json").write_text(json.dumps(record, indent=2) + "\n")
    exit_code = process.wait()
record.update(
    exit=exit_code,
    elapsed_seconds=time.time() - record["start_epoch"],
    status="complete",
)
(folder / "run.json").write_text(json.dumps(record, indent=2) + "\n")
print(
    f'FINISH {name} exit={exit_code} seconds={record["elapsed_seconds"]:.3f}',
    flush=True,
)
raise SystemExit(exit_code)
