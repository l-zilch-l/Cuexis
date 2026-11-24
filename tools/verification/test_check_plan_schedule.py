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
CARD_SECTIONS = ("目标", "输入", "写域", "步骤", "任务级验收", "证据", "停止条件")


def card_text(task_id: str, status: str = "active") -> str:
    return (f"# {task_id}\n\n状态：{status}\n\n<!-- cuexis-meta\n"
            f"id: PLAN-test.{task_id}\nstatus: {status}\nauthority: owns\n-->\n\n"
            + "\n\n".join(f"## {name}\n\nFixture {name}." for name in CARD_SECTIONS)
            + "\n")


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
            (tasks / f"{task_id}.md").write_text(card_text(task_id), encoding="utf-8")
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
            (stage / "tasks" / "T-02.md").write_text(
                card_text("T-02") + "depends_on: T-01\n", encoding="utf-8")
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
            (stage / "tasks/T-02.md").write_text(
                card_text("T-02") + "depends_on: T-01\n", encoding="utf-8")
            failures = []
            check_docs.check_stage7_rr_plan_schedule(failures, docs_dir)
            self.assertEqual(checker.validate_stage_dir(stage), [
                (failure.path, failure.message) for failure in failures
            ])
            self.assertEqual(1, len(failures))

    def test_cli_returns_one_for_invalid_absolute_fixture(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            stage = self.make_stage(Path(temporary))
            (stage / "tasks/T-02.md").write_text(
                card_text("T-02") + "depends_on: T-01\n", encoding="utf-8")
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
            self.assertIn("3 active task cards; 0 candidate cards", output.getvalue())

    def test_rejects_each_missing_task_section(self) -> None:
        for section in CARD_SECTIONS:
            with self.subTest(section=section), tempfile.TemporaryDirectory() as temporary:
                stage = self.make_stage(Path(temporary))
                card = stage / "tasks/T-02.md"
                card.write_text(card_text("T-02").replace(f"## {section}\n", ""), encoding="utf-8")
                self.assertTrue(any("task sections must occur once" in m for m in self.messages(stage)))

    def test_rejects_duplicate_empty_and_out_of_order_sections(self) -> None:
        original = card_text("T-02")
        variants = (
            original + "\n## 停止条件\n\nDuplicate.\n",
            original.replace("Fixture 停止条件.", "<!-- empty -->"),
            original.replace("## 目标", "## TEMP").replace("## 输入", "## 目标")
                    .replace("## TEMP", "## 输入"),
        )
        for variant in variants:
            with self.subTest(variant=variant), tempfile.TemporaryDirectory() as temporary:
                stage = self.make_stage(Path(temporary))
                (stage / "tasks/T-02.md").write_text(variant, encoding="utf-8")
                self.assertTrue(any("task section" in m for m in self.messages(stage)))

    def test_fenced_or_commented_headings_do_not_complete_a_card(self) -> None:
        for fence in ("```text\n## 停止条件\n```", "<!--\n## 停止条件\n-->"):
            with self.subTest(fence=fence), tempfile.TemporaryDirectory() as temporary:
                stage = self.make_stage(Path(temporary))
                (stage / "tasks/T-02.md").write_text(
                    card_text("T-02").replace("## 停止条件", fence), encoding="utf-8")
                self.assertTrue(any("task sections must occur once" in m for m in self.messages(stage)))

    def test_accepts_task_headings_with_up_to_three_leading_spaces(self) -> None:
        for spaces in range(4):
            with self.subTest(spaces=spaces), tempfile.TemporaryDirectory() as temporary:
                stage = self.make_stage(Path(temporary))
                (stage / "tasks/T-02.md").write_text(
                    card_text("T-02").replace("## ", " " * spaces + "## "), encoding="utf-8")
                self.assertEqual([], self.messages(stage))

    def test_indented_code_headings_do_not_complete_a_card(self) -> None:
        for indent in ("    ", "\t", " \t"):
            with self.subTest(indent=indent), tempfile.TemporaryDirectory() as temporary:
                stage = self.make_stage(Path(temporary))
                (stage / "tasks/T-02.md").write_text(
                    card_text("T-02").replace("## 停止条件", indent + "## 停止条件"),
                    encoding="utf-8")
                self.assertTrue(any("task sections must occur once" in m for m in self.messages(stage)))

    def test_rejects_invalid_or_disagreeing_status(self) -> None:
        for variant in (card_text("T-02", "future"),
                        card_text("T-02").replace("状态：active", "状态：candidate")):
            with self.subTest(variant=variant), tempfile.TemporaryDirectory() as temporary:
                stage = self.make_stage(Path(temporary))
                (stage / "tasks/T-02.md").write_text(variant, encoding="utf-8")
                self.assertTrue(any("status" in m for m in self.messages(stage)))

    def test_active_task_cannot_depend_on_candidate(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            stage = self.make_stage(Path(temporary))
            (stage / "tasks/T-01.md").write_text(card_text("T-01", "candidate"), encoding="utf-8")
            self.assertTrue(any("active task T-02 must not depend on candidate task T-01"
                                in m for m in self.messages(stage)))

    def test_completed_task_cannot_depend_on_candidate(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            stage = self.make_stage(Path(temporary))
            (stage / "tasks/T-01.md").write_text(card_text("T-01", "candidate"), encoding="utf-8")
            (stage / "tasks/T-02.md").write_text(card_text("T-02", "completed"), encoding="utf-8")
            self.assertTrue(any("completed task T-02 must not depend on candidate task T-01"
                                in m for m in self.messages(stage)))

    def test_completed_prerequisite_is_valid_but_not_dispatched(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            stage = self.make_stage(Path(temporary))
            (stage / "tasks/T-01.md").write_text(card_text("T-01", "completed"), encoding="utf-8")
            (stage / "tasks/T-03.md").write_text(card_text("T-03", "candidate"), encoding="utf-8")
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(0, checker.main(["--stage-dir", str(stage)]))
            lines = output.getvalue().splitlines()
            self.assertIn("Plan schedule checks passed: 1 active task cards; 1 candidate cards; "
                          "1 completed cards.", lines)
            self.assertIn("Current-stage cards (adoption and prerequisites still apply): T-02", lines)
            self.assertIn("Candidate-only cards (separate adoption required): T-03", lines)
            self.assertIn("Completed cards: T-01", lines)

    def test_candidate_cards_are_validated_but_reported_separately(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            stage = self.make_stage(Path(temporary))
            (stage / "tasks/T-03.md").write_text(card_text("T-03", "candidate"), encoding="utf-8")
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(0, checker.main(["--stage-dir", str(stage)]))
            self.assertIn("2 active task cards; 1 candidate cards", output.getvalue())
            self.assertIn("Current-stage cards (adoption and prerequisites still apply): T-01, T-02",
                          output.getvalue())
            self.assertIn("Candidate-only cards (separate adoption required): T-03", output.getvalue())


if __name__ == "__main__":
    unittest.main()
