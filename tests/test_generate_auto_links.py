import sys
import tempfile
import unittest
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from scripts import generate_auto_links as generate


class AutoLinkGeneratorTest(unittest.TestCase):
    def document(self, name: str) -> dict:
        path = ROOT / "configs" / "soc" / "autolink" / f"{name}.yaml"
        return yaml.safe_load(path.read_text(encoding="utf-8"))

    def reject(self, document: dict, message: str) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "gc.yaml"
            path.write_text(yaml.safe_dump(document), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, message):
                generate.load_config(path)

    def load(self, name: str) -> tuple[generate.AutoLinkConfig, str]:
        path = ROOT / "configs" / "soc" / "autolink" / f"{name}.yaml"
        config = generate.load_config(path)
        return config, generate.scala_text(config)

    def test_gc_inference(self):
        config, scala = self.load("gc")
        self.assertEqual(config.tasks, (generate.Task("gemmini", "cgra", 128),))
        self.assertIn('AutoEndpointSpec(name = "cgra", buffer = None', scala)
        self.assertIn(
            "AutoCopySpec(route = 0, sourceOffset = 65408, destinationOffset = 0, bytes = 128)",
            scala,
        )

    def test_gca_inference(self):
        config, scala = self.load("gca")
        self.assertEqual(
            config.tasks,
            (
                generate.Task("gemmini", "cgra", 128),
                generate.Task("cgra", "aes", 128),
            ),
        )
        self.assertIn(
            'AutoEndpointSpec(name = "cgra", buffer = Some(AutoBuffer(BigInt("60010000", 16), 512)',
            scala,
        )
        self.assertIn(
            "AutoCopySpec(route = 1, sourceOffset = 0, destinationOffset = 0, bytes = 128)",
            scala,
        )

    def test_configs_share_generated_interface(self):
        _, gc_scala = self.load("gc")
        _, gca_scala = self.load("gca")
        self.assertIn("object AutoLinkGenerated", gc_scala)
        self.assertIn("object AutoLinkGenerated", gca_scala)
        self.assertNotEqual(gc_scala, gca_scala)
        self.assertEqual(generate.DEFAULT_OUTPUT.name, "AutoLinkGenerated.scala")

    def test_source_capability_is_enforced(self):
        document = self.document("gc")
        document["communication"]["auto_tasks"][0]["source"] = "aes"
        self.reject(document, "AutoLink endpoint 'aes' cannot be a source")

    def test_aes_size_is_enforced(self):
        document = self.document("gca")
        document["communication"]["auto_tasks"][1]["size_bytes"] = 64
        self.reject(document, "AES AutoLink task must be 128 bytes")

    def test_fan_in_and_fan_out_are_rejected(self):
        cases = {
            "fan-in": (
                lambda tasks: tasks[0].update(destination="aes"),
                "endpoint 'aes' has multiple incoming tasks",
            ),
            "fan-out": (
                lambda tasks: tasks[1].update(source="gemmini"),
                "endpoint 'gemmini' has multiple outgoing tasks",
            ),
        }
        for name, (update, message) in cases.items():
            with self.subTest(name=name):
                document = self.document("gca")
                update(document["communication"]["auto_tasks"])
                self.reject(document, message)

    def test_cgra_source_requires_input(self):
        document = self.document("gca")
        document["communication"]["auto_tasks"] = document["communication"][
            "auto_tasks"
        ][1:]
        self.reject(document, "CGRA source task requires an incoming CGRA task")


if __name__ == "__main__":
    unittest.main()
