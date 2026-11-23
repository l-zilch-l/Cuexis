"""Deterministic documentation structure checks; notices are not semantic proof.

Rules use exact legacy paths, never inherited directory exemptions. Metadata is
shared with gen_docs_map. Topic protection compares actual Git objects, including
deleted documents; a missing Git/base is an error, not a successful protection check.
"""

from __future__ import annotations

import argparse
from collections import defaultdict
from datetime import date
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
from urllib.parse import unquote, urlsplit


_SPEC = importlib.util.spec_from_file_location("cuexis_governance_map", Path(__file__).with_name("gen_docs_map.py"))
assert _SPEC is not None and _SPEC.loader is not None
_MAP = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(_MAP)

STATUSES = {"active", "current", "accepted", "candidate", "future", "deferred", "completed",
            "historical", "superseded", "archived", "evidence", "unverified"}
AUTHORITIES = {"owns", "summarizes", "references", "index", "none"}
INACTIVE = {"completed", "historical", "superseded", "archived", "evidence", "deferred"}
KEBAB = re.compile(r"[a-z0-9]+(?:-[a-z0-9]+)*\.md\Z")
REPORT = re.compile(r"(\d{4}-\d{2}-\d{2})-[a-z0-9]+(?:-[a-z0-9]+)*\.md\Z")
ADR = re.compile(r"(\d{4})-[a-z0-9]+(?:-[a-z0-9]+)*\.md\Z")
PURPOSES = {"readiness", "decisions", "implementation", "verification", "handoffs"}
RULE_MAPS = ("legacy_names", "legacy_report_layout", "legacy_index_authority")


def is_document(relative: str) -> bool:
    parts = PurePosixPath(relative).parts
    return (relative.lower().endswith(".md") and not (relative.startswith("docs/stage_reports/") and "evidence" in parts)
            and not (relative.startswith("docs/templates/") and parts[-1] != "README.md"))


def _template_links(index: Path) -> set[Path]:
    """Resolve real Markdown links; names, images and code examples do not register templates."""
    if not index.is_file():
        return set()
    text = _MAP.unfenced_text(index.read_text(encoding="utf-8-sig"))
    text = re.sub(r"<!--.*?-->", "", text, flags=re.DOTALL)
    text = re.sub(r"(`+).*?\1", "", text, flags=re.DOTALL)
    destination = r"(?:<([^>\n]+)>|([^\s)]+))"
    urls = [left or right for left, right in re.findall(
        r"(?<!!)\[[^\]\n]+\]\(\s*" + destination, text)]
    definition = re.compile(r"^ {0,3}\[([^\]\n]+)\]:\s*" + destination + r"[^\n]*$", re.MULTILINE)
    references = {label.casefold(): left or right for label, left, right in definition.findall(text)}
    body = definition.sub("", text)
    for label, reference in re.findall(r"(?<!!)\[([^\]\n]+)\](?:\[([^\]\n]*)\])?(?!\(|:)", body):
        url = references.get((reference or label).casefold())
        if url:
            urls.append(url)
    targets: set[Path] = set()
    for url in urls:
        parsed = urlsplit(url)
        if parsed.scheme or parsed.netloc or not parsed.path:
            continue
        target = (index.parent / unquote(parsed.path)).resolve()
        if target.is_file():
            targets.add(target)
    return targets


def _topics(value: str) -> list[str]:
    if value.startswith("[") and value.endswith("]"):
        try:
            parsed = json.loads(value)
        except json.JSONDecodeError:
            parsed = [item.strip() for item in value[1:-1].split(",") if item.strip()]
    else:
        parsed = [part.strip() for part in value.split(",") if part.strip()]
    if (not isinstance(parsed, list) or not parsed or any(not isinstance(item, str) or not re.fullmatch(
            r"[a-z0-9]+(?:[.-][a-z0-9]+)*", item) for item in parsed)):
        raise ValueError("topics must be a nonempty inline list of stable lowercase topic keys")
    if len(parsed) != len(set(parsed)):
        raise ValueError("duplicate topic key")
    return parsed


