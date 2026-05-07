#!/usr/bin/env python3
"""
Generate C packet headers from VectorCGRA YAML control signals.
"""

from __future__ import annotations

import argparse
import contextlib
import copy
import glob
import io
import sys
import tempfile
from pathlib import Path
from typing import Iterable, Mapping, Sequence, cast


ROOT = Path(__file__).resolve().parents[1]
VECTOR_ROOT = ROOT / "VectorCGRA"
DEFAULT_ARCH_YAML = ROOT / "configs" / "architectures" / "neura_architecture.yaml"
DEFAULT_SOC_YAML = ROOT / "configs" / "cgra_soc_neura_4x4.yaml"
DEFAULT_CONTROL_YAML = ROOT / "configs" / "kernels" / "fir.yaml"
DEFAULT_OUTPUT = ROOT / "tests" / "generated" / "cgra_fir_packets.h"

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
    CMD_RESUME,
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


def is_compiler_instructions_yaml(data: Mapping[str, object]) -> bool:
  array_config = data.get("array_config")
  if not isinstance(array_config, Mapping):
    return False
  if "compiled_ii" in array_config:
    return True
  cores = array_config.get("cores")
  if not isinstance(cores, list):
    return False
  for core in cores:
    if not isinstance(core, Mapping):
      continue
    entries = core.get("entries")
    if not isinstance(entries, list):
      continue
    for entry in entries:
      if not isinstance(entry, Mapping):
        continue
      instructions = entry.get("instructions")
      if not isinstance(instructions, list):
        continue
      for instruction in instructions:
        if isinstance(instruction, Mapping) and "index_per_ii" in instruction:
          return True
  return False


def operand_text(operand: object) -> str:
  if isinstance(operand, Mapping):
    value = operand.get("operand")
  else:
    value = operand
  if not isinstance(value, str):
    raise ValueError(f"operand must be a string or operand mapping, got {operand!r}")
  return value


def is_imm_operand(operand: object) -> bool:
  value = operand_text(operand)
  return value.startswith("#") or value.startswith("arg")


def parse_imm_operand(value: str) -> int:
  if value.startswith("#"):
    value = value[1:]
  if value.startswith("arg"):
    suffix = value[3:]
    if not suffix.isdigit():
      raise ValueError(f"symbolic operand '{value}' must use argN form")
    return int(suffix) * 64
  return int(value, 0)


def normalize_imm_operand(operand: object) -> object:
  if isinstance(operand, Mapping):
    value = operand_text(operand)
    normalized = dict(operand)
    normalized["operand"] = f"#{parse_imm_operand(value)}"
    return normalized
  return f"#{parse_imm_operand(operand_text(operand))}"


def operation_context(path: Path, core: Mapping[str, object],
                      instruction: Mapping[str, object],
                      operation: Mapping[str, object]) -> str:
  return (
      f"{path}: core_id={core.get('core_id')} "
      f"index_per_ii={instruction.get('index_per_ii')} "
      f"opcode={operation.get('opcode')}"
  )


def normalize_opcode(opcode: str) -> str:
  opcode_map = {
      "CTRL_MOV": "DATA_MOV",
      "GEP": "ADD",
      "LOAD": "LDD",
      "LDD": "LDD",
      "STORE": "STORE",
      "ICMP_EQ": "EQ",
      "ICMP_SGE": "GTE",
      "ICMP_SGT": "GT",
      "ICMP_ULT": "LT",
      "RETURN": "RETURN",
      "RETURN_VALUE": "RETURN",
      "RETURN_VOID": "RETURN_VOID",
      "PHI_START": "PHI_START",
  }
  return opcode_map.get(opcode, opcode)


