"""Measure the two matched Hybrid on-chip ResNet backends."""

from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import statistics
import subprocess
import time

OUT = Path(__file__).resolve().parent
ROOT = OUT.parents[4]
SIM = ROOT / "chipyard/sims/verilator/simulator-chipyard.harness-MultiAccelRocketConfig"
BINARY = OUT / "medium.riscv"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(backend):
    directory = OUT / backend
    directory.mkdir(exist_ok=True)
    command = [
        str(SIM),
        "+permissive",
        "+dramsim",
        f"+dramsim_ini_dir={ROOT}/chipyard/generators/testchipip/src/main/resources/dramsim2_ini",
        "+max-cycles=10000000",
        f"+loadmem={BINARY}",
        "+permissive-off",
        str(BINARY),
        "0" if backend == "cgra" else "1",
    ]
    (directory / "command.json").write_text(json.dumps(command, indent=2) + "\n")
    started = time.monotonic()
    print("START", backend, flush=True)
    log_path = directory / "run.log"
    with log_path.open("w") as log:
        process = subprocess.run(
            command,
            cwd=directory,
            env=dict(os.environ, LD_LIBRARY_PATH=str(ROOT / "chipyard/.conda-env/lib")),
            stdout=log,
            stderr=subprocess.STDOUT,
        )
    text = log_path.read_text()
    records = []
    for line in text.splitlines():
        if line.startswith("SPLIT resnet backend="):
            fields = dict(re.findall(r"(\w+)=([^\s]+)", line))
            records.append({k: int(v) if v.isdigit() else v for k, v in fields.items()})
    passed = process.returncode == 0 and "SPLIT: PASS" in text and len(records) == 4
    passed = passed and all(
        r["checked"] == 1024
        and r["errors"] == r["counter_errors"] == r["status"] == 0
        and r["submissions"] == 2
        for r in records
    )
    result = dict(
        backend=backend,
        passed=passed,
        exit=process.returncode,
        elapsed_seconds=time.monotonic() - started,
        records=records,
        log=str(log_path.relative_to(ROOT)),
        log_sha256=digest(log_path),
    )
    if passed:
        result["warm_mean"] = statistics.mean(
            r["cycles"] for r in records if r["phase"] == "warm"
        )
    (directory / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print("DONE", backend, "PASS" if passed else "FAIL", flush=True)
    return result


def main():
    resource.setrlimit(
        resource.RLIMIT_STACK,
        (64 * 1024 * 1024, resource.getrlimit(resource.RLIMIT_STACK)[1]),
    )
    dependencies = (
        (OUT / "medium.d").read_text().replace("\\\n", " ").split(":", 1)[1].split()
    )
    paths = {Path(p) if p.startswith("/") else ROOT / p for p in dependencies}
    paths.update([SIM, BINARY, OUT / "run.py", OUT / "compile.sh"])
    manifest = {str(p): digest(p) for p in sorted(paths)}
    (OUT / "inputs.json").write_text(json.dumps(manifest, indent=2) + "\n")
    results = dict(
        scope="1024-output projected ResNet; independent Add/ReLU ASIC; both regional Hybrid with two cached region submissions; on-chip intermediates",
        sampling="One cold plus three warm executions, alternating input 0,1,0,1; numerical checks outside timed interval; no CPU cache flush",
        runs=[],
    )
    with ThreadPoolExecutor(max_workers=2) as pool:
        for future in as_completed(
            [pool.submit(run, name) for name in ("cgra", "asic")]
        ):
            results["runs"].append(future.result())
            (OUT / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    results["passed"] = all(r["passed"] for r in results["runs"])
    if results["passed"]:
        means = {r["backend"]: r["warm_mean"] for r in results["runs"]}
        results["cgra_warm_cycle_reduction"] = 1 - means["cgra"] / means["asic"]
    (OUT / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    return 0 if results["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
