"""Focused regression tests for the machine-readable documentation status contract."""

from __future__ import annotations

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


CHECK_DOCS_PATH = Path(__file__).with_name("check_docs.py")
SPEC = importlib.util.spec_from_file_location("check_docs", CHECK_DOCS_PATH)
assert SPEC is not None and SPEC.loader is not None
check_docs = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(check_docs)


class StatusContractTests(unittest.TestCase):
    def write_contract(self, root: Path, stale_fragment: str = "obsolete CFU state") -> Path:
        contract = {
            "snapshotDate": "2026-08-24",
            "requiredFragments": {"docs/status.md": ["current status"]},
            "staleFragments": [{"fragment": stale_fragment, "minLength": 12}],
            "datedFiles": ["docs/status.md"],
        }
        path = root / "docs" / "status_contract.json"
        path.write_text(json.dumps(contract), encoding="utf-8")
        return path

    def test_accepts_whitespace_normalized_current_status(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            docs = root / "docs"
            docs.mkdir()
            (docs / "status.md").write_text(
                "更新日期：2026-08-24\ncurrent\nstatus\n", encoding="utf-8"
            )
            contract = self.write_contract(root)

            failures: list[check_docs.CheckFailure] = []
            check_docs.check_cfu_status(failures, contract, root)

            self.assertEqual([], failures)

    def test_reports_missing_stale_and_dated_contract_failures(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            docs = root / "docs"
            docs.mkdir()
            (docs / "status.md").write_text(
                "更新日期：2026-08-23\nobsolete CFU state\n", encoding="utf-8"
            )
            contract = self.write_contract(root)

            failures: list[check_docs.CheckFailure] = []
            check_docs.check_cfu_status(failures, contract, root)
            messages = [failure.message for failure in failures]

            self.assertIn(
                "status contract requiredFragments: missing current CFU status: current status", messages
            )
            self.assertIn(
                "status contract staleFragments: contains stale CFU status: obsolete CFU state", messages
            )
            self.assertIn(
                "status contract datedFiles: expected 更新日期 on or after 2026-08-24", messages
            )

    def test_rejects_short_stale_fragment(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            docs = root / "docs"
            docs.mkdir()
            (docs / "status.md").write_text("更新日期：2026-08-24\n", encoding="utf-8")
            contract = self.write_contract(root, stale_fragment="short")

            failures: list[check_docs.CheckFailure] = []
            result = check_docs.load_status_contract(contract, root, failures)

            self.assertIsNone(result)
            self.assertEqual(1, len(failures))
            self.assertIn("staleFragments[0]: fragment is shorter than minLength", failures[0].message)


if __name__ == "__main__":
    unittest.main()
