"""Generate docs/guides/PLAYER_OPTIONS.md from app/player/src/player_options.cpp.

Flag names and build gates are read from the parser so the page cannot drift from the
accepted command line. Descriptions live here because the parser has no help strings.

Usage:
    python -B tools/verification/gen_player_options.py
    python -B tools/verification/gen_player_options.py --check
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "app" / "player" / "src" / "player_options.cpp"
TARGET = ROOT / "docs" / "guides" / "PLAYER_OPTIONS.md"

FLAG = re.compile(r'argument == "(--[a-z0-9-]+)"')

GATED_GAMEPLAY = "`CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE`"
GATED_EXPERIMENTAL = "`CUEXIS_EXPERIMENTAL_BUILD`"

# name, value, repeat, gate, meaning
GROUPS: dict[str, list[tuple[str, str, str, str, str]]] = {
    "来源与时钟": [
        ("--chart", "路径", "一次", "—", "Chart 文本；与 `--project`、`--cxc` 互斥"),
        ("--project", "路径", "一次", "—", "文件系统工程或 `cuexis.project.json`；与 `--chart`、`--cxc` 互斥"),
        ("--cxc", "路径", "一次", "—", "CXC 包定位；与 chart/project 互斥"),
        ("--mode", "`chart` / `host` / `audio`", "一次", "—", "启动 Load 使用的时钟"),
        ("--candidate-entry", "相对路径", "一次", GATED_EXPERIMENTAL, "候选入口；需要 `--project` 或 `--cxc`"),
    ],
    "冒烟与诊断": [
        ("--smoke-test", "开关", "不查重", "—", "六帧后退出并安装表现 smoke hook；与 `--audio-smoke-test` 互斥"),
        ("--audio-smoke-test", "开关", "不查重", "—", "九十帧后退出并安装音频 smoke hook"),
        ("--show-fps", "开关", "不查重", "—", "显示 FPS"),
        ("--debug-axes", "开关", "不查重", "—", "绘制调试轴"),
        ("--shader-cache-dir", "路径", "一次", "—", "OpenGL shader cache 目录"),
        ("--frame-stats", "路径前缀", "一次", "—", "帧诊断导出前缀"),
    ],
    "候选与执行": [
        ("--execution-test-only", "开关", "一次", GATED_GAMEPLAY, "执行测试播放器；与 smoke 互斥、与 H/T step 冲突"),
        ("--gameplay-packed", "路径", "一次", "运行时要求 `--execution-test-only`", "独立 Gameplay Packed 文件"),
        ("--gameplay-configuration", "路径", "一次", GATED_GAMEPLAY, "Gameplay 配置"),
        ("--gameplay-config-budget", "值", "一次", GATED_GAMEPLAY, "Gameplay 配置预算"),
        ("--gameplay-h-step", "值", "一次", GATED_GAMEPLAY, "Gameplay H 步长"),
        ("--gameplay-presentation-step", "值", "一次", GATED_GAMEPLAY, "Gameplay 表现步长"),
        ("--gameplay-key", "值", "可重复", GATED_GAMEPLAY, "Gameplay 键位"),
        ("--execution-tail-input-deadline", "值", "一次", GATED_GAMEPLAY + "，运行时要求 execution-test", "tail 输入的截止时间"),
    ],
}

RULES = [
    "`chart` / `project` / `cxc` 三种来源互斥；`candidate-entry` 必须配合 `project` 或 `cxc`。",
    "启用 gameplay 或 execution-test 时，必须同时给出 `candidate-entry`、`gameplay-configuration`、"
    "`gameplay-config-budget` 以及 H/T steps（execution-test 自己推导 Tick 位置，拒绝 H/T step）。",
    "gameplay 与两种 smoke 互斥；`--gameplay-packed` 与 tail deadline 只在 execution-test 下有效。",
    "候选门控选项在非候选构建返回 `player.candidate.disabled`；未知参数返回 `player.arguments.unknown`。",
    "布尔开关不检查重复；带值选项按实现分别返回 `player.arguments.duplicate_*` 或 `player.arguments.unknown`。",
]


def source_flags() -> set[str]:
    found = set(FLAG.findall(SOURCE.read_text(encoding="utf-8")))
    known = {row[0] for rows in GROUPS.values() for row in rows}
    if found != known:
        missing = sorted(known - found)
        extra = sorted(found - known)
        raise SystemExit(
            "player option table is out of sync with player_options.cpp: "
            f"missing from parser={missing} undocumented={extra}"
        )
    total = sum(len(rows) for rows in GROUPS.values())
    if total != 19:
        raise SystemExit(f"expected 19 documented options, found {total}")
    return found


def build() -> str:
    source_flags()
    lines = [
        "# Player 命令行选项",
        "",
        "<!-- cuexis-meta",
        "id: GUIDE-PLAYER_OPTIONS",
        "status: current",
        "authority: references",
        "-->",
        "",
        "状态：current（由 `tools/verification/gen_player_options.py` 从 "
        "`app/player/src/player_options.cpp` 生成，勿手改）",
        "",
        "本页是 Player 选项的**单一可读来源**；`PLAYER_APPLICATION.md` 只保留结构说明并链接本页。",
        "新增或删除选项时必须同步本生成器的描述表，否则生成器拒绝生成（比的是源码里的 flag 名）。",
        "",
    ]
    for title, rows in GROUPS.items():
        lines += [f"## {title}", "", "| 选项 | 取值 | 重复 | 构建门控 | 含义 |", "| --- | --- | --- | --- | --- |"]
        for name, value, repeat, gate, meaning in rows:
            lines.append(f"| `{name}` | {value} | {repeat} | {gate} | {meaning} |")
        lines.append("")
    lines += ["## 校验规则", ""]
    lines += [f"- {rule}" for rule in RULES]
    lines.append("")
    return "\n".join(lines)


def main(argv: list[str]) -> int:
    text = build()
    if "--check" in argv:
        if not TARGET.is_file() or TARGET.read_text(encoding="utf-8") != text:
            print("docs/guides/PLAYER_OPTIONS.md is stale; run python -B tools/verification/gen_player_options.py")
            return 1
        print("docs/guides/PLAYER_OPTIONS.md is current")
        return 0
    TARGET.write_text(text, encoding="utf-8", newline="\n")
    print(f"wrote docs/guides/PLAYER_OPTIONS.md ({len(text.splitlines())} lines)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
