"""Validate World subset proofs. A lower bound can prove failure, never acceptance."""
from .memory_domains import collect as collect_origins, records, unsigned


SPAN_KINDS = {"legacy_component_page", "transform_page", "hierarchy_page",
              "world_transform_page", "behavior_page", "renderable_page", "appearance_page",
              "camera_page", "component_entities", "world_object", "transform_cache",
              "transform_children", "transform_order", "material_string", "camera_string"}


def collect(text):
    for row in records(text, "RPA_MEMORY "):
        if row.get("exceeded") is not False:
            raise ValueError("sticky observer invalidity anywhere in log")
    for row in records(text, "RPA_MEMORY_PARTITION "):
        if row.get("exact") is not True:
            raise ValueError("inexact allocation partition anywhere in log")
    proofs = records(text, "RPA_MEMORY_WORLD_PROOF ")
    allocations = records(text, "RPA_MEMORY_WORLD_ALLOCATION ")
    spans = records(text, "RPA_MEMORY_WORLD_SPAN ")
    if not proofs:
        raise ValueError("missing instantaneous World proof")
    by_id = {}
    identities = {}
    for proof in proofs:
        census_id = unsigned(proof, "census_id")
        root_id = unsigned(proof, "root_id")
        if (not census_id or not root_id or census_id in by_id or
                proof.get("schema") != "rpa.execution.memory.world-proof.v1" or
                proof.get("valid") is not True or
                proof.get("complete_world_graph") is not False or
                proof.get("bound_kind") != "instantaneous_unique_lower_bound" or
                proof.get("domain") != "state.world" or
                proof.get("phase") not in {"prepared", "runtime-installed", "active"}):
            raise ValueError("invalid or duplicate World proof")
        rows = [row for row in allocations if row.get("census_id") == census_id]
        keys = [unsigned(row, "allocation_id") for row in rows]
        if (not all(keys) or len(keys) != len(set(keys)) or
                unsigned(proof, "live_count") != len(rows)):
            raise ValueError("missing or duplicate unique allocations")
        for row in rows:
            if (row.get("schema") != "rpa.execution.memory.world-allocation.v1" or
                    row.get("root_id") != root_id or row.get("membership") not in
                    {"actual_component_page", "actual_owned_span", "stable_registry_subset"}):
                raise ValueError("invalid allocation membership")
            requested = unsigned(row, "requested_cpp_bytes")
            storage = unsigned(row, "crt_usable_storage_bytes")
            origin = unsigned(row, "origin_cell")
            if requested > storage or origin >= 210:
                raise ValueError("invalid storage or origin identity")
            selected_spans = [part for part in spans if part.get("census_id") == census_id and
                              part.get("allocation_id") == row["allocation_id"]]
            intervals = []
            for part in selected_spans:
                if part.get("schema") != "rpa.execution.memory.world-span.v1":
                    raise ValueError("invalid page span schema")
                if part.get("kind", "legacy_component_page") not in SPAN_KINDS:
                    raise ValueError("unknown observed span kind")
                offset, size = unsigned(part, "offset"), unsigned(part, "bytes")
                if not size or offset > requested or size > requested - offset:
                    raise ValueError("page span leaves its backing new allocation")
                intervals.append((offset, offset + size))
            if row["membership"] == "actual_owned_span" and (
                    not selected_spans or any("kind" not in part for part in selected_spans)):
                raise ValueError("typed owned span requires explicit kind")
            end, union = 0, 0
            for start, stop in sorted(intervals):
                if stop > end:
                    union += stop - max(start, end)
                end = max(end, stop)
            charge = requested if row["membership"] == "stable_registry_subset" else union
            if unsigned(row, "charged_cpp_bytes") != charge or not charge:
                raise ValueError("page union or stable subset charge disagrees")
            identity = (requested, storage, origin)
            previous = identities.setdefault(row["allocation_id"], identity)
            if previous != identity:
                raise ValueError("allocation identity changed between censuses")
            unsigned(row, "age_us")
        for field, local in (("lower_bound_requested_cpp_bytes", "charged_cpp_bytes"),
                             ("backing_requested_cpp_bytes", "requested_cpp_bytes"),
                             ("backing_crt_usable_storage_bytes", "crt_usable_storage_bytes")):
            if unsigned(proof, field) != sum(unsigned(row, local) for row in rows):
                raise ValueError("World lower-bound total disagrees with allocation ledger")
        if unsigned(proof, "oldest_live_age_us") != max(
                (row["age_us"] for row in rows), default=0):
            raise ValueError("oldest allocation age disagrees")
        unsigned(proof, "stamp_us")
        unsigned(proof, "census_stack_bytes")
        by_id[census_id] = {"proof": proof, "allocations": rows,
                            "spans": [part for part in spans if part.get("census_id") == census_id]}
    if any(row.get("census_id") not in by_id for row in allocations):
        raise ValueError("orphan World allocation")
    keys = {(row["census_id"], row["allocation_id"]) for row in allocations}
    if any((part.get("census_id"), part.get("allocation_id")) not in keys for part in spans):
        raise ValueError("orphan page span")
    return {"schema": "rpa.execution.memory.world-proofs.v1", "censuses": list(by_id.values())}