def _metadata(path: Path, text: str, errors: list[tuple[Path, str]]) -> dict[str, str]:
    blocks = _MAP.header_meta_blocks(text)
    if len(blocks) != 1:
        errors.append((path, f"expected one real header cuexis-meta block, found {len(blocks)}"))
        return {}
    fields: dict[str, str] = {}
    for line in blocks[0].splitlines():
        if not line.strip():
            continue
        if ":" not in line:
            errors.append((path, "metadata line must use key: value"))
            continue
        key, value = (part.strip() for part in line.split(":", 1))
        if key in fields:
            errors.append((path, f"duplicate metadata key: {key}"))
        if not re.fullmatch(r"[a-z][a-z0-9_]*", key) or not value:
            errors.append((path, f"invalid metadata field: {key}"))
        fields[key] = value
    return fields


def _git(root: Path, *arguments: str) -> bytes:
    result = subprocess.run(["git", "-C", str(root), *arguments], capture_output=True, check=False)
    if result.returncode:
        raise ValueError(result.stderr.decode("utf-8", errors="replace").strip() or "Git command failed")
    return result.stdout


def _base_documents(root: Path, base: str) -> dict[str, str]:
    # Resolve once: both reads must use exactly the same immutable commit.
    commit = _git(root, "rev-parse", "--verify", "--end-of-options", base + "^{commit}").decode().strip()
    entries = _git(root, "ls-tree", "-rz", commit, "--", "docs").split(b"\0")
    objects: list[tuple[str, str]] = []
    for entry in entries:
        if not entry:
            continue
        attributes, raw_path = entry.split(b"\t", 1)
        relative = raw_path.decode("utf-8")
        if is_document(relative):
            objects.append((relative, attributes.split()[2].decode()))
    batch = subprocess.run(["git", "-C", str(root), "cat-file", "--batch"],
                           input="".join(oid + "\n" for _, oid in objects).encode(), capture_output=True)
    if batch.returncode:
        raise ValueError(batch.stderr.decode("utf-8", errors="replace"))
    result: dict[str, str] = {}
    offset = 0
    for relative, _ in objects:
        end = batch.stdout.index(b"\n", offset)
        size = int(batch.stdout[offset:end].split()[-1])
        offset = end + 1
        result[relative] = batch.stdout[offset:offset + size].decode("utf-8-sig")
        offset += size + 1
    return result


def _owners(documents: dict[str, dict[str, str]], root: Path,
            errors: list[tuple[Path, str]]) -> dict[str, tuple[str, str]]:
    owners: dict[str, tuple[str, str]] = {}
    for relative, meta in documents.items():
        if "topics" not in meta:
            continue
        try:
            topics = _topics(meta["topics"])
        except ValueError as error:
            errors.append((root / relative, str(error)))
            continue
        if meta.get("authority") != "owns" or meta.get("status") in INACTIVE | {"future", "unverified"}:
            continue
        for topic in topics:
            if topic in owners:
                errors.append((root / relative, f"topic {topic} has multiple active owners: {owners[topic][1]}"))
            else:
                owners[topic] = (meta.get("id", ""), relative)
    return owners


def _topic_protection(root: Path, previous: dict[str, dict[str, str]],
                      current: dict[str, dict[str, str]], errors: list[tuple[Path, str]],
                      notices: list[str], required: list[str]) -> None:
    prior = _owners(previous, root, errors)
    now = _owners(current, root, errors)
    for topic in required:
        if topic not in now:
            errors.append((root / "docs/governance_rules.json", f"required topic {topic} has no active authority: owns document"))
    for topic, old_owner in prior.items():
        if topic not in now:
            errors.append((root / old_owner[1], f"protected topic {topic} lost its active owner; deleting/renaming topics or changing authority/status cannot disable protection"))
            continue
        if old_owner == now[topic]:
            continue
        _, relative = now[topic]
        # A path move also needs a named reason, even when the stable ID is kept.
        meta = current[relative]
        reason = meta.get("topic_transfer_reason", "").strip()
        sources = [item.strip() for item in meta.get("topic_transfer_from", "").split(",")]
        if old_owner[0] not in sources or len(reason) < 8:
            errors.append((root / relative, f"topic {topic} ownership move from {old_owner[0]} ({old_owner[1]}) requires topic_transfer_from and a named topic_transfer_reason"))
    notices.append(f"topics covered: {len(now)}; unregistered/synonymous topics are not covered; semantic correctness is not checked")
    if not prior:
        notices.append("Git base has no active topics; this run establishes coverage and cannot prove earlier ownership")


