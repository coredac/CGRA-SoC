import hashlib
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
SOURCE = HERE.parents[1] / "multi-asic.json"
data = json.loads(SOURCE.read_text())
speedup = next(
    r["historical_speedup"] for r in data["rows"] if r["workload"] == "ResNet"
)
area = data["area"]["cgra"] / data["area"]["seven_asic"]

tex = [
    r"\documentclass[border=3pt]{standalone}",
    r"\usepackage{pgfplots}",
    r"\usepackage{mathptmx}",
    r"\pgfplotsset{compat=1.18}",
    r"\definecolor{cgra}{HTML}{7698BB}",
    r"\begin{document}",
    r"\begin{minipage}{7.3cm}\centering",
    r"\begin{tikzpicture}",
    r"\begin{axis}[width=5.8cm,height=2.3cm,scale only axis,",
    r"font=\fontsize{8}{9}\selectfont,xmin=.5,xmax=2.5,ymin=0,ymax=1.30,",
    r"xtick={1,2},xticklabels={Speedup,Area},ytick={0,.5,1},yticklabels={0,0.5,1.0},",
    r"ylabel={Normalized Value},ylabel style={at={(axis description cs:-.14,.5)},anchor=south,inner sep=0pt},",
    r"axis line style={black!70},tick style={black!70},tick align=outside,",
    r"xtick pos=left,ytick pos=left,scaled y ticks=false,clip=false,",
    r"ymajorgrids=true,major grid style={gray!25,line width=.2pt},axis on top=false,",
    r"extra y ticks={1},extra y tick labels={},extra y tick style={grid=major,major grid style={draw=black!45,dashed,line width=.4pt}},",
    r"legend columns=2,legend cell align=left,bar width=10pt,",
    r"legend style={at={(.5,1.06)},anchor=south,draw=black,fill=white,line width=.4pt,",
    r"font=\fontsize{8}{9}\selectfont,inner xsep=3pt,inner ysep=2pt,nodes={inner sep=1pt},/tikz/every even column/.append style={column sep=3pt}},",
    r"legend image code/.code={\draw[#1] (0cm,-.06cm) rectangle (.16cm,.06cm);}]",
    r"\addplot[ybar,bar shift=-8pt,fill=black!20,draw=black,line width=.35pt,forget plot] coordinates {(1,1) (2,1)};",
    rf"\addplot[ybar,bar shift=8pt,fill=cgra,draw=black,line width=.35pt,forget plot] coordinates {{(1,{speedup:.9f}) (2,{area:.9f})}};",
    r"\addlegendimage{fill=black!20,draw=black,solid,line width=.35pt}",
    r"\addlegendentry{ASIC}",
    r"\addlegendimage{fill=cgra,draw=black,solid,line width=.35pt}",
    r"\addlegendentry{CGRA}",
]
for x, shift, value in [(1, -8, 1), (1, 8, speedup), (2, -8, 1), (2, 8, area)]:
    tex.append(
        rf"\node[anchor=south,fill=white,inner sep=.5pt,yshift=3pt] at ([xshift={shift}pt]axis cs:{x},{value:.9f}){{{value:.2f}}};"
    )
tex += [
    r"\end{axis}",
    r"\end{tikzpicture}",
    r"\end{minipage}",
    r"\end{document}",
]
(HERE / "multi-asic-v5.tex").write_text("\n".join(tex) + "\n")
(HERE / "caption.txt").write_text(
    "Historical ResNet speedup is 1.08x, and the separate modified-core comparison has a normalized CGRA area of 0.91. Both pairs normalize their ASIC baseline to one; speedup uses the 1024-output ResNet block against the independent Add/ReLU ASIC with on-chip exchange, whereas area compares the modified CGRA against all seven expanded ASIC cores, including optional SAD. The performance and area results belong to different hardware revisions and do not establish a same-design performance/area improvement. Core area includes local/configuration storage but excludes endpoint adapters, DMA and external SPM; common-frequency and equal-throughput validation remain incomplete.\n"
)
(HERE / "plot-data.json").write_text(
    json.dumps(
        {
            "source": str(SOURCE),
            "source_sha256": hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
            "performance_workload": "ResNet",
            "performance_aggregation": "Three configuration-resident executions per backend; ratio of arithmetic mean cycles; not the three-workload mean",
            "bars": [
                {"metric": "speedup", "backend": "ASIC", "value": 1.0},
                {"metric": "speedup", "backend": "CGRA", "value": speedup},
                {"metric": "core_area", "backend": "ASIC", "value": 1.0},
                {"metric": "core_area", "backend": "CGRA", "value": area},
            ],
            "font_pt": 8,
            "intended_width_cm": 7.3,
            "bar_width_pt": 10,
            "label_decimal_places": 2,
            "zero_based_bar_axis": True,
            "single_axis": True,
            "same_hardware_revision": False,
            "version_scope_note_visible": False,
            "version_scope_record": "caption.txt",
            "footer_removed": True,
            "source_results_modified": False,
        },
        indent=2,
    )
    + "\n"
)
print(HERE / "multi-asic-v5.tex")
