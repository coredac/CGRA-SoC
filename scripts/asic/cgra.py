#!/usr/bin/env python3
"""Run matched kernels on one archived, unchanged Verilated CGRA instance."""

import argparse
import ast
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT), str(ROOT / "scripts")]

from cffi import FFI
from pymtl3 import mk_bits
import block_data
import gcn_data
import gpt2_data
import p1_resnet_data
from VectorCGRA.cgra.test import CgraRTL_attention_test_from_yaml as attention
from VectorCGRA.cgra.test import CgraRTL_block_test_from_yaml as block
from VectorCGRA.cgra.test import CgraRTL_gcn_test_from_yaml as gcn
from VectorCGRA.lib.cmd_type import (
    CMD_COMPLETE,
    CMD_CONFIG_CTRL_LOWER_BOUND,
    CMD_LAUNCH,
    CMD_LOAD_REQUEST,
    CMD_LOAD_RESPONSE,
    CMD_REARM,
    CMD_STORE_REQUEST,
)

ARCHIVE = ROOT / "build/experiments/p1/gpt2/cgra1/v002/reference"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class Fabric:
    def __init__(self, archive):
        tree = ast.parse((archive / "AttentionCgra_v.py").read_text())
        declaration = next(
            node.args[0].value
            for node in ast.walk(tree)
            if isinstance(node, ast.Call)
            and isinstance(node.func, ast.Attribute)
            and node.func.attr == "cdef"
        )
        self.ffi = FFI()
        self.ffi.cdef(declaration)
        self.lib = self.ffi.dlopen(str(archive / "libAttentionCgra_v.so"))
        self.model = self.lib.create_model(self.ffi.new("char[]", b""))
        self.cycle = 0
        self.model.address_lower[0] = 0
        self.model.address_upper[0] = 32767
        self.model.cgra_id[0] = 0
        self.model.recv_from_cpu_pkt___05Fval[0] = 0
        self.model.recv_from_inter_cgra_noc___05Fval[0] = 0
        self.model.send_to_cpu_pkt___05Frdy[0] = 1
        self.model.send_to_inter_cgra_noc___05Frdy[0] = 0
        for direction in ("north", "south", "east", "west"):
            for index in range(4):
                getattr(self.model, f"recv_data_on_boundary_{direction}___05Fval")[
                    index
                ][0] = 0
                getattr(self.model, f"send_data_on_boundary_{direction}___05Frdy")[
                    index
                ][0] = 0
        self.model.reset[0] = 1
        for _ in range(5):
            self.tick()
        self.model.reset[0] = 0
        self.tick()

    def tick(self, packet=None):
        self.model.recv_from_cpu_pkt___05Fval[0] = packet is not None
        if packet is not None:
            value = int(packet.to_bits())
            for index in range(7):
                self.model.recv_from_cpu_pkt___05Fmsg[index] = (
                    value >> (index * 32)
                ) & 0xFFFFFFFF
        self.lib.comb_eval(self.model)
        accepted = packet is not None and bool(self.model.recv_from_cpu_pkt___05Frdy[0])
        response = None
        if self.model.send_to_cpu_pkt___05Fval[0]:
            value = sum(
                int(self.model.send_to_cpu_pkt___05Fmsg[index]) << (index * 32)
                for index in range(7)
            )
            response = attention.Packet.from_bits(mk_bits(198)(value))
        self.lib.seq_eval(self.model)
        self.cycle += 1
        return accepted, response

    def send(self, packets):
        for packet in packets:
            for _ in range(100000):
                accepted, response = self.tick(packet)
                if response is not None:
                    raise RuntimeError(
                        f"Unexpected response during configuration: {response}"
                    )
                if accepted:
                    break
            else:
                raise RuntimeError("CGRA command input stalled")

    def job(self, name, packets, inputs, expected, completion_count=None):
        self.send(
            [
                attention.Packet(dst=tile, payload=attention.Payload(CMD_REARM))
                for tile in range(16)
            ]
        )
        self.send(
            [
                attention.memory_packet(CMD_STORE_REQUEST, address, value)
                for address, value in inputs.items()
            ]
        )
        launch = [packet for packet in packets if int(packet.payload.cmd) == CMD_LAUNCH]
        config = [packet for packet in packets if int(packet.payload.cmd) != CMD_LAUNCH]
        targets = {int(packet.dst) for packet in launch}
        # Full kernel replacement starts at slot zero; resident phases override it.
        self.send(
            [
                attention.Packet(
                    dst=tile,
                    payload=attention.Payload(
                        CMD_CONFIG_CTRL_LOWER_BOUND,
                        data=attention.common.DataType(0, 1),
                    ),
                )
                for tile in sorted(targets)
            ]
        )
        self.send(config)
        count = len(targets) if completion_count is None else completion_count
        completed = set()
        sent, started = 0, None
        for _ in range(50000):
            packet = launch[sent] if sent < len(launch) else None
            accepted, response = self.tick(packet)
            if accepted:
                if started is None:
                    started = self.cycle - 1
                sent += 1
            if response is not None:
                target = int(response.src)
                if (
                    int(response.payload.cmd) != CMD_COMPLETE
                    or target not in targets
                    or target in completed
                ):
                    raise RuntimeError(f"Unexpected completion: {response}")
                completed.add(target)
            if len(completed) == count and sent == len(launch):
                break
        else:
            raise RuntimeError(f"CGRA timeout {name}: {completed}/{targets}")
        cycles = self.cycle - started
        queries = [
            attention.memory_packet(CMD_LOAD_REQUEST, address) for address in expected
        ]
        sent, received = 0, set()
        for _ in range(1000000):
            packet = queries[sent] if sent < len(queries) else None
            accepted, response = self.tick(packet)
            sent += int(accepted)
            if response is not None:
                address = int(response.payload.data_addr)
                value = int(response.payload.data.payload)
                if (
                    int(response.payload.cmd) != CMD_LOAD_RESPONSE
                    or address in received
                    or address not in expected
                ):
                    raise RuntimeError(f"Unexpected verification read: {response}")
                if value != expected[address] & 0xFFFFFFFF:
                    raise RuntimeError(
                        f"{name} word{address}: got {value}, expected {expected[address] & 0xFFFFFFFF}"
                    )
                received.add(address)
            if len(received) == len(expected):
                break
        else:
            raise RuntimeError(f"CGRA readback timeout {name}")
        result = dict(
            name=name,
            passed=True,
            cycles=cycles,
            logical_kernel_starts=1,
            launch_packets=len(launch),
            completions=len(completed),
            config_packets=len(config) + len(targets),
            preload_payload_bytes=4 * len(inputs),
            verification_read_bytes=4 * len(expected),
            checked_outputs=len(expected),
        )
        print(json.dumps(result), flush=True)
        return result


