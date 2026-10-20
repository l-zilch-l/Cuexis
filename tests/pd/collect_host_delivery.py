"""Preregister the existing CPU ExecutionHost sample/present/receipt baseline."""
import argparse
import datetime as dt
import json
import math
import os
from pathlib import Path
import statistics as st
import subprocess as sp
import sys
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.verification.playback_performance import baseline_method as base


def invoke(output, name, binary, machine, active):
    env = os.environ.copy()
    env.pop("CUEXIS_PD_METRICS", None)
    if active:
        env["CUEXIS_PD_METRICS"] = str(output / name)
    command = [sys.executable, "-B", str(ROOT / "tools/verification/playback_performance/baseline_method.py"),
               "--child", str(machine["selected_affinity"]), str(binary),
               "PD host delivery and receipt acknowledgement baseline", "--rng-seed", "1"]
    with (output / (name + ".log")).open("wb") as log:
        completed = sp.run(command, cwd=ROOT, env=env, stdout=log, stderr=sp.STDOUT, timeout=120)
    lines = (output / (name + ".log")).read_text(encoding="utf-8", errors="replace").splitlines()
    parse = lambda prefix: [json.loads(line[len(prefix):]) for line in lines if line.startswith(prefix)]
    rows, identities = parse("PD_HOST_SAMPLE "), parse("PD_HOST_ID ")
    if completed.returncode or len(identities) != 1 or len(rows) != 512:
        raise RuntimeError(f"Host probe failed/incomplete: {name}, exit={completed.returncode}")
    if identities[0].get("receipt_mode") == "asynchronous":
        raise RuntimeError("PD4 present-return timing cannot be compared with the historical receipt-ack baseline")
    if [row["ordinal"] for row in rows] != list(range(512)) or identities[0]["recording"] != active:
        raise RuntimeError("Host sample order or observation mode differs")
    diagnostics = [json.loads(line) for p in output.glob(name + ".*.jsonl")
                   for line in p.read_text(encoding="utf-8").splitlines()]
    if any(d["omitted"] for d in diagnostics):
        raise RuntimeError("Host diagnostic coverage lost")
    for stage in ("receipt", "ledger_publish", "ledger_take"):
        count = sum(len(d["exclusive_us"]) for d in diagnostics if d["stage"] == stage)
        if count != (544 if active else 0):
            raise RuntimeError(f"Unexpected {stage} coverage: {count}")
    fields = ("request_to_ready_us", "ready_to_receipt_ack_us", "request_to_receipt_ack_us",
              "present_call_to_ack_us")
    if any(row[field] < 0 for row in rows for field in fields):
        raise RuntimeError("Negative host interval")
    record = {"name": name, "command": command, "exit_code": completed.returncode,
              "active": active, "identity": identities[0], "samples": rows,
              "statistics": {field: base.stats([row[field] for row in rows]) for field in fields},
              "diagnostics": [{**{k: v for k, v in d.items() if k != "exclusive_us"},
                               "statistics": base.stats(d["exclusive_us"])} for d in diagnostics]}
    base.save(output / (name + ".json"), record)
    print(name + ": exit=0", flush=True)
    return record


