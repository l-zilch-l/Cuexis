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
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DOCS = ROOT / "docs"
VERSION_CMAKE = ROOT / "cmake" / "CuexisVersion.cmake"

CONTRACT_DIRS = ("adr", "api", "architecture", "formats", "guides", "stage_plans")
TOP_LEVEL = ("CURRENT_STATUS.md", "PROJECT_GUIDE.md", "ROADMAP.md", "DOCUMENTATION_POLICY.md", "README.md")

BUILD_LITERAL = re.compile(r"\b\d{2}\.\d{2}\.\d{2}-\d+\b")
SDK_LITERAL = re.compile(r"SDK API\s*`?(\d+\.\d+\.\d+)`?")
HISTORICAL_MARKERS = ("历史", "归档", "historical", "已被取代", "旧", "基线", "baseline", "此前", "不外推", "不作为当前")


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


def scan(files: list[Path], sdk: str, build: str) -> list[tuple[str, int, str, str]]:
    findings: list[tuple[str, int, str, str]] = []
    for path in files:
        relative = path.relative_to(ROOT).as_posix()
        if relative.startswith("docs/stage_reports/") or relative.startswith("docs/archive/"):
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
            lowered = line.lower()
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
