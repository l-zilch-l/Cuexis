#!/usr/bin/env python3
"""Compare function bodies between a pre-split file and its post-split files.

Extraction is brace-matched on comment-stripped text.  Functions are grouped by
(name, arity) with class qualifiers, `static`, `inline` and `override` ignored, and
compared as a multiset of body token streams.  Default arguments are stripped from
the parameter list so an out-of-line definition that drops a default still matches.
"""
from __future__ import annotations

import argparse
import json
import re
from collections import defaultdict
from pathlib import Path

from compare_split import strip_comments, tokenize

FUNC_START = re.compile(
    r"(?:\[\[nodiscard\]\]\s*)?(?:inline\s+)?(?:static\s+)?(?:constexpr\s+)?"
    r"(?:auto|void|bool|int|double|float|std::[\w:<>]+|core::[\w:<>]+|playback::[\w:<>]+|chart::[\w:<>]+)"
    r"\s+(?:([A-Za-z_]\w*)::)?([A-Za-z_]\w*)\s*\("
)


def split_params(text: str) -> list[str]:
    depth = 0
    current: list[str] = []
    parts: list[str] = []
    for ch in text:
        if ch in "([{<":
            depth += 1
        elif ch in ")]}>":
            depth -= 1
        if ch == "," and depth == 0:
            parts.append("".join(current).strip())
            current = []
            continue
        current.append(ch)
    if "".join(current).strip():
        parts.append("".join(current).strip())
    return parts


def strip_defaults(param: str) -> str:
    depth = 0
    for index, ch in enumerate(param):
        if ch in "([{<":
            depth += 1
        elif ch in ")]}>":
            depth -= 1
        elif ch == "=" and depth == 0:
            # not part of ==, !=, <=, >=, =>, <=>
            before = param[index - 1] if index else ""
            after = param[index + 1] if index + 1 < len(param) else ""
            if before not in "=!<>" and after != "=":
                return param[:index].strip()
    return param.strip()


def extract_functions(text: str) -> dict[tuple[str, int], list[list[str]]]:
    clean = strip_comments(text)
    functions: dict[tuple[str, int], list[list[str]]] = defaultdict(list)
    pos = 0
    while True:
        m = FUNC_START.search(clean, pos)
        if not m:
            break
        name = m.group(2)
        open_paren = m.end() - 1
        depth = 0
        i = open_paren
        while i < len(clean):
            if clean[i] == "(":
                depth += 1
            elif clean[i] == ")":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        if i >= len(clean):
            break
        params = [strip_defaults(p) for p in split_params(clean[open_paren + 1 : i])]
        params = [p for p in params if p]
        j = i + 1
        while j < len(clean) and clean[j] in " \t\r\n":
            j += 1
        k = j
        while k < len(clean) and clean[k] not in "{;":
            k += 1
        if k >= len(clean) or clean[k] == ";":
            pos = i + 1
            continue
        depth = 0
        end = k
        while end < len(clean):
            if clean[end] == "{":
                depth += 1
            elif clean[end] == "}":
                depth -= 1
                if depth == 0:
                    break
            end += 1
        body = clean[k + 1 : end]
        functions[(name, len(params))].append(tokenize(body))
        pos = end + 1
    return functions


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--old", required=True, type=Path)
    parser.add_argument("--new", required=True, nargs="+", type=Path)
    parser.add_argument("--label", required=True)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()

    old_text = args.old.read_text(encoding="utf-8-sig")
    new_text = "\n".join(p.read_text(encoding="utf-8-sig") for p in args.new)
    old_funcs = extract_functions(old_text)
    new_funcs = extract_functions(new_text)

    old_counts = {k: len(v) for k, v in old_funcs.items()}
    new_counts = {k: len(v) for k, v in new_funcs.items()}
    missing_keys = sorted(k for k in old_counts if k not in new_counts)
    extra_keys = sorted(k for k in new_counts if k not in old_counts)
    mismatched = []
    identical = 0
    for key in sorted(set(old_funcs) & set(new_funcs)):
        old_sorted = sorted(tuple(tokens) for tokens in old_funcs[key])
        new_sorted = sorted(tuple(tokens) for tokens in new_funcs[key])
        if old_sorted == new_sorted:
            identical += len(old_sorted)
            continue
        # compare as multisets of bodies, report the token-level deficit
        old_flat = [t for body in old_sorted for t in body]
        new_flat = [t for body in new_sorted for t in body]
        from collections import Counter
        deficit = Counter(old_flat) - Counter(new_flat)
        mismatched.append(
            {
                "key": f"{key[0]}/{key[1]}",
                "old_bodies": len(old_sorted),
                "new_bodies": len(new_sorted),
                "old_tokens": len(old_flat),
                "new_tokens": len(new_flat),
                "deficit": sorted(deficit.items(), key=lambda kv: (-kv[1], kv[0]))[:40],
            }
        )
    report = {
        "label": args.label,
        "old_keys": len(old_funcs),
        "new_keys": len(new_funcs),
        "old_functions": sum(old_counts.values()),
        "new_functions": sum(new_counts.values()),
        "identical_functions": identical,
        "missing_keys": [f"{k[0]}/{k[1]}" for k in missing_keys],
        "extra_keys": [f"{k[0]}/{k[1]}" for k in extra_keys],
        "mismatched": mismatched,
    }
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / f"bodies_{args.label}.json").write_text(
        json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8"
    )
    print(json.dumps(report, indent=2)[:6000])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
