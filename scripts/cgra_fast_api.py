#!/usr/bin/env python3
"""
Generate fast-only local single-CGRA packet APIs from per-kernel YAML configs.
"""

from __future__ import annotations

import argparse
import contextlib
import copy
import glob
import io
import re
import sys
from dataclasses import dataclass, replace
from pathlib import Path
from typing import Mapping, Sequence

SCRIPT_DIR = Path(__file__).resolve().parent
ROOT = SCRIPT_DIR.parent
VECTOR_ROOT = ROOT / "VectorCGRA"
DEFAULT_ARCH_YAML = ROOT / "configs" / "arch" / "arch.yaml"
DEFAULT_SOC_YAML = ROOT / "configs" / "soc" / "cgra_soc.yaml"
DEFAULT_OUTPUT_DIR = ROOT / "tests" / "generated"
DEFAULT_CONFIGS = (
    ROOT / "configs" / "kernels" / "kernel_fir4x4_4x4.yaml",
    ROOT / "configs" / "kernels" / "kernel_relu4x4_4x4.yaml",
    ROOT / "configs" / "kernels" / "kernel_gemv_4x4.yaml",
    ROOT / "configs" / "kernels" / "kernel_histogram_4x4.yaml",
    ROOT / "configs" / "kernels" / "kernel_axpy_4x4.yaml",
)
SUPPORTED_CONFIGS = DEFAULT_CONFIGS + (
    ROOT / "configs" / "kernels" / "kernel_add_relu_4x4.yaml",
    ROOT / "configs" / "kernels" / "kernel_relu_tail_4x4.yaml",
    ROOT / "configs" / "kernels" / "kernel_relu_runtime_4x4.yaml",
    ROOT / "configs" / "kernels" / "kernel_add_relu_runtime_4x4.yaml",
)
SUPPORTED_CONFIG_NAMES = {path.name for path in SUPPORTED_CONFIGS}
SUPPORTED_KERNEL_NAMES = {
    "fir4x4",
    "relu4x4",
    "gemv",
    "histogram",
    "axpy",
    "add_relu",
    "relu_tail",
    "relu_runtime",
    "add_relu_runtime",
}

for path in (SCRIPT_DIR, ROOT, VECTOR_ROOT):
    if str(path) not in sys.path:
        sys.path.insert(0, str(path))
python_tag = f"python{sys.version_info.major}.{sys.version_info.minor}"
for site_packages in glob.glob(
    str(ROOT / ".venv" / "lib" / "python*" / "site-packages")
):
    if site_packages not in sys.path:
        sys.path.insert(0, site_packages)
for site_packages in (
    str(Path(sys.prefix) / "lib" / python_tag / "site-packages"),
    str(Path(sys.base_prefix) / "lib" / python_tag / "site-packages"),
):
    if site_packages not in sys.path:
        sys.path.append(site_packages)

from pymtl3 import b1, b2, clog2, mk_bits  # noqa: E402
from util.schema import (  # noqa: E402
    load_yaml_mapping,
    require_int,
    require_mapping,
    require_string as require_str,
)

from VectorCGRA.lib.cmd_type import (
    CMD_CONFIG,
    CMD_CONFIG_COUNT_PER_ITER,
    CMD_CONFIG_CTRL_LOWER_BOUND,
    CMD_CONFIG_PROLOGUE_FU,
    CMD_CONFIG_PROLOGUE_FU_CROSSBAR,
    CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR,
    CMD_CONFIG_TOTAL_CTRL_COUNT,
    CMD_CONST,
    CMD_LAUNCH,
    CMD_LOAD_REQUEST,
    CMD_REARM,
    CMD_STORE_REQUEST,
)
from VectorCGRA.lib.messages import (  # noqa: E402
    mk_cgra_payload,
    mk_ctrl,
    mk_data,
    mk_intra_cgra_pkt,
)
from VectorCGRA.cgra.test.CgraTemplateRTL_single_test import (
    load_soc_config,
)  # noqa: E402
from VectorCGRA.multi_cgra.arch_parser.ArchParser import ArchParser  # noqa: E402
from VectorCGRA.validation.script_generator import ScriptFactory  # noqa: E402

_MASK64 = (1 << 64) - 1


@dataclass(frozen=True)
class RuntimeSymbol:
    symbol: str
    kind: str
    role: str


@dataclass(frozen=True)
class RuntimeCount:
    symbol: str
    scale: int
    offset: int


@dataclass(frozen=True)
class Relocation:
    packet_index: int
    symbol_index: int
    scale: int
    offset: int


@dataclass(frozen=True)
class KernelConfig:
    name: str
    source_path: Path
    kernel_yaml: Path
    bindings: Mapping[str, int]
    x_tiles: int
    y_tiles: int
    num_tiles: int
    tile_targets: tuple[tuple[int, int, int], ...]
    num_tile_inports: int
    num_tile_outports: int
    num_fu_inports: int
    num_fu_outports: int
    data_nbits: int
    predicate_nbits: int
    data_mem_size_global: int
    data_mem_size_per_bank: int
    num_banks_per_cgra: int
    num_registers_per_reg_bank: int
    config_mem_size: int
    num_cgra_columns: int
    num_cgra_rows: int
    compiled_ii: int
    ctrl_base: int
    loop_times: int
    expected_completes: int
    runtime_symbols: tuple[RuntimeSymbol, ...]
    runtime_count: RuntimeCount | None


