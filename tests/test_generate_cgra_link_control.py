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


if __name__ == "__main__":
    unittest.main()
