#!/usr/bin/env python3
"""
Generate semantic C CGRA runtime APIs from per-kernel YAML configs.
"""

from __future__ import annotations

import argparse
import contextlib
import glob
import io
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Mapping, Sequence


ROOT = Path(__file__).resolve().parents[1]
VECTOR_ROOT = ROOT / "VectorCGRA"
DEFAULT_CONFIGS = (
    ROOT / "configs" / "kernel_fir4x4_4x4.yaml",
)
DEFAULT_OUTPUT_DIR = ROOT / "tests" / "generated"

for path in (ROOT, VECTOR_ROOT):
  if str(path) not in sys.path:
    sys.path.insert(0, str(path))
python_tag = f"python{sys.version_info.major}.{sys.version_info.minor}"
for site_packages in glob.glob(str(ROOT / ".venv" / "lib" / "python*" / "site-packages")):
  if site_packages not in sys.path:
    sys.path.insert(0, site_packages)
for site_packages in (
    str(Path(sys.prefix) / "lib" / python_tag / "site-packages"),
    str(Path(sys.base_prefix) / "lib" / python_tag / "site-packages"),
):
  if site_packages not in sys.path:
    sys.path.append(site_packages)

import yaml  # noqa: E402

from pymtl3 import b1, b2, clog2, mk_bits  # noqa: E402

from VectorCGRA.lib.cmd_type import (  # noqa: E402
    CMD_CONFIG,
    CMD_CONFIG_COUNT_PER_ITER,
    CMD_CONFIG_PROLOGUE_FU,
    CMD_CONFIG_PROLOGUE_FU_CROSSBAR,
    CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR,
    CMD_CONFIG_TOTAL_CTRL_COUNT,
    CMD_CONST,
    CMD_LAUNCH,
)
from VectorCGRA.lib.messages import (  # noqa: E402
    mk_cgra_payload,
    mk_ctrl,
    mk_data,
    mk_intra_cgra_pkt,
)
from VectorCGRA.validation.script_generator import ScriptFactory  # noqa: E402


OPT_INT_TO_C = {
    0: "OPT_START",
    1: "OPT_NAH",
    2: "OPT_ADD",
    4: "OPT_SUB",
    7: "OPT_MUL",
    10: "OPT_AND",
    11: "OPT_NOT",
    12: "OPT_LD",
    13: "OPT_STR",
    14: "OPT_EQ",
    16: "OPT_GRT_PRED",
    17: "OPT_PHI",
    18: "OPT_MUL_ADD",
    25: "OPT_ADD_CONST",
    26: "OPT_DIV",
    27: "OPT_SEL",
    29: "OPT_MUL_CONST",
    31: "OPT_PAS",
    32: "OPT_PHI_CONST",
    33: "OPT_EQ_CONST",
    35: "OPT_RET",
    36: "OPT_SUB_CONST",
    44: "OPT_REM",
    45: "OPT_NE",
    46: "OPT_NE_CONST",
    47: "OPT_GRT_ONCE",
    60: "OPT_LT",
    61: "OPT_GTE",
    62: "OPT_GT",
    64: "OPT_RET_VOID",
    65: "OPT_DIV_CONST",
    80: "OPT_CONST",
    84: "OPT_PHI_START",
    88: "OPT_GRT_ONCE_CONST",
    89: "OPT_STREAM_LD",
    90: "OPT_GTE_CONST",
    92: "OPT_GEP",
    93: "OPT_GEP_CONST",
    94: "OPT_GEP_2D",
    95: "OPT_GEP_2D_CONST",
    96: "OPT_LLS_CONST",
    97: "OPT_GT_CONST",
    98: "OPT_LT_CONST",
}

PORT_INT_TO_C = {
    0: "PORT_NAH",
    1: "PORT_NORTH",
    2: "PORT_SOUTH",
    3: "PORT_WEST",
    4: "PORT_EAST",
    5: "PORT_NORTHWEST",
    6: "PORT_NORTHEAST",
    7: "PORT_SOUTHEAST",
    8: "PORT_SOUTHWEST",
}

