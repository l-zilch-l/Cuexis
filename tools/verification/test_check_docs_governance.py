"""Temporary-repository positive/negative tests for structural governance."""

from __future__ import annotations

import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch


SPEC = importlib.util.spec_from_file_location("governance", Path(__file__).with_name("check_docs_governance.py"))
assert SPEC is not None and SPEC.loader is not None
G = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(G)


def document(identity: str, status: str = "active", authority: str = "owns", extra: str = "") -> str:
    return (f"# Document\n\n<!-- cuexis-meta\nid: {identity}\nstatus: {status}\n"
            f"authority: {authority}\n{extra}-->\n\n## Body\n\nOriginal content.\n")


class GovernanceTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.rules = {"version": 1, "required_topics": [], "legacy_names": {},
                      "legacy_report_layout": {}, "legacy_index_authority": {}}
        self.write_rules()
        self.write("docs/contract.md", document("CONTRACT"))
        self.git("init", "-q")
        self.commit()

    def write(self, relative: str, text: str) -> Path:
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8", newline="\n")
        return path

    def write_rules(self) -> None:
        self.write("docs/governance_rules.json", json.dumps(self.rules))

    def git(self, *arguments: str) -> str:
        result = subprocess.run(["git", "-C", str(self.root), "-c", "user.name=Governance test",
                                 "-c", "user.email=governance@example.invalid", *arguments],
                                capture_output=True, text=True)
        self.assertEqual(0, result.returncode, result.stderr)
        return result.stdout.strip()

    def commit(self) -> None:
        self.git("add", ".")
        self.git("commit", "-qm", "fixture")

    def check(self, base: str = "HEAD") -> tuple[list[str], list[str]]:
        errors, notices = G.check_repository(self.root, base)
        return [message for _, message in errors], notices

    def assert_error(self, fragment: str) -> None:
        errors, _ = self.check()
        self.assertTrue(any(fragment in error for error in errors), errors)

    def assert_clean(self) -> list[str]:
        errors, notices = self.check()
        self.assertEqual([], errors)
        return notices

    def topic_baseline(self) -> None:
        self.write("docs/contract.md", document("CONTRACT", extra="topics: chart-v5.render-alpha\n"))
        self.commit()

    def test_regular_document_and_uncovered_notice(self) -> None:
        notices = self.assert_clean()
        self.assertTrue(any("not covered" in notice for notice in notices))

    def test_import_without_sys_modules_and_map_uses_same_parser(self) -> None:
        self.assertEqual("CONTRACT", G._MAP.parse_meta(document("CONTRACT"))["id"])

    def test_new_uppercase_name_rejected(self) -> None:
        self.write("docs/NewContract.md", document("UPPER"))
        self.assert_error("lowercase kebab-case")

    def test_uppercase_extension_cannot_hide_markdown(self) -> None:
        self.write("docs/new-contract.MD", document("UPPER-EXT"))
        self.assert_error("lowercase kebab-case")

    def test_new_file_cannot_add_its_own_legacy_exception(self) -> None:
        self.write("docs/NewFile.md", document("NEW"))
        self.rules["legacy_names"]["docs/NewFile.md"] = "pretend this new file is old"
        self.write_rules()
        self.assert_error("new files cannot acquire legacy exemptions")

    def test_exact_name_exception_does_not_cover_copy(self) -> None:
        self.write("docs/OldSpec.md", document("OLD"))
        self.commit()
        self.rules["legacy_names"]["docs/OldSpec.md"] = "existing stable spec"
        self.write_rules()
        self.assert_clean()
        self.write("docs/OtherSpec.md", document("COPY"))
        self.assert_error("lowercase kebab-case")

    def test_glob_or_directory_exception_rejected(self) -> None:
        for path in ("docs/old/*", "docs/old/", "../outside.md"):
            with self.subTest(path=path):
                self.rules["legacy_names"] = {path: "bad exemption"}
                self.write_rules()
                self.assert_error("invalid exact exception")

    def test_task_special_case_and_reject_wrong_case(self) -> None:
        self.write("docs/stage_plans/active/sample/tasks/T-03.md", document("TASK"))
        self.assert_clean()
        self.write("docs/stage_plans/active/sample/tasks/T-foo.md", document("BAD-TASK"))
        self.assert_error("lowercase kebab-case")

    def test_adr_number_unique(self) -> None:
        self.write("docs/adr/0001-first.md", document("ADR-1", "accepted"))
        self.assert_clean()
        self.write("docs/adr/0001-other.md", document("ADR-OTHER", "accepted"))
        self.assert_error("duplicate ADR number 0001")

    def test_adr_shape_not_general_kebab(self) -> None:
        self.write("docs/adr/no-number.md", document("ADR-BAD"))
        self.assert_error("NNNN-kebab-title")

    def test_valid_report_calendar_and_layout(self) -> None:
        self.write("docs/stage_reports/stages/sample/verification/2024-02-29-t-03-check.md",
                   document("REPORT", "evidence", "none"))
        self.assert_clean()

    def test_invalid_calendar_even_with_name_exception(self) -> None:
        relative = "docs/stage_reports/stages/sample/verification/2025-02-29-check.md"
        self.write(relative, document("REPORT", "evidence", "none"))
        self.rules["legacy_names"][relative] = "not a calendar bypass"
        self.write_rules()
        self.assert_error("real calendar date")

    def test_flat_report_exact_legacy_layout_does_not_cover_new_report(self) -> None:
        old = "docs/stage_reports/stages/old/2026-01-01-old.md"
        self.write(old, document("OLD-REPORT", "evidence", "none"))
        self.commit()
        self.rules["legacy_report_layout"][old] = "existing closed stage"
        self.write_rules()
        self.assert_clean()
        self.write("docs/stage_reports/stages/old/2026-01-02-new.md", document("NEW-REPORT", "evidence", "none"))
        self.assert_error("legacy exceptions are exact")

    def test_misc_report_directory_rejected(self) -> None:
        self.write("docs/stage_reports/stages/new/misc/2026-01-01-check.md", document("MISC", "evidence", "none"))
        self.assert_error("report must be in")

    def test_report_non_date_name_needs_separate_exact_exception(self) -> None:
        path = "docs/stage_reports/stages/old/verification/completion.md"
        self.write(path, document("COMPLETE", "evidence", "none"))
        self.assert_error("YYYY-MM-DD")
        self.commit()
        self.rules["legacy_names"][path] = "original completion entry"
        self.write_rules()
        self.assert_clean()

    def test_report_cannot_claim_owns(self) -> None:
        self.write("docs/stage_reports/stages/sample/verification/2026-01-01-check.md",
                   document("REPORT", "evidence", "owns"))
        self.assert_error("report must declare evidence/none")

    def test_fenced_and_body_metadata_not_registered(self) -> None:
        text = document("REAL") + "\n```markdown\n" + document("FAKE") + "```\n" + document("BODY-EXAMPLE")
        self.write("docs/contract.md", text)
        self.assertEqual("REAL", G._MAP.parse_meta(text)["id"])
        self.assert_clean()

    def test_fenced_only_metadata_is_missing(self) -> None:
        text = "# Example\n\n~~~markdown\n" + document("FAKE") + "~~~\n"
        self.write("docs/contract.md", text)
        self.assertEqual({}, G._MAP.parse_meta(text))
        self.assert_error("found 0")

    def test_header_provenance_before_real_metadata(self) -> None:
        text = document("REAL").replace("<!-- cuexis-meta", "适用版本：SDK API `0.9.1`\n\n文档角色：参考\n\nShort provenance.\n\n<!-- cuexis-meta")
        self.write("docs/contract.md", text)
        self.assert_clean()

    def test_two_real_header_blocks_fail(self) -> None:
        text = document("ONE").replace("## Body", "<!-- cuexis-meta\nid: TWO\nstatus: active\nauthority: owns\n-->\n\n## Body")
        self.write("docs/contract.md", text)
        self.assert_error("found 2")
        self.assertEqual({}, G._MAP.parse_meta(text))

    def test_duplicate_metadata_key_rejected_by_both_parsers(self) -> None:
        text = document("ONE", extra="id: TWO\n")
        self.write("docs/contract.md", text)
        self.assert_error("duplicate metadata key: id")
        self.assertEqual({}, G._MAP.parse_meta(text))

    def test_real_ids_global_unique(self) -> None:
        self.write("docs/other.md", document("CONTRACT"))
        self.assert_error("duplicate metadata id")

    def test_unknown_status_and_authority(self) -> None:
        self.write("docs/contract.md", document("CONTRACT", "done", "master"))
        self.assert_error("unknown metadata status or authority")

    def test_index_authority_and_legacy_none_are_precise(self) -> None:
        relative = "docs/old/README.md"
        self.write(relative, document("OLD-INDEX", "candidate", "none"))
        self.assert_error("index must use")
        self.commit()
        self.rules["legacy_index_authority"][relative] = "original candidate routing"
        self.write_rules()
        self.assert_clean()
        self.write(relative, document("OLD-INDEX", "active", "owns"))
        self.assert_error("index must use")

    def test_existing_index_cannot_downgrade_with_new_legacy_exception(self) -> None:
        relative = "docs/old/README.md"
        self.write(relative, document("OLD-INDEX", authority="index"))
        self.commit()
        self.write(relative, document("OLD-INDEX", authority="none"))
        self.rules["legacy_index_authority"][relative] = "same-change downgrade"
        self.write_rules()
        self.assert_error("trusted Git base must already declare authority: none")

    def test_fenced_none_does_not_qualify_for_legacy_index_exception(self) -> None:
        relative = "docs/old/README.md"
        self.write(relative, "# Index\n\n```\n<!-- cuexis-meta\nid: FAKE\nstatus: active\nauthority: none\n-->\n```\n"
                   + document("OLD-INDEX", authority="index"))
        self.commit()
        self.write(relative, document("OLD-INDEX", authority="none"))
        self.rules["legacy_index_authority"][relative] = "fenced example is not old authority"
        self.write_rules()
        self.assert_error("trusted Git base must already declare authority: none")

    def test_template_name_checked_before_metadata_exclusion(self) -> None:
        self.write("docs/templates/BadTemplate.MD", document("CONTRACT"))
        self.write("docs/templates/README.md", document("TEMPLATE-INDEX", authority="index")
                   + "\n[BadTemplate.MD](BadTemplate.MD)\n")
        self.assert_error("template name must be lowercase kebab-case.md")

    def test_template_extension_must_be_lowercase(self) -> None:
        self.write("docs/templates/new-template.MD", "# Template\n")
        self.write("docs/templates/README.md", document("TEMPLATE-INDEX", authority="index")
                   + "\n[new](new-template.MD)\n")
        self.assert_error("template name must be lowercase kebab-case.md")

    def test_linked_kebab_template_needs_no_real_metadata(self) -> None:
        self.write("docs/templates/new-template.md", "# Template without actual metadata\n")
        self.write("docs/templates/README.md", document("TEMPLATE-INDEX", authority="index")
                   + "\n[new](new-template.md)\n")
        self.assert_clean()

    def test_template_reference_link_is_real_registration(self) -> None:
        self.write("docs/templates/new-template.md", "# Template\n")
        self.write("docs/templates/README.md", document("TEMPLATE-INDEX", authority="index")
                   + "\n[new][template]\n\n[template]: new-template.md\n")
        self.assert_clean()

    def test_unused_template_link_definition_does_not_register(self) -> None:
        self.write("docs/templates/new-template.md", "# Template\n")
        self.write("docs/templates/README.md", document("TEMPLATE-INDEX", authority="index")
                   + "\n[template]: new-template.md\n")
        self.assert_error("template must be linked from docs/templates/README.md")

    def test_new_template_requires_index_link(self) -> None:
        self.write("docs/templates/new-template.md", document("CONTRACT"))
        self.write("docs/templates/README.md", document("TEMPLATE-INDEX", authority="index")
                   + "\nTemplate name: new-template.md\n")
        self.assert_error("template must be linked from docs/templates/README.md")

    def test_fake_template_links_do_not_register_template(self) -> None:
        self.write("docs/templates/new-template.md", document("CONTRACT"))
        for fake_link in ("```md\n[new](new-template.md)\n```", "<!-- [new](new-template.md) -->",
                          "`[new](new-template.md)`", "![new](new-template.md)"):
            with self.subTest(fake_link=fake_link):
                self.write("docs/templates/README.md", document("TEMPLATE-INDEX", authority="index")
                           + "\n" + fake_link + "\n")
                self.assert_error("template must be linked from docs/templates/README.md")

    def test_template_handoff_id_and_raw_evidence_are_excluded(self) -> None:
        self.write("docs/templates/handoff.md", document("CONTRACT"))
        self.write("docs/templates/README.md", document("TEMPLATE-INDEX", authority="index")
                   + "\n[Handoff](handoff.md)\n")
        self.write("docs/stage_reports/stages/sample/evidence/Original_File.md", "raw attachment")
        self.assert_clean()
        with patch.object(G._MAP, "DOCS", self.root / "docs"):
            paths = {row["rel"] for row in G._MAP.collect()}
        self.assertNotIn("templates/handoff.md", paths)
        self.assertNotIn("stage_reports/stages/sample/evidence/Original_File.md", paths)

    def test_template_readme_is_real_index(self) -> None:
        self.write("docs/templates/README.md", "# Index\n")
        self.assert_error("found 0")

    def test_evidence_directory_outside_reports_cannot_hide_contract(self) -> None:
        relative = "docs/formats/evidence/new-contract.md"
        self.write(relative, "# Contract without real metadata\n")
        self.assert_error("found 0")
        with patch.object(G._MAP, "DOCS", self.root / "docs"):
            paths = {row["rel"] for row in G._MAP.collect()}
        self.assertIn("formats/evidence/new-contract.md", paths)

    def test_required_initial_topic_cannot_disappear_before_first_commit(self) -> None:
        self.rules["required_topics"] = ["chart-v5.render-alpha"]
        self.write_rules()
        self.assert_error("required topic chart-v5.render-alpha")
        self.write("docs/contract.md", document("CONTRACT", extra="topics: chart-v5.render-alpha\n"))
        self.assert_clean()

    def test_comma_separated_and_inline_topic_lists(self) -> None:
        for value in ('a.v1, b.v2', '["a.v1", "b.v2"]', '[a.v1, b.v2]'):
            with self.subTest(value=value):
                self.write("docs/contract.md", document("CONTRACT", extra=f"topics: {value}\n"))
                notices = self.assert_clean()
                self.assertTrue(any("topics covered: 2" in message for message in notices))

    def test_topic_duplicate_owner_rejected(self) -> None:
        self.topic_baseline()
        self.write("docs/second.md", document("SECOND", extra="topics: chart-v5.render-alpha\n"))
        self.assert_error("multiple active owners")

    def test_duplicate_topic_within_one_owner_rejected(self) -> None:
        self.write("docs/contract.md", document("CONTRACT", extra="topics: a.v1, a.v1\n"))
        self.assert_error("duplicate topic key")

    def test_delete_topics_cannot_disable_protection(self) -> None:
        self.topic_baseline()
        self.write("docs/contract.md", document("CONTRACT"))
        self.assert_error("lost its active owner")

    def test_rename_topics_cannot_disable_protection(self) -> None:
        self.topic_baseline()
        self.write("docs/contract.md", document("CONTRACT", extra="topics: chart-v6.render-alpha\n"))
        self.assert_error("lost its active owner")

    def test_change_authority_or_status_cannot_disable_protection(self) -> None:
        self.topic_baseline()
        for status, authority in (("active", "references"), ("historical", "owns"), ("future", "owns")):
            with self.subTest(status=status, authority=authority):
                self.write("docs/contract.md", document("CONTRACT", status, authority, "topics: chart-v5.render-alpha\n"))
                self.assert_error("lost its active owner")

    def test_deleting_entire_document_still_protected(self) -> None:
        self.topic_baseline()
        (self.root / "docs/contract.md").unlink()
        self.assert_error("lost its active owner")

    def test_legal_topic_transfer_requires_named_source_and_reason(self) -> None:
        self.topic_baseline()
        self.write("docs/contract.md", document("CONTRACT", authority="references"))
        self.write("docs/new-owner.md", document("NEW", extra="topics: chart-v5.render-alpha\n"))
        self.assert_error("requires topic_transfer_from")
        self.write("docs/new-owner.md", document("NEW", extra=("topics: chart-v5.render-alpha\n"
                   "topic_transfer_from: CONTRACT\ntopic_transfer_reason: Accepted ADR-0001 consolidates the v5 contract.\n")))
        self.assert_clean()

    def test_document_move_keeps_id_and_needs_reason(self) -> None:
        self.topic_baseline()
        (self.root / "docs/contract.md").rename(self.root / "docs/moved.md")
        self.assert_error("requires topic_transfer_from")
        self.write("docs/moved.md", document("CONTRACT", extra=("topics: chart-v5.render-alpha\n"
                   "topic_transfer_from: CONTRACT\ntopic_transfer_reason: Accepted G04 navigation migration.\n")))
        self.assert_clean()

    def test_trusted_older_base_not_just_head(self) -> None:
        self.topic_baseline()
        trusted = self.git("rev-parse", "HEAD")
        self.write("docs/contract.md", document("CONTRACT"))
        self.commit()
        errors, _ = self.check(trusted)
        self.assertTrue(any("lost its active owner" in message for message in errors))

    def test_base_environment_is_used(self) -> None:
        # Replace the Python lookup, not the process environment: restoring empty
        # Git config values via putenv is lossy on Windows.
        with patch.object(G.os, "environ", {**os.environ, "CUEXIS_DOCS_BASE_REF": "missing-trusted-base"}):
            errors, _ = G.check_repository(self.root)
        self.assertTrue(any("NOT CHECKED" in message for _, message in errors))

    def test_missing_git_and_missing_base_fail_visibly(self) -> None:
        with patch.object(G.subprocess, "run", side_effect=FileNotFoundError("git missing")):
            errors, notices = G.check_repository(self.root)
        self.assertTrue(any("NOT CHECKED" in message for _, message in errors))
        self.assertTrue(any("not checked" in message for message in notices))
        errors, _ = self.check("does-not-exist")
        self.assertTrue(any("NOT CHECKED" in message for message in errors))

    def test_duplicate_large_body_and_table_are_notice_only(self) -> None:
        prose = "Explicit contract text and constraints. " * 20
        table = "| Key | Rule |\n| --- | --- |\n" + "\n".join(f"| row-{i} | value-{i} |" for i in range(6))
        self.write("docs/contract.md", document("CONTRACT") + prose + "\n\n" + table)
        self.write("docs/summary.md", document("SUMMARY", authority="summarizes") + prose + "\n\n" + table)
        notices = self.assert_clean()
        self.assertEqual(1, sum("duplicate-content suspicion" in message for message in notices))

    def test_reports_generated_views_and_code_not_duplicate_suspects(self) -> None:
        prose = "Repeated evidence snapshot text. " * 25
        self.write("docs/contract.md", document("CONTRACT") + prose)
        self.write("docs/generated.md", document("GENERATED", authority="references") + "由 tools/generate.py 生成\n\n" + prose)
        self.write("docs/code.md", document("CODE") + "\n```\n" + prose + "\n```\n")
        self.write("docs/stage_reports/stages/sample/verification/2026-01-01-check.md", document("REPORT", "evidence", "none") + prose)
        notices = self.assert_clean()
        self.assertFalse(any("duplicate-content suspicion" in message for message in notices))


if __name__ == "__main__":
    unittest.main()
