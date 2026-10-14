"""Check per-instance CGRA packet capacities without changing generated outputs."""

import sys
import unittest
from pathlib import Path

import yaml

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from generate_accels import instance_layout, load_layout, scala_text

ROOT = Path(__file__).resolve().parents[2]


class AccelsTest(unittest.TestCase):
    def test_default_packet_capacity(self):
        layout = load_layout(ROOT / "configs/soc/autolink/attention.yaml")
        if layout.cgra_packets != {"cgra0": 96}:
            self.fail(f"Attention packet capacity changed: {layout.cgra_packets}")
        if '"cgra0" -> 96' not in scala_text(layout):
            self.fail("Scala output omitted the default CGRA packet capacity")

    def test_per_instance_packet_capacity(self):
        document = yaml.safe_load(
            (ROOT / "configs/soc/autolink/blocks.yaml").read_text()
        )
        document["communication"]["instances"]["cgra1"]["packet_capacity"] = 192
        layout = instance_layout(document)
        if layout.cgra_packets != {"cgra0": 96, "cgra1": 192}:
            self.fail(f"CGRA capacities are not independent: {layout.cgra_packets}")
        output = scala_text(layout)
        if '"cgra0" -> 96' not in output or '"cgra1" -> 192' not in output:
            self.fail("Scala output did not preserve per-instance capacities")

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