CMD_INT_TO_C = {
    CMD_CONFIG_COUNT_PER_ITER: "CGRA_CMD_CONFIG_COUNT_PER_ITER",
    CMD_CONFIG_TOTAL_CTRL_COUNT: "CGRA_CMD_CONFIG_TOTAL_CTRL_COUNT",
    CMD_CONFIG_PROLOGUE_FU: "CGRA_CMD_CONFIG_PROLOGUE_FU",
    CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR:
        "CGRA_CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR",
    CMD_CONFIG_PROLOGUE_FU_CROSSBAR: "CGRA_CMD_CONFIG_PROLOGUE_FU_CROSSBAR",
    CMD_CONST: "CGRA_CMD_CONST",
    CMD_LAUNCH: "CGRA_CMD_LAUNCH",
}


@dataclass(frozen=True)
class KernelConfig:
  name: str
  source_path: Path
  kernel_yaml: Path
  x_tiles: int
  y_tiles: int
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
  loop_times: int


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


def load_yaml_mapping(path: Path) -> Mapping[str, object]:
  with path.open("r", encoding="utf-8") as stream:
    data = yaml.safe_load(stream)
  if not isinstance(data, Mapping):
    raise ValueError(f"YAML must contain a top-level mapping: {path}")
  return data


def require_mapping(data: Mapping[str, object], key: str, path: Path) -> Mapping[str, object]:
  value = data.get(key)
  if not isinstance(value, Mapping):
    raise ValueError(f"{path}: missing mapping '{key}'")
  return value


def require_int(data: Mapping[str, object], key: str, path: Path) -> int:
  value = data.get(key)
  if not isinstance(value, int) or isinstance(value, bool):
    raise ValueError(f"{path}: '{key}' must be an integer")
  return value


def require_str(data: Mapping[str, object], key: str, path: Path) -> str:
  value = data.get(key)
  if not isinstance(value, str) or not value:
    raise ValueError(f"{path}: '{key}' must be a non-empty string")
  return value


def load_kernel_config(path: Path) -> KernelConfig:
  data = load_yaml_mapping(path)
  kernel = require_mapping(data, "kernel", path)
  cgra = require_mapping(data, "cgra", path)
  interface = require_mapping(data, "interface", path)
  memory = require_mapping(data, "memory", path)
  execution = require_mapping(data, "execution", path)

  name = require_str(kernel, "name", path)
  kernel_yaml = resolve_input_path(require_str(kernel, "kernel_yaml", path),
                                   path.parent)
  if not kernel_yaml.exists():
    raise FileNotFoundError(f"{path}: kernel_yaml does not exist: {kernel_yaml}")

  return KernelConfig(
      name=name,
      source_path=path,
      kernel_yaml=kernel_yaml,
      x_tiles=require_int(cgra, "x_tiles", path),
      y_tiles=require_int(cgra, "y_tiles", path),
      num_cgra_columns=require_int(cgra, "num_cgra_columns", path),
      num_cgra_rows=require_int(cgra, "num_cgra_rows", path),
      config_mem_size=require_int(cgra, "config_mem_size", path),
      num_tile_inports=require_int(interface, "num_tile_inports", path),
      num_tile_outports=require_int(interface, "num_tile_outports", path),
      num_fu_inports=require_int(interface, "num_fu_inports", path),
      num_fu_outports=require_int(interface, "num_fu_outports", path),
      data_nbits=require_int(interface, "data_nbits", path),
      predicate_nbits=require_int(interface, "predicate_nbits", path),
      data_mem_size_global=require_int(memory, "data_mem_size_global", path),
      data_mem_size_per_bank=require_int(memory, "data_mem_size_per_bank", path),
      num_banks_per_cgra=require_int(memory, "num_banks_per_cgra", path),
      num_registers_per_reg_bank=require_int(
          memory, "num_registers_per_reg_bank", path),
      compiled_ii=require_int(execution, "compiled_ii", path),
      loop_times=require_int(execution, "loop_times", path),
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
  CgraPayloadType = mk_cgra_payload(DataType, DataAddrType, CtrlType,
                                    CtrlAddrType)
  num_tiles = cfg.x_tiles * cfg.y_tiles
  IntraCgraPktType = mk_intra_cgra_pkt(
      cfg.num_cgra_columns, cfg.num_cgra_rows, num_tiles, CgraPayloadType)
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


def make_vector_cgra_packets(cfg: KernelConfig, types: Mapping[str, object]):
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
      CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR_input=
      CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR,
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
  )

  with contextlib.redirect_stdout(io.StringIO()):
    return factory.makeVectorCGRAPkts()


