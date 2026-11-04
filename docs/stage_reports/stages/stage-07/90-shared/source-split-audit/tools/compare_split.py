#!/usr/bin/env python3
"""Independent split auditor for the Cuexis source-file refactor.

Measures, for one pre-split file and the set of post-split files:
  1. token multiset coverage  - how much of the old token stream survives, by token kind
  2. normalised line coverage - old non-comment, non-blank, non-preprocessor lines present
  3. declaration inventory    - struct/class/enum names and function-ish signatures

No repo helper is used; the tokenizer and comparators are written here.
"""
from __future__ import annotations

import argparse
import difflib
import json
import re
from collections import Counter
from pathlib import Path

IDENT = r"[A-Za-z_]\w*"
NUMBER = r"(?:0[xX][0-9a-fA-F']+|(?:\d[\d']*)(?:\.\d*)?(?:[eE][+-]?\d+)?[uUlLfF]*)"
RAW_STRING = r'R"(?P<raw_delim>[^()\\ ]{0,16})\(.*?\)(?P=raw_delim)"'
STRING = r'"(?:\\.|[^"\\\n])*"'
CHAR = r"'(?:\\.|[^'\\\n])*'"

TOKEN_RE = re.compile(
    "|".join(
        [
            RAW_STRING,
            STRING,
            CHAR,
            r"//[^\n]*",
            r"/\*.*?\*/",
            NUMBER,
            IDENT,
            r"<=>|<<=|>>=|->\*|\.\.\.|::|->|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||\+=|-=|\*=|/=|%=|&=|\|=|\^=",
            r"[^\sA-Za-z0-9_]",
        ]
    ),
    re.S,
)

MULTI_OPS = {
    "<=>", "<<=", ">>=", "->*", "...", "::", "->", "++", "--", "<<", ">>",
    "<=", ">=", "==", "!=", "&&", "||", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=",
}


def strip_comments(text: str) -> str:
    """Remove // and /* */ comments, keep string/char literals intact."""
    out: list[str] = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            j = text.find("\n", i)
            if j < 0:
                break
            i = j
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            if j < 0:
                break
            # keep a newline so line numbers/blank-line structure do not merge
            out.append("\n" * text.count("\n", i, j + 2))
            i = j + 2
            continue
        if c == "R" and i + 1 < n and text[i + 1] == '"':
            m = re.match(RAW_STRING, text[i:], re.S)
            if m:
                out.append(m.group(0))
                i += m.end()
                continue
        if c in "\"'":
            quote = c
            j = i + 1
            while j < n:
                if text[j] == "\\":
                    j += 2
                    continue
                if text[j] == quote:
                    j += 1
                    break
                if text[j] == "\n":
                    break
                j += 1
            out.append(text[i:j])
            i = j
            continue
        out.append(c)
        i += 1
    return "".join(out)


def tokenize(text: str) -> list[str]:
    tokens: list[str] = []
    for m in TOKEN_RE.finditer(text):
        tok = m.group(0)
        if tok.startswith("//") or tok.startswith("/*"):
            continue
        tokens.append(tok)
    return tokens


def token_kind(tok: str) -> str:
    if re.fullmatch(IDENT, tok):
        return "ident"
    if tok.startswith('"') or tok.startswith("R\"") or tok.startswith("'"):
        return "literal"
    if re.fullmatch(NUMBER, tok):
        return "number"
    return "punct"


def normalise_lines(text: str) -> list[str]:
    text = strip_comments(text)
    lines = []
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        line = re.sub(r"\s+", " ", line)
        lines.append(line)
    return lines


DECL_RE = re.compile(
    r"\b(?:enum\s+class|enum\s+struct|struct|class)\s+([A-Za-z_]\w*)"
)
FUNC_RE = re.compile(
    r"(?:^|[;{}:\s])(?:\[\[[^\]]*\]\]\s*)?(?:auto|void|bool|int|std::[A-Za-z_:<>]+|core::[A-Za-z_:<>]+)\s+"
    r"([A-Za-z_]\w*)\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?(?:->\s*[^;{}]+)?[;{]",
    re.M,
)
MEMBER_RE = re.compile(r"^\s*(?:auto|void|bool)\s+([A-Za-z_]\w*)\s*\(", re.M)


def declarations(text: str) -> dict[str, list[str]]:
    clean = strip_comments(text)
    types = sorted({m.group(1) for m in DECL_RE.finditer(clean)})
    funcs = sorted({m.group(1) for m in FUNC_RE.finditer(clean)})
    members = sorted({m.group(1) for m in MEMBER_RE.finditer(clean)})
    return {"types": types, "functions": funcs, "members": members}


