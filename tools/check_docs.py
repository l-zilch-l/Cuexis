"""Repository documentation consistency checks.

This checker intentionally validates documentation and candidate examples only. It does not
pretend that candidate Chart/CXC/CXT files are production schemas.
"""

from __future__ import annotations

import json
import re
import sys
from collections import deque
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
AGENTS = ROOT / "AGENTS.md"
DOCS = ROOT / "docs"
DOCS_INDEX = DOCS / "README.md"
EXAMPLES = DOCS / "examples" / "chart_format_update"
FUTURE_STAGE_PLANS = (
    "stage_5_implementation_plan.md",
    "stage_6_implementation_plan.md",
    "stage_7_implementation_plan.md",
    "stage_8_implementation_plan.md",
    "stage_9a_implementation_plan.md",
    "stage_9b_implementation_plan.md",
    "stage_10_implementation_plan.md",
    "stage_11_implementation_plan.md",
    "stage_12_implementation_plan.md",
)
DIRECTORY_INDEXES = (
    "README.md",
    "adr/README.md",
    "architecture/README.md",
    "archive/README.md",
    "examples/README.md",
    "formats/README.md",
    "guides/README.md",
    "proposals/README.md",
    "proposals/deferred/README.md",
    "stage_plans/README.md",
    "stage_reports/README.md",
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


def check_stage_name(files: list[Path], failures: list[CheckFailure]) -> None:
    for path in files:
        if "archive" in path.relative_to(DOCS).parts:
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
            if re.search(r"Stage\s*3\.5|stage[_ -]?3[_ -]?5", line, re.IGNORECASE):
                if "不使用" not in line and "not use" not in line.lower():
                    failures.append(CheckFailure(path, f"legacy Stage 3.5 name at line {number}"))
    current = (DOCS / "CURRENT_STATUS.md").read_text(encoding="utf-8")
    roadmap = (DOCS / "ROADMAP.md").read_text(encoding="utf-8")
    if "Stage Chart Format Update" not in current or "Stage Chart Format Update" not in roadmap:
        failures.append(CheckFailure(DOCS_INDEX, "canonical Stage Chart Format Update name is missing"))


def check_cfu_status(failures: list[CheckFailure]) -> None:
    status_contracts = {
        AGENTS: (
            "CFU-F closed after final-SHA hosted verification",
            "CFU-G closed after explicit owner acceptance on August 24, 2026",
            "G0 status calibration, G1 exit audit, and G2 Stage 4 typed handoff are complete",
            "G3 local candidate gates passed on August 19, 2026",
            "passed same-SHA hosted Linux Quality, Windows MSVC",
            "G3 hosted validation and G4 are complete",
            "G5 report-SHA hosted revalidation passed",
            "G6 owner acceptance is recorded",
            "Stage Chart Format Update is complete",
            "Stage 4 is complete",
            "Stage 5 S5-A, S5-B, S5-C, S5-D, S5-E, S5-F, and S5-G are complete; S5-H is not started",
        ),
        DOCS / "CURRENT_STATUS.md": (
            "G3 hosted 门禁与 G4 已完成",
            "18 PASS / 0 PENDING / 0 PRODUCT BLOCKER",
            "CFU-G2 已于同日完成 Stage 4 typed handoff",
            "CFU-G3 已于 2026-08-19",
            "G5 completion report SHA",
            "hosted revalidation 均已通过",
            "CFU-G 与 Stage Chart Format Update 已关闭",
            "CFU-G6 已于 2026-08-24 完成",
            "Stage 4 已完成",
            "S5-A 合同已冻结",
            "S5-B 已接线",
            "S5-C 已完成",
            "S5-D 已完成",
            "S5-E 已完成",
            "S5-F 已完成",
            "S5-G 已完成",
        ),
        DOCS / "ROADMAP.md": (
            "CFU-F consumers and determinism closed; final-SHA hosted gates passed 2026-08-16",
            "Stage 4 已完成；当前下一阶段是 **Stage 5**",
            "CFU-G final closure                                 completed; G6 owner acceptance recorded 2026-08-24",
        ),
        DOCS / "PROJECT_GUIDE.md": (
            "CFU-F 已在最终实现 SHA 上关闭跨平台 consumer、确定性、安全与性能门禁",
            "G1 退出审计和 G2 Stage 4 typed handoff 已完成",
            "G3 本地候选门禁已于 2026-08-19 通过",
            "hosted Linux/MSVC/MinGW 验证已于 2026-08-24",
            "G3 hosted 与 G4 已完成",
            "G5 completion report SHA",
            "CFU-G 与 Stage Chart Format Update 已关闭",
            "Stage 4 已完成",
            "S5-A 合同已冻结",
            "S5-B 已接线",
            "S5-C 已完成",
            "S5-D 已完成",
            "S5-E 已完成",
            "S5-F 已完成",
            "S5-G 已完成",
        ),
        DOCS / "formats" / "README.md": (
            "CFU-G hosted 验证、completion report 和 owner acceptance 已完成",
        ),
        DOCS / "formats" / "CHART_V4_FORMAT.md": (
            "G5 report-SHA hosted revalidation 与 G6 owner acceptance 已完成",
        ),
        DOCS / "formats" / "CXT_FORMAT.md": (
            "G5 report-SHA hosted revalidation 与 G6 owner acceptance 已完成",
        ),
        DOCS / "formats" / "CXC_FORMAT.md": (
            "CFU-G 经项目所有者接受",
        ),
        DOCS / "stage_plans" / "README.md": (
            "CFU-D/CFU-E/CFU-F/G 已关闭；G6 owner acceptance recorded 2026-08-24",
        ),
        DOCS / "stage_plans" / "stage_chart_format_update_implementation_plan.md": (
            "CFU-A/CFU-B、CFU-C0/C1/C2/C3/C4、CFU-D、CFU-E、CFU-F、CFU-G 已完成并封存",
            "CFU-G1 审计报告",
            "CFU-G2 交接报告",
            "CFU-G3 验证报告",
            "最终候选 SHA",
            "CFU-G4 hosted 验证报告",
            "G3 hosted 与 G4 已完成",
            "G5/G6 completion report",
            "CFU-G 与 Stage Chart Format Update 已关闭",
            "Stage 4 已切换为 unblocked / not started",
        ),
        DOCS / "stage_plans" / "stage_4_implementation_plan.md": (
            "CFU-G2 Stage 4 typed handoff",
            "项目所有者已接受 completion report",
            "CFU-G4 hosted verification",
            "G5 report-SHA hosted revalidation 与 G6 owner acceptance",
            "unblocked / not started",
        ),
        DOCS / "stage_reports" / "README.md": (
            "260816 Chart Format Update CFU-G1 exit audit",
            "260816 Chart Format Update CFU-G2 Stage 4 handoff",
            "260819 Chart Format Update CFU-G3 validation",
            "260820 Chart Format Update CFU-G4 closure readiness",
            "260824 Chart Format Update CFU-G4 hosted verification",
            "Stage Chart Format Update completion report",
            "G6 owner acceptance recorded 2026-08-24",
            "Stage 4 completion report",
        ),
        DOCS / "stage_reports" / "stage_chart_format_update_completion_report.md": (
            "G6 complete",
            "项目所有者接受记录：**已接受",
            "G5 report-SHA hosted revalidation",
            "18 PASS / 0 PENDING",
            "Stage 4 `unblocked but not started`",
            "`.codex/` is intentionally excluded",
        ),
    }
    stale_fragments = (
        "CFU-G is active",
        "G5 completion report candidate",
        "G5/G6 pending",
        "G6 owner acceptance pending",
        "report-SHA hosted/G6 pending",
        "等待 report-SHA hosted revalidation",
        "Stage 4 stays blocked / not started",
        "CFU-F is next",
        "CFU-F consumers and determinism next",
        "CFU-G cross-platform closure and Stage 4 handoff pending",
        "F next",
        "CFU-G 为下一批次",
        "最终 hosted 产品门禁仍未关闭",
        "G2 Stage 4 typed handoff is next",
        "下一检查点为 G2 handoff",
        "G3 final candidate validation is next",
        "G3 validation next",
        "G3 next",
    )
    for path, required_fragments in status_contracts.items():
        text = re.sub(r"\s+", " ", path.read_text(encoding="utf-8")).strip()
        compact_text = re.sub(r"\s+", "", text)
        for fragment in required_fragments:
            if fragment not in text and re.sub(r"\s+", "", fragment) not in compact_text:
                failures.append(CheckFailure(path, f"missing current CFU status: {fragment}"))
        for fragment in stale_fragments:
            if fragment in text or re.sub(r"\s+", "", fragment) in compact_text:
                failures.append(CheckFailure(path, f"contains stale CFU status: {fragment}"))

    dated_status_files = (
        DOCS / "CURRENT_STATUS.md",
        DOCS / "ROADMAP.md",
        DOCS / "PROJECT_GUIDE.md",
        DOCS / "formats" / "CHART_V4_FORMAT.md",
        DOCS / "formats" / "CXT_FORMAT.md",
        DOCS / "formats" / "CXC_FORMAT.md",
        DOCS / "formats" / "README.md",
        DOCS / "stage_plans" / "README.md",
        DOCS / "stage_reports" / "README.md",
    )
    for path in dated_status_files:
        text = path.read_text(encoding="utf-8")
        match = re.search(r"^更新日期：(\d{4}-\d{2}-\d{2})$", text, re.MULTILINE)
        if match is None or match.group(1) < "2026-08-24":
            failures.append(CheckFailure(path, "current CFU status predates the CFU-G4 documentation checkpoint"))


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
    plans = DOCS / "stage_plans"
    for name in FUTURE_STAGE_PLANS:
        path = plans / name
        if not path.is_file():
            failures.append(CheckFailure(path, "required future/deferred stage plan is missing"))
            continue
        text = path.read_text(encoding="utf-8")
        for heading in ("阶段目标", "验收标准"):
            pattern = rf"^##\s+(?:\d+\.\s+)?{heading}\s*$"
            if not re.search(pattern, text, re.MULTILINE):
                failures.append(CheckFailure(path, f"missing required section: {heading}"))
        if "归档来源：" not in text:
            failures.append(CheckFailure(path, "missing archived source reference"))
        if not re.search(r"^状态：(future|deferred)；", text, re.MULTILINE):
            failures.append(CheckFailure(path, "status must be future or deferred"))


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
            "proposals/README.md",
            "examples/README.md",
        ):
            if f"]({target})" not in text:
                failures.append(CheckFailure(docs_index, f"main index does not link {target}"))


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
        "`docs/stage_plans/stage_4_implementation_plan.md`",
        "`docs/stage_plans/stage_12_implementation_plan.md`",
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
    check_stage_name(files, failures)
    check_cfu_status(failures)
    check_script_policy(failures)
    check_future_stage_plans(failures)
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