@dataclass(frozen=True)
class EncodedPacket:
    coord: tuple[int, int]
    tile: int
    cmd: int
    lo: int
    mid: int
    hi: int
    top: int
    is_launch: bool


@dataclass(frozen=True)
class BitSegment:
    chunk: int
    value_lsb: int
    chunk_lsb: int
    width: int


@dataclass(frozen=True)
class BasicPacketTemplate:
    cmd: int
    lo: int
    mid: int
    hi: int
    top: int
    data_payload_segments: tuple[BitSegment, ...]
    data_addr_segments: tuple[BitSegment, ...]


def resolve_input_path(path: str | Path, base: Path | None = None) -> Path:
    candidate = Path(path)
    if candidate.is_absolute():
        return candidate
    search_roots = [p for p in (base, Path.cwd(), ROOT, VECTOR_ROOT) if p]
    for root in search_roots:
        resolved = root / candidate
        if resolved.exists():
            return resolved.resolve()
    return ((base or ROOT) / candidate).resolve()


def rel_to_root(path: Path) -> str:
    try:
        return path.resolve().relative_to(ROOT).as_posix()
    except ValueError:
        return path.as_posix()


def load_runtime(data: Mapping, path: Path, bindings: Mapping):
    if "runtime" not in data:
        return (), None
    runtime = require_mapping(data, "runtime", path)
    entries = runtime.get("symbols")
    if not isinstance(entries, list) or not entries:
        raise ValueError(f"{path}: runtime.symbols must be a non-empty list")
    symbols = tuple(
        RuntimeSymbol(
            *(require_str(entry, key, path) for key in ("symbol", "kind", "role"))
        )
        for entry in entries
    )
    if len({item.symbol for item in symbols}) != len(symbols) or {
        item.symbol for item in symbols
    } != set(bindings):
        raise ValueError(f"{path}: runtime symbols must name each binding exactly once")
    roles = set()
    for item in symbols:
        input_role = re.fullmatch(r"input[0-9]+", item.role)
        if item.kind not in ("spm_word", "count") or (
            item.role not in ("output", "elements") and not input_role
        ):
            raise ValueError(f"{path}: unsupported runtime symbol kind or role")
        if (item.kind == "count") != (item.role == "elements") or item.role in roles:
            raise ValueError(
                f"{path}: runtime roles must be unique and elements must be a count"
            )
        roles.add(item.role)
    count_data = require_mapping(
        require_mapping(data, "execution", path), "runtime_count", path
    )
    count = RuntimeCount(
        require_str(count_data, "symbol", path),
        require_int(count_data, "scale", path),
        require_int(count_data, "offset", path),
    )
    if [item.symbol for item in symbols if item.kind == "count"] != [count.symbol]:
        raise ValueError(
            f"{path}: execution.runtime_count must name the elements symbol"
        )
    if bindings[count.symbol] <= 0 or count.scale <= 0 or count.offset < 0:
        raise ValueError(
            f"{path}: runtime count and scale must be positive, offset nonnegative"
        )
    return symbols, count


def load_kernel_config(path: Path, arch_yaml: Path, soc_yaml: Path) -> KernelConfig:
    data = load_yaml_mapping(path)
    kernel = require_mapping(data, "kernel", path)
    execution = require_mapping(data, "execution", path)

    name = require_str(kernel, "name", path)
    if name not in SUPPORTED_KERNEL_NAMES or path.name not in SUPPORTED_CONFIG_NAMES:
        supported = ", ".join(sorted(SUPPORTED_CONFIG_NAMES))
        raise ValueError(
            f"{path}: unsupported single-CGRA fast kernel '{name}'. "
            f"Supported configs are: {supported}"
        )

    kernel_yaml = resolve_input_path(
        require_str(kernel, "kernel_yaml", path), path.parent
    )
    if not kernel_yaml.exists():
        raise FileNotFoundError(f"{path}: kernel_yaml does not exist: {kernel_yaml}")

    arch_parser = ArchParser(str(arch_yaml))
    param_cgra = arch_parser.get_simplest_cgra_param()
    soc_cfg = load_soc_config(soc_yaml)
    required_words = require_int(kernel, "required_words", path, default=0)
    bindings = require_mapping(kernel, "bindings", path, default={})
    for symbol, value in bindings.items():
        if not isinstance(symbol, str) or not symbol:
            raise ValueError(f"{path}: binding names must be non-empty strings")
        if type(value) is not int:
            raise TypeError(f"{path}: binding '{symbol}' must be an integer")
    local_words = soc_cfg.data_mem_size_per_bank * soc_cfg.num_banks_per_cgra
    if local_words < required_words:
        raise ValueError(
            f"{path}: kernel '{name}' requires {required_words} local words, "
            f"but the selected SoC provides {local_words}"
        )
    runtime_symbols, runtime_count = load_runtime(data, path, bindings)
    scalar_symbols = {item.symbol for item in runtime_symbols if item.kind == "count"}
    for symbol, value in bindings.items():
        limit = 1 << soc_cfg.data_nbits if symbol in scalar_symbols else local_words
        if value < 0 or value >= limit:
            raise ValueError(f"{path}: binding '{symbol}' must be in [0, {limit})")
    expected_completes = require_int(execution, "expected_completes", path)
    loop_times = require_int(execution, "loop_times", path)
    if runtime_count is not None and not 0 < loop_times < (1 << soc_cfg.data_nbits):
        raise ValueError(f"{path}: runtime execution count must fit the data payload")
    if (
        runtime_count is not None
        and loop_times
        != runtime_count.scale * bindings[runtime_count.symbol] + runtime_count.offset
    ):
        raise ValueError(
            f"{path}: loop_times must match the initial runtime count formula"
        )

    return KernelConfig(
        name=name,
        source_path=path,
        kernel_yaml=kernel_yaml,
        bindings=dict(bindings),
        x_tiles=param_cgra.columns,
        y_tiles=param_cgra.rows,
        num_tiles=len(param_cgra.getValidTiles()),
        tile_targets=tuple(
            (index, tile.dimX, tile.dimY)
            for index, tile in enumerate(param_cgra.getValidTiles())
            if not tile.disabled
        ),
        num_cgra_columns=arch_parser.cgra_columns,
        num_cgra_rows=arch_parser.cgra_rows,
        config_mem_size=param_cgra.configMemSize,
        num_tile_inports=soc_cfg.num_tile_inports,
        num_tile_outports=soc_cfg.num_tile_outports,
        num_fu_inports=soc_cfg.num_fu_inports,
        num_fu_outports=soc_cfg.num_fu_outports,
        data_nbits=soc_cfg.data_nbits,
        predicate_nbits=soc_cfg.predicate_nbits,
        data_mem_size_global=soc_cfg.data_mem_size_global,
        data_mem_size_per_bank=soc_cfg.data_mem_size_per_bank,
        num_banks_per_cgra=soc_cfg.num_banks_per_cgra,
        num_registers_per_reg_bank=soc_cfg.num_registers_per_reg_bank,
        compiled_ii=require_int(execution, "compiled_ii", path),
        ctrl_base=require_int(execution, "ctrl_base", path, default=0),
        loop_times=loop_times,
        expected_completes=expected_completes,
        runtime_symbols=runtime_symbols,
        runtime_count=runtime_count,
    )


