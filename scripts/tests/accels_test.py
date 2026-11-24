"""Check per-instance accelerator parameters without changing generated outputs."""

import sys
import unittest
from pathlib import Path

import yaml

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from generate_accels import instance_layout, load_layout, scala_text

ROOT = Path(__file__).resolve().parents[2]


class AccelsTest(unittest.TestCase):
    def test_default_gemmini_array(self):
        layout = load_layout(ROOT / "configs/soc/autolink/attention.yaml")
        if layout.gemmini_arrays != {"gemmini0": {}, "gemmini1": {}}:
            self.fail(f"Gemmini array defaults changed: {layout.gemmini_arrays}")
        output = scala_text(layout)
        for name in layout.gemmini_arrays:
            if f'"{name}" -> GemminiArrayParams()' not in output:
                self.fail(f"Scala output omitted the default array for {name}")

    def test_per_instance_gemmini_array(self):
        document = yaml.safe_load(
            (ROOT / "configs/soc/autolink/attention.yaml").read_text()
        )
        baseline = instance_layout(document)
        document["communication"]["instances"]["gemmini0"].update(
            dim=64, acc_kib=64, command_capacity=9
        )
        layout = instance_layout(document)
        if layout.gemmini_arrays != {
            "gemmini0": {"dim": 64, "acc_kib": 64},
            "gemmini1": {},
        }:
            self.fail(f"Gemmini arrays are not independent: {layout.gemmini_arrays}")
        caches = {name: dict(params) for name, params in baseline.gemminis.items()}
        caches["gemmini0"]["command_capacity"] = 9
        if layout.gemminis != caches:
            self.fail("Gemmini array and command cache parameters are mixed")
        if layout.instances != baseline.instances:
            self.fail("Gemmini array parameters changed accelerator addresses")
        output = scala_text(layout)
        if '"gemmini0" -> GemminiArrayParams(dim = 64, accKiB = 64)' not in output:
            self.fail("Scala output omitted the selected Gemmini array parameters")
        if '"gemmini1" -> GemminiArrayParams()' not in output:
            self.fail("Scala output changed the other Gemmini array")

    def test_default_packet_capacity(self):
        layout = load_layout(ROOT / "configs/soc/autolink/attention.yaml")
        if layout.cgra_packets != {"cgra0": 96}:
            self.fail(f"Attention packet capacity changed: {layout.cgra_packets}")
        if '"cgra0" -> 96' not in scala_text(layout):
            self.fail("Scala output omitted the default CGRA packet capacity")
        if layout.cgra_patches != {"cgra0": True}:
            self.fail("CGRA runtime patches must remain enabled by default")

    def test_per_instance_packet_capacity(self):
        document = yaml.safe_load(
            (ROOT / "configs/soc/autolink/blocks.yaml").read_text()
        )
        document["communication"]["instances"]["cgra1"]["packet_capacity"] = 192
        document["communication"]["instances"]["cgra1"]["patch_support"] = False
        layout = instance_layout(document)
        if layout.cgra_packets != {"cgra0": 96, "cgra1": 192}:
            self.fail(f"CGRA capacities are not independent: {layout.cgra_packets}")
        output = scala_text(layout)
        if '"cgra0" -> 96' not in output or '"cgra1" -> 192' not in output:
            self.fail("Scala output did not preserve per-instance capacities")
        if layout.cgra_patches != {"cgra0": True, "cgra1": False}:
            self.fail("CGRA patch support is not independent per instance")
        if '"cgra0" -> true' not in output or '"cgra1" -> false' not in output:
            self.fail("Scala output omitted per-instance CGRA patch support")

    def test_independent_asic_layout(self):
        path = ROOT / "configs/soc/autolink/independent.yaml"
        layout = load_layout(path)
        asics = [accel for accel in layout.instances if accel.kind == "asic"]
        sizes = [32768, 32768, 32768, 8192, 8192, 16384]
        if [accel.size for accel in asics] != sizes:
            self.fail("Independent ASIC SPM capacities changed")
        if sum(accel.size for accel in asics) != 131072:
            self.fail("Independent ASIC SPM budget is not 128 KiB")
        base = 0x60080000
        for accel in asics:
            if accel.base != base:
                self.fail(f"Private SPM address gap or overlap at {accel.name}")
            base += accel.size
        functions = {
            "add0": "add",
            "relu0": "relu",
            "gcn0": "gcn",
            "norm0": "norm",
            "softmax0": "softmax",
            "gpt_vector0": "gpt_vector",
        }
        if layout.asic_functions != functions:
            self.fail("Independent ASIC function selection changed")
        output = scala_text(layout)
        for name, function in functions.items():
            if f'"{name}" -> "{function}"' not in output:
                self.fail(f"Scala output omitted ASIC family {name}")
        jobs = yaml.safe_load(path.read_text())["communication"]["capacity"]["jobs"]
        if [jobs[accel.name] for accel in asics] != [4, 4, 2, 8, 6, 8]:
            self.fail("Independent ASIC descriptor partition changed")
        if layout.control_base < base:
            self.fail("Control pages overlap private ASIC SPMs")

    def test_fixed_asic_default_sizes(self):
        layout = load_layout(ROOT / "configs/soc/autolink/split.yaml")
        asics = [accel for accel in layout.instances if accel.kind == "asic"]
        if [accel.size for accel in asics] != [65536, 65536]:
            self.fail("Existing split ASIC SPM defaults changed")
        if asics[1].base != asics[0].base + 65536:
            self.fail("Existing split ASIC address layout changed")

    def test_gpt2_configuration(self):
        path = ROOT / "configs/soc/autolink/gpt2.yaml"
        layout = load_layout(path)
        baseline = load_layout(ROOT / "configs/soc/autolink/attention.yaml")
        if layout.instances != baseline.instances:
            self.fail("GPT-2 capacity changes must preserve accelerator memory layout")
        if layout.cgra_packets != {"cgra0": 192}:
            self.fail("GPT-2 does not select the expanded packet cache")
        communication = yaml.safe_load(path.read_text())["communication"]
        if communication["capacity"]["jobs"] != {
            "gemmini0": 32,
            "gemmini1": 32,
            "cgra0": 32,
        }:
            self.fail("GPT-2 does not reserve the planned per-endpoint job caches")
        if (
            communication["run_capacity"] != 32
            or communication["capacity"]["results"] != 512
        ):
            self.fail("GPT-2 run or result capacity differs from the control budget")


if __name__ == "__main__":
    unittest.main()
