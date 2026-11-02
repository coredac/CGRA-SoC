"""Check multihead caller views and root argument bindings."""

import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import gpt2_plan as plan
from kernel_graph import Graph


class Gpt2PlanTest(unittest.TestCase):
    def test_produced_view_identity(self):
        graph = Graph("views")
        source = graph.tensor("source", 3, 16, 8)
        produced = graph.tensor("produced", 3, 16, 8)
        graph.call("producer", "copy", [source], [produced])
        view = graph.view("slice", produced, 1, 4, 2, 8)
        result = graph.tensor("result", 2, 8, 8)
        graph.call("consumer", "copy", [view], [result])
        text = graph.emit([result])
        if view.type != "memref<2x8xi8, strided<[16, 1], offset: 20>>":
            self.fail("view lost its parent pitch or offset")
        if "%slice_ready = memref.subview %producer_done0" not in text:
            self.fail("view does not depend on the produced source")
        if "original_read_memrefs(%slice :" not in text:
            self.fail("logical view identity was replaced by its produced state")
        signature = next(line for line in text.splitlines() if "@views(" in line)
        if "%slice" in signature or len(graph.tensors) != 3:
            self.fail("views became root arguments")

    def test_two_head_bindings(self):
        model = {"heads": 2, "length": 2, "seed": 3}
        if plan.validate(model, 16):
            self.fail("two-head model was rejected")
        graph, output = plan.workload(model)
        text = graph.emit([output])
        if output.columns != 128:
            self.fail("hidden size did not follow the head count")
        if text.count(" = taskflow.join ") != 1:
            self.fail("head outputs were not joined")
        if "taskflow.join %concat [%h0_requant_done0, %h1_requant_done0]" not in text:
            self.fail("concat does not wait for all heads")
        for head in range(2):
            if f"taskflow.task @h{head}_scores" not in text:
                self.fail("head attention computation is missing")
        references = plan.samples(model)
        header = plan.header(graph, output, references, 16)
        if f"GPT2_ARGUMENTS = {len(graph.tensors)}" not in header:
            self.fail("header root count differs from the graph")
        bindings = re.findall(r"arguments\[(\d+)\] =", header)
        if list(map(int, bindings)) != list(range(len(graph.tensors))):
            self.fail("root bindings are not complete")
        if "gpt2_h0_q" in header or "gpt2_h1_concat" in header:
            self.fail("view storage leaked into the input fixture")
        if references[0]["expected_y"][1] == references[1]["expected_y"][1]:
            self.fail("reference samples did not vary")
        for reference in references:
            if len(reference["expected_y"][1]) != output.rows * output.columns:
                self.fail("expected output does not cover the complete block")

    def test_reference_head_constraint(self):
        model = {"heads": 2, "head_dim": 32, "length": 2, "seed": 0}
        if (
            plan.validate(model, 16)
            != "the existing native reference supports head_dim = 64"
        ):
            self.fail("unsupported reference head width was accepted")
        model["head_dim"] = 64
        model["heads"] = 0
        if plan.validate(model, 16) != "heads must be a positive integer":
            self.fail("empty head set was accepted")


if __name__ == "__main__":
    unittest.main()