def build_packet_types(cfg: KernelConfig) -> Mapping[str, object]:
    DataType = mk_data(cfg.data_nbits, cfg.predicate_nbits)
    DataAddrType = mk_bits(clog2(cfg.data_mem_size_global))
    CtrlType = mk_ctrl(
        cfg.num_fu_inports,
        cfg.num_fu_outports,
        cfg.num_tile_inports,
        cfg.num_tile_outports,
        cfg.num_registers_per_reg_bank,
    )
    CtrlAddrType = mk_bits(clog2(cfg.config_mem_size))
    CgraPayloadType = mk_cgra_payload(DataType, DataAddrType, CtrlType, CtrlAddrType)
    IntraCgraPktType = mk_intra_cgra_pkt(
        cfg.num_cgra_columns, cfg.num_cgra_rows, cfg.num_tiles, CgraPayloadType
    )
    TileInType = mk_bits(clog2(cfg.num_tile_inports + cfg.num_fu_inports + 1))
    FuInType = mk_bits(clog2(cfg.num_fu_inports + 1))
    FuOutType = mk_bits(clog2(cfg.num_fu_outports + 1))
    RegIdxType = mk_bits(clog2(cfg.num_registers_per_reg_bank))

    return {
        "DataType": DataType,
        "DataAddrType": DataAddrType,
        "CtrlType": CtrlType,
        "CtrlAddrType": CtrlAddrType,
        "CgraPayloadType": CgraPayloadType,
        "IntraCgraPktType": IntraCgraPktType,
        "TileInType": TileInType,
        "FuInType": FuInType,
        "FuOutType": FuOutType,
        "RegIdxType": RegIdxType,
    }


def _make_vector_cgra_packets(cfg: KernelConfig, types: Mapping[str, object]):
    factory = ScriptFactory(
        path=str(cfg.kernel_yaml),
        CtrlType=types["CtrlType"],
        IntraCgraPktType=types["IntraCgraPktType"],
        CgraPayloadType=types["CgraPayloadType"],
        TileInType=types["TileInType"],
        FuOutType=types["FuOutType"],
        FuInType=types["FuInType"],
        CMD_CONFIG_input=CMD_CONFIG,
        CMD_CONST_input=CMD_CONST,
        CMD_LAUNCH_input=CMD_LAUNCH,
        CMD_CONFIG_COUNT_PER_ITER_input=CMD_CONFIG_COUNT_PER_ITER,
        CMD_CONFIG_TOTAL_CTRL_COUNT_input=CMD_CONFIG_TOTAL_CTRL_COUNT,
        CMD_CONFIG_PROLOGUE_FU_input=CMD_CONFIG_PROLOGUE_FU,
        CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR_input=CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR,
        CMD_CONFIG_PROLOGUE_FU_CROSSBAR_input=CMD_CONFIG_PROLOGUE_FU_CROSSBAR,
        ii=cfg.compiled_ii,
        loop_times=cfg.loop_times,
        DataType=types["DataType"],
        B1Type=b1,
        B2Type=b2,
        RegIdxType=types["RegIdxType"],
        CtrlAddrType=types["CtrlAddrType"],
        DataAddrType=types["DataAddrType"],
        num_registers_per_reg_bank=cfg.num_registers_per_reg_bank,
        bindings=cfg.bindings,
    )

    with contextlib.redirect_stdout(io.StringIO()):
        return factory.makeVectorCGRAPkts()


