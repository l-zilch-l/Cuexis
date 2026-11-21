"""Verify documentation facts that can be derived mechanically.

Checks:
1. docs/MAP.md and the AGENTS.md state block are up to date with their sources.
2. Build-identity literals (`yy.mm.dd-v`) outside historical context match the current version.
3. SDK API literals in API/guide documents match `CUEXIS_SDK_API_VERSION`.

Usage:
    python -B tools/verification/check_docs_facts.py         # report + exit code
    python -B tools/verification/check_docs_facts.py --list  # list every finding
"""

from __future__ import annotations

import importlib.util
import re
import sys
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DOCS = ROOT / "docs"
VERSION_CMAKE = ROOT / "cmake" / "CuexisVersion.cmake"
HANDOFF = DOCS / "HANDOFF.md"
HANDOFF_SECTIONS = ("当前任务", "下一步", "阻塞与待决", "验证状态", "最近改动")
HANDOFF_PLACEHOLDERS = ("TODO", "TBD", "待填", "（待补）")

CONTRACT_DIRS = ("adr", "api", "architecture", "formats", "guides", "stage_plans")
TOP_LEVEL = ("CURRENT_STATUS.md", "PROJECT_GUIDE.md", "ROADMAP.md", "DOCUMENTATION_POLICY.md", "README.md")

BUILD_LITERAL = re.compile(r"\b\d{2}\.\d{2}\.\d{2}-\d+\b")
SDK_LITERAL = re.compile(r"SDK API\s*`?(\d+\.\d+\.\d+)`?")
HISTORICAL_MARKERS = ("历史", "归档", "historical", "已被取代", "旧", "基线", "baseline", "此前", "不外推", "不作为当前", "示例")
FACTS_SKIP_STATUSES = {"historical", "evidence", "completed", "superseded", "archived", "deferred"}


def version_info() -> tuple[str, str]:
    text = VERSION_CMAKE.read_text(encoding="utf-8")

    def value(pattern: str) -> str:
        found = re.search(pattern, text, re.MULTILINE)
        return found.group(1) if found else "unknown"

    year = value(r"set\(CUEXIS_VERSION_YEAR (\d+)\)")
    month = value(r"set\(CUEXIS_VERSION_MONTH (\d+)\)")
    day = value(r"set\(CUEXIS_VERSION_DAY (\d+)\)")
    build = value(r"set\(CUEXIS_VERSION_BUILD (\d+)\)")
    sdk = value(r'set\(CUEXIS_SDK_API_VERSION "([^"]+)"\)')
    return sdk, f"{year}.{month}.{day}-{build}"


def generated_is_current() -> bool:
    spec = importlib.util.spec_from_file_location("gen_docs_map", ROOT / "tools" / "verification" / "gen_docs_map.py")
    if spec is None or spec.loader is None:
        return False
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    rows = module.collect()
    status_row = next((row for row in rows if row["rel"] == "CURRENT_STATUS.md"), None)
    stage = str(status_row["meta"].get("current_stage", "unknown")) if status_row else "unknown"
    sdk, build = version_info()
    item = module.current_item(rows)
    register = module.current_register(rows)
    map_text = module.build_map(rows, stage, item, register)
    agents_text = module.AGENTS.read_text(encoding="utf-8")
    expected_agents = module.replace_state_block(agents_text, module.build_state(sdk, build, stage, item, register))
    if not module.MAP.is_file() or module.MAP.read_text(encoding="utf-8") != map_text:
        return False
    return expected_agents == agents_text


def candidate_files() -> list[Path]:
    files: list[Path] = []
    for name in CONTRACT_DIRS:
        base = DOCS / name
        if base.is_dir():
            files.extend(sorted(base.rglob("*.md")))
    for name in TOP_LEVEL:
        path = DOCS / name
        if path.is_file():
            files.append(path)
    return files


def document_status(path: Path) -> str:
    """Read the machine status from the cuexis-meta block, if present."""
    head = "\n".join(path.read_text(encoding="utf-8").splitlines()[:40])
    match = re.search(r"(?m)^status:\s*([A-Za-z_-]+)\s*$", head)
    return match.group(1).lower() if match else ""


def scan(files: list[Path], sdk: str, build: str) -> list[tuple[str, int, str, str]]:
    findings: list[tuple[str, int, str, str]] = []
    for path in files:
        relative = path.relative_to(ROOT).as_posix()
        if relative.startswith("docs/stage_reports/") or relative.startswith("docs/archive/"):
            continue
        if relative.startswith("docs/adr/"):
            continue
        if document_status(path) in FACTS_SKIP_STATUSES:
            continue
        in_fence = False
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
            if line.strip().startswith("```"):
                in_fence = not in_fence
                continue
            if in_fence:
                continue
            lowered = line.lower()
            if "cuexis-facts-exempt" in line:
                continue
            if any(marker in lowered or marker in line for marker in HISTORICAL_MARKERS):
                continue
            for literal in BUILD_LITERAL.findall(line):
                if literal != build:
                    findings.append((relative, number, literal, f"build identity is {build}"))
            for literal in SDK_LITERAL.findall(line):
                if literal != sdk:
                    findings.append((relative, number, literal, f"SDK API is {sdk}"))
    return findings


