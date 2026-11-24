"""Positive and negative contracts for the RR findings gate (no product build)."""

from __future__ import annotations

import contextlib
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch


SPEC = importlib.util.spec_from_file_location("check_findings", Path(__file__).with_name("check_findings.py"))
assert SPEC and SPEC.loader
checker = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(checker)

HEADER = "| " + " | ".join(checker.COLUMNS) + " |\n| " + " | ".join(["---"] * 7) + " |\n"
LEGACY = """# Register

### 1. Original findings

| ID | 位置 | 问题 | 证据状态 |
| --- | --- | --- | --- |
| RR-F001 | `source.cpp:1` | Original claim | 已修（旧说法） |
"""


def row(identity="RR-F001", original="Original claim",
        evidence="`source.cpp:1`<br>原证据状态：已修（旧说法）",
        verification="未复核", disposition="开放", responsibility="T-09",
        current="依据：原记录；迁移说明：未重新核验"):
    return "| " + " | ".join((identity, original, evidence, verification,
                                disposition, responsibility, current)) + " |\n"


def document(*rows, tail=""):
    return "# Register\n\n" + checker.MARKER + "\n" + HEADER + "".join(rows or [row()]) + tail


@contextlib.contextmanager
def base_environment(value):
    """Restore only this key; copying Windows' entire environment loses casing."""
    key = "CUEXIS_DOCS_BASE_REF"
    previous = os.environ.get(key)
    if value is None:
        os.environ.pop(key, None)
    else:
        os.environ[key] = value
    try:
        yield
    finally:
        if previous is None:
            os.environ.pop(key, None)
        else:
            os.environ[key] = previous


