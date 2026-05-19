#!/usr/bin/env python3
"""
Generate and sync a YAML-configured MeshMultiCgraTemplateRTL into Chipyard.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
VECTOR_RTL = ROOT / "VectorCGRA" / "MeshMultiCgraTemplateRTL_multi__pickled.v"
TOP_MODULE = "MeshMultiCgraTemplateRTL_multi"
PYTHON = ROOT / ".venv" / "bin" / "python"
PYTHON_EXE = str(PYTHON if PYTHON.exists() else Path(sys.executable))
DEFAULT_ARCH_YAML = ROOT / "configs" / "arch" / "multi_cgra_arch.yaml"
DEFAULT_SOC_YAML = ROOT / "configs" / "soc" / "multi_cgra_soc.yaml"


def resolve_input_path(path: str) -> Path:
  candidate = Path(path)
  if candidate.is_absolute():
    return candidate
  for base in (Path.cwd(), ROOT, ROOT / "VectorCGRA"):
    resolved = base / candidate
    if resolved.exists():
      return resolved
  return Path.cwd() / candidate


def run(cmd: list[str]) -> None:
  print(" ".join(cmd))
  subprocess.run(cmd, cwd=ROOT, check=True)


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
      "--arch-yaml",
      default=str(DEFAULT_ARCH_YAML),
      help=f"Multi-CGRA architecture YAML (default: {DEFAULT_ARCH_YAML.relative_to(ROOT)})",
  )
  parser.add_argument(
      "--soc-yaml",
      default=str(DEFAULT_SOC_YAML),
      help=f"Multi-CGRA SoC/interface YAML (default: {DEFAULT_SOC_YAML.relative_to(ROOT)})",
  )
  return parser.parse_args()


def main() -> int:
  args = parse_args()
  arch_yaml = resolve_input_path(args.arch_yaml)
  soc_yaml = resolve_input_path(args.soc_yaml)
  if not arch_yaml.exists():
    raise FileNotFoundError(f"arch yaml not found: {arch_yaml}")
  if not soc_yaml.exists():
    raise FileNotFoundError(f"soc yaml not found: {soc_yaml}")

  run([
      PYTHON_EXE,
      str(ROOT / "VectorCGRA" / "multi_cgra" / "test" / "MeshMultiCgraTemplateRTL_multi_test.py"),
      "--arch-yaml",
      str(arch_yaml),
      "--soc-yaml",
      str(soc_yaml),
      "--output",
      str(VECTOR_RTL),
      "--top-module",
      TOP_MODULE,
  ])

  run([
      PYTHON_EXE,
      str(ROOT / "scripts" / "sync_cgra_blackbox.py"),
      "--rtl",
      str(VECTOR_RTL),
      "--top-module",
      TOP_MODULE,
  ])

  return 0


if __name__ == "__main__":
  raise SystemExit(main())