def make_vector_cgra_packets(cfg: KernelConfig, types: Mapping[str, object]):
    if not cfg.runtime_symbols:
        return _make_vector_cgra_packets(cfg, types)
    # ScriptFactory treats bindings as addresses. Resolve typed runtime values in
    # root-owned payload relocations; native generation sees only zero/one probes.
    baseline = replace(cfg, bindings={symbol: 0 for symbol in cfg.bindings})
    packets = ordered_packets(cfg, _make_vector_cgra_packets(baseline, types))
    for item in build_relocations(cfg, packets, types):
        packet = packets[item.packet_index][1]
        symbol = cfg.runtime_symbols[item.symbol_index].symbol
        value = cfg.bindings[symbol] * item.scale + item.offset
        packet.payload.data.payload = type(packet.payload.data.payload)(value)
    grouped = {}
    for coord, packet in packets:
        grouped.setdefault(coord, []).append(packet)
    return grouped


def ordered_packets(
    cfg: KernelConfig,
    pkts_by_coord: Mapping[tuple[int, int], Sequence[object]],
) -> list[tuple[tuple[int, int], object]]:
    columns = cfg.x_tiles
    launch_packets = []
    other_packets = []
    for coord in sorted(pkts_by_coord, key=lambda xy: xy[1] * columns + xy[0]):
        for pkt in pkts_by_coord[coord]:
            if int(pkt.payload.cmd) == CMD_LAUNCH:
                launch_packets.append((coord, pkt))
            else:
                other_packets.append((coord, pkt))
    return other_packets + launch_packets


def _type_nbits(types: Mapping[str, object], name: str) -> int:
    type_obj = types[name]
    nbits = getattr(type_obj, "nbits", None)
    if not isinstance(nbits, int):
        raise TypeError(f"{name} does not expose integer nbits")
    return nbits


def _packet_to_int(pkt: object) -> int:
    try:
        return int(pkt)
    except TypeError:
        pass

    to_bits = getattr(pkt, "to_bits", None)
    if to_bits is None:
        raise TypeError("packet object does not support int(pkt) or pkt.to_bits()")

    bits = to_bits()
    try:
        return int(bits)
    except TypeError:
        uint = getattr(bits, "uint", None)
        if uint is None:
            raise TypeError("packet to_bits() result does not expose int() or uint()")
        return int(uint())


def _chunks_from_int(value: int) -> tuple[int, int, int, int]:
    return (
        value & _MASK64,
        (value >> 64) & _MASK64,
        (value >> 128) & _MASK64,
        (value >> 192) & _MASK64,
    )


def _basic_packet_value(
    cfg: object,
    types: Mapping[str, object],
    cmd: int,
    data_payload: int,
    predicate: int,
    data_addr: int,
) -> int:
    del cfg
    DataType = types["DataType"]
    CgraPayloadType = types["CgraPayloadType"]
    IntraCgraPktType = types["IntraCgraPktType"]
    data = DataType(data_payload, predicate, 0, 0)
    payload = CgraPayloadType(cmd, data=data, data_addr=data_addr)
    return _packet_to_int(IntraCgraPktType(0, 0, payload=payload))


def _lowbit_index(value: int) -> int:
    if value == 0:
        raise ValueError("cannot find low bit of zero mask")
    return (value & -value).bit_length() - 1


def _mask_segments(mask: int) -> tuple[BitSegment, ...]:
    if mask == 0:
        return ()
    field_lsb = _lowbit_index(mask)
    segments = []
    for chunk in range(4):
        chunk_base = chunk * 64
        chunk_mask = (mask >> chunk_base) & _MASK64
        bit = 0
        while bit < 64:
            if not ((chunk_mask >> bit) & 1):
                bit += 1
                continue
            start = bit
            while bit < 64 and ((chunk_mask >> bit) & 1):
                bit += 1
            width = bit - start
            segments.append(
                BitSegment(
                    chunk=chunk,
                    value_lsb=chunk_base + start - field_lsb,
                    chunk_lsb=start,
                    width=width,
                )
            )
    return tuple(segments)


def _validate_contiguous_mask(mask: int, width: int, field_name: str) -> None:
    if width <= 0:
        raise ValueError(f"{field_name} width must be positive")
    actual_width = mask.bit_count()
    if actual_width != width:
        raise ValueError(f"{field_name} mask has {actual_width} bits, expected {width}")
    lsb = _lowbit_index(mask)
    expected = ((1 << width) - 1) << lsb
    if mask != expected:
        raise ValueError(f"{field_name} mask is not contiguous")


def build_basic_packet_template(
    cfg: object,
    types: Mapping[str, object],
    cmd: int,
    predicate: int,
    data_payload_is_variable: bool,
) -> BasicPacketTemplate:
    payload_width = int(getattr(cfg, "data_nbits"))
    data_addr_width = _type_nbits(types, "DataAddrType")
    payload_ones = (1 << payload_width) - 1
    data_addr_ones = (1 << data_addr_width) - 1
    base = _basic_packet_value(
        cfg, types, cmd, data_payload=0, predicate=predicate, data_addr=0
    )
    data_addr_mask = base ^ _basic_packet_value(
        cfg, types, cmd, data_payload=0, predicate=predicate, data_addr=data_addr_ones
    )
    _validate_contiguous_mask(data_addr_mask, data_addr_width, "data_addr")

    if data_payload_is_variable:
        data_payload_mask = base ^ _basic_packet_value(
            cfg, types, cmd, data_payload=payload_ones, predicate=predicate, data_addr=0
        )
        _validate_contiguous_mask(data_payload_mask, payload_width, "data_payload")
    else:
        data_payload_mask = 0

    return BasicPacketTemplate(
        cmd=cmd,
        lo=base & _MASK64,
        mid=(base >> 64) & _MASK64,
        hi=(base >> 128) & _MASK64,
        top=(base >> 192) & _MASK64,
        data_payload_segments=_mask_segments(data_payload_mask),
        data_addr_segments=_mask_segments(data_addr_mask),
    )