def rate(document, profile):
    caps = [row for row in profile["domains"] if row["id"] == "state.world"]
    if (len(caps) != 1 or type(caps[0].get("requested_bytes")) is not int or
            caps[0]["requested_bytes"] < 0):
        raise ValueError("missing accepted World ceiling")
    limit = caps[0]["requested_bytes"]
    failures = []
    for row in document["censuses"]:
        proof = row["proof"]
        # Use a real active instant. Scope peaks can contain construction temporaries.
        if proof["phase"] == "active" and proof["lower_bound_requested_cpp_bytes"] > limit:
            failures.append({"census_id": proof["census_id"], "root_id": proof["root_id"],
                             "domain": "state.world", "lower_bound_bytes":
                             proof["lower_bound_requested_cpp_bytes"], "limit_bytes": limit,
                             "excess_at_least_bytes": proof["lower_bound_requested_cpp_bytes"] - limit})
    return {"schema": "rpa.execution.memory.semantic-rating.v1",
            "status": "proven_over_budget" if failures else "unrated", "failures": failures,
            "formal_acceptance": False, "complete_semantic_graph": False,
            "interpretation": "A unique exclusive subset at a real active instant is a lower bound. "
                              "Exceeding the unchanged ceiling proves failure; staying below it "
                              "cannot prove acceptance or an upper bound.",
            "remaining_unknown": ["complete World graph and semantic peaks",
                                  "current/candidate/scratch/shared retained roots",
                                  "prepare-only and legal owning handles",
                                  "native/GPU/driver accounting and observer perturbation"]}


def analyze_workflow(text, profile):
    origins = collect_origins(text)  # Whole-log range and exact global/partition rejection.
    checkpoints = {row["label"]: row for row in origins["checkpoints"]}
    for label in ("before-load", "active", "after-close", "after-fixture-release"):
        if label not in checkpoints:
            raise ValueError("missing workflow boundary: " + label)
    if checkpoints["after-fixture-release"]["product"]["requested_cpp_bytes"]:
        raise ValueError("product allocations remain after final legal release")
    roots = records(text, "RPA_MEMORY_SEMANTIC_ROOT ")
    blocks = records(text, "RPA_MEMORY_SEMANTIC_ALLOCATION ")
    root_keys = set()
    for root in roots:
        key = (root.get("label"), root.get("root_id"))
        if key in root_keys or key[0] not in checkpoints or root.get("valid") is not True:
            raise ValueError("invalid or duplicate root checkpoint")
        root_keys.add(key)
        selected = [row for row in blocks if (row.get("label"), row.get("root_id")) == key]
        ids = [unsigned(row, "allocation_id") for row in selected]
        if len(ids) != len(set(ids)) or len(ids) != unsigned(root, "live_count"):
            raise ValueError("root allocation identity/count mismatch")
        for field in ("requested_cpp_bytes", "crt_usable_storage_bytes"):
            if unsigned(root, field) != sum(unsigned(row, field) for row in selected):
                raise ValueError("root subset bytes disagree with unique allocation ledger")
        if root["label"] in {"after-close", "after-fixture-release"} and (
                root.get("phase") != "released" or root["live_count"] or
                not unsigned(root, "released_us") or
                root["released_us"] < unsigned(root, "retired_us")):
            raise ValueError("stable registry subset did not retire after Close")
    if not roots or any((row.get("label"), row.get("root_id")) not in root_keys for row in blocks):
        raise ValueError("missing roots or orphan subset allocation")
    document = collect(text)
    return {"proofs": document, "rating": rate(document, profile),
            "checkpoints": len(checkpoints), "root_snapshots": len(roots),
            "product_final_requested_bytes": 0,
            "release_scope": "tracked registry subset only; complete product C++ new book final zero"}
