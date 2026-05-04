#!/usr/bin/env python3
"""
Generate C packet headers from VectorCGRA YAML control signals.
"""

from __future__ import annotations

import argparse
import contextlib
import glob
import io
import sys
from pathlib import Path
from typing import Iterable, Mapping, Sequence

import yaml


ROOT = Path(__file__).resolve().parents[1]
VECTOR_ROOT = ROOT / "VectorCGRA"
DEFAULT_ARCH_YAML = ROOT / "configs" / "arch_fir_yaml_4x4.yaml"
DEFAULT_SOC_YAML = ROOT / "configs" / "cgra_soc_fir_yaml_4x4.yaml"
DEFAULT_CONTROL_YAML = VECTOR_ROOT / "validation" / "test" / "fir_acceptance_test.yaml"
DEFAULT_OUTPUT = ROOT / "tests" / "generated" / "cgra_fir_yaml_4x4_packets.h"

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

from pymtl3 import b1, b2, clog2, mk_bits  # noqa: E402

from VectorCGRA.cgra.test.CgraTemplateRTL_single_test import load_soc_config  # noqa: E402
from VectorCGRA.lib.cmd_type import (  # noqa: E402
    CMD_CONFIG,
    CMD_CONFIG_COUNT_PER_ITER,
    CMD_CONFIG_PROLOGUE_FU,
    CMD_CONFIG_PROLOGUE_FU_CROSSBAR,
    CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR,
    CMD_CONFIG_TOTAL_CTRL_COUNT,
    CMD_CONST,
    CMD_LAUNCH,
    CMD_STORE_REQUEST,
)
from VectorCGRA.lib.messages import (  # noqa: E402
    mk_cgra_payload,
    mk_ctrl,
    mk_data,
    mk_intra_cgra_pkt,
)
from VectorCGRA.multi_cgra.arch_parser.ArchParser import ArchParser  # noqa: E402
from VectorCGRA.validation.script_generator import ScriptFactory  # noqa: E402


def resolve_input_path(path: str) -> Path:
  candidate = Path(path)
  if candidate.is_absolute():
    return candidate
  for base in (Path.cwd(), ROOT, VECTOR_ROOT):
    resolved = base / candidate
    if resolved.exists():
      return resolved
  return Path.cwd() / candidate


def load_yaml(path: Path) -> Mapping[str, object]:
  with path.open("r", encoding="utf-8") as stream:
    data = yaml.safe_load(stream)
  if not isinstance(data, Mapping):
    raise ValueError(f"yaml must contain a top-level mapping: {path}")
  return data


def build_packet_types(arch_yaml: Path, soc_yaml: Path):
  soc_cfg = load_soc_config(soc_yaml)
  arch_parser = ArchParser(str(arch_yaml))
  param_cgra = arch_parser.get_simplest_cgra_param()
  num_tiles = len(param_cgra.getValidTiles())

  DataType = mk_data(soc_cfg.data_nbits, soc_cfg.predicate_nbits)
  DataAddrType = mk_bits(clog2(soc_cfg.data_mem_size_global))
  CtrlType = mk_ctrl(
      soc_cfg.num_fu_inports,
      soc_cfg.num_fu_outports,
      soc_cfg.num_tile_inports,
      soc_cfg.num_tile_outports,
      soc_cfg.num_registers_per_reg_bank,
  )
  CtrlAddrType = mk_bits(clog2(param_cgra.configMemSize))
  CgraPayloadType = mk_cgra_payload(DataType, DataAddrType, CtrlType,
                                    CtrlAddrType)
  IntraCgraPktType = mk_intra_cgra_pkt(1, 1, num_tiles, CgraPayloadType)

  return {
      "soc_cfg": soc_cfg,
      "param_cgra": param_cgra,
      "DataType": DataType,
      "DataAddrType": DataAddrType,
      "CtrlType": CtrlType,
      "CtrlAddrType": CtrlAddrType,
      "CgraPayloadType": CgraPayloadType,
      "IntraCgraPktType": IntraCgraPktType,
      "TileInType": mk_bits(clog2(soc_cfg.num_tile_inports + 1)),
      "FuInType": mk_bits(clog2(soc_cfg.num_fu_inports + 1)),
      "FuOutType": mk_bits(clog2(soc_cfg.num_fu_outports + 1)),
      "RegIdxType": mk_bits(clog2(soc_cfg.num_registers_per_reg_bank)),
  }


