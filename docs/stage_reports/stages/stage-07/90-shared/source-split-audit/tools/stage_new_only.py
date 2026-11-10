#!/usr/bin/env python3
"""Report new-only normalised lines per post-split function, against the old file.

A line that exists nowhere in the pre-split file is either scaffolding (signature,
namespace, include, member assignment that replaces a local declaration) or a real
semantic addition.  The report lists them so each one can be classified.
"""
from __future__ import annotations

import argparse
import re
from collections import Counter
from pathlib import Path

from compare_split import normalise_lines, strip_comments


def extract_methods(text: str) -> dict[str, list[str]]:
    clean = strip_comments(text)
    methods: dict[str, list[str]] = {}
    pattern = re.compile(r"auto\s+(?:Decoder::)?([A-Za-z_]\w*)\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?(?:->\s*[^;{}]+)?\{")
    for match in pattern.finditer(clean):
        name = match.group(1)
        start = match.end() - 1
        depth = 0
        end = start
        while end < len(clean):
            if clean[end] == "{":
                depth += 1
            elif clean[end] == "}":
                depth -= 1
                if depth == 0:
                    break
            end += 1
        body = clean[start + 1 : end]
        methods[name] = normalise_lines(body)
    return methods


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--old", required=True, type=Path)
    parser.add_argument("--new", required=True, nargs="+", type=Path)
    parser.add_argument("--label", required=True)
    args = parser.parse_args()
    old_lines = Counter(normalise_lines(args.old.read_text(encoding="utf-8-sig")))
    new_text = "\n".join(p.read_text(encoding="utf-8-sig") for p in args.new)
    methods = extract_methods(new_text)
    print(f"# {args.label}: {len(methods)} functions")
    total = 0
    for name, lines in methods.items():
        new_only = [line for line in lines if old_lines[line] == 0]
        total += len(new_only)
        print(f"\n## {name}: {len(lines)} lines, {len(new_only)} new-only")
        for line in new_only:
            print(f"   + {line}")
    print(f"\ntotal new-only lines: {total}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
