import unittest
from pathlib import Path

from scripts import sync_cgra_blackbox as sync


ROOT = Path(__file__).resolve().parents[1]
RTL = ROOT / "build" / "cgra" / "IntegratedCgraWithDmaRTL_single__pickled.v"
TOP = "IntegratedCgraWithDmaRTL_single"


class ExternalSpmReadTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text = RTL.read_text(encoding="utf-8")

    def test_metadata_and_wrapper(self):
        meta = sync.infer_metadata(self.text, RTL.name, TOP)
        self.assertEqual(meta.data_width, 35)
        self.assertTrue(meta.spm_read.enabled)
        self.assertEqual(meta.spm_read.addr_width, 32)
        self.assertEqual(meta.spm_read.data_width, 32)
        self.assertEqual(meta.spm_read.words, 128)

        wrapper = sync.gen_wrapper(meta)
        self.assertIn(
            "input  logic [31:0] recv_from_ext_spm_rd_req_addr", wrapper
        )
        self.assertIn(
            "output logic [31:0] send_to_ext_spm_rd_resp_data", wrapper
        )
        self.assertIn(
            ".recv_from_ext_spm_rd_req__msg    ( w_recv_from_ext_spm_rd_req_msg )",
            wrapper,
        )
        self.assertIn(
            ".send_to_ext_spm_rd_resp__msg     ( w_send_to_ext_spm_rd_resp_msg )",
            wrapper,
        )

    def test_group_validation(self):
        partial = self.text.replace(
            "  input logic [0:0] send_to_ext_spm_rd_resp__rdy  ,\n", "", 1
        )
        with self.assertRaisesRegex(ValueError, "partial external SPM read"):
            sync.infer_metadata(partial, RTL.name, TOP)

        wrong_direction = self.text.replace(
            "  output logic [0:0] recv_from_ext_spm_rd_req__rdy  ,",
            "  input logic [0:0] recv_from_ext_spm_rd_req__rdy  ,",
            1,
        )
        with self.assertRaisesRegex(ValueError, "expected output"):
            sync.infer_metadata(wrong_direction, RTL.name, TOP)


if __name__ == "__main__":
    unittest.main()
