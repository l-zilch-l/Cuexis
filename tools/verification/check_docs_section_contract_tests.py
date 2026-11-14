"""Regression tests for live Stage 7 / 7.5 ownership, anchors, and registration."""

import importlib.util
import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from urllib.parse import quote


CHECK_DOCS_PATH = Path(__file__).resolve().parents[1] / "check_docs.py"
SPEC = importlib.util.spec_from_file_location("check_docs", CHECK_DOCS_PATH)
assert SPEC is not None and SPEC.loader is not None
check_docs = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(check_docs)

STAGE7_PLAN = "stage-07/plan.md"
STAGE75_PLAN = "stage-07-5/plan.md"
W_CLASS_ANCHOR = "53-w-类缺口记录w-class-gap-record"
STAGE75_PLAN_INDEX = "stage_plans/active/stage-07-5/README.md"
STAGE75_REPORT_INDEX = "stage_reports/stages/stage-07-5/README.md"


class SectionTests(unittest.TestCase):
    def run_fixture(self, target, label="总计划§1.1", source_path="docs/api/test.md"):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            plans = root / "docs/stage_plans/active"
            for name in (
                STAGE7_PLAN, "stage-07/01-a/plan.md", "stage-07/06-b/plan.md", STAGE75_PLAN,
            ):
                plan = plans / name
                plan.parent.mkdir(parents=True, exist_ok=True)
                plan.write_text(
                    "# Plan\n\n## 1.1 权威输入和冻结顺序\n\n"
                    "## 5.3 W 类缺口记录（W-class gap record）\n\n"
                    "## 10. 交接物和后续阶段关系\n",
                    encoding="utf-8",
                )
            source = root / source_path
            source.parent.mkdir(parents=True, exist_ok=True)
            destination, separator, fragment = target.partition("#")
            relative = os.path.relpath(plans / destination, source.parent).replace("\\", "/")
            source.write_text(
                f"# Test\n\n[{label}]({relative}{separator}{fragment})\n", encoding="utf-8"
            )
            failures = []
            check_docs.check_stage7_section_links([source], failures, root)
            return [failure.message for failure in failures]

    def test_original_stage7_sections_keep_owner(self):
        for section, fragment in (("1.1", "11-权威输入和冻结顺序"), ("10", "10-交接物和后续阶段关系")):
            with self.subTest(section=section):
                self.assertEqual([], self.run_fixture(
                    f"{STAGE7_PLAN}#{fragment}", f"总计划§{section}"
                ))

    def test_new_w_class_owner(self):
        self.assertEqual([], self.run_fixture(
            f"{STAGE75_PLAN}#{W_CLASS_ANCHOR}", "Stage 7.5 §5.3"
        ))

    def test_new_w_class_owner_without_section_in_label(self):
        self.assertEqual([], self.run_fixture(f"{STAGE75_PLAN}#{W_CLASS_ANCHOR}", "Stage 7.5计划"))

    def test_old_b_owner_rejected(self):
        messages = self.run_fixture(f"stage-07/06-b/plan.md#{W_CLASS_ANCHOR}", "旧B分册§5.3")
        self.assertEqual(1, len(messages))
        self.assertIn(f"belongs to {STAGE75_PLAN}", messages[0])

    def test_old_b_owner_rejected_from_fragment(self):
        messages = self.run_fixture(f"stage-07/06-b/plan.md#{W_CLASS_ANCHOR}", "Stage 7B+计划")
        self.assertEqual(1, len(messages))
        self.assertIn(f"belongs to {STAGE75_PLAN}", messages[0])

    def test_w_class_rejects_other_stage7_owners(self):
        for plan in (STAGE7_PLAN, "stage-07/01-a/plan.md"):
            with self.subTest(plan=plan):
                messages = self.run_fixture(f"{plan}#{W_CLASS_ANCHOR}", "W 类缺口")
                self.assertEqual(1, len(messages))
                self.assertIn(f"belongs to {STAGE75_PLAN}", messages[0])

    def test_stage7_sections_reject_wrong_owners_in_both_stages(self):
        for plan in ("stage-07/01-a/plan.md", STAGE75_PLAN):
            for section, fragment in (("1.1", "11-权威输入和冻结顺序"), ("10", "10-交接物和后续阶段关系")):
                with self.subTest(plan=plan, section=section):
                    messages = self.run_fixture(f"{plan}#{fragment}", f"总计划§{section}")
                    self.assertEqual(1, len(messages))
                    self.assertIn(f"belongs to {STAGE7_PLAN}", messages[0])

    def test_stage75_plan_can_reference_original_stage7_owner(self):
        self.assertEqual([], self.run_fixture(
            f"{STAGE7_PLAN}#11-权威输入和冻结顺序",
            source_path="docs/stage_plans/active/stage-07-5/admission.md",
        ))

    def test_live_formats_reject_legacy_w_class_owner(self):
        messages = self.run_fixture(
            f"stage-07/06-b/plan.md#{W_CLASS_ANCHOR}",
            "Stage 7B+计划",
            source_path="docs/formats/GAMEPLAY_V2_SPEC.md",
        )
        self.assertEqual(1, len(messages))
        self.assertIn(f"belongs to {STAGE75_PLAN}", messages[0])

    def test_missing_anchor_in_both_stages(self):
        for plan, section in ((STAGE7_PLAN, "1.1"), (STAGE75_PLAN, "5.3")):
            with self.subTest(plan=plan):
                messages = self.run_fixture(f"{plan}#旧标题", f"计划§{section}")
                self.assertEqual(1, len(messages))
                self.assertIn("missing anchor", messages[0])

    def test_encoded_w_class_anchor(self):
        self.assertEqual([], self.run_fixture(
            f"{STAGE75_PLAN}#{quote(W_CLASS_ANCHOR)}", "Stage 7.5计划"
        ))

    def test_dated_historical_snapshot_exempt(self):
        for plan, section in (("stage-07/06-b/plan.md", "5.3"), (STAGE75_PLAN, "1.1")):
            with self.subTest(plan=plan):
                self.assertEqual([], self.run_fixture(
                    f"{plan}#旧标题", f"历史计划§{section}",
                    source_path="docs/stage_reports/stages/stage-07/2026-11-08-review.md",
                ))

    def test_legacy_navigation_entry_without_section_remains_valid(self):
        self.assertEqual([], self.run_fixture("stage-07/06-b/plan.md", "旧路径兼容入口"))

    def test_fences_and_duplicate_slugs(self):
        self.assertEqual({"title", "title-1", "11-冻结顺序"}, check_docs.heading_anchors(
            "# Title\n## Title\n```\n## Ignore\n```\n## 1.1 冻结顺序\n"
        ))


