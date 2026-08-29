"""Focused regression tests for BUILDING.md target and SDK API contracts."""

from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path


CHECK_DOCS_PATH = Path(__file__).with_name("check_docs.py")
SPEC = importlib.util.spec_from_file_location("check_docs", CHECK_DOCS_PATH)
assert SPEC is not None and SPEC.loader is not None
check_docs = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(check_docs)


class TargetContractTests(unittest.TestCase):
    def write_building(self, directory: Path, targets: list[str], version: str = "0.7") -> Path:
        path = directory / "BUILDING.md"
        path.write_text(
            "\n".join(
                (
                    "# Building",
                    check_docs.TARGET_BLOCK_BEGIN,
                    "```text",
                    *targets,
                    "```",
                    check_docs.TARGET_BLOCK_END,
                    f"find_package(Cuexis {version} CONFIG REQUIRED COMPONENTS Playback)",
                    "",
                )
            ),
            encoding="utf-8",
        )
        return path

    def write_sdk_version(self, directory: Path, version: str = "0.7.0") -> Path:
        path = directory / "CuexisVersion.cmake"
        path.write_text(f'set(CUEXIS_SDK_API_VERSION "{version}")\n', encoding="utf-8")
        return path

    def test_accepts_generated_targets_and_sdk_minor(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            targets = ["cuexis_core", "cuexis_playback"]
            building = self.write_building(directory, targets)
            facts = directory / "cuexis-targets.txt"
            facts.write_text("\n".join(targets) + "\n", encoding="utf-8")
            version_file = self.write_sdk_version(directory)

            failures: list[check_docs.CheckFailure] = []
            check_docs.check_target_contract(failures, building, facts)
            check_docs.check_sdk_api_contract(failures, building, version_file)

            self.assertEqual([], failures)

    def test_reports_target_order_and_sdk_version_drift(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            building = self.write_building(directory, ["cuexis_playback", "cuexis_core"], version="0.6")
            facts = directory / "cuexis-targets.txt"
            facts.write_text("cuexis_core\ncuexis_playback\n", encoding="utf-8")
            version_file = self.write_sdk_version(directory)

            failures: list[check_docs.CheckFailure] = []
            check_docs.check_target_contract(failures, building, facts)
            check_docs.check_sdk_api_contract(failures, building, version_file)
            messages = [failure.message for failure in failures]

            self.assertIn(
                "target contract: generated target facts differ at line 1: expected cuexis_core, found cuexis_playback",
                messages,
            )
            self.assertIn(
                "SDK API contract: find_package requests 0.6, expected 0.7 from 0.7.0",
                messages,
            )


if __name__ == "__main__":
    unittest.main()