def _duplicate_notices(texts: dict[str, str], metadata: dict[str, dict[str, str]],
                       changed: set[str], notices: list[str]) -> None:
    blocks: dict[str, set[str]] = defaultdict(set)
    for relative, meta in metadata.items():
        if (meta.get("status") in INACTIVE or relative.startswith(("docs/stage_reports/", "docs/archive/"))
                or meta.get("authority") not in {"owns", "summarizes", "references"}
                or "由 tools/" in texts[relative] or "generated" in texts[relative][:300].lower()):
            continue
        visible = _MAP.unfenced_text(texts[relative])
        visible = re.sub(r"<!--.*?-->", "", visible, flags=re.DOTALL)
        for block in re.split(r"\n\s*\n", visible):
            lines = block.splitlines()
            table = len(lines) >= 7 and all(line.lstrip().startswith("|") for line in lines)
            prose = len(block) >= 500 and not any(line.startswith(("#", "|")) for line in lines)
            if table or prose:
                blocks[re.sub(r"\s+", " ", block).strip()].add(relative)
    reported: set[tuple[str, ...]] = set()
    for paths in blocks.values():
        if len(paths) > 1 and paths & changed:
            pair = tuple(sorted(paths))
            if pair not in reported:
                notices.append("duplicate-content suspicion (review only): " + " <-> ".join(pair))
                reported.add(pair)


