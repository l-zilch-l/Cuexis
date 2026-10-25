"""Harmless Windows Job/ProcessRef tests. No WPR, ETW, elevation, or Cuexis binary."""
from __future__ import annotations

import ctypes as ct
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import unittest
from unittest import mock
import uuid


REPO_ROOT = Path(__file__).resolve().parents[2]
TOOLS = REPO_ROOT / "tools" / "verification" / "playback_performance"
OUTPUT_PARENT = REPO_ROOT / "out" / "pd5-etw-benign-tests"
RUN_OUTPUT = Path(os.environ.get(
    "PD5_ETW_BENIGN_OUTPUT_DIR",
    str(OUTPUT_PARENT / ("run-" + uuid.uuid4().hex)),
)).resolve()
sys.path.insert(0, str(TOOLS))

from etw_safety import job  # noqa: E402
from etw_safety.job import Job, ProcessRef  # noqa: E402


def _python(command: str) -> list[str]:
    return [sys.executable, "-B", "-I", "-c", command]


class JobPrimitiveTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.name != "nt" or ct.sizeof(ct.c_void_p) != 8:
            raise unittest.SkipTest("these primitives require 64-bit Windows")
        if RUN_OUTPUT.parent != OUTPUT_PARENT.resolve():
            raise ValueError("test output must stay under out/pd5-etw-benign-tests")
        RUN_OUTPUT.mkdir(parents=True, exist_ok=True)

    def setUp(self):
        self.case_dir = RUN_OUTPUT / ("case-" + uuid.uuid4().hex)
        self.case_dir.mkdir(parents=True)
        self.job_name = "PD5SafetyTest_" + uuid.uuid4().hex
        self.job = Job.create(self.job_name)
        self.jobs = [self.job]
        self.processes: list[subprocess.Popen[bytes]] = []
        self.refs: list[ProcessRef] = []

    def _spawn(self, command: list[str], output_name: str = "worker.log"):
        output_path = self.case_dir / output_name
        with output_path.open("ab", buffering=0) as output:
            process = self.job.spawn(command, output)
        self.processes.append(process)
        return process, output_path

    def _wait_until(self, predicate, timeout: float = 5.0, *, deadline=None):
        if deadline is None:
            deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if predicate():
                return
            time.sleep(0.02)
        self.fail("timed out waiting for probe condition")

    def _spawn_nested(self, output_name: str = "nested.log"):
        marker = self.case_dir / ("child-" + uuid.uuid4().hex + ".pid")
        pending_marker = marker.with_suffix('.pending')
        child_code = "import time; time.sleep(30)"
        parent_code = (
            "import subprocess,sys,time; from pathlib import Path; "
            f"p=subprocess.Popen([sys.executable,'-B','-I','-c',{child_code!r}]); "
            f"Path({str(pending_marker)!r}).write_text(str(p.pid),encoding='ascii'); "
            f"Path({str(pending_marker)!r}).rename({str(marker)!r}); "
            "time.sleep(30)"
        )
        process, _ = self._spawn(_python(parent_code), output_name)
        self._wait_until(marker.exists)
        pid = int(marker.read_text(encoding="ascii"))
        ref = ProcessRef.from_pid(pid)
        self.refs.append(ref)
        self.assertTrue(ref.alive())
        return process, ref

    def tearDown(self):
        cleanup_errors = []
        for job in self.jobs:
            try:
                if job.active():
                    job.terminate_and_drain(time.monotonic() + 5.0)
                if job.active() != 0:
                    cleanup_errors.append(f"Job {job.name} remained active")
            except Exception as exc:
                cleanup_errors.append(f"Job cleanup failed: {type(exc).__name__}: {exc}")

        for process in self.processes:
            try:
                if process.poll() is None:
                    # This is the exact Popen handle for a test-created gate process.
                    process.kill()
                process.wait(timeout=5)
            except Exception as exc:
                cleanup_errors.append(f"known worker handle cleanup failed: {exc}")

        for ref in self.refs:
            try:
                if ref.alive():
                    ref.terminate_and_wait(time.monotonic() + 5.0)
                if ref.alive():
                    cleanup_errors.append(f"known child PID {ref.pid} remained alive")
            except Exception as exc:
                cleanup_errors.append(f"known child handle cleanup failed: {exc}")

        for job in self.jobs:
            try:
                job.close()
            except Exception as exc:
                cleanup_errors.append(f"Job handle close failed: {exc}")
        for ref in self.refs:
            try:
                ref.close()
            except Exception as exc:
                cleanup_errors.append(f"process handle close failed: {exc}")

        if cleanup_errors:
            self.fail("unexpected test resource state: " + "; ".join(cleanup_errors))


    def test_named_job_is_fresh_and_duplicate_is_rejected(self):
        self.assertEqual(self.job.active(), 0)

    def test_controller_is_outside_new_worker_job(self):
        with ProcessRef.from_pid(os.getpid()) as identity:
            self.assertFalse(self.job.contains(identity))
        with self.assertRaises(FileExistsError):
            Job.create(self.job_name)
        self.assertEqual(self.job.active(), 0)

    def test_spawn_assigns_gate_before_go_and_returns_worker_exit_code(self):
        code = "import time; print('probe-started',flush=True); time.sleep(.4); print('probe-done',flush=True)"
        process, output_path = self._spawn(_python(code))
        self._wait_until(lambda: output_path.exists() and "probe-started" in output_path.read_text(
            encoding="utf-8", errors="replace"
        ))
        self.assertGreater(self.job.active(), 0)
        self.assertEqual(process.wait(timeout=5), 0)
        self._wait_until(lambda: self.job.active() == 0)
        output = output_path.read_text(encoding="utf-8", errors="replace")
        self.assertIn("probe-done", output)

    def test_open_guardian_handle_can_query_and_terminate_but_cannot_spawn(self):
        process, _ = self._spawn(_python("import time; time.sleep(30)"))
        self._wait_until(lambda: self.job.active() > 0)
        guardian = Job.open(self.job_name)
        self.jobs.append(guardian)
        self.assertGreater(guardian.active(), 0)
        with ProcessRef.from_pid(process.pid) as member:
            self.assertTrue(guardian.contains(member))
        with self.assertRaises(PermissionError):
            guardian.spawn(_python("raise SystemExit(0)"), None)
        self.assertEqual(guardian.terminate_and_drain(time.monotonic() + 5.0), 0)
        self.assertEqual(guardian.active(), 0)
        process.wait(timeout=5)
        self.assertEqual(self.job.active(), 0)

    def test_terminate_and_drain_reaps_nested_python_child(self):
        process, child = self._spawn_nested()
        self.assertGreaterEqual(self.job.active(), 3)
        self.assertTrue(child.alive())
        deadline = time.monotonic() + 5.0
        self.assertEqual(self.job.terminate_and_drain(deadline), 0)
        self.assertEqual(self.job.active(), 0)
        process.wait(timeout=max(0, deadline - time.monotonic()))
        # Job accounting zero and the gate's exit do not synchronize this child's
        # retained process handle. Observe its signal within the same deadline;
        # do not terminate it here, which would hide failed Job reclamation.
        self._wait_until(lambda: not child.alive(), deadline=deadline)
        self.assertFalse(child.alive())

    def test_kill_on_close_terminates_nested_python_child(self):
        process, child = self._spawn_nested("kill-on-close.log")
        self.assertGreaterEqual(self.job.active(), 3)
        closing_job = self.job
        self.job.close()
        self.jobs.remove(closing_job)
        self.job = None
        process.wait(timeout=5)
        self._wait_until(lambda: not child.alive())
        self.assertFalse(child.alive())

    def test_process_ref_seals_creation_identity_and_waits_for_exact_process(self):
        process = subprocess.Popen(
            _python("import time; time.sleep(30)"),
            stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.STDOUT,
            creationflags=subprocess.CREATE_NO_WINDOW,
        )
        self.processes.append(process)
        ref = ProcessRef.from_pid(process.pid)
        self.refs.append(ref)
        self.assertEqual(ref.pid, process.pid)
        self.assertGreater(ref.creation_time, 0)
        self.assertTrue(ref.alive())
        with self.assertRaisesRegex(RuntimeError, 'before process exit'):
            ref.exit_code()
        with self.assertRaises(ProcessLookupError):
            ProcessRef(process.pid, ref.creation_time + 1)
        ref.terminate_and_wait(time.monotonic() + 5.0, exit_code=42)
        self.assertFalse(ref.alive())
        self.assertEqual(ref.exit_code(), 42)
        process.wait(timeout=5)

    def test_worker_gate_eof_and_non_go_never_spawn_command(self):
        gate = TOOLS / "etw_safety" / "worker_gate.py"
        marker = self.case_dir / "must-not-exist.txt"
        command = _python(f"open({str(marker)!r},'w').write('spawned')")
        output_path = self.case_dir / "direct-gate.log"

        with output_path.open("ab", buffering=0) as output:
            eof = subprocess.Popen(
                [sys.executable, "-B", "-I", "-S", str(gate)],
                stdin=subprocess.PIPE,
                stdout=output,
                stderr=subprocess.STDOUT,
                creationflags=subprocess.CREATE_NO_WINDOW,
            )
            self.processes.append(eof)
            eof.stdin.close()
            self.assertEqual(eof.wait(timeout=5), 0)
        self.assertFalse(marker.exists())

        with output_path.open("ab", buffering=0) as output:
            no_go = subprocess.Popen(
                [sys.executable, "-B", "-I", "-S", str(gate)],
                stdin=subprocess.PIPE,
                stdout=output,
                stderr=subprocess.STDOUT,
                creationflags=subprocess.CREATE_NO_WINDOW,
            )
            self.processes.append(no_go)
            payload = json.dumps(
                {"go": False, "command": command}, separators=(",", ":")
            ).encode("utf-8") + b"\n"
            no_go.stdin.write(payload)
            no_go.stdin.close()
            self.assertEqual(no_go.wait(timeout=5), 2)
        self.assertFalse(marker.exists())

        with output_path.open("ab", buffering=0) as output:
            duplicate = subprocess.Popen(
                [sys.executable, "-B", "-I", "-S", str(gate)],
                stdin=subprocess.PIPE,
                stdout=output,
                stderr=subprocess.STDOUT,
                creationflags=subprocess.CREATE_NO_WINDOW,
            )
            self.processes.append(duplicate)
            payload = b'{"go":false,"go":true,"command":' + json.dumps(command).encode("utf-8") + b'}\n'
            duplicate.stdin.write(payload)
            duplicate.stdin.close()
            self.assertEqual(duplicate.wait(timeout=5), 2)
        self.assertFalse(marker.exists())


