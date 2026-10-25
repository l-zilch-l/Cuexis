"""Validate Stage C functional evidence; this is not performance registration."""

import json
import re


STAGES = (
    "identity", "fixture-construction", "before-load-memory", "encode",
    "GameplayContent::fromPacked", "frame-budget", "host-create", "host-load",
    "host-advance", "host-query", "snapshot", "snapshot-encode", "snapshot-decode",
    "archive", "evaluateReplay-sameResult", "pause", "replayCut", "seek", "sample",
    "present", "seek-sameResult", "active-memory", "close", "after-close-memory",
)
EXPECTED_40K = {
    "low": (6818424, 6817880, 3725942,
            "78c23507481084ba7aa900efd56f72065992d8ad2409f657fcf194784252038d"),
    "high": (6746664, 6746120, 3654182,
             "e41eccde1593ca70b4b9c17696827f36b8884523d66e6c0ecb111a92ffc1cfcf"),
}


def validate(text, *, source_sha, reuse, entities):
    """Return observed records only after all required stages and claims agree."""
    def require(condition, message):
        if not condition:
            raise ValueError(message)

    rows = {}
    for line in text.splitlines():
        prefix, separator, body = line.partition(" ")
        if separator and (prefix.startswith("PD5_C_") or prefix in ("PD5_CAPACITY", "PD5_MEMORY")):
            rows.setdefault(prefix, []).append(json.loads(body))
    require(not rows.get("PD5_C_ERROR"), "reported Result failure")
    events = [(x["stage"], x["state"]) for x in rows.get("PD5_C_STAGE", [])]
    require(events == [(name, state) for name in STAGES for state in ("begin", "succeeded")],
            "missing, failed, duplicate or out-of-order stage")

    def one(prefix):
        require(len(rows.get(prefix, [])) == 1, "expected one " + prefix)
        return rows[prefix][0]

    identity, packed, summary = (one(key) for key in ("PD5_C_ID", "PD5_C_PACKED", "PD5_CAPACITY"))
    require(identity["source_sha"] == source_sha and len(source_sha) == 40, "source identity")
    require(identity["lifetime_observer"] is True, "memory checkpoint observer not compiled")
    require(identity["formal_measurement"] is False, "formal measurement not authorized")
    for record in (identity, packed, summary):
        require(record["entities"] == record["requirements"] == entities, "fixture count mismatch")
    require(identity["reuse"] == summary["reuse"] == reuse, "reuse mismatch")
    require(identity["revision"] == packed["revision"] == 4, "R4 not selected")
    require(packed["file_bytes"] == summary["packed_bytes"] <= 16777216, "file budget/count")
    require(packed["decoded_bytes"] <= 16777216 and packed["max_section_bytes"] <= 16777216,
            "decoded/section budget")
    require(packed["strings"] <= 100000 and packed["references"] <= 100000 and entities <= 40000,
            "physical table budgets")
    require(packed["four_hash_limits_accepted"] is True and packed["budgets_relaxed"] is False,
            "hash profile was not accepted unchanged")
    require(packed["semantic_bytes"] <= 134217728 and packed["scratch_peak"] <= 8388608,
            "hash byte/scratch budget")
    require(packed["ix_limit"] == 1000000 and packed["key_limit"] == 4096, "hash limits changed")
    require(packed["sha_computations"] == 1, "single semantic SHA")
    if entities == 40000:
        require((packed["file_bytes"], packed["decoded_bytes"], packed["max_section_bytes"],
                 packed["semantic_digest"]) == EXPECTED_40K[reuse], "Stage B wire identity/sizing")
        require(packed["semantic_bytes"] == 99588531 and packed["strings"] == 40048 and
                packed["references"] == 13, "Stage B count mismatch")
    for field in ("snapshot_valid", "decoded_snapshot_valid", "advance_executed",
                  "replay_evidence_valid", "replay_equal", "closed"):
        require(summary[field] is True, "false success claim: " + field)
    require(summary["formal_acceptance"] is False, "performance acceptance not authorized")
    require(summary["seek_start_us"] <= summary["seek_confirmed_us"] <= summary["seek_present_us"],
            "seek observation order")
    require(summary["seek_us"] == summary["seek_present_us"] - summary["seek_start_us"],
            "seek observation arithmetic")
    memory = rows.get("PD5_MEMORY", [])
    require([x["label"] for x in memory] == ["before-load", "active", "after-close"],
            "missing real memory checkpoint")
    require(all(x["allocator"] == "lifetime-observer" for x in memory), "memory observer identity")
    passed = re.search(r"All tests passed \((\d+) assertions in (\d+) test cases?\)", text)
    require(passed is not None and int(passed[2]) == 1, "Catch did not confirm one passing case")
    return {"identity": identity, "packed": packed, "summary": summary, "memory": memory,
            "assertions": int(passed[1]), "stages_completed": list(STAGES)}
