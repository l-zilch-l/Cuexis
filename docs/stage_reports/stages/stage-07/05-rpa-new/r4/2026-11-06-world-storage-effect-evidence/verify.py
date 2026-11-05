"""Recompute archived owning subsets and byte deltas; never execute a workload."""
import hashlib
import json
from pathlib import Path
import zipfile


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def rows(text, prefix):
    return [json.loads(line[len(prefix):]) for line in text.splitlines() if line.startswith(prefix)]


def reconstruct(text):
    globals_ = rows(text, "RPA_MEMORY ")
    partitions = rows(text, "RPA_MEMORY_PARTITION ")
    domains = rows(text, "RPA_MEMORY_DOMAIN ")
    require(len(globals_) == len(partitions) == 19, "checkpoint set differs")
    for global_, partition in zip(globals_, partitions):
        require(global_["exceeded"] is False and partition["exact"] is True and
                global_["label"] == partition["label"] and partition["untagged_live_count"] == 0,
                "invalid global partition")
        selected = [row for row in domains if row["label"] == global_["label"]]
        for key in ("live_count", "requested_cpp_bytes", "crt_usable_storage_bytes"):
            require(sum(row[key] for row in selected) == global_[key], "domain/global sum differs")
    proofs = rows(text, "RPA_MEMORY_WORLD_PROOF ")
    allocations = rows(text, "RPA_MEMORY_WORLD_ALLOCATION ")
    spans = rows(text, "RPA_MEMORY_WORLD_SPAN ")
    require([proof["phase"] for proof in proofs] == ["prepared", "runtime-installed", "active"],
            "true active transition missing")
    summaries = []
    for proof in proofs:
        require(proof["valid"] is True and proof["complete_world_graph"] is False and
                proof["bound_kind"] == "instantaneous_unique_lower_bound", "invalid proof")
        selected = [row for row in allocations if row["census_id"] == proof["census_id"]]
        require(len(selected) == len({row["allocation_id"] for row in selected}) == proof["live_count"],
                "allocation identities differ")
        total, stable, payload = 0, 0, 0
        for row in selected:
            intervals = []
            for span in spans:
                if (span["census_id"], span["allocation_id"]) == (proof["census_id"], row["allocation_id"]):
                    offset, size = span["offset"], span["bytes"]
                    require(0 <= offset <= row["requested_cpp_bytes"] and
                            0 < size <= row["requested_cpp_bytes"] - offset, "span exceeds backing")
                    intervals.append((offset, offset + size))
            end, union = 0, 0
            for start, stop in sorted(intervals):
                union += max(0, stop - max(start, end))
                end = max(end, stop)
            require(row["membership"] in {"stable_registry_subset", "actual_component_page"},
                    "unknown owning membership")
            charge = row["requested_cpp_bytes"] if row["membership"] == "stable_registry_subset" else union
            require(charge == row["charged_cpp_bytes"], "charged bytes differ from union")
            total += charge
            if row["membership"] == "stable_registry_subset":
                stable += charge
            else:
                payload += charge
        require(total == proof["lower_bound_requested_cpp_bytes"], "proof sum differs")
        for proof_key, field in (("backing_requested_cpp_bytes", "requested_cpp_bytes"),
                                 ("backing_crt_usable_storage_bytes", "crt_usable_storage_bytes")):
            require(proof[proof_key] == sum(row[field] for row in selected), "backing sum differs")
        summaries.append({"bytes": total, "stable": stable, "payload": payload})
    roots = rows(text, "RPA_MEMORY_SEMANTIC_ROOT ")
    closed = [row for row in roots if row["label"] == "after-close"]
    require(bool(closed) and all(row["phase"] == "released" and row["live_count"] == 0 and
            row["requested_cpp_bytes"] == row["crt_usable_storage_bytes"] == 0 and
            row["released_us"] >= row["retired_us"] > 0 for row in closed), "Close release differs")
    products = rows(text, "RPA_MEMORY_PRODUCT ")
    require(products[-1]["label"] == "after-fixture-release" and
            products[-1]["requested_cpp_bytes"] == products[-1]["crt_usable_storage_bytes"] == 0,
            "final product C++ new book is not zero")
    require("All tests passed (21 assertions in 1 test case)" in text, "correctness assertions differ")
    return summaries[-1]


