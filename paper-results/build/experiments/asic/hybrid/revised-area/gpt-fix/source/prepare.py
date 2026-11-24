"""Freeze family five with pipeline cuts in native square and bias arithmetic."""

import hashlib
import json
from pathlib import Path


def digest(text):
    return hashlib.sha256(text.encode()).hexdigest()


def main():
    output = Path(__file__).resolve().parent
    source = output.parents[2] / "capability-trial"
    native = source / "native/GptVector.sv"
    original = native.read_text()
    edits = [
        ("COEFFICIENT, SQUARE,", "COEFFICIENT, SQUARE, SQUARE_SUM, SQUARE_FINISH,"),
        ("GATE, PRODUCT, BIAS, ACTIVATE", "GATE, PRODUCT, BIAS, BIAS_FINISH, ACTIVATE"),
        ("  logic negative;", "  logic negative;\n  logic [31:0] square_low;\n  logic [15:0] square_cross_a, square_cross_b, square_upper;\n  logic [16:0] bias_low;"),
        (
            "        SQUARE: begin square <= coefficient * delta; state <= CORRECTION; end",
            """        SQUARE: begin
          square_low <= coefficient[15:0] * delta[15:0];
          square_cross_a <= coefficient[31:16] * delta[15:0];
          square_cross_b <= coefficient[15:0] * delta[31:16];
          state <= SQUARE_SUM;
        end
        SQUARE_SUM: begin
          square_upper <= square_low[31:16] + square_cross_a;
          state <= SQUARE_FINISH;
        end
        SQUARE_FINISH: begin
          square <= {16'(square_upper + square_cross_b), square_low[15:0]};
          state <= CORRECTION;
        end""",
        ),
        (
            "        BIAS: begin biased <= product + 4096; state <= ACTIVATE; end",
            """        BIAS: begin
          bias_low <= {1'b0, product[15:0]} + 17'd4096;
          state <= BIAS_FINISH;
        end
        BIAS_FINISH: begin
          biased <= {16'(product[31:16] + {15'd0, bias_low[16]}), bias_low[15:0]};
          state <= ACTIVATE;
        end""",
        ),
    ]
    revised = original
    for before, after in edits:
        if revised.count(before) != 1:
            return "Source edit does not have one exact match: " + before
        revised = revised.replace(before, after, 1)
    restored = revised
    for before, after in reversed(edits):
        restored = restored.replace(after, before, 1)
    files = [path for folder in ["rtl", "native"] for path in sorted((source / folder).glob("*.sv"))]
    snapshots = []
    hashes = {}
    for path in files:
        text = path.read_text()
        hashes[str(path.relative_to(source))] = digest(text)
        if path == native:
            text = revised
        if path.name == "ExpandedAsic.sv":
            if text.count("parameter integer FAMILY = 0") != 1:
                return "ExpandedAsic FAMILY default is not unique"
            text = text.replace("parameter integer FAMILY = 0", "parameter integer FAMILY = 5", 1)
        snapshots.append(text)
    full = "\n".join(snapshots)
    (output / "GptVector.sv").write_text(revised)
    (output / "all.sv").write_text(full)
    manifest = {
        "top": "ExpandedAsic",
        "family": 5,
        "input_directory": str(source.relative_to(Path.cwd())),
        "input_sha256": hashes,
        "all_sv_sha256": digest(full),
        "native_sv_sha256": digest(revised),
        "only_documented_native_edits": restored == original,
        "unchanged": "All remaining native FSM actions, busy/done/start semantics, three arrays and primitive-bank logic are byte-identical to frozen input. Other modules change only ExpandedAsic FAMILY default to five.",
        "latency_change": "Native operation zero adds three cycles per element: SQUARE adds two and BIAS adds one. Other operation paths unchanged. Busy stays asserted across inserted states; done remains emitted only by final STORE.",
        "pipeline_bits_added": 97,
        "pipeline_registers": {"square_low": 32, "square_cross_a": 16, "square_cross_b": 16, "square_upper": 16, "bias_low": 17},
        "arithmetic": "Keep low 32 bits of square multiplication and bias sum for every 32-bit input, including signed overflow. No delta range restriction.",
        "timing_expectation": "Replace one 32x32 truncated square multiply by registered 16x16 partial products and two successive 16-bit add stages. Split bias carry at bit16. Closure requires remapping at unchanged constraints.",
        "simulation_run": False,
        "performance_run": False,
    }
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps({key: manifest[key] for key in ["top", "family", "all_sv_sha256", "only_documented_native_edits", "pipeline_bits_added"]}, indent=2))


if __name__ == "__main__":
    import sys

    sys.exit(main())
