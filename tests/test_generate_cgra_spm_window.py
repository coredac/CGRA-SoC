import sys
import tempfile
import unittest
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from scripts import generate_cgra_spm_window as generate


class CgraSpmWindowTest(unittest.TestCase):
    def write_metadata(self, directory: Path) -> Path:
        path = directory / "CGRAGenerated.scala"
        path.write_text(
            """object CGRAGenerated {
  val params = CGRAParams(
    spmRead = CGRASpmReadParams(
      enabled = true,
      addrWidth = 7,
      dataWidth = 32,
      words = 128
    )
  )
}
""",
            encoding="utf-8",
        )
        return path

    def write_yaml(self, directory: Path, base: int, size: int) -> Path:
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
                            "base_address": base,
                            "size_bytes": size,
                        },
                    }
                }
            ),
            encoding="utf-8",
        )
        return path

    def test_contract_uses_generated_size(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            metadata = generate.load_metadata(self.write_metadata(root))
            path = self.write_yaml(root, 0x60010000, 512)
            contract = generate.load_contract(path, metadata)
        self.assertEqual(contract.base_address, 0x60010000)
        self.assertEqual(contract.size_bytes, metadata.size_bytes)
        self.assertIn("CGRASpmWindowParams(baseAddress)", generate.scala_text(contract))

    def test_size_mismatch_is_rejected(self):
        metadata = generate.CgraSpmMetadata(data_width=32, words=128)
        with tempfile.TemporaryDirectory() as directory:
            path = self.write_yaml(Path(directory), 0x60010000, 1024)
            with self.assertRaisesRegex(ValueError, "does not match generated size"):
                generate.load_contract(path, metadata)

    def test_missing_window_is_rejected(self):
        metadata = generate.CgraSpmMetadata(data_width=32, words=128)
        with tempfile.TemporaryDirectory() as directory:
            path = self.write_yaml(Path(directory), 0x60010000, 512)
            document = yaml.safe_load(path.read_text(encoding="utf-8"))
            del document["memory"]["cgra_spm_window"]
            path.write_text(yaml.safe_dump(document), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "missing mapping 'cgra_spm_window'"):
                generate.load_contract(path, metadata)

    def test_overlap_is_rejected(self):
        metadata = generate.CgraSpmMetadata(data_width=32, words=128)
        with tempfile.TemporaryDirectory() as directory:
            path = self.write_yaml(Path(directory), 0x6000FE00, 512)
            with self.assertRaisesRegex(ValueError, "SPM ranges overlap"):
                generate.load_contract(path, metadata)


if __name__ == "__main__":
    unittest.main()
