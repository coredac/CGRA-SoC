"""Extract the complete trial compute core with explicit external memory ports."""

import hashlib
import json
import re
from collections import Counter
from pathlib import Path


def digest(text):
    return hashlib.sha256(text.encode()).hexdigest()


def instances(text, modules):
    return [
        (module, instance)
        for module, instance in re.findall(r"(?m)^\s*(\w+)\s+(\w+)\s*\(", text)
        if module in modules
    ]


def main():
    output = Path(__file__).resolve().parent
    source = output.parents[2] / "control-trial/IntegratedTrial.sv"
    text = source.read_text()
    matches = list(re.finditer(r"^module\s+(\w+)\b.*?^endmodule\b", text, re.M | re.S))
    modules = {match[1]: match[0] for match in matches}
    original = modules["CgraTemplateRTL_single_dma"]
    memory_name = "DataMemControllerRTL__593f9c3d20c85a7d"
    memory = modules[memory_name]
    declaration = re.search(rf"(?m)^  {memory_name} data_mem\s*\(.*?\n  \);", original, re.S)
    connections = dict(re.findall(r"\.(\w+)\(\s*(\w+)\s*\)", declaration[0]))
    header, body = original.split(");", 1)
    body = body.replace(declaration[0], "")
    ports = []
    removed_declarations = []
    for direction, dtype, name, dimensions in re.findall(
        r"(?m)^\s*(input|output)\s+(.+?)\s+(\w+)\s*(\[[^\n,]+\])?\s*,?\s*$",
        memory[: memory.index(");")],
    ):
        signal = connections[name]
        promoted = "output" if direction == "input" else "input"
        port = f"  {promoted} {dtype.strip()} {signal} {dimensions or ''}".rstrip()
        ports.append({"name": signal, "memory_port": name, "direction": promoted, "type": dtype.strip(), "dimensions": dimensions or "", "declaration": port.strip()})
        pattern = rf"(?m)^  {re.escape(dtype.strip())}\s+{signal}\s*[^\n;]*;\n"
        found = re.findall(pattern, body)
        if len(found) != 1:
            return f"Expected one internal declaration for {signal}, found {len(found)}"
        removed_declarations.extend(found)
        body = re.sub(pattern, "", body)
    if len(ports) != len(connections):
        return "Memory port parsing did not cover all named connections"
    new_header = header.replace("module CgraTemplateRTL_single_dma", "module CgraCompute", 1).rstrip()
    new_header += ",\n" + ",\n".join(port["declaration"] for port in ports) + "\n"
    compute = new_header + ");" + body
    modules["CgraCompute"] = compute
    needed = {"CgraCompute"}
    pending = ["CgraCompute"]
    while pending:
        for module, _ in instances(modules[pending.pop()], modules):
            if module not in needed:
                needed.add(module)
                pending.append(module)
    selected = [match[0] for match in matches if match[1] in needed]
    result = text[: matches[0].start()] + "\n\n".join(selected + [compute]) + "\n"
    hierarchy = Counter()
    pending = ["CgraCompute"]
    while pending:
        module = pending.pop()
        hierarchy[module] += 1
        pending.extend(child for child, _ in instances(modules[module], modules))
    original_instances = instances(original, modules)
    new_instances = instances(compute, modules)
    expected = [(module, instance) for module, instance in original_instances if instance != "data_mem"]
    comparison_body = original.split(");", 1)[1].replace(declaration[0], "")
    for line in removed_declarations:
        comparison_body = comparison_body.replace(line, "")
    metadata = {
        "source": str(source.relative_to(Path.cwd())),
        "source_sha256": digest(text),
        "top": "CgraCompute",
        "output_sha256": digest(result),
        "boundary": "Complete modified CGRA with only external data_mem instance removed; its signals become explicit top ports. DMA shell was outside the selected original core. Local arrays, native access logic, all PE routing and control remain.",
        "clock_reset_boundary": "Memory clock/reset and address/id inputs are exported as driven output ports together with every other removed-instance input; no clock/reset behavior or constraints are changed by this extraction.",
        "memory_ports": ports,
        "selected_modules": sorted(needed),
        "recursive_module_instances": dict(sorted(hierarchy.items())),
        "retained_module_hashes": {name: digest(modules[name]) for name in sorted(needed) if name != "CgraCompute"},
        "top_instances": new_instances,
        "checks": {
            "all_memory_connections_promoted": len(ports) == len(connections),
            "remaining_top_body_identical": body == comparison_body,
            "remaining_instances_identical": new_instances == expected,
            "tile_instances": sum(instance.startswith("tile__") for _, instance in new_instances),
            "data_mem_module_excluded": memory_name not in needed,
            "dma_shell_excluded": "IntegratedCgraWithDmaRTL_single" not in needed,
        },
    }
    (output / "CgraCompute.sv").write_text(result)
    (output / "boundary.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(json.dumps({"top": metadata["top"], "checks": metadata["checks"], "memory_ports": len(ports), "modules": len(needed)}, indent=2))


if __name__ == "__main__":
    import sys

    sys.exit(main())
