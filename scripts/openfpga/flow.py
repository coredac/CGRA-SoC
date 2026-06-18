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


_EXTRA_TEMPLATE_PATH_KEYS = {
    "openfpga_clock_arch_file",
    "openfpga_group_tile_config_file",
    "openfpga_pin_constraints_file",
    "yosys_adder_map_verilog",
    "yosys_bram_map_rules",
    "yosys_bram_map_verilog",
    "yosys_cell_sim_verilog",
    "yosys_dff_map_verilog",
    "yosys_dsp_map_verilog",
}


def _make_shell_template(
    src: Path,
    dst: Path,
    *,
    inject_route_chan_width: bool,
    inject_vpr_device_layout: bool,
    explicit_port_mapping: bool,
) -> Path:
    text = src.read_text(encoding="utf-8")
    if inject_route_chan_width or inject_vpr_device_layout:
        vpr_re = re.compile(
            r"^(?P<line>\s*vpr\s+\$\{VPR_ARCH_FILE\}\s+\$\{VPR_TESTBENCH_BLIF\}[^\n]*)(?P<newline>\n?)",
            flags=re.MULTILINE,
        )

        def add_vpr_options(match: re.Match[str]) -> str:
            line = match.group("line").rstrip()
            if inject_vpr_device_layout and "--device" not in line:
                line = f"{line} --device ${{OPENFPGA_VPR_DEVICE_LAYOUT}}"
            if inject_route_chan_width and "--route_chan_width" not in line:
                line = f"{line} --route_chan_width ${{ROUTE_CHAN_WIDTH}}"
            return line + match.group("newline")

        text, count = vpr_re.subn(add_vpr_options, text, count=1)
        if count != 1:
            raise ValueError(f"could not find a VPR command for option injection in {src}")

    if not re.search(r"^\s*write_fabric_bitstream\b", text, flags=re.MULTILINE):
        build_bitstream_re = re.compile(
            r"^(?P<indent>\s*)build_fabric_bitstream\b(?P<rest>[^\n]*)(?P<newline>\n?)",
            flags=re.MULTILINE,
        )

        def add_write_fabric_bitstream(match: re.Match[str]) -> str:
            newline = match.group("newline") or "\n"
            write_cmd = (
                f"{match.group('indent')}write_fabric_bitstream "
                "--file fabric_bitstream.bit --format plain_text"
            )
            return match.group(0) + write_cmd + newline

        text, count = build_bitstream_re.subn(add_write_fabric_bitstream, text, count=1)
        if count != 1:
            raise ValueError(f"could not find build_fabric_bitstream for bitstream output injection in {src}")

    if explicit_port_mapping:
        for command in (
            "write_fabric_verilog",
            "write_full_testbench",
            "write_preconfigured_fabric_wrapper",
            "write_preconfigured_testbench",
        ):
            text = _append_command_option(text, command, "--explicit_port_mapping")

    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text(text, encoding="utf-8")
    return dst


def _append_command_option(text: str, command: str, option: str) -> str:
    command_re = re.compile(
        rf"^(?P<line>\s*{re.escape(command)}\b[^\n]*)(?P<newline>\n?)",
        flags=re.MULTILINE,
    )

    def add_option(match: re.Match[str]) -> str:
        line = match.group("line").rstrip()
        if option not in line:
            line = f"{line} {option}"
        return line + match.group("newline")

    text, count = command_re.subn(add_option, text, count=1)
    if count != 1:
        raise ValueError(f"could not find {command} command for {option} injection")
    return text


def _required_app_file(root: Path, value: str | None, label: str) -> Path:
    if value is None:
        raise ValueError(f"{label} is required for this OpenFPGA flow")
    return ensure_file(resolve_under(root, value), label)


def _resolve_optional_file(root: Path, value: str | None, label: str) -> str | None:
    if value is None:
        return None
    return str(ensure_file(resolve_under(root, value), label))


def _resolve_semicolon_files(root: Path, values: tuple[str, ...], label: str) -> str | None:
    if not values:
        return None
    resolved = [str(ensure_file(resolve_under(root, value), label)) for value in values]
    return ";".join(resolved)


