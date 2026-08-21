#!/usr/bin/env python3
"""Generate the Gemmini external-SPAD hardware/software address contract."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
from typing import Mapping

import yaml

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOC_YAML = ROOT / "configs" / "soc" / "cgra_soc.yaml"
DEFAULT_SCALA_OUT = (
    ROOT
    / "chipyard"
    / "generators"
    / "chipyard"
    / "src"
    / "main"
    / "scala"
    / "example"
    / "GemminiExternalSpadGenerated.scala"
)
DEFAULT_C_HEADER_OUT = ROOT / "tests" / "include" / "gemmini_external_spad.h"
DEFAULT_CONTROL_HEADER_OUT = (
    ROOT / "tests" / "include" / "cgra_transfer_control_generated.h"
)

CONTROL_REGISTERS = {
    "PULL_JOB_ID": 0x000,
    "PULL_SLOT": 0x008,
    "PULL_BYTES": 0x010,
    "PULL_SPM_WORD_ADDRESS": 0x018,
    "PULL_DMA_TAG": 0x020,
    "PULL_SUBMIT": 0x028,
    "LAUNCH_JOB_ID": 0x040,
    "LAUNCH_SLOT": 0x048,
    "LAUNCH_BYTES": 0x050,
    "LAUNCH_SPM_WORD_ADDRESS": 0x058,
    "LAUNCH_DMA_TAG": 0x060,
    "LAUNCH_PACKET_COUNT": 0x068,
    "LAUNCH_SUBMIT": 0x070,
    "PACKET_LO": 0x080,
    "PACKET_MID": 0x088,
    "PACKET_HI": 0x090,
    "PACKET_TOP": 0x098,
    "PACKET_SUBMIT": 0x0A0,
    "LAUNCH_RESULT_VALID": 0x100,
    "LAUNCH_RESULT_POP": 0x108,
    "LAUNCH_RESULT_JOB_ID": 0x110,
    "LAUNCH_RESULT_SLOT": 0x118,
    "LAUNCH_RESULT_REQUESTED_BYTES": 0x120,
    "LAUNCH_RESULT_ACTUAL_BYTES": 0x128,
    "LAUNCH_RESULT_SPM_WORD_ADDRESS": 0x130,
    "LAUNCH_RESULT_DMA_TAG": 0x138,
    "LAUNCH_RESULT_PACKET_COUNT": 0x140,
    "LAUNCH_RESULT_STATUS": 0x148,
    "LAUNCH_ERROR_VALID": 0x180,
    "LAUNCH_ERROR_POP": 0x188,
    "LAUNCH_ERROR_JOB_ID": 0x190,
    "LAUNCH_ERROR_SLOT": 0x198,
    "LAUNCH_ERROR_REQUESTED_BYTES": 0x1A0,
    "LAUNCH_ERROR_ACTUAL_BYTES": 0x1A8,
    "LAUNCH_ERROR_SPM_WORD_ADDRESS": 0x1B0,
    "LAUNCH_ERROR_DMA_TAG": 0x1B8,
    "LAUNCH_ERROR_OPERATION": 0x1C0,
    "LAUNCH_ERROR_REASON": 0x1C8,
    "COMPUTE_RESULT_VALID": 0x200,
    "COMPUTE_RESULT_POP": 0x208,
    "COMPUTE_RESULT_JOB_ID": 0x210,
    "COMPUTE_RESULT_SLOT": 0x218,
    "COMPUTE_RESULT_REQUESTED_BYTES": 0x220,
    "COMPUTE_RESULT_ACTUAL_BYTES": 0x228,
    "COMPUTE_RESULT_SPM_WORD_ADDRESS": 0x230,
    "COMPUTE_RESULT_DMA_TAG": 0x238,
    "COMPUTE_RESULT_PACKET_COUNT": 0x240,
    "COMPUTE_RESULT_DATA": 0x248,
    "COMPUTE_RESULT_STATUS": 0x250,
    "COMPUTE_ERROR_VALID": 0x280,
    "COMPUTE_ERROR_POP": 0x288,
    "COMPUTE_ERROR_JOB_ID": 0x290,
    "COMPUTE_ERROR_SLOT": 0x298,
    "COMPUTE_ERROR_REQUESTED_BYTES": 0x2A0,
    "COMPUTE_ERROR_ACTUAL_BYTES": 0x2A8,
    "COMPUTE_ERROR_SPM_WORD_ADDRESS": 0x2B0,
    "COMPUTE_ERROR_DMA_TAG": 0x2B8,
    "COMPUTE_ERROR_OPERATION": 0x2C0,
    "COMPUTE_ERROR_REASON": 0x2C8,
}

LAUNCH_STATUS = {
    "LAUNCH_ACCEPTED": 0,
    "INVALID_JOB": 1,
    "INVALID_SLOT": 2,
    "INVALID_LENGTH": 3,
    "SPM_RANGE": 4,
    "INVALID_PACKET_COUNT": 5,
    "CONSUMER_FAILURE": 6,
    "PRODUCER_FAILURE": 7,
    "IDENTITY_MISMATCH": 8,
    "INVALID_PACKET": 9,
}

COMPUTE_STATUS = {"SUCCESS": 0}

LAUNCH_ERROR_OPERATIONS = {
    "HEADER": 0,
    "PACKET": 1,
    "COMPLETION": 2,
    "PULL": 3,
}

LAUNCH_ERROR_REASONS = {
    "UNEXPECTED_EVENT": 1,
    "DUPLICATE_EVENT": 2,
    "IDENTITY_MISMATCH": 3,
    "LENGTH_MISMATCH": 4,
    "NON_LAUNCH_PACKET": 5,
    "MALFORMED_COMPLETION": 6,
    "FIELD_OUT_OF_RANGE": 7,
}

COMPUTE_ERROR_OPERATIONS = {
    "LAUNCH_RESULT": 0,
    "COMPLETE": 1,
}

COMPUTE_ERROR_REASONS = {
    "UNEXPECTED_EVENT": 1,
    "IDENTITY_MISMATCH": 2,
    "COMPLETE_BEFORE_LAUNCH": 3,
    "DUPLICATE_COMPLETE": 4,
    "DUPLICATE_LAUNCH_RESULT": 5,
}


@dataclass(frozen=True)
class ExternalSpadContract:
    base_address: int
    size_bytes: int
    production_control_address: int
    validation_telemetry_address: int
    control_page_size_bytes: int
    spad_row_bytes: int
    full_width_row_stride: int
    output_slot_count: int
    output_slot_size_bytes: int

    @property
    def full_width_row_bytes(self) -> int:
        return self.spad_row_bytes * self.full_width_row_stride

    @property
    def matrix_dimension(self) -> int:
        return self.output_slot_size_bytes // self.full_width_row_bytes

    @property
    def output_reserved_bytes(self) -> int:
        return self.output_slot_count * self.output_slot_size_bytes

    @property
    def output_reserved_base(self) -> int:
        return self.base_address + self.size_bytes - self.output_reserved_bytes

    def output_slot_base(self, index: int) -> int:
        if index < 0 or index >= self.output_slot_count:
            raise IndexError(f"output slot index out of range: {index}")
        return self.output_reserved_base + index * self.output_slot_size_bytes

    def output_slot_row(self, index: int) -> int:
        return (self.output_slot_base(index) - self.base_address) // self.spad_row_bytes


def is_power_of_two(value: int) -> bool:
    return value > 0 and value & (value - 1) == 0


def require_mapping(
    mapping: Mapping[str, object], key: str, source: Path
) -> Mapping[str, object]:
    value = mapping.get(key)
    if not isinstance(value, Mapping):
        raise ValueError(f"{source}: missing mapping '{key}'")
    return value


def require_int(mapping: Mapping[str, object], key: str, source: Path) -> int:
    value = mapping.get(key)
    if not isinstance(value, int) or isinstance(value, bool):
        raise ValueError(f"{source}: '{key}' must be an integer")
    return value


def validate_contract(contract: ExternalSpadContract, source: Path) -> None:
    prefix = f"{source}: memory.gemmini_external_spad"
    if contract.base_address < 0:
        raise ValueError(f"{prefix}.base_address must be non-negative")
    if not is_power_of_two(contract.size_bytes):
        raise ValueError(f"{prefix}.size_bytes must be a positive power of two")
    if contract.base_address % contract.size_bytes != 0:
        raise ValueError(f"{prefix}.base_address must be size-aligned")
    if not is_power_of_two(contract.control_page_size_bytes):
        raise ValueError(
            f"{prefix}.control_page_size_bytes must be a positive power of two"
        )
    external_spad_end = contract.base_address + contract.size_bytes
    control_pages = (
        contract.production_control_address,
        contract.validation_telemetry_address,
    )
    if len(set(control_pages)) != len(control_pages):
        raise ValueError(f"{prefix} control pages must be distinct")
    for address in control_pages:
        if address % contract.control_page_size_bytes != 0:
            raise ValueError(f"{prefix} control pages must be page-aligned")
        if address < external_spad_end:
            raise ValueError(f"{prefix} control pages must not overlap the SPAD")
    if not is_power_of_two(contract.spad_row_bytes):
        raise ValueError(f"{prefix}.spad_row_bytes must be a positive power of two")
    if not is_power_of_two(contract.full_width_row_stride):
        raise ValueError(
            f"{prefix}.full_width_row_stride must be a positive power of two"
        )
    if contract.output_slot_count != 2:
        raise ValueError(f"{prefix}.output_slots.count must be exactly 2")
    if not is_power_of_two(contract.output_slot_size_bytes):
        raise ValueError(
            f"{prefix}.output_slots.size_bytes must be a positive power of two"
        )
    if contract.output_slot_size_bytes % contract.full_width_row_bytes != 0:
        raise ValueError(
            f"{prefix}.output_slots.size_bytes must contain whole full-width rows"
        )
    if contract.matrix_dimension <= 0:
        raise ValueError(f"{prefix} must describe a non-empty full-width matrix")
    if contract.output_reserved_bytes > contract.size_bytes:
        raise ValueError(f"{prefix}.output_slots exceed the external SPAD capacity")
    if contract.output_reserved_base % contract.output_slot_size_bytes != 0:
        raise ValueError(f"{prefix}.output_slots reservation must be slot-aligned")
    if contract.size_bytes % contract.spad_row_bytes != 0:
        raise ValueError(f"{prefix}.size_bytes must contain whole SPAD rows")
    if contract.size_bytes // contract.spad_row_bytes >= 1 << 31:
        raise ValueError(f"{prefix} has too many rows for the generated Scala Int")

    previous_end = contract.output_reserved_base
    for index in range(contract.output_slot_count):
        slot_base = contract.output_slot_base(index)
        slot_end = slot_base + contract.output_slot_size_bytes
        if slot_base != previous_end:
            raise ValueError(f"{prefix}.output_slots are not contiguous")
        if slot_base % contract.output_slot_size_bytes != 0:
            raise ValueError(f"{prefix}.output_slots[{index}] is not slot-aligned")
        if (slot_base - contract.base_address) % contract.spad_row_bytes != 0:
            raise ValueError(f"{prefix}.output_slots[{index}] is not row-aligned")
        if slot_end > contract.base_address + contract.size_bytes:
            raise ValueError(f"{prefix}.output_slots[{index}] exceeds capacity")
        previous_end = slot_end


def load_contract(path: Path) -> ExternalSpadContract:
    with path.open("r", encoding="utf-8") as stream:
        document = yaml.safe_load(stream)
    if not isinstance(document, Mapping):
        raise ValueError(f"{path}: YAML must contain a top-level mapping")

    memory = require_mapping(document, "memory", path)
    external_spad = require_mapping(memory, "gemmini_external_spad", path)
    output_slots = require_mapping(external_spad, "output_slots", path)
    contract = ExternalSpadContract(
        base_address=require_int(external_spad, "base_address", path),
        size_bytes=require_int(external_spad, "size_bytes", path),
        production_control_address=require_int(
            external_spad, "production_control_address", path
        ),
        validation_telemetry_address=require_int(
            external_spad, "validation_telemetry_address", path
        ),
        control_page_size_bytes=require_int(
            external_spad, "control_page_size_bytes", path
        ),
        spad_row_bytes=require_int(external_spad, "spad_row_bytes", path),
        full_width_row_stride=require_int(external_spad, "full_width_row_stride", path),
        output_slot_count=require_int(output_slots, "count", path),
        output_slot_size_bytes=require_int(output_slots, "size_bytes", path),
    )
    validate_contract(contract, path)
    return contract


def generate_scala(contract: ExternalSpadContract, source: Path) -> str:
    try:
        source_label = source.relative_to(ROOT)
    except ValueError:
        source_label = source
    slot_bases = ", ".join(
        f'BigInt("{contract.output_slot_base(index):x}", 16)'
        for index in range(contract.output_slot_count)
    )
    slot_rows = ", ".join(
        str(contract.output_slot_row(index))
        for index in range(contract.output_slot_count)
    )
    register_values = "\n".join(
        f"  val {name}: Int = 0x{offset:03x}"
        for name, offset in CONTROL_REGISTERS.items()
    )
    launch_status_values = "\n".join(
        f"  val LaunchStatus{name.title().replace('_', '')}: Int = {value}"
        for name, value in LAUNCH_STATUS.items()
    )
    compute_status_values = "\n".join(
        f"  val ComputeStatus{name.title().replace('_', '')}: Int = {value}"
        for name, value in COMPUTE_STATUS.items()
    )
    launch_error_operation_values = "\n".join(
        f"  val LaunchErrorOperation{name.title().replace('_', '')}: Int = {value}"
        for name, value in LAUNCH_ERROR_OPERATIONS.items()
    )
    launch_error_reason_values = "\n".join(
        f"  val LaunchErrorReason{name.title().replace('_', '')}: Int = {value}"
        for name, value in LAUNCH_ERROR_REASONS.items()
    )
    compute_error_operation_values = "\n".join(
        f"  val ComputeErrorOperation{name.title().replace('_', '')}: Int = {value}"
        for name, value in COMPUTE_ERROR_OPERATIONS.items()
    )
    compute_error_reason_values = "\n".join(
        f"  val ComputeErrorReason{name.title().replace('_', '')}: Int = {value}"
        for name, value in COMPUTE_ERROR_REASONS.items()
    )
    return f"""package chipyard.example

