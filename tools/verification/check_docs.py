"""Repository documentation consistency checks.

This checker intentionally validates documentation and candidate examples only. It does not
pretend that candidate Chart/CXC/CXT files are production schemas.
"""

from __future__ import annotations

import importlib.util
import json
import os
import re
import sys
import unicodedata
from urllib.parse import unquote
from collections import deque
from datetime import date
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
AGENTS = ROOT / "AGENTS.md"
DOCS = ROOT / "docs"
DOCS_INDEX = DOCS / "README.md"
EXAMPLES = DOCS / "examples" / "chart_format_update"
STATUS_CONTRACT = DOCS / "status_contract.json"
BUILDING_GUIDE = DOCS / "guides" / "BUILDING.md"
VERSION_CMAKE = ROOT / "cmake" / "CuexisVersion.cmake"
DEFAULT_TARGET_FACTS = ROOT / "out" / "build" / "debug" / "generated" / "cuexis-targets.txt"
TARGET_FACTS_ENV = "CUEXIS_TARGETS_FILE"
TARGET_BLOCK_BEGIN = "<!-- CUEXIS_ACTIVE_TARGETS_BEGIN -->"
TARGET_BLOCK_END = "<!-- CUEXIS_ACTIVE_TARGETS_END -->"
STAGE_PLAN_REQUIREMENTS = (
    ("stage_plans/completed/260830-followup/plan.md", ("completed",)),
    ("stage_plans/completed/stage-05/plan.md", ("completed",)),
    ("stage_plans/active/chart-format-update-for-v5/plan.md", ("active",)),
    ("stage_plans/completed/chart-format-foundation/plan.md", ("completed",)),
    ("stage_plans/completed/chart-format-foundation-hardening/plan.md", ("completed",)),
    ("stage_plans/completed/stage-06/plan.md", ("completed",)),
    ("stage_plans/completed/stage-07/plan.md", ("completed",)),
    ("stage_plans/active/stage-07-rr/plan.md", ("active",)),
    ("stage_plans/active/stage-07-5/plan.md", ("active",)),
    ("stage_plans/future/stage-08/plan.md", ("future",)),
    ("stage_plans/future/stage-11/plan.md", ("future",)),
    ("stage_plans/future/stage-12/plan.md", ("future",)),
)
DIRECTORY_INDEXES = (
    "README.md",
    "adr/README.md",
    "architecture/README.md",
    "archive/README.md",
    "api/README.md",
    "examples/README.md",
    "formats/README.md",
    "guides/README.md",
    "proposals/README.md",
    "proposals/deferred/README.md",
    "stage_plans/README.md",
    "stage_reports/README.md",
    "stage_reports/stages/chart-format-update/README.md",
    "stage_reports/reviews/full-review-2026-08/README.md",
    "stage_reports/stages/stage-01/README.md",
    "stage_reports/stages/stage-04/README.md",
    "stage_reports/stages/stage-05/README.md",
    "stage_reports/stages/chart-format-foundation/README.md",
    "stage_reports/stages/stage-06/README.md",
    "stage_reports/stages/stage-07/README.md",
    "stage_reports/stages/stage-07-5/README.md",
    "stage_reports/stages/stage-07-rr/README.md",
    "stage_reports/stages/stage-07/01-a/README.md",
    "stage_reports/stages/stage-07/06-b/README.md",
    "stage_reports/stages/stage-07/05-rpa-new/README.md",
    "stage_plans/completed/stage-07/README.md",
    "stage_plans/active/stage-07-rr/README.md",
    "stage_plans/active/stage-07-5/README.md",
    "stage_reports/stages/stage-07/03-rpa-legacy/README.md",
    "stage_reports/stages/stage-07/04-pd/README.md",
    "stage_reports/stages/stage-07/90-shared/README.md",
)
STAGE_NAVIGATION_INDEXES = {
    "stage_plans/README.md",
    "stage_plans/completed/stage-07/README.md",
    "stage_plans/active/stage-07-rr/README.md",
    "stage_plans/active/stage-07-5/README.md",
    "stage_plans/completed/stage-07/01-a/README.md",
    "stage_plans/completed/stage-07/06-b/README.md",
    "stage_plans/completed/stage-07/02-diagnostics/README.md",
    "stage_plans/completed/stage-07/04-pd/README.md",
    "stage_plans/completed/stage-07/03-rpa-legacy/README.md",
    "stage_plans/completed/stage-07/05-rpa-new/README.md",
    "stage_reports/README.md",
    "stage_reports/stages/chart-format-update/README.md",
    "stage_reports/reviews/full-review-2026-08/README.md",
    "stage_reports/stages/chart-format-foundation/README.md",
    "stage_reports/stages/stage-01/README.md",
    "stage_reports/stages/stage-04/README.md",
    "stage_reports/stages/stage-05/README.md",
    "stage_reports/stages/stage-06/README.md",
    "stage_reports/stages/stage-07/README.md",
    "stage_reports/stages/stage-07-5/README.md",
    "stage_reports/stages/stage-07-rr/README.md",
    "stage_reports/stages/stage-07/01-a/README.md",
    "stage_reports/stages/stage-07/01-a/s7a-0/README.md",
    "stage_reports/stages/stage-07/01-a/s7a-2/README.md",
    "stage_reports/stages/stage-07/01-a/s7a-3-4/README.md",
    "stage_reports/stages/stage-07/01-a/s7a-3/README.md",
    "stage_reports/stages/stage-07/01-a/s7a-5-6/README.md",
    "stage_reports/stages/stage-07/01-a/s7a-7-8/README.md",
    "stage_reports/stages/stage-07/01-a/s7a-8-4/README.md",
    "stage_reports/stages/stage-07/06-b/README.md",
    "stage_reports/stages/stage-07/02-diagnostics/README.md",
    "stage_reports/stages/stage-07/02-diagnostics/audit/README.md",
    "stage_reports/stages/stage-07/02-diagnostics/n0/README.md",
    "stage_reports/stages/stage-07/02-diagnostics/n1/README.md",
    "stage_reports/stages/stage-07/02-diagnostics/n3/README.md",
    "stage_reports/stages/stage-07/04-pd/README.md",
    "stage_reports/stages/stage-07/04-pd/design/README.md",
    "stage_reports/stages/stage-07/04-pd/pd-0/README.md",
    "stage_reports/stages/stage-07/04-pd/pd-2/README.md",
    "stage_reports/stages/stage-07/04-pd/pd-3/c/README.md",
    "stage_reports/stages/stage-07/04-pd/pd-4/README.md",
    "stage_reports/stages/stage-07/04-pd/pd-5/README.md",
    "stage_reports/stages/stage-07/04-pd/pd-5/admission/stage-a/README.md",
    "stage_reports/stages/stage-07/04-pd/pd-5/admission/stage-b/README.md",
    "stage_reports/stages/stage-07/04-pd/pd-5/admission/stage-c/README.md",
    "stage_reports/stages/stage-07/04-pd/pd-5/etw-tools/README.md",
    "stage_reports/stages/stage-07/04-pd/pd-5/lifecycle/README.md",
    "stage_reports/stages/stage-07/04-pd/pd-5/measurement/README.md",
    "stage_reports/stages/stage-07/04-pd/pd-5/memory-attribution/README.md",
    "stage_reports/stages/stage-07/04-pd/pd-5/preparation/README.md",
    "stage_reports/stages/stage-07/03-rpa-legacy/README.md",
    "stage_reports/stages/stage-07/03-rpa-legacy/b1/README.md",
    "stage_reports/stages/stage-07/03-rpa-legacy/deployment/README.md",
    "stage_reports/stages/stage-07/03-rpa-legacy/design/README.md",
    "stage_reports/stages/stage-07/03-rpa-legacy/device/README.md",
    "stage_reports/stages/stage-07/03-rpa-legacy/f1-f2/README.md",
    "stage_reports/stages/stage-07/03-rpa-legacy/f3/README.md",
    "stage_reports/stages/stage-07/03-rpa-legacy/performance/README.md",
    "stage_reports/stages/stage-07/05-rpa-new/README.md",
    "stage_reports/stages/stage-07/05-rpa-new/r1/README.md",
    "stage_reports/stages/stage-07/05-rpa-new/r2/README.md",
    "stage_reports/stages/stage-07/90-shared/README.md",
    "stage_reports/stages/stage-07/90-shared/release-gates/README.md",
}
ROOT_DOCUMENTATION_FILES = {
    "README.md",
    "CURRENT_STATUS.md",
    "PROJECT_GUIDE.md",
    "ROADMAP.md",
    "DOCUMENTATION_POLICY.md",
    "legacy-paths.md",
    "MAP.md",
    "HANDOFF.md",
}
API_REFERENCE_FILES = (
    "README.md",
    "playback-session.md",
    "sources-and-content.md",
    "frames-digests-and-timelines.md",
    "presentation-and-capabilities.md",
    "diagnostics-identity-and-compatibility.md",
    "internal-module-catalog.md",
)
REQUIRED_STAGE_LABELS = (
    "Stage 0",
    "Stage 1A",
    "Stage 1B",
    "Stage 1C",
    "Stage 1D",
    "Stage 1E",
    "Stage 2",
    "Stage 3",
    "Stage Chart Format Update",
    "Stage 4",
    "Stage 5",
    "Stage 6",
    "Stage 7",
    "Stage 7.5",
    "Stage 8",
    "Stage 9A",
    "Stage 9B",
    "Stage 10",
    "Stage 11",
    "Stage 12",
)