def run(args):
    output = ROOT / "out/pd0-host-delivery" / (args.phase + "-" +
        dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + uuid.uuid4().hex[:6])
    output.mkdir(parents=True)
    machine, source = base.machine(), base.identity()
    if args.affinity == "default":
        machine["selected_affinity"] = machine["parent_affinity"]
    binaries = {config: base.binary_info(ROOT / "out/build" / directory / "bin/cuexis_pd_host_probe.exe")
                for config, directory in (("release", "rpa-candidate-release"), ("debug", "candidate-debug"))}
    manifest = {"revision": "pd0-existing-host-delivery-v2-affinity", "phase": args.phase,
                "source": source, "machine": machine, "binaries": binaries, "runs": [],
                "registration": {"profile": "CPU TestPresentationRenderer, filesystem stage3_project, no Gameplay/audio/GPU",
                    "affinity": args.affinity,
                    "warmup": 32, "measured": 512, "frame_capacity": 1024,
                    "pairs": 1 if args.phase == "calibrate" else 3,
                    "order": [[False, True], [True, False], [False, True]],
                    "primary": "process mean request_to_receipt_ack_us, includes worker processing and polling",
                    "noise_limit": .15, "observer_relative_limit": .15,
                    "noise": "(max-min)/median of three independent process means per configuration/mode",
                    "precision": "receipt call timestamp is integer us; other boundaries steady_clock",
                    "quantiles": "p95 n>=200, p99 n>=1000; pooled values descriptive",
                    "no_filter_or_retry": True,
                    "not_steady_state": "544 retained old frames below 1024 capacity; old defect is separately characterized",
                    "no_new_delivery_protocol": True},
                "status": "registered", "pd0_exit": False}
    if args.phase == "accept":
        if args.calibration is None:
            raise ValueError("Separate calibration required")
        calibration = json.loads(args.calibration.read_text(encoding="utf-8"))
        if calibration["status"] != "calibration_applicable" or calibration["machine"] != machine or calibration["binaries"] != binaries or calibration["source"]["implementation_sha256"] != source["implementation_sha256"]:
            raise ValueError("Calibration identity or applicability differs")
        manifest["calibration"] = {"path": str(args.calibration.resolve()), "sha256": base.sha(args.calibration)}
    base.save(output / "manifest.json", manifest)
    print("EVIDENCE " + str(output), flush=True)
    try:
        for pair, order in enumerate(manifest["registration"]["order"][:1 if args.phase == "calibrate" else 3]):
            for config in (("release", "debug") if pair % 2 == 0 else ("debug", "release")):
                for active in order:
                    record = invoke(output, f"{config}-{pair+1}-{'active' if active else 'dormant'}",
                                    binaries[config]["path"], machine, active)
                    record.update(config=config, pair=pair+1)
                    manifest["runs"].append(record)
                    base.save(output / "manifest.json", manifest)
        result, effects = [], []
        for config in binaries:
            pairs = []
            for active in (False, True):
                runs = [r for r in manifest["runs"] if r["config"] == config and r["active"] == active]
                means = [r["statistics"]["request_to_receipt_ack_us"]["mean_us"] for r in runs]
                n = base.noise(means) if len(means) >= 3 else None
                result.append({"config": config, "active": active, "process_means_us": means,
                               "noise": n, "pass": n is None or n <= .15,
                               "pooled": {field: base.stats([x[field] for r in runs for x in r["samples"]])
                                          for field in runs[0]["statistics"]}})
                pairs.append(means)
            effect = math.expm1(st.mean(math.log(a/d) for d, a in zip(*pairs)))
            effects.append({"config": config, "effect": effect, "pass": effect <= .15})
        signatures = [{k: r["identity"][k] for k in ("digest", "warmup", "measured", "frame_capacity",
                       "frames_published", "successful_receipts", "source", "viewport", "objects", "audio", "gpu", "closed")}
                      for r in manifest["runs"]]
        if any(s != signatures[0] for s in signatures):
            raise RuntimeError("Host semantic identities differ")
        manifest.update(results=result, observer_effects=effects, source_after=base.identity())
        manifest["source_unchanged"] = source["implementation_sha256"] == manifest["source_after"]["implementation_sha256"]
        manifest["binaries_unchanged"] = all(base.sha(b["path"]) == b["sha256"] for b in binaries.values())
        good = all(r["pass"] for r in result + effects) and manifest["source_unchanged"] and manifest["binaries_unchanged"]
        manifest["status"] = ("calibration_applicable" if args.phase == "calibrate" else "baseline_validated") if good else "inconclusive"
        base.save(output / "manifest.json", manifest)
        base.save(output / "evidence-hashes.json", {p.name: base.sha(p) for p in output.iterdir() if p.is_file()})
        print("STATUS " + manifest["status"], flush=True)
        return 0 if good else 1
    except Exception as error:
        manifest.update(status="stopped_error", error=str(error))
        base.save(output / "manifest.json", manifest)
        raise


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("phase", choices=("calibrate", "accept"))
    parser.add_argument("--affinity", choices=("single", "default"), default="single")
    parser.add_argument("--calibration", type=Path)
    raise SystemExit(run(parser.parse_args()))
