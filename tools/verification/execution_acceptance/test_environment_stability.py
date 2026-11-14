import unittest

from tools.verification.execution_acceptance import admission, environment_stability


def row(monotonic, foreground=4242, ac=1, scheme="aa", conflicts=()):
    return {"monotonic": monotonic, "foreground_pid": foreground, "ac_line_status": ac,
            "power_scheme_bytes": scheme, "conflicts": list(conflicts)}


class EnvironmentStabilityTests(unittest.TestCase):
    def test_a_clean_interval_passes(self):
        result = environment_stability.summary([row(0.0), row(1.0), row(2.0)])
        self.assertTrue(result["passed"])
        self.assertEqual(result["distinct_foreground_pids"], [4242])
        self.assertEqual(result["errors"], [])

    def test_a_locked_workstation_is_a_failed_remedy(self):
        result = environment_stability.summary([row(0.0), row(1.0, foreground=0)])
        self.assertFalse(result["passed"])
        self.assertIn("no foreground window", result["errors"])
        self.assertIn("foreground process changed", result["errors"])

    def test_ac_power_scheme_conflicts_and_gaps_are_separate_reasons(self):
        cases = {
            "AC line left": [row(0.0), row(1.0, ac=0)],
            "active power scheme changed": [row(0.0), row(1.0, scheme="bb")],
            "conflicting process present": [row(0.0), row(1.0, conflicts=[{"name": "ninja.exe"}])],
            "incomplete environment sampling interval": [row(0.0), row(9.0)],
        }
        for reason, samples in cases.items():
            with self.subTest(reason=reason):
                result = environment_stability.summary(samples)
                self.assertFalse(result["passed"])
                self.assertIn(reason, result["errors"])

    def test_empty_interval_is_rejected(self):
        with self.assertRaises(ValueError):
            environment_stability.summary([])

    def test_nudge_cadence_resets_the_idle_timer_on_schedule(self):
        self.assertFalse(environment_stability.nudge_due(0, 240, 0))
        self.assertTrue(environment_stability.nudge_due(240, 240, 0))
        self.assertFalse(environment_stability.nudge_due(479, 240, 1))
        self.assertTrue(environment_stability.nudge_due(480, 240, 1))
        self.assertFalse(environment_stability.nudge_due(9999, 0, 0))


class NudgeInputTests(unittest.TestCase):
    def test_a_nudge_sends_exactly_two_relative_moves(self):
        if __import__("os").name != "nt":
            self.skipTest("SendInput is Windows-only")
        self.assertEqual(environment_stability.nudge(), 2)


class RetryDispositionTests(unittest.TestCase):
    def setUp(self):
        import tempfile
        from pathlib import Path
        from tools.verification.execution_acceptance.binding import sha256, write_once
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.ledger = Path(self.temporary.name) / "ledger"
        self.sha256, self.write_once = sha256, write_once

    def test_disposition_binds_the_previous_claim(self):
        directory = self.ledger / "p1-no-consumer-release"
        directory.mkdir(parents=True)
        claim = directory / "attempt-1.json"
        self.write_once(claim, {"schema": "rpa.execution.claim.v1", "unit": "p1-no-consumer"})
        remedy = {"path": "remedy.json", "bytes": 1, "sha256": "a" * 64}
        record = admission.retry_disposition(self.ledger, "p1-no-consumer-release", remedy)
        self.assertEqual(record["schema"], "rpa.execution.retry-disposition.v1")
        self.assertEqual(record["previous_claim_sha256"], self.sha256(claim))
        self.assertEqual(record["verified_remedy_binding"], remedy)

    def test_a_unit_without_a_previous_attempt_cannot_be_retried(self):
        with self.assertRaises(ValueError):
            admission.retry_disposition(self.ledger, "p1-no-consumer-release", {"path": "x"})
        with self.assertRaises(ValueError):
            admission.retry_disposition(self.ledger, "not a unit id", {"path": "x"})


