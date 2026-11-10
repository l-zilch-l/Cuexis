import unittest

from tools.verification.execution_acceptance.runtime_monitor import RuntimeMonitor


class MonitorTests(unittest.TestCase):
    def test_changes_inside_interval_remain_failures_after_restoration(self):
        rows = iter([dict(monotonic=float(index), conflicts=conflict, foreground_pid=foreground,
                          ac_line_status=1, power_scheme_bytes=plan)
                     for index, (conflict, foreground, plan) in enumerate([
                         ([], 42, "a"), ([{"name": "ninja.exe"}], 42, "a"),
                         ([], 44, "b"), ([], 42, "a")])])
        monitor = RuntimeMonitor(42, timeout=10, snapshot=lambda: next(rows))
        for _ in range(4):
            monitor.sample()
        result = monitor.result()
        self.assertFalse(result["passed"])
        self.assertTrue(any("power scheme" in error for error in result["errors"]))
        self.assertEqual(len(result["samples"]), 4)

    def test_missing_monitoring_and_sampling_gaps_cannot_pass(self):
        monitor = RuntimeMonitor(42, timeout=10, snapshot=lambda: None)
        self.assertFalse(monitor.result()["passed"])
        monitor.samples = [{"monotonic": 0}, {"monotonic": 3}]
        self.assertFalse(monitor.result()["passed"])


if __name__ == "__main__":
    unittest.main()
