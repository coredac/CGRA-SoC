"""Stage dedicated accelerator RTL for the Chipyard black box."""

from pathlib import Path

from generate_gemmini_ext_spm import write

ROOT = Path(__file__).resolve().parents[2]
SOURCES = (
    "Core.sv",
    "CoreMemory.sv",
    "Fixed.sv",
    "FixedMemory.sv",
    "vector/Gcn.sv",
    "vector/Vector.sv",
    "gpt/norm.sv",
    "gpt/div.sv",
    "gpt/softmax.sv",
    "gpt/vector.sv",
)


def stage(check=False):
    target = ROOT / "chipyard/generators/chipyard/src/main/resources/vsrc/asic"
    for name in SOURCES:
        write(target / name, (ROOT / "rtl/asic" / name).read_text(), check)
