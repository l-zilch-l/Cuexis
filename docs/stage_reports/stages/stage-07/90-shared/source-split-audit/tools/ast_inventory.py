#!/usr/bin/env python3
"""Compiler-grade declaration inventory comparison from clang -ast-dump text.

Tracks the current source file through the dump's location ranges and keeps only
declarations that originate in the split header set.  Each declaration is reduced to
(kind, name, type/definition flag) so line numbers, addresses and dump order do not
matter.
"""
from __future__ import annotations

import argparse
import json
import re
from collections import Counter
from pathlib import Path

LOCATION = re.compile(r"<[^<>]*>")
ADDRESS = re.compile(r"0x[0-9a-fA-F]+")
LINE_COL = re.compile(r"\b(?:line|col):\d+(?::\d+)?")
DECL = re.compile(
    r"^(CXXRecordDecl|RecordDecl|CXXMethodDecl|CXXConstructorDecl|CXXDestructorDecl|"
    r"CXXConversionDecl|FunctionDecl|FieldDecl|VarDecl|EnumDecl|EnumConstantDecl|"
    r"TypeAliasDecl|TypedefDecl|ClassTemplateDecl|ClassTemplateSpecializationDecl|"
    r"FunctionTemplateDecl|UsingDecl|UsingDirectiveDecl)\b"
)
TREE = re.compile(r"^[|`\-\s]+")


def current_file(line: str, previous: str) -> str:
    m = re.search(r"<([^<>]*\.(?:hpp|cpp)):\d+:\d+", line)
    if m:
        return m.group(1)
    return previous


def canonical(line: str) -> str | None:
    body = TREE.sub("", line)
    kind_match = DECL.match(body)
    if not kind_match:
        return None
    kind = kind_match.group(1)
    body = LOCATION.sub(" ", body)
    body = ADDRESS.sub(" ", body)
    body = LINE_COL.sub(" ", body)
    body = re.sub(r"\b(?:prev|parent|previous)\b", " ", body)
    body = re.sub(r"\s+", " ", body).strip()
    if " implicit " in f" {body} ":
        return None
    quote = body.find("'")
    quoted = ""
    head = body
    if quote >= 0:
        head = body[:quote].strip()
        end = body.find("'", quote + 1)
        quoted = body[quote : end + 1] if end > quote else ""
    tokens = head.split()
    # drop the kind token
    tokens = tokens[1:]
    if kind in ("CXXRecordDecl", "RecordDecl", "EnumDecl", "ClassTemplateDecl",
                "ClassTemplateSpecializationDecl"):
        keywords = {"struct", "class", "union", "enum", "definition", "referenced", "used",
                    "final", "complete"}
        names = [t for t in tokens if t not in keywords and not t.startswith(":")]
        name = names[0] if names else "?"
        definition = "definition" if "definition" in tokens else "declaration"
        return f"{kind} {name} {definition}"
    # name is the token immediately before the quoted type
    names = [t for t in tokens if re.fullmatch(r"[A-Za-z_~][\w~]*", t)]
    name = names[-1] if names else "?"
    return f"{kind} {name} {quoted}".strip()


def inventory(path: Path, needle: str) -> Counter:
    counts: Counter = Counter()
    current = ""
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        current = current_file(line, current)
        if needle not in current:
            continue
        item = canonical(line)
        if item:
            counts[item] += 1
    return counts


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--old", required=True, type=Path)
    parser.add_argument("--new", required=True, type=Path)
    parser.add_argument("--needle", default="gameplay_graph")
    parser.add_argument("--label", required=True)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    old = inventory(args.old, args.needle)
    new = inventory(args.new, args.needle)
    report = {
        "label": args.label,
        "old_declarations": sum(old.values()),
        "new_declarations": sum(new.values()),
        "missing_from_new": sorted((old - new).items()),
        "added_in_new": sorted((new - old).items()),
    }
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / f"inventory_{args.label}.json").write_text(
        json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8"
    )
    print(json.dumps(report, indent=2)[:6000])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