def limits_is_current() -> bool:
    spec = importlib.util.spec_from_file_location(
        "gen_docs_limits", ROOT / "tools" / "verification" / "gen_docs_limits.py"
    )
    if spec is None or spec.loader is None:
        return False
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    expected = module.build(module.collect())
    if not module.TARGET.is_file():
        return False
    return module.TARGET.read_text(encoding="utf-8") == expected


def codes_are_current() -> bool:
    spec = importlib.util.spec_from_file_location(
        "gen_docs_codes", ROOT / "tools" / "verification" / "gen_docs_codes.py"
    )
    if spec is None or spec.loader is None:
        return False
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    expected = module.build()
    if not module.TARGET.is_file():
        return False
    return module.TARGET.read_text(encoding="utf-8") == expected


def player_options_are_current() -> bool:
    spec = importlib.util.spec_from_file_location(
        "gen_player_options", ROOT / "tools" / "verification" / "gen_player_options.py"
    )
    if spec is None or spec.loader is None:
        return False
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    try:
        expected = module.build()
    except SystemExit:
        return False
    if not module.TARGET.is_file():
        return False
    return module.TARGET.read_text(encoding="utf-8") == expected


def frame_digest_is_current() -> bool:
    """Keep the documented FrameDigest version range aligned with the public header default."""
    header = ROOT / "engine" / "playback" / "include" / "cuexis" / "playback" / "frame_digest.hpp"
    page = DOCS / "api" / "frames-digests-and-timelines.md"
    if not header.is_file() or not page.is_file():
        return False
    match = re.search(r"algorithmVersion\{(\d+)\}", header.read_text(encoding="utf-8"))
    if match is None:
        return False
    text = page.read_text(encoding="utf-8")
    return re.search(rf"FrameDigest v1-v{int(match.group(1))}\b", text) is not None


def handoff_issues() -> tuple[list[str], list[str]]:
    """Validate docs/HANDOFF.md structure and report staleness as a warning."""
    if not HANDOFF.is_file():
        return (["docs/HANDOFF.md is missing"], [])
    errors: list[str] = []
    warnings: list[str] = []
    text = HANDOFF.read_text(encoding="utf-8")
    if "cuexis-meta" not in text:
        errors.append("docs/HANDOFF.md is missing its cuexis-meta block")
    updated = None
    match = re.search(r"更新日期：(\d{4}-\d{2}-\d{2})", text)
    if not match:
        errors.append("docs/HANDOFF.md is missing 更新日期：YYYY-MM-DD")
    else:
        try:
            updated = date.fromisoformat(match.group(1))
        except ValueError:
            errors.append("docs/HANDOFF.md 更新日期 is not a valid date")
    headings = "\n".join(line for line in text.splitlines() if line.startswith("## "))
    missing = [section for section in HANDOFF_SECTIONS if section not in headings]
    if missing:
        errors.append("docs/HANDOFF.md is missing sections: " + ", ".join(missing))
    for placeholder in HANDOFF_PLACEHOLDERS:
        if placeholder in text:
            errors.append(f"docs/HANDOFF.md still contains placeholder {placeholder}")
    next_block = text.split("## 下一步", 1)[-1].split("\n## ", 1)[0]
    if not re.search(r"^\s*\d+\.\s+\S", next_block, re.MULTILINE):
        errors.append("docs/HANDOFF.md 下一步 must contain at least one numbered step")
    if updated is not None:
        age = (date.today() - updated).days
        if age > 7:
            warnings.append(f"docs/HANDOFF.md 已 {age} 天未更新（active 阶段建议 7 天内更新）")
    return errors, warnings


def main(argv: list[str]) -> int:
    sdk, build = version_info()
    failures: list[str] = []
    if not generated_is_current():
        failures.append("docs/MAP.md or the AGENTS.md state block is stale; run python -B tools/verification/gen_docs_map.py")
    if not limits_is_current():
        failures.append("docs/guides/LIMITS.md is stale; run python -B tools/verification/gen_docs_limits.py")
    if not codes_are_current():
        failures.append("docs/guides/DIAGNOSTIC_CODES.md is stale; run python -B tools/verification/gen_docs_codes.py")
    if not player_options_are_current():
        failures.append(
            "docs/guides/PLAYER_OPTIONS.md is stale or the option table is out of sync; "
            "run python -B tools/verification/gen_player_options.py"
        )
    if not frame_digest_is_current():
        failures.append(
            "docs/api/frames-digests-and-timelines.md does not match the FrameDigest version in "
            "engine/playback/include/cuexis/playback/frame_digest.hpp"
        )
    handoff_errors, handoff_warnings = handoff_issues()
    failures.extend(handoff_errors)
    for warning in handoff_warnings:
        print("WARNING: " + warning)

    findings = scan(candidate_files(), sdk, build)
    if "--list" in argv:
        for relative, number, literal, expected in findings:
            print(f"{relative}:{number}: found {literal}; {expected}")

    # Version drift is reported as a warning: historical documents legitimately mention old builds and
    # the allowlist is maintained by hand. Only the generated-block check is fatal.
    print(f"version-drift findings: {len(findings)} (warning; see --list)")
    if failures:
        for failure in failures:
            print("FAIL: " + failure)
        return 1
    print(f"documentation facts are current (SDK {sdk}, build {build})")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