def encode_packets(
    cfg: object,
    packets: Sequence[tuple[tuple[int, int], object]],
    types: Mapping[str, object],
) -> list[EncodedPacket]:
    """Encode ordered PyMTL packets into four 64-bit C packet chunks."""

    del cfg
    pkt_nbits = _type_nbits(types, "IntraCgraPktType")
    if pkt_nbits > 256:
        raise ValueError("fast API supports packets up to four 64-bit chunks")

    encoded = []
    for coord, pkt in packets:
        value = _packet_to_int(pkt)
        cmd = int(pkt.payload.cmd)
        encoded.append(
            EncodedPacket(
                coord=coord,
                tile=int(pkt.dst),
                cmd=cmd,
                lo=value & _MASK64,
                mid=(value >> 64) & _MASK64,
                hi=(value >> 128) & _MASK64,
                top=(value >> 192) & _MASK64,
                is_launch=(cmd == CMD_LAUNCH),
            )
        )
    return encoded


def build_relocations(
    cfg: KernelConfig,
    packets: Sequence[tuple[tuple[int, int], object]],
    types: Mapping[str, object],
) -> list[Relocation]:
    if not cfg.runtime_symbols:
        return []
    template = build_basic_packet_template(
        cfg, types, CMD_CONST, predicate=1, data_payload_is_variable=True
    )
    bit_offset = _template_field_lsb(template.data_payload_segments)
    field_mask = ((1 << cfg.data_nbits) - 1) << bit_offset
    relocations = []
    for symbol_index, symbol in enumerate(cfg.runtime_symbols):
        variants = []
        for value in (0, 1):
            bindings = {name: 0 for name in cfg.bindings}
            bindings[symbol.symbol] = value
            bound = replace(cfg, bindings=bindings)
            variants.append(
                ordered_packets(bound, _make_vector_cgra_packets(bound, types))
            )
        if any(len(variant) != len(packets) for variant in variants):
            raise ValueError(f"{cfg.source_path}: symbol binding changed packet count")
        found = False
        for index, ((coord0, packet0), (coord1, packet1)) in enumerate(zip(*variants)):
            difference = _packet_to_int(packet0) ^ _packet_to_int(packet1)
            if coord0 != coord1 or difference & ~field_mask:
                raise ValueError(
                    f"{cfg.source_path}: symbol binding changed non-payload fields"
                )
            if not difference:
                continue
            if int(packet0.payload.cmd) != CMD_CONST or difference != 1 << bit_offset:
                raise ValueError(
                    f"{cfg.source_path}: runtime symbols must lower to direct constants"
                )
            found = True
            relocations.append(Relocation(index, symbol_index, 1, 0))
        if not found:
            raise ValueError(
                f"{cfg.source_path}: no packet relocation for {symbol.symbol}"
            )
    count = cfg.runtime_count
    count_index = next(
        index
        for index, symbol in enumerate(cfg.runtime_symbols)
        if symbol.symbol == count.symbol
    )
    relocations.extend(
        Relocation(index, count_index, count.scale, count.offset)
        for index, (_, packet) in enumerate(packets)
        if int(packet.payload.cmd) == CMD_CONFIG_TOTAL_CTRL_COUNT
    )
    return sorted(relocations, key=lambda relocation: relocation.packet_index)


def split_packets(cfg: KernelConfig, packets, types):
    static_commands = {
        CMD_CONFIG,
        CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR,
        CMD_CONFIG_PROLOGUE_FU_CROSSBAR,
    }
    setup_commands = {
        CMD_CONFIG_CTRL_LOWER_BOUND,
        CMD_CONFIG_COUNT_PER_ITER,
        CMD_CONFIG_TOTAL_CTRL_COUNT,
    }
    indexed_commands = static_commands | {CMD_CONFIG_PROLOGUE_FU}
    relocations = build_relocations(cfg, packets, types)
    patched = {item.packet_index for item in relocations}
    PacketType = types["IntraCgraPktType"]
    PayloadType = types["CgraPayloadType"]
    DataType = types["DataType"]
    rearm = [
        ((x, y), PacketType(0, target, payload=PayloadType(CMD_REARM)))
        for target, x, y in cfg.tile_targets
    ]
    setup = [
        (
            coord,
            PacketType(
                0,
                packet.dst,
                payload=PayloadType(
                    CMD_CONFIG_CTRL_LOWER_BOUND, data=DataType(cfg.ctrl_base, 1)
                ),
            ),
        )
        for coord, packet in packets
        if int(packet.payload.cmd) == CMD_LAUNCH
    ]
    static = []
    runtime = []
    indices = {}
    for index, (coord, original) in enumerate(packets):
        packet = copy.deepcopy(original)
        command = int(packet.payload.cmd)
        if command in indexed_commands:
            packet.payload.ctrl_addr = types["CtrlAddrType"](
                int(packet.payload.ctrl_addr) + cfg.ctrl_base
            )
        if command in static_commands:
            static.append((coord, packet))
            continue
        if command in setup_commands and index not in patched:
            setup.append((coord, packet))
            continue
        indices[index] = len(runtime)
        runtime.append((coord, packet))
    prefix_count = len(rearm) + len(setup)
    relocations = [
        replace(item, packet_index=prefix_count + indices[item.packet_index])
        for item in relocations
    ]
    return static, rearm + setup + runtime, relocations, len(rearm), len(setup)