class JobReapingSynchronizationTests(unittest.TestCase):
    """Force the accounting-zero/handle-signal gap without scheduler luck."""

    def _nested_case(self):
        case = JobPrimitiveTests('test_terminate_and_drain_reaps_nested_python_child')
        case.job = mock.Mock(spec=Job)
        case.job.active.side_effect = [3, 0]
        case.job.terminate_and_drain.return_value = 0
        process = mock.Mock(spec=subprocess.Popen)
        process.wait.return_value = 1
        child = mock.Mock(spec=ProcessRef)
        case._spawn_nested = mock.Mock(return_value=(process, child))
        return case, process, child

    def test_job_zero_and_gate_exit_still_wait_for_child_handle_signal(self):
        case, process, child = self._nested_case()
        child.alive.side_effect = [True, True, False, False]
        with mock.patch.object(time, 'monotonic', return_value=0), \
                mock.patch.object(time, 'sleep'):
            case.test_terminate_and_drain_reaps_nested_python_child()
        case.job.terminate_and_drain.assert_called_once_with(5.0)
        process.wait.assert_called_once_with(timeout=5.0)
        self.assertEqual(child.alive.call_count, 4)
        child.terminate_and_wait.assert_not_called()

    def test_unsignaled_child_fails_at_the_original_deadline(self):
        case, process, child = self._nested_case()
        child.alive.return_value = True
        with mock.patch.object(time, 'monotonic', side_effect=[0, 0, 0, 5]), \
                mock.patch.object(time, 'sleep'):
            with self.assertRaisesRegex(AssertionError, 'timed out waiting'):
                case.test_terminate_and_drain_reaps_nested_python_child()
        case.job.terminate_and_drain.assert_called_once_with(5.0)
        process.wait.assert_called_once_with(timeout=5.0)
        child.terminate_and_wait.assert_not_called()

    def test_child_signal_observed_after_the_deadline_does_not_pass(self):
        case, process, child = self._nested_case()
        # The third observation would see a signal, but only after budget expiry.
        child.alive.side_effect = [True, True, False, False]
        with mock.patch.object(time, 'monotonic', side_effect=[0, 0, 0, 5]), \
                mock.patch.object(time, 'sleep'):
            with self.assertRaisesRegex(AssertionError, 'timed out waiting'):
                case.test_terminate_and_drain_reaps_nested_python_child()
        self.assertEqual(child.alive.call_count, 2)
        case.job.terminate_and_drain.assert_called_once_with(5.0)
        process.wait.assert_called_once_with(timeout=5.0)
        child.terminate_and_wait.assert_not_called()


