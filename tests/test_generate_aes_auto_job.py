import sys
import tempfile
import unittest
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from scripts import generate_aes_auto_job as generate


class AesAutoJobTest(unittest.TestCase):
    def document(self) -> dict:
        return {
            "memory": {
                "gemmini_external_spm": {
                    "base_address": 0x60000000,
                    "size_bytes": 0x10000,
                },
                "cgra_spm_window": {
                    "base_address": 0x60010000,
                    "size_bytes": 0x200,
                },
            },
            "accelerators": {
                "aes": {
                    "auto_job": {
                        "ciphertext_address": 0x81000000,
                        "completion_address": 0x81000080,
                        "key": 1,
                        "mode": "encrypt",
                    }
                }
            },
        }

    def load(self, document: dict) -> generate.AesAutoJobContract:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "soc.yaml"
            path.write_text(yaml.safe_dump(document), encoding="utf-8")
            return generate.load_contract(path)

    def test_contract_generates_addresses_and_key(self):
        contract = self.load(self.document())
        self.assertEqual(contract.ciphertext_address, 0x81000000)
        self.assertEqual(contract.completion_address, 0x81000080)
        self.assertIn(
            'BigInt("0000000000000000000000000000000000000000000000000000000000000001", 16)',
            generate.scala_text(contract),
        )

    def test_invalid_contracts_are_rejected(self):
        cases = {
            "missing": (lambda job: job.pop("key"), "'key' must be an integer"),
            "key": (lambda job: job.update(key=0), "nonzero 256-bit"),
            "mode": (lambda job: job.update(mode="cbc"), "mode must be 'encrypt'"),
            "alignment": (
                lambda job: job.update(ciphertext_address=0x81000020),
                "128-byte aligned",
            ),
            "overlap": (
                lambda job: job.update(completion_address=0x81000040),
                "ranges overlap",
            ),
            "window": (
                lambda job: job.update(ciphertext_address=0x60010000),
                "collides with an accelerator window",
            ),
            "control-pages": (
                lambda job: job.update(ciphertext_address=0x60013000),
                "collides with an accelerator window",
            ),
        }
        for name, (update, message) in cases.items():
            with self.subTest(name=name):
                document = self.document()
                update(document["accelerators"]["aes"]["auto_job"])
                with self.assertRaisesRegex(ValueError, message):
                    self.load(document)


if __name__ == "__main__":
    unittest.main()
