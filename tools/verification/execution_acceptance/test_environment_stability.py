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


if __name__ == "__main__":
    unittest.main()