def normalize_operation(path: Path, core: Mapping[str, object],
                        instruction: Mapping[str, object],
                        operation: Mapping[str, object]) -> dict[str, object]:
  if "opcode" not in operation:
    raise ValueError(f"{operation_context(path, core, instruction, operation)} missing opcode")
  normalized = copy.deepcopy(dict(operation))
  opcode = str(normalized["opcode"])
  supported = {
      "LDD",
      "LOAD",
      "STORE",
      "GEP",
      "DATA_MOV",
      "CTRL_MOV",
      "CONSTANT",
      "GRANT_ONCE",
      "GRANT_PREDICATE",
      "PHI",
      "PHI_START",
      "PHI_CONST",
      "ICMP_EQ",
      "ICMP_SGE",
      "ICMP_SGT",
      "ICMP_ULT",
      "NE",
      "NOT",
      "ADD",
      "SUB",
      "MUL",
      "DIV",
      "OR",
      "AND",
      "SEL",
      "SEXT",
      "ZEXT",
      "CAST_TRUNC",
      "SHL",
      "RETURN",
      "RETURN_VALUE",
      "RETURN_VOID",
  }
  if opcode not in supported:
    raise ValueError(f"unsupported opcode in {operation_context(path, core, instruction, operation)}")

  src_operands = list(normalized.get("src_operands", []) or [])
  dst_operands = list(normalized.get("dst_operands", []) or [])

  const_source_supported = {
      "ADD",
      "GEP",
      "MUL",
      "ICMP_EQ",
      "NE",
      "CONSTANT",
      "GRANT_ONCE",
      "PHI_CONST",
  }
  if any(is_imm_operand(operand) for operand in src_operands) and opcode not in const_source_supported:
    raise ValueError(
        "unsupported immediate operand in "
        f"{operation_context(path, core, instruction, operation)}"
    )

  if opcode == "GRANT_ONCE":
    if not src_operands:
      raise ValueError(f"unsupported GRANT_ONCE without source in {operation_context(path, core, instruction, operation)}")
    src_operands = [normalize_imm_operand(src_operands[0]) if is_imm_operand(src_operands[0]) else src_operands[0]]
  else:
    src_operands = [
        normalize_imm_operand(operand) if is_imm_operand(operand) else operand
        for operand in src_operands
    ]

  if opcode == "STORE":
    if len(src_operands) != 2:
      raise ValueError(f"unsupported STORE operand count in {operation_context(path, core, instruction, operation)}")
    # Compiler instructions list STORE operands as (value, address). VectorCGRA
    # MemUnitRTL consumes operands as (address, value).
    src_operands = [src_operands[1], src_operands[0]]

  opcode = normalize_opcode(opcode)

  normalized["opcode"] = opcode
  normalized["src_operands"] = src_operands
  if "dst_operands" in normalized or dst_operands:
    normalized["dst_operands"] = dst_operands
  return normalized


def require_mapping(value: object, message: str) -> dict[str, object]:
  if not isinstance(value, Mapping):
    raise ValueError(message)
  return cast(dict[str, object], value)


def require_list(value: object, message: str,
                 *, non_empty: bool = False) -> list[object]:
  if not isinstance(value, list) or (non_empty and not value):
    raise ValueError(message)
  return value


def require_int(value: object, message: str) -> int:
  if not isinstance(value, int):
    raise ValueError(message)
  return value


def normalize_instruction_operations(
    control_yaml: Path,
    core: Mapping[str, object],
    instruction: Mapping[str, object],
) -> tuple[list[dict[str, object]], int]:
  index_per_ii = instruction.get("index_per_ii")
  operations = require_list(
      instruction.get("operations"),
      "instruction operations must be a non-empty list in "
      f"{control_yaml}: core_id={core.get('core_id')} "
      f"index_per_ii={index_per_ii}",
      non_empty=True,
  )

  normalized_operations = []
  timesteps = []
  for operation_value in operations:
    operation = require_mapping(
        operation_value,
        "instruction operations must all be mappings in "
        f"{control_yaml}: core_id={core.get('core_id')} "
        f"index_per_ii={index_per_ii}",
    )
    timesteps.append(require_int(
        operation.get("time_step"),
        "operation time_step must be an integer in "
        f"{operation_context(control_yaml, core, instruction, operation)}",
    ))
    normalized_operations.append(
        normalize_operation(control_yaml, core, instruction, operation)
    )

  return normalized_operations, min(timesteps)


def normalize_instruction(control_yaml: Path, core: Mapping[str, object],
                          instruction: dict[str, object]) -> None:
  index_per_ii = require_int(
      instruction.get("index_per_ii"),
      f"instruction index_per_ii must be an integer in {control_yaml}: "
      f"core_id={core.get('core_id')}",
  )
  operations, timestep = normalize_instruction_operations(
      control_yaml, core, instruction
  )
  instruction["timestep"] = timestep
  instruction["ctrl_addr"] = index_per_ii
  instruction["operations"] = operations


