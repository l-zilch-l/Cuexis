"""Generate docs/guides/LIMITS.md from engine/chart/include/cuexis/chart/limits.hpp.

The page is the single readable source for the frozen chart/packed budgets; contract documents
should point at it instead of restating changing numbers.

Usage:
    python -B tools/verification/gen_docs_limits.py
    python -B tools/verification/gen_docs_limits.py --check
"""

from __future__ import annotations

import ast
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "engine" / "chart" / "include" / "cuexis" / "chart" / "limits.hpp"
TARGET = ROOT / "docs" / "guides" / "LIMITS.md"

STRUCT = re.compile(r"struct (ChartLimits|PackedChartLimits) final \{(.*?)\n\};", re.DOTALL)
MEMBER = re.compile(r"std::(?:size_t|int64_t|uint64_t|int32_t|uint32_t) (\w+)\{([^}]+)\};")

TITLES = {
    "ChartLimits": ("Chart 解析/编译上限（Cuexis Chart Limit）", "每个字段都是上限：调用方可以更严格，但不得放宽。"),
    "PackedChartLimits": ("Packed 预算（Frozen Foundation）", "每个字段都是天花板：`min(requested, frozen)`；16 MiB 文件门禁与 40,000 实体门禁不因宿主放宽。"),
}


def evaluate(expression: str) -> int:
    cleaned = expression.replace("LL", "").replace("ULL", "").replace("U", "").replace("u", "").strip()
    tree = ast.parse(cleaned, mode="eval")

    def walk(node: ast.AST) -> int:
        if isinstance(node, ast.Expression):
            return walk(node.body)
        if isinstance(node, ast.Constant) and isinstance(node.value, int):
            return node.value
        if isinstance(node, ast.BinOp):
            left, right = walk(node.left), walk(node.right)
            if isinstance(node.op, ast.Mult):
                return left * right
            if isinstance(node.op, ast.Add):
                return left + right
            if isinstance(node.op, ast.Sub):
                return left - right
            if isinstance(node.op, ast.Div):
                return left // right
        raise ValueError(f"unsupported expression: {expression}")

    return walk(tree)


def human(value: int) -> str:
    if value >= 1024 * 1024 and value % (1024 * 1024) == 0:
        return f"{value // (1024 * 1024)} MiB"
    if value >= 1024 and value % 1024 == 0:
        return f"{value // 1024} KiB"
    return str(value)


def collect() -> dict[str, list[tuple[str, int, str]]]:
    text = SOURCE.read_text(encoding="utf-8")
    groups: dict[str, list[tuple[str, int, str]]] = {}
    for name, body in STRUCT.findall(text):
        rows: list[tuple[str, int, str]] = []
        for member, expression in MEMBER.findall(body):
            rows.append((member, evaluate(expression), expression.strip()))
        groups[name] = rows
    return groups


def build(groups: dict[str, list[tuple[str, int, str]]]) -> str:
    lines = [
        "# Chart 与 Packed 预算上限",
        "",
        "<!-- cuexis-meta",
        "id: GUIDE-LIMITS",
        "status: current",
        "authority: references",
        "-->",
        "",
        "状态：current（由 `tools/verification/gen_docs_limits.py` 从 `engine/chart/include/cuexis/chart/limits.hpp` 生成，勿手改）",
        "",
        "本页是上限数值的**单一可读来源**。合同文档引用预算时应链接本页，而不是复述数字；",
        "修改上限只能改 `limits.hpp` 后重新生成本页。",
        "",
    ]
    for name in ("ChartLimits", "PackedChartLimits"):
        rows = groups.get(name, [])
        title, note = TITLES[name]
        lines.append(f"## {title}")
        lines.append("")
        lines.append(note)
        lines.append("")
        lines.append("| 字段 | 值 | 源码表达式 |")
        lines.append("| --- | --- | --- |")
        for member, value, expression in rows:
            shown = f"{value:,}"
            if member.endswith("Bytes"):
                shown = f"{value:,}（{human(value)}）"
            lines.append(f"| `{member}` | {shown} | `{expression}` |")
        lines.append("")
    return "\n".join(lines)


def main(argv: list[str]) -> int:
    check = "--check" in argv
    text = build(collect())
    if check:
        if not TARGET.is_file() or TARGET.read_text(encoding="utf-8") != text:
            print("docs/guides/LIMITS.md is stale; run python -B tools/verification/gen_docs_limits.py")
            return 1
        print("docs/guides/LIMITS.md is current")
        return 0
    TARGET.write_text(text, encoding="utf-8", newline="\n")
    print(f"wrote docs/guides/LIMITS.md ({len(text.splitlines())} lines)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