// Auto-generated by scripts/generate_gemmini_external_spad.py from {source_label}.
// This is an intentional versioned hardware/software interface; do not edit by hand.
object GemminiExternalSpadGenerated {{
  val baseAddress: BigInt = BigInt("{contract.base_address:x}", 16)
  val sizeBytes: Int = {contract.size_bytes}
  val productionControlAddress: BigInt = BigInt("{contract.production_control_address:x}", 16)
  val validationTelemetryAddress: BigInt = BigInt("{contract.validation_telemetry_address:x}", 16)
  val controlPageSizeBytes: Int = {contract.control_page_size_bytes}
  val spadRowBytes: Int = {contract.spad_row_bytes}
  val fullWidthRowStride: Int = {contract.full_width_row_stride}
  val fullWidthRowBytes: Int = {contract.full_width_row_bytes}
  val matrixDimension: Int = {contract.matrix_dimension}
  val outputSlotCount: Int = {contract.output_slot_count}
  val outputSlotSizeBytes: Int = {contract.output_slot_size_bytes}
  val outputReservedBytes: Int = {contract.output_reserved_bytes}
  val outputReservedBase: BigInt = BigInt("{contract.output_reserved_base:x}", 16)
  val outputSlotBases: Seq[BigInt] = Seq({slot_bases})
  val outputSlotRows: Seq[Int] = Seq({slot_rows})