class StabilityCollectionTests(unittest.TestCase):
    class Clock:
        def __init__(self, interrupt_at=None, trailing_gap=False):
            self.value = 0.0
            self.interrupt_at = interrupt_at
            self.trailing_gap = trailing_gap

        def monotonic(self):
            return self.value

        def sleep(self, interval):
            self.value += 3 if self.trailing_gap and self.value == 59 else interval
            if self.interrupt_at is not None and self.value >= self.interrupt_at:
                raise KeyboardInterrupt("interrupted virtual collection")

    def collect(self, clock, *, seconds=60, failing_snapshot=False, hold_ok=True,
                first_delay=0, nudge_events=2, failing_after=None):
        from contextlib import ExitStack
        import tempfile
        from pathlib import Path
        from unittest.mock import patch
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.output = Path(self.temporary.name) / "remedy.json"
        snapshots = 0

        def snapshot():
            nonlocal snapshots
            if failing_snapshot:
                raise RuntimeError("snapshot failed before first sample")
            if failing_after is not None and snapshots >= failing_after:
                raise RuntimeError("snapshot failed after existing samples")
            if snapshots == 0:
                clock.value += first_delay
            snapshots += 1
            return row(clock.value)

        with ExitStack() as stack:
            stack.enter_context(patch.object(environment_stability, "time", clock))
            stack.enter_context(patch.object(environment_stability, "WindowsSnapshot",
                                             return_value=snapshot))
            self.hold = stack.enter_context(patch.object(environment_stability, "hold",
                return_value={"requested": 1, "previous_state": int(hold_ok)}))
            stack.enter_context(patch.object(environment_stability, "nudge",
                                             return_value=nudge_events))
            return environment_stability.verify(seconds, 1, self.output)

    def result(self):
        from tools.verification.execution_acceptance.binding import read
        return read(self.output)

    def test_complete_requested_duration_and_coverage_pass(self):
        self.assertTrue(self.collect(self.Clock()))
        result = self.result()
        self.assertTrue(result["completed"])
        self.assertEqual(result["duration_seconds"], 60)
        self.assertEqual(result["sample_span_seconds"], 59)
        self.assertEqual(result["errors"], [])
        self.hold.assert_any_call(False)

    def test_interruption_after_three_nudges_cannot_pass(self):
        with self.assertRaisesRegex(KeyboardInterrupt, "interrupted virtual collection"):
            self.collect(self.Clock(interrupt_at=725), seconds=900)
        result = self.result()
        self.assertEqual(len(result["nudges"]), 3)
        self.assertEqual(result["requested_seconds"], 900)
        self.assertEqual(result["duration_seconds"], 725)
        self.assertFalse(result["passed"])
        self.assertFalse(result["completed"])
        self.hold.assert_any_call(False)

    def test_first_snapshot_failure_keeps_original_error_and_failed_record(self):
        with self.assertRaisesRegex(RuntimeError, "snapshot failed before first sample"):
            self.collect(self.Clock(), failing_snapshot=True)
        result = self.result()
        self.assertEqual(result["samples"], [])
        self.assertFalse(result["passed"])
        self.assertIn("no environment samples", result["errors"])
        self.hold.assert_any_call(False)

    def test_unsampled_trailing_time_cannot_complete_coverage(self):
        self.assertFalse(self.collect(self.Clock(trailing_gap=True)))
        result = self.result()
        self.assertTrue(result["completed"])
        self.assertGreaterEqual(result["duration_seconds"], 60)
        self.assertIn("incomplete environment sampling coverage", result["errors"])

    def test_failed_awake_hold_cannot_pass(self):
        self.assertFalse(self.collect(self.Clock(), hold_ok=False))
        self.assertIn("system/display hold failed", self.result()["errors"])

    def test_complete_900_seconds_keeps_three_successful_nudges(self):
        self.assertTrue(self.collect(self.Clock(), seconds=900))
        result = self.result()
        self.assertEqual(result["duration_seconds"], 900)
        self.assertEqual(result["sample_span_seconds"], 899)
        self.assertEqual([n["elapsed_seconds"] for n in result["nudges"]], [240, 480, 720])
        self.assertTrue(all(n["send_input_events"] == 2 for n in result["nudges"]))

    def test_unsampled_initial_time_cannot_complete_coverage(self):
        self.assertFalse(self.collect(self.Clock(), first_delay=3))
        result = self.result()
        self.assertTrue(result["completed"])
        self.assertGreaterEqual(result["duration_seconds"], 60)
        self.assertIn("incomplete environment sampling coverage", result["errors"])

    def test_nudge_failure_is_recorded_as_an_error(self):
        self.assertFalse(self.collect(self.Clock(), seconds=900, nudge_events=0))
        self.assertIn("idle nudge failed", self.result()["errors"])

    def test_later_snapshot_failure_keeps_samples_and_original_error(self):
        with self.assertRaisesRegex(RuntimeError, "snapshot failed after existing samples"):
            self.collect(self.Clock(), failing_after=2)
        result = self.result()
        self.assertEqual(len(result["samples"]), 2)
        self.assertFalse(result["passed"])
        self.hold.assert_any_call(False)


if __name__ == "__main__":
    unittest.main()