def check_repository(root: Path, base_ref: str | None = None) -> tuple[list[tuple[Path, str]], list[str]]:
    root = root.resolve()
    errors: list[tuple[Path, str]] = []
    notices: list[str] = []
    config_path = root / "docs/governance_rules.json"
    try:
        rules = json.loads(config_path.read_text(encoding="utf-8"))
        if not isinstance(rules, dict) or rules.get("version") != 1:
            raise ValueError("expected governance rules version 1")
        required = rules.get("required_topics")
        if not isinstance(required, list) or any(not isinstance(topic, str) for topic in required):
            raise ValueError("required_topics must list the initial covered scope (keys only, no owner copy)")
        if required:
            _topics(json.dumps(required))
        for key in RULE_MAPS:
            mapping = rules.get(key, {})
            if not isinstance(mapping, dict):
                raise ValueError(f"{key} must map exact existing paths to reasons")
            for relative, reason in mapping.items():
                parts = PurePosixPath(relative).parts
                if (not relative.startswith("docs/") or ".." in parts or "\\" in relative
                        or any(char in relative for char in "*?[") or not relative.endswith(".md")
                        or not isinstance(reason, str) or not reason.strip()):
                    raise ValueError(f"invalid exact exception/reason: {key}: {relative}")
    except (OSError, ValueError, TypeError) as error:
        return [(config_path, str(error))], notices

    texts: dict[str, str] = {}
    metadata: dict[str, dict[str, str]] = {}
    ids: dict[str, str] = {}
    numbers: dict[str, str] = {}
    template_links = _template_links(root / "docs/templates/README.md")
    for path in sorted(path for path in (root / "docs").rglob("*") if path.is_file() and path.suffix.lower() == ".md"):
        relative = path.relative_to(root).as_posix()
        if relative.startswith("docs/templates/") and path.name != "README.md":
            if not KEBAB.fullmatch(path.name):
                errors.append((path, "template name must be lowercase kebab-case.md"))
            if path.resolve() not in template_links:
                errors.append((path, "template must be linked from docs/templates/README.md"))
        if not is_document(relative):
            continue
        text = path.read_text(encoding="utf-8-sig")
        texts[relative] = text
        meta = _metadata(path, text, errors)
        metadata[relative] = meta
        identity = meta.get("id", "")
        if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]*", identity):
            errors.append((path, "missing or invalid metadata id"))
        elif identity in ids:
            errors.append((path, f"duplicate metadata id {identity}: {ids[identity]}"))
        else:
            ids[identity] = relative
        status, authority = meta.get("status"), meta.get("authority")
        if status not in STATUSES or authority not in AUTHORITIES:
            errors.append((path, "unknown metadata status or authority"))
        if status == "evidence" and authority != "none":
            errors.append((path, "evidence must use authority: none"))
        if (status == "unverified" and authority not in {"none", "index"}):
            errors.append((path, "unverified cannot own or summarize an accepted contract"))
        index = path.name == "README.md" or relative == "docs/MAP.md"
        legacy_index = relative in rules.get("legacy_index_authority", {}) and authority == "none"
        if index and authority != "index" and not legacy_index:
            errors.append((path, "index must use authority: index"))

        report = relative.startswith("docs/stage_reports/") and path.name not in {"README.md", "legacy-paths.md"}
        if report and (status != "evidence" or authority != "none"):
            errors.append((path, "report must declare evidence/none"))
        if relative.startswith("docs/adr/") and not index:
            match = ADR.fullmatch(path.name)
            if not match:
                errors.append((path, "ADR name must be NNNN-kebab-title.md"))
            else:
                number = match.group(1)
                if number in numbers:
                    errors.append((path, f"duplicate ADR number {number}: {numbers[number]}"))
                numbers[number] = relative
        elif relative not in rules.get("legacy_names", {}) and path.name != "README.md":
            task = path.parent.name == "tasks" and re.fullmatch(r"T-\d{2}\.md", path.name)
            if not task and not KEBAB.fullmatch(path.name):
                errors.append((path, "Markdown name must be lowercase kebab-case (task cards: T-xx.md)"))
            if report and not REPORT.fullmatch(path.name):
                errors.append((path, "report name must be YYYY-MM-DD-kebab-topic.md"))
        if re.match(r"\d{4}-\d{2}-\d{2}", path.name):
            try:
                date.fromisoformat(path.name[:10])
            except ValueError:
                errors.append((path, "report/date prefix is not a real calendar date"))
        if report:
            parts = PurePosixPath(relative).parts
            valid_layout = len(parts) == 6 and parts[2] in {"stages", "reviews"} and parts[4] in PURPOSES
            if not valid_layout and relative not in rules.get("legacy_report_layout", {}):
                errors.append((path, "report must be in readiness/decisions/implementation/verification/handoffs; legacy exceptions are exact paths"))
    try:
        base = base_ref if base_ref is not None else os.environ.get("CUEXIS_DOCS_BASE_REF", "HEAD")
        prior_texts = _base_documents(root, base)
        prior = {relative: _MAP.parse_meta(text) for relative, text in prior_texts.items()}
        for key in RULE_MAPS:
            for relative in rules.get(key, {}):
                if relative not in prior_texts:
                    errors.append((config_path, f"{key}: {relative} is not an existing document in trusted Git base; new files cannot acquire legacy exemptions"))
                elif key == "legacy_index_authority" and prior[relative].get("authority") != "none":
                    errors.append((config_path, f"{key}: {relative}: trusted Git base must already declare authority: none; a new exception cannot downgrade an index"))
        _topic_protection(root, prior, metadata, errors, notices, required)
        changed = {relative for relative, text in texts.items() if prior_texts.get(relative) != text}
    except (OSError, ValueError, UnicodeError) as error:
        errors.append((config_path, f"topic protection NOT CHECKED: Git base unavailable: {error}"))
        notices.append("topic history protection was not checked; no successful protection claim")
        _owners(metadata, root, errors)
        changed = set(texts)
    _duplicate_notices(texts, metadata, changed, notices)
    notices.append("templates, fenced examples and original evidence attachments are excluded; functional semantics are not checked")
    return errors, notices


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-ref", help="trusted Git base; default CUEXIS_DOCS_BASE_REF or HEAD")
    args = parser.parse_args(argv)
    root = Path(__file__).resolve().parents[2]
    errors, notices = check_repository(root, args.base_ref)
    for notice in notices:
        print("NOTICE: " + notice)
    for path, message in errors:
        print(f"FAIL: {path.relative_to(root)}: {message}")
    print(f"documentation governance: {len(errors)} error(s); structural scope only")
    return int(bool(errors))


if __name__ == "__main__":
    sys.exit(main())
