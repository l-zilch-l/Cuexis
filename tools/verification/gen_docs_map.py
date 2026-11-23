"""Generate docs/MAP.md and the generated state block in AGENTS.md.

Sources of truth:
- every Markdown file under docs/ that carries a `cuexis-meta` comment block
- docs/CURRENT_STATUS.md `current_stage`
- cmake/CuexisVersion.cmake (SDK API version and build identity)

Usage:
    python -B tools/verification/gen_docs_map.py          # write outputs
    python -B tools/verification/gen_docs_map.py --check  # fail when stale
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DOCS = ROOT / "docs"
MAP = DOCS / "MAP.md"
STATUS = DOCS / "CURRENT_STATUS.md"
AGENTS = ROOT / "AGENTS.md"
VERSION_CMAKE = ROOT / "cmake" / "CuexisVersion.cmake"

META = re.compile(r"<!-- cuexis-meta\n(.*?)\n-->", re.DOTALL)
H1 = re.compile(r"^#\s+(.+)$", re.MULTILINE)
STATE_BEGIN = "<!-- CUEXIS_STATE_BEGIN -->"
STATE_END = "<!-- CUEXIS_STATE_END -->"

CONTRACT_DIRS = ("adr/", "api/", "architecture/", "formats/", "guides/")
SECTION_TITLES = {
    "adr/": "ADR（决策）",
    "api/": "API 参考",
    "architecture/": "架构",
    "formats/": "格式与系统 Spec",
    "guides/": "工程指南与政策",
}
STATUS_ORDER = {"active": 0, "current": 0, "accepted": 1, "candidate": 2, "future": 3, "deferred": 4,
                "completed": 5, "historical": 6, "unverified": 7}


def unfenced_text(text: str) -> str:
    """Blank fenced examples, retaining line positions for header/diagnostic parsing."""
    lines: list[str] = []
    fence = ""
    width = 0
    for line in text.splitlines():
        marker = re.match(r"^ {0,3}(`{3,}|~{3,})", line)
        if not fence and marker:
            fence, width = marker.group(1)[0], len(marker.group(1))
            lines.append("")
        elif fence:
            if re.fullmatch(r" {0,3}" + re.escape(fence) + "{" + str(width) + r",}\s*", line):
                fence = ""
            lines.append("")
        else:
            lines.append(line)
    return "\n".join(lines)


def header_meta_blocks(text: str) -> list[str]:
    """Read unfenced comments before the first section, within the real header.

    Existing headers include version/role notes and short provenance paragraphs.
    The first section ends that header; fenced examples are never metadata.
    """
    header: list[str] = []
    for line in unfenced_text(text).splitlines():
        if re.match(r"^#{2,6}\s", line):
            break
        header.append(line)
    return re.findall(r"<!--\s*cuexis-meta\s*\n(.*?)-->", "\n".join(header), re.DOTALL)


def parse_meta(text: str) -> dict[str, str]:
    blocks = header_meta_blocks(text)
    if len(blocks) != 1:
        return {}
    fields: dict[str, str] = {}
    for line in blocks[0].splitlines():
        if not line.strip():
            continue
        if ":" not in line:
            return {}
        key, value = (part.strip() for part in line.split(":", 1))
        if not key or key in fields:
            return {}
        fields[key] = value
    return fields


def first_heading(text: str) -> str:
    match = H1.search(unfenced_text(text))
    return match.group(1).strip() if match else "(无标题)"


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


def collect() -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for path in sorted(DOCS.rglob("*.md")):
        relative = path.relative_to(DOCS).as_posix()
        if relative == "MAP.md" or (relative.startswith("templates/") and path.name != "README.md"):
            continue
        if relative.startswith("stage_reports/") and "evidence" in path.relative_to(DOCS).parts:
            continue
        text = path.read_text(encoding="utf-8")
        meta = parse_meta(text)
        rows.append(
            {
                "rel": relative,
                "title": first_heading(text),
                "status": meta.get("status", "unverified"),
                "authority": meta.get("authority", "unknown"),
                "id": meta.get("id", ""),
                "lines": text.count("\n") + (0 if text.endswith("\n") else 1),
                "meta": meta,
            }
        )
    return rows


def table(rows: list[dict[str, object]]) -> list[str]:
    out = ["| 文档 | 状态 | 权限 | 标题 |", "| --- | --- | --- | --- |"]
    for row in rows:
        rel = str(row["rel"])
        status = str(row["status"])
        authority = str(row["authority"])
        out.append(f"| [{rel}]({rel}) | {status} | {authority} | {row['title']} |")
    return out


def current_field(rows: list[dict[str, object]], field: str) -> str:
    for row in rows:
        if row["rel"] == "CURRENT_STATUS.md":
            candidate = str(row["meta"].get(field, ""))
            if candidate and (DOCS / candidate).is_file():
                return candidate
    return ""


def current_item(rows: list[dict[str, object]]) -> str:
    return current_field(rows, "current_item")


def current_register(rows: list[dict[str, object]]) -> str:
    return current_field(rows, "current_register")


def build_map(rows: list[dict[str, object]], stage: str, item: str, register: str) -> str:
    by_rel = {str(row["rel"]): row for row in rows}
    item_link = f"[{item}]({item})" if item else "（未登记）"
    register_link = f"[{register}]({register})" if register else "（未登记）"

    def sorted_group(predicate) -> list[dict[str, object]]:
        selected = [row for row in rows if predicate(str(row["rel"]))]
        return sorted(selected, key=lambda row: (STATUS_ORDER.get(str(row["status"]), 9), str(row["rel"])))

    lines: list[str] = []
    lines.append("# Documentation Map")
    lines.append("")
    lines.append("<!-- cuexis-meta")
    lines.append("id: INDEX-MAP")
    lines.append("status: current")
    lines.append("authority: index")
    lines.append("-->")
    lines.append("")
    lines.append("状态：current（由 tools/verification/gen_docs_map.py 生成，勿手改）")
    lines.append("")
    lines.append("给 agent 的阅读规则：")
    lines.append("")
    lines.append("1. 先读本页，再只打开与任务相关的 1–3 篇；不要全仓 grep docs/。")
    lines.append("2. 事实以代码、`schemas/`、`cmake/CuexisVersion.cmake`、`limits.hpp` 为准；文档中的数值若是复述，以生成物/代码为准。")
    lines.append("3. `docs/stage_reports/**` 与 `docs/archive/**` 是**证据**，不是权威；不得据其推断当前实现。")
    lines.append("4. `status` 为 `completed`/`historical` 的文档只用于追溯；`candidate`/`future`/`deferred` 不是已实现能力。")
    lines.append("")
    lines.append(f"当前阶段：**{stage}**（见 [CURRENT_STATUS.md](CURRENT_STATUS.md)）；"
                 f"当前工作项：{item_link}；当前登记册：{register_link}。")
    lines.append("")
    lines.append("## 常见变更的阅读路径")
    lines.append("")
    lines.append("| 要改什么 | 先读 |")
    lines.append("| --- | --- |")
    lines.append("| 公共 API 签名/语义 | [api/README.md](api/README.md) → 对应 api 页 → [ADR 0030](adr/0030-playback-preview-api-version-and-result.md) |")
    lines.append("| 格式字段/预算 | [formats/README.md](formats/README.md) → 对应 Spec → [guides/LIMITS.md](guides/LIMITS.md) |")
    lines.append("| 格式 Schema/校验 | `schemas/*.schema.json`（如 [cuexis.chart.v4.schema.json](../schemas/cuexis.chart.v4.schema.json)）→ 对应 Spec |")
    lines.append("| 依赖/构建 target | [guides/DEPENDENCY_POLICY.md](guides/DEPENDENCY_POLICY.md) → [guides/BUILDING.md](guides/BUILDING.md) |")
    lines.append("| 修 bug / 复核差异 | [CURRENT_STATUS.md](CURRENT_STATUS.md) → 相关合同 → 当前登记册（见上方） |")
    lines.append("")
    lines.append("单一事实源：")
    lines.append("")
    lines.append("| 事实 | 权威位置 |")
    lines.append("| --- | --- |")
    lines.append("| 当前阶段与遗留 | [CURRENT_STATUS.md](CURRENT_STATUS.md) |")
    lines.append("| 阶段顺序与依赖 | [ROADMAP.md](ROADMAP.md) |")
    lines.append("| 版本（SDK / 日期构建） | `cmake/CuexisVersion.cmake`（机器校验：`vcpkg.json`） |")
    lines.append("| 构建 target 与依赖 | [guides/BUILDING.md](guides/BUILDING.md) 的生成块 + CMakeLists |")
    lines.append("| 格式预算/上限 | [guides/LIMITS.md](guides/LIMITS.md)（生成自 `engine/chart/include/cuexis/chart/limits.hpp`） |")
    lines.append("| 诊断码与类别 | `schemas/cuexis.gameplay-diagnostics.v2.codes.json`（可读视图：[guides/DIAGNOSTIC_CODES.md](guides/DIAGNOSTIC_CODES.md)） |")
    lines.append("| 决策 | `docs/adr/` |")
    lines.append("")
    lines.append("## 合同类（权威，按状态排序）")
    lines.append("")
    lines.extend(table(sorted_group(lambda rel: rel.split("/", 1)[0] + "/" in CONTRACT_DIRS
                                    and rel not in {"README.md"})))
    lines.append("")
    lines.append("## 计划类")
    lines.append("")
    plans = sorted_group(lambda rel: rel.startswith("stage_plans/"))
    lines.extend(table(plans))
    lines.append("")
    lines.append("## 证据与历史（非权威）")
    lines.append("")
    lines.append("- [stage_reports](stage_reports/README.md)：带日期的实施/验证证据；只用于追溯与核对结论。")
    lines.append("- [archive](archive/README.md)：被取代的历史文档。")
    lines.append("- [proposals](proposals/README.md)：候选设计输入，不是合同。")
    lines.append("- [examples](examples/README.md)：评审与验证用的样例，不是合同。")
    lines.append("")
    lines.append("## 顶层")
    lines.append("")
    top = sorted_group(lambda rel: "/" not in rel and rel not in {"MAP.md", "CURRENT_STATUS.md"})
    lines.extend(table(top))
    lines.append("")
    return "\n".join(lines)


def build_state(sdk: str, build: str, stage: str, item: str, register: str) -> list[str]:
    item_link = f"[{item}](docs/{item})" if item else "（未登记）"
    register_link = f"[{register}](docs/{register})" if register else "（未登记）"
    return [
        STATE_BEGIN,
        "> 本块由 `tools/verification/gen_docs_map.py` 生成，勿手改；由 `check_docs_facts.py` 在预推送校验。",
        "",
        f"- 当前阶段：**{stage}**；当前工作项：{item_link}；当前登记册：{register_link}；"
        "进度以 [CURRENT_STATUS](docs/CURRENT_STATUS.md) 为准。",
        "- 交接：[docs/HANDOFF.md](docs/HANDOFF.md)（当前任务、下一步、阻塞与验证状态）。",
        f"- SDK API：`{sdk}`；日期构建：`{build}`；权威定义在 `cmake/CuexisVersion.cmake`。",
        "- 文档入口：[docs/MAP.md](docs/MAP.md)（先读它，再按任务打开 1–3 篇）。",
        "- 权威规则：事实以代码/schema/CMake 为准；`docs/stage_reports/**` 与 `docs/archive/**` 是证据，不是权威；"
        "`candidate`/`future`/`deferred` 不代表已实现。",
        "- 本文件正文只写路由与硬约束，不复述事实；冲突时以上块、`docs/MAP.md` 与 `docs/CURRENT_STATUS.md` 为准。",
        STATE_END,
    ]


def replace_state_block(text: str, block: list[str]) -> str:
    if STATE_BEGIN in text and STATE_END in text:
        start = text.index(STATE_BEGIN)
        end = text.index(STATE_END) + len(STATE_END)
        return text[:start] + "\n".join(block) + text[end:]
    marker = "## Product direction"
    if marker in text:
        index = text.index(marker)
        return text[:index] + "\n".join(block) + "\n\n" + text[index:]
    lines = text.split("\n")
    for index, line in enumerate(lines[:20]):
        if line.startswith("# "):
            rest = lines[index + 1 :]
            while rest and rest[0].strip() == "":
                rest = rest[1:]
            return "\n".join(lines[: index + 1] + [""] + block + [""] + rest)
    return "\n".join(block) + "\n\n" + text


def main(argv: list[str]) -> int:
    check = "--check" in argv
    rows = collect()
    status_row = next((row for row in rows if row["rel"] == "CURRENT_STATUS.md"), None)
    stage = "unknown"
    if status_row is not None:
        stage = str(status_row["meta"].get("current_stage", "unknown"))
    sdk, build = version_info()
    item = current_item(rows)
    register = current_register(rows)

    map_text = build_map(rows, stage, item, register)
    agents_text = AGENTS.read_text(encoding="utf-8")
    new_agents = replace_state_block(agents_text, build_state(sdk, build, stage, item, register))

    stale: list[str] = []
    if not MAP.is_file() or MAP.read_text(encoding="utf-8") != map_text:
        stale.append("docs/MAP.md")
    if new_agents != agents_text:
        stale.append("AGENTS.md")
    if check:
        if stale:
            print("generated documentation is stale: " + ", ".join(stale))
            return 1
        print(f"generated documentation is current ({len(rows)} documents)")
        return 0

    MAP.write_text(map_text, encoding="utf-8", newline="\n")
    if new_agents != agents_text:
        AGENTS.write_text(new_agents, encoding="utf-8", newline="")
    print(f"wrote docs/MAP.md ({len(map_text.splitlines())} lines) and AGENTS.md state block")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