def _resolve_extra_template_var(root: Path, key: str, value: str) -> str:
    if key.lower() in _EXTRA_TEMPLATE_PATH_KEYS:
        return str(ensure_file(resolve_under(root, value), f"flow.extra_template_vars.{key}"))
    return value


def run_openfpga_flow(demo: DemoConfig) -> GeneratedPaths:
    root = demo.openfpga_root
    arch = demo.architecture
    app = demo.application
    flow = demo.flow

    vpr_arch = ensure_file(resolve_under(root, arch.vpr_arch), "VPR arch")
    openfpga_arch = ensure_file(resolve_under(root, arch.openfpga_arch), "OpenFPGA arch")
    sim_setting = ensure_file(resolve_under(root, arch.simulation_setting), "OpenFPGA simulation setting")
    shell_template = ensure_file(resolve_under(root, arch.shell_template), "OpenFPGA shell template")
    ref_verilog = ensure_file(resolve_under(root, app.benchmark_verilog), "reference Verilog")

    if flow.fpga_flow == "vpr_blif":
        benchmark_files = [_required_app_file(root, app.benchmark_blif, "BLIF")]
        activity_file = _required_app_file(root, app.benchmark_act, "activity file")
        base_verilog = ref_verilog
    elif flow.fpga_flow == "yosys_vpr":
        benchmark_files = [ref_verilog]
        activity_file = None
        base_verilog = None
    else:
        raise ValueError(f"unsupported OpenFPGA flow {flow.fpga_flow!r}")

    flow_py = ensure_file(root / "openfpga_flow" / "scripts" / "run_fpga_flow.py", "OpenFPGA flow script")
    openfpga_python = ensure_file(root / ".venv" / "bin" / "python", "OpenFPGA Python")

    template = _make_shell_template(
        shell_template,
        demo.workdir.parent / f"{demo.name}_route_chan_template.openfpga",
        inject_route_chan_width=flow.inject_route_chan_width,
        inject_vpr_device_layout="openfpga_vpr_device_layout" in flow.extra_template_vars,
        explicit_port_mapping=flow.explicit_port_mapping,
    )
    yosys_tmpl = _resolve_optional_file(root, flow.yosys_tmpl, "Yosys template")
    ys_rewrite_tmpl = _resolve_semicolon_files(root, flow.ys_rewrite_tmpl, "Yosys rewrite template")

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
        flow.fpga_flow,
        "--run_dir",
        str(demo.workdir),
        "--openfpga_shell_template",
        str(template),
        "--openfpga_arch_file",
        str(openfpga_arch),
        "--openfpga_sim_setting_file",
        str(sim_setting),
        str(vpr_arch),
        *[str(path) for path in benchmark_files],
        "--route_chan_width",
        str(app.chan_width),
        "--openfpga_vpr_route_chan_width",
        str(app.chan_width),
    ]
    if activity_file is not None:
        cmd.extend(["--activity_file", str(activity_file)])
    if base_verilog is not None:
        cmd.extend(["--base_verilog", str(base_verilog)])
    if yosys_tmpl is not None:
        cmd.extend(["--yosys_tmpl", yosys_tmpl])
    if ys_rewrite_tmpl is not None:
        cmd.extend(["--ys_rewrite_tmpl", ys_rewrite_tmpl])
    for flag in flow.extra_flags:
        cmd.append(f"--{flag}")
    for key, value in flow.extra_template_vars.items():
        cmd.extend([f"--{key}", _resolve_extra_template_var(root, key, value)])

    print("Running OpenFPGA flow:")
    print(" ".join(cmd), flush=True)
    subprocess.run(cmd, cwd=ROOT, env=env, check=True)

    src_dir = ensure_dir(demo.workdir / "SRC", "OpenFPGA generated SRC")
    ensure_file(src_dir / "fpga_top.v", "generated fpga_top")
    ensure_file(demo.workdir / "fabric_bitstream.bit", "generated fabric bitstream")
    return GeneratedPaths(workdir=demo.workdir, vsrc_dir=demo.vsrc_dir, src_dir=src_dir)
