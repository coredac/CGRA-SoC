"""Check the dedicated endpoint's generated memory and job contract."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from generate_accels import load_layout, scala_text as accel_scala
from generate_auto_links import load_config, scala_text

ROOT = Path(__file__).resolve().parents[2]
SOC = ROOT / "configs/soc/autolink/asic.yaml"


class AsicTest(unittest.TestCase):
    def test_split_contract(self):
        path = ROOT / "configs/soc/autolink/split.yaml"
        layout = load_layout(path)
        if layout.asic_functions != {"add0": "add", "relu0": "relu"}:
            self.fail("Split endpoints must be fixed Add and ReLU hardware")
        devices = {item.name: item for item in layout.instances}
        if devices["add0"].size + devices["relu0"].size != devices["cgra0"].size:
            self.fail("Dedicated total SPM differs from CGRA SPM")
        if devices["add0"].base + devices["add0"].size > devices["relu0"].base:
            self.fail("Fixed ASIC memories overlap")
        text = accel_scala(layout)
        if '"add0" -> "add"' not in text or '"relu0" -> "relu"' not in text:
            self.fail("Generated hardware function selection is missing")
        config = load_config(path)
        for name in ("add0", "relu0"):
            if config.jobs[name] != 4:
                self.fail("Fixed endpoints need the configured descriptor capacity")
        if "localBytes = 65536" not in scala_text(config):
            self.fail("Endpoint publication does not match its physical memory")

    def test_system_contract(self):
        layout = load_layout(SOC)
        if layout.asic_functions != {"asic0": "all"}:
            self.fail("The existing programmable ASIC baseline changed")
        asic = next(item for item in layout.instances if item.kind == "asic")
        if (asic.name, asic.base, asic.size) != ("asic0", 0x60080000, 131072):
            self.fail(f"Unexpected dedicated memory: {asic}")
        if layout.control_base < asic.base + asic.size:
            self.fail("Control registers overlap dedicated memory")
        config = load_config(SOC)
        if config.jobs["asic0"] != 32:
            self.fail("Dedicated job cache must hold 32 descriptors")
        text = scala_text(config)
        endpoint = next(line for line in text.splitlines() if 'name = "asic0"' in line)
        if (
            "bufferedInput = true" not in endpoint
            or "releaseOnCopy = true" not in endpoint
        ):
            self.fail("Dedicated endpoint is not a buffered copy consumer")
        if 'AutoStageSpec(name = "dedicated", endpoint = "asic0"' not in text:
            self.fail("Default graph does not reach the dedicated endpoint")


if __name__ == "__main__":
    unittest.main()
