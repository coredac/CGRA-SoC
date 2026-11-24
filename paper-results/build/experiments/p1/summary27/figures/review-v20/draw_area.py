"""Draw compact IP area composition with within-workload normalization."""

import csv
import hashlib
import json
import math
from pathlib import Path

HERE = Path(__file__).resolve().parent
SOURCE = HERE / "layers.csv"
ROWS = list(csv.DictReader(SOURCE.open()))
GROUPS = [
    ("gem_compute", "GEMM compute", "7698BB", ["gemmini_core_logic"]),
    ("gem_spm", "GEMM SPM", "A5BED2", ["gemmini_core_sram", "external_spm_sram"]),
    (
        "gem_adapter",
        "GEMM adapter",
        "D2BF93",
        ["gemmini_adapter_logic", "gemmini_adapter_sram"],
    ),
    ("cgra_compute", "CGRA compute", "80AAA1", ["cgra_core_logic"]),
    ("cgra_spm", "CGRA SPM", "B1C7AC", ["cgra_core_sram"]),
    (
        "cgra_adapter",
        "CGRA adapter",
        "D3A48A",
        ["cgra_adapter_logic", "cgra_adapter_sram"],
    ),
    (
        "autolink",
        "AutoLink",
        "BA858A",
        [
            "shared_scheduler_logic",
            "shared_cdc_logic",
            "shared_profile_logic",
            "shared_profile_sram",
        ],
    ),
    ("others", "Others", "9E99AA", ["external_spm_logic"]),
]
STACK = (
    "gem_compute",
    "gem_spm",
    "cgra_compute",
    "cgra_spm",
    "gem_adapter",
    "cgra_adapter",
    "autolink",
    "others",
)
MODES = ("Manual", "Hybrid", "Auto")
POLICIES = dict(zip(MODES, ("1", "2", "3")))
WORKLOADS = (("resnet", "ResNet"), ("gpt2-rows", "GPT-2"), ("gcn", "GCN"))
POINTS = [
    (1 + wi + offset, prefix, mode)
    for wi, (prefix, _) in enumerate(WORKLOADS)
    for offset, mode in zip((-0.28, 0, 0.28), MODES)
]


def components(prefix, mode):
    cats = {}
    for row in ROWS:
        if row["configuration"] != prefix + "-cgra1" or row["mode"] != mode:
            continue
        for kind in ("logic", "sram"):
            cat = row[kind + "_category"]
            if cat:
                cats[cat] = cats.get(cat, 0.0) + float(row[kind])
    return {
        name: sum(cats.get(cat, 0) for cat in members) for name, _, _, members in GROUPS
    }


DATA = {(prefix, mode): components(prefix, mode) for _, prefix, mode in POINTS}
TOTALS = {key: sum(value.values()) for key, value in DATA.items()}


