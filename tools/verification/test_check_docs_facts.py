"""Regression tests for version claims, historical scope and missing target facts."""

from importlib.util import module_from_spec, spec_from_file_location
from pathlib import Path
import os
import tempfile
import unittest
from unittest.mock import patch

HERE = Path(__file__).resolve().parent


def load(name, path):
    spec = spec_from_file_location(name, path)
    module = module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


facts = load("facts_for_test", HERE / "check_docs_facts.py")
docs = load("docs_for_facts_test", HERE / "check_docs.py")


class FactsTests(unittest.TestCase):
    def scan(self, line):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / "docs/guides/live.md"
            path.parent.mkdir(parents=True)
            path.write_text("# Live\n" + line + "\n", encoding="utf-8")
            with patch.object(facts, "ROOT", root):
                return facts.scan([path], "1.2.3", "26.11.23-1")

    def test_current_sdk_drift_fails_even_with_old_word(self):
        self.assertEqual(1, len(self.scan("当前 SDK API `0.9.1`，保留旧入口。")))

    def test_old_clause_cannot_exempt_current_clause(self):
        self.assertEqual(1, len(self.scan("历史 SDK API `0.9.0`；当前 SDK API `0.9.1`。")))

    def test_generic_baseline_or_archive_word_cannot_exempt_current_claim(self):
        self.assertEqual(1, len(self.scan("当前版本兼容基线为 SDK API `0.9.0`。")))
        self.assertEqual(1, len(self.scan("当前归档接口为 SDK API `0.9.0`。")))

    def test_prototype_evidence_is_not_a_current_official_version_claim(self):
        self.assertEqual([], self.scan("原型实现使用 SDK API `0.9.0`；未声明正式支持。"))

    def test_experiment_word_cannot_hide_current_official_version(self):
        self.assertEqual(1, len(self.scan("实验证据声称当前正式 SDK API `0.9.0`。")))

    def test_matching_current_claim(self):
        self.assertEqual([], self.scan("当前 SDK API `1.2.3`。"))

    def test_explicit_history_is_allowed(self):
        self.assertEqual([], self.scan("历史 SDK API `0.9.1`；此前构建 `26.11.22-1`。"))

    def test_suffix_history_is_allowed(self):
        self.assertEqual([], self.scan("`26.11.22-1`（历史）"))

    def test_history_suffix_cannot_override_current_claim(self):
        self.assertEqual(1, len(self.scan("当前 SDK API `0.9.0`（历史）")))

    def test_example_suffix_cannot_override_current_claim(self):
        for suffix in ("（格式示例）", "（示例）", "(historical)"):
            with self.subTest(suffix=suffix):
                self.assertEqual(1, len(self.scan("现行 SDK API `0.9.0`" + suffix)))

    def test_fenced_examples_are_not_current_claims(self):
        self.assertEqual([], self.scan("```text\nSDK API `0.0.1`\n```"))
        self.assertEqual([], self.scan("~~~text\nSDK API `0.0.1`\n~~~"))

    def test_fenced_historical_status_cannot_hide_current_claim(self):
        text = "```text\nstatus: historical\n```\n<!-- cuexis-meta\nid: LIVE\nstatus: active\nauthority: owns\n-->\n当前 SDK API `0.0.1`"
        self.assertEqual(1, len(self.scan(text)))

    def test_missing_facts_are_explicit_and_strict_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            page = root / "BUILDING.md"
            page.write_text("\n".join(("# Build", docs.TARGET_BLOCK_BEGIN,
                            "```text", "cuexis_core", "```", docs.TARGET_BLOCK_END)), encoding="utf-8")
            with patch.dict(os.environ, {}, clear=True), patch.dict(
                docs.generated_target_facts.__globals__, {"DEFAULT_TARGET_FACTS": root / "missing"}
            ):
                errors = []
                docs.check_target_contract(errors, page)
                self.assertEqual([], errors)
                os.environ["CUEXIS_DOCS_STRICT"] = "1"
                docs.check_target_contract(errors, page)
                self.assertEqual(1, len(errors))
                self.assertIn("requires generated target facts", errors[0].message)


if __name__ == "__main__":
    unittest.main()
