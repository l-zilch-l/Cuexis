"""Regression tests for live Stage 7 split-plan ownership and heading anchors."""
import tempfile
import unittest
from pathlib import Path
import importlib.util

CHECK_DOCS_PATH = Path(__file__).resolve().parents[1] / "check_docs.py"
SPEC = importlib.util.spec_from_file_location("check_docs", CHECK_DOCS_PATH)
assert SPEC is not None and SPEC.loader is not None
check_docs = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(check_docs)

class SectionTests(unittest.TestCase):
    def run_fixture(self, target, label="总计划§1.1", historical=False):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            stage = root / "docs/stage_plans/active/stage-07"
            stage.mkdir(parents=True)
            for name in ("plan.md", "01-a/plan.md", "06-b/plan.md"):
                (stage / name).parent.mkdir(parents=True, exist_ok=True)
                (stage / name).write_text("# Plan\n\n## 1.1 冻结顺序\n\n## 5.3 后续能力\n", encoding="utf-8")
            source = root / ("docs/stage_reports/dated.md" if historical else "docs/api/test.md")
            source.parent.mkdir(parents=True)
            source.write_text(f"# Test\n\n[{label}](../stage_plans/active/stage-07/{target})\n", encoding="utf-8")
            failures = []
            check_docs.check_stage7_section_links([source], failures, root)
            return [f.message for f in failures]

    def test_valid_live_anchor(self):
        self.assertEqual([], self.run_fixture("plan.md#11-冻结顺序"))
    def test_wrong_existing_owner(self):
        self.assertIn("belongs to plan.md", self.run_fixture("01-a/plan.md#11-冻结顺序")[0])
    def test_nested_b_plan_owner(self):
        self.assertEqual([], self.run_fixture("06-b/plan.md#53-后续能力", label="B分册§5.3"))
    def test_nested_b_rejects_a_owner(self):
        self.assertIn("belongs to 06-b/plan.md", self.run_fixture("01-a/plan.md#53-后续能力", label="B分册§5.3")[0])
    def test_missing_anchor(self):
        self.assertIn("missing anchor", self.run_fixture("plan.md#11-旧标题")[0])
    def test_historical_snapshot_exempt(self):
        self.assertEqual([], self.run_fixture("01-a/plan.md#11-旧标题", historical=True))
    def test_fences_and_duplicate_slugs(self):
        self.assertEqual({"title", "title-1", "11-冻结顺序"}, check_docs.heading_anchors(
            "# Title\n## Title\n```\n## Ignore\n```\n## 1.1 冻结顺序\n"))

if __name__ == "__main__":
    unittest.main()