class StageRegistrationTests(unittest.TestCase):
    def write_plans(self, docs, omit=None):
        for relative, statuses in check_docs.STAGE_PLAN_REQUIREMENTS:
            if relative == omit:
                continue
            self.write_plan(docs / relative, statuses[0])

    def write_plan(self, path, status):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(
            f"# Plan\n\n状态：{status}；fixture\n\n"
            "## 阶段目标\n\n## 验收标准\n\n归档来源：fixture\n",
            encoding="utf-8",
        )

    def test_stage75_plan_is_required(self):
        with tempfile.TemporaryDirectory() as temporary:
            docs = Path(temporary) / "docs"
            relative = f"stage_plans/active/{STAGE75_PLAN}"
            self.write_plans(docs, omit=relative)
            failures = []
            # The compatibility wrapper preserves the implementation's function globals.
            with patch.dict(check_docs.check_future_stage_plans.__globals__, {"DOCS": docs}):
                check_docs.check_future_stage_plans(failures)
            self.assertEqual([(docs / relative, "required stage plan is missing")], [
                (failure.path, failure.message) for failure in failures
            ])

    def test_stage7_completed_status_and_stage75_active_status(self):
        for relative, status, expected in (
            (f"stage_plans/active/{STAGE7_PLAN}", "completed", []),
            (f"stage_plans/active/{STAGE7_PLAN}", "active", ["status must be one of: completed"]),
            (f"stage_plans/active/{STAGE75_PLAN}", "active", []),
            (f"stage_plans/active/{STAGE75_PLAN}", "future", ["status must be one of: active"]),
        ):
            with (
                self.subTest(relative=relative, status=status),
                tempfile.TemporaryDirectory() as temporary,
            ):
                docs = Path(temporary) / "docs"
                self.write_plans(docs)
                self.write_plan(docs / relative, status)
                failures = []
                with patch.dict(check_docs.check_future_stage_plans.__globals__, {"DOCS": docs}):
                    check_docs.check_future_stage_plans(failures)
                self.assertEqual(expected, [failure.message for failure in failures])

    def write_navigation(self, docs, omit=None, omit_label=None):
        for relative in check_docs.DIRECTORY_INDEXES:
            if relative == omit:
                continue
            path = docs / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("# Index\n", encoding="utf-8")
        (docs / "stage_plans/README.md").write_text(
            "# Stage plans\n\n" + "\n".join(
                label for label in check_docs.REQUIRED_STAGE_LABELS if label != omit_label
            ),
            encoding="utf-8",
        )
        (docs / "README.md").write_text(
            "# Docs\n\n" + "\n".join(f"[entry]({target})" for target in (
                "PROJECT_GUIDE.md", "architecture/README.md", "guides/README.md",
                "api/README.md", "proposals/README.md", "examples/README.md",
            )),
            encoding="utf-8",
        )
        for name in check_docs.API_REFERENCE_FILES:
            (docs / "api" / name).write_text("# API\n", encoding="utf-8")

    def test_stage75_navigation_indexes_are_required(self):
        for relative in (STAGE75_PLAN_INDEX, STAGE75_REPORT_INDEX):
            with self.subTest(index=relative), tempfile.TemporaryDirectory() as temporary:
                docs = Path(temporary) / "docs"
                self.write_navigation(docs, omit=relative)
                failures = []
                with patch.dict(check_docs.check_navigation_indexes.__globals__, {"DOCS": docs}):
                    check_docs.check_navigation_indexes(failures)
                self.assertEqual([(docs / relative, "required documentation index is missing")], [
                    (failure.path, failure.message) for failure in failures
                ])

    def test_stage75_label_is_required(self):
        with tempfile.TemporaryDirectory() as temporary:
            docs = Path(temporary) / "docs"
            self.write_navigation(docs, omit_label="Stage 7.5")
            failures = []
            with patch.dict(check_docs.check_navigation_indexes.__globals__, {"DOCS": docs}):
                check_docs.check_navigation_indexes(failures)
            self.assertEqual(["stage index is missing Stage 7.5"], [
                failure.message for failure in failures
            ])

    def test_new_and_compatibility_navigation_indexes_are_allowed(self):
        with tempfile.TemporaryDirectory() as temporary:
            docs = Path(temporary) / "docs"
            for relative in (
                STAGE75_PLAN_INDEX, STAGE75_REPORT_INDEX,
                "stage_plans/active/stage-07/06-b/README.md",
                "stage_reports/stages/stage-07/06-b/README.md",
            ):
                path = docs / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("# Index\n", encoding="utf-8")
            failures = []
            with patch.dict(check_docs.check_stage_directory_layout.__globals__, {"DOCS": docs}):
                check_docs.check_stage_directory_layout(failures)
            self.assertEqual([], failures)


if __name__ == "__main__":
    unittest.main()
