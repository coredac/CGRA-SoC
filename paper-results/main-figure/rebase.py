from pathlib import Path
from decimal import Decimal
import csv
import re

src = Path("build/experiments/p1/summary27/figures/review-v20/metrics.csv")
rows = list(csv.DictReader(src.open()))
workloads = ["ResNet","GPT-2","GCN"]
policies = ["per-task","per-segment","per-graph"]
counts = [1,2,4]
lookup = {(r["workload"],int(r["cgras"]),r["policy"]):r for r in rows}
def metrics(w,n,p):
    r = lookup[w,n,p]
    base = lookup[w,1,"per-task"]
    speed = Decimal(base["cycles_high"]) / Decimal(r["cycles_high"])
    ae = speed * Decimal(base["area_library_units"]) / Decimal(r["area_library_units"])
    return speed,ae

source = Path("mainres.tex").read_text()
out = source.replace(source.splitlines()[0], f"% Source: {src}\n% At equal clock, speedup = C0/C and AE = (C0*A0)/(C*A), using each workload's 1(1) reference, cycles_high, and area_library_units.")
out = out.replace("ymax=4.9","ymax=6.0").replace("ytick={0,1,2,3,4}","ytick={0,1,2,3,4,5}").replace("Normalized Speedup","Speedup vs. 1(1)").replace("Normalized AE","AE vs. 1(1)")
plots = list(re.finditer(r"coordinates \{([^}]+)\}",out))
if len(plots) != 18:
    raise SystemExit("Unexpected plot format")
for i,match in reversed(list(enumerate(plots))):
    n = counts[(i%9)//3]
    policy = policies[i%3]
    idx = i//9
    new = []
    for j,w in enumerate(workloads,1):
        r = lookup[w,n,policy]
        old = Decimal(r[("speedup_low","ae_low")[idx]])
        if f"({j},{old:.9f})" not in match[1]:
            raise SystemExit("Old figure disagrees with metrics")
        val = metrics(w,n,policy)[idx]
        new.append(f"({j},{val:.9f})")
    out = out[:match.start(1)] + " ".join(new) + out[match.end(1):]
def update_label(m):
    j,n,idx = map(int,m.group(2,3,4))
    vals = [max(metrics(workloads[j-1],n,p)[k] for p in policies) for k in (0,1)]
    height = vals[idx] if idx else max(vals)
    node = re.sub(r"(axis cs:\d,)[\d.]+(\)\{)[\d.]+(\};)",lambda h: h[1]+f"{height:.9f}"+h[2]+f"{vals[idx]:.2f}"+h[3],m[1])
    line = re.sub(r"(axis cs:\d,)[\d.]+",lambda h: h[1]+f"{vals[idx]:.9f}",m[5])
    return node + "\n" + line
out,num = re.subn(r"(\\node[^\n]+?\(value-(\d)-(\d)-(\d)\)[^\n]+)\n(\\draw[^\n]+)",update_label,out)
if num != 18:
    raise SystemExit("Missing peak labels")
Path("mainres-1c.tex").write_text(out)
with Path("mainres-1c.csv").open("w", newline="") as stream:
    writer = csv.writer(stream,lineterminator="\n")
    writer.writerow(["workload","configuration","policy","cycles_high","cycle_status","area_library_units","area_status","speedup_vs_1_1","ae_vs_1_1"])
    for r in rows:
        n,p = int(r["cgras"]),r["policy"]
        vals = metrics(r["workload"],n,p)
        writer.writerow([r["workload"],f"{policies.index(p)+1}({n})",p,r["cycles_high"],r["cycle_status"],r["area_library_units"],r["area_status"],*(f"{v:.9f}" for v in vals)])
