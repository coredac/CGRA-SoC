import sys
import tempfile
import unittest
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from scripts import generate_cgra_link_control as generate


class CgraLinkControlTest(unittest.TestCase):
    def write_yaml(self, directory: Path, cgra_base: int, cgra_size: int) -> Path:
        path = directory / "soc.yaml"
        path.write_text(
            yaml.safe_dump(
                {
                    "memory": {
                        "gemmini_external_spm": {
                            "base_address": 0x60000000,
                            "size_bytes": 0x10000,
                        },
                        "cgra_spm_window": {
                            "base_address": cgra_base,
                            "size_bytes": cgra_size,
                        },
                    }
                }
            ),
            encoding="utf-8",
        )
        return path

    def test_control_follows_all_spm_windows(self):
        with tempfile.TemporaryDirectory() as directory:
            path = self.write_yaml(Path(directory), 0x60010000, 0x200)
            address = generate.control_address(path)
        self.assertEqual(address, 0x60011000)

    def test_control_uses_highest_spm_end(self):
        with tempfile.TemporaryDirectory() as directory:
            path = self.write_yaml(Path(directory), 0x5FFF0000, 0x200)
            address = generate.control_address(path)
        self.assertEqual(address, 0x60010000)

    def test_generated_job_abi(self):
        scala = generate.scala_text(0x60011000)
        header = generate.header_text(0x60011000)
        self.assertIn('gemminiJobAddress: BigInt = BigInt("60012000", 16)', scala)
        self.assertIn('aesJobAddress: BigInt = BigInt("60013000", 16)', scala)
        self.assertIn("#define GEMMINI_JOB_BASE UINT64_C(0x60012000)", header)
        self.assertIn("#define AES_JOB_BASE UINT64_C(0x60013000)", header)
        for name, offset in generate.GEMMINI_REGISTERS.items():
            self.assertIn(f"val GEMMINI_{name}: Int = 0x{offset:03x}", scala)
            self.assertIn(f"#define GEMMINI_JOB_{name} 0x{offset:03x}u", header)
        for name, offset in generate.AES_REGISTERS.items():
            self.assertIn(f"val AES_{name}: Int = 0x{offset:03x}", scala)
            self.assertIn(f"#define AES_JOB_{name} 0x{offset:03x}u", header)


if __name__ == "__main__":
    unittest.main()