def render_runtime_section(cfg: KernelConfig, relocations) -> list[str]:
    if not cfg.runtime_symbols:
        return []
    prefix = cfg.name.upper()
    lines = [
        "",
        "// Relocations index CONFIG_PACKETS followed by LAUNCH_PACKETS.",
        "// Fields: packet_index, symbol_index, scale, offset.",
        f"#define {prefix}_SYMBOL_COUNT {len(cfg.runtime_symbols)}",
    ]
    for index, symbol in enumerate(cfg.runtime_symbols):
        lines.append(f"#define {prefix}_SYMBOL_{symbol.role.upper()} {index}")
    lines.extend(["", f"static const cgra_patch_t {prefix}_PATCHES[] = {{"])
    lines.extend(
        f"  {{ {item.packet_index}, {item.symbol_index}, {item.scale}, {item.offset} }},"
        for item in relocations
    )
    lines.append("};")
    return lines


def render_kernel(cfg: KernelConfig, relocations) -> list[str]:
    prefix = cfg.name.upper()
    patches = f"{prefix}_PATCHES" if relocations else "0"
    return [
        "",
        f"static const cgra_kernel_t {prefix} = {{",
        f"  .static_packets = {prefix}_FAST_STATIC_PACKETS,",
        f"  .static_count = {prefix}_FAST_STATIC_PACKET_COUNT,",
        f"  .config_packets = {prefix}_FAST_CONFIG_PACKETS,",
        f"  .config_count = {prefix}_FAST_CONFIG_PACKET_COUNT,",
        f"  .rearm_count = {prefix}_FAST_REARM_PACKET_COUNT,",
        f"  .setup_count = {prefix}_FAST_SETUP_PACKET_COUNT,",
        f"  .launch_packets = {prefix}_FAST_LAUNCH_PACKETS,",
        f"  .launch_count = {prefix}_FAST_LAUNCH_PACKET_COUNT,",
        f"  .expected_completes = {prefix}_EXPECTED_COMPLETES,",
        f"  .patches = {patches},",
        f"  .patch_count = {len(relocations)},",
        "};",
        "",
    ]


def _uint64_c(value: int) -> str:
    return f"UINT64_C(0x{value:016x})"


def _uint32_c(value: int) -> str:
    return f"UINT32_C(0x{value:08x})"


def _packet_initializer(pkt: EncodedPacket) -> str:
    return (
        f"  {{ {_uint64_c(pkt.lo)}, {_uint64_c(pkt.mid)}, "
        f"{_uint64_c(pkt.hi)}, {_uint64_c(pkt.top)} }},"
    )


def _render_packet_array(name: str, packets: Sequence[EncodedPacket]) -> list[str]:
    lines = [f"static const cgra_packet_t {name}[] = {{"]
    lines.extend(_packet_initializer(pkt) for pkt in packets)
    lines.append("};")
    return lines


def _render_layout_guard(header_name: str, macro_name: str, actual: int) -> list[str]:
    return [
        f"#if {macro_name} != {actual}",
        f'#error "{header_name} fast API was generated for {macro_name}={actual}"',
        "#endif",
    ]


def _render_layout_guard_expr(
    header_name: str, expr: str, label: str, actual: int
) -> list[str]:
    return [
        f"#if ({expr}) != {actual}",
        f'#error "{header_name} fast API was generated for {label}={actual}"',
        "#endif",
    ]


def _chunk_suffix(chunk: int) -> str:
    return ("LO", "MID", "HI", "TOP")[chunk]


def _template_chunks(tpl: BasicPacketTemplate) -> tuple[int, int, int, int]:
    return tpl.lo, tpl.mid, tpl.hi, tpl.top


def _segment_mask(segment: BitSegment) -> int:
    return (1 << segment.width) - 1


def _render_insert_macro(
    prefix: str, field: str, arg_name: str, segment: BitSegment
) -> str:
    macro = f"{prefix}_{field}_{_chunk_suffix(segment.chunk)}"
    mask = _segment_mask(segment)
    expr = f"((uint64_t)({arg_name})"
    if segment.value_lsb:
        expr += f" >> {segment.value_lsb}"
    expr += f") & {_uint64_c(mask)}"
    if segment.chunk_lsb:
        expr = f"(({expr}) << {segment.chunk_lsb})"
    return f"#define {macro}({arg_name}) ({expr})"


def _render_template_base(prefix: str, tpl: BasicPacketTemplate) -> list[str]:
    return [
        f"#define {prefix}_PKT_BASE_LO  {_uint64_c(tpl.lo)}",
        f"#define {prefix}_PKT_BASE_MID {_uint64_c(tpl.mid)}",
        f"#define {prefix}_PKT_BASE_HI  {_uint64_c(tpl.hi)}",
        f"#define {prefix}_PKT_BASE_TOP {_uint64_c(tpl.top)}",
    ]


def _render_packet_expr(
    prefix: str, fields_by_chunk: Sequence[tuple[str, str]]
) -> list[str]:
    lines = []
    for chunk, suffix in enumerate(("LO", "MID", "HI", "TOP")):
        terms = [f"{prefix}_PKT_BASE_{suffix}"]
        for field, arg_name in fields_by_chunk[chunk]:
            terms.append(f"{prefix}_{field}_{suffix}({arg_name})")
        lines.append("    " + " | ".join(terms) + ",")
    return lines


