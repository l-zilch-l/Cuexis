"""Derive proposed R4 ceilings from declarations and audited layouts, never observed peaks."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MIB = 1024 * 1024
REQUIRED_DOMAINS = {
    "state.world", "state.current", "state.candidate", "state.scratch", "state.index",
    "content.decoded", "prepare.transient", "execution.retained", "owning.results",
    "codec.buffers", "delivery.payload", "delivery.attempt", "delivery.ready",
    "delivery.receipt", "history.request", "command.session", "command.audio", "command.host",
    "command.capture", "steps", "journal", "static", "gpu", "audio",
}


def positive(value, name):
    if type(value) is not int or value <= 0:
        raise ValueError("positive finite integer required: " + name)
    return value


def derive(layout):
    sizes = layout["sizes"]
    required = ("SessionChannelSlot", "AudioChannelSlot", "OwnerStepSlot", "ControlReceiptSlot",
                "HistorySlot", "ConsumerSlot", "JournalFrameSlot", "FeedbackSlot",
                "CaptureDelivery", "Occurrence", "PauseTransition", "FrameObject", "FeedbackId",
                "FramePayload", "FrameEnvelope", "PortableResourcePtr", "NativeRecoveryMaxAnchors",
                "optional_PresentReceipt", "FrameAttemptLease", "DispatchToken")
    for name in required:
        positive(sizes.get(name), name)
    profiles = []
    for entities in (64, 2048, 40000):
        # The frame estimate is the existing frameByteBound formula with two resources and N members.
        frame = (sizes["FramePayload"] + sizes["FrameEnvelope"] + 1024 +
                 entities * (2 * sizes["FrameObject"] + 4 * (512 + 32)) +
                 2 * 2 * sizes["PortableResourcePtr"] +
                 entities * (2 * sizes["FeedbackId"] + 2 * (512 + 32)))
        domains = []

        def add(name, value, basis, *, count=None, lifetime="session", kind="declared_budget",
                age=None, alias=None, device=False):
            owner = ("renderer-owner" if name == "gpu" else
                     "audio-worker" if name in ("audio", "command.audio") else
                     "main" if name in ("command.host", "command.capture", "owning.results") else
                     "playback-worker")
            domains.append(dict(id=name, requested_bytes=value, count=count, basis=basis,
                                lifetime=lifetime, age_cap_us=age, alias_of=alias, device=device,
                                owner=owner, kind=kind, enforcement="measurement_gate"))

        for name, coefficient in (("world", 4096), ("current", 2048), ("candidate", 4096),
                                  ("scratch", 2048), ("index", 1024)):
            add("state." + name, coefficient * entities + 65536,
                f"B0 declared coefficient {coefficient} * N + 65536; new scale needs acceptance")
        # These are new design budgets. They are deliberately not analytical heap proofs.
        add("content.decoded", 32768 * entities + 8 * MIB,
            "declared 32 KiB per requirement/entity + 8 MiB fixed; all owned graph/string tables")
        add("prepare.transient", 65536 * entities + 16 * MIB,
            "declared 64 KiB per requirement/entity + 16 MiB fixed; overlapping prepare allocations",
            lifetime="prepare")
        add("execution.retained", 32768 * entities + 16 * MIB,
            "declared aggregate 32 KiB per N + 16 MiB; all log results/native anchors/old generations",
            lifetime="session_and_owning_handles")
        add("owning.results", 4 * (8192 * entities + MIB),
            "4 simultaneous non-aliasing CPU result owners, each 8 KiB per N + 1 MiB",
            count=4, lifetime="last_legal_handle")
        add("codec.buffers", 2 * 256 * MIB + 16 * MIB,
            "2 simultaneous encoded buffers of declared 256 MiB + 16 MiB codec scratch",
            count=2, lifetime="codec")
        add("delivery.payload", 2 * frame, "2 * audited existing frameByteBound(N,2,N)",
            count=2, lifetime="last_legal_handle")
        add("delivery.attempt",
            64 * (2 * sizes["FrameEnvelope"] + sizes["FrameAttemptLease"] +
                  sizes["DispatchToken"] + 512),
            "64 * existing attemptBytes: 2 FrameEnvelope + FrameAttemptLease + DispatchToken + 512",
            count=64, lifetime="last_legal_handle")
        add("delivery.ready", None, "ready aliases payload/attempt; no duplicate byte charge",
            count=8, lifetime="last_legal_handle", alias="delivery.payload")
        add("delivery.receipt", 128 * sizes["optional_PresentReceipt"] + 128 * 4096,
            "128 audited receipt slots + declared 4 KiB bounded structured error per receipt",
            count=128, age=5000000)
        add("history.request", 8 * 32 * sizes["HistorySlot"] + 8 * sizes["ConsumerSlot"],
            "8 consumers * 32 actual history slots + 8 actual consumer slots",
            count=256, lifetime="ring")
        add("command.session", 128 * sizes["SessionChannelSlot"] + 16 * MIB,
            "128 actual command+reply slots, 32 are reserved within 128; 16 MiB nested owned payload",
            count=128)
        add("command.audio", 128 * sizes["AudioChannelSlot"] + 16 * MIB,
            "128 actual audio command+reply slots, 32 reserved; 16 MiB encoded input aggregate",
            count=128)
        add("command.host", 256 * sizes["ControlReceiptSlot"] + MIB,
            "256 actual host receipt slots + 1 MiB nested diagnostic strings", count=256)
        add("command.capture",
            4096 * (sizes["CaptureDelivery"] + 2 * sizes["Occurrence"] + sizes["PauseTransition"])
            + 8 * MIB,
            "4096 capture slots, two occurrence vectors and trace carrier + declared 8 MiB nested data",
            count=4096)
        add("steps", (2 * 4096 + 4096 + 128) * sizes["OwnerStepSlot"] + 4 * MIB,
            "software/renderer/capture:4096 each; audio:128; actual slots + 4 MiB nested diagnostics",
            count=12416)
        add("journal", 8 * MIB + 4096 * (sizes["JournalFrameSlot"] + sizes["FeedbackSlot"]) +
            128 * (sizes["FeedbackSlot"] + 8) + 8 * 16,
            "8 MiB journal-owned dynamic data + actual 4096 frame/feedback slots +128 tombstones/8 coverage",
            count=4096, lifetime="ring", age=5000000)
        add("static", 8 * MIB, "declared fixed bookkeeping/cache/allocator control metadata")
        add("gpu", 64 * MIB,
            "declared backend-owned active/candidate/retired allocations aggregate; driver allocation separate",
            count=2, lifetime="fence_and_owner_retirement", device=True)
        add("audio", 512 * MIB + 16 * MIB,
            "existing 256 MiB/clip and 512 MiB registered PCM + declared 16 MiB transport/decode overhead",
            count=4, lifetime="last_audio_lease_and_callback_isolation")
        cpu = sum(d["requested_bytes"] for d in domains if not d["device"] and not d["alias_of"])
        margin = (cpu + 7) // 8
        profiles.append(dict(id=f"r4-reference-n{entities}-v1", entities=entities, reuse=["low", "high"],
                             status="proposed", formal_registration=False, frame_byte_bound=frame,
                             max_native_anchors=sizes["NativeRecoveryMaxAnchors"],
                             old_generations="bounded by aggregate execution.retained, never counted as zero",
                             domains=domains, cpu_requested_sum=cpu, cpu_requested_margin=margin,
                             cpu_requested_envelope=cpu + margin,
                             fixture_requested_ceiling=65536 * entities + 16 * MIB,
                             observer_requested_ceiling=2 * 1024**3,
                             # CRT usable storage is a separate proposed gate, not a proven factor.
                             cpu_storage_envelope=(5 * (cpu + margin) + 3) // 4,
                             process_private_commit_ceiling=16 * 1024**3,
                             process_working_set_ceiling=16 * 1024**3,
                             gpu_backend_allocation_ceiling=64 * MIB,
                             gpu_driver_delta_ceiling=256 * MIB,
                             observer_allocation_count_ceiling=8 * 1024**2))
    return dict(schema="rpa.r4.budget-proposal.v1", status="proposed", accepted=None,
                formal_registration=False, source="declarations_and_sizeof_only",
                analytical_upper_bound_proven=False, profiles=profiles,
                limitations=[
                    "No fixed total production bound: retained record results and external handles can grow.",
                    "New per-domain coefficients are design ceilings; complete heap bounds remain unproven.",
                    "Existing observer ownership tags and legacy classification cannot fully rate these domains.",
                    "No production testOnly flag is removed by accepting this proposal.",
                ])


def validate(document):
    if document.get("schema") != "rpa.r4.budget-proposal.v1":
        raise ValueError("unsupported budget schema")
    if document.get("accepted") is not None or document.get("formal_registration") is not False:
        raise ValueError("proposal cannot self-accept or register")
    profiles = document.get("profiles", [])
    if ([p.get("entities") for p in profiles] != [64, 2048, 40000] or
            any(type(p.get("entities")) is not int for p in profiles) or
            len({p.get("id") for p in profiles}) != 3):
        raise ValueError("all three explicit scales are required")
    for profile in document["profiles"]:
        domains = profile["domains"]
        if {d["id"] for d in domains} != REQUIRED_DOMAINS or len(domains) != len(REQUIRED_DOMAINS):
            raise ValueError("missing or duplicate memory domain")
        for domain in domains:
            if domain["alias_of"]:
                if domain["alias_of"] not in REQUIRED_DOMAINS or domain["requested_bytes"] is not None:
                    raise ValueError("alias must reference a declared domain without additive bytes")
            else:
                positive(domain["requested_bytes"], domain["id"])
            if not domain["lifetime"] or not domain["basis"] or not domain["owner"]:
                raise ValueError("lifecycle and derivation required")
            if domain["count"] is not None:
                positive(domain["count"], domain["id"] + ".count")
            if domain["age_cap_us"] is not None:
                positive(domain["age_cap_us"], domain["id"] + ".age")
        expected = sum(d["requested_bytes"] for d in domains if not d["device"] and not d["alias_of"])
        if profile["cpu_requested_sum"] != expected:
            raise ValueError("CPU domain sum mismatch")
        if profile["cpu_requested_envelope"] != expected + (expected + 7) // 8:
            raise ValueError("declared 1/8 margin changed")
        if profile["cpu_storage_envelope"] != (5 * profile["cpu_requested_envelope"] + 3) // 4:
            raise ValueError("independent storage gate changed")
    return document


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--layout", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    document = validate(derive(json.loads(args.layout.read_text(encoding="utf-8"))))
    document["layout_sha256"] = hashlib.sha256(args.layout.read_bytes()).hexdigest()
    document["derivation_sha256"] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    with args.output.open("x", encoding="utf-8", newline="\n") as stream:
        stream.write(json.dumps(document, ensure_ascii=False, indent=2) + "\n")
    for profile in document["profiles"]:
        print(profile["id"], profile["frame_byte_bound"], profile["cpu_requested_envelope"])


if __name__ == "__main__":
    main()