def main():
    directory = Path(__file__).resolve().parent
    manifest = json.loads((directory / "manifest.json").read_text(encoding="utf-8"))
    archive_bytes = (directory / "raw.zip").read_bytes()
    require(digest(archive_bytes) == manifest["archive_sha256"], "ZIP hash differs")
    with zipfile.ZipFile(directory / "raw.zip") as bundle:
        names = bundle.namelist()
        require(len(names) == len(set(names)) and set(names) == set(manifest["files"]), "member set differs")
        for name, expected in manifest["files"].items():
            data = bundle.read(name)
            require(len(data) == expected["bytes"] and digest(data) == expected["sha256"], "member differs: " + name)
        def read(name):
            return json.loads(bundle.read(name))
        authorization = read("authorization-and-plan.json")
        registration = read("round-1/registration.json")
        disposition = read("final-disposition.json")
        require(registration["source_sha"] == authorization["source_sha"] == manifest["source_sha"] and
                registration["source_tree"] == authorization["source_tree"], "source identity differs")
        require(registration["maximum_processes"] == 1 and registration["timeout_seconds"] == 120 and
                registration["workload_environment"] == read("baseline/registration.json")["workload_environment"],
                "execution registration differs")
        for field, name in (("authorization", "authorization-and-plan.json"),
                            ("controller", "verify-effect.py"),
                            ("source", "build/source.json"),
                            ("build_binding", "build/cuexis_pd_host_probe-build-binding.json")):
            require(registration[field]["sha256"] == digest(bundle.read(name)), "registration binding differs")
        before_build, after_build = read("baseline/build-binding.json"), read("build/cuexis_pd_host_probe-build-binding.json")
        require(before_build["toolchain_signature"] == after_build["toolchain_signature"] and
                before_build["options"] == after_build["options"] and
                before_build["config"] == after_build["config"] == "Release", "builds not comparable")
        require(after_build["source_sha"] == registration["source_sha"] and
                after_build["binary"] == registration["binary"], "build/candidate binding differs")
        require(registration["budget"]["sha256"] == read("baseline/registration.json")["budget"]["sha256"],
                "budget changed")
        require(digest(bundle.read("budget.json")) == registration["budget"]["sha256"] and
                read("o1-acceptance.json")["accepted"] is True and
                read("o1-acceptance.json")["accepted_input_sha256"][
                    "tools/verification/execution_acceptance/budget-proposal.v1.json"] == registration["budget"]["sha256"],
                "accepted budget binding differs")
        profile = next(row for row in read("budget.json")["profiles"] if row["id"] == registration["profile"])
        require(next(row["requested_bytes"] for row in profile["domains"] if row["id"] == "state.world") == 327680,
                "original cap differs")
        for prefix in ("round-1", "baseline"):
            process = read(prefix + "/process-result.json")
            require(process["exit_code"] == 0 and not process["timed_out"], "process failed")
            for stream in ("stdout", "stderr"):
                data = bundle.read(prefix + "/" + stream + ".log")
                require(process[stream]["sha256"] == digest(data), "process stream binding differs")
        before = reconstruct(bundle.read("baseline/stdout.log").decode("utf-8"))
        after = reconstruct(bundle.read("round-1/stdout.log").decode("utf-8"))
        require(before == {"bytes": 358241, "stable": 145249, "payload": 212992} and
                after == {"bytes": 103738, "stable": 77114, "payload": 26624}, "reported byte comparison differs")
        comparison = read("round-1/comparison.json")
        require(comparison["saved_lower_bound_bytes"] == before["bytes"] - after["bytes"] == 254503,
                "saved byte count differs")
        require(read("round-1/analysis.json")["rating"]["status"] == "unrated" and
                disposition["capacity_processes_run"] == 1 and not disposition["second_capacity_process_run"] and
                disposition["formal_attempts_consumed"] == disposition["p8_attempts_consumed"] == 0 and
                not any(disposition[key] for key in ("budget_changed", "memory_admitted", "p8_admitted", "entities_40000_run")),
                "scope or admission differs")
        contracts = read("contracts/summary.json")
        require(contracts["source_sha"] == registration["source_sha"] and
                len(contracts["results"]) == 4 and all(row["exit_code"] == 0 for row in contracts["results"]),
                "allocator qualification differs")
        for row in contracts["results"]:
            require(row["log"]["sha256"] == digest(bundle.read("contracts/" + row["case"] + ".log")),
                    "allocator contract log differs")
    print(json.dumps({"verified_files": len(manifest["files"]), "before_bytes": before["bytes"],
        "after_bytes": after["bytes"], "saved_bytes": before["bytes"] - after["bytes"],
        "saved_percent": 100 * (before["bytes"] - after["bytes"]) / before["bytes"],
        "status": "unrated", "budget_changed": False, "native_processes_launched_by_verifier": 0}, indent=2))


if __name__ == "__main__":
    main()
