"""Regression tests for the Stage 7-RR plan schedule checker."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


HERE = Path(__file__).resolve().parent


def load_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


checker = load_module("check_plan_schedule", HERE / "check_plan_schedule.py")
check_docs = load_module("check_docs_for_plan_schedule_test", HERE / "check_docs.py")


SCHEDULE = """# Stage schedule

<!-- cuexis-meta
id: PLAN-test.schedule
status: active
authority: owns
-->

| 波次 | 任务 ID | 前置 ID | 可并行/独占 |
| --- | --- | --- | --- |
| 1 | T-01 | — | 独占 |
| 2 | T-02 | T-01 | 独占 |
| 3 | T-03 | — | 独占 |

```mermaid
flowchart TD
T-01 --> T-02
T-03
```
"""
PLAIN_SCHEDULE = SCHEDULE.partition("```mermaid")[0]


class PlanScheduleTests(unittest.TestCase):
    def make_stage(
        self,
        root: Path,
        schedule: str = SCHEDULE,
        task_ids: tuple[str, ...] = ("T-01", "T-02", "T-03"),
        acceptance: str | None = "Tasks: T-01, T-02, T-03\n",
    ) -> Path:
        stage = root / "docs" / "stage_plans" / "active" / "stage-07-rr"
        tasks = stage / "tasks"
        tasks.mkdir(parents=True)
        (stage / "schedule.md").write_text(schedule, encoding="utf-8")
        for task_id in task_ids:
            (tasks / f"{task_id}.md").write_text(f"# {task_id}\n", encoding="utf-8")
        if acceptance is not None:
            (stage / "acceptance.md").write_text(acceptance, encoding="utf-8")
        return stage

    def messages(self, stage: Path) -> list[str]:
        return [message for _, message in checker.validate_stage_dir(stage)]

    def test_accepts_schedule_cards_mermaid_and_acceptance(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            stage = self.make_stage(Path(temporary))
            self.assertEqual([], self.messages(stage))

    def test_accepts_page_metadata_and_links_to_matching_cards(self) -> None:
        schedule = SCHEDULE.replace(
            "<!-- cuexis-meta", "状态：active\n更新日期：2026-11-21\n\n<!-- cuexis-meta"
        ).replace("| T-02 | T-01 |", "| [T-02](tasks/T-02.md) | `T-01` |")
        with tempfile.TemporaryDirectory() as temporary:
            stage = self.make_stage(Path(temporary), schedule=schedule)
            self.assertEqual([], self.messages(stage))

    def test_accepts_parallel_tasks_and_multiple_prerequisites(self) -> None:
        schedule = PLAIN_SCHEDULE.replace(
            "| 3 | T-03 | — | 独占 |", "| 2 | T-03 | T-01 | 可并行 |"
        ).replace("| 2 | T-02 | T-01 | 独占 |", "| 2 | T-02 | T-01 | 可并行 |")
        for value in (schedule, PLAIN_SCHEDULE.replace(
            "| 3 | T-03 | — |", "| 3 | T-03 | T-01, `T-02` |"
        )):
            with self.subTest(schedule=value), tempfile.TemporaryDirectory() as temporary:
                stage = self.make_stage(Path(temporary), schedule=value)
                self.assertEqual([], self.messages(stage))

    def test_rejects_body_hidden_in_metadata_or_outside_table(self) -> None:
        for text, expected in (
            (SCHEDULE.replace("authority: owns", "authority: owns\nowner: Alice"), "metadata"),
            (SCHEDULE + "%% owner: Alice\n", "content outside"),
            (PLAIN_SCHEDULE + "状态：done\n", "content outside"),
        ):
            with self.subTest(text=text), tempfile.TemporaryDirectory() as temporary:
                stage = self.make_stage(Path(temporary), schedule=text)
                self.assertTrue(any(expected in message for message in self.messages(stage)))

    def test_rejects_names_status_and_wrong_card_links_in_cells(self) -> None:
        for cell in ("T-01 work", "T-01 (done)", "[Alice](tasks/T-01.md)",
                     "[T-01](tasks/T-02.md)"):
            with self.subTest(cell=cell), tempfile.TemporaryDirectory() as temporary:
                stage = self.make_stage(
                    Path(temporary), schedule=SCHEDULE.replace("| 1 | T-01 |", f"| 1 | {cell} |")
                )
                self.assertTrue(any("task ID must" in message for message in self.messages(stage)))

    def test_rejects_mermaid_names_and_mismatched_dependencies(self) -> None:
        for text, expected in (
            (SCHEDULE.replace("T-03\n", "T-03[owner Alice]\n"), "only bare task IDs"),
            (SCHEDULE.replace("T-03\n", "T-99\n"), "unknown task IDs: T-99"),
            (SCHEDULE.replace("T-01 --> T-02", "T-02 --> T-01"), "not declared in the table"),
        ):
            with self.subTest(text=text), tempfile.TemporaryDirectory() as temporary:
                stage = self.make_stage(Path(temporary), schedule=text)
                self.assertTrue(any(expected in message for message in self.messages(stage)))

    def test_rejects_rows_outside_the_four_column_contract(self) -> None:
        invalid = SCHEDULE.replace(
            "| 1 | T-01 | — | 独占 |", "| 1 | T-01 | — | 独占 | in progress |"
        )
        with tempfile.TemporaryDirectory() as temporary:
            stage = self.make_stage(Path(temporary), schedule=invalid)
            self.assertTrue(any("exactly four columns" in message for message in self.messages(stage)))

    def test_rejects_non_monotone_wave_rows(self) -> None:
        wave_two = "| 2 | T-02 | T-01 | 独占 |\n"
        wave_three = "| 3 | T-03 | — | 独占 |\n"
        invalid = SCHEDULE.replace(wave_two + wave_three, wave_three + wave_two)
        with tempfile.TemporaryDirectory() as temporary:
            stage = self.make_stage(Path(temporary), schedule=invalid)
            self.assertTrue(any("nondecreasing order" in message for message in self.messages(stage)))

    def test_rejects_duplicate_and_missing_task_cards(self) -> None:
        duplicate = SCHEDULE.replace(
            "| 3 | T-03 | — | 独占 |", "| 3 | T-02 | — | 独占 |"
        )
        with tempfile.TemporaryDirectory() as temporary:
            stage = self.make_stage(Path(temporary), schedule=duplicate)
            messages = self.messages(stage)
            self.assertTrue(any("scheduled more than once" in message for message in messages))

        with tempfile.TemporaryDirectory() as temporary:
            stage = self.make_stage(Path(temporary), task_ids=("T-01", "T-02"))
            self.assertTrue(any("has no task card: T-03" in message for message in self.messages(stage)))

    def test_rejects_unknown_and_non_earlier_prerequisites(self) -> None:
        unknown = SCHEDULE.replace("| 2 | T-02 | T-01 |", "| 2 | T-02 | T-99 |")
        with tempfile.TemporaryDirectory() as temporary:
            stage = self.make_stage(Path(temporary), schedule=unknown)
            self.assertTrue(any("not scheduled: T-99" in message for message in self.messages(stage)))

        for dependency in ("T-02", "T-03"):
            with self.subTest(dependency=dependency), tempfile.TemporaryDirectory() as temporary:
                invalid = PLAIN_SCHEDULE.replace(
                    "| 2 | T-02 | T-01 |", f"| 2 | T-02 | {dependency} |"
                )
                stage = self.make_stage(Path(temporary), schedule=invalid)
                self.assertTrue(any(
                    "must be in an earlier wave" in message for message in self.messages(stage)
                ))

    def test_rejects_multi_task_exclusive_wave(self) -> None:
        invalid = SCHEDULE.replace(
            "| 2 | T-02 | T-01 | 独占 |", "| 1 | T-02 | T-01 | 可并行 |"
        )
        with tempfile.TemporaryDirectory() as temporary:
            stage = self.make_stage(Path(temporary), schedule=invalid)
            self.assertTrue(any("exclusive wave 1" in message for message in self.messages(stage)))

    def test_rejects_depends_on_in_task_card(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            stage = self.make_stage(Path(temporary))
            (stage / "tasks" / "T-02.md").write_text("depends_on: T-01\n", encoding="utf-8")
            self.assertTrue(any("must not define depends_on" in message for message in self.messages(stage)))

    def test_acceptance_validates_ids_and_expands_explicit_ranges(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            stage = self.make_stage(Path(temporary), acceptance="Tasks: T-01–T-03\n")
            self.assertEqual([], self.messages(stage))

        with tempfile.TemporaryDirectory() as temporary:
            stage = self.make_stage(Path(temporary), acceptance="Tasks: T-01, T-02, T-99\n")
            messages = self.messages(stage)
            self.assertTrue(any("unknown task ID: T-99" in message for message in messages))
            self.assertTrue(any("does not cover task ID: T-03" in message for message in messages))

    def test_check_docs_gate_uses_the_same_checker_for_stage7_rr(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            docs_dir = Path(temporary) / "docs"
            self.make_stage(Path(temporary))
            failures = []
            check_docs.check_stage7_rr_plan_schedule(failures, docs_dir)
            self.assertEqual([], failures)

    def test_check_docs_gate_propagates_failures_without_scanning_future(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            stage = self.make_stage(Path(temporary))
            docs_dir = Path(temporary) / "docs"
            (docs_dir / "stage_plans/future/stage-08").mkdir(parents=True)
            (stage / "tasks/T-02.md").write_text("depends_on: T-01\n", encoding="utf-8")
            failures = []
            check_docs.check_stage7_rr_plan_schedule(failures, docs_dir)
            self.assertEqual(checker.validate_stage_dir(stage), [
                (failure.path, failure.message) for failure in failures
            ])
            self.assertEqual(1, len(failures))

    def test_cli_returns_one_for_invalid_absolute_fixture(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            stage = self.make_stage(Path(temporary))
            (stage / "tasks/T-02.md").write_text("depends_on: T-01\n", encoding="utf-8")
            result = subprocess.run(
                [sys.executable, "-B", str(HERE / "check_plan_schedule.py"),
                 "--stage-dir", str(stage.resolve())],
                capture_output=True, text=True, encoding="utf-8", check=False,
            )
            self.assertEqual(1, result.returncode)
            self.assertIn("task cards must not define depends_on", result.stdout)
            self.assertEqual("", result.stderr)

    def test_cli_accepts_absolute_fixture_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            stage = self.make_stage(Path(temporary))
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                result = checker.main(["--stage-dir", str(stage.resolve())])
            self.assertEqual(0, result)
            self.assertIn("3 task cards scheduled", output.getvalue())


if __name__ == "__main__":
    unittest.main()
