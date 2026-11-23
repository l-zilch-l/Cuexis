"""Focused regression tests for the machine-readable documentation status contract."""

from __future__ import annotations

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


CHECK_DOCS_PATH = Path(__file__).resolve().parents[1] / "check_docs.py"
SPEC = importlib.util.spec_from_file_location("check_docs", CHECK_DOCS_PATH)
assert SPEC is not None and SPEC.loader is not None
check_docs = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(check_docs)


class StatusContractTests(unittest.TestCase):
    def live_fixture(self, root: Path):
        stage = root / "docs/stage_plans/active/stage-x"
        stage.mkdir(parents=True)
        (stage / "plan.md").write_text("# Stage X Plan\n", encoding="utf-8")
        (stage / "findings.md").write_text("# Findings\n", encoding="utf-8")
        status = root / "docs/CURRENT_STATUS.md"
        status.write_text("\n".join((
            "# Current", "<!-- cuexis-meta", "id: STATUS", "status: current", "authority: owns",
            "current_stage: Stage X", "current_item: stage_plans/active/stage-x/plan.md",
            "current_register: stage_plans/active/stage-x/findings.md", "-->",
            "更新日期：2026-11-23", "## 当前任务", "Working", "## 阶段状态", "Active",
            "## 当前遗留问题与生产关闭前置", "Tracked", "",
        )), encoding="utf-8")
        contract = root / "docs/status_contract.json"
        contract.write_text(json.dumps({"schemaVersion": 2, "currentStatus": "docs/CURRENT_STATUS.md"}),
                            encoding="utf-8")
        return status, contract

    def test_live_status_needs_no_historical_fragments(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            _, contract = self.live_fixture(root)
            errors = []
            check_docs.check_cfu_status(errors, contract, root)
            self.assertEqual([], errors)

    def test_fenced_metadata_example_does_not_override_real_header(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            status, contract = self.live_fixture(root)
            text = status.read_text(encoding="utf-8")
            status.write_text(text.replace("# Current\n", "# Current\n```\n<!-- cuexis-meta\nstatus: historical\n-->\n```\n"),
                              encoding="utf-8")
            errors = []
            check_docs.check_cfu_status(errors, contract, root)
            self.assertEqual([], errors)

    def test_live_status_rejects_missing_or_wrong_stage_pointer(self):
        for before, after, message in (
            ("current_stage: Stage X", "current_stage: Stage Wrong", "match the selected plan"),
            ("stage-x/findings.md", "stage-x/missing.md", "referenced file is missing"),
            ("current_item: stage_plans/active/stage-x/plan.md", "current_item: /etc/passwd", "relative docs path"),
            ("2026-11-23", "2026-02-30", "calendar date"),
        ):
            with self.subTest(after=after), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                status, contract = self.live_fixture(root)
                status.write_text(status.read_text(encoding="utf-8").replace(before, after), encoding="utf-8")
                errors = []
                check_docs.check_cfu_status(errors, contract, root)
                self.assertTrue(any(message in error.message for error in errors), [e.message for e in errors])

    def test_foundation_hardening_close_promotes_stage_six(self) -> None:
        requirements = dict(check_docs.STAGE_PLAN_REQUIREMENTS)
        self.assertEqual(
            ("completed",),
            requirements["stage_plans/completed/chart-format-foundation/plan.md"],
        )
        self.assertEqual(
            ("completed",),
            requirements["stage_plans/completed/chart-format-foundation-hardening/plan.md"],
        )
        self.assertEqual(("completed",), requirements["stage_plans/completed/stage-06/plan.md"])
        self.assertNotIn("docs/ROADMAP.md", requirements)
        self.assertEqual(
            ("active",),
            requirements["stage_plans/active/chart-format-update-for-v5/plan.md"],
        )
        self.assertNotIn("stage_plans/active/stage-06/plan.md", requirements)
        self.assertNotIn("stage_plans/active/chart-format-foundation/plan.md", requirements)
        self.assertNotIn(
            "stage_plans/active/chart-format-foundation-hardening/plan.md", requirements
        )
        self.assertNotIn("stage_plans/future/stage-06/plan.md", requirements)

    def test_rejects_legacy_and_malformed_contract(self):
        for content in ('{"snapshotDate":"2026-09-01","requiredFragments":{}}',
                        '{"schemaVersion":3}', '[]', '{broken'):
            with self.subTest(content=content), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                _, contract = self.live_fixture(root)
                contract.write_text(content, encoding="utf-8")
                errors = []
                check_docs.check_cfu_status(errors, contract, root)
                self.assertEqual(1, len(errors))

    def test_rejects_status_contract_path_escape(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            _, contract = self.live_fixture(root)
            contract.write_text(json.dumps({"schemaVersion":2,"currentStatus":"../outside.md"}),
                                encoding="utf-8")
            errors = []
            check_docs.check_cfu_status(errors, contract, root)
            self.assertTrue(any("stable docs/CURRENT_STATUS.md" in item.message for item in errors))

    def test_another_valid_current_document_cannot_replace_status(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            status, contract = self.live_fixture(root)
            other = root / "docs/not-the-status.md"
            other.write_text(status.read_text(encoding="utf-8").replace("id: STATUS", "id: OTHER"), encoding="utf-8")
            contract.write_text(json.dumps({"schemaVersion":2,"currentStatus":"docs/not-the-status.md"}), encoding="utf-8")
            errors = []
            check_docs.check_cfu_status(errors, contract, root)
            self.assertEqual(1, len(errors))
            self.assertIn("stable docs/CURRENT_STATUS.md", errors[0].message)

    def test_stable_path_requires_stable_status_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            status, contract = self.live_fixture(root)
            status.write_text(status.read_text(encoding="utf-8").replace("id: STATUS", "id: OTHER"), encoding="utf-8")
            errors = []
            check_docs.check_cfu_status(errors, contract, root)
            self.assertTrue(any("stable id STATUS" in item.message for item in errors))


if __name__ == "__main__":
    unittest.main()