def sparse(fabric):
    results = []
    graph, weights = gcn_data.graphs(0)[0], gcn_data.weights(0)
    for run, sign in enumerate((1, -1)):
        tensors, _ = gcn_data.reference(graph, weights, sign=sign)
        values = {key: data[1] for key, data in tensors.items()}
        initial = dict(enumerate(values["expected_z"]))
        initial.update({4096 + i: v for i, v in enumerate(values["col"])})
        initial.update({4352 + i: v for i, v in enumerate(values["edge_weight"])})
        for row in range(16):
            first, last = values["rowptr"][row : row + 2]
            bindings = dict(
                arg0=0,
                arg1=4096 + first,
                arg2=4352 + first,
                arg3=8192,
                arg4=last - first,
                arg5=0,
                arg7=128,
            )
            packets = gcn.packets(bindings, 128 * (last - first))
            expected = {
                8192 + i: v
                for i, v in enumerate(values["expected_y"][row * 128 : (row + 1) * 128])
            }
            results.append(
                fabric.job(
                    f"gcn.run{run}.row{row}",
                    packets,
                    initial if row == 0 else {},
                    expected,
                )
            )
    return results


def resnet(fabric):
    import importlib.util

    source = ROOT / "build/experiments/p1/resnet/cgra1/v001/reference/source.py"
    spec = importlib.util.spec_from_file_location(
        "VectorCGRA.cgra.test.asic_resident", source
    )
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    kernels = ROOT / "build/experiments/p1/resnet/cgra1/v002/config/kernels/runtime"
    fixture = p1_resnet_data.reference("small")
    results = []
    for run, sign in enumerate((1, -1)):
        x = [value * sign for value in fixture["p1_input"]]
        first = block_data.conv(x, fixture["p1_weights1"], 8, 8, 4, 8, 3, 2, 1, False)
        relu = [max(0, value) for value in first]
        main = block_data.conv(relu, fixture["p1_weights2"], 4, 4, 8, 8, 3, 1, 1)
        skip = block_data.conv(x, fixture["p1_projection"], 8, 8, 4, 8, 1, 2, 0)
        for name, bindings, inputs, output in (
            ("relu", dict(arg0=0, arg1=2048, arg2=128), dict(enumerate(first)), relu),
            (
                "add_relu",
                dict(arg0=0, arg1=1024, arg2=2048, arg3=128),
                dict(enumerate(main)) | {1024 + i: v for i, v in enumerate(skip)},
                [max(0, a + b) for a, b in zip(main, skip)],
            ),
        ):
            static, runtime, _ = helper.make_job(
                name, 0, bindings, kernels, attention.Packet, ii=5, steps=650
            )
            expected = {2048 + i: v for i, v in enumerate(output)}
            results.append(
                fabric.job(
                    f"resnet.run{run}.{name}",
                    static + runtime,
                    inputs,
                    expected,
                    completion_count=6 if name == "relu" else 11,
                )
            )
    return results


