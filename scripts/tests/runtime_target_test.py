"""Check exported compiler resources and kernel metadata."""

import copy
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import yaml

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from runtime_target import kernel_metadata, target_metadata

ROOT = Path(__file__).resolve().parents[2]
SOC = ROOT / "configs/soc/autolink/gpt2.yaml"
ARCH = ROOT / "configs/arch/attention.yaml"
RELU = ROOT / "configs/kernels/kernel_relu_runtime_4x4.yaml"
ADD_RELU = ROOT / "configs/kernels/kernel_add_relu_runtime_4x4.yaml"


class RuntimeTargetTest(unittest.TestCase):
    def test_current_target(self):
        target = target_metadata(SOC, ARCH, [RELU, ADD_RELU])
        if [
            (item["name"], item["kind"], item["id"], item["spm_bytes"])
            for item in target["devices"]
        ] != [
            ("gemmini0", "gemmini", 0, 131072),
            ("gemmini1", "gemmini", 1, 131072),
            ("cgra0", "cgra", 2, 131072),
        ]:
            self.fail("exported endpoint identities differ from the SoC")
        if target["fabric"] != {
            "stages": 3,
            "edges": 3,
            "runs": 32,
            "results": 512,
            "collector": 2,
            "alignment": 16,
        }:
            self.fail(f"unexpected fabric resources: {target['fabric']}")
        if target["cgra"] != {
            "word_bits": 32,
            "counter_bits": 16,
            "words": 32768,
        }:
            self.fail("CGRA memory and counter resources were not exported")
        if target["kernel_sources"] != [str(RELU), str(ADD_RELU)]:
            self.fail("kernel sources were not recorded")
        relu, add_relu = target["kernels"]
        if relu != {
            "name": "relu_runtime",
            "header": "generated/cgra_relu_runtime_fast_api.h",
            "symbol": "RELU_RUNTIME",
            "input_bits": [32],
            "output_bits": [8, 32],
            "tile_rows": 2,
            "symbols": ["input0", "output", "elements"],
            "packets": 76,
            "static_packets": 90,
            "count_scale": 5,
            "count_offset": 10,
        }:
            self.fail(f"unexpected ReLU metadata: {relu}")
        if add_relu["input_bits"] != [32, 32] or add_relu["symbols"] != [
            "input0",
            "input1",
            "output",
            "elements",
        ]:
            self.fail("multi-input kernel symbols were not preserved")

    def test_resources_follow_soc(self):
        soc = yaml.safe_load(SOC.read_text())
        changed = copy.deepcopy(soc)
        communication = changed["communication"]
        communication["buffer_slots"]["gemmini0"] = 3
        communication["instances"]["gemmini1"]["command_capacity"] = 48
        communication["instances"]["gemmini1"]["patch_capacity"] = 16
        communication["instances"]["cgra0"]["packet_capacity"] = 160
        communication["capacity"] = {
            "stages": 5,
            "dependencies": 6,
            "jobs": {"gemmini0": 4, "gemmini1": 5, "cgra0": 6},
        }
        communication["run_capacity"] = 4
        changed["execution"]["ctrl_count_bits"] = 14
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "soc.yaml"
            path.write_text(yaml.safe_dump(changed, sort_keys=False))
            target = target_metadata(path, ARCH, [])
        first, second, cgra = target["devices"]
        if first["slots"] != 3 or first["jobs"] != 4:
            self.fail("endpoint slots/jobs are fixed to the example")
        if second["commands"] != 48 or second["patches"] != 16:
            self.fail("Gemmini capture capacities are fixed to the example")
        if cgra["packets"] != 160 or cgra["packed_words"] != 32768:
            self.fail("CGRA capture or packed-window resources were lost")
        if target["fabric"]["stages"] != 5 or target["fabric"]["edges"] != 6:
            self.fail("graph capacity is fixed to the example")
        if target["fabric"]["results"] != 20:
            self.fail("default result capacity does not match hardware")
        if target["cgra"]["counter_bits"] != 14:
            self.fail("execution counter width is fixed to the example")

    def test_kernel_identity_and_defaults(self):
        kernel = yaml.safe_load(RELU.read_text())
        kernel["kernel"]["name"] = "custom_activation"
        kernel["kernel"]["kernel_yaml"] = str(
            ROOT / "configs/kernels/runtime/relu.yaml"
        )
        kernel["runtime"]["tile_rows"] = 4
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "custom.yaml"
            path.write_text(yaml.safe_dump(kernel, sort_keys=False))
            exported = kernel_metadata(path, ARCH, SOC)
        if (
            exported["name"] != "custom_activation"
            or exported["symbol"] != "CUSTOM_ACTIVATION"
            or exported["header"] != "generated/cgra_custom_activation_fast_api.h"
            or exported["tile_rows"] != 4
        ):
            self.fail("kernel identity/defaults did not follow metadata")

    def test_export_and_check(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "target.yaml"
            source = Path(directory) / "kernel.yaml"
            kernel = yaml.safe_load(RELU.read_text())
            kernel["kernel"]["kernel_yaml"] = str(
                RELU.parent / kernel["kernel"]["kernel_yaml"]
            )
            source.write_text(yaml.safe_dump(kernel, sort_keys=False))
            command = [
                sys.executable,
                str(ROOT / "scripts/runtime_target.py"),
                "--soc-yaml",
                str(SOC),
                "--arch-yaml",
                str(ARCH),
                "--output",
                str(path),
                str(source),
            ]
            subprocess.run(command, check=True, capture_output=True, text=True)
            check = [*command[:-1], "--check"]
            subprocess.run(check, check=True, capture_output=True, text=True)
            content = path.read_text()
            kernel["runtime"]["tile_rows"] = 4
            source.write_text(yaml.safe_dump(kernel, sort_keys=False))
            checked = subprocess.run(check, capture_output=True, text=True)
            if checked.returncode == 0 or "stale generated file" not in checked.stderr:
                self.fail("check accepted a stale target")
            if path.read_text() != content:
                self.fail("check modified the target")


if __name__ == "__main__":
    unittest.main()
