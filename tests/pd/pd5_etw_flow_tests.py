"""Fault tests for the production cleanup ordering, using effects that cannot touch ETW."""
from __future__ import annotations

from pathlib import Path
import sys
import unittest


HERE = Path(__file__).resolve().parent
REPO_ROOT = Path(__file__).resolve().parents[2]
TOOLS = REPO_ROOT / "tools" / "verification" / "playback_performance" / "etw_safety"
sys.path.insert(0, str(TOOLS))

import flow  # noqa: E402


class InjectedFailure(Exception):
    pass


class Effects:
    def __init__(
        self,
        *,
        controller_alive=False,
        active=0,
        cleanup_result=None,
        absent=True,
        failures=None,
    ):
        self.events = []
        self._controller_alive = controller_alive
        self._active = active
        self._cleanup_result = cleanup_result or {"sessions_absent": True, "raw_trace_only": True}
        self._absent = absent
        self._failures = failures or {}

    def _record(self, name):
        self.events.append(name)
        failure = self._failures.get(name)
        if failure is not None:
            raise failure

    def stop_controller(self, deadline):
        self._record("stop_controller")

    def controller_alive(self):
        self._record("controller_alive")
        return self._controller_alive

    def drain_job(self, deadline):
        self._record("drain_job")

    def active_processes(self):
        self._record("active_processes")
        return self._active

    def stop_owned_and_prove(self, deadline):
        self._record("stop_owned_and_prove")
        return self._cleanup_result

    def prove_absent(self, deadline):
        self._record("prove_absent")
        return self._absent


class TakeOverTests(unittest.TestCase):
    deadline = 12345.0

    def test_success_orders_parent_death_job_drain_zero_then_owned_cleanup(self):
        effects = Effects(cleanup_result={"sessions_absent": True, "raw_trace_only": True})
        result = flow.take_over(effects, self.deadline)
        self.assertEqual(
            effects.events,
            [
                "stop_controller",
                "controller_alive",
                "drain_job",
                "active_processes",
                "stop_owned_and_prove",
            ],
        )
        self.assertEqual(result, {"sessions_absent": True, "raw_trace_only": True})

    def test_controller_stop_error_prevents_all_later_effects(self):
        effects = Effects(failures={"stop_controller": InjectedFailure("stop failed")})
        with self.assertRaisesRegex(InjectedFailure, "stop failed"):
            flow.take_over(effects, self.deadline)
        self.assertEqual(effects.events, ["stop_controller"])

    def test_live_controller_blocks_job_and_etw_cleanup(self):
        effects = Effects(controller_alive=True)
        with self.assertRaisesRegex(RuntimeError, "controller death not proved"):
            flow.take_over(effects, self.deadline)
        self.assertEqual(effects.events, ["stop_controller", "controller_alive"])

    def test_drain_error_blocks_active_query_and_etw_cleanup(self):
        effects = Effects(failures={"drain_job": InjectedFailure("drain failed")})
        with self.assertRaisesRegex(InjectedFailure, "drain failed"):
            flow.take_over(effects, self.deadline)
        self.assertEqual(
            effects.events,
            ["stop_controller", "controller_alive", "drain_job"],
        )

    def test_nonzero_job_count_blocks_owned_stop(self):
        effects = Effects(active=1)
        with self.assertRaisesRegex(RuntimeError, "worker Job is not empty"):
            flow.take_over(effects, self.deadline)
        self.assertEqual(
            effects.events,
            ["stop_controller", "controller_alive", "drain_job", "active_processes"],
        )

    def test_job_query_error_blocks_owned_stop(self):
        effects = Effects(failures={"active_processes": InjectedFailure("query denied")})
        with self.assertRaisesRegex(InjectedFailure, "query denied"):
            flow.take_over(effects, self.deadline)
        self.assertEqual(
            effects.events,
            ["stop_controller", "controller_alive", "drain_job", "active_processes"],
        )

    def test_owned_cleanup_error_is_propagated_after_prerequisites(self):
        effects = Effects(
            failures={"stop_owned_and_prove": InjectedFailure("inventory unavailable")}
        )
        with self.assertRaisesRegex(InjectedFailure, "inventory unavailable"):
            flow.take_over(effects, self.deadline)
        self.assertEqual(
            effects.events,
            [
                "stop_controller",
                "controller_alive",
                "drain_job",
                "active_processes",
                "stop_owned_and_prove",
            ],
        )


class ReleasedCleanTests(unittest.TestCase):
    deadline = 12345.0

    def test_release_requires_zero_and_absence_proof(self):
        effects = Effects(active=0, absent=True)
        result = flow.released_clean(effects, self.deadline)
        self.assertEqual(effects.events, ["active_processes", "prove_absent"])
        self.assertEqual(
            result,
            {"released_clean": True, "active_descendants": 0, "sessions_absent": True},
        )

    def test_live_descendant_blocks_absence_query(self):
        effects = Effects(active=2, absent=True)
        with self.assertRaisesRegex(RuntimeError, "release with live worker descendants"):
            flow.released_clean(effects, self.deadline)
        self.assertEqual(effects.events, ["active_processes"])

    def test_active_query_error_blocks_release(self):
        effects = Effects(failures={"active_processes": InjectedFailure("active query failed")})
        with self.assertRaisesRegex(InjectedFailure, "active query failed"):
            flow.released_clean(effects, self.deadline)
        self.assertEqual(effects.events, ["active_processes"])

    def test_missing_exact_absence_proof_blocks_release(self):
        effects = Effects(active=0, absent=False)
        with self.assertRaisesRegex(RuntimeError, "release without exact ETW absence proof"):
            flow.released_clean(effects, self.deadline)
        self.assertEqual(effects.events, ["active_processes", "prove_absent"])

    def test_absence_query_error_blocks_release(self):
        effects = Effects(failures={"prove_absent": InjectedFailure("inventory incomplete")})
        with self.assertRaisesRegex(InjectedFailure, "inventory incomplete"):
            flow.released_clean(effects, self.deadline)
        self.assertEqual(effects.events, ["active_processes", "prove_absent"])


if __name__ == "__main__":
    unittest.main(verbosity=2, failfast=True)