def draw(kind):
    fraction = kind == "share"
    limit = 100 if fraction else 1
    denominator = {
        key: total if fraction else max(TOTALS[(key[0], m)] for m in MODES)
        for key, total in TOTALS.items()
    }
    values = {
        key: {cat: limit * area / denominator[key] for cat, area in group.items()}
        for key, group in DATA.items()
    }
    ticks = ",".join(str(x) for x, _, _ in POINTS)
    ticklabels = ",".join(POLICIES[mode] for _, _, mode in POINTS)
    ylabel = r"Area share (\%)" if fraction else "Normalized area"
    yticks = "0,25,50,75,100" if fraction else "0,.25,.5,.75,1"
    lines = [r"""\documentclass[border=3pt]{standalone}
\usepackage{pgfplots}
\usepackage{mathptmx}
\pgfplotsset{compat=1.18}
"""]
    for name, _, color, _ in GROUPS:
        lines.append(r"\definecolor{" + name + "}{HTML}{" + color + "}")
    lines.append(
        r"""\begin{document}
\begin{minipage}{8.3cm}
\centering
\begin{tikzpicture}
\begin{axis}[name=area,
width=7cm,height=3.2cm,scale only axis,
ybar stacked,bar shift=0pt,bar width=14pt,
xmin=.5,xmax=3.5,ymin=0,ymax="""
        + str(limit)
        + ",\n"
        + "ytick={"
        + yticks
        + "},scaled y ticks=false,\n"
        + "ylabel={"
        + ylabel
        + "},\n"
        + r"""font=\fontsize{8}{9}\selectfont,tick align=outside,
xtick pos=left,ytick pos=left,axis line style={black!70},tick style={black!70},
ylabel style={at={(axis description cs:-.12,.5)},anchor=south,inner sep=0pt},
xticklabel style={align=center},
ymajorgrids=true,major grid style={gray!25,line width=.2pt},axis on top=false,clip=false,
legend columns=4,legend cell align=left,
legend style={at={(.428571,1.06)},anchor=south,draw=black,fill=white,line width=.4pt,
font=\fontsize{8}{9}\selectfont,inner xsep=2pt,inner ysep=2pt,
nodes={inner sep=1pt},/tikz/every even column/.append style={column sep=1pt}},
legend image code/.code={\draw[#1] (0cm,-.06cm) rectangle (.16cm,.06cm);},
xtick={"""
        + ticks
        + "},xticklabels={"
        + ticklabels
        + "}]"
    )
    for name in STACK:
        coords = " ".join(
            f"({x},{values[(prefix, mode)][name]:.9f})" for x, prefix, mode in POINTS
        )
        lines.append(
            r"\addplot[fill="
            + name
            + r",draw=none,forget plot] coordinates {"
            + coords
            + "};"
        )
    # Read the legend row by row in bottom-to-top stack order.
    for index in (0, 1, 3, 4, 2, 5, 6, 7):
        name, label, _, _ = GROUPS[index]
        lines += [
            r"\addlegendimage{fill=" + name + r",draw=black,line width=.35pt}",
            r"\addlegendentry{" + label + "}",
        ]
    # Draw annotations on a later empty axis so stacked fills cannot cover them.
    lines += [
        r"\end{axis}",
        r"\begin{axis}[at={(area.south west)},anchor=south west,width=7cm,height=3.2cm,scale only axis,xmin=.5,xmax=3.5,ymin=0,ymax="
        + str(limit)
        + r",hide axis,clip=false,font=\fontsize{8}{9}\selectfont]",
    ]
    for x, prefix, mode in POINTS:
        cumulative = 0.0
        total = sum(values[(prefix, mode)].values())
        parts = [
            (name, values[(prefix, mode)][name])
            for name in STACK
            if values[(prefix, mode)][name] > 0
        ]
        for index, (name, value) in enumerate(parts):
            if fraction and value >= 8:
                color = (
                    "white"
                    if name in ("gem_compute", "gem_spm", "cgra_compute")
                    else "black"
                )
                lines.append(
                    r"\node[text="
                    + color
                    + r",inner sep=0pt,font=\fontsize{8}{9}\selectfont] at (axis cs:"
                    + f"{x},{cumulative+value/2:.9f}"
                    + ") {"
                    + f"{value:.0f}"
                    + "};"
                )
            cumulative += value
            if index + 1 < len(parts):
                height = min(value, parts[index + 1][1]) / limit * 3.2 / 2.54 * 72.27
                width = min(0.15, 0.4 * height)
                lines.append(
                    r"\draw[black!75"
                    + f",line width={width:.4f}pt]"
                    + r" ([xshift=-7pt]axis cs:"
                    + f"{x},{cumulative:.9f}"
                    + r") -- ([xshift=7pt]axis cs:"
                    + f"{x},{cumulative:.9f}"
                    + ");"
                )
        lines.append(
            r"\draw[black,line width=.35pt] ([xshift=-7pt]axis cs:"
            + f"{x},0"
            + r") rectangle ([xshift=7pt]axis cs:"
            + f"{x},{total:.9f}"
            + ");"
        )
    for x, (_, label) in enumerate(WORKLOADS, 1):
        lines.append(
            r"\node[anchor=north,inner sep=0pt,yshift=-18pt] at (axis cs:"
            + f"{x},0"
            + ") {"
            + label
            + "};"
        )
    lines += [
        r"\end{axis}",
        r"\coordinate (plot-bottom) at (current bounding box.south);",
        r"\coordinate (plot-top) at (current bounding box.north);",
        r"\coordinate (plot-left) at ([xshift=-1.15cm]area.south west);",
        r"\coordinate (plot-right) at ([xshift=.15cm]area.north east);",
        r"\pgfresetboundingbox",
        r"\path[use as bounding box] (plot-left |- plot-bottom) rectangle (plot-right |- plot-top);",
        r"\end{tikzpicture}",
        r"\end{minipage}",
        r"\end{document}",
    ]
    (HERE / f"area-{kind}.tex").write_text("\n".join(lines) + "\n")
    records = [
        dict(
            workload=dict(WORKLOADS)[prefix],
            policy=mode,
            denominator=denominator[(prefix, mode)],
            **values[(prefix, mode)],
        )
        for _, prefix, mode in POINTS
    ]
    with (HERE / f"area-{kind}.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)


def main():
    draw("relative")
    categories = [cat for _, _, _, members in GROUPS for cat in members]
    available = {
        r[k + "_category"]
        for r in ROWS
        for k in ("logic", "sram")
        if r[k + "_category"]
    }
    checks = [
        (
            "categories partition original breakdown",
            len(categories) == len(set(categories)) and set(categories) == available,
        )
    ]
    for _, prefix, mode in POINTS:
        original = sum(
            float(r["area"])
            for r in ROWS
            if r["configuration"] == prefix + "-cgra1" and r["mode"] == mode
        )
        checks.append(
            (
                prefix + " " + mode + " conserved area",
                math.isclose(original, TOTALS[(prefix, mode)], abs_tol=1e-5),
            )
        )
    method = {
        "status": "compact_GPT_single_CGRA_data_SPM_area_projection_not_new_synthesis",
        "revision": "GPT one-CGRA core data SRAM is projected from 128 to 32 KiB using the same macro family. All other absolute component areas are unchanged; data-memory peripheral logic is not re-synthesized. Relative bar heights use the updated workload-local maximum area.",
        "share": "Each bar sums to100 percent of its own accounted accelerator-subsystem area. Integer labels are rounded percentages and only shown for components at least8 percent.",
        "relative": "Each bar is divided by the largest total area among the three policies for the SAME workload. No cross-workload denominator.",
        "groups": {name: members for name, _, _, members in GROUPS},
        "stack_bottom_to_top": STACK,
        "colors": {name: color for name, _, color, _ in GROUPS},
        "palette": "Muted categorical palette: GEMM blue, CGRA teal/sage, adapters sand/peach, AutoLink dusty rose, Others gray-violet. Adjacent regions transition through blue-green and green-sand rather than directly between blue and orange. Legend follows the stack order row by row.",
        "borders": "All internal separators use black75 at up to 0.15 pt, capped at 40 percent of the thinner adjacent segment height to avoid erasing small areas. Outer bar outlines remain black at 0.35 pt. Zero-height categories do not add duplicate separators.",
        "compute_scope": "IP core logic includes local control, routing and registers; not independently synthesized arithmetic-only datapaths.",
        "gemmini_spm": "Includes accumulator SRAM and the external Gemmini SPM shared with other endpoints. The latter is counted exactly once, reclassified from Shared SPM; no new storage was added.",
        "adapter": "Combines existing adapter logic and configuration SRAM.",
        "autolink": "Combines scheduler/stages, CDC, profile control and profile SRAM.",
        "others": "External SPM peripheral/bridge/local-crossbar logic. CPU/cache/global interconnect remain outside scope and are not assumed zero.",
        "gcn": "Per-segment uses the prior conditional shared-template subtotal; new binding logic excluded. Each100percent bar is normalized to that subtotal, not a fully synthesized whole-SoC area.",
        "source": str(SOURCE),
        "source_sha256": hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
        "checks": checks,
    }
    (HERE / "area-method.json").write_text(json.dumps(method, indent=2) + "\n")
    (HERE / "area-caption.txt").write_text(
        "Area breakdown of one GEMM and one CGRA. Policy codes 1/2/3 denote per-task/per-segment/per-graph control. Areas are normalized to the largest modeled total within each workload. GPT uses the compact 32 KiB data-SPM projection with unchanged peripheral logic; GCN per-segment excludes additional binding logic. CPU, caches, and global SoC interconnect are outside the accounting scope.\n"
    )
    print(f"{sum(ok for _, ok in checks)}/{len(checks)} regrouping checks passed")


if __name__ == "__main__":
    main()
