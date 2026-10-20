"""Protect the fixed compiler preset against incompatible SoC layouts."""

import copy
import sys
import tempfile
import unittest
from pathlib import Path

import yaml

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from runtime_target import check_target

ROOT = Path(__file__).resolve().parents[2]
SOC = ROOT / "configs/soc/autolink/gpt2.yaml"
ARCH = ROOT / "configs/arch/attention.yaml"


class RuntimeTargetTest(unittest.TestCase):
    def test_current_target(self):
        if check_target(SOC, ARCH):
            self.fail("the current gpt2 preset was rejected")

    def test_layout_and_capture_drift(self):
        document = yaml.safe_load(SOC.read_text())
        changes = [
            ("buffer_slots", {"gemmini0": 3, "gemmini1": 2, "cgra0": 2}, "slots"),
            ("run_capacity", 0, "capacity"),
            ("capacity", {"stages": 4, "dependencies": 4, "results": 512}, "capacity"),
        ]
        for key, value, message in changes:
            with self.subTest(key=key), tempfile.TemporaryDirectory() as directory:
                changed = copy.deepcopy(document)
                changed["communication"][key] = value
                path = Path(directory) / "soc.yaml"
                path.write_text(yaml.safe_dump(changed, sort_keys=False))
                if not any(message in error for error in check_target(path, ARCH)):
                    self.fail(f"accepted incompatible {key}")
        with tempfile.TemporaryDirectory() as directory:
            changed = copy.deepcopy(document)
            changed["communication"]["instances"]["gemmini1"]["command_capacity"] = 3
            path = Path(directory) / "soc.yaml"
            path.write_text(yaml.safe_dump(changed, sort_keys=False))
            if "gemmini1 cannot capture the fixed GEMM template" not in check_target(
                path, ARCH
            ):
                self.fail("accepted insufficient Gemmini command capacity")

    def test_counter_and_mapping_drift(self):
        with tempfile.TemporaryDirectory() as directory:
            soc = yaml.safe_load(SOC.read_text())
            soc["execution"]["ctrl_count_bits"] = 7
            path = Path(directory) / "soc.yaml"
            path.write_text(yaml.safe_dump(soc, sort_keys=False))
            if not any("counter" in error for error in check_target(path, ARCH)):
                self.fail("accepted a counter too small for 170 ReLU steps")
            soc["execution"] = {"ctrl_count_bits": 16, "ctrl_count_per_iter": 1}
            path.write_text(yaml.safe_dump(soc, sort_keys=False))
            if not any("counter" in error for error in check_target(path, ARCH)):
                self.fail("accepted a control PC too small for ReLU II=5")
            arch = yaml.safe_load(ARCH.read_text())
            arch["tile_overrides"] = [
                {
                    "cgra_x": 0,
                    "cgra_y": 0,
                    "tile_x": 3,
                    "tile_y": 0,
                    "existence": True,
                    "fu_types": ["add"],
                }
            ]
            path = Path(directory) / "arch.yaml"
            path.write_text(yaml.safe_dump(arch, sort_keys=False))
            if not any("overrides" in error for error in check_target(SOC, path)):
                self.fail("accepted removal of the mapped load and comparison units")


if __name__ == "__main__":
    unittest.main()
