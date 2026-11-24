"""Extend the validated GPT test to cold plus three alternating warm samples."""

from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[5]
OUT = Path(__file__).resolve().parent
PRE = ROOT / "build/experiments/asic/preflight/system"
source = (PRE / "lanes.c").read_text()
changes = {
    "static int run(int chained, unsigned input) {": "static int run(int chained, unsigned input, unsigned sample) {",
    "  if (!input) {": "  if (!sample) {",
    "int status = input ? 0 : configure(chained);": "int status = sample ? 0 : configure(chained);",
    "phase=%s input=%u": "phase=%s sample=%u input=%u",
    'input ? "warm" : "cold", input,': 'sample ? "warm" : "cold", sample, input,',
    "for (unsigned input = 0; input < 2; ++input) {\n        if (run(selected, input)) {": "for (unsigned sample = 0; sample < 4; ++sample) {\n        const unsigned input = sample % 2;\n        if (run(selected, input, sample)) {",
    "submission=AutoLink-both-backends CGRA-cache=full-static-per-phase": "submission=AutoLink-both-backends chain-scope=local-kernel-chain-not-full-graphAuto samples=4 inputs=0,1,0,1 CGRA-cache=full-static-per-phase",
}
for old, new in changes.items():
    if old not in source:
        raise SystemExit(f"Missing validated-source anchor: {old}")
    source = source.replace(old, new)
(OUT / "gpt.c").write_text(source)
for name in [
    "accel_generated.h",
    "auto_link_generated.h",
    "cgra_link_control_generated.h",
]:
    shutil.copyfile(PRE / "include" / name, OUT / "include" / name)
script = (PRE / "compile.sh").read_text()
script = script.replace("name=$1\n", "")
script = script.replace(
    "build/experiments/asic/preflight/system/include/",
    "build/experiments/asic/full/gpt/include/",
)
script = script.replace(
    "build/experiments/asic/preflight/system/$name",
    "build/experiments/asic/full/gpt/gpt",
)
script = script.replace(
    "-std=gnu99", "-MMD -MF build/experiments/asic/full/gpt/dependencies.d -std=gnu99"
)
(OUT / "compile.sh").write_text(script)