def make_preload_packets(types: Mapping[str, object], start: int, count: int,
                         tile: int) -> list[object]:
  IntraCgraPktType = types["IntraCgraPktType"]
  CgraPayloadType = types["CgraPayloadType"]
  DataType = types["DataType"]
  DataAddrType = types["DataAddrType"]

  return [
      IntraCgraPktType(
          0,
          tile,
          payload=CgraPayloadType(
              CMD_STORE_REQUEST,
              data=DataType(start + addr, 1),
              data_addr=DataAddrType(addr),
          ),
      )
      for addr in range(count)
  ]


def make_control_packets(types: Mapping[str, object], control_yaml: Path,
                         ii: int, total_steps: int) -> list[object]:
  soc_cfg = types["soc_cfg"]
  factory = ScriptFactory(
      path=str(control_yaml),
      CtrlType=types["CtrlType"],
      IntraCgraPktType=types["IntraCgraPktType"],
      CgraPayloadType=types["CgraPayloadType"],
      TileInType=types["TileInType"],
      FuOutType=types["FuOutType"],
      CMD_CONFIG_input=CMD_CONFIG,
      FuInType=types["FuInType"],
      ii=ii,
      loop_times=total_steps,
      CMD_CONST_input=CMD_CONST,
      CMD_CONFIG_COUNT_PER_ITER_input=CMD_CONFIG_COUNT_PER_ITER,
      CMD_CONFIG_TOTAL_CTRL_COUNT_input=CMD_CONFIG_TOTAL_CTRL_COUNT,
      CMD_CONFIG_PROLOGUE_FU_input=CMD_CONFIG_PROLOGUE_FU,
      CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR_input=
      CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR,
      CMD_CONFIG_PROLOGUE_FU_CROSSBAR_input=CMD_CONFIG_PROLOGUE_FU_CROSSBAR,
      CMD_LAUNCH_input=CMD_LAUNCH,
      DataType=types["DataType"],
      B1Type=b1,
      B2Type=b2,
      RegIdxType=types["RegIdxType"],
      CtrlAddrType=types["CtrlAddrType"],
      DataAddrType=types["DataAddrType"],
      num_registers_per_reg_bank=soc_cfg.num_registers_per_reg_bank,
      num_tile_inports=soc_cfg.num_tile_inports,
      num_tile_outports=soc_cfg.num_tile_outports,
      num_fu_inports=soc_cfg.num_fu_inports,
  )

  # ScriptFactory is chatty; keep generated headers deterministic.
  with contextlib.redirect_stdout(io.StringIO()):
    by_coord = factory.makeVectorCGRAPkts()

  control_data = load_yaml(control_yaml)
  array_config = control_data.get("array_config")
  if not isinstance(array_config, Mapping):
    raise ValueError("control yaml is missing array_config mapping")
  columns = array_config.get("columns")
  if not isinstance(columns, int):
    raise ValueError("control yaml array_config.columns must be an integer")

  packets: list[object] = []
  launch_packets: list[object] = []
  for coord in sorted(by_coord, key=lambda xy: xy[1] * columns + xy[0]):
    for pkt in by_coord[coord]:
      if int(pkt.payload.cmd) == int(CMD_LAUNCH):
        launch_packets.append(pkt)
      else:
        packets.append(pkt)
  packets.extend(launch_packets)
  return packets


def packet_words(pkt: object) -> tuple[int, int, int, int]:
  value = int(pkt.to_bits())
  return (
      value & ((1 << 64) - 1),
      (value >> 64) & ((1 << 64) - 1),
      (value >> 128) & ((1 << 64) - 1),
      (value >> 192) & ((1 << 64) - 1),
  )


