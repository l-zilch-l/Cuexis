"""Validate RR finding definitions, references, dispositions and Git history.

The legacy reader exists only to compare the first seven-column migration with
trusted Git input. It is never a valid format for the current working register.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import subprocess
from typing import NamedTuple


REGISTER = Path("docs/stage_plans/active/stage-07-rr/findings.md")
MARKER = "<!-- cuexis-findings: definitions -->"
COLUMNS = ("ID", "原始发现", "原始依据", "核验状态", "处置状态", "责任/任务", "当前处置依据")
LEGACY_SECTIONS = {"1", "2.1", "2.2", "2.3", "2.4", "2.5", "3", "6",
                   "7.1", "7.2", "8.2", "10.1", "11.2", "17.1", "18", "19", "23"}
VERIFICATIONS = {"未复核", "已核实"}
DISPOSITIONS = {"开放", "处理中", "已修", "已处置", "延后", "撤回"}
ID = re.compile(r"RR-F[0-9]{3}\Z")
REFERENCE = re.compile(r"(?<![\w-])(?:RR-)?F([0-9]{3})(?![\w])")
RANGE = re.compile(r"(?<![\w-])(?:RR-)?F([0-9]{3})\s*[–—-]\s*(?:(?:RR-)?F)?([0-9]{3})(?![\w])")


class Finding(NamedTuple):
    identity: str
    original: str
    evidence: str
    verification: str
    disposition: str
    responsibility: str
    current: str
    line: int


class Table(NamedTuple):
    line: int
    end: int
    section: str
    header: list[str]
    rows: list[tuple[int, list[str]]]
    marked: bool


def visible_lines(text: str) -> list[str]:
    """Keep line numbers while excluding fenced examples from registration."""
    lines: list[str] = []
    fence = ""
    for line in text.splitlines():
        match = re.match(r"^\s*(`{3,}|~{3,})", line)
        if match:
            marker = match[1]
            if not fence:
                fence = marker
            elif marker[0] == fence[0] and len(marker) >= len(fence):
                fence = ""
            lines.append("")
        else:
            lines.append("" if fence else line)
    return lines


def split_row(line: str) -> list[str]:
    """Split Markdown cells, respecting escaped pipes and matched code spans."""
    value = line.strip()
    cells: list[str] = []
    start = 1 if value.startswith("|") else 0
    index = start
    while index < len(value):
        char = value[index]
        if char == "\\":
            index += 2
            continue
        if char == "`":
            end = index + 1
            while end < len(value) and value[end] == "`":
                end += 1
            ticks = value[index:end]
            close = re.search(r"(?<!`)" + re.escape(ticks) + r"(?!`)", value[end:])
            if close:
                index = end + close.end()
                continue
            index = end
            continue
        if char == "|":
            cells.append(value[start:index].strip())
            start = index + 1
        index += 1
    if start < len(value):
        cells.append(value[start:].strip())
    return cells


def tables(text: str) -> list[Table]:
    lines = visible_lines(text)
    result: list[Table] = []
    section = ""
    index = 0
    while index < len(lines):
        heading = re.match(r"^#{1,6}\s+(\d+(?:\.\d+)*)(?:[.\s]|$)", lines[index])
        if heading:
            section = heading[1]
        if (index + 1 < len(lines) and lines[index].lstrip().startswith("|")
                and all(re.fullmatch(r":?-{3,}:?", cell) for cell in split_row(lines[index + 1]))
                and split_row(lines[index + 1])):
            previous = index - 1
            while previous >= 0 and not lines[previous].strip():
                previous -= 1
            marked = previous >= 0 and lines[previous].strip() == MARKER
            rows = []
            end = index + 2
            while end < len(lines) and lines[end].lstrip().startswith("|"):
                rows.append((end + 1, split_row(lines[end])))
                end += 1
            result.append(Table(index + 1, end, section, split_row(lines[index]), rows, marked))
            index = end
        else:
            index += 1
    return result


def legacy_fields(table: Table, cells: list[str]) -> tuple[str, str, str]:
    """Losslessly map the original row, including its old status/verdict."""
    if table.section == "24" and len(cells) == 3 and "RR-F547" in cells[0]:
        return "RR-F547", cells[1], f"原议题：{cells[0]}<br>原状态：{cells[2]}"
    identity = cells[0]
    if table.section == "23" and len(cells) == 5:
        return (identity, f"原严重度：{cells[1]}<br>{cells[2]}<br>原结论：{cells[3]}",
                f"原处置与剩余条件：{cells[4]}")
    if len(cells) == 3:
        return identity, cells[1], f"首次证据定位：未知；原证据状态：{cells[2]}"
    if table.header[1] in {"严重度", "级别"}:
        if len(cells) < 5:
            raise ValueError("legacy severity definition has missing columns")
        original = f"原严重度：{cells[1]}<br>{cells[3]}<br>原结论：{cells[4]}"
        evidence = cells[2] if len(cells) == 5 else f"原处置与剩余条件：{cells[5]}"
        return identity, original, evidence
    label = table.header[-1] if len(table.header) == len(cells) else "证据状态"
    return identity, cells[2], f"{cells[1]}<br>原{label}：{cells[3]}"


def legacy_definitions(text: str) -> tuple[dict[str, Finding], list[str]]:
    records: dict[str, Finding] = {}
    errors: list[str] = []
    for table in tables(text):
        if table.section not in LEGACY_SECTIONS | {"24"}:
            continue
        for number, cells in table.rows:
            if not (cells and (ID.fullmatch(cells[0]) or
                    (table.section == "24" and "RR-F547" in cells[0]))):
                continue
            if len(cells) not in (3, 4, 5, 6):
                errors.append(f"line {number}: unsupported legacy definition width")
                continue
            try:
                identity, original, evidence = legacy_fields(table, cells)
            except (IndexError, ValueError) as error:
                errors.append(f"line {number}: {error}")
                continue
            if identity in records:
                errors.append(f"line {number}: duplicate legacy definition {identity}")
            records[identity] = Finding(identity, original, evidence, "", "", "", "", number)
    if not records:
        errors.append("trusted legacy baseline contains no recognized definitions")
    return records, errors


def normalized(value: str) -> str:
    value = re.sub(r"<br\s*/?>", " ", value, flags=re.IGNORECASE)
    value = value.replace(r"\|", "|")
    return re.sub(r"\s+", " ", value).strip()


def evidence_fields(value: str) -> dict[str, str]:
    fields: dict[str, str] = {}
    for part in re.split(r"<br\s*/?>|[；\n]", value):
        match = re.match(r"\s*([^：:]+)[：:]\s*(.+?)\s*$", part)
        if match:
            fields[match[1].strip()] = match[2].strip()
    return fields


def parse_register(text: str) -> tuple[dict[str, Finding], list[str]]:
    records: dict[str, Finding] = {}
    errors: list[str] = []
    parsed = tables(text)
    lines = visible_lines(text)
    marked_tables = [table for table in parsed if table.marked]
    marker_count = sum(line.strip() == MARKER for line in visible_lines(text))
    if not marked_tables:
        errors.append("no marked definition tables; current register must use seven-column definitions")
    if marker_count != len(marked_tables):
        errors.append("definition marker must immediately precede exactly one table")
    for table in parsed:
        if not table.marked:
            if tuple(table.header) == COLUMNS:
                errors.append(f"line {table.line}: definition table has lost its marker")
            elif table.section in LEGACY_SECTIONS and table.header[:1] == ["ID"]:
                errors.append(f"line {table.line}: legacy definition table must be migrated and marked")
            continue
        if tuple(table.header) != COLUMNS:
            errors.append(f"line {table.line}: definition columns must be {', '.join(COLUMNS)}")
            continue
        if len(split_row(lines[table.line])) != len(COLUMNS):
            errors.append(f"line {table.line + 1}: definition separator must have seven cells")
        if not table.rows:
            errors.append(f"line {table.line}: empty definition table")
        for number, cells in table.rows:
            if len(cells) != len(COLUMNS):
                errors.append(f"line {number}: expected seven definition cells, found {len(cells)}")
                continue
            record = Finding(*cells, number)
            prefix = f"line {number}: {record.identity}"
            if not ID.fullmatch(record.identity):
                errors.append(f"{prefix}: invalid definition ID")
                continue
            if record.identity in records:
                errors.append(f"{prefix}: duplicate definition")
            records[record.identity] = record
            if any(not cell.strip() for cell in cells):
                errors.append(f"{prefix}: all seven fields must be nonempty")
            if record.responsibility.strip() in {"-", "—", "无", "未知", "待定", "待补"}:
                errors.append(f"{prefix}: missing responsible task or named follow-up")
            if record.verification not in VERIFICATIONS:
                errors.append(f"{prefix}: invalid verification state")
            if record.disposition not in DISPOSITIONS:
                errors.append(f"{prefix}: invalid disposition state")
            fields = evidence_fields(record.current)
            required = {"依据"}
            if record.verification == "已核实":
                required.update(("复核者", "复核证据"))
            if record.disposition in {"已修", "已处置", "撤回"}:
                if record.verification != "已核实":
                    errors.append(f"{prefix}: closing requires verified evidence")
                required.update(("理由", "决定依据"))
            if record.disposition == "已修":
                required.update(("改动修订", "实际验证"))
            if record.disposition == "延后":
                required.update(("责任阶段", "恢复条件", "决定依据"))
            for key in sorted(required):
                if fields.get(key, "").strip() in {"", "未知", "无", "待补", "未检查", "未执行"}:
                    errors.append(f"{prefix}: missing disposition evidence field {key}")
            if "首次证据定位：未知" in record.evidence and record.disposition != "开放":
                if record.verification != "已核实" or not fields.get("补证依据"):
                    errors.append(f"{prefix}: unknown original evidence must remain open until verified supplementation")
    visible = "\n".join(visible_lines(text))
    references = {"RR-F" + match[1] for match in REFERENCE.finditer(visible)}
    for match in RANGE.finditer(visible):
        first, last = int(match[1]), int(match[2])
        if first > last:
            errors.append(f"reversed finding range: {match[0]}")
        else:
            references.update(f"RR-F{value:03}" for value in range(first, last + 1))
    for identity in sorted(references - records.keys()):
        errors.append(f"undefined finding reference: {identity}")
    return records, errors


def compare_protected(current: dict[str, Finding], baseline: dict[str, Finding]) -> list[str]:
    errors: list[str] = []
    for identity, old in baseline.items():
        new = current.get(identity)
        if new is None:
            errors.append(f"protected definition removed or renumbered: {identity}")
            continue
        for field in ("original", "evidence"):
            if normalized(getattr(old, field)) != normalized(getattr(new, field)):
                errors.append(f"{identity}: protected {field} changed; append a dated correction instead")
        if (old.disposition in {"已修", "已处置", "延后", "撤回"}
                and new.disposition != old.disposition
                and (f"历史处置：{old.disposition}" not in new.current
                     or normalized(old.current) not in normalized(new.current))):
            errors.append(f"{identity}: changing a disposition must preserve its history and reasons")
    return errors


def git(root: Path, *args: str) -> str:
    process = subprocess.run(["git", "-C", str(root), *args], capture_output=True,
                             encoding="utf-8", errors="strict", check=False)
    if process.returncode:
        raise ValueError(process.stderr.strip() or "Git returned no usable history")
    return process.stdout


def check_repository(root: Path, base_ref: str | None = None) -> tuple[list[tuple[Path, str]], list[str]]:
    root = root.resolve()
    path = root / REGISTER
    errors: list[tuple[Path, str]] = []
    notes: list[str] = []
    try:
        text = path.read_text(encoding="utf-8")
        records, issues = parse_register(text)
        errors.extend((path, issue) for issue in issues)
        for record in records.values():
            for task in re.findall(r"\bT-[0-9]{2}\b", record.responsibility):
                if not (path.parent / "tasks" / f"{task}.md").is_file():
                    errors.append((path, f"{record.identity}: unknown responsible task {task}"))
    except (OSError, UnicodeError) as error:
        return [(path, f"cannot read findings: {error}")], notes
    selected = base_ref if base_ref is not None else os.environ.get("CUEXIS_DOCS_BASE_REF", "HEAD")
    try:
        if not selected or selected.startswith("-"):
            raise ValueError("base revision is empty or an option")
        revision = git(root, "rev-parse", "--verify", selected + "^{commit}").strip()
        old = git(root, "show", f"{revision}:{REGISTER.as_posix()}")
        if any(line.strip() == MARKER for line in visible_lines(old)):
            baseline, issues = parse_register(old)
        else:
            # A previously migrated register may not revert to the legacy escape path.
            baseline, issues = legacy_definitions(old)
            notes.append(f"first migration: compared original rows from trusted legacy revision {revision}")
        errors.extend((path, f"baseline: {issue}") for issue in issues)
        errors.extend((path, issue) for issue in compare_protected(records, baseline))
        if not issues:
            notes.append(f"Git protection checked against {revision}: {len(baseline)} definitions")
    except (OSError, UnicodeError, ValueError) as error:
        errors.append((path, f"Git protection NOT CHECKED: {error}"))
    notes.append(f"current definitions: {len(records)}; evidence sufficiency remains a human review")
    return errors, notes


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-ref", help="trusted Git base (otherwise CUEXIS_DOCS_BASE_REF or HEAD)")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()
    failures, notes = check_repository(args.root, args.base_ref)
    for note in notes:
        print(note)
    for path, message in failures:
        print(f"FAIL: {path}: {message}")
    return int(bool(failures))


if __name__ == "__main__":
    raise SystemExit(main())
