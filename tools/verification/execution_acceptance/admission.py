"""Fail-closed technical admission and single-use execution claims; no workloads launched."""
import re
from pathlib import Path

from .binding import accepted_budget, read, sha256, verify_artifact, verify_source, write_once
from .registration import validate_draft
from . import build_binding, probe_units


PRE_PD = "d344088236ae589c0b49a4994ef0fbddd6567ce2"
ORDERS = ["AB/BA/AB", "BA/AB/BA", "AB/BA/AB"]


def unit_id(value):
    if not isinstance(value, str) or not re.fullmatch(r"[a-z0-9][a-z0-9._-]{0,95}", value):
        raise ValueError("unsafe or missing stable unit id")
    return value


def stable_id(unit):
    """The protocol name and exact workload semantics survive candidate/unit renames."""
    workload = unit["workload"]
    config = unit.get("config")
    if config not in ("Release", "Debug"):
        raise ValueError("explicit protocol configuration required")
    if workload == "seek":
        if unit.get("entities") not in (64, 2048, 40000):
            raise ValueError("unknown Seek scale")
        expected = f"seek-{unit['seek_class']}-{unit['entities']}-{config.lower()}"
    elif workload in ("memory", "p8"):
        entities, reuse = unit.get("entities"), unit.get("reuse")
        if (entities not in (64, 2048, 40000) or reuse not in ("low", "high") or
                (workload == "p8" and entities != 40000)):
            raise ValueError("unknown memory/P8 protocol")
        expected = f"{workload}-{entities}-{reuse}-{config.lower()}"
    else:
        gates = ("p0-legacy", "p0-rpa", "p0-read", "p0-host-common", "p1-no-consumer",
                 "p2-low-delivery", "p3-renderer", "p4-capture-1", "p4-capture-2",
                 "p4-capture-4", "p5-controls", "p6-sparse", "p6-topology", "p7-music", "p7-dfjk")
        if unit.get("gate") not in gates:
            raise ValueError("unknown fixed workload gate")
        expected = unit["gate"] + "-" + config.lower()
    if unit_id(unit["stable_protocol_id"]) != expected:
        raise ValueError("stable protocol id differs from canonical workload identity")
    return expected


