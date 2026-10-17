"""Protect synthetic graph structure, padding and normalized sparse arithmetic."""

import math
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import gcn_data as data


class GcnTest(unittest.TestCase):
    def test_graph_structure_and_seed(self):
        graphs = data.graphs(0)
        if graphs != data.graphs(0) or graphs == data.graphs(1):
            self.fail("Graph generation must be deterministic and depend on the seed")
        if [len(graph["neighbors"]) for graph in graphs] != [16, 20, 32]:
            self.fail("Synthetic graph shapes changed")
        for graph in graphs:
            neighbors = graph["neighbors"]
            for node, adjacent in enumerate(neighbors):
                if node in adjacent or any(
                    node not in neighbors[other] for other in adjacent
                ):
                    self.fail(
                        "Generated adjacency must be undirected without self-loops"
                    )
        if len(graphs[1]["neighbors"][0]) != 19 or graphs[2]["neighbors"][-1]:
            self.fail("Fixtures must exercise the degree-20 split and an isolated node")

    def test_sparse_reference_matches_dense(self):
        graph = dict(id=7, name="path-isolated", neighbors=[{1}, {0, 2}, {1}, set()])
        weight = data.weights(3)
        tensors, report = data.reference(graph, weight)
        source = tensors["x"][1]
        projected = tensors["expected_z"][1]
        for row in range(report["rows"]):
            for feature in range(data.OUTPUTS):
                dense = sum(
                    source[row * data.INPUTS + k] * weight[k * data.OUTPUTS + feature]
                    for k in range(data.INPUTS)
                )
                if projected[row * data.OUTPUTS + feature] != dense:
                    self.fail("One-hot shortcut does not equal the dense hardware GEMM")
        adjacency = [[1, 1, 0, 0], [1, 1, 1, 0], [0, 1, 1, 0], [0, 0, 0, 1]]
        degrees = list(map(sum, adjacency))
        expected_sum = []
        for row in range(4):
            for feature in range(data.OUTPUTS):
                value = sum(
                    math.floor(
                        data.SCALE / math.sqrt(degrees[row] * degrees[col]) + 0.5
                    )
                    * projected[col * data.OUTPUTS + feature]
                    for col in range(4)
                    if adjacency[row][col]
                )
                expected_sum.append(value)
        if tensors["expected_sum"][1] != expected_sum:
            self.fail("Sparse aggregation differs from dense normalized reference")
        if tensors["expected_y"][1] != [max(value, 0) for value in expected_sum]:
            self.fail("ReLU must follow the full reduction without downshift")
        if (
            tensors["rowptr"][1] != [0, 2, 5, 7, 8]
            or tensors["edge_weight"][1][-1] != data.SCALE
        ):
            self.fail("Self-loop insertion or isolated-node normalization is incorrect")
        if report["float_error"]["max_abs"] > 3 * 8 / (2 * data.SCALE):
            self.fail("Q15 error exceeds the per-edge nearest-rounding bound")
        if report["accum_max_abs"] >= 2**31:
            self.fail("Reference accumulation does not fit signed INT32")

    def test_fixture_header_identity(self):
        graphs = [
            dict(id=1, name="pair", neighbors=[{1}, {0}]),
            dict(id=2, name="isolated", neighbors=[set()]),
        ]
        weight = data.weights(0)
        fixtures = [data.reference(graph, weight) for graph in graphs]
        result = "".join(data.header(weight, fixtures))
        if (
            '#define GCN_DATASET "synthetic-gcn"' not in result
            or "GCN_GRAPHS = 2" not in result
        ):
            self.fail(
                "Generated fixture must retain its dataset identity and graph count"
            )
        if weight != data.weights(0) or weight == data.weights(1):
            self.fail("Synthetic model weights must be reproducible from the seed")
        if "gcn_1_expected_y" not in result or "gcn_graphs[GCN_GRAPHS]" not in result:
            self.fail("Generated descriptor does not include every complete graph")
        if (
            "gcn_1_col[4]" not in result
            or "gcn_1_edge_weight[4]" not in result
            or "32768, 0, 0, 0," not in result
        ):
            self.fail("Raw DMA sources must include zero-filled 16-byte tail padding")


if __name__ == "__main__":
    unittest.main()