LINK_RE = re.compile(r"\[[^\]]*\]\(([^)]+)\)")
H1_RE = re.compile(r"^#\s+\S")
SCRIPT_KEYS = {"script", "scripts", "runtimescript", "runtimescripts", "runtime_script", "bytecode"}


class CheckFailure:
    def __init__(self, path: Path, message: str) -> None:
        self.path = path
        self.message = message

    def __str__(self) -> str:
        return f"{self.path.relative_to(ROOT)}: {self.message}"


def check_stage7_rr_plan_schedule(
    failures: list[CheckFailure], docs_dir: Path = DOCS
) -> None:
    checker_path = ROOT / "tools" / "verification" / "check_plan_schedule.py"
    spec = importlib.util.spec_from_file_location("cuexis_check_plan_schedule", checker_path)
    if spec is None or spec.loader is None:
        failures.append(CheckFailure(checker_path, "cannot load plan schedule checker"))
        return
    checker = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(checker)
    stage_dir = docs_dir / "stage_plans" / "active" / "stage-07-rr"
    for path, message in checker.validate_stage_dir(stage_dir):
        failures.append(CheckFailure(path, message))


def markdown_files() -> list[Path]:
    return sorted(DOCS.rglob("*.md"))


def lines_outside_fences(text: str) -> list[str]:
    result: list[str] = []
    fence: str | None = None
    for line in text.splitlines():
        stripped = line.lstrip()
        marker = stripped[:3]
        if marker in {"```", "~~~"}:
            if fence is None:
                fence = marker
            elif fence == marker:
                fence = None
            continue
        if fence is None:
            result.append(line)
    return result


