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
GEMMINI = ROOT / "configs/kernels/gemmini_tile.yaml"


class RuntimeTargetTest(unittest.TestCase):
    def test_flat_elements(self):
        path = ROOT / "configs/kernels/kernel_block_requant_4x4.yaml"
        exported = kernel_metadata(path, ARCH, SOC)
        if not exported["flat"]:
            self.fail("flat-element capability was not exported")
        source = yaml.safe_load(path.read_text())
        source["kernel"]["kernel_yaml"] = str(
            ROOT / "VectorCGRA/validation/test/block_requant.yaml"
        )
        source["runtime"]["output_shapes"] = [["m", "s"]]
        with tempfile.TemporaryDirectory() as directory:
            changed = Path(directory) / "reduction.yaml"
            changed.write_text(yaml.safe_dump(source, sort_keys=False))
            with self.assertRaisesRegex(ValueError, "matching elementwise shapes"):
                kernel_metadata(changed, ARCH, SOC)

    def test_current_target(self):
        target = target_metadata(SOC, ARCH, [RELU, ADD_RELU, GEMMINI])
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
        if target["kernel_sources"] != [str(RELU), str(ADD_RELU), str(GEMMINI)]:
            self.fail("kernel sources were not recorded")
        relu, add_relu, gemmini = target["kernels"]
        if relu != {
            "name": "relu_runtime",
            "kind": "cgra",
            "header": "generated/cgra_relu_runtime_fast_api.h",
            "symbol": "RELU_RUNTIME",
            "input_bits": [[8, 32]],
            "output_bits": [[8, 32]],
            "input_shapes": [["m", "n"]],
            "output_shapes": [["m", "n"]],
            "iteration_shape": ["m", "n"],
            "tile_rows": 2,
            "max_tile_rows": 65535,
            "shape_limits": {},
            "whole_inputs": [],
            "symbols": ["input0", "output", "elements"],
            "packets": 76,
            "static_packets": 90,
            "count_scale": 5,
            "count_offset": 10,
        }:
            self.fail(f"unexpected ReLU metadata: {relu}")
        if add_relu["input_bits"] != [[8, 32], [8, 32]] or add_relu["symbols"] != [
            "input0",
            "input1",
            "output",
            "elements",
        ]:
            self.fail("multi-input kernel symbols were not preserved")
        if gemmini != {
            "name": "gemmini_i8",
            "kind": "gemmini",
            "input_bits": [[8], [8]],
            "output_bits": [[32]],
            "input_shapes": [["m", "k"], ["k", "n"]],
            "output_shapes": [["m", "n"]],
            "tile_rows": 16,
            "max_tile_rows": 16,
            "shape_limits": {"k": 16, "n": 16},
            "whole_inputs": [1],
            "commands": 8,
            "patches": 3,
        }:
            self.fail(f"unexpected native interface metadata: {gemmini}")

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

    def test_row_streams(self):
        total = kernel_metadata(
            ROOT / "configs/kernels/kernel_block_ln_sum_4x4.yaml", ARCH, SOC
        )
        norm = kernel_metadata(
            ROOT / "configs/kernels/kernel_block_ln_norm_4x4.yaml", ARCH, SOC
        )
        if total["iteration_shape"] != ["m", "n"] or total["output_shapes"] != [
            ["m", "s"]
        ]:
            self.fail("reduction iteration shape must differ from its output shape")
        if norm["whole_inputs"] != [1, 2] or norm["input_shapes"][1] != ["s", "n"]:
            self.fail("whole-input broadcast shape was lost")
        if norm["symbols"] != [
            "input0",
            "input1",
            "input2",
            "output",
            "input3",
            "input4",
            "columns",
            "elements",
        ]:
            self.fail("native symbol order changed")

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

    def test_native_interface_identity(self):
        kernel = yaml.safe_load(GEMMINI.read_text())
        kernel["kernel"]["name"] = "matrix_tile"
        kernel["runtime"]["tile_rows"] = 4
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "custom.yaml"
            path.write_text(yaml.safe_dump(kernel, sort_keys=False))
            exported = kernel_metadata(path, ARCH, SOC)
        if exported != {
            "name": "matrix_tile",
            "kind": "gemmini",
            **kernel["runtime"],
        }:
            self.fail("native interface selection depends on a fixed name")

    def test_copy_interface(self):
        path = ROOT / "configs/kernels/gemmini_copy.yaml"
        source = yaml.safe_load(path.read_text())
        exported = kernel_metadata(path, ARCH, SOC)
        expected = {
            "name": source["kernel"]["name"],
            "kind": "gemmini_copy",
            **source["runtime"],
        }
        expected["query"] = {
            "library": str(ROOT / "build/native-query.so"),
            "symbol": "amoeba_query_copy",
        }
        if exported != expected:
            self.fail("native copy capabilities were not preserved")

    def test_parameter_steps(self):
        for name in ("max", "exp"):
            path = ROOT / f"configs/kernels/kernel_attention_{name}_4x4.yaml"
            exported = kernel_metadata(path, ARCH, SOC)
            if exported["parameters"] != ["columns", "row"] or exported[
                "parameter_steps"
            ] != {"row": 1}:
                self.fail("attention row advancement was not exported")
        source = yaml.safe_load(
            (ROOT / "configs/kernels/kernel_block_requant_4x4.yaml").read_text()
        )
        source["runtime"]["flat"] = False
        source["runtime"]["symbols"][2]["row_stride"] = 3
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "custom.yaml"
            path.write_text(yaml.safe_dump(source, sort_keys=False))
            exported = kernel_metadata(path, ARCH, SOC)
        if exported["parameter_steps"] != {"divisor": 3}:
            self.fail("parameter advancement depends on a fixed role name")

    def test_export_and_check(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "target.yaml"
            source = Path(directory) / "kernel.yaml"
            native_source = Path(directory) / "native.yaml"
            native = yaml.safe_load(GEMMINI.read_text())
            native_source.write_text(yaml.safe_dump(native, sort_keys=False))
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
                str(native_source),
            ]
            subprocess.run(command, check=True, capture_output=True, text=True)
            check = [*command[:-2], "--check"]
            subprocess.run(check, check=True, capture_output=True, text=True)
            content = path.read_text()
            kernel["runtime"]["tile_rows"] = 4
            source.write_text(yaml.safe_dump(kernel, sort_keys=False))
            checked = subprocess.run(check, capture_output=True, text=True)
            if checked.returncode == 0 or "stale generated file" not in checked.stderr:
                self.fail("check accepted a stale target")
            if path.read_text() != content:
                self.fail("check modified the target")
            subprocess.run(command, check=True, capture_output=True, text=True)
            native["runtime"]["max_tile_rows"] = 8
            native_source.write_text(yaml.safe_dump(native, sort_keys=False))
            checked = subprocess.run(check, capture_output=True, text=True)
            if checked.returncode == 0 or "stale generated file" not in checked.stderr:
                self.fail("check ignored changed native interface capabilities")


if __name__ == "__main__":
    unittest.main()
