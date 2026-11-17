"""Generate docs/guides/DIAGNOSTIC_CODES.md from the registered diagnostics code table.

The JSON remains the registration source; this page is a readable projection for agents.
Category semantics stay owned by GAMEPLAY_V2_SPEC 9.2.

Usage:
    python -B tools/verification/gen_docs_codes.py
    python -B tools/verification/gen_docs_codes.py --check
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "schemas" / "cuexis.gameplay-diagnostics.v2.codes.json"
TARGET = ROOT / "docs" / "guides" / "DIAGNOSTIC_CODES.md"


def collect() -> tuple[list[dict[str, str]], list[dict[str, str]], dict[str, str]]:
    data = json.loads(SOURCE.read_text(encoding="utf-8"))
    return data.get("categories", []), data.get("codes", []), {
        "status": str(data.get("status", "unknown")),
        "tableId": str(data.get("tableId", "unknown")),
        "schemaVersion": str(data.get("schemaVersion", "unknown")),
    }


def build() -> str:
    categories, codes, meta = collect()
    lines = [
        "# 诊断码与类别（可读视图）",
        "",
        "<!-- cuexis-meta",
        "id: GUIDE-DIAGNOSTIC-CODES",
        "status: current",
        "authority: references",
        "-->",
        "",
        "状态：current（由 `tools/verification/gen_docs_codes.py` 从 "
        "`schemas/cuexis.gameplay-diagnostics.v2.codes.json` 生成，勿手改）",
        "",
        f"表 `{meta['tableId']}`，schemaVersion `{meta['schemaVersion']}`，"
        f"注册状态 `{meta['status']}`。",
        "",
        "**权威边界**：本页只是注册表的可读投影。九个类别的语义权威仍是 "
        "`docs/formats/GAMEPLAY_V2_SPEC.md` §9.2；码的注册与首次消费以 JSON 为准。"
        "本页不新增裁定，也不改变任何码的含义。",
        "",
        "## 类别",
        "",
        "| 类别 | 来源 |",
        "| --- | --- |",
    ]
    for category in categories:
        lines.append(f"| `{category.get('name', '')}` | {category.get('source', '')} |")
    lines.append("")
    lines.append(f"## 已注册码（{len(codes)} 条）")
    lines.append("")
    lines.append("| 码 | 类别 | 严重度 | 会话影响 | 首个消费批次 |")
    lines.append("| --- | --- | --- | --- | --- |")
    for code in sorted(codes, key=lambda item: (item.get("category", ""), item.get("code", ""))):
        lines.append(
            "| `{}` | {} | {} | {} | {} |".format(
                code.get("code", ""),
                code.get("category", ""),
                code.get("severity", ""),
                code.get("faulted", ""),
                code.get("ownerBatch", ""),
            )
        )
    lines.append("")
    return "\n".join(lines)


def main(argv: list[str]) -> int:
    text = build()
    if "--check" in argv:
        if not TARGET.is_file() or TARGET.read_text(encoding="utf-8") != text:
            print("docs/guides/DIAGNOSTIC_CODES.md is stale; run python -B tools/verification/gen_docs_codes.py")
            return 1
        print("docs/guides/DIAGNOSTIC_CODES.md is current")
        return 0
    TARGET.write_text(text, encoding="utf-8", newline="\n")
    print(f"wrote docs/guides/DIAGNOSTIC_CODES.md ({len(text.splitlines())} lines)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
