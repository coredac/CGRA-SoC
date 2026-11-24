"""Keep archived GCN cycles and account a separate shared-template area model."""

import csv
import hashlib
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
HW = HERE.parent
BASE = HW / "policy-cost"
AREAS = list(csv.DictReader((BASE / "area.csv").open()))
METHOD = json.loads((BASE / "method.json").read_text())
LAYERS = json.loads((HW / "layers.json").read_text())
PAIRS = list(csv.DictReader((HW.parent / "pairs.csv").open()))


def write_csv(name, rows):
    with (HERE / name).open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def main():
    areas, cycles, deductions, margins = [], [], [], []
    for n in (1, 2, 4):
        name = f"gcn-cgra{n}"
        rows = {r["mode"]: r for r in AREAS if r["configuration"] == name}
        adapters = {
            l["id"]
            for l in LAYERS["layers"]
            if l["configuration"] == name
            and l["ip"].startswith("cgra")
            and l["role"] == "link_adapter"
        }
        cuts = [
            c
            for c in METHOD["changes"]
            if c["layer_id"] in adapters and c["mode"] in ("Hybrid", "Auto")
        ]
        restored_patch = sum(c["removed_sram"] for c in cuts if c["mode"] == "Hybrid")
        shared_cut = sum(c["removed_sram"] for c in cuts if c["mode"] == "Auto")
        segment = float(rows["Hybrid"]["area"]) + restored_patch - shared_cut
        for mode, row in rows.items():
            proposal = mode == "Hybrid"
            area = segment if proposal else float(row["area"])
            areas.append(
                dict(
                    configuration=name,
                    cgras=n,
                    mode=mode,
                    policy=row["policy"],
                    prior_area=float(row["area"]),
                    accounted_area=area,
                    additional_binding_area=(
                        "unmeasured" if proposal else "not_applicable"
                    ),
                    basis=(
                        "proposed_shared_template_component_model"
                        if proposal
                        else "prior_component_area_model"
                    ),
                    actual_new_synthesis=False,
                )
            )
            pair = next(
                p
                for p in PAIRS
                if p["workload"] == "gcn-star64"
                and int(p["cgras"]) == n
                and p["mode"] == mode
            )
            cycles.append(
                dict(
                    configuration=name,
                    cgras=n,
                    mode=mode,
                    policy=row["policy"],
                    cycles=int(pair["warm_cycles"]),
                    source=pair["archived_log"],
                    source_sha256=pair["log_sha256"],
                    scope="archived_original_design_not_measurement_of_proposed_shared_template_segment",
                )
            )
        for cut in cuts:
            for memory in cut["memories"]:
                deductions.append(
                    dict(
                        configuration=name,
                        layer_id=cut["layer_id"],
                        source_policy=cut["mode"],
                        path=memory["path"],
                        macro=memory["macro"],
                        old_macros=memory["old_macros"],
                        new_macros=memory["new_macros"],
                        old_depth=memory["old_depth"],
                        new_depth=memory["new_depth"],
                    )
                )
        graph = float(rows["Auto"]["area"])
        margins.append(
            dict(
                configuration=name,
                segment_accounted_area=segment,
                graph_accounted_area=graph,
                unpriced_area_budget_before_equal=graph - segment,
                segment_sram_reduction=float(rows["Hybrid"]["area"]) - segment,
                task_segment_graph_order_on_accounted_components=float(
                    rows["Manual"]["area"]
                )
                < segment
                < graph,
            )
        )
    write_csv("area-model.csv", areas)
    write_csv("original-cycles.csv", cycles)
    write_csv("memory.csv", deductions)
    write_csv("margins.csv", margins)
    notes = {
        "status": "hypothetical_area_model_separate_from_archived_performance",
        "change": "Propose that per-segment and per-graph share one patched CGRA job-template set across the two GCN feature bands. Per-segment still uses CPU-controlled Gemmini and two segment launches.",
        "source_mechanism": "Original GCN capture(mode=1) creates two resolved job sets without patches; mode=2 creates one set with output-offset updates from TileId. New segment behavior needs correct per-band binding and cannot be assumed identical to the archived execution.",
        "interface_audit": "Frozen AutoTileCursor resets tile.id to zero on every start. Hybrid launches one tile per profile, so its second band cannot reuse Auto's patched template without a per-segment tile-origin binding. No tile-origin register is exposed in the audited interface. Existing patch arithmetic shares the packet-load stage; do not arbitrarily add one replay cycle per packet.",
        "accounting": "Start from the prior Hybrid component model. Undo its complete removal of CGRA patch SRAM, then apply the Auto half-depth packet AND patch SRAM deductions, including macro rounding. Keep Hybrid native paths, stage count, descriptors, all retained CGRA adapter logic and all compute/data SRAM.",
        "capacity": "Per-IP job slots:128->64,64->32,32->16 for1/2/4C. Packet/patch slots per job unchanged. This shares immutable templates; it does not delete the state needed by the next segment.",
        "unpriced": "Additional per-segment band binding, job-address selection or control changes, if required, have not been synthesized. Area values are accounted subtotals; margins.csv gives the remaining gap to graph area. Do not claim the ordering is validated for complete hardware.",
        "timing": "original-cycles.csv preserves all nine measured cycles exactly. No new shared-template segment latency is claimed. Patch replay and per-band binding may change cycles. Do not multiply this proposed segment area by old segment cycles and label the result measured AE.",
        "scope": "Inherited accelerator-component area scope and library units. No whole-SoC area or timing-closed PPA claim. Old synthesis SRAM/capacity variants differ from timing simulation.",
        "original_files_modified": False,
        "source_hashes": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [
                BASE / "area.csv",
                BASE / "method.json",
                HW / "layers.json",
                HW.parent / "pairs.csv",
                HERE / "estimate.py",
                HW.parents[4] / "build/experiments/p1/gcn/cgra1/v002/p1.c",
                HW.parents[4]
                / "build/experiments/p1/trim/saed-patch-v001/gcn-cgra1/autolink-ClockSinkDomain_4/src/AutoTileCursor.sv",
                HW.parents[4]
                / "build/experiments/p1/trim/saed-patch-v001/gcn-cgra1/cgra-CGRAAccelerator/src/CgraLinkAdapter.sv",
            ]
        },
    }
    (HERE / "method.json").write_text(json.dumps(notes, indent=2) + "\n")
    for x in margins:
        print(
            x["configuration"],
            "segment accounted",
            round(x["segment_accounted_area"] / 1e6, 6),
            "graph",
            round(x["graph_accounted_area"] / 1e6, 6),
            "unpriced area margin",
            round(x["unpriced_area_budget_before_equal"], 3),
        )


if __name__ == "__main__":
    main()