def _fields_by_chunk(
    tpl: BasicPacketTemplate,
    include_data_payload: bool,
) -> list[list[tuple[str, str]]]:
    fields: list[list[tuple[str, str]]] = [[], [], [], []]
    for segment in tpl.data_addr_segments:
        fields[segment.chunk].append(("DATA_ADDR", "data_addr"))
    if include_data_payload:
        for segment in tpl.data_payload_segments:
            fields[segment.chunk].append(("DATA_PAYLOAD", "data"))
    return fields


def _render_template_macros(
    guard_kernel: str, store: BasicPacketTemplate, load_req: BasicPacketTemplate
) -> list[str]:
    lines = [
        f"#define {guard_kernel}_FAST_DATA_ADDR_MASK "
        f"{_uint32_c((1 << _template_field_width(store.data_addr_segments)) - 1)}",
        f"#define {guard_kernel}_FAST_DATA_PAYLOAD_MASK "
        f"{_uint32_c((1 << _template_field_width(store.data_payload_segments)) - 1)}",
        "",
    ]
    lines.extend(_render_template_base(f"{guard_kernel}_STORE", store))
    for segment in store.data_addr_segments:
        lines.append(
            _render_insert_macro(
                f"{guard_kernel}_STORE", "DATA_ADDR", "data_addr", segment
            )
        )
    for segment in store.data_payload_segments:
        lines.append(
            _render_insert_macro(
                f"{guard_kernel}_STORE", "DATA_PAYLOAD", "data", segment
            )
        )
    lines.append("")
    lines.extend(_render_template_base(f"{guard_kernel}_LOAD_REQ", load_req))
    for segment in load_req.data_addr_segments:
        lines.append(
            _render_insert_macro(
                f"{guard_kernel}_LOAD_REQ", "DATA_ADDR", "data_addr", segment
            )
        )
    return lines


def _template_field_width(segments: Sequence[BitSegment]) -> int:
    return sum(segment.width for segment in segments)


def _template_field_lsb(segments: Sequence[BitSegment]) -> int:
    if not segments:
        raise ValueError("field has no segments")
    return min(
        segment.chunk * 64 + segment.chunk_lsb - segment.value_lsb
        for segment in segments
    )


def _render_basic_packet_helpers(
    kernel: str,
    guard_kernel: str,
    store: BasicPacketTemplate,
    load_req: BasicPacketTemplate,
) -> list[str]:
    store_fields = _fields_by_chunk(store, include_data_payload=True)
    load_req_fields = _fields_by_chunk(load_req, include_data_payload=False)
    lines = [
        "",
        f"static inline void {kernel}_store_fast(uint32_t data_addr,",
        "                                     uint32_t data) {",
        "  const cgra_packet_t pkt = {",
    ]
    lines.extend(_render_packet_expr(f"{guard_kernel}_STORE", store_fields))
    lines.extend(
        [
            "  };",
            "  cgra_send_packet_fast(pkt);",
            "}",
            "",
            f"static inline uint64_t {kernel}_read_mem_fast(uint32_t data_addr) {{",
            "  uint64_t result = 0;",
            "  const cgra_packet_t pkt = {",
        ]
    )
    lines.extend(_render_packet_expr(f"{guard_kernel}_LOAD_REQ", load_req_fields))
    lines.extend(
        [
            "  };",
            "  cgra_send_packet_fast(pkt);",
            "  CGRA_LOAD_RESULT(result);",
            "  return result;",
            "}",
        ]
    )
    return lines


def _render_basic_template_section(
    cfg: object,
    types: Mapping[str, object],
    header_name: str,
    guard_kernel: str,
) -> list[str]:
    kernel = cfg.name
    store = build_basic_packet_template(
        cfg, types, CMD_STORE_REQUEST, predicate=1, data_payload_is_variable=True
    )
    load_req = build_basic_packet_template(
        cfg, types, CMD_LOAD_REQUEST, predicate=0, data_payload_is_variable=False
    )
    payload_lsb = _template_field_lsb(store.data_payload_segments)
    data_addr_lsb = _template_field_lsb(store.data_addr_segments)

    lines = [
        "",
        "// Fast basic packet templates: fixed fields are generated constants;",
        "// runtime code inserts only data_addr and, for stores, data payload.",
        "",
    ]
    lines.extend(
        _render_layout_guard(
            header_name, "CGRA_DATA_PAYLOAD_NBITS", int(getattr(cfg, "data_nbits"))
        )
    )
    lines.extend(
        _render_layout_guard(
            header_name, "DATA_ADDR_NBITS", _type_nbits(types, "DataAddrType")
        )
    )
    lines.extend(
        _render_layout_guard_expr(
            header_name,
            "PKT_DATA_LSB + DATA_PAYLOAD_LSB",
            "PKT_DATA_LSB+DATA_PAYLOAD_LSB",
            payload_lsb,
        )
    )
    lines.extend(_render_layout_guard(header_name, "PKT_DATA_ADDR_LSB", data_addr_lsb))
    lines.append("")
    lines.extend(_render_template_macros(guard_kernel, store, load_req))
    lines.extend(_render_basic_packet_helpers(kernel, guard_kernel, store, load_req))
    return lines