def validate_preparation(document, *, source_verifier=verify_source):
    if document.get("schema") != "rpa.execution.preparation.v1":
        raise ValueError("unexpected preparation schema")
    if document.get("formal_registration") is not False:
        raise ValueError("preparation cannot declare itself formally registered")
    draft = dict(document, schema="rpa.r4.registration-draft.v1", formal_registration=False)
    validate_draft(draft)
    source_verifier(document["source_binding"])
    budgets = accepted_budget(document["acceptance_binding"], document["budget_binding"])
    profiles = {profile["id"] for profile in budgets["profiles"]}
    authorization = read(verify_artifact(document["authorization_binding"]))
    if (authorization.get("schema") != "rpa.r4.execution-authorization.v1" or
            authorization.get("automatic_work_authorized") is not True):
        raise ValueError("missing execution authorization")
    if not document.get("tool_bindings") or not document.get("fixture_bindings"):
        raise ValueError("tools and exact fixtures must be bound")
    for group in ("tool_bindings", "fixture_bindings", "binary_bindings"):
        for item in document[group].values():
            verify_artifact(item)
    if not document.get("invalid_process_rules"):
        raise ValueError("invalid-process rules must be declared before running")
    if len({stable_id(unit) for unit in document["units"]}) != len(document["units"]):
        raise ValueError("duplicate canonical workload protocol")
    for unit in document["units"]:
        unit_id(unit["id"])
        stable_id(unit)
        if unit.get("requires_o1_scale") and unit["profile"] not in profiles:
            raise ValueError("unknown accepted scale profile")
        if unit.get("comparison") == "A/B":
            if document.get("baseline_sha") != PRE_PD:
                raise ValueError("common A must be the original pre-PD implementation")
            if unit.get("baseline_binary") not in document["binary_bindings"]:
                raise ValueError("missing matching baseline executable")
            source_verifier(document["baseline_source_binding"])
            if document["baseline_source_binding"].get("sha") != PRE_PD:
                raise ValueError("baseline source binding is not the original pre-PD tree")
        if unit.get("candidate_binary") not in document["binary_bindings"]:
            raise ValueError("missing candidate executable")
        if unit.get("fixture") not in document["fixture_bindings"]:
            raise ValueError("unit has no exact fixture binding")
        metrics = unit.get("required_metrics")
        if not isinstance(metrics, list) or not metrics or len(metrics) != len(set(metrics)):
            raise ValueError("explicit unique primary metric list required")
        for kind in ("semantic", "pilot", "environment"):
            proof = read(verify_artifact(unit[kind + "_binding"]))
            if (proof.get("schema") != "rpa.execution." + kind + ".v1" or
                    proof.get("passed") is not True or proof.get("unit") != unit["id"] or
                    proof.get("source_sha") != document["source_binding"]["sha"]):
                raise ValueError("missing, failed or unrelated " + kind + " proof")
            if proof.get("endpoint") != unit["completion_endpoint"]:
                raise ValueError("proof uses a different completion endpoint")
            if proof.get("candidate_binary_sha256") != document["binary_bindings"][unit["candidate_binary"]]["sha256"]:
                raise ValueError("proof is bound to another candidate binary")
            if not proof.get("raw_bindings"):
                raise ValueError("proof has no immutable raw evidence")
            for item in proof["raw_bindings"]:
                verify_artifact(item)
        mode = unit.get("measurement_mode")
        if mode not in ("normal", "stage-timing", "memory", "device"):
            raise ValueError("explicit measurement mode required")
        if unit.get("adapter") == "probe":
            # The presentation-matrix harness has no pre-PD counterpart and no device endpoint.
            probe_units.validate_registration(unit)
        builds = [(unit["candidate_binary"], document["source_binding"])]
        if unit["comparison"] == "A/B":
            builds.append((unit["baseline_binary"], document["baseline_source_binding"]))
        toolchains = []
        for key, source in builds:
            metadata = read(verify_artifact(document["binary_build_bindings"][key]))
            build_binding.verify(metadata, source, mode)
            if metadata["binary"] != document["binary_bindings"][key] or metadata["config"] != unit["config"]:
                raise ValueError("build metadata differs from the selected binary/configuration")
            toolchains.append(metadata["toolchain_signature"])
        if any(value != toolchains[0] for value in toolchains):
            raise ValueError("A/B compiler, architecture or dependency baseline differs")
        if mode in ("stage-timing", "memory", "device"):
            proof = read(verify_artifact(unit["observer_binding"]))
            if (proof.get("schema") != "rpa.execution.observer-admission.v1" or
                    proof.get("passed") is not True or proof.get("unit") != unit["id"] or
                    proof.get("mode") != mode or
                    proof.get("source_sha") != document["source_binding"]["sha"]):
                raise ValueError("observer coverage, range and overhead not admitted")
    return document


ATTEMPT = re.compile(r"attempt-(\d+)\.json")


def attempts_of(directory):
    """Every sealed attempt in numeric order; a widened allowance can pass two attempts."""
    return sorted((path for path in Path(directory).glob("attempt-*.json")
                   if ATTEMPT.fullmatch(path.name)),
                  key=lambda path: int(ATTEMPT.fullmatch(path.name).group(1)))


def retry_disposition(ledger, stable_protocol_id, remedy):
    """Build the one disposition a further registration needs; the failed attempt stays visible."""
    unit_id(stable_protocol_id)
    directory = Path(ledger) / stable_protocol_id
    attempts = attempts_of(directory)
    if not attempts:
        raise ValueError("no previous attempt to recover from: " + stable_protocol_id)
    return {"schema": "rpa.execution.retry-disposition.v1",
            "stable_protocol_id": stable_protocol_id,
            "previous_claim_sha256": sha256(attempts[-1]),
            "verified_remedy_binding": remedy,
            "scope": "Remedy verified before this registration; no sample replacement and no "
                     "merging with the failed attempt's readings."}


def allowance(unit, waiver=None):
    """Default allowance is one p8 attempt and two for every other unit.

    An owner waiver may widen it for named stable protocol ids. The waiver artifact is bound into
    the claim, so a third attempt is never silent: it always carries the owner's scope.
    """
    maximum = 1 if unit["workload"] == "p8" else 2
    if waiver is None:
        return maximum
    if (waiver.get("schema") != "rpa.execution.attempt-waiver.v1" or
            waiver.get("granted_by") != "owner" or
            stable_id(unit) not in waiver.get("stable_protocol_ids", []) or
            not isinstance(waiver.get("maximum_attempts"), int) or
            waiver["maximum_attempts"] <= maximum):
        raise ValueError("owner waiver does not authorize this unit")
    return waiver["maximum_attempts"]