  require(sizeBytes > 0 && (sizeBytes & (sizeBytes - 1)) == 0)
  require((baseAddress & (sizeBytes - 1)) == 0)
  require(controlPageSizeBytes > 0 &&
    (controlPageSizeBytes & (controlPageSizeBytes - 1)) == 0)
  require((productionControlAddress & (controlPageSizeBytes - 1)) == 0)
  require((validationTelemetryAddress & (controlPageSizeBytes - 1)) == 0)
  require(productionControlAddress != validationTelemetryAddress)
  require(productionControlAddress >= baseAddress + sizeBytes)
  require(validationTelemetryAddress >= baseAddress + sizeBytes)
  require(outputReservedBytes == outputSlotCount * outputSlotSizeBytes)
  require(outputReservedBase == baseAddress + sizeBytes - outputReservedBytes)
  require(outputSlotBases.size == outputSlotCount)
  require(outputSlotRows.size == outputSlotCount)
  require(outputSlotBases.zipWithIndex.forall {{ case (address, index) =>
    address == outputReservedBase + index * outputSlotSizeBytes
  }})
  require(outputSlotRows.zip(outputSlotBases).forall {{ case (row, address) =>
    baseAddress + row * spadRowBytes == address
  }})
}}

object CgraTransferControlGenerated {{
  val baseAddress: BigInt = GemminiExternalSpadGenerated.productionControlAddress
  val pageSizeBytes: Int = GemminiExternalSpadGenerated.controlPageSizeBytes
  private val ControlRegistersMaxOffset: Int = 0x{max(CONTROL_REGISTERS.values()):03x}
{register_values}
{launch_status_values}
{compute_status_values}
{launch_error_operation_values}
{launch_error_reason_values}
{compute_error_operation_values}
{compute_error_reason_values}