def render_fast_api_section(
    cfg: object,
    static: Sequence[tuple[tuple[int, int], object]],
    packets: Sequence[tuple[tuple[int, int], object]],
    types: Mapping[str, object],
    rearm_count: int,
    setup_count: int,
) -> list[str]:
    """Return the generated fast API C header section."""

    kernel = cfg.name
    guard_kernel = kernel.upper()
    header_name = f"cgra_{kernel}_fast_api.h"
    static_packets = encode_packets(cfg, static, types)
    encoded = encode_packets(cfg, packets, types)
    config_packets = [pkt for pkt in encoded if not pkt.is_launch]
    launch_packets = [pkt for pkt in encoded if pkt.is_launch]
    lines = [
        "",
        "// Fast API: local single-CGRA packets precomputed by scripts/cgra_fast_api.py.",
        "// Fast API is precomputed for cgra_target_local().",
        "// CONFIG contains REARM, fixed setup, then per-run initialization.",
        "",
        f"#define {guard_kernel}_FAST_STATIC_PACKET_COUNT {len(static_packets)}",
        f"#define {guard_kernel}_FAST_CONFIG_PACKET_COUNT {len(config_packets)}",
        f"#define {guard_kernel}_FAST_REARM_PACKET_COUNT {rearm_count}",
        f"#define {guard_kernel}_FAST_SETUP_PACKET_COUNT {setup_count}",
        f"#define {guard_kernel}_FAST_LAUNCH_PACKET_COUNT {len(launch_packets)}",
        "",
    ]

    lines.extend(
        _render_layout_guard(
            header_name, "CGRA_INTRA_PKT_NBITS", _type_nbits(types, "IntraCgraPktType")
        )
    )
    lines.extend(
        _render_layout_guard(
            header_name, "CGRA_CTRL_NBITS", _type_nbits(types, "CtrlType")
        )
    )
    lines.extend(
        _render_layout_guard(
            header_name, "CGRA_DATA_NBITS", _type_nbits(types, "DataType")
        )
    )
    lines.extend(
        _render_layout_guard(
            header_name,
            "CGRA_CMD_NBITS",
            _type_nbits(types, "CgraPayloadType")
            - _type_nbits(types, "DataType")
            - _type_nbits(types, "DataAddrType")
            - _type_nbits(types, "CtrlType")
            - _type_nbits(types, "CtrlAddrType"),
        )
    )
    lines.extend(_render_basic_template_section(cfg, types, header_name, guard_kernel))
    lines.append("")

    lines.extend(
        _render_packet_array(f"{guard_kernel}_FAST_STATIC_PACKETS", static_packets)
    )
    lines.append("")
    lines.extend(
        _render_packet_array(f"{guard_kernel}_FAST_CONFIG_PACKETS", config_packets)
    )
    lines.append("")
    lines.extend(
        _render_packet_array(f"{guard_kernel}_FAST_LAUNCH_PACKETS", launch_packets)
    )
    return lines


def write_header(
    cfg: KernelConfig,
    packets: Sequence[tuple[tuple[int, int], object]],
    types: Mapping[str, object],
    output: Path,
) -> None:
    guard_kernel = cfg.name.upper()
    guard = f"CGRA_{guard_kernel}_FAST_API_H"
    static, runtime, relocations, rearm_count, setup_count = split_packets(
        cfg, packets, types
    )

    lines = [
        f"#ifndef {guard}",
        f"#define {guard}",
        '#include "cgra_protocol.h"',
        '#include "cgra_runtime.h"',
        "",
        "// Auto-generated by scripts/cgra_fast_api.py",
        f"// Config: {rel_to_root(cfg.source_path)}",
        "",
        f"#define {guard_kernel}_CTRL_COUNT_PER_ITER {cfg.compiled_ii}",
        f"#define {guard_kernel}_CTRL_BASE {cfg.ctrl_base}",
        f"#define {guard_kernel}_TOTAL_CTRL_STEPS {cfg.loop_times}",
        f"#define {guard_kernel}_EXPECTED_COMPLETES {cfg.expected_completes}",
    ]

    lines.extend(
        render_fast_api_section(cfg, static, runtime, types, rearm_count, setup_count)
    )
    lines.extend(render_runtime_section(cfg, relocations))
    lines.extend(render_kernel(cfg, relocations))

    lines.extend(
        [
            f"#endif",
            "",
        ]
    )

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines), encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "configs",
        nargs="*",
        default=[str(path) for path in DEFAULT_CONFIGS],
        help="Per-kernel config YAMLs to process.",
    )
    parser.add_argument(
        "--arch-yaml",
        default=str(DEFAULT_ARCH_YAML),
        help="CGRA architecture YAML.",
    )
    parser.add_argument(
        "--soc-yaml",
        default=str(DEFAULT_SOC_YAML),
        help="SoC/interface/memory YAML.",
    )
    parser.add_argument(
        "--output-dir",
        default=str(DEFAULT_OUTPUT_DIR),
        help="Directory for generated cgra_<kernel>_fast_api.h headers.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    arch_yaml = resolve_input_path(args.arch_yaml)
    soc_yaml = resolve_input_path(args.soc_yaml)
    if not arch_yaml.exists():
        raise FileNotFoundError(arch_yaml)
    if not soc_yaml.exists():
        raise FileNotFoundError(soc_yaml)
    output_dir = resolve_input_path(args.output_dir)
    for config_arg in args.configs:
        config_path = resolve_input_path(config_arg)
        if not config_path.exists():
            raise FileNotFoundError(config_path)
        cfg = load_kernel_config(config_path, arch_yaml, soc_yaml)
        types = build_packet_types(cfg)
        pkts_by_coord = make_vector_cgra_packets(cfg, types)
        packets = ordered_packets(cfg, pkts_by_coord)
        output = output_dir / f"cgra_{cfg.name}_fast_api.h"
        write_header(cfg, packets, types, output)
        print(f"wrote {rel_to_root(output)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