def claim(preparation, ledger, waiver=None, waiver_binding=None):
    """A failed partial claim stays visible; it never restores an attempt allowance."""
    validate_preparation(preparation)
    ledger = Path(ledger).resolve()
    if ledger != Path(preparation["attempt_ledger"]).resolve():
        raise ValueError("attempt ledger differs from bound authorization")
    authorization = read(verify_artifact(preparation["authorization_binding"]))
    if ledger != Path(authorization["attempt_ledger"]).resolve():
        raise ValueError("attempt ledger differs from the single execution authorization")
    ledger.mkdir(parents=True, exist_ok=True)
    claims = []
    for unit in preparation["units"]:
        directory = ledger / stable_id(unit)
        directory.mkdir(exist_ok=True)
        attempts = attempts_of(directory)
        if len(attempts) >= allowance(unit, waiver):
            raise ValueError("fixed attempt allowance exhausted: " + unit["id"])
        if attempts:
            disposition = read(verify_artifact(unit["retry_disposition_binding"]))
            if (disposition.get("schema") != "rpa.execution.retry-disposition.v1" or
                    disposition.get("stable_protocol_id") != stable_id(unit) or
                    disposition.get("previous_claim_sha256") != sha256(attempts[-1]) or
                    not disposition.get("verified_remedy_binding")):
                raise ValueError("new registration requires a bound, verified remedy")
            verify_artifact(disposition["verified_remedy_binding"])
        path = directory / f"attempt-{len(attempts) + 1}.json"
        record = {"schema": "rpa.execution.claim.v1", "unit": unit["id"],
                  "source_sha": preparation["source_binding"]["sha"],
                  "comparison": unit["comparison"], "attempt": len(attempts) + 1,
                  "stable_protocol_id": stable_id(unit)}
        if waiver_binding is not None:
            record["waiver_binding"] = waiver_binding
        write_once(path, record)
        claims.append(path)
    return claims


def seal(preparation_path, claims, output):
    preparation_path = Path(preparation_path).resolve()
    document = validate_preparation(read(preparation_path))
    if len(claims) != len(document["units"]):
        raise ValueError("claims must cover every unit exactly once")
    bound = []
    for unit, path in zip(document["units"], claims):
        path = Path(path).resolve()
        row = read(path)
        if (row.get("unit") != unit["id"] or
                row.get("stable_protocol_id") != stable_id(unit) or
                row.get("source_sha") != document["source_binding"]["sha"] or
                row.get("comparison") != unit["comparison"]):
            raise ValueError("claim does not match preparation")
        marker = path.with_suffix(".seal.json")
        write_once(marker, {"preparation_sha256": sha256(preparation_path),
                            "seal": str(Path(output).resolve())})
        bound.append({"path": str(path), "sha256": sha256(path),
                      "marker": str(marker), "marker_sha256": sha256(marker)})
    write_once(output, {"schema": "rpa.execution.seal.v1",
                        "preparation": str(preparation_path),
                        "preparation_sha256": sha256(preparation_path), "claims": bound})


def read_sealed(path):
    path = Path(path).resolve()
    record = read(path)
    preparation = Path(record["preparation"])
    if (record.get("schema") != "rpa.execution.seal.v1" or
            sha256(preparation) != record["preparation_sha256"]):
        raise ValueError("sealed preparation changed")
    document = validate_preparation(read(preparation))
    if len(record["claims"]) != len(document["units"]):
        raise ValueError("incomplete sealed claim list")
    for unit, item in zip(document["units"], record["claims"]):
        claim_path, marker = Path(item["path"]), Path(item["marker"])
        if sha256(claim_path) != item["sha256"] or sha256(marker) != item["marker_sha256"]:
            raise ValueError("claim or seal marker changed")
        claimed, marked = read(claim_path), read(marker)
        if (claimed.get("unit") != unit["id"] or
                claimed.get("source_sha") != document["source_binding"]["sha"] or
                marked.get("seal") != str(path) or
                marked.get("preparation_sha256") != sha256(preparation)):
            raise ValueError("claim, unit or seal identity mismatch")
    return document
