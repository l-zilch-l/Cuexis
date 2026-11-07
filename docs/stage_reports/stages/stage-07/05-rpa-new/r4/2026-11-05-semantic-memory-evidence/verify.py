"""Verify archived bytes and independently reconstruct the bounded World failure; no workload."""
import hashlib
import json
from pathlib import Path
import zipfile


def digest(data):
    return hashlib.sha256(data).hexdigest()


def rows(text, prefix):
    return [json.loads(line[len(prefix):]) for line in text.splitlines() if line.startswith(prefix)]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def main():
    base = Path(__file__).resolve().parent
    manifest = json.loads((base / "manifest.json").read_text(encoding="utf-8"))
    archive = base / "raw.zip"
    require(digest(archive.read_bytes()) == manifest["archive_sha256"], "archive hash mismatch")
    with zipfile.ZipFile(archive) as bundle:
        names = bundle.namelist()
        require(len(names) == len(set(names)) and set(names) == set(manifest["files"]),
                "archive member set mismatch")
        for name, expected in manifest["files"].items():
            data = bundle.read(name)
            require(len(data) == expected["bytes"] and digest(data) == expected["sha256"],
                    "member hash/size mismatch: " + name)

        def read(name):
            return json.loads(bundle.read(name))

        authorization = read("owner-authorization.json")
        require(authorization["maximum_rounds"] == 2 and authorization["owner_choice"] == "A" and
                authorization["maximum_processes_per_round"] == 1 and
                authorization["timeout_seconds"] == 120 and not authorization["p8_authorized"],
                "owner authorization differs")
        disposition = read("final-batch-disposition.json")
        require(disposition["rounds_consumed"] == 2 and disposition["remaining_rounds"] == 0 and
                disposition["formal_attempts_consumed"] == disposition["p8_attempts_consumed"] == 0 and
                not any(disposition[key] for key in ("budget_changed", "memory_admitted",
                                                     "p8_admitted", "entities_40000_run")),
                "stop boundary differs")
        for number in (1, 2):
            registration = read(f"round-{number}/registration.json")
            result = read(f"round-{number}/process-result.json")
            require(registration["maximum_processes"] == 1 and registration["timeout_seconds"] == 120 and
                    registration["round"] == number and not registration["formal_sampling"] and
                    not registration["p8"] and registration["workload_environment"] == {
                        "CUEXIS_PD5_ENTITIES": "64", "CUEXIS_PD5_REUSE": "high",
                        "CUEXIS_PD5_CAPSULE_REVISION": "3"}, "workload changed")
            require(result["exit_code"] == 0 and not result["timed_out"], "process did not finish")
            for stream in ("stdout", "stderr"):
                require(digest(bundle.read(f"round-{number}/{stream}.log")) == result[stream]["sha256"],
                        "original process log binding differs")
            require(registration["budget"]["sha256"] == authorization["budget_binding"]["sha256"],
                    "budget changed")
        require(read("round-1/tool-invalid.json")["formal_acceptance"] is False,
                "invalid first round erased")
        text = bundle.read("round-2/stdout.log").decode("utf-8")
        globals_ = rows(text, "RPA_MEMORY ")
        partitions = rows(text, "RPA_MEMORY_PARTITION ")
        domains = rows(text, "RPA_MEMORY_DOMAIN ")
        require(len(globals_) == len(partitions) == 19, "checkpoint coverage differs")
        for global_, partition in zip(globals_, partitions):
            label = global_["label"]
            require(global_["exceeded"] is False and partition["exact"] is True and
                    partition["label"] == label and partition["untagged_live_count"] == 0,
                    "invalid global/partition")
            cells = [row for row in domains if row["label"] == label]
            for key in ("live_count", "requested_cpp_bytes", "crt_usable_storage_bytes"):
                require(sum(row[key] for row in cells) == global_[key], "global/domain sum mismatch")
        proofs = rows(text, "RPA_MEMORY_WORLD_PROOF ")
        allocations = rows(text, "RPA_MEMORY_WORLD_ALLOCATION ")
        spans = rows(text, "RPA_MEMORY_WORLD_SPAN ")
        require([p["phase"] for p in proofs] == ["prepared", "runtime-installed", "active"],
                "true active transition missing")
        for proof in proofs:
            require(proof["valid"] is True and proof["complete_world_graph"] is False,
                    "invalid semantic proof")
            census_id = proof["census_id"]
            selected = [row for row in allocations if row["census_id"] == census_id]
            require(len(selected) == len({r["allocation_id"] for r in selected}) == 25,
                    "unique allocation count differs")
            total = 0
            for allocation in selected:
                requested = allocation["requested_cpp_bytes"]
                intervals = []
                for span in spans:
                    if (span["census_id"], span["allocation_id"]) == (census_id, allocation["allocation_id"]):
                        offset, size = span["offset"], span["bytes"]
                        require(0 <= offset <= requested and 0 < size <= requested - offset,
                                "span leaves new allocation")
                        intervals.append((offset, offset + size))
                union, end = 0, 0
                for start, stop in sorted(intervals):
                    if stop > end:
                        union += stop - max(end, start)
                    end = max(end, stop)
                charge = requested if allocation["membership"] == "stable_registry_subset" else union
                require(charge == allocation["charged_cpp_bytes"], "payload union mismatch")
                total += charge
            require(total == proof["lower_bound_requested_cpp_bytes"] == 358241,
                    "semantic lower-bound total differs")
        active = proofs[-1]
        require(active["root_id"] == 1 and active["lower_bound_requested_cpp_bytes"] - 327680 == 30561,
                "active failure differs")
        roots = rows(text, "RPA_MEMORY_SEMANTIC_ROOT ")
        closed = next(row for row in roots if row["label"] == "after-close")
        require(closed["phase"] == "released" and closed["live_count"] == 0 and
                closed["requested_cpp_bytes"] == closed["crt_usable_storage_bytes"] == 0 and
                closed["released_us"] >= closed["retired_us"] > 0, "subset not retired")
        final = rows(text, "RPA_MEMORY_PRODUCT ")[-1]
        require(final["label"] == "after-fixture-release" and
                final["requested_cpp_bytes"] == final["crt_usable_storage_bytes"] == 0,
                "final product C++ book not zero")
        require("All tests passed (21 assertions in 1 test case)" in text, "functional checks differ")
    print(json.dumps({"verified_files": len(manifest["files"]), "status": "proven_over_budget",
                      "lower_bound_bytes": 358241, "limit_bytes": 327680,
                      "excess_at_least_bytes": 30561, "formal_acceptance": False,
                      "native_processes_launched_by_verifier": 0}, indent=2))


if __name__ == "__main__":
    main()