def ordered_packets(cfg: KernelConfig, pkts_by_coord: Mapping[tuple[int, int], Sequence[object]]):
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


def c_array(values: Sequence[int], size: int, value_map: Mapping[int, str] | None = None) -> str:
  if len(values) != size:
    raise ValueError(f"expected {size} array elements, got {len(values)}")
  rendered = []
  for value in values:
    rendered.append(value_map.get(value, str(value)) if value_map else str(value))
  return f"(const uint8_t[{size}]){{{', '.join(rendered)}}}"


def op_name(value: int) -> str:
  return OPT_INT_TO_C.get(value, str(value))


def rel_to_root(path: Path) -> str:
  try:
    return path.resolve().relative_to(ROOT).as_posix()
  except ValueError:
    return path.as_posix()


def tile_comment(coord: tuple[int, int], tile: int) -> str:
  return f"  // === Tile ({coord[0]},{coord[1]}) core_id={tile} ==="


def emit_config(pkt: object, nfo: int, nro: int) -> str:
  ctrl = pkt.payload.ctrl
  tile = int(pkt.dst)
  ctrl_addr = int(pkt.payload.ctrl_addr)
  op = int(ctrl.operation)
  fu_in = [int(ctrl.fu_in[i]) for i in range(nfo)]
  routing_xbar = [int(ctrl.routing_xbar_outport[i]) for i in range(nro)]
  fu_xbar = [int(ctrl.fu_xbar_outport[i]) for i in range(nro)]
  wreg_from = [int(ctrl.write_reg_from[i]) for i in range(nfo)]
  wreg_idx = [int(ctrl.write_reg_idx[i]) for i in range(nfo)]
  rreg_towards = [int(ctrl.read_reg_towards[i]) for i in range(nfo)]
  rreg_idx = [int(ctrl.read_reg_idx[i]) for i in range(nfo)]
  return (
      f"  send_config({tile}, {ctrl_addr}, build_ctrl("
      f"{op_name(op)}, "
      f"{c_array(fu_in, nfo)}, "
      f"{c_array(routing_xbar, nro, PORT_INT_TO_C)}, "
      f"{c_array(fu_xbar, nro)}, "
      f"{c_array(wreg_from, nfo)}, "
      f"{c_array(wreg_idx, nfo)}, "
      f"{c_array(rreg_towards, nfo)}, "
      f"{c_array(rreg_idx, nfo)}));"
  )


