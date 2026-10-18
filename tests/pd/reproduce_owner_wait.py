"""Fixed barrier-controlled verification of OwnerChannel reply readiness."""
import datetime as dt
import json
import os
from pathlib import Path
import subprocess as sp
import sys
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/verification"))
import measure_pd0_method as base


def run():
    output = ROOT / "out/pd0-owner-wait" / (dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + uuid.uuid4().hex[:6])
    output.mkdir(parents=True)
    machine, source = base.machine(), base.identity()
    masks = {"default": machine["parent_affinity"], "single": machine["selected_affinity"]}
    binaries = {config: base.binary_info(ROOT / "out/build" / directory / "bin/cuexis_pd_host_probe.exe")
                for config, directory in (("release", "rpa-candidate-release"), ("debug", "candidate-debug"))}
    order = []
    for repeat in range(3):
        configs = list(binaries) if repeat % 2 == 0 else list(reversed(binaries))
        for config in configs:
            for affinity in (list(masks) if repeat % 2 == 0 else list(reversed(masks))):
                order.append({"repeat": repeat+1, "config": config, "affinity": affinity})
    manifest = {"revision": "pd0-owner-wait-barrier-v2-fixed", "source": source, "machine": machine,
        "binaries": binaries, "masks": masks, "registration": {
            "order": order, "timeouts_ms": [0, 1, 20, 100], "rows_per_process": 12,
            "scenarios": ["queued_command_without_reply", "in_flight_without_reply", "completed_between_poll_and_wait"],
            "completion_order": "worker taken -> host poll empty -> worker complete -> join -> waitFor",
            "verification": "completed reply wait returns with median <50ms for requested100ms; in-flight still times out",
            "control": "queued command uses command wait and returns with median <50ms for requested100ms",
            "no_sleep_filter_or_retry": True, "known_defect_characterization": False,
            "host_long_tail_attribution": "not proven by this isolated channel test"},
        "runs": [], "status": "registered", "pd0_exit": False}
    base.save(output / "manifest.json", manifest)
    print("EVIDENCE " + str(output), flush=True)
    try:
        for item in order:
            name = "{config}-{repeat}-{affinity}".format(**item)
            command = [sys.executable, "-B", str(ROOT / "tools/verification/measure_pd0_method.py"),
                "--child", str(masks[item["affinity"]]), binaries[item["config"]]["path"],
                "PD owner channel completed reply wait characterization", "--rng-seed", "1"]
            env = os.environ.copy()
            env.pop("CUEXIS_PD_METRICS", None)
            with (output / (name + ".log")).open("wb") as log:
                result = sp.run(command, cwd=ROOT, env=env, stdout=log, stderr=sp.STDOUT, timeout=30)
            lines = (output / (name + ".log")).read_text(encoding="utf-8", errors="replace").splitlines()
            rows = [json.loads(line[len("PD_WAIT "):]) for line in lines if line.startswith("PD_WAIT ")]
            if result.returncode or len(rows) != 12:
                raise RuntimeError(f"Incomplete/failed diagnosis {name}, exit={result.returncode}")
            expected = {(s, t) for s in manifest["registration"]["scenarios"] for t in [0, 1, 20, 100]}
            if {(r["scenario"], r["requested_ms"]) for r in rows} != expected:
                raise RuntimeError("Unexpected diagnosis cases")
            manifest["runs"].append(dict(item, name=name, command=command, exit_code=0, samples=rows))
            base.save(output / "manifest.json", manifest)
            print(name + ": exit=0", flush=True)
        summaries = []
        for config in binaries:
            for affinity in masks:
                for scenario in manifest["registration"]["scenarios"]:
                    for timeout in [0, 1, 20, 100]:
                        values = [r["elapsed_us"] for run in manifest["runs"] if run["config"] == config and run["affinity"] == affinity for r in run["samples"] if r["scenario"] == scenario and r["requested_ms"] == timeout]
                        summaries.append({"config": config, "affinity": affinity,
                            "scenario": scenario, "requested_ms": timeout, "statistics": base.stats(values)})
        controls_ok = all(s["statistics"]["median_us"] < 50000 for s in summaries if s["scenario"] in ("queued_command_without_reply", "completed_between_poll_and_wait") and s["requested_ms"] == 100)
        manifest.update(summaries=summaries, controls_ok=controls_ok, source_after=base.identity())
        manifest["source_unchanged"] = source["implementation_sha256"] == manifest["source_after"]["implementation_sha256"]
        manifest["binaries_unchanged"] = all(base.sha(b["path"]) == b["sha256"] for b in binaries.values())
        good = controls_ok and manifest["source_unchanged"] and manifest["binaries_unchanged"]
        manifest["status"] = "reply_wait_verified" if good else "inconclusive"
        base.save(output / "manifest.json", manifest)
        base.save(output / "evidence-hashes.json", {p.name: base.sha(p) for p in output.iterdir() if p.is_file()})
        print("STATUS " + manifest["status"], flush=True)
        return 0 if good else 1
    except Exception as error:
        manifest.update(status="stopped_error", error=str(error))
        base.save(output / "manifest.json", manifest)
        raise


if __name__ == "__main__":
    raise SystemExit(run())
