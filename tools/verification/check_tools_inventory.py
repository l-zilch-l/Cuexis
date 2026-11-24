"""Check that every maintainer script is registered in tools/README.md.

Frozen evidence suites, practice fixtures and archived spikes are covered by their own
directory documentation and are exempt here.

Usage:
    python -B tools/verification/check_tools_inventory.py
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools"
README = TOOLS / "README.md"
SCRIPT_SUFFIXES = {".py", ".ps1", ".sh", ".cmd"}
EXEMPT_PREFIXES = (
    "tools/archive/",
    "tools/practice/",
    "tools/verification/execution_acceptance/",
    "tools/verification/playback_performance/",
)


def scripts() -> list[str]:
    found: list[str] = []
    for path in sorted(TOOLS.rglob("*")):
        if not path.is_file() or path.suffix not in SCRIPT_SUFFIXES:
            continue
        relative = path.relative_to(ROOT).as_posix()
        if relative.startswith(EXEMPT_PREFIXES):
            continue
        found.append(relative)
    return found


def main() -> int:
    if not README.is_file():
        print("FAIL: tools/README.md is missing")
        return 1
    text = README.read_text(encoding="utf-8")
    failures: list[str] = []
    for script in scripts():
        if script not in text:
            failures.append(f"script is not registered in tools/README.md: {script}")
    for token in re.findall(r"`(tools/[^`]+\.(?:py|ps1|sh|cmd))`", text):
        if "{" in token or "*" in token:
            continue
        if not (ROOT / token).is_file():
            failures.append(f"tools/README.md lists a missing script: {token}")
    if failures:
        print("Tools inventory checks failed:")
        for failure in failures[:40]:
            print(f"- {failure}")
        return 1
    print(f"tools inventory is current ({len(scripts())} scripts registered).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