def format_packet_array(name: str, packets: Sequence[object]) -> str:
  lines = [f"static const cgra_packet_t {name}[] = {{"]
  for pkt in packets:
    lo, mid, hi, top = packet_words(pkt)
    lines.append(
        "  {UINT64_C(0x%016x), UINT64_C(0x%016x), "
        "UINT64_C(0x%016x), UINT64_C(0x%016x)}," %
        (lo, mid, hi, top)
    )
  lines.append("};")
  return "\n".join(lines)


def write_header(output: Path, packets: Sequence[object], arch_yaml: Path,
                 soc_yaml: Path, control_yaml: Path, expected_completes: int,
                 expected_result: int, ii: int, total_steps: int) -> None:
  guard = "CGRA_FIR_YAML_4X4_PACKETS_H"
  packet_array = format_packet_array("CGRA_FIR_YAML_4X4_PACKETS", packets)
  rel = lambda path: path.resolve().relative_to(ROOT)
  text = f"""/*
 * Auto-generated by scripts/generate_cgra_control_signals.py.
 * arch_yaml: {rel(arch_yaml)}
 * soc_yaml: {rel(soc_yaml)}
 * control_yaml: {rel(control_yaml)}
 * Do not edit by hand; regenerate after YAML or packet type changes.
 */
#ifndef {guard}
#define {guard}

#include \"cgra_runtime.h\"

#define CGRA_FIR_YAML_4X4_EXPECTED_COMPLETES {expected_completes}
#define CGRA_FIR_YAML_4X4_EXPECTED_RESULT {expected_result}
#define CGRA_FIR_YAML_4X4_CTRL_COUNT_PER_ITER {ii}
#define CGRA_FIR_YAML_4X4_TOTAL_CTRL_STEPS {total_steps}

{packet_array}

#define CGRA_FIR_YAML_4X4_PACKET_COUNT \\
  (sizeof(CGRA_FIR_YAML_4X4_PACKETS) / sizeof(CGRA_FIR_YAML_4X4_PACKETS[0]))

#endif
"""
  output.parent.mkdir(parents=True, exist_ok=True)
  output.write_text(text, encoding="utf-8")


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--arch-yaml", default=str(DEFAULT_ARCH_YAML))
  parser.add_argument("--soc-yaml", default=str(DEFAULT_SOC_YAML))
  parser.add_argument("--control-yaml", default=str(DEFAULT_CONTROL_YAML))
  parser.add_argument("--output", default=str(DEFAULT_OUTPUT))
  parser.add_argument("--preload-start", type=int, default=10)
  parser.add_argument("--preload-count", type=int, default=16)
  parser.add_argument("--preload-tile", type=int, default=0)
  parser.add_argument("--expected-completes", type=int, default=1)
  parser.add_argument("--expected-result", type=int, default=2215)
  return parser.parse_args()


def main() -> int:
  args = parse_args()
  arch_yaml = resolve_input_path(args.arch_yaml)
  soc_yaml = resolve_input_path(args.soc_yaml)
  control_yaml = resolve_input_path(args.control_yaml)
  output = Path(args.output)
  if not output.is_absolute():
    output = ROOT / output

  for path in (arch_yaml, soc_yaml, control_yaml):
    if not path.exists():
      raise FileNotFoundError(path)

  types = build_packet_types(arch_yaml, soc_yaml)
  soc_cfg = types["soc_cfg"]
  ii = soc_cfg.ctrl_count_per_iter
  total_steps = soc_cfg.total_ctrl_steps
  if ii is None or total_steps is None:
    raise ValueError("soc yaml execution.ctrl_count_per_iter and total_ctrl_steps are required")

  packets = make_preload_packets(types, args.preload_start, args.preload_count,
                                 args.preload_tile)
  packets.extend(make_control_packets(types, control_yaml, ii, total_steps))
  write_header(output, packets, arch_yaml, soc_yaml, control_yaml,
               args.expected_completes, args.expected_result, ii, total_steps)
  print(f"wrote {output.relative_to(ROOT)} ({len(packets)} packets)")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
