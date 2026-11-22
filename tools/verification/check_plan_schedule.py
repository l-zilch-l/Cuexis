"""Validate the Stage Plan schedule and its task-card/acceptance references.

Usage:
    python -B tools/verification/check_plan_schedule.py
    python -B tools/verification/check_plan_schedule.py --stage-dir PATH

The schedule has four exact columns, positive decimal waves, T-xx task IDs,
comma-separated prerequisite IDs (or an em dash), and 可并行/独占 modes.
Task IDs may be bare, inline code, or links to the matching tasks/T-xx.md card.
Acceptance coverage is checked when acceptance.md exists; explicit T-01–T-03
ranges are supported. Relative --stage-dir paths use the working directory.
All cards must contain the seven task sections and declare active/candidate/completed
status. Candidate and completed cards are checked but excluded from current-stage dispatch.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_STAGE_DIR = ROOT / "docs" / "stage_plans" / "active" / "stage-07-rr"
TASK_ID = re.compile(r"T-[0-9]{2}\Z")
TASK_REFERENCE = re.compile(r"(?<![A-Za-z0-9_-])T-[0-9]{2}(?![A-Za-z0-9_-])")
TASK_RANGE = re.compile(r"\b(T-[0-9]{2})\s*[-\u2013\u2014~\uff5e]\s*(T-[0-9]{2})\b")
EXPECTED_HEADER = ("波次", "任务 ID", "前置 ID", "可并行/独占")
TASK_SECTIONS = ("目标", "输入", "写域", "步骤", "任务级验收", "证据", "停止条件")
FENCE = re.compile(r"^\s*(```+|~~~+)\s*([A-Za-z0-9_-]*)\s*$")
METADATA_VALUES = {
    "id": r"[A-Za-z0-9_.:/-]+",
    "status": r"[A-Za-z][A-Za-z_-]*",
    "authority": r"(?:owns|summarizes|references|index|none)",
    "date": r"[0-9]{4}-[0-9]{2}-[0-9]{2}",
}


def _cells(line: str) -> list[str]:
    value = line.strip()
    if value.startswith("|"):
        value = value[1:]
    if value.endswith("|"):
        value = value[:-1]
    return [cell.strip() for cell in value.split("|")]


def _is_separator(cells: list[str]) -> bool:
    return bool(cells) and all(re.fullmatch(r":?-{3,}:?", cell) for cell in cells)


def _table_id(value: str) -> str | None:
    link = re.fullmatch(r"\[(`?T-[0-9]{2}`?)\]\((?:\./)?tasks/(T-[0-9]{2})\.md\)", value)
    if link:
        label, target = link.groups()
        return target if label in (target, f"`{target}`") else None
    if value.startswith("`") and value.endswith("`"):
        value = value[1:-1]
    return value if TASK_ID.fullmatch(value) else None


def _acceptance_ids(text: str) -> set[str]:
    ids = set(TASK_REFERENCE.findall(text))
    for first, last in TASK_RANGE.findall(text):
        start = int(first[2:])
        end = int(last[2:])
        if end >= start:
            ids.update(f"T-{number:02d}" for number in range(start, end + 1))
    return ids


def _parse_schedule(path: Path) -> tuple[list[dict[str, object]], set[str], list[str]]:
    failures: list[str] = []
    entries: list[dict[str, object]] = []
    graph_nodes: set[str] = set()
    graph_edges: set[tuple[str, str]] = set()
    seen_ids: set[str] = set()
    in_metadata = False
    metadata_seen = False
    in_mermaid = False
    mermaid_seen = False
    mermaid_lines: list[tuple[int, str]] = []
    table_started = False
    table_closed = False
    table_header_seen = False
    separator_seen = False
    h1_count = 0
    last_wave: int | None = None
    metadata_keys: set[str] = set()
    mermaid_fence_length = 0

    lines = path.read_text(encoding="utf-8").splitlines()
    for number, raw in enumerate(lines, start=1):
        line = raw.strip()
        if in_metadata:
            if line == "-->":
                in_metadata = False
            else:
                key, separator, value = line.partition(":")
                pattern = METADATA_VALUES.get(key)
                if (not separator or pattern is None or key in metadata_keys
                        or not re.fullmatch(pattern, value.strip())):
                    failures.append(f"line {number}: invalid or duplicate cuexis-meta metadata entry")
                metadata_keys.add(key)
            continue

        fence = FENCE.fullmatch(raw)
        if in_mermaid:
            if (fence and fence.group(2) == "" and fence.group(1).startswith("`")
                    and len(fence.group(1)) >= mermaid_fence_length):
                in_mermaid = False
            else:
                mermaid_lines.append((number, raw.strip()))
            continue

        if not line:
            if table_started:
                table_closed = True
            continue
        if line.startswith("<!-- cuexis-meta"):
            if metadata_seen or table_started or line != "<!-- cuexis-meta":
                failures.append(f"line {number}: misplaced or duplicate cuexis-meta block")
            metadata_seen = True
            in_metadata = True
            continue
        if fence:
            if table_started:
                table_closed = True
            if fence.group(2).lower() != "mermaid":
                failures.append(f"line {number}: only a Mermaid fence is allowed")
            elif mermaid_seen:
                failures.append(f"line {number}: only one Mermaid diagram is allowed")
            elif fence.group(1).startswith("~~~"):
                failures.append(f"line {number}: Mermaid must use a backtick fence")
            else:
                mermaid_seen = True
                in_mermaid = True
                mermaid_fence_length = len(fence.group(1))
            continue
        if line.startswith("# "):
            if table_started or h1_count:
                failures.append(f"line {number}: schedule may contain one H1 before its table")
            h1_count += 1
            continue
        if not table_started and (
            re.fullmatch(r"状态[：:]\s*[A-Za-z][A-Za-z_-]*", line)
            or re.fullmatch(r"更新日期[：:]\s*[0-9]{4}-[0-9]{2}-[0-9]{2}", line)
        ):
            continue
        if "|" in line:
            if table_closed:
                failures.append(f"line {number}: schedule table must be contiguous")
                continue
            table_started = True
            cells = _cells(line)
            if not table_header_seen:
                table_header_seen = True
                if tuple(cells) != EXPECTED_HEADER:
                    failures.append(
                        f"line {number}: table header must be " + " | ".join(EXPECTED_HEADER)
                    )
                continue
            if not separator_seen:
                separator_seen = True
                if len(cells) != 4 or not _is_separator(cells):
                    failures.append(f"line {number}: expected a four-column Markdown separator")
                continue
            if len(cells) != 4:
                failures.append(f"line {number}: schedule rows must have exactly four columns")
                continue

            wave_text, task_text, dependency_text, mode = cells
            if not re.fullmatch(r"[1-9][0-9]*", wave_text):
                failures.append(f"line {number}: wave must be a positive decimal integer")
                wave: int | None = None
            else:
                wave = int(wave_text)
                if last_wave is not None and wave < last_wave:
                    failures.append(f"line {number}: schedule waves must be in nondecreasing order")
                last_wave = wave
            task_id = _table_id(task_text)
            if task_id is None:
                failures.append(f"line {number}: task ID must be T-xx or a link to its matching task card")
            elif task_id in seen_ids:
                failures.append(f"line {number}: task ID is scheduled more than once: {task_id}")
            else:
                seen_ids.add(task_id)

            if dependency_text == "\u2014":
                dependencies: list[str] = []
            else:
                values = [value.strip() for value in dependency_text.split(",")]
                dependencies = []
                for value in values:
                    dependency = _table_id(value)
                    if dependency is None:
                        failures.append(f"line {number}: invalid prerequisite ID: {value}")
                    else:
                        dependencies.append(dependency)
                if len(set(dependencies)) != len(dependencies):
                    failures.append(f"line {number}: prerequisite IDs must not repeat")

            if mode not in ("\u53ef\u5e76\u884c", "\u72ec\u5360"):
                failures.append(f"line {number}: mode must be 可并行 or 独占")
            entries.append({
                "line": number,
                "wave": wave,
                "task_id": task_id,
                "dependencies": dependencies,
                "mode": mode,
            })
            continue

        if table_started:
            table_closed = True
        failures.append(f"line {number}: content outside the table, H1, metadata, or Mermaid diagram")

    if in_metadata:
        failures.append("cuexis-meta block is not closed")
    if in_mermaid:
        failures.append("Mermaid fence is not closed")
    if not table_header_seen or not separator_seen:
        failures.append("schedule must contain a four-column Markdown table")
    if not entries:
        failures.append("schedule table must contain at least one task")

    rows_by_id = {
        entry["task_id"]: entry for entry in entries if isinstance(entry["task_id"], str)
    }
    wave_sizes: dict[int, int] = {}
    exclusive_waves: set[int] = set()
    for entry in entries:
        wave = entry["wave"]
        if isinstance(wave, int):
            wave_sizes[wave] = wave_sizes.get(wave, 0) + 1
            if entry["mode"] == "\u72ec\u5360":
                exclusive_waves.add(wave)
        for dependency in entry["dependencies"]:
            dependency_row = rows_by_id.get(dependency)
            if dependency_row is None:
                failures.append(
                    f"line {entry['line']}: prerequisite is not scheduled: {dependency}"
                )
            elif isinstance(wave, int) and isinstance(dependency_row["wave"], int):
                if dependency_row["wave"] >= wave:
                    failures.append(
                        f"line {entry['line']}: prerequisite {dependency} must be in an earlier wave"
                    )
    for wave in sorted(exclusive_waves):
        if wave_sizes[wave] != 1:
            failures.append(f"exclusive wave {wave} must contain exactly one task")

    if mermaid_seen:
        direction_seen = False
        for number, line in mermaid_lines:
            if not line:
                continue
            if not direction_seen and re.fullmatch(r"(?:graph|flowchart)\s+(?:TB|TD|BT|RL|LR)", line):
                direction_seen = True
                continue
            if not direction_seen:
                failures.append(f"line {number}: Mermaid diagram must start with graph/flowchart direction")
                continue
            edge = re.fullmatch(r"(T-[0-9]{2})\s+-->\s+(T-[0-9]{2});?", line)
            if edge:
                source, target = edge.groups()
                graph_nodes.update((source, target))
                graph_edges.add((source, target))
                continue
            node = re.fullmatch(r"(T-[0-9]{2});?", line)
            if node:
                graph_nodes.add(node.group(1))
                continue
            failures.append(f"line {number}: Mermaid may contain only bare task IDs and ID-to-ID arrows")
        if not direction_seen:
            failures.append("Mermaid diagram is missing its graph/flowchart direction")
        expected_nodes = set(rows_by_id)
        if graph_nodes != expected_nodes:
            missing = sorted(expected_nodes - graph_nodes)
            unknown = sorted(graph_nodes - expected_nodes)
            if missing:
                failures.append(f"Mermaid diagram omits scheduled task IDs: {', '.join(missing)}")
            if unknown:
                failures.append(f"Mermaid diagram has unknown task IDs: {', '.join(unknown)}")
        expected_edges = {
            (dependency, entry["task_id"])
            for entry in entries
            for dependency in entry["dependencies"]
            if isinstance(entry["task_id"], str)
        }
        for source, target in sorted(graph_edges - expected_edges):
            failures.append(f"Mermaid edge {source} --> {target} is not declared in the table")
        for source, target in sorted(expected_edges - graph_edges):
            failures.append(f"Mermaid diagram omits table prerequisite edge {source} --> {target}")

    return entries, seen_ids, failures


def _card_status(text: str) -> str | None:
    blocks = re.findall(r"<!-- cuexis-meta\s*\n(.*?)\n-->", text, re.DOTALL)
    if len(blocks) != 1:
        return None
    statuses = re.findall(r"^status:\s*(\S+)\s*$", blocks[0], re.MULTILINE)
    return statuses[0] if len(statuses) == 1 else None


def _card_sections(text: str) -> list[tuple[str, bool]]:
    sections: list[tuple[str, bool]] = []
    name: str | None = None
    has_body = False
    fence_char: str | None = None
    fence_length = 0
    in_comment = False
    for raw in text.splitlines():
        line = raw.strip()
        if in_comment:
            if "-->" in line:
                in_comment = False
            continue
        if fence_char is None and line.startswith("<!--"):
            in_comment = "-->" not in line
            continue
        fence = re.match(r"^(`{3,}|~{3,})(.*)$", line)
        if fence_char is not None:
            if (fence and fence.group(1)[0] == fence_char
                    and len(fence.group(1)) >= fence_length
                    and not fence.group(2).strip()):
                fence_char = None
            elif line:
                has_body = True
            continue
        if fence:
            fence_char, fence_length = fence.group(1)[0], len(fence.group(1))
            continue
        heading = re.fullmatch(r" {0,3}##[ \t]+(.+?)[ \t]*", raw)
        if heading:
            if name is not None:
                sections.append((name, has_body))
            name, has_body = heading.group(1), False
        elif line and name is not None:
            has_body = True
    if name is not None:
        sections.append((name, has_body))
    return sections


def validate_stage_dir(stage_dir: Path) -> list[tuple[Path, str]]:
    """Return path/message pairs for schedule, task-card, and optional acceptance failures."""
    stage_dir = stage_dir.resolve()
    schedule_path = stage_dir / "schedule.md"
    task_dir = stage_dir / "tasks"
    findings: list[tuple[Path, str]] = []
    if not schedule_path.is_file():
        return [(schedule_path, "schedule.md is missing")]

    entries, scheduled_ids, failures = _parse_schedule(schedule_path)
    findings.extend((schedule_path, message) for message in failures)

    if not task_dir.is_dir():
        findings.append((task_dir, "tasks directory is missing"))
        task_paths: list[Path] = []
    else:
        task_paths = sorted(task_dir.glob("*.md"))

    task_ids: set[str] = set()
    statuses: dict[str, str | None] = {}
    for path in task_paths:
        task_id = path.stem
        if not TASK_ID.fullmatch(task_id):
            findings.append((path, "task card filename must use T-xx.md"))
            continue
        if task_id in task_ids:
            findings.append((path, f"duplicate task-card ID: {task_id}"))
        task_ids.add(task_id)
        text = path.read_text(encoding="utf-8")
        status = _card_status(text)
        statuses[task_id] = status
        if status not in ("active", "candidate", "completed"):
            findings.append((path, "task metadata status must be active, candidate, or completed"))
        visible_status = re.findall(r"^状态[：:]\s*([^\s；;]+)\s*$", text, re.MULTILINE)
        if visible_status != [status]:
            findings.append((path, "task visible status must match metadata status"))
        sections = _card_sections(text)
        if tuple(name for name, _ in sections) != TASK_SECTIONS:
            findings.append((path, "task sections must occur once in order: "
                             + ", ".join(TASK_SECTIONS)))
        for name, has_body in sections:
            if name in TASK_SECTIONS and not has_body:
                findings.append((path, f"task section must not be empty: {name}"))
        for number, line in enumerate(text.splitlines(), start=1):
            if re.search(r"\bdepends_on\b", line, re.IGNORECASE):
                findings.append((path, f"line {number}: task cards must not define depends_on"))

    for entry in entries:
        status = statuses.get(entry["task_id"])
        if status in ("active", "completed"):
            for dependency in entry["dependencies"]:
                if statuses.get(dependency) == "candidate":
                    findings.append((schedule_path,
                        f"line {entry['line']}: {status} task {entry['task_id']} "
                        f"must not depend on candidate task {dependency}"))

    for task_id in sorted(scheduled_ids - task_ids):
        findings.append((schedule_path, f"scheduled task has no task card: {task_id}"))
    for task_id in sorted(task_ids - scheduled_ids):
        findings.append((task_dir / f"{task_id}.md", f"task card is not scheduled: {task_id}"))

    acceptance_path = stage_dir / "acceptance.md"
    if acceptance_path.is_file():
        acceptance = acceptance_path.read_text(encoding="utf-8")
        acceptance_ids = _acceptance_ids(acceptance)
        for task_id in sorted(acceptance_ids - task_ids):
            findings.append((acceptance_path, f"acceptance references unknown task ID: {task_id}"))
        for task_id in sorted(task_ids - acceptance_ids):
            findings.append((acceptance_path, f"acceptance does not cover task ID: {task_id}"))

    return findings


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage-dir", type=Path, default=DEFAULT_STAGE_DIR)
    args = parser.parse_args(argv)

    findings = validate_stage_dir(args.stage_dir)
    if findings:
        print("Plan schedule checks failed:")
        for path, message in findings:
            print(f"- {path}: {message}")
        return 1

    groups: dict[str, list[str]] = {"active": [], "candidate": [], "completed": []}
    for path in sorted((args.stage_dir / "tasks").glob("*.md")):
        status = _card_status(path.read_text(encoding="utf-8"))
        groups[status].append(path.stem)
    print(f"Plan schedule checks passed: {len(groups['active'])} active task cards; "
          f"{len(groups['candidate'])} candidate cards; "
          f"{len(groups['completed'])} completed cards.")
    print("Current-stage cards (adoption and prerequisites still apply): "
          + ", ".join(groups["active"]))
    print("Candidate-only cards (separate adoption required): "
          + (", ".join(groups["candidate"]) or "none"))
    print("Completed cards: " + (", ".join(groups["completed"]) or "none"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
