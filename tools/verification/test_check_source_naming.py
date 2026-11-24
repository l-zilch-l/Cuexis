"""Temporary-tree positive/negative tests for the source naming checker."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest


SPEC = importlib.util.spec_from_file_location(
    "source_naming", Path(__file__).with_name("check_source_naming.py")
)
assert SPEC is not None and SPEC.loader is not None
CS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CS)


class SourceNamingTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.write_rules({})

    def write(self, relative: str, text: str = "// fixture\n") -> Path:
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8", newline="\n")
        return path

    def write_rules(self, exceptions: dict[str, str], extra: dict[str, object] | None = None) -> None:
        rules: dict[str, object] = {
            "version": 1,
            "source_naming_exceptions": exceptions,
        }
        if extra:
            rules.update(extra)
        self.write("docs/governance_rules.json", json.dumps(rules, ensure_ascii=False))

    def check(self) -> tuple[list[tuple[Path, str]], list[str]]:
        return CS.check_repository(self.root)

    def messages(self) -> list[str]:
        errors, _ = self.check()
        return [message for _, message in errors]

    def test_clean_tree_passes_and_counts_scanned_segments(self) -> None:
        for relative in (
            "engine/core/src/error.cpp",
            "engine/core/src/cxt_v2_animation_adapter.cpp",
            "engine/core/src/playback_session_prepare_stages.cpp",
            "engine/core/src/gameplay_packed_read.cpp",
            "engine/include/cuexis/version.hpp.in",
            "app/player/main.cpp",
            "cmake/CuexisConfig.cmake.in",
        ):
            self.write(relative)
        errors, notices = self.check()
        self.assertEqual([], errors)
        self.assertEqual(1, len(notices))
        self.assertTrue(notices[0].startswith("source naming: 扫描 "), notices[0])
        self.assertIn("0 个遗留例外", notices[0])

    def test_stage_and_task_tokens_are_rejected(self) -> None:
        for relative in (
            "cmake/VerifyRrFoo.cmake",
            "cmake/VerifyStage7Gate.cmake",
            "engine/core/src/t01_helper.cpp",
            "engine/core/src/rr2_stage.cpp",
        ):
            self.write(relative)
        errors, _ = self.check()
        by_path = {path.relative_to(self.root).as_posix(): message for path, message in errors}
        expected = {
            "cmake/VerifyRrFoo.cmake": "rr",
            "cmake/VerifyStage7Gate.cmake": "stage7",
            "engine/core/src/t01_helper.cpp": "t01",
            "engine/core/src/rr2_stage.cpp": "rr2",
        }
        self.assertEqual(sorted(expected), sorted(by_path))
        for relative, token in expected.items():
            with self.subTest(relative=relative):
                message = by_path[relative]
                self.assertIn(relative, message)
                self.assertIn(token, message.lower())
                self.assertIn(
                    "改成领域名，或在 docs/governance_rules.json 记录遗留原因", message
                )

    def test_directory_names_are_scanned(self) -> None:
        self.write("engine/rr_support/payload.cpp")
        errors, _ = self.check()
        self.assertEqual(1, len(errors))
        path, message = errors[0]
        self.assertEqual("engine/rr_support", path.relative_to(self.root).as_posix())
        self.assertIn("rr_support", message)

    def test_tests_tools_and_other_roots_are_exempt(self) -> None:
        for relative in (
            "tests/rr_thing_tests.cpp",
            "tools/verification/check_stage6_a2.py",
            "docs/stage7/notes.md",
            "examples/rr_demo.cpp",
            "third_party/pd_support/payload.c",
        ):
            self.write(relative)
        errors, notices = self.check()
        self.assertEqual([], errors)
        self.assertIn("0 个遗留例外", notices[0])

    def test_exact_legacy_exception_is_a_notice_not_an_error(self) -> None:
        legacy = "cmake/VerifyCfuF3Determinism.cmake"
        self.write(legacy)
        self.write_rules({legacy: "frozen evidence binds this path and its content hash"})
        errors, notices = self.check()
        self.assertEqual([], errors)
        self.assertEqual(1, len(notices))
        self.assertIn("1 个遗留例外", notices[0])
        self.assertIn(legacy, notices[0])

        self.write("cmake/VerifyCfuF4Performance.cmake")
        errors, _ = self.check()
        self.assertEqual(1, len(errors))
        self.assertIn("VerifyCfuF4Performance.cmake", errors[0][0].name)

    def test_exception_with_empty_reason_is_an_error(self) -> None:
        legacy = "cmake/VerifyCfuF3Determinism.cmake"
        self.write(legacy)
        self.write_rules({legacy: "   "})
        messages = self.messages()
        self.assertTrue(messages)
        self.assertTrue(any("reason is empty" in message and legacy in message for message in messages), messages)

    def test_missing_governance_rules_fails_closed(self) -> None:
        self.write("cmake/VerifyCfuF3Determinism.cmake")
        (self.root / "docs/governance_rules.json").unlink()
        messages = self.messages()
        self.assertTrue(messages)
        self.assertTrue(
            any("governance_rules.json" in message and "fail closed" in message for message in messages),
            messages,
        )

    def test_unparsable_governance_rules_fails_closed(self) -> None:
        self.write("cmake/VerifyCfuF3Determinism.cmake")
        self.write("docs/governance_rules.json", "{not json")
        messages = self.messages()
        self.assertTrue(any("cannot parse" in message and "fail closed" in message for message in messages), messages)

    def test_missing_exception_table_fails_closed(self) -> None:
        self.write("cmake/VerifyCfuF3Determinism.cmake")
        self.write("docs/governance_rules.json", json.dumps({"version": 1}))
        messages = self.messages()
        self.assertTrue(any("source_naming_exceptions" in message for message in messages), messages)

    def test_main_exit_codes(self) -> None:
        self.write("engine/core/src/error.cpp")
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(0, CS.main(["--root", str(self.root)]))
        self.write("cmake/VerifyRrFoo.cmake")
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(1, CS.main(["--root", str(self.root)]))


if __name__ == "__main__":
    unittest.main()
