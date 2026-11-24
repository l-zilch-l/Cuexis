"""Reject stage and task names in production source, install and build file names.

Hard rule: production sources, install headers and build-system filenames must not
carry a stage or task name. Tests, tools, docs, examples and third_party may carry
them because they legitimately identify a stage or a task.

Scope: every directory and file under ``engine/``, ``app/`` and ``cmake/``. Each
entry contributes its own name segment; the scan roots themselves are not checked.
``tests/``, ``tools/``, ``docs/``, ``examples/`` and ``third_party/`` are excluded.

One segment is normalized by dropping its last extension, inserting separators
between an acronym/number and a following CamelCase word, then splitting on
``_`` and ``-``::

    VerifyCfuF3Determinism.cmake -> Verify, Cfu, F3, Determinism
    VerifyS4GPerformance.cmake   -> Verify, S4G, Performance
    cxt_v2_animation_adapter.cpp -> cxt, v2, animation, adapter

A token is forbidden when the whole token matches, case-insensitively:
``rr\\d*``, ``s\\d[a-z]\\d*``, ``stage\\d+``, ``task\\d*``, ``t\\d{2}``,
``f\\d{2,3}``, ``cfu``, ``pd\\d*``. Digits are never split from a leading
uppercase letter, so ``S4G`` stays one token; ``stages``, ``v2`` and
``error`` are not hits.

Legacy paths are allowed only by exact relative path in
``docs/governance_rules.json`` -> ``source_naming_exceptions``. A missing,
unparsable, malformed or empty-reason exception table fails closed.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from pathlib import Path, PurePosixPath


SCAN_ROOTS = ("engine", "app", "cmake")
GOVERNANCE_RULES = Path("docs") / "governance_rules.json"
EXCEPTIONS_KEY = "source_naming_exceptions"
FORBIDDEN_TOKEN_SOURCES = (
    r"rr\d*",
    r"s\d[a-z]\d*",
    r"stage\d+",
    r"task\d*",
    r"t\d{2}",
    r"f\d{2,3}",
    r"cfu",
    r"pd\d*",
)
FORBIDDEN_TOKEN_PATTERNS = tuple(
    (source, re.compile(source, re.IGNORECASE)) for source in FORBIDDEN_TOKEN_SOURCES
)
HINT = "改成领域名，或在 docs/governance_rules.json 记录遗留原因"


def reject_duplicate_keys(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def name_tokens(segment: str) -> list[str]:
    """Split one path segment into checks; only the last extension is dropped."""
    stem = segment
    dot = stem.rfind(".")
    if dot > 0:
        stem = stem[:dot]
    stem = re.sub(r"(?<=[A-Z0-9])(?=[A-Z][a-z])", "_", stem)
    stem = re.sub(r"(?<=[a-z])(?=[A-Z])", "_", stem)
    return [token for token in re.split(r"[_\-]+", stem) if token]


def forbidden_hits(segment: str) -> list[tuple[str, str]]:
    hits: list[tuple[str, str]] = []
    for token in name_tokens(segment):
        for source, pattern in FORBIDDEN_TOKEN_PATTERNS:
            if pattern.fullmatch(token):
                hits.append((token, source))
                break
    return hits


def valid_exception_path(key: str) -> bool:
    """Exceptions name one exact relative path inside the scanned roots."""
    if not key or key.startswith("/") or key.endswith("/") or "\\" in key:
        return False
    if any(character in key for character in "*?["):
        return False
    parts = PurePosixPath(key).parts
    if any(part in {"", ".", ".."} for part in parts):
        return False
    return bool(parts) and parts[0] in SCAN_ROOTS


def load_exceptions(
    root: Path, errors: list[tuple[Path, str]]
) -> dict[str, str] | None:
    """Load exact legacy exceptions; return None after recording a fail-closed error."""
    path = root / GOVERNANCE_RULES
    relative = GOVERNANCE_RULES.as_posix()
    try:
        text = path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as error:
        errors.append(
            (path, f"source naming: cannot read {relative}: {error}; fail closed")
        )
        return None
    try:
        value = json.loads(text, object_pairs_hook=reject_duplicate_keys)
    except (json.JSONDecodeError, ValueError) as error:
        errors.append(
            (path, f"source naming: cannot parse {relative}: {error}; fail closed")
        )
        return None
    if not isinstance(value, dict):
        errors.append(
            (path, f"source naming: {relative} must contain a JSON object; fail closed")
        )
        return None
    table = value.get(EXCEPTIONS_KEY)
    if not isinstance(table, dict):
        errors.append(
            (
                path,
                f"source naming: {relative} must define an {EXCEPTIONS_KEY} object; fail closed",
            )
        )
        return None
    allowed: dict[str, str] = {}
    for key, reason in table.items():
        if not isinstance(key, str) or not valid_exception_path(key):
            errors.append(
                (
                    path,
                    f"source naming: {EXCEPTIONS_KEY} key must be an exact relative "
                    f"path under engine/, app/ or cmake/: {key!r}",
                )
            )
            continue
        if not isinstance(reason, str) or not reason.strip():
            errors.append(
                (
                    path,
                    f"source naming: {EXCEPTIONS_KEY} reason is empty for {key}; "
                    "record the legacy reason here or rename the path",
                )
            )
            continue
        allowed[key] = reason.strip()
    return allowed


def iter_entries(root: Path):
    """Yield (repository-relative posix path, path) for every entry in the roots."""
    for scanned_root in SCAN_ROOTS:
        base = root / scanned_root
        if not base.is_dir():
            continue
        for current, directories, files in os.walk(base):
            directories.sort()
            files.sort()
            for name in directories:
                entry = Path(current) / name
                yield entry.relative_to(root).as_posix(), entry
            for name in files:
                entry = Path(current) / name
                yield entry.relative_to(root).as_posix(), entry


def check_repository(
    root: Path,
) -> tuple[list[tuple[Path, str]], list[str]]:
    """Return (path, message) errors and notices for one repository root."""
    root = Path(root).resolve()
    errors: list[tuple[Path, str]] = []
    notices: list[str] = []
    allowed = load_exceptions(root, errors)
    if allowed is None:
        return errors, notices
    scanned = 0
    legacy: list[str] = []
    for relative, path in iter_entries(root):
        scanned += 1
        hits = forbidden_hits(path.name)
        if not hits:
            continue
        if relative in allowed:
            legacy.append(relative)
            continue
        rendered = ", ".join(f"{token!r} ({source})" for token, source in hits)
        errors.append(
            (
                path,
                f"source naming: {relative}: path segment {path.name!r} carries "
                f"stage/task token {rendered}; {HINT}",
            )
        )
    unique_legacy = sorted(set(legacy))
    notice = (
        f"source naming: 扫描 {scanned} 个路径段，{len(unique_legacy)} 个遗留例外"
    )
    if unique_legacy:
        notice += "（" + "、".join(unique_legacy) + "）"
    notices.append(notice)
    return errors, notices


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Reject stage/task names in production source, install and build filenames."
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=None,
        help="repository root; defaults to the enclosing Cuexis checkout",
    )
    arguments = parser.parse_args(argv)
    root = (arguments.root or Path(__file__).resolve().parents[2]).resolve()
    errors, notices = check_repository(root)
    for notice in notices:
        print("NOTICE: " + notice)
    for _, message in errors:
        print("FAIL: " + message)
    print(f"source naming checks: {len(errors)} error(s)")
    return 0 if not errors else 1


if __name__ == "__main__":
    sys.exit(main())
