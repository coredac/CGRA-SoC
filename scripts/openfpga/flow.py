#!/usr/bin/env python3
"""Run the OpenFPGA flow for a validated demo config."""

from __future__ import annotations

import os
import re
import subprocess
from dataclasses import dataclass
from pathlib import Path

from config import DemoConfig, ROOT, ensure_dir, ensure_file, resolve_under


@dataclass(frozen=True)
class GeneratedPaths:
    workdir: Path
    vsrc_dir: Path
    src_dir: Path


def _make_route_fixed_shell_template(src: Path, dst: Path, chan_width: int) -> Path:
    text = src.read_text(encoding="utf-8")
    replacement = (
        "vpr ${VPR_ARCH_FILE} ${VPR_TESTBENCH_BLIF} "
        "--clock_modeling route --route_chan_width ${ROUTE_CHAN_WIDTH}"
    )
    text, count = re.subn(
        r"^vpr\s+\$\{VPR_ARCH_FILE\}\s+\$\{VPR_TESTBENCH_BLIF\}\s+--clock_modeling\s+route\s*$",
        replacement,
        text,
        count=1,
        flags=re.MULTILINE,
    )
    if count != 1:
        raise ValueError(f"could not inject route_chan_width into {src}")
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text(text, encoding="utf-8")
    return dst


def run_openfpga_flow(demo: DemoConfig) -> GeneratedPaths:
    root = demo.openfpga_root
    arch = demo.architecture
    app = demo.application

    vpr_arch = ensure_file(resolve_under(root, arch.vpr_arch), "VPR arch")
    openfpga_arch = ensure_file(resolve_under(root, arch.openfpga_arch), "OpenFPGA arch")
    sim_setting = ensure_file(resolve_under(root, arch.simulation_setting), "OpenFPGA simulation setting")
    shell_template = ensure_file(resolve_under(root, arch.shell_template), "OpenFPGA shell template")
    blif = ensure_file(resolve_under(root, app.benchmark_blif), "BLIF")
    ref_verilog = ensure_file(resolve_under(root, app.benchmark_verilog), "reference Verilog")
    act = ensure_file(resolve_under(root, app.benchmark_act), "activity file")

    flow_py = ensure_file(root / "openfpga_flow" / "scripts" / "run_fpga_flow.py", "OpenFPGA flow script")
    openfpga_python = ensure_file(root / ".venv" / "bin" / "python", "OpenFPGA Python")

    template = _make_route_fixed_shell_template(
        shell_template,
        demo.workdir.parent / f"{demo.name}_route_chan_template.openfpga",
        app.chan_width,
    )

    env = os.environ.copy()
    env["PATH"] = f"{root / '.local' / 'bin'}:{env.get('PATH', '')}"
    env["LD_LIBRARY_PATH"] = f"{root / '.local' / 'lib'}:{env.get('LD_LIBRARY_PATH', '')}"
    env["BUILD_USING_CCACHE"] = "off"

    cmd = [
        str(openfpga_python),
        str(flow_py),
        "--top_module",
        app.top_module,
        "--fpga_flow",
        "vpr_blif",
        "--run_dir",
        str(demo.workdir),
        "--openfpga_shell_template",
        str(template),
        "--openfpga_arch_file",
        str(openfpga_arch),
        "--openfpga_sim_setting_file",
        str(sim_setting),
        "--activity_file",
        str(act),
        "--base_verilog",
        str(ref_verilog),
        str(vpr_arch),
        str(blif),
        "--route_chan_width",
        str(app.chan_width),
    ]

    print("Running OpenFPGA flow:")
    print(" ".join(cmd), flush=True)
    subprocess.run(cmd, cwd=ROOT, env=env, check=True)

    src_dir = ensure_dir(demo.workdir / "SRC", "OpenFPGA generated SRC")
    ensure_file(src_dir / "fpga_top.v", "generated fpga_top")
    ensure_file(demo.workdir / "fabric_bitstream.bit", "generated fabric bitstream")
    return GeneratedPaths(workdir=demo.workdir, vsrc_dir=demo.vsrc_dir, src_dir=src_dir)
