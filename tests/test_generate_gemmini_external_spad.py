import os
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import yaml

from scripts import generate_single_cgra
from scripts.generate_gemmini_external_spad import (
    DEFAULT_C_HEADER_OUT,
    DEFAULT_CONTROL_HEADER_OUT,
    DEFAULT_SCALA_OUT,
    DEFAULT_SOC_YAML,
    generate_c_header,
    generate_control_c_header,
    generate_scala,
    load_contract,
    write_or_check,
)


class GemminiExternalSpadGeneratorTest(unittest.TestCase):
    def test_default_yaml_generates_matching_stable_interfaces(self) -> None:
        contract = load_contract(DEFAULT_SOC_YAML)

        self.assertEqual(contract.base_address, 0x60000000)
        self.assertEqual(contract.size_bytes, 0x10000)
        self.assertEqual(contract.production_control_address, 0x60010000)
        self.assertEqual(contract.control_page_size_bytes, 4096)
        self.assertEqual(contract.output_slot_count, 2)
        self.assertEqual(contract.output_slot_size_bytes, 1024)
        self.assertEqual(contract.output_reserved_base, 0x6000F800)
        self.assertEqual(contract.output_slot_base(0), 0x6000F800)
        self.assertEqual(contract.output_slot_base(1), 0x6000FC00)
        self.assertEqual(contract.output_slot_row(0), 0x0F80)
        self.assertEqual(contract.output_slot_row(1), 0x0FC0)
        self.assertEqual(contract.spad_row_bytes, 16)
        self.assertEqual(contract.full_width_row_stride, 4)

        scala = generate_scala(contract, DEFAULT_SOC_YAML)
        header = generate_c_header(contract, DEFAULT_SOC_YAML)
        control_header = generate_control_c_header(contract, DEFAULT_SOC_YAML)
        self.assertIn('val baseAddress: BigInt = BigInt("60000000", 16)', scala)
        self.assertIn('val outputReservedBase: BigInt = BigInt("6000f800", 16)', scala)
        self.assertIn(
            'val outputSlotBases: Seq[BigInt] = Seq(BigInt("6000f800", 16), BigInt("6000fc00", 16))',
            scala,
        )
        self.assertIn("val outputSlotRows: Seq[Int] = Seq(3968, 4032)", scala)
        self.assertIn(
            'val productionControlAddress: BigInt = BigInt("60010000", 16)',
            scala,
        )
        self.assertIn(
            "#define GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT0_BASE UINT64_C(0x6000f800)",
            header,
        )
        self.assertIn(
            "#define GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT1_BASE UINT64_C(0x6000fc00)",
            header,
        )
        self.assertIn("#define GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT0_ROW 3968", header)
        self.assertIn("#define GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT1_ROW 4032", header)
        self.assertNotIn("OUTPUT_SLOT_BASE(index)", header)
        self.assertNotIn("OUTPUT_SLOT_ROW(index)", header)
        self.assertIn(
            "#define CGRA_TRANSFER_CONTROL_BASE UINT64_C(0x60010000)",
            control_header,
        )
        self.assertIn("#define CGRA_TRANSFER_COMPUTE_STATUS_SUCCESS 0u", control_header)
        self.assertIn(
            "#define CGRA_TRANSFER_LAUNCH_ERROR_REASON_IDENTITY_MISMATCH 3u",
            control_header,
        )
        self.assertIn(
            "#define CGRA_TRANSFER_LAUNCH_ERROR_OPERATION_PULL 3u",
            control_header,
        )
        self.assertIn(
            "#define CGRA_TRANSFER_LAUNCH_ERROR_REASON_FIELD_OUT_OF_RANGE 7u",
            control_header,
        )
        self.assertIn(
            "#define CGRA_TRANSFER_COMPUTE_ERROR_REASON_DUPLICATE_LAUNCH_RESULT 5u",
            control_header,
        )

        with tempfile.TemporaryDirectory() as directory:
            output_dir = Path(directory)
            scala_path = output_dir / "contract.scala"
            header_path = output_dir / "contract.h"
            control_header_path = output_dir / "control.h"
            write_or_check(scala_path, scala, check=False)
            write_or_check(header_path, header, check=False)
            write_or_check(control_header_path, control_header, check=False)
            first_scala = scala_path.read_bytes()
            first_header = header_path.read_bytes()
            first_control_header = control_header_path.read_bytes()
            fixed_mtime_ns = 1_000_000_000
            os.utime(scala_path, ns=(fixed_mtime_ns, fixed_mtime_ns))
            os.utime(header_path, ns=(fixed_mtime_ns, fixed_mtime_ns))
            os.utime(control_header_path, ns=(fixed_mtime_ns, fixed_mtime_ns))
            write_or_check(scala_path, scala, check=False)
            write_or_check(header_path, header, check=False)
            write_or_check(control_header_path, control_header, check=False)
            self.assertEqual(scala_path.read_bytes(), first_scala)
            self.assertEqual(header_path.read_bytes(), first_header)
            self.assertEqual(control_header_path.read_bytes(), first_control_header)
            self.assertEqual(scala_path.stat().st_mtime_ns, fixed_mtime_ns)
            self.assertEqual(header_path.stat().st_mtime_ns, fixed_mtime_ns)
            self.assertEqual(control_header_path.stat().st_mtime_ns, fixed_mtime_ns)
            write_or_check(scala_path, scala, check=True)
            write_or_check(header_path, header, check=True)
            write_or_check(control_header_path, control_header, check=True)

    def test_rejects_misaligned_or_oversized_contract(self) -> None:
        document = yaml.safe_load(DEFAULT_SOC_YAML.read_text(encoding="utf-8"))
        with tempfile.TemporaryDirectory() as directory:
            yaml_path = Path(directory) / "invalid.yaml"

            document["memory"]["gemmini_external_spad"]["base_address"] = 0x60000010
            yaml_path.write_text(yaml.safe_dump(document), encoding="utf-8")
            with self.assertRaisesRegex(
                ValueError, "base_address must be size-aligned"
            ):
                load_contract(yaml_path)

            document["memory"]["gemmini_external_spad"]["base_address"] = 0x60000000
            document["memory"]["gemmini_external_spad"]["output_slots"][
                "size_bytes"
            ] = 65536
            yaml_path.write_text(yaml.safe_dump(document), encoding="utf-8")
            with self.assertRaisesRegex(
                ValueError, "exceed the external SPAD capacity"
            ):
                load_contract(yaml_path)

    def test_custom_single_cgra_yaml_does_not_touch_production_contract(self) -> None:
        production_scala = DEFAULT_SCALA_OUT.read_bytes()
        production_header = DEFAULT_C_HEADER_OUT.read_bytes()
        production_control_header = DEFAULT_CONTROL_HEADER_OUT.read_bytes()

        with tempfile.TemporaryDirectory() as directory:
            input_dir = Path(directory)
            arch_yaml = input_dir / "arch.yaml"
            arch_yaml.write_text("architecture: {}\n", encoding="utf-8")

            custom_documents = (
                {"memory": {"num_banks_per_cgra": 1}},
                {
                    "memory": {
                        "num_banks_per_cgra": 1,
                        "gemmini_external_spad": {
                            "base_address": 0x70000000,
                            "size_bytes": 32768,
                        },
                    }
                },
            )
            for index, document in enumerate(custom_documents):
                with self.subTest(index=index):
                    soc_yaml = input_dir / f"soc-{index}.yaml"
                    soc_yaml.write_text(yaml.safe_dump(document), encoding="utf-8")
                    argv = [
                        "generate_single_cgra.py",
                        "--arch-yaml",
                        str(arch_yaml),
                        "--soc-yaml",
                        str(soc_yaml),
                    ]
                    with mock.patch.object(generate_single_cgra, "run") as run_mock:
                        with mock.patch("sys.argv", argv):
                            self.assertEqual(generate_single_cgra.main(), 0)

                    commands = [call.args[0] for call in run_mock.call_args_list]
                    self.assertEqual(len(commands), 2)
                    self.assertTrue(
                        any(
                            "cgra_rtl_generator.py" in command[1]
                            for command in commands
                        )
                    )
                    self.assertTrue(
                        any(
                            "sync_cgra_blackbox.py" in command[1]
                            for command in commands
                        )
                    )
                    self.assertFalse(
                        any(
                            "generate_gemmini_external_spad.py" in " ".join(command)
                            for command in commands
                        )
                    )

        self.assertEqual(DEFAULT_SCALA_OUT.read_bytes(), production_scala)
        self.assertEqual(DEFAULT_C_HEADER_OUT.read_bytes(), production_header)
        self.assertEqual(
            DEFAULT_CONTROL_HEADER_OUT.read_bytes(), production_control_header
        )

    def test_derived_fields_do_not_require_legacy_yaml_keys(self) -> None:
        document = yaml.safe_load(DEFAULT_SOC_YAML.read_text(encoding="utf-8"))
        external_spad = document["memory"]["gemmini_external_spad"]
        derived_keys = {
            "production_control_address",
            "control_page_size_bytes",
            "spad_row_bytes",
            "full_width_row_stride",
        }
        self.assertTrue(derived_keys.isdisjoint(external_spad))

        with tempfile.TemporaryDirectory() as directory:
            yaml_path = Path(directory) / "minimal-contract.yaml"
            yaml_path.write_text(yaml.safe_dump(document), encoding="utf-8")
            contract = load_contract(yaml_path)

        self.assertEqual(contract.production_control_address, 0x60010000)
        self.assertEqual(contract.control_page_size_bytes, 4096)
        self.assertEqual(contract.spad_row_bytes, 16)
        self.assertEqual(contract.full_width_row_stride, 4)

    def test_rejects_misaligned_derived_control_page(self) -> None:
        document = yaml.safe_load(DEFAULT_SOC_YAML.read_text(encoding="utf-8"))
        external_spad = document["memory"]["gemmini_external_spad"]
        external_spad["size_bytes"] = 2048
        external_spad["output_slots"]["size_bytes"] = 512
        with tempfile.TemporaryDirectory() as directory:
            yaml_path = Path(directory) / "invalid-control.yaml"
            yaml_path.write_text(yaml.safe_dump(document), encoding="utf-8")
            with self.assertRaisesRegex(
                ValueError, "control page must be page-aligned"
            ):
                load_contract(yaml_path)


if __name__ == "__main__":
    unittest.main()
