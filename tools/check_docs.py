"""Repository documentation consistency checks.

This checker intentionally validates documentation and candidate examples only. It does not
pretend that candidate Chart/CXC/CXT files are production schemas.
"""

from __future__ import annotations

import json
import os
import re
import sys
from collections import deque
from datetime import date
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
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
    ("stage_plans/future/stage-06/plan.md", ("future",)),
    ("stage_plans/future/stage-07/plan.md", ("future",)),
    ("stage_plans/future/stage-08/plan.md", ("future",)),
    ("stage_plans/future/stage-09a/plan.md", ("future",)),
    ("stage_plans/deferred/stage-09b/plan.md", ("deferred",)),
    ("stage_plans/deferred/stage-10/plan.md", ("deferred",)),
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
    "stage_reports/chart-format-update/README.md",
    "stage_reports/reviews/full-review-2026-08/README.md",
    "stage_reports/stages/stage-01/README.md",
    "stage_reports/stages/stage-04/README.md",
    "stage_reports/stages/stage-05/README.md",
)
STAGE_NAVIGATION_INDEXES = {
    "stage_plans/README.md",
    "stage_reports/README.md",
    "stage_reports/chart-format-update/README.md",
    "stage_reports/reviews/full-review-2026-08/README.md",
    "stage_reports/stages/stage-01/README.md",
    "stage_reports/stages/stage-04/README.md",
    "stage_reports/stages/stage-05/README.md",
}
ROOT_DOCUMENTATION_FILES = {
    "README.md",
    "CURRENT_STATUS.md",
    "PROJECT_GUIDE.md",
    "ROADMAP.md",
    "DOCUMENTATION_POLICY.md",
    "legacy-paths.md",
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


def normalize_whitespace(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()


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


def load_status_contract(
    contract: Path, root: Path, failures: list[CheckFailure]
) -> tuple[dict[Path, tuple[str, ...]], tuple[str, ...], tuple[Path, ...], str] | None:
    failure_count = len(failures)
    try:
        value = json.loads(contract.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        failures.append(CheckFailure(contract, f"invalid status contract: cannot read JSON: {error}"))
        return None
    if not isinstance(value, dict):
        failures.append(CheckFailure(contract, "invalid status contract: root must be a JSON object"))
        return None

    snapshot_date = value.get("snapshotDate")
    if not isinstance(snapshot_date, str) or not re.fullmatch(r"\d{4}-\d{2}-\d{2}", snapshot_date):
        failures.append(CheckFailure(contract, "invalid status contract snapshotDate: expected YYYY-MM-DD"))
        return None
    try:
        date.fromisoformat(snapshot_date)
    except ValueError:
        failures.append(CheckFailure(contract, "invalid status contract snapshotDate: expected a calendar date"))
        return None

    raw_required = value.get("requiredFragments")
    if not isinstance(raw_required, dict):
        failures.append(CheckFailure(contract, "invalid status contract requiredFragments: expected an object"))
        return None
    required: dict[Path, tuple[str, ...]] = {}
    for relative, fragments in raw_required.items():
        if not isinstance(relative, str):
            failures.append(CheckFailure(contract, "invalid status contract requiredFragments: path must be a string"))
            continue
        path = status_contract_path(root, relative, "requiredFragments", failures, contract)
        if path is None:
            continue
        if not isinstance(fragments, list) or not fragments or any(
            not isinstance(fragment, str) or not fragment.strip() for fragment in fragments
        ):
            failures.append(
                CheckFailure(
                    contract,
                    f"invalid status contract requiredFragments for {relative}: expected nonempty strings",
                )
            )
            continue
        required[path] = tuple(fragments)

    raw_stale = value.get("staleFragments")
    if not isinstance(raw_stale, list) or not raw_stale:
        failures.append(CheckFailure(contract, "invalid status contract staleFragments: expected a nonempty array"))
        return None
    stale: list[str] = []
    for index, entry in enumerate(raw_stale):
        if not isinstance(entry, dict):
            failures.append(CheckFailure(contract, f"invalid status contract staleFragments[{index}]: expected an object"))
            continue
        fragment = entry.get("fragment")
        minimum_length = entry.get("minLength")
        if not isinstance(fragment, str) or not fragment.strip():
            failures.append(
                CheckFailure(contract, f"invalid status contract staleFragments[{index}].fragment: expected text")
            )
            continue
        if isinstance(minimum_length, bool) or not isinstance(minimum_length, int) or minimum_length < 8:
            failures.append(
                CheckFailure(
                    contract,
                    f"invalid status contract staleFragments[{index}].minLength: expected integer >= 8",
                )
            )
            continue
        if len(normalize_whitespace(fragment)) < minimum_length:
            failures.append(
                CheckFailure(
                    contract,
                    f"invalid status contract staleFragments[{index}]: fragment is shorter than minLength",
                )
            )
            continue
        stale.append(fragment)

    raw_dated = value.get("datedFiles")
    if not isinstance(raw_dated, list) or not raw_dated:
        failures.append(CheckFailure(contract, "invalid status contract datedFiles: expected a nonempty array"))
        return None
    dated: list[Path] = []
    for relative in raw_dated:
        if not isinstance(relative, str):
            failures.append(CheckFailure(contract, "invalid status contract datedFiles: path must be a string"))
            continue
        path = status_contract_path(root, relative, "datedFiles", failures, contract)
        if path is not None:
            dated.append(path)

    if len(failures) != failure_count:
        return None
    return required, tuple(stale), tuple(dated), snapshot_date


def check_cfu_status(
    failures: list[CheckFailure], contract: Path = STATUS_CONTRACT, root: Path = ROOT
) -> None:
    status_contract = load_status_contract(contract, root, failures)
    if status_contract is None:
        return
    required_fragments, stale_fragments, dated_status_files, snapshot_date = status_contract
    for path, required in required_fragments.items():
        text = normalize_whitespace(path.read_text(encoding="utf-8"))
        compact_text = re.sub(r"\s+", "", text)
        for fragment in required:
            if fragment not in text and re.sub(r"\s+", "", fragment) not in compact_text:
                failures.append(
                    CheckFailure(
                        path,
                        f"status contract requiredFragments: missing current CFU status: {fragment}",
                    )
                )
        for fragment in stale_fragments:
            if fragment in text or re.sub(r"\s+", "", fragment) in compact_text:
                failures.append(
                    CheckFailure(
                        path,
                        f"status contract staleFragments: contains stale CFU status: {fragment}",
                    )
                )

    for path in dated_status_files:
        text = path.read_text(encoding="utf-8")
        match = re.search(r"^更新日期：(\d{4}-\d{2}-\d{2})$", text, re.MULTILINE)
        if match is None or match.group(1) < snapshot_date:
            failures.append(
                CheckFailure(
                    path,
                    f"status contract datedFiles: expected 更新日期 on or after {snapshot_date}",
                )
            )


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
    required_fragments = (
        "SDK API `0.6.0`",
        "Stage Chart Format Update",
        "python -B tools/check_docs.py",
        "python -B tools/update_version.py",
        "Format: `yy.mm.dd-v[-suffix]`",
        "`docs/README.md`",
        "`docs/CURRENT_STATUS.md`",
        "`docs/guides/README.md`",
        "`docs/stage_plans/completed/stage-04/plan.md`",
        "`docs/stage_plans/future/stage-12/plan.md`",
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


def main() -> int:
    failures: list[CheckFailure] = []
    files = markdown_files()
    graph = check_h1_and_links(files, failures)
    check_reachability(graph, failures)
    check_cfu_status(failures)
    check_target_contract(failures)
    check_sdk_api_contract(failures)
    check_script_policy(failures)
    check_future_stage_plans(failures)
    check_stage_directory_layout(failures)
    check_root_document_layout(failures)
    check_api_reference_style(failures)
    check_navigation_indexes(failures)
    check_agents_guide(failures)
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