def normalize_control_yaml(data: Mapping[str, object],
                           control_yaml: Path) -> tuple[Mapping[str, object], int | None]:
  if not is_compiler_instructions_yaml(data):
    return data, None

  normalized = copy.deepcopy(dict(data))
  array_config = require_mapping(
      normalized.get("array_config"),
      f"control yaml is missing array_config mapping: {control_yaml}",
  )
  compiled_ii = require_int(
      array_config.get("compiled_ii"),
      "compiler-format control yaml array_config.compiled_ii must be an "
      f"integer: {control_yaml}",
  )
  cores = require_list(
      array_config.get("cores"),
      f"compiler-format control yaml array_config.cores must be a list: "
      f"{control_yaml}",
  )

  for core_value in cores:
    core = require_mapping(
        core_value,
        f"compiler-format control yaml core must be a mapping: {control_yaml}",
    )
    entries = require_list(
        core.get("entries"),
        f"compiler-format control yaml core entries must be a list: "
        f"{control_yaml}",
    )
    for entry_value in entries:
      entry = require_mapping(
          entry_value,
          f"compiler-format control yaml entry must be a mapping: {control_yaml}",
      )
      instructions = require_list(
          entry.get("instructions"),
          "compiler-format control yaml entry instructions must be a list: "
          f"{control_yaml}",
      )
      for instruction_value in instructions:
        instruction = require_mapping(
            instruction_value,
            "compiler-format control yaml instruction must be a mapping: "
            f"{control_yaml}",
        )
        normalize_instruction(control_yaml, core, instruction)
      instructions.sort(key=lambda item: item["timestep"])

  return normalized, compiled_ii


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
  control_data = load_yaml(control_yaml)
  normalized_data, yaml_ii = normalize_control_yaml(control_data, control_yaml)
  if yaml_ii is not None:
    ii = yaml_ii

  factory_path = control_yaml
  temp_file: tempfile.NamedTemporaryFile[str] | None = None
  if yaml_ii is not None:
    temp_file = tempfile.NamedTemporaryFile("w", suffix=".yaml", delete=False,
                                            encoding="utf-8")
    yaml.safe_dump(normalized_data, temp_file, sort_keys=False)
    temp_file.close()
    factory_path = Path(temp_file.name)

  factory = ScriptFactory(
      path=str(factory_path),
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
      # Compiler register names are global logical IDs, grouped as four
      # 8-entry banks ($0, $8, $16, ...). The hardware can still expose wider
      # physical banks; RegIdxType comes from the SoC config.
      num_registers_per_reg_bank=8 if yaml_ii is not None else soc_cfg.num_registers_per_reg_bank,
      num_tile_inports=soc_cfg.num_tile_inports,
      num_tile_outports=soc_cfg.num_tile_outports,
      num_fu_inports=soc_cfg.num_fu_inports,
  )

  # ScriptFactory is chatty; keep generated headers deterministic.
  try:
    with contextlib.redirect_stdout(io.StringIO()):
      by_coord = factory.makeVectorCGRAPkts()
  finally:
    if temp_file is not None:
      Path(temp_file.name).unlink(missing_ok=True)

  array_config = normalized_data.get("array_config")
  if not isinstance(array_config, Mapping):
    raise ValueError(f"control yaml is missing array_config mapping: {control_yaml}")
  columns = array_config.get("columns")
  if not isinstance(columns, int):
    raise ValueError(f"control yaml array_config.columns must be an integer: {control_yaml}")

  packets: list[object] = []
  launch_packets: list[object] = []
  for coord in sorted(by_coord, key=lambda xy: xy[1] * columns + xy[0]):
    for pkt in by_coord[coord]:
      if int(pkt.payload.cmd) == int(CMD_LAUNCH):
        launch_packets.append(pkt)
      else:
        packets.append(pkt)
  packets.extend(launch_packets)
  if launch_packets:
    packets.append(
        types["IntraCgraPktType"](
            0,
            0,
            opaque=0xff,
            payload=types["CgraPayloadType"](CMD_RESUME),
        )
    )
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
  base_name = output.stem
  if base_name.startswith("cgra_") and base_name.endswith("_packets"):
    base_name = base_name[len("cgra_"):-len("_packets")]
  prefix = f"CGRA_{base_name.upper()}".replace("-", "_")
  guard = f"{prefix}_PACKETS_H"
  packet_array = format_packet_array(f"{prefix}_PACKETS", packets)
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

#define {prefix}_EXPECTED_COMPLETES {expected_completes}
#define {prefix}_EXPECTED_RESULT {expected_result}
#define {prefix}_CTRL_COUNT_PER_ITER {ii}
#define {prefix}_TOTAL_CTRL_STEPS {total_steps}

{packet_array}

#define {prefix}_PACKET_COUNT \\
  (sizeof({prefix}_PACKETS) / sizeof({prefix}_PACKETS[0]))

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
  parser.add_argument("--expected-result", type=int, default=528)
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
  control_data = load_yaml(control_yaml)
  _normalized_data, yaml_ii = normalize_control_yaml(control_data, control_yaml)
  ii = yaml_ii if yaml_ii is not None else soc_cfg.ctrl_count_per_iter
  total_steps = soc_cfg.total_ctrl_steps
  if ii is None or total_steps is None:
    raise ValueError("soc yaml execution.ctrl_count_per_iter and total_ctrl_steps are required")

  packets = []
  if yaml_ii is None:
    packets.extend(make_preload_packets(types, args.preload_start,
                                        args.preload_count,
                                        args.preload_tile))
  packets.extend(make_control_packets(types, control_yaml, ii, total_steps))
  write_header(output, packets, arch_yaml, soc_yaml, control_yaml,
               args.expected_completes, args.expected_result, ii, total_steps)
  try:
    display_output = output.relative_to(ROOT)
  except ValueError:
    display_output = output
  print(f"wrote {display_output} ({len(packets)} packets)")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