def is_within(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
    except ValueError:
        return False
    return True


def check_h1_and_links(files: list[Path], failures: list[CheckFailure]) -> dict[Path, set[Path]]:
    graph: dict[Path, set[Path]] = {path: set() for path in files}
    known = {path.resolve() for path in files}
    for path in files:
        text = path.read_text(encoding="utf-8")
        visible_lines = lines_outside_fences(text)
        h1_count = sum(1 for line in visible_lines if H1_RE.match(line))
        if h1_count != 1:
            failures.append(CheckFailure(path, f"expected exactly one H1, found {h1_count}"))
        for target in LINK_RE.findall("\n".join(visible_lines)):
            target = target.strip().split("#", 1)[0].split("?", 1)[0]
            if not target or re.match(r"^(?:[a-z]+:)?//", target) or target.startswith("mailto:"):
                continue
            resolved = (path.parent / target).resolve()
            if not is_within(resolved, ROOT):
                continue
            if not resolved.exists():
                failures.append(CheckFailure(path, f"broken relative link: {target}"))
                continue
            if resolved in known:
                graph[path.resolve()].add(resolved)
    return graph


def check_reachability(graph: dict[Path, set[Path]], failures: list[CheckFailure]) -> None:
    start = DOCS_INDEX.resolve()
    if start not in graph:
        failures.append(CheckFailure(DOCS_INDEX, "documentation index is missing"))
        return
    visited: set[Path] = set()
    queue = deque([start])
    while queue:
        current = queue.popleft()
        if current in visited:
            continue
        visited.add(current)
        queue.extend(graph.get(current, ()))
    for path in graph:
        if path not in visited:
            failures.append(CheckFailure(path, "not reachable from docs/README.md"))


def heading_anchors(text: str) -> set[str]:
    """GitHub heading slugs, including duplicate-heading suffixes; ignore fenced code."""
    counts: dict[str, int] = {}
    anchors: set[str] = set()
    for line in lines_outside_fences(text):
        match = re.match(r"^#{1,6}\s+(.+?)\s*#*\s*$", line)
        if not match:
            continue
        heading = re.sub(r"!?\[([^\]]+)\]\([^)]*\)", r"\1", match[1])
        heading = re.sub(r"<[^>]*>", "", heading).lower()
        slug = "".join(c for c in heading if c in " -_" or not unicodedata.category(c).startswith(("P", "S")))
        slug = slug.replace(" ", "-")
        count = counts.get(slug, 0)
        counts[slug] = count + 1
        anchors.add(slug if count == 0 else f"{slug}-{count}")
    return anchors


def check_stage7_section_links(files: list[Path], failures: list[CheckFailure], root: Path = ROOT) -> None:
    """Check live Stage 7 and 7.5 owners and anchors while preserving dated evidence."""
    plans = (root / "docs/stage_plans").resolve()
    owners = {
        "1.1": ("completed/stage-07/plan.md", "11-"),
        "5.3": ("active/stage-07-5/plan.md", "53-"),
        "10": ("completed/stage-07/plan.md", "10-"),
    }
    plan_paths = {
        plans / name for name in (
            "completed/stage-07/plan.md",
            "completed/stage-07/01-a/plan.md",
            "completed/stage-07/06-b/plan.md",
            "active/stage-07-5/plan.md",
        )
    }
    for path in files:
        relative = path.resolve().relative_to(root.resolve()).as_posix()
        if not relative.startswith(
            ("docs/api/", "docs/formats/", "docs/stage_plans/active/", "docs/stage_plans/completed/")
        ):
            continue
        text = "\n".join(lines_outside_fences(path.read_text(encoding="utf-8")))
        for label, target in re.findall(r"\[([^\]]*)\]\(([^)]+)\)", text):
            destination, _, fragment = target.partition("#")
            resolved = (path.parent / destination).resolve()
            if resolved not in plan_paths or not resolved.is_file():
                continue
            fragment = unquote(fragment)
            if fragment and fragment not in heading_anchors(resolved.read_text(encoding="utf-8")):
                failures.append(CheckFailure(
                    path, f"Stage 7 / 7.5 section link: missing anchor {target}"
                ))
            sections = set(re.findall(r"§\s*(\d+(?:\.\d+)*)", label))
            # Contract links can place the section reference outside the link label.
            sections.update(
                section for section, (_, prefix) in owners.items() if fragment.startswith(prefix)
            )
            relative_plan = resolved.relative_to(plans).as_posix()
            for section in sorted(sections):
                owner = owners.get(section)
                if owner and relative_plan != owner[0]:
                    failures.append(CheckFailure(
                        path,
                        f"Stage 7 / 7.5 section link: section {section} "
                        f"belongs to {owner[0]}, found {relative_plan}",
                    ))


def status_contract_path(root: Path, value: str, label: str, failures: list[CheckFailure], contract: Path) -> Path | None:
    relative = Path(value)
    candidate = (root / relative).resolve()
    if relative.is_absolute() or not is_within(candidate, root.resolve()):
        failures.append(CheckFailure(contract, f"invalid status contract {label}: path escapes repository: {value}"))
        return None
    if not candidate.is_file():
        failures.append(CheckFailure(contract, f"invalid status contract {label}: referenced file is missing: {value}"))
        return None
    return candidate


def check_cfu_status(
    failures: list[CheckFailure], contract: Path = STATUS_CONTRACT, root: Path = ROOT
) -> None:
    # Keep the old callable name for the compatibility entry point, but only one live schema.
    try:
        value = json.loads(contract.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        failures.append(CheckFailure(contract, f"invalid current status contract: {error}"))
        return
    if not isinstance(value, dict) or value.get("schemaVersion") != 2:
        failures.append(CheckFailure(contract, "current status contract requires schemaVersion 2; historical fragment locks are unsupported"))
        return
    check_current_status_contract(failures, contract, root, value)


def check_current_status_contract(
    failures: list[CheckFailure], contract: Path, root: Path, value: dict[str, Any]
) -> None:
    """Validate live pointers, rather than preserving historical stage wording."""
    relative = value.get("currentStatus")
    if relative != "docs/CURRENT_STATUS.md":
        failures.append(CheckFailure(contract, "currentStatus must be the stable docs/CURRENT_STATUS.md entry"))
        return
    status = status_contract_path(root, relative, "currentStatus", failures, contract)
    if status is None:
        return
    text = status.read_text(encoding="utf-8")
    spec = importlib.util.spec_from_file_location("status_docs_map", Path(__file__).with_name("gen_docs_map.py"))
    if spec is None or spec.loader is None:
        failures.append(CheckFailure(status, "cannot load shared metadata parser"))
        return
    metadata_parser = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(metadata_parser)
    fields = metadata_parser.parse_meta(text)
    if not fields:
        failures.append(CheckFailure(status, "current status metadata is missing"))
        return
    if fields.get("id") != "STATUS":
        failures.append(CheckFailure(status, "current status must keep its stable id STATUS"))
    if fields.get("status") != "current" or fields.get("authority") != "owns":
        failures.append(CheckFailure(status, "current status must be current/owns"))
    pointers: dict[str, Path] = {}
    for key in ("current_item", "current_register"):
        pointer = fields.get(key)
        if not pointer:
            failures.append(CheckFailure(status, f"current status is missing {key}"))
            continue
        if Path(pointer).is_absolute() or pointer.startswith("/") or ":" in pointer or "\\" in pointer:
            failures.append(CheckFailure(status, f"{key} must be a relative docs path"))
            continue
        path = status_contract_path(root, "docs/" + pointer, key, failures, contract)
        if path is None:
            continue
        if not is_within(path, (root / "docs" / "stage_plans").resolve()):
            failures.append(CheckFailure(status, f"{key} must point inside docs/stage_plans"))
        pointers[key] = path
    item, register = pointers.get("current_item"), pointers.get("current_register")
    stage = fields.get("current_stage", "")
    if not stage:
        failures.append(CheckFailure(status, "current status is missing current_stage"))
    if item is not None:
        plan_text = item.read_text(encoding="utf-8")
        heading = next((line[2:] for line in plan_text.splitlines() if line.startswith("# ")), "")
        if item.name != "plan.md" or not stage or re.search(
            re.escape(stage) + r"(?![A-Za-z0-9.-])", heading
        ) is None:
            failures.append(CheckFailure(status, "current_stage must match the selected plan heading"))
        if register is not None and (register.parent != item.parent or register.name != "findings.md"):
            failures.append(CheckFailure(status, "current_register must be the selected plan's findings.md"))
    for heading in ("当前任务", "阶段状态", "当前遗留问题与生产关闭前置"):
        if f"## {heading}" not in lines_outside_fences(text):
            failures.append(CheckFailure(status, f"current status is missing section {heading}"))
    updated = re.search(r"^更新日期：(\d{4}-\d{2}-\d{2})$", text, re.M)
    try:
        date.fromisoformat(updated[1] if updated else "")
    except ValueError:
        failures.append(CheckFailure(status, "current status update date must be a calendar date"))


def target_block(path: Path, failures: list[CheckFailure]) -> list[str] | None:
    lines = path.read_text(encoding="utf-8").splitlines()
    begin_lines = [index for index, line in enumerate(lines) if line == TARGET_BLOCK_BEGIN]
    end_lines = [index for index, line in enumerate(lines) if line == TARGET_BLOCK_END]
    if len(begin_lines) != 1 or len(end_lines) != 1 or begin_lines[0] >= end_lines[0]:
        failures.append(CheckFailure(path, "target contract: expected one ordered begin/end marker pair"))
        return None
    begin = begin_lines[0]
    end = end_lines[0]
    if end - begin < 3 or lines[begin + 1] != "```text" or lines[end - 1] != "```":
        failures.append(CheckFailure(path, "target contract: expected a text code block between markers"))
        return None
    targets = lines[begin + 2 : end - 1]
    if not targets or any(not re.fullmatch(r"cuexis_[a-z0-9_]+", target) for target in targets):
        failures.append(CheckFailure(path, "target contract: expected nonempty cuexis_* target names"))
        return None
    if len(targets) != len(set(targets)):
        failures.append(CheckFailure(path, "target contract: target block contains duplicates"))
        return None
    return targets


def generated_target_facts(failures: list[CheckFailure]) -> Path | None:
    configured = os.environ.get(TARGET_FACTS_ENV)
    if configured is None:
        return DEFAULT_TARGET_FACTS if DEFAULT_TARGET_FACTS.is_file() else None
    path = Path(configured)
    candidate = path.resolve() if path.is_absolute() else (ROOT / path).resolve()
    if not is_within(candidate, ROOT.resolve()):
        failures.append(CheckFailure(BUILDING_GUIDE, f"target contract: {TARGET_FACTS_ENV} escapes repository"))
        return None
    if not candidate.is_file():
        failures.append(
            CheckFailure(BUILDING_GUIDE, f"target contract: {TARGET_FACTS_ENV} does not name a generated file")
        )
        return None
    return candidate


def check_target_contract(
    failures: list[CheckFailure], building: Path = BUILDING_GUIDE, facts: Path | None = None
) -> None:
    documented = target_block(building, failures)
    if documented is None:
        return
    target_facts = facts if facts is not None else generated_target_facts(failures)
    if target_facts is None:
        print("NOT CHECKED: CMake target facts are missing; set CUEXIS_TARGETS_FILE to fresh configure output")
        if os.environ.get("CUEXIS_DOCS_STRICT") == "1":
            failures.append(CheckFailure(building, "strict documentation check requires generated target facts"))
        return
    generated = [line.strip() for line in target_facts.read_text(encoding="utf-8").splitlines() if line.strip()]
    if not generated or any(not re.fullmatch(r"cuexis_[a-z0-9_]+", target) for target in generated):
        failures.append(CheckFailure(target_facts, "target contract: generated target facts are invalid"))
        return
    if len(generated) != len(set(generated)):
        failures.append(CheckFailure(target_facts, "target contract: generated target facts contain duplicates"))
        return
    if documented == generated:
        return
    mismatch = next(
        (index for index, (left, right) in enumerate(zip(documented, generated), start=1) if left != right),
        min(len(documented), len(generated)) + 1,
    )
    expected = generated[mismatch - 1] if mismatch <= len(generated) else "<end>"
    actual = documented[mismatch - 1] if mismatch <= len(documented) else "<end>"
    failures.append(
        CheckFailure(
            building,
            f"target contract: generated target facts differ at line {mismatch}: expected {expected}, found {actual}",
        )
    )


def check_sdk_api_contract(
    failures: list[CheckFailure], building: Path = BUILDING_GUIDE, version_file: Path = VERSION_CMAKE
) -> None:
    version_text = version_file.read_text(encoding="utf-8")
    match = re.search(r'^set\(CUEXIS_SDK_API_VERSION "(\d+\.\d+\.\d+)"\)$', version_text, re.MULTILINE)
    if match is None:
        failures.append(CheckFailure(version_file, "SDK API contract: CUEXIS_SDK_API_VERSION is missing"))
        return
    sdk_version = match.group(1)
    sdk_minor = ".".join(sdk_version.split(".")[:2])
    documented_versions = re.findall(r"find_package\(Cuexis\s+(\d+\.\d+)\s+CONFIG", building.read_text(encoding="utf-8"))
    if not documented_versions:
        failures.append(CheckFailure(building, "SDK API contract: find_package(Cuexis <major.minor> ...) is missing"))
        return
    for documented_version in documented_versions:
        if documented_version != sdk_minor:
            failures.append(
                CheckFailure(
                    building,
                    f"SDK API contract: find_package requests {documented_version}, expected {sdk_minor} from {sdk_version}",
                )
            )


def check_script_policy(failures: list[CheckFailure]) -> None:
    required = {
        DOCS / "CURRENT_STATUS.md",
        DOCS / "DOCUMENTATION_POLICY.md",
        DOCS / "adr" / "0038-cxc-v1-and-chart-v4-boundary.md",
        DOCS / "formats" / "CXT_FORMAT.md",
    }
    phrase = "\u65e0\u9650\u671f\u5ef6\u540e"
    for path in required:
        if phrase not in path.read_text(encoding="utf-8"):
            failures.append(CheckFailure(path, "missing runtime-script indefinite-deferral policy"))
    for path in EXAMPLES.rglob("*"):
        if path.suffix.lower() not in {".json", ".cxt"} or not path.is_file():
            continue
        if "runtime_script" in path.name:
            continue
        try:
            value = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            continue
        for key in find_keys(value):
            if key.casefold() in SCRIPT_KEYS:
                failures.append(CheckFailure(path, f"runtime-script field is not allowed: {key}"))


def check_future_stage_plans(failures: list[CheckFailure]) -> None:
    for relative, allowed_statuses in STAGE_PLAN_REQUIREMENTS:
        path = DOCS / relative
        if not path.is_file():
            failures.append(CheckFailure(path, "required stage plan is missing"))
            continue
        text = path.read_text(encoding="utf-8")
        for heading in ("阶段目标", "验收标准"):
            pattern = rf"^##\s+(?:\d+\.\s+)?{heading}\s*$"
            if not re.search(pattern, text, re.MULTILINE):
                failures.append(CheckFailure(path, f"missing required section: {heading}"))
        if "归档来源：" not in text:
            failures.append(CheckFailure(path, "missing archived source reference"))
        expected = "|".join(re.escape(status) for status in allowed_statuses)
        if not re.search(rf"^状态：(?:{expected})；", text, re.MULTILINE):
            failures.append(
                CheckFailure(path, f"status must be one of: {', '.join(allowed_statuses)}")
            )


def check_stage_directory_layout(failures: list[CheckFailure]) -> None:
    for root in (DOCS / "stage_plans", DOCS / "stage_reports"):
        for path in root.glob("*.md"):
            if path.name in {"README.md", "legacy-paths.md"}:
                continue
            failures.append(CheckFailure(path, "stage root may contain only README.md and legacy-paths.md"))
        for path in root.rglob("README.md"):
            relative = path.relative_to(DOCS).as_posix()
            if relative not in STAGE_NAVIGATION_INDEXES:
                failures.append(CheckFailure(path, "leaf stage directory must be indexed by its nearest navigation hub"))


def check_root_document_layout(failures: list[CheckFailure]) -> None:
    for path in DOCS.glob("*.md"):
        if path.name not in ROOT_DOCUMENTATION_FILES:
            failures.append(CheckFailure(path, "docs root contains a non-entry document"))
    if not STATUS_CONTRACT.is_file():
        failures.append(CheckFailure(STATUS_CONTRACT, "documentation status contract is missing"))


def check_api_reference_style(failures: list[CheckFailure]) -> None:
    version_text = VERSION_CMAKE.read_text(encoding="utf-8")
    version_match = re.search(
        r'^set\(CUEXIS_SDK_API_VERSION "(\d+\.\d+\.\d+)"\)$', version_text, re.MULTILINE
    )
    if version_match is None:
        failures.append(CheckFailure(VERSION_CMAKE, "API reference style: SDK API version is missing"))
        return
    expected_version = f"适用版本：SDK API `{version_match.group(1)}`"
    metadata_pattern = re.compile(
        rf"^#\s+\S.*\n\n状态：active\n\n更新日期：\d{{4}}-\d{{2}}-\d{{2}}"
        rf"\n\n{re.escape(expected_version)}\n\n文档角色：\S",
        re.MULTILINE,
    )
    for name in API_REFERENCE_FILES:
        path = DOCS / "api" / name
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8")
        if metadata_pattern.search(text) is None:
            failures.append(CheckFailure(path, "API reference style: metadata block is inconsistent"))
        headings = re.findall(r"^##\s+(.+)$", text, re.MULTILINE)
        expected_first = "先记住这五条" if name == "README.md" else "快速结论"
        if not headings or headings[0] != expected_first:
            failures.append(CheckFailure(path, f"API reference style: first section must be {expected_first}"))
        for number, line in enumerate(text.splitlines(), start=1):
            if re.fullmatch(r"#{1,2}\s+[A-Za-z][A-Za-z ]+", line):
                failures.append(
                    CheckFailure(path, f"API reference style: natural-language heading must be Chinese at line {number}")
                )


def check_navigation_indexes(failures: list[CheckFailure]) -> None:
    for relative in DIRECTORY_INDEXES:
        path = DOCS / relative
        if not path.is_file():
            failures.append(CheckFailure(path, "required documentation index is missing"))

    stage_index = DOCS / "stage_plans" / "README.md"
    if stage_index.is_file():
        text = stage_index.read_text(encoding="utf-8")
        for label in REQUIRED_STAGE_LABELS:
            if label not in text:
                failures.append(CheckFailure(stage_index, f"stage index is missing {label}"))

    docs_index = DOCS / "README.md"
    if docs_index.is_file():
        text = docs_index.read_text(encoding="utf-8")
        for target in (
            "PROJECT_GUIDE.md",
            "architecture/README.md",
            "guides/README.md",
            "api/README.md",
            "proposals/README.md",
            "examples/README.md",
        ):
            if f"]({target})" not in text:
                failures.append(CheckFailure(docs_index, f"main index does not link {target}"))

    for name in API_REFERENCE_FILES:
        path = DOCS / "api" / name
        if not path.is_file():
            failures.append(CheckFailure(path, "required API reference is missing"))


def check_agents_guide(failures: list[CheckFailure]) -> None:
    if not AGENTS.is_file():
        failures.append(CheckFailure(AGENTS, "repository agent guide is missing"))
        return

    text = AGENTS.read_text(encoding="utf-8")

    text = AGENTS.read_text(encoding="utf-8")
    required_fragments = (
        "docs/MAP.md",
        "docs/CURRENT_STATUS.md",
        "docs/DOCUMENTATION_POLICY.md",
        "python -B tools/check_docs.py",
        "python -B tools/verification/check_docs_facts.py",
    )
    for fragment in required_fragments:
        if fragment not in text:
            failures.append(CheckFailure(AGENTS, f"missing current documentation guidance: {fragment}"))

    stale_fragments = (
        "SDK API `0.3.0`",
        "phase 12",
        "phase 0-12 SDK migration route",
        "Format: `yy.mm.dd.hh-v[-suffix]`",
    )
    folded = text.casefold()
    for fragment in stale_fragments:
        if fragment.casefold() in folded:
            failures.append(CheckFailure(AGENTS, f"contains stale baseline wording: {fragment}"))


def check_agents_links(failures: list[CheckFailure]) -> None:
    """AGENTS.md is repository-root relative; every local link must resolve."""
    if not AGENTS.is_file():
        return
    text = AGENTS.read_text(encoding="utf-8")
    for match in re.finditer(r"\]\(([^)\s]+)\)", text):
        target = match.group(1)
        if target.startswith(("http://", "https://", "mailto:", "#")):
            continue
        destination = target.partition("#")[0]
        if not destination:
            continue
        resolved = (AGENTS.parent / destination).resolve()
        if not resolved.exists():
            failures.append(CheckFailure(AGENTS, f"broken relative link: {target}"))


def find_keys(value: Any) -> list[str]:
    keys: list[str] = []
    if isinstance(value, dict):
        for key, child in value.items():
            keys.append(key)
            keys.extend(find_keys(child))
    elif isinstance(value, list):
        for child in value:
            keys.extend(find_keys(child))
    return keys


def find_dicts(value: Any) -> list[dict[str, Any]]:
    result: list[dict[str, Any]] = []
    if isinstance(value, dict):
        result.append(value)
        for child in value.values():
            result.extend(find_dicts(child))
    elif isinstance(value, list):
        for child in value:
            result.extend(find_dicts(child))
    return result


def has_animator_deep_patch(value: Any) -> bool:
    for item in find_dicts(value):
        path = item.get("path")
        if isinstance(path, str) and path.startswith("/components/cuexis.animator/"):
            return True
    return False


def has_parameterized_asset(value: Any) -> bool:
    for item in find_dicts(value):
        renderable = item.get("cuexis.renderable")
        if not isinstance(renderable, dict):
            continue
        for field in ("mesh", "material"):
            field_value = renderable.get(field)
            if isinstance(field_value, dict) and "parameter" in field_value:
                return True
    return False


def has_partial_discrete_weight(value: Any) -> bool:
    has_discrete_track = any(
        item.get("property") in {"render.visible", "render.material"}
        and isinstance(item.get("steps"), list)
        for item in find_dicts(value)
    )
    has_partial_layer_or_group = any(
        isinstance(item.get("weight"), (int, float))
        and not isinstance(item.get("weight"), bool)
        and item.get("weight") != 1
        and ("layerId" in item or "groupId" in item)
        for item in find_dicts(value)
    )
    return has_discrete_track and has_partial_layer_or_group


def has_mask_internal_overlap(value: Any) -> bool:
    for item in find_dicts(value):
        properties = item.get("properties")
        prefixes = item.get("prefixes")
        if not isinstance(properties, list) or not isinstance(prefixes, list):
            continue
        string_properties = [entry for entry in properties if isinstance(entry, str)]
        string_prefixes = [entry for entry in prefixes if isinstance(entry, str)]
        if len(string_properties) != len(set(string_properties)):
            return True
        if len(string_prefixes) != len(set(string_prefixes)):
            return True
        if any(prop.startswith(prefix) for prop in string_properties for prefix in string_prefixes):
            return True
        if any(
            left != right and left.startswith(right)
            for left in string_prefixes
            for right in string_prefixes
        ):
            return True
    return False


def reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def parse_candidate(path: Path, failures: list[CheckFailure]) -> Any | None:
    try:
        return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=reject_duplicate_keys)
    except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as error:
        failures.append(CheckFailure(path, f"invalid JSON: {error}"))
        return None


def check_candidate_examples(failures: list[CheckFailure]) -> int:
    candidates: dict[Path, Any] = {}
    for path in sorted(EXAMPLES.rglob("*")):
        if path.is_file() and path.suffix.lower() in {".json", ".cxt"}:
            value = parse_candidate(path, failures)
            if value is not None:
                candidates[path] = value

    for path, value in candidates.items():
        if not isinstance(value, dict):
            failures.append(CheckFailure(path, "candidate root must be a JSON object"))
            continue
        name = path.name
        expected_invalid = ".invalid." in name
        format_id = value.get("format")
        if path.suffix.lower() == ".cxt":
            if format_id != "cuexis.animation-template" or value.get("version") != 1:
                failures.append(CheckFailure(path, "CXT candidate must use cuexis.animation-template v1"))
            if not expected_invalid and any(key.casefold() in SCRIPT_KEYS for key in find_keys(value)):
                failures.append(CheckFailure(path, "valid CXT candidate contains a runtime-script field"))
            if expected_invalid and "runtime_script" in name:
                if not any(key.casefold() in SCRIPT_KEYS for key in find_keys(value)):
                    failures.append(CheckFailure(path, "runtime-script fixture has no forbidden script field"))
        elif format_id == "cuexis.cxc":
            entries = value.get("entries")
            if not isinstance(entries, list):
                failures.append(CheckFailure(path, "CXC candidate entries must be an array"))
                continue
            paths = [entry.get("path") for entry in entries if isinstance(entry, dict)]
            if len(paths) != len(entries) or any(not isinstance(entry_path, str) for entry_path in paths):
                failures.append(CheckFailure(path, "CXC entries must contain string paths"))
                continue
            duplicate_paths = len(paths) != len(set(paths))
            if duplicate_paths:
                failures.append(CheckFailure(path, "CXC entries contain duplicate paths"))
            casefold_groups: dict[str, set[str]] = {}
            for entry_path in paths:
                casefold_groups.setdefault(entry_path.casefold(), set()).add(entry_path)
            case_conflict = any(len(group) > 1 for group in casefold_groups.values())
            if expected_invalid and "case_conflict" in name:
                if not case_conflict:
                    failures.append(CheckFailure(path, "case-conflict fixture has no case-folded duplicate"))
            elif not expected_invalid and case_conflict:
                failures.append(CheckFailure(path, "valid CXC entries contain a case-folded duplicate"))
            sorted_paths = sorted(paths)
            if expected_invalid and "unsorted" in name:
                if paths == sorted_paths:
                    failures.append(CheckFailure(path, "unsorted CXC fixture unexpectedly has sorted entries"))
            elif not expected_invalid and paths != sorted_paths:
                failures.append(CheckFailure(path, "valid CXC entries must be path-sorted"))
        elif format_id == "cuexis.chart":
            if value.get("version") != 4 and "chart_v4" in name:
                failures.append(CheckFailure(path, "Chart v4 candidate must use version 4"))
            imports = value.get("animationTemplateImports", [])
            if not isinstance(imports, list):
                failures.append(CheckFailure(path, "animationTemplateImports must be an array"))
                continue
            missing = []
            mismatched_ids = []
            for item in imports:
                source = item.get("source") if isinstance(item, dict) else None
                if not isinstance(source, str) or not (EXAMPLES / source).is_file():
                    missing.append(source)
                    continue
                imported = candidates.get(EXAMPLES / source)
                import_id = item.get("id") if isinstance(item, dict) else None
                if not isinstance(imported, dict) or imported.get("templateId") != import_id:
                    mismatched_ids.append(source)
            if expected_invalid and "missing_import" in name:
                if not missing:
                    failures.append(CheckFailure(path, "missing-import fixture has no missing import"))
            elif missing:
                failures.append(CheckFailure(path, f"candidate has unexpected missing imports: {missing}"))
            if expected_invalid and "id_mismatch" in name:
                if not mismatched_ids:
                    failures.append(CheckFailure(path, "ID-mismatch fixture has matching CXT templateId"))
            elif mismatched_ids:
                failures.append(
                    CheckFailure(path, f"candidate has unexpected mismatched CXT IDs: {mismatched_ids}")
                )

            if expected_invalid and "animator_deep_patch" in name:
                if not has_animator_deep_patch(value):
                    failures.append(CheckFailure(path, "animator deep-patch fixture has no deep patch"))
            if expected_invalid and "parameter_asset_use" in name:
                if not has_parameterized_asset(value):
                    failures.append(CheckFailure(path, "parameter asset-use fixture has no asset ParameterRef"))
            if expected_invalid and "discrete_partial_weight" in name:
                if not has_partial_discrete_weight(value):
                    failures.append(
                        CheckFailure(path, "discrete-weight fixture lacks a discrete track or partial weight")
                    )
            if expected_invalid and "mask_overlap" in name:
                if not has_mask_internal_overlap(value):
                    failures.append(CheckFailure(path, "mask-overlap fixture has no internal mask overlap"))
    return len(candidates)


ACTIVE_PLAN_SECTIONS = (
    ("阶段目标",),
    ("验收标准",),
    ("排除", "不包含"),
    ("前置基线", "前置条件", "前置"),
    ("交接", "进展记录", "当前状态"),
)
REPORT_FORMAT_MIN_DATE = "2026-11-17"


def _has_any(headings: str, words: tuple[str, ...]) -> bool:
    return any(word in headings for word in words)


def check_active_plan_sections(failures: list[CheckFailure]) -> None:
    for path in sorted((DOCS / "stage_plans" / "active").rglob("plan.md")):
        headings = "\n".join(
            line for line in lines_outside_fences(path.read_text(encoding="utf-8"))
            if line.startswith("## ")
        )
        for group in ACTIVE_PLAN_SECTIONS:
            if not _has_any(headings, group):
                failures.append(CheckFailure(
                    path,
                    "active plan is missing a section containing one of: " + " / ".join(group),
                ))


def check_recent_report_sections(failures: list[CheckFailure]) -> None:
    for path in sorted((DOCS / "stage_reports" / "stages").rglob("*.md")):
        match = re.match(r"(\d{4}-\d{2}-\d{2})-", path.name)
        if match is None or match.group(1) < REPORT_FORMAT_MIN_DATE:
            continue
        text = path.read_text(encoding="utf-8")
        if "cuexis-report-format-exempt" in text:
            continue
        headings = "\n".join(
            line for line in lines_outside_fences(text) if line.startswith("## ")
        )
        if not _has_any(headings, ("证据", "验证", "复核")):
            failures.append(CheckFailure(
                path, "report is missing an evidence/verification section"
            ))
        if not _has_any(headings, ("限制", "未做", "未覆盖", "未复核", "边界")):
            failures.append(CheckFailure(
                path, "report is missing a limits/not-done section"
            ))


def check_evidence_metadata(failures: list[CheckFailure]) -> None:
    """Reports need machine metadata; README indexes are routing and stay exempt."""
    for path in sorted((DOCS / "stage_reports").rglob("*.md")):
        if path.name in {"README.md", "legacy-paths.md"}:
            continue
        text = path.read_text(encoding="utf-8")
        if "<!-- cuexis-meta" not in text:
            failures.append(CheckFailure(path, "evidence report is missing its cuexis-meta block"))
            continue
        if not re.search(r"(?m)^status:\s*evidence\s*$", text):
            failures.append(CheckFailure(path, "evidence report must declare status: evidence"))
        if not re.search(r"(?m)^authority:\s*none\s*$", text):
            failures.append(CheckFailure(path, "evidence report must declare authority: none"))


def check_governance(failures: list[CheckFailure]) -> None:
    for name in ("check_findings", "check_docs_governance", "check_source_naming"):
        path = ROOT / "tools" / "verification" / (name + ".py")
        spec = importlib.util.spec_from_file_location(name, path)
        if spec is None or spec.loader is None:
            failures.append(CheckFailure(path, "cannot load governance checker"))
            continue
        module = importlib.util.module_from_spec(spec)
        sys.modules[name] = module
        try:
            spec.loader.exec_module(module)
            errors, notices = module.check_repository(ROOT)
            failures.extend(CheckFailure(p, message) for p, message in errors)
            for notice in notices:
                print("NOTICE: " + notice)
        except Exception as error:
            failures.append(CheckFailure(path, f"governance checker could not complete: {error}"))


def main() -> int:
    failures: list[CheckFailure] = []
    files = markdown_files()
    graph = check_h1_and_links(files, failures)
    check_stage7_section_links(files, failures)
    check_reachability(graph, failures)
    check_cfu_status(failures)
    check_target_contract(failures)
    check_sdk_api_contract(failures)
    check_script_policy(failures)
    check_future_stage_plans(failures)
    check_active_plan_sections(failures)
    check_stage7_rr_plan_schedule(failures)
    check_recent_report_sections(failures)
    check_evidence_metadata(failures)
    check_stage_directory_layout(failures)
    check_root_document_layout(failures)
    check_api_reference_style(failures)
    check_navigation_indexes(failures)
    check_agents_guide(failures)
    check_agents_links(failures)
    check_governance(failures)
    candidate_count = check_candidate_examples(failures)
    if failures:
        print("Documentation checks failed:")
        for failure in failures:
            print(f"- {failure}")
        return 1
    print(
        f"Documentation checks passed: {len(files)} Markdown files and "
        f"{candidate_count} candidate JSON/CXT files validated."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
