"""Derive ``pd5.memory.caps.v1`` per-cell memory caps from declared capacities.

B0 of the PD-5 memory-domain plan: static derivation only.  No workload runs here and no
measured value may become a cap source (pd5-read-candidate-and-memory-domains-plan.md 3.5).
Every cell is either derived from a capacity already declared in ``profiles.json`` or
reported as an uncovered gap with an explicit reason -- never 0 and never unlimited.

Inputs (both hashed into the output):
  profiles.json   the accepted testOnly profile registry (pd5.profile-suite.v1)
  this script     derivation rules, proposal table and gap register

Usage:
  python -B tools/verification/playback_performance/memory_caps.py --write
  python -B tools/verification/playback_performance/memory_caps.py --check
  python -B tools/verification/playback_performance/memory_caps.py --markdown
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

SCHEMA = "pd5.memory.caps.v1"
SUITE_SCHEMA = "pd5.profile-suite.v1"
CAPS_KIND_COUNT = "count"
CAPS_KIND_BYTES = "bytes"
CAPS_KIND_BOTH = "both"
CAPS_KIND_DURATION = "duration"

ROOT = Path(__file__).resolve().parents[3]
PROFILES = ROOT / "tools/verification/playback_performance/profiles.json"
ELEMENT_SIZES = Path(__file__).resolve().parent / "element_sizes.v1.json"
OUTPUT = ROOT / "tools/verification/playback_performance/memory_caps.v1.json"

#: Audited ``sizeof`` per element, produced by ``audit_element_sizes.py``.  A missing audit leaves
#: the dependent byte cells uncovered; it never falls back to a guessed element size.
ELEMENT_SCHEMA = "pd5.element-sizes.v1"


def load_element_sizes() -> dict:
    if not ELEMENT_SIZES.is_file():
        return {}
    document = json.loads(ELEMENT_SIZES.read_text(encoding="utf-8"))
    if document.get("schema") != ELEMENT_SCHEMA:
        raise SystemExit(f"unexpected element-size schema: {document.get('schema')}")
    return {item["type"]: item["bytes"] for item in document["elements"]}


ELEMENT_BYTES = load_element_sizes()

#: pd5-measurement-proposal.md section 5 (L119-120) items, each mapped to the cells that
#: bound it.  An item with any uncovered cell is a sampling blocker, not a silent omission.
CHECKLIST = {
    "cpu_payload": ["delivery.payload", "delivery.frame_bytes"],
    "attempt": ["delivery.attempt", "delivery.payload"],
    "gpu_inflight": ["gpu.inflight"],
    "world": ["state.world"],
    "current": ["state.current"],
    "candidate": ["state.candidate"],
    "scratch": ["state.scratch"],
    "index": ["state.index"],
    "ready": ["delivery.ready", "delivery.payload"],
    "history": ["history.request"],
    "receipt": ["delivery.receipt"],
    "journal": ["journal.attempt", "journal.feedback", "journal.bytes"],
    "retention_deadline": ["retention.window", "deadline.receipt", "deadline.reply"],
    "age": ["age.max"],
}

#: Owner acceptance that turns a proposal into a cap (B0 gate G4) and the envelope ruling that
#: replaces the old fixed global constant for new runs (B0 gate G7).  Both are recorded here so
#: the derivation stays reproducible without reading a report.
OWNER_ACCEPTED = "2026-10-24"
ENVELOPE_DECISION = "option 2+3: per-content reference-workload envelope from the cap sum plus a " \
                    "declared margin, fixture accounted separately; the old fixed constant is " \
                    "retained for historical runs and superseded for new ones"

#: Per-entity charges accepted by the owner on 2026-10-24 (G4).  A declared design budget
#: derived from component ``sizeof`` plus a stated container headroom -- never a measured
#: high-water mark, and never a value back-filled from the 40k attribution numbers.
ACCEPTED_PER_ENTITY = {
    "state.world": (4096, "component sizeof sum 小于 1 KiB，乘 4 倍声明的 sparse-set/registry 余量"),
    "state.current": (2048, "每实体一个已发布 runtime 句柄槽"),
    "state.candidate": (4096, "candidate 等于 current 加在途 transform plan"),
    "state.scratch": (2048, "每次 update 的临时 gather/scratch 缓冲"),
    "state.index": (1024, "target/binding/group 索引每实体一项"),
}

ACCEPTED_FIXED = {
    "state.world": 65536,
    "state.current": 65536,
    "state.candidate": 65536,
    "state.scratch": 65536,
    "state.index": 65536,
}

#: Declared margin over the cap sum, expressed as an exact fraction so the envelope is
#: reproducible.  1/8 is a declaration covering unregistered cells, allocator headers and
#: container slack; it is not fitted to any observation.
DECLARED_MARGIN_NUMERATOR = 1
DECLARED_MARGIN_DENOMINATOR = 8

#: Per-domain age caps (G3), each derived from a deadline already declared in ``profiles.json``.
#: The observer's 900 s global default is explicitly NOT a source: a legitimate steady state in
#: a 30-minute run would exceed it, so it would fail valid runs rather than bound anything.
AGE_CAP_SOURCE = {
    "consumer.": ("receipt_deadline_us", "槽由回执期限回收"),
    "delivery.": ("receipt_deadline_us", "回执期限内未确认的交付单元已是协议违约"),
    "journal.": ("receipt_deadline_us", "attempt lease 与回执同批回收"),
    "retention.": ("retention_us", "保留窗口本身即声明值"),
}

#: Domains whose ownership ends at close.  They get no duration cap on purpose: the honest
#: bound is "live must reach zero at after-close", which the analyzer checks instead.
SESSION_SCOPED = ("state.", "command.", "steps", "observation.", "gpu.", "audio.")

#: Fixed-capacity rings overwritten by position.  Their resident count is capped and their age
#: is unbounded by design, so a duration cap would be a fiction.
RING_SCOPED = ("history.",)

#: Byte cells whose element size is still not auditable (gap G9).  The count cap stands on its
#: own; the byte side stays uncovered rather than charging a guessed element size.
UNAUDITED_BYTES = {
    "command.session_bytes": "G9：OwnerChannel 的槽类型是模板私嵌套 Record，静态审计不可达",
}

NA_REASONS = {
    "gpu.inflight": "profile 未启用 GPU 后端；设备域由 B4 单独登记",
    "audio.live": "profile audio=false；设备域由 B4 单独登记",
}

GAPS = [
    {
        "id": "G1",
        "items": ["world", "current", "candidate", "scratch", "index"],
        "reason": "content 规模（entities）未在 profiles.json 声明，取自 probe harness",
        "blocking": True,
        "closed": True,
        "disposition": f"{OWNER_ACCEPTED}：逐 profile 声明 content_entities，脚本不再内置规模表",
    },
    {
        "id": "G2",
        "items": ["receipt", "history"],
        "reason": "元素尺寸静态审计已完成（element_sizes.v1.json）：receipt 槽 "
                  "std::optional<PresentReceipt> 232 B，history 槽 "
                  "std::optional<RequestRecord> 208 B",
        "blocking": False,
        "closed": True,
    },
    {
        "id": "G3",
        "items": ["age"],
        "reason": "逐域 age 上限未声明",
        "blocking": True,
        "closed": True,
        "disposition": f"{OWNER_ACCEPTED}：逐域 age 上限由已声明期限推导（delivery/journal="
                       "receipt_deadline_us、retention=retention_us），session 生命周期域显式"
                       "不设时长上限；观察器 900 s 全局默认弃用",
    },
    {
        "id": "G4",
        "items": ["world", "current", "candidate", "scratch", "index"],
        "reason": "逐实体计费值只是 proposal，需 owner 在 B0 复核中接受后才成为 cap",
        "blocking": True,
        "closed": True,
        "disposition": f"{OWNER_ACCEPTED}：接受 13,312 B/实体（5 项 × 65,536 B fixed），"
                       "state.* 五项转为 covered",
    },
    {
        "id": "G5",
        "items": ["cpu_payload"],
        "reason": "storage charge 与 requested 的声明比例未登记；56 B/块是诊断实测，不能作 cap",
        "blocking": False,
    },
    {
        "id": "G6",
        "items": ["gpu_inflight"],
        "reason": "GPU/audio 设备域按 B4 另行登记",
        "blocking": False,
    },
    {
        "id": "G7",
        "items": ["cpu_payload"],
        "reason": "40k 固定 global_cpp_new_envelope 与逐域上限之和的关系未裁决",
        "blocking": True,
        "closed": True,
        "disposition": f"{OWNER_ACCEPTED}：{ENVELOPE_DECISION}",
    },
    {
        "id": "G8",
        "items": ["attempt", "ready"],
        "reason": "attempt/ready 按引用别名计入 payload 域，分析器需实现该不重复计口径",
        "blocking": False,
    },
    {
        "id": "G9",
        "items": ["command.session"],
        "reason": "session 队列定容 80 槽，元素是 OwnerChannel<SessionCommand, SessionReply> 的"
                  "私嵌套 Record；variant 本体已审得 37,680 B（最大分支携带 ControlReceipt），"
                  "故队列元素下界约 3.0 MB，但槽包装（序号、回复、时间戳）静态不可达，"
                  "字节上限尚未登记；该项不属于 §5 十四项，B1 计数后补登",
        "blocking": False,
    },
]


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def cell(name, kind, *, count=None, requested=None, storage=None, basis="", retention=None,
         coverage="covered", alias=None, unit=None, zero_is_declared=False, additive=True,
         proposed=None):
    return {
        "cell": name,
        "cap_kind": kind,
        "cap_count": count,
        "cap_requested_bytes": requested,
        #: A proposed value is never a cap: it is recorded only so the B0 review has a concrete
        #: number to accept or reject, and it stays null on every covered cell.
        "proposed_cap_requested_bytes": proposed,
        "cap_storage_bytes": storage,
        "basis": basis,
        "retention": retention,
        "coverage": coverage,
        "alias_of": alias,
        "unit": unit,
        #: 0 is forbidden as an unknown placeholder but legal when the profile declares it,
        #: for example P1 with F = R = 0.  The flag records which reading applies.
        "zero_is_declared": zero_is_declared,
        #: A bound that constrains a cell already counted elsewhere (per-frame bound, slot
        #: lease) is not added again when the declared envelope is summed.
        "additive": additive,
    }


def _audited(count, element):
    """Capacity × audited element size, or None when the element audit is missing."""
    size = ELEMENT_BYTES.get(element)
    return None if size is None else count * size


def _audit_coverage(element, reason):
    return "covered" if element in ELEMENT_BYTES else "NA:" + reason


def _max_age_cap_us(capacity):
    """Largest declared age cap over the bounded domains (G3)."""
    return max(capacity[source] for source, _ in AGE_CAP_SOURCE.values())


def _retention(name, capacity, steady_live):
    """Declared steady live and age bound for one cell, or None for a cell that is not a
    resident allocation domain (per-frame bound, duration cap, unobservable device domain)."""
    for prefix, (source, why) in AGE_CAP_SOURCE.items():
        if name.startswith(prefix):
            return {"steady_live_count": steady_live,
                    "age_cap_us": capacity[source],
                    "lifetime": "bounded",
                    "basis": f"{source}={capacity[source]}：{why}"}
    if name.startswith(SESSION_SCOPED):
        return {"steady_live_count": steady_live,
                "age_cap_us": None,
                "lifetime": "session",
                "basis": "持有到 close；由 after-close 产品域归零判据约束，不设时长上限"}
    if name.startswith(RING_SCOPED):
        return {"steady_live_count": steady_live,
                "age_cap_us": None,
                "lifetime": "ring",
                "basis": "定长环按位置覆盖；稳态 live 由 count 上限约束，不设时长上限"}
    return None


def derive_profile(profile_id, profile, defaults):
    capacity = dict(defaults["capacity"])
    capacity.update(profile.get("capacity") or {})
    consumers = profile.get("capture_consumers", capacity["consumers"])
    mode = profile.get("mode")
    entities = profile.get("content_entities")
    if not isinstance(entities, int) or entities <= 0:
        raise SystemExit(
            f"profile {profile_id}: content_entities 未声明或非法（G1 要求逐 profile 声明）")
    gpu = bool(profile.get("gpu"))
    max_age = _max_age_cap_us(capacity)

    def retain(name, steady=None):
        return _retention(name, capacity, steady)

    cells = [
        cell("consumer.slots", CAPS_KIND_COUNT, count=consumers,
             basis="profiles.json capture_consumers",
             alias="delivery.payload：槽只持引用，字节不重复计",
             zero_is_declared=consumers == 0,
             retention=retain("consumer.slots", consumers)),
        cell("delivery.payload", CAPS_KIND_BOTH, count=capacity["global_payloads"],
             requested=capacity["global_bytes"],
             basis="profiles.json defaults.capacity.global_payloads / global_bytes",
             retention=retain("delivery.payload", capacity["global_payloads"])),
        cell("delivery.frame_bytes", CAPS_KIND_BYTES,
             requested=profile.get("frame_byte_bound", capacity["global_bytes"]),
             basis="profiles.json frame_byte_bound：每帧保守计费上界"
                   if profile.get("frame_byte_bound")
                   else "未声明 frame_byte_bound，取域字节上限 global_bytes 作单帧上界",
             additive=False),
        cell("delivery.attempt", CAPS_KIND_COUNT, count=capacity["global_attempts"],
             basis="profiles.json defaults.capacity.global_attempts",
             alias="delivery.payload：lease 持引用，字节不重复计",
             retention=retain("delivery.attempt", capacity["global_attempts"])),
        cell("delivery.ready", CAPS_KIND_COUNT, count=consumers,
             basis="每消费者一个 ready 槽",
             alias="delivery.payload：槽只持引用，字节不重复计",
             zero_is_declared=consumers == 0,
             retention=retain("delivery.ready", consumers)),
        cell("delivery.receipt", CAPS_KIND_BOTH, count=capacity["receipt_capacity"],
             requested=_audited(capacity["receipt_capacity"], "std::optional<PresentReceipt>"),
             basis="profiles.json defaults.capacity.receipt_capacity × "
                   "sizeof(std::optional<PresentReceipt>)",
             coverage=_audit_coverage("std::optional<PresentReceipt>",
                                      "G2：receipt 元素尺寸未审计"),
             retention=retain("delivery.receipt", capacity["receipt_capacity"])),
        cell("journal.attempt", CAPS_KIND_COUNT, count=capacity["journal_attempts"],
             basis="profiles.json defaults.capacity.journal_attempts",
             retention=retain("journal.attempt", capacity["journal_attempts"])),
        cell("journal.feedback", CAPS_KIND_COUNT, count=capacity["journal_feedback"],
             basis="profiles.json defaults.capacity.journal_feedback",
             retention=retain("journal.feedback", capacity["journal_feedback"])),
        cell("journal.bytes", CAPS_KIND_BYTES, requested=capacity["journal_bytes"],
             basis="profiles.json defaults.capacity.journal_bytes",
             retention=retain("journal.bytes")),
        cell("observation.tombstone", CAPS_KIND_COUNT, count=capacity["tombstones"],
             basis="profiles.json defaults.capacity.tombstones",
             retention=retain("observation.tombstone", capacity["tombstones"])),
        cell("observation.coverage", CAPS_KIND_COUNT, count=capacity["coverage_intervals"],
             basis="profiles.json defaults.capacity.coverage_intervals",
             retention=retain("observation.coverage", capacity["coverage_intervals"])),
        cell("command.session", CAPS_KIND_COUNT,
             count=capacity["session_commands"] + capacity["control_reserve"],
             basis="session_commands + control_reserve",
             retention=retain("command.session",
                              capacity["session_commands"] + capacity["control_reserve"])),
        cell("command.host", CAPS_KIND_COUNT, count=capacity["host_commands"],
             basis="profiles.json defaults.capacity.host_commands",
             retention=retain("command.host", capacity["host_commands"])),
        cell("command.capture", CAPS_KIND_COUNT,
             count=capacity["capture_commands"] + capacity["capture_reserve"],
             basis="capture_commands + capture_reserve",
             retention=retain("command.capture",
                              capacity["capture_commands"] + capacity["capture_reserve"])),
        cell("steps", CAPS_KIND_COUNT, count=capacity["steps"] + capacity["step_reserve"],
             basis="steps + step_reserve",
             retention=retain("steps", capacity["steps"] + capacity["step_reserve"])),
        cell("history.request", CAPS_KIND_BOTH, count=consumers * capacity["request_history"],
             requested=_audited(consumers * capacity["request_history"],
                                "std::optional<RequestRecord>"),
             basis="capture_consumers × defaults.capacity.request_history（每个消费者一份"
                   "定长 history）× sizeof(std::optional<RequestRecord>)",
             zero_is_declared=consumers == 0,
             coverage=_audit_coverage("std::optional<RequestRecord>",
                                      "G2：request history 元素尺寸未审计"),
             retention=retain("history.request", consumers * capacity["request_history"])),
        cell("retention.window", CAPS_KIND_DURATION, requested=capacity["retention_us"],
             unit="us", basis="profiles.json defaults.capacity.retention_us"),
        cell("deadline.receipt", CAPS_KIND_DURATION, requested=capacity["receipt_deadline_us"],
             unit="us", basis="profiles.json defaults.capacity.receipt_deadline_us"),
        cell("deadline.reply", CAPS_KIND_DURATION, requested=capacity["reply_deadline_ms"],
             unit="ms", basis="profiles.json defaults.capacity.reply_deadline_ms"),
        cell("gpu.inflight", CAPS_KIND_BOTH,
             coverage="NA:" + NA_REASONS["gpu.inflight"] if not gpu
             else "NA:GPU 后端存在但设备域未登记（G6）"),
        cell("audio.live", CAPS_KIND_BOTH, coverage="NA:" + NA_REASONS["audio.live"]),
        cell("age.max", CAPS_KIND_DURATION, requested=max_age, unit="us",
             basis="各域 age 上限的静态最大值：delivery/journal=receipt_deadline_us、"
                   "retention=retention_us；不沿用观察器 900 s 全局默认（G3）"),
    ]

    for name, (per_entity, why) in ACCEPTED_PER_ENTITY.items():
        fixed = ACCEPTED_FIXED[name]
        cells.append(cell(name, CAPS_KIND_BYTES,
                          requested=per_entity * entities + fixed,
                          basis=f"owner {OWNER_ACCEPTED} 接受（G4）：{why}；"
                                f"entities={entities} × {per_entity} + fixed {fixed}",
                          retention=retain(name)))

    for name, reason in UNAUDITED_BYTES.items():
        cells.append(cell(name, CAPS_KIND_BYTES, coverage="NA:" + reason))

    return {
        "profile": profile_id,
        "harness": profile.get("harness", "host"),
        "mode": mode,
        "gpu": gpu,
        "consumers": consumers,
        "content_entities": entities,
        "age_caps": {"max_age_cap_us": max_age,
                     "domains": {prefix.rstrip("."): capacity[source]
                                 for prefix, (source, _) in AGE_CAP_SOURCE.items()},
                     "basis": "各域 age 上限均由 profiles.json 已声明的期限推导；"
                              "session 生命周期域不设时长上限"},
        "cells": sorted(cells, key=lambda c: c["cell"]),
    }


def checklist_coverage(contents):
    covered = {c["cell"] for entry in contents for c in entry["cells"]
               if c["coverage"] == "covered"}
    return {
        item: {"cells": cells, "covered": all(c in covered for c in cells)}
        for item, cells in CHECKLIST.items()
    }


SUPERSEDED_ENVELOPE = {
    "count": 2097152,
    "requested_bytes": 1073741824,
    "storage_bytes": 1342177280,
    "status": "superseded for new runs by the per-content envelope below; every historical run "
              "keeps its recorded failure and none is re-run",
}


def envelope(contents):
    """Per-content reference-workload envelope: additive cap sum plus the declared margin.

    Plan 3.8 option 2 registers a new per-content envelope instead of reusing the fixed global
    constant, and option 3 keeps fixture/decoder objects out of the sum, so the envelope covers
    product domains only.  The margin is a declared fraction, never fitted to a measurement.
    """
    def total(entry):
        return sum(c["cap_requested_bytes"] or 0 for c in entry["cells"]
                   if c["coverage"] == "covered" and c["unit"] is None and c["additive"])

    per_content = []
    for entry in contents:
        capped = total(entry)
        margin = capped * DECLARED_MARGIN_NUMERATOR // DECLARED_MARGIN_DENOMINATOR
        per_content.append({
            "profile": entry["profile"],
            "content_entities": entry["content_entities"],
            "declared_capped_bytes": capped,
            "declared_margin_bytes": margin,
            "reference_workload_envelope_bytes": capped + margin,
        })
    worst = max(per_content, key=lambda e: e["reference_workload_envelope_bytes"])
    return {
        "decision": ENVELOPE_DECISION,
        "decided": OWNER_ACCEPTED,
        "margin": f"{DECLARED_MARGIN_NUMERATOR}/{DECLARED_MARGIN_DENOMINATOR}",
        "product_domains_only": True,
        "excluded_from_envelope": [
            "测试用例的 decode/fixture owning 对象（Packed、decoded content、fixture）",
            "Snapshot / Replay 归档副本",
            "观察器自身的块头与结构表",
        ],
        "per_content": per_content,
        "worst": worst["profile"],
        "worst_reference_workload_envelope_bytes": worst["reference_workload_envelope_bytes"],
        "superseded_global_cpp_new_envelope": SUPERSEDED_ENVELOPE,
        "basis": "仅相加 covered、无 unit、additive 的字节上限，再加声明余量；"
                 "40k 完整规模不在此登记，见 P8 单侧完整规模定案表",
    }


def build():
    suite = json.loads(PROFILES.read_text(encoding="utf-8"))
    if suite.get("schema") != SUITE_SCHEMA:
        raise SystemExit(f"unexpected profile suite schema: {suite.get('schema')}")
    defaults = suite["defaults"]
    contents = [derive_profile(p["id"], p, defaults) for p in suite["profiles"]]
    document = {
        "schema": SCHEMA,
        "formal_registration": False,
        "status": f"B0 gate accepted {OWNER_ACCEPTED}; awaiting B1 observer tag and analyzer",
        "derived_from": {
            "profiles": {"path": str(PROFILES.relative_to(ROOT)).replace("\\", "/"),
                         "sha256": sha256(PROFILES)},
            "script": {"path": str(Path(__file__).relative_to(ROOT)).replace("\\", "/"),
                       "sha256": sha256(Path(__file__).resolve())},
            "element_sizes": {
                "path": str(ELEMENT_SIZES.relative_to(ROOT)).replace("\\", "/"),
                "sha256": sha256(ELEMENT_SIZES),
                "tool": "tools/verification/playback_performance/audit_element_sizes.py",
            } if ELEMENT_SIZES.is_file() else None,
            "checklist_source": "docs/stage_plans/active/stage-07/pd5-measurement-proposal.md L119-120",
        },
        "checklist_coverage": checklist_coverage(contents),
        "contents": contents,
        "gaps": GAPS,
        "envelope": envelope(contents),
    }
    return document


def validate(document):
    problems = []
    for entry in document["contents"]:
        max_age = entry["age_caps"]["max_age_cap_us"]
        age_cell = next((c for c in entry["cells"] if c["cell"] == "age.max"), None)
        if age_cell is None or age_cell["coverage"] != "covered":
            problems.append(f"{entry['profile']}: age.max is not registered (G3)")
        elif age_cell["cap_requested_bytes"] != max_age:
            problems.append(f"{entry['profile']}: age.max disagrees with the domain maximum")
        for item in entry["cells"]:
            label = f"{entry['profile']}/{item['cell']}"
            if not item["coverage"]:
                problems.append(f"{label}: missing coverage")
                continue
            if item["coverage"] == "covered":
                if item["cap_count"] is None and item["cap_requested_bytes"] is None:
                    problems.append(f"{label}: covered without a cap")
                zero = item["cap_requested_bytes"] == 0 or item["cap_count"] == 0
                if zero and not item["zero_is_declared"]:
                    problems.append(f"{label}: 0 used as an unknown placeholder")
                if item["proposed_cap_requested_bytes"] is not None:
                    problems.append(f"{label}: covered cell carries a proposal")
            else:
                if not item["coverage"].startswith("NA:") or len(item["coverage"]) <= 3:
                    problems.append(f"{label}: NA without a reason")
                if item["cap_count"] is not None or item["cap_requested_bytes"] is not None:
                    problems.append(f"{label}: uncovered cell carries a cap")
            if item["alias_of"] and item["cap_requested_bytes"] is not None:
                problems.append(f"{label}: aliased cell must not carry a byte cap")
            if item["coverage"] == "covered" and item["unit"] is None and item["additive"]:
                if item["retention"] is None:
                    problems.append(f"{label}: resident allocation domain lacks retention (G3)")
                elif item["retention"]["age_cap_us"] is None:
                    if item["retention"]["lifetime"] not in ("session", "ring"):
                        problems.append(f"{label}: no age cap and no declared lifetime class")
                elif not 0 < item["retention"]["age_cap_us"] <= max_age:
                    problems.append(f"{label}: age cap exceeds the declared domain maximum")
            elif item["retention"] is not None:
                problems.append(f"{label}: non-resident domain must not carry a retention")
    return problems


def render(document):
    return json.dumps(document, ensure_ascii=False, indent=2, sort_keys=False) + "\n"


def markdown(document):
    lines = ["| 单元 | cap_kind | count | requested bytes | 来源 | 覆盖 |",
             "| --- | --- | --- | --- | --- | --- |"]
    entry = document["contents"][0]
    for item in entry["cells"]:
        lines.append("| `{cell}` | {cap_kind} | {count} | {requested} | {basis} | {coverage} |"
                     .format(cell=item["cell"], cap_kind=item["cap_kind"],
                             count=item["cap_count"] if item["cap_count"] is not None else "—",
                             requested=item["cap_requested_bytes"]
                             if item["cap_requested_bytes"] is not None else "—",
                             basis=item["basis"] or "—", coverage=item["coverage"]))
    return "\n".join(lines) + "\n"


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--write", action="store_true", help="write the caps document")
    group.add_argument("--check", action="store_true", help="verify the document is current")
    group.add_argument("--markdown", action="store_true", help="render one profile as Markdown")
    args = parser.parse_args(argv)

    sys.stdout.reconfigure(encoding="utf-8")
    document = build()
    problems = validate(document)
    if problems:
        print("caps 校验失败：", file=sys.stderr)
        for problem in problems:
            print("  - " + problem, file=sys.stderr)
        return 1

    if args.markdown:
        print(markdown(document), end="")
        return 0
    if args.write:
        OUTPUT.write_text(render(document), encoding="utf-8", newline="\n")
        print(f"wrote {OUTPUT.relative_to(ROOT)}")
        return 0
    current = OUTPUT.read_text(encoding="utf-8") if OUTPUT.exists() else ""
    if current != render(document):
        print("caps 文档与推导不一致，请用 --write 重新生成", file=sys.stderr)
        return 1
    print(f"caps 文档一致：{OUTPUT.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