def load(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig", errors="strict")


def compare(old_path: Path, new_paths: list[Path], label: str, out_dir: Path) -> dict:
    old = load(old_path)
    new_text = "\n".join(load(p) for p in new_paths)

    old_tokens = tokenize(strip_comments(old))
    new_tokens = tokenize(strip_comments(new_text))
    old_bag = Counter(old_tokens)
    new_bag = Counter(new_tokens)
    deficit = old_bag - new_bag
    surplus = new_bag - old_bag
    by_kind: Counter = Counter()
    for tok, count in deficit.items():
        by_kind[token_kind(tok)] += count

    old_lines = normalise_lines(old)
    new_lines = normalise_lines(new_text)
    new_line_bag = Counter(new_lines)
    missing_lines = [line for line in old_lines if new_line_bag[line] == 0]
    missing_counts = [
        (line, old_lines.count(line), new_line_bag[line])
        for line in dict.fromkeys(old_lines)
        if new_line_bag[line] < old_lines.count(line)
    ]
    # order-sensitive coverage: LCS of normalised line sequences
    sm = difflib.SequenceMatcher(None, old_lines, new_lines, autojunk=False)
    matched = sum(block.size for block in sm.get_matching_blocks())
    old_decl = declarations(old)
    new_decl = declarations(new_text)

    report = {
        "label": label,
        "old_file": old_path.name,
        "new_files": [p.name for p in new_paths],
        "old_tokens": len(old_tokens),
        "new_tokens": len(new_tokens),
        "deficit_total": sum(deficit.values()),
        "deficit_by_kind": dict(by_kind),
        "surplus_total": sum(surplus.values()),
        "deficit_tokens": sorted(deficit.items(), key=lambda kv: (-kv[1], kv[0]))[:120],
        "old_norm_lines": len(old_lines),
        "new_norm_lines": len(new_lines),
        "missing_norm_lines": len(missing_lines),
        "missing_norm_line_samples": missing_lines[:80],
        "line_count_shortfalls": missing_counts[:40],
        "line_lcs_matched": matched,
        "line_lcs_ratio": round(matched / max(1, len(old_lines)), 4),
        "old_decl": old_decl,
        "new_decl": new_decl,
        "decl_types_missing": sorted(set(old_decl["types"]) - set(new_decl["types"])),
        "decl_types_added": sorted(set(new_decl["types"]) - set(old_decl["types"])),
        "decl_funcs_missing": sorted(set(old_decl["functions"]) - set(new_decl["functions"])),
        "decl_funcs_added": sorted(set(new_decl["functions"]) - set(old_decl["functions"])),
        "decl_members_missing": sorted(set(old_decl["members"]) - set(new_decl["members"])),
        "decl_members_added": sorted(set(new_decl["members"]) - set(old_decl["members"])),
    }

    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / f"report_{label}.json").write_text(
        json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8"
    )
    lines = [
        f"# {label}",
        f"old: {old_path.name} ({len(old_tokens)} tokens, {len(old_lines)} norm lines)",
        f"new: {', '.join(p.name for p in new_paths)} ({len(new_tokens)} tokens, {len(new_lines)} norm lines)",
        "",
        f"token deficit: {sum(deficit.values())} {dict(by_kind)}",
        f"token surplus: {sum(surplus.values())}",
        f"normalised old lines absent from new: {len(missing_lines)}/{len(old_lines)}",
        f"order-sensitive line LCS: {matched}/{len(old_lines)} = {report['line_lcs_ratio']}",
        f"types missing: {report['decl_types_missing']}",
        f"types added: {report['decl_types_added']}",
        f"functions missing: {report['decl_funcs_missing']}",
        f"functions added: {report['decl_funcs_added']}",
        f"members missing: {report['decl_members_missing']}",
        "",
        "## missing normalised lines",
        *[f"  - {line}" for line in missing_lines[:80]],
        "",
        "## top deficit tokens",
        *[f"  {tok!r} x{count}" for tok, count in report["deficit_tokens"][:60]],
    ]
    (out_dir / f"report_{label}.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    return report


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--old", required=True, type=Path)
    parser.add_argument("--new", required=True, nargs="+", type=Path)
    parser.add_argument("--label", required=True)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    report = compare(args.old, args.new, args.label, args.out)
    print(json.dumps({k: report[k] for k in (
        "old_tokens", "new_tokens", "deficit_total", "deficit_by_kind",
        "missing_norm_lines", "old_norm_lines", "line_lcs_ratio",
        "decl_types_missing", "decl_funcs_missing", "decl_members_missing",
    )}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
