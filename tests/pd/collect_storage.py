"""Collect PD-0 requested-allocation and CPU delivery observations; never a timing gate."""
import datetime as dt
import argparse
import json
from pathlib import Path
import subprocess as sp
import sys
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "verification"))
import measure_pd0_method as method


def main(build_directories=None):
    output = ROOT / "out/pd0-storage-v1" / (dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + uuid.uuid4().hex[:6])
    output.mkdir(parents=True)
    source = method.identity()
    machine = method.machine()
    manifest = {"revision": "pd0-storage-v3", "source": source, "machine": machine,
                "registration": {"processes_per_config_profile": 3, "profiles": ["legacy", "rpa"],
                    "measurement": "C++ operator new requested bytes; excludes malloc/CRT/driver/GPU",
                    "timing": "allocation probe timings excluded from performance conclusions",
                    "ledger": "expected old defect at capacity+1, not a steady-state pass",
                    "quantiles": "same support rules as method-v1", "pd0_exit": False},
                "binaries": {}, "cold_binaries": {}, "runs": [], "status": "registered"}
    method.save(output / "manifest.json", manifest)
    print("EVIDENCE " + str(output), flush=True)
    try:
        directories = build_directories or {
            "release": ROOT / "out/build/rpa-candidate-release",
            "debug": ROOT / "out/build/candidate-debug"}
        for config, directory in directories.items():
            binary = Path(directory).resolve() / "bin/cuexis_pd_allocation_probe.exe"
            manifest["binaries"][config] = method.binary_info(binary)
            manifest["cold_binaries"][config] = method.binary_info(binary.with_name("cuexis_pd_baseline_probe.exe"))
            for profile in ("legacy", "rpa"):
                for repeat in range(1, 4):
                    stem = f"{config}-{profile}-{repeat}"
                    record = method.invoke(output, stem, binary, machine["selected_affinity"], profile, True)
                    text = (output / (stem + ".log")).read_text(encoding="utf-8")
                    record["memory"] = [json.loads(line[len("PD_MEMORY "):]) for line in text.splitlines()
                                        if line.startswith("PD_MEMORY ")]
                    if len(record["memory"]) != 18:
                        raise RuntimeError("Expected nine prepare and nine trace windows")
                    record.update(config=config, profile=profile)
                    manifest["runs"].append(record)
                    method.save(output / "manifest.json", manifest)
            for repeat in range(1, 4):
                stem = f"{config}-delivery-{repeat}"
                env = dict(method.os.environ, CUEXIS_PD_METRICS=str(output / stem))
                command = [sys.executable, str(ROOT / "tools/verification/measure_pd0_method.py"), "--child",
                           str(machine["selected_affinity"]), str(binary), "[pd][allocation],[pd][ledger],[pd][renderer]",
                           "--rng-seed", "1"]
                with (output / (stem + ".log")).open("wb") as log:
                    result = sp.run(command, cwd=ROOT, env=env, stdout=log, stderr=sp.STDOUT, timeout=120)
                if result.returncode:
                    raise RuntimeError(f"Delivery/self-check failed: {stem}")
                diagnostics = [json.loads(line) for p in output.glob(stem + ".*.jsonl")
                               for line in p.read_text().splitlines()]
                counts = {d["stage"]: len(d["exclusive_us"]) for d in diagnostics}
                # The renderer fixture installs one ordinary Runtime frame and extracts
                # one Snapshot before its repeated submit/present calls.
                if counts != {"renderer": 4096, "renderer_present": 4096, "receipt": 8200,
                              "ledger_take": 8200, "ledger_publish": 8204, "runtime": 1,
                              "runtime_prepare": 1, "runtime_commit": 1, "snapshot": 1}:
                    raise RuntimeError(f"Delivery observation coverage incomplete: {counts}")
                manifest["runs"].append({"config": config, "name": stem, "exit_code": 0,
                                         "command": command, "use": "allocation and logical baseline only"})
                method.save(output / "manifest.json", manifest)
                print(stem + ": exit=0", flush=True)
            for repeat in range(1, 4):
                stem = f"{config}-cold-projection-{repeat}"
                env = dict(method.os.environ, CUEXIS_PD_METRICS=str(output / stem))
                command = [sys.executable, "-B", str(ROOT / "tools/verification/measure_pd0_method.py"), "--child",
                           str(machine["selected_affinity"]), str(binary.with_name("cuexis_pd_baseline_probe.exe")),
                           "PD deterministic presentation baseline", "--rng-seed", "1"]
                with (output / (stem + ".log")).open("wb") as log:
                    result = sp.run(command, cwd=ROOT, env=env, stdout=log, stderr=sp.STDOUT, timeout=120)
                if result.returncode:
                    raise RuntimeError(f"Cold projection observation failed: {stem}")
                diagnostics = {d["stage"]: d for p in output.glob(stem + ".*.jsonl")
                               for line in p.read_text().splitlines() for d in [json.loads(line)]}
                if diagnostics["projection"]["items"] != 1:
                    raise RuntimeError("Expected one cold binding scan")
                manifest["runs"].append({"config": config, "name": stem, "exit_code": 0,
                                         "command": command, "use": "cold counters only; no allocation hook"})
                method.save(output / "manifest.json", manifest)
        for config in ("release", "debug"):
            for profile in ("legacy", "rpa"):
                windows = [w for r in manifest["runs"] if r.get("config") == config
                           and r.get("profile") == profile for w in r["memory"]
                           if w["phase"] == "trace" and w["recording"]]
                signatures = {(w["allocation_count"], w["requested_bytes"],
                               w["live_after"] - w["live_before"],
                               w["peak_live"] - w["live_before"]) for w in windows}
                if len(windows) != 24 or len(signatures) != 1:
                    raise RuntimeError(f"Requested-memory counts differ: {config}/{profile}")
        manifest["source_after"] = method.identity()
        if source["implementation_sha256"] != manifest["source_after"]["implementation_sha256"]:
            raise RuntimeError("Measurement implementation changed during collection")
        if not all(method.sha(b["path"]) == b["sha256"] for b in
                   list(manifest["binaries"].values()) + list(manifest["cold_binaries"].values())):
            raise RuntimeError("Binary changed during collection")
        manifest["status"] = "observed"
        method.save(output / "manifest.json", manifest)
        method.save(output / "sha256.json", {p.name: method.sha(p) for p in output.iterdir() if p.is_file()})
        print("STATUS observed", flush=True)
    except Exception as error:
        manifest.update(status="stopped_error", error=str(error))
        method.save(output / "manifest.json", manifest)
        raise


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--release-build', type=Path, default=ROOT / 'out/build/rpa-candidate-release')
    parser.add_argument('--debug-build', type=Path, default=ROOT / 'out/build/candidate-debug')
    args = parser.parse_args()
    main({'release': args.release_build, 'debug': args.debug_build})