class ParseTests(unittest.TestCase):
    def errors(self, text):
        return "\n".join(checker.parse_register(text)[1])

    def test_open_definition_and_repeated_references_are_valid(self):
        text = document(tail="\nRR-F001 / F001 / RR-F001\n\n| ID | Action |\n| --- | --- |\n| RR-F001 | Follow-up |\n")
        records, errors = checker.parse_register(text)
        self.assertFalse(errors)
        self.assertEqual(list(records), ["RR-F001"])

    def test_escaped_pipes_and_code_spans(self):
        self.assertEqual(checker.split_row(r"| a\|b | `a|b` | ``a`|b`` | tail |"),
                         [r"a\|b", "`a|b`", "``a`|b``", "tail"])
        self.assertFalse(self.errors(document(row(original=r"a\|b and `x|y`"))))

    def test_unmatched_tick_does_not_hide_a_column(self):
        self.assertEqual(checker.split_row("| `unfinished | next |"), ["`unfinished", "next"])

    def test_fenced_examples_are_not_definitions_or_references(self):
        text = document(tail="\n```markdown\n" + document(row("RR-F999")) + "\n```\n")
        self.assertFalse(self.errors(text))

    def test_duplicate_definitions_fail(self):
        self.assertIn("duplicate definition", self.errors(document(row(), row())))

    def test_unknown_full_and_abbreviated_references_fail(self):
        for ref in ("RR-F999", "F999"):
            with self.subTest(ref=ref):
                self.assertIn("undefined finding reference: RR-F999", self.errors(document(tail=ref)))

    def test_reference_ranges_check_intermediate_ids(self):
        text = document(row(), row("RR-F003"), tail="F001–F003")
        self.assertIn("undefined finding reference: RR-F002", self.errors(text))

    def test_reversed_ranges_fail(self):
        self.assertIn("reversed finding range", self.errors(document(tail="F003–F001")))

    def test_missing_or_cross_section_marker_fails(self):
        text = document().replace(checker.MARKER, "")
        self.assertIn("lost its marker", self.errors(text))
        text = document().replace(checker.MARKER, checker.MARKER + "\n## A new section")
        self.assertIn("immediately precede", self.errors(text))

    def test_marker_does_not_apply_to_the_next_table(self):
        text = document() + "\n" + HEADER + row("RR-F002")
        self.assertIn("lost its marker", self.errors(text))

    def test_unknown_columns_and_extra_cells_fail(self):
        self.assertIn("definition columns", self.errors(document().replace("原始发现", "摘要")))
        self.assertIn("seven definition cells", self.errors(document(row().replace(" |\n", " | extra |\n"))))

    def test_invalid_states_and_empty_responsibility_fail(self):
        self.assertIn("invalid verification", self.errors(document(row(verification="成立"))))
        self.assertIn("invalid disposition", self.errors(document(row(disposition="已核实"))))
        self.assertIn("responsible task", self.errors(document(row(responsibility="待定"))))

    def test_verified_requires_named_reviewer_and_evidence(self):
        errors = self.errors(document(row(verification="已核实")))
        self.assertIn("复核者", errors)
        self.assertIn("复核证据", errors)

    def test_fixed_requires_revision_validation_and_decision(self):
        errors = self.errors(document(row(disposition="已修")))
        for expected in ("closing requires verified", "改动修订", "实际验证", "理由", "决定依据"):
            self.assertIn(expected, errors)
        proof = "依据：记录；复核者：reviewer；复核证据：报告#case；改动修订：abcdef0；实际验证：command exit 0；理由：修正；决定依据：accepted decision"
        self.assertFalse(self.errors(document(row(verification="已核实", disposition="已修", current=proof))))

    def test_deferred_requires_owner_and_resume_conditions(self):
        errors = self.errors(document(row(disposition="延后")))
        for expected in ("责任阶段", "恢复条件", "决定依据"):
            self.assertIn(expected, errors)
        proof = "依据：记录；责任阶段：Stage 14；恢复条件：取得消费者证据；决定依据：owner record"
        self.assertFalse(self.errors(document(row(disposition="延后", current=proof))))

    def test_disposed_and_withdrawn_need_verified_reasons(self):
        for disposition in ("已处置", "撤回"):
            errors = self.errors(document(row(disposition=disposition)))
            self.assertIn("closing requires verified", errors)
            self.assertIn("决定依据", errors)

    def test_unknown_source_stays_open(self):
        self.assertIn("unknown original evidence", self.errors(document(row(evidence="首次证据定位：未知", disposition="处理中"))))

    def test_later_evidence_can_resolve_unknown_source_without_rewriting_first_evidence(self):
        proof = "依据：new record；复核者：reviewer；复核证据：report；补证依据：dated source"
        self.assertFalse(self.errors(document(row(evidence="首次证据定位：未知", verification="已核实", disposition="处理中", current=proof))))

    def test_separator_width_is_checked(self):
        text = document().replace("| " + " | ".join(["---"] * 7) + " |", "| --- |")
        self.assertIn("separator must have seven", self.errors(text))

    def test_current_legacy_structure_is_rejected(self):
        self.assertIn("must be migrated", self.errors(LEGACY))

    def test_legacy_mapping_keeps_old_status_without_claiming_new_verification(self):
        records, errors = checker.legacy_definitions(LEGACY)
        self.assertFalse(errors)
        self.assertEqual(records["RR-F001"].original, "Original claim")
        self.assertEqual(records["RR-F001"].evidence, "`source.cpp:1`<br>原证据状态：已修（旧说法）")
        current, errors = checker.parse_register(document())
        self.assertFalse(errors)
        self.assertFalse(checker.compare_protected(current, records))

    def test_legacy_malformed_width_and_embedded_f547_have_lossless_mapping(self):
        text = """# Register
### 6. Architecture
| ID | 发现 | 证据状态 |
| --- | --- | --- |
| RR-F301 | source | fact | old evidence |
## 24. Decisions
| 议题 | 当前依据与处理 | 状态 |
| --- | --- | --- |
| topic (RR-F547) | accepted boundary | not implemented |
"""
        records, errors = checker.legacy_definitions(text)
        self.assertFalse(errors)
        self.assertEqual(records["RR-F301"].original, "fact")
        self.assertEqual(records["RR-F301"].evidence, "source<br>原证据状态：old evidence")
        self.assertEqual(records["RR-F547"].original, "accepted boundary")
        self.assertIn("not implemented", records["RR-F547"].evidence)

    def test_reopen_preserves_previous_disposition_and_reasons(self):
        previous, _ = checker.parse_register(document(row(disposition="延后", current="依据：record；责任阶段：future；恢复条件：evidence；决定依据：owner")))
        reopened, _ = checker.parse_register(document())
        self.assertIn("preserve its history", "\n".join(checker.compare_protected(reopened, previous)))
        old = previous["RR-F001"]
        reopened, _ = checker.parse_register(document(row(current="依据：new evidence；历史处置：延后<br>" + old.current)))
        self.assertFalse(checker.compare_protected(reopened, previous))


class GitTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.path = self.root / checker.REGISTER
        self.path.parent.mkdir(parents=True)
        task = self.path.parent / "tasks/T-09.md"
        task.parent.mkdir()
        task.write_text("# Task\n", encoding="utf-8")
        self.git("init", "--quiet")

    def git(self, *args):
        result = subprocess.run(["git", "-C", str(self.root), "-c", "user.name=Test",
                                 "-c", "user.email=test@example.invalid", *args],
                                capture_output=True, text=True, encoding="utf-8", check=True)
        return result.stdout.strip()

    def commit(self, text):
        self.path.write_text(text, encoding="utf-8")
        self.git("add", ".")
        self.git("commit", "--quiet", "-m", "fixture")
        return self.git("rev-parse", "HEAD")

    def check(self, base=None):
        with base_environment(None):
            return checker.check_repository(self.root, base)

    def test_initial_legacy_to_new_migration_uses_actual_git_content(self):
        self.commit(LEGACY)
        self.path.write_text(document(), encoding="utf-8")
        errors, notes = self.check()
        self.assertFalse(errors)
        self.assertTrue(any("first migration" in note for note in notes))

    def test_new_format_baseline_protects_claim_and_first_evidence(self):
        self.commit(document())
        for text in (document(row(original="rewritten")), document(row(evidence="new first evidence"))):
            self.path.write_text(text, encoding="utf-8")
            self.assertTrue(any("protected" in message for _, message in self.check()[0]))

    def test_arbitrary_evidence_link_target_change_is_not_stripped(self):
        original = document(row(evidence="[source](report-a.md#proof)"))
        self.commit(original)
        self.path.write_text(document(row(evidence="[source](report-b.md#proof)")), encoding="utf-8")
        self.assertTrue(any("protected evidence" in message for _, message in self.check()[0]))

    def test_missing_git_executable_fails_closed(self):
        self.commit(document())
        with patch.object(checker, "git", side_effect=FileNotFoundError("git")):
            errors, _ = self.check()
        self.assertTrue(any("NOT CHECKED" in message for _, message in errors))

    def test_baseline_without_register_cannot_certify_protection(self):
        self.commit(document())
        self.path.unlink()
        self.git("add", ".")
        self.git("commit", "--quiet", "-m", "missing register")
        self.path.write_text(document(), encoding="utf-8")
        self.assertTrue(any("NOT CHECKED" in message for _, message in self.check()[0]))

    def test_deleting_definition_or_renumbering_fails_even_without_references(self):
        self.commit(document(row(), row("RR-F002")))
        self.path.write_text(document(row()), encoding="utf-8")
        self.assertTrue(any("removed or renumbered" in message for _, message in self.check()[0]))

    def test_deleting_all_markers_cannot_escape_git_protection(self):
        self.commit(document())
        self.path.write_text(document().replace(checker.MARKER, ""), encoding="utf-8")
        self.assertTrue(self.check()[0])

    def test_missing_git_or_revision_or_baseline_file_is_failure(self):
        self.path.write_text(document(), encoding="utf-8")
        self.assertTrue(any("NOT CHECKED" in message for _, message in self.check()[0]))
        self.commit(document())
        self.assertTrue(any("NOT CHECKED" in message for _, message in self.check("missing-ref")[0]))
        self.path.unlink()
        self.assertTrue(self.check()[0])

    def test_env_base_ref_is_used_and_explicit_ref_takes_precedence(self):
        first = self.commit(document())
        second = self.commit(document(row(), row("RR-F002")))
        self.path.write_text(document(), encoding="utf-8")
        with base_environment(second):
            self.assertTrue(checker.check_repository(self.root)[0])
            self.assertFalse(checker.check_repository(self.root, first)[0])

    def test_legal_wrapping_and_escaping_do_not_rewrite_protected_fields(self):
        self.commit(document(row(original="a | b".replace("|", r"\|"))))
        self.path.write_text(document(row(original="a  `|`  b")), encoding="utf-8")
        self.assertTrue(self.check()[0])  # Introducing a code span changes the original content.
        self.path.write_text(document(row(original="a   \\|   b")), encoding="utf-8")
        self.assertFalse(self.check()[0])

    def test_responsible_task_must_exist(self):
        self.commit(document())
        self.path.write_text(document(row(responsibility="T-99")), encoding="utf-8")
        self.assertTrue(any("unknown responsible task" in message for _, message in self.check()[0]))

    def test_cli_reports_failure_instead_of_zero_tests_success(self):
        self.commit(document())
        self.path.write_text(document(row(), row()), encoding="utf-8")
        result = subprocess.run([sys.executable, "-B", str(Path(checker.__file__)), "--root", str(self.root), "--base-ref", "HEAD"],
                                capture_output=True, text=True, encoding="utf-8")
        self.assertEqual(result.returncode, 1)
        self.assertIn("duplicate definition", result.stdout)


if __name__ == "__main__":
    unittest.main()