def emit_packet(cfg: KernelConfig, pkt: object, nfo: int, nro: int) -> str:
  tile = int(pkt.dst)
  cmd = int(pkt.payload.cmd)
  ctrl_addr = int(pkt.payload.ctrl_addr)

  if cmd == CMD_CONFIG:
    return emit_config(pkt, nfo, nro)

  if cmd == CMD_CONST:
    data = int(pkt.payload.data.payload)
    pred = int(pkt.payload.data.predicate)
    return f"  send_basic({tile}, CGRA_CMD_CONST, {data}, {pred}, 0);"

  if cmd == CMD_CONFIG_COUNT_PER_ITER:
    return (
        f"  send_basic({tile}, CGRA_CMD_CONFIG_COUNT_PER_ITER, "
        f"{cfg.name.upper()}_CTRL_COUNT_PER_ITER, 1, 0);"
    )

  if cmd == CMD_CONFIG_TOTAL_CTRL_COUNT:
    return (
        f"  send_basic({tile}, CGRA_CMD_CONFIG_TOTAL_CTRL_COUNT, "
        f"{cfg.name.upper()}_TOTAL_CTRL_STEPS, 1, 0);"
    )

  if cmd == CMD_CONFIG_PROLOGUE_FU:
    count = int(pkt.payload.data.payload)
    return (
        f"  send_prologue({tile}, CGRA_CMD_CONFIG_PROLOGUE_FU, "
        f"{ctrl_addr}, {count}, (cgra_ctrl_t){{0, 0, 0}});"
    )

  if cmd == CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR:
    count = int(pkt.payload.data.payload)
    routing_xbar = [int(pkt.payload.ctrl.routing_xbar_outport[i])
                    for i in range(nro)]
    return (
        f"  send_prologue({tile}, CGRA_CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR, "
        f"{ctrl_addr}, {count}, build_ctrl(0, 0, "
        f"{c_array(routing_xbar, nro, PORT_INT_TO_C)}, "
        f"0, 0, 0, 0, 0));"
    )

  if cmd == CMD_CONFIG_PROLOGUE_FU_CROSSBAR:
    count = int(pkt.payload.data.payload)
    fu_xbar = [int(pkt.payload.ctrl.fu_xbar_outport[i]) for i in range(nro)]
    if all(value == 0 for value in fu_xbar):
      ctrl_expr = "(cgra_ctrl_t){0, 0, 0}"
    else:
      ctrl_expr = (
          f"build_ctrl(0, 0, 0, {c_array(fu_xbar, nro)}, "
          "0, 0, 0, 0)"
      )
    return (
        f"  send_prologue({tile}, CGRA_CMD_CONFIG_PROLOGUE_FU_CROSSBAR, "
        f"{ctrl_addr}, {count}, {ctrl_expr});"
    )

  if cmd == CMD_LAUNCH:
    return f"  send_basic({tile}, CGRA_CMD_LAUNCH, 0, 0, 0);"

  cmd_name = CMD_INT_TO_C.get(cmd, str(cmd))
  raise ValueError(f"unsupported packet command {cmd_name} ({cmd})")


def write_header(cfg: KernelConfig, packets: Sequence[tuple[tuple[int, int], object]],
                 output: Path) -> None:
  guard_kernel = cfg.name.upper()
  guard = f"CGRA_{guard_kernel}_API_H"
  nfo = cfg.num_fu_inports
  nro = cfg.num_tile_outports + cfg.num_fu_inports

  lines = [
      f"#ifndef {guard}",
      f"#define {guard}",
      '#include "cgra_protocol.h"',
      '#include "cgra_runtime.h"',
      "",
      "// Auto-generated by scripts/generate_cgra_c_api.py",
      f"// Config: {rel_to_root(cfg.source_path)}",
      "",
      f"#define {guard_kernel}_CTRL_COUNT_PER_ITER {cfg.compiled_ii}",
      f"#define {guard_kernel}_TOTAL_CTRL_STEPS {cfg.loop_times}",
      "",
      f"static inline void configure_{cfg.name}(void) {{",
  ]

  last_coord = None
  for coord, pkt in packets:
    tile = int(pkt.dst)
    if coord != last_coord:
      lines.append(tile_comment(coord, tile))
      last_coord = coord
    lines.append(emit_packet(cfg, pkt, nfo, nro))

  lines.extend([
      "}",
      f"#endif",
      "",
  ])

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
      "--output-dir",
      default=str(DEFAULT_OUTPUT_DIR),
      help="Directory for generated cgra_<kernel>_api.h headers.",
  )
  return parser.parse_args()


def main() -> int:
  args = parse_args()
  output_dir = resolve_input_path(args.output_dir)
  for config_arg in args.configs:
    config_path = resolve_input_path(config_arg)
    if not config_path.exists():
      raise FileNotFoundError(config_path)
    cfg = load_kernel_config(config_path)
    types = build_packet_types(cfg)
    pkts_by_coord = make_vector_cgra_packets(cfg, types)
    packets = ordered_packets(cfg, pkts_by_coord)
    output = output_dir / f"cgra_{cfg.name}_api.h"
    write_header(cfg, packets, output)
    print(f"wrote {rel_to_root(output)} ({len(packets)} packets)")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
