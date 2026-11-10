"""One fixed, counterbalanced host affinity diagnostic; no adaptive repetitions."""
import datetime as dt
import json
import math
from pathlib import Path
import statistics as st
import uuid

import collect_host_delivery as host

base, ROOT = host.base, host.ROOT


def run():
    output = ROOT / "out/pd0-host-affinity" / (dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + uuid.uuid4().hex[:6])
    output.mkdir(parents=True)
    machine, source = base.machine(), base.identity()
    masks = {"default": machine["parent_affinity"], "single": machine["selected_affinity"]}
    if masks["default"] == masks["single"]:
        raise ValueError("No distinct default/single affinity masks available")
    binaries = {config: base.binary_info(ROOT / "out/build" / directory / "bin/cuexis_pd_host_probe.exe")
                for config, directory in (("release", "rpa-candidate-release"), ("debug", "candidate-debug"))}
    combinations = [(config, active) for config in binaries for active in (False, True)]
    order = []
    for repeat in range(3):
        rotated = combinations[repeat:] + combinations[:repeat]
        for position, (config, active) in enumerate(rotated):
            profiles = ("default", "single") if (position + repeat) % 2 == 0 else ("single", "default")
            for affinity in profiles:
                order.append({"repeat": repeat+1, "config": config, "active": active, "affinity": affinity})
    manifest = {"revision": "pd0-host-affinity-comparison-v1", "source": source,
                "machine": machine, "masks": masks, "binaries": binaries, "runs": [],
                "registration": {"processes": 24, "order": order, "warmup": 32, "measured": 512,
                    "profile": "unchanged host probe; CPU renderer; no Gameplay/audio/GPU",
                    "primary": "all-sample process mean request_to_receipt_ack_us",
                    "noise_limit": .15, "observer_limit": .15,
                    "noise": "(max-min)/median of 3 process means for each config/mode/mask",
                    "no_filter_or_retry": True,
                    "diagnosis_only": "no automatic baseline acceptance or PD-0 exit; independent calibration/acceptance follows if applicable",
                    "default_mask": "parent allowed processor mask, no independent per-thread core assignment",
                    "unchanged": "binary, workload/history length, priority, power, waiting and receipt implementation"},
                "status": "registered", "pd0_exit": False}
    base.save(output / "manifest.json", manifest)
    print("EVIDENCE " + str(output), flush=True)
    try:
        for item in order:
            name = "{config}-{repeat}-{affinity}-".format(**item) + ("active" if item["active"] else "dormant")
            settings = dict(machine, selected_affinity=masks[item["affinity"]])
            record = host.invoke(output, name, binaries[item["config"]]["path"], settings, item["active"])
            record.update(item)
            manifest["runs"].append(record)
            base.save(output / "manifest.json", manifest)
        results, effects = [], []
        for affinity in masks:
            for config in binaries:
                paired = []
                for active in (False, True):
                    runs = [r for r in manifest["runs"] if r["affinity"] == affinity and r["config"] == config and r["active"] == active]
                    means = [r["statistics"]["request_to_receipt_ack_us"]["mean_us"] for r in runs]
                    noise = base.noise(means)
                    results.append({"affinity": affinity, "config": config, "active": active,
                        "process_means_us": means, "noise": noise, "within_noise_limit": noise <= .15,
                        "pump_counts": [r["identity"]["pump_count"] for r in runs],
                        "pooled": {field: base.stats([x[field] for r in runs for x in r["samples"]])
                                   for field in runs[0]["statistics"]}})
                    paired.append(means)
                effect = math.expm1(st.mean(math.log(a/d) for d, a in zip(*paired)))
                effects.append({"affinity": affinity, "config": config, "effect": effect, "within_limit": effect <= .15})
        signatures = [{k: r["identity"][k] for k in ("digest", "warmup", "measured", "frame_capacity", "frames_published", "successful_receipts", "source", "viewport", "objects", "audio", "gpu", "closed")} for r in manifest["runs"]]
        if any(s != signatures[0] for s in signatures):
            raise RuntimeError("Semantic identities differ across affinity/configuration/mode")
        manifest.update(results=results, observer_effects=effects, source_after=base.identity())
        manifest["source_unchanged"] = source["implementation_sha256"] == manifest["source_after"]["implementation_sha256"]
        manifest["binaries_unchanged"] = all(base.sha(b["path"]) == b["sha256"] for b in binaries.values())
        if not manifest["source_unchanged"] or not manifest["binaries_unchanged"]:
            raise RuntimeError("Implementation/binary changed during comparison")
        manifest["profile_applicability"] = {affinity: all(r["within_noise_limit"] for r in results if r["affinity"] == affinity) and all(r["within_limit"] for r in effects if r["affinity"] == affinity) for affinity in masks}
        manifest["status"] = "diagnostic_complete"
        base.save(output / "manifest.json", manifest)
        base.save(output / "evidence-hashes.json", {p.name: base.sha(p) for p in output.iterdir() if p.is_file()})
        print("APPLICABILITY " + json.dumps(manifest["profile_applicability"]), flush=True)
        return 0
    except Exception as error:
        manifest.update(status="stopped_error", error=str(error))
        base.save(output / "manifest.json", manifest)
        raise


if __name__ == "__main__":
    raise SystemExit(run())