class ProcessIdentityAcquisitionTests(unittest.TestCase):
    """``ProcessRef.acquire`` is how a finished identity is proved dead, not raised on."""

    @classmethod
    def setUpClass(cls):
        if os.name != "nt" or ct.sizeof(ct.c_void_p) != 8:
            raise unittest.SkipTest("these primitives require 64-bit Windows")

    def test_the_running_identity_is_returned_with_its_handle(self):
        current = ProcessRef.from_pid(os.getpid())
        try:
            reference = ProcessRef.acquire(os.getpid(), current.creation_time)
            self.assertIsNotNone(reference)
            try:
                self.assertEqual(reference.pid, os.getpid())
                self.assertEqual(reference.creation_time, current.creation_time)
                self.assertTrue(reference.alive())
            finally:
                reference.close()
        finally:
            current.close()

    def test_a_vanished_pid_is_absent_instead_of_raising(self):
        # r3's real failure: the death proof re-opened a native PID that no longer
        # existed, and OpenProcess reported 87 as a hard error.
        for code in (2, 87, 1168):
            with mock.patch.object(job, "_open_process",
                                   side_effect=ct.WinError(code, "OpenProcess")):
                self.assertIsNone(ProcessRef.acquire(4242, 134372335283453126))

    def test_a_refused_pid_still_raises_instead_of_reading_as_death(self):
        with mock.patch.object(job, "_open_process",
                               side_effect=ct.WinError(5, "OpenProcess")):
            with self.assertRaises(OSError) as caught:
                ProcessRef.acquire(4242, 134372335283453126)
        self.assertEqual(caught.exception.winerror, 5)

    def test_a_reused_pid_with_another_creation_time_is_absent(self):
        current = ProcessRef.from_pid(os.getpid())
        try:
            self.assertIsNone(ProcessRef.acquire(os.getpid(), current.creation_time + 1))
        finally:
            current.close()

    def test_a_malformed_creation_time_is_refused(self):
        for value in (-1, 1 << 64, "134372335283453126", True):
            with self.assertRaises(ValueError):
                ProcessRef.acquire(os.getpid(), value)


if __name__ == "__main__":
    unittest.main(verbosity=2, failfast=True)