  require((ControlRegistersMaxOffset + 8) <= pageSizeBytes)
}}
"""


def generate_c_header(contract: ExternalSpadContract, source: Path) -> str:
    try:
        source_label = source.relative_to(ROOT)
    except ValueError:
        source_label = source
    lines = [
        "/*",
        " * Auto-generated by scripts/generate_gemmini_external_spad.py.",
        f" * Source: {source_label}",
        " * This is an intentional versioned hardware/software interface.",
        " * Do not edit by hand.",
        " */",
        "#ifndef GEMMINI_EXTERNAL_SPAD_H",
        "#define GEMMINI_EXTERNAL_SPAD_H",
        "",
        "#include <stdint.h>",
        "",
        f"#define GEMMINI_EXTERNAL_SPAD_BASE UINT64_C(0x{contract.base_address:x})",
        f"#define GEMMINI_EXTERNAL_SPAD_SIZE_BYTES {contract.size_bytes}",
        f"#define GEMMINI_EXTERNAL_SPAD_VALIDATION_TELEMETRY_BASE UINT64_C(0x{contract.validation_telemetry_address:x})",
        f"#define GEMMINI_EXTERNAL_SPAD_ROW_BYTES {contract.spad_row_bytes}",
        f"#define GEMMINI_EXTERNAL_SPAD_FULL_WIDTH_ROW_STRIDE {contract.full_width_row_stride}",
        f"#define GEMMINI_EXTERNAL_SPAD_FULL_WIDTH_ROW_BYTES {contract.full_width_row_bytes}",
        f"#define GEMMINI_EXTERNAL_SPAD_MATRIX_DIMENSION {contract.matrix_dimension}",
        f"#define GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT_COUNT {contract.output_slot_count}",
        f"#define GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT_SIZE_BYTES {contract.output_slot_size_bytes}",
        f"#define GEMMINI_EXTERNAL_SPAD_OUTPUT_RESERVED_BYTES {contract.output_reserved_bytes}",
        f"#define GEMMINI_EXTERNAL_SPAD_OUTPUT_RESERVED_BASE UINT64_C(0x{contract.output_reserved_base:x})",
        "",
    ]
    for index in range(contract.output_slot_count):
        lines.extend(
            [
                f"#define GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT{index}_BASE UINT64_C(0x{contract.output_slot_base(index):x})",
                f"#define GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT{index}_ROW {contract.output_slot_row(index)}",
            ]
        )
    lines.extend(["", "#endif", ""])
    return "\n".join(lines)


def generate_control_c_header(contract: ExternalSpadContract, source: Path) -> str:
    try:
        source_label = source.relative_to(ROOT)
    except ValueError:
        source_label = source
    lines = [
        "/*",
        " * Auto-generated by scripts/generate_gemmini_external_spad.py.",
        f" * Source: {source_label}",
        " * This is an intentional versioned production control ABI.",
        " * Do not edit by hand.",
        " */",
        "#ifndef CGRA_TRANSFER_CONTROL_GENERATED_H",
        "#define CGRA_TRANSFER_CONTROL_GENERATED_H",
        "",
        "#include <stdint.h>",
        "",
        f"#define CGRA_TRANSFER_CONTROL_BASE UINT64_C(0x{contract.production_control_address:x})",
        f"#define CGRA_TRANSFER_CONTROL_PAGE_SIZE_BYTES {contract.control_page_size_bytes}",
        "",
    ]
    for name, offset in CONTROL_REGISTERS.items():
        lines.append(f"#define CGRA_TRANSFER_CONTROL_{name} 0x{offset:03x}u")
    lines.append("")
    for name, value in LAUNCH_STATUS.items():
        lines.append(f"#define CGRA_TRANSFER_LAUNCH_STATUS_{name} {value}u")
    lines.append("")
    for name, value in COMPUTE_STATUS.items():
        lines.append(f"#define CGRA_TRANSFER_COMPUTE_STATUS_{name} {value}u")
    lines.append("")
    for name, value in LAUNCH_ERROR_OPERATIONS.items():
        lines.append(f"#define CGRA_TRANSFER_LAUNCH_ERROR_OPERATION_{name} {value}u")
    for name, value in LAUNCH_ERROR_REASONS.items():
        lines.append(f"#define CGRA_TRANSFER_LAUNCH_ERROR_REASON_{name} {value}u")
    lines.append("")
    for name, value in COMPUTE_ERROR_OPERATIONS.items():
        lines.append(f"#define CGRA_TRANSFER_COMPUTE_ERROR_OPERATION_{name} {value}u")
    for name, value in COMPUTE_ERROR_REASONS.items():
        lines.append(f"#define CGRA_TRANSFER_COMPUTE_ERROR_REASON_{name} {value}u")
    lines.extend(["", "#endif", ""])
    return "\n".join(lines)


def write_or_check(path: Path, content: str, check: bool) -> None:
    if check:
        if not path.exists() or path.read_text(encoding="utf-8") != content:
            raise ValueError(f"generated interface is stale: {path}")
        return
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--soc-yaml", type=Path, default=DEFAULT_SOC_YAML)
    parser.add_argument("--scala-out", type=Path, default=DEFAULT_SCALA_OUT)
    parser.add_argument("--c-header-out", type=Path, default=DEFAULT_C_HEADER_OUT)
    parser.add_argument(
        "--control-header-out", type=Path, default=DEFAULT_CONTROL_HEADER_OUT
    )
    parser.add_argument(
        "--check", action="store_true", help="fail if committed outputs are stale"
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    source = args.soc_yaml.resolve()
    contract = load_contract(source)
    write_or_check(args.scala_out, generate_scala(contract, source), args.check)
    write_or_check(args.c_header_out, generate_c_header(contract, source), args.check)
    write_or_check(
        args.control_header_out,
        generate_control_c_header(contract, source),
        args.check,
    )
    print(f"scala_out={args.scala_out}")
    print(f"c_header_out={args.c_header_out}")
    print(f"control_header_out={args.control_header_out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