def transformer(fabric):
    results = []
    for run in range(2):
        tensors, _ = gpt2_data.reference(heads=2, length=2, seed=0, run=run)
        values = {key: data[1] for key, data in tensors.items()}
        for layer, name in ((1, "x"), (2, "expected_u")):
            x, gain, bias, out = 0, 256, 384, 1024
            total, mean, squares, variance, std = 2048, 2064, 2080, 2096, 2112
            rows = [values[name][i * 128 : (i + 1) * 128] for i in range(2)]
            gamma, beta = values[f"gamma{layer}"], values[f"beta{layer}"]
            reference = [block.norm_reference(row, gamma, beta) for row in rows]
            stats = [entry[0] for entry in reference]
            normalized = [value for entry in reference for value in entry[1]]
            inputs = (
                dict(enumerate(values[name]))
                | {gain + i: v for i, v in enumerate(gamma)}
                | {bias + i: v for i, v in enumerate(beta)}
            )
            phases = [
                (
                    "ln_sum",
                    [x, total, 128],
                    256,
                    {total + i: v[0] for i, v in enumerate(stats)},
                ),
                (
                    "ln_mean",
                    [total, mean, 128],
                    2,
                    {mean + i: v[1] for i, v in enumerate(stats)},
                ),
                (
                    "ln_squares",
                    [x, mean, squares, 128],
                    256,
                    {squares + i: v[2] for i, v in enumerate(stats)},
                ),
                (
                    "ln_variance",
                    [squares, variance, 128],
                    2,
                    {variance + i: v[3] for i, v in enumerate(stats)},
                ),
                (
                    "ln_sqrt",
                    [variance, std],
                    2,
                    {std + i: v[4] for i, v in enumerate(stats)},
                ),
                (
                    "ln_norm",
                    [x, gain, bias, out, mean, std, 128],
                    256,
                    {out + i: v for i, v in enumerate(normalized)},
                ),
                (
                    "requant",
                    [out, out, 4],
                    256,
                    {out + i: v for i, v in enumerate(values[f"expected_ln{layer}"])},
                ),
            ]
            for index, (phase, args, count, expected) in enumerate(phases):
                packets = block.packets(
                    phase, {f"arg{i}": v for i, v in enumerate(args)}, count
                )
                results.append(
                    fabric.job(
                        f"gpt.run{run}.ln{layer}.{phase}",
                        packets,
                        inputs if index == 0 else {},
                        expected,
                    )
                )
        for head in range(2):
            scores = [
                values["scores"][head * 4 + row * 2 : head * 4 + row * 2 + 2]
                for row in range(2)
            ]
            maxima, exps, sums, output = attention.reference(scores, 0)
            bindings = [
                dict(arg0=0, arg1=1024, arg2=2, arg3=0),
                dict(arg0=0, arg1=1024, arg2=2048, arg3=3072, arg4=2, arg5=0),
                dict(arg0=2048, arg1=3072, arg2=4096, arg3=2),
            ]
            expected = [
                {1024 + i: v for i, v in enumerate(maxima)},
                {2048 + i: v for i, v in enumerate(exps)}
                | {3072 + i: v for i, v in enumerate(sums)},
                {
                    4096 + i: v
                    for i, v in enumerate(
                        values["probability"][head * 4 : head * 4 + 4]
                    )
                },
            ]
            for index, (phase, args) in enumerate(zip(attention.PHASES, bindings)):
                static, runtime, _ = attention.make_phase(*phase, args, 4)
                inputs = dict(enumerate(sum(scores, []))) if index == 0 else {}
                results.append(
                    fabric.job(
                        f"gpt.run{run}.head{head}.{phase[0]}",
                        static + runtime,
                        inputs,
                        expected[index],
                    )
                )
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=ROOT / "build/experiments/asic/cgra"
    )
    parser.add_argument(
        "--workload", choices=("all", "gcn", "resnet", "gpt"), default="all"
    )
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    fabric = Fabric(ARCHIVE)
    results = []
    cases = {"gcn": sparse, "resnet": resnet, "gpt": transformer}
    try:
        for name, run in cases.items():
            if args.workload in ("all", name):
                results.extend(run(fabric))
        report = dict(
            passed=True,
            hardware="One frozen 4x4 Attention CGRA, 48 slots, 32768x32b synchronous SPM, 8 banks",
            cycle_scope="First per-PE launch accepted through last completion accepted; excludes preload/config/verification. Not SoC CPU cycles.",
            transfer_scope="32-bit test-port payload only; not DRAM traffic. Internal SPM traffic is not instrumented.",
            results=results,
            rtl_sha256=digest(ARCHIVE / "AttentionCgra__pickled.v"),
            library_sha256=digest(ARCHIVE / "libAttentionCgra_v.so"),
        )
        (args.output / f"{args.workload}.json").write_text(
            json.dumps(report, indent=2) + "\n"
        )
        print(f"PASS {len(results)} matched kernel jobs on the same frozen hardware")
    finally:
        fabric.lib.destroy_model(fabric.model)


if __name__ == "__main__":
    main()
