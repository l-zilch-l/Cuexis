"""Fault-injected ETW/controller tests. No real ETW, WPR, administrator or Cuexis calls."""
import ctypes as ct
import os
from pathlib import Path
import subprocess
import sys
import time
import unittest
from unittest import mock
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/verification/playback_performance/etw_safety'))
import controller
import etw
from common import load, write
from common import sha
from profile_config import profile_argument


class ControllerTests(unittest.TestCase):
    def setUp(self):
        self.run = ROOT / 'out/pd5-etw-mock-tests' / uuid.uuid4().hex
        self.run.mkdir(parents=True)
        self.claim_hash = 'c' * 64

    def test_one_raw_stop_then_queries_until_absence(self):
        effects = controller.Effects(None, None, self.run, self.claim_hash)
        calls = []

        def fake_api(run, digest, operation, deadline):
            calls.append(operation)
            if operation == 'stop':
                write(run / 'raw-stop-authority.json', {'output': 'fake'})
                write(run / 'raw-stop-helper-exit.json', {'helper_exit_proved': True})
            return {'sessions_absent': len(calls) == 3}

        with mock.patch.object(controller, 'api_call', side_effect=fake_api), mock.patch.object(controller.time, 'sleep'):
            result = effects.stop_owned_and_prove(time.monotonic() + 2)
        self.assertEqual(calls, ['stop', 'query', 'query'])
        self.assertTrue(result['sessions_absent'])

    def test_failed_guardian_cannot_pass_from_stale_cleanup_success_fields(self):
        guard = {'takeover': True, 'sessions_absent': True, 'active_descendants': 0}
        self.assertTrue(controller.case_passed('controller-death', 73, 0, guard, True))
        self.assertFalse(controller.case_passed('controller-death', 73, 1, guard, True))
        guard['status'] = 'UNRESOLVED; DO NOT RERUN'
        self.assertFalse(controller.case_passed('controller-death', 73, 0, guard, True))

    def test_pending_mutator_never_retried_or_accepted_as_clean(self):
        write(self.run / 'raw-stop-authority.json', {'output': 'unreaped'})
        effects = controller.Effects(None, None, self.run, self.claim_hash)
        calls = []

        def fake_api(run, digest, operation, deadline):
            calls.append(operation)
            return {'sessions_absent': True}

        ticks = iter([0, 0, .2, .4, .6, .8, 1, 2, 3, 4])
        with mock.patch.object(controller, 'api_call', side_effect=fake_api), \
             mock.patch.object(controller.time, 'monotonic', side_effect=lambda: next(ticks)), \
             mock.patch.object(controller.time, 'sleep'):
            with self.assertRaises(TimeoutError):
                effects.stop_owned_and_prove(1)
        self.assertTrue(calls)
        self.assertTrue(all(operation == 'query' for operation in calls))

    def test_unreaped_mutating_helper_leaves_no_exit_proof_and_no_retry(self):
        process = mock.Mock(pid=123, returncode=None)
        process.wait.side_effect = subprocess.TimeoutExpired('fake', 1)
        process.poll.return_value = None
        with mock.patch.object(controller, 'spawn', return_value=process) as spawn:
            with self.assertRaises(subprocess.TimeoutExpired):
                controller.api_call(self.run, self.claim_hash, 'stop', time.monotonic() + 2)
            self.assertTrue((self.run / 'raw-stop-authority.json').exists())
            self.assertFalse((self.run / 'raw-stop-helper-exit.json').exists())
            with self.assertRaises(FileExistsError):
                controller.api_call(self.run, self.claim_hash, 'stop', time.monotonic() + 2)
            self.assertEqual(spawn.call_count, 1)
        process.kill.assert_called_once()

    def test_reaped_timeout_retains_failure_and_positive_exit_proof(self):
        process = mock.Mock(pid=456, returncode=1)
        process.poll.return_value = None
        process.wait.side_effect = [subprocess.TimeoutExpired('fake', 1), 1]
        with mock.patch.object(controller, 'spawn', return_value=process):
            with self.assertRaises(subprocess.TimeoutExpired):
                controller.api_call(self.run, self.claim_hash, 'stop', time.monotonic() + 2)
        self.assertTrue(load(self.run / 'raw-stop-helper-exit.json')['helper_exit_proved'])

    def test_ambiguous_spawn_failure_never_proves_helper_exited(self):
        with mock.patch.object(controller, 'spawn', side_effect=OSError('failure after possible launch')):
            with self.assertRaises(OSError):
                controller.api_call(self.run, self.claim_hash, 'stop', time.monotonic() + 2)
        self.assertTrue((self.run / 'raw-stop-authority.json').exists())
        self.assertFalse((self.run / 'raw-stop-helper-exit.json').exists())

    def _invoke_api(self, *, parent_alive=False, active=0, sessions=None):
        nonce = 'a' * 32
        claim = {'instance': 'PD5F1Noise_' + nonce, 'parent_pid': 100, 'parent_creation': 101,
                 'job': 'Local\\CuexisPD5_' + nonce}
        output = self.run / 'api-stop-test.json'
        write(self.run / 'raw-stop-authority.json', {'claim_sha256': self.claim_hash, 'output': str(output)})
        parent = mock.Mock()
        parent.alive.return_value = parent_alive
        job = mock.Mock()
        job.active.return_value = active
        backend = mock.Mock()
        backend.prove_absent.return_value = {'sessions': sessions or [], 'owned': [], 'exact': [],
                                             'sessions_absent': True}
        with mock.patch.object(controller, 'checked_claim', return_value=(claim, self.run)), \
             mock.patch.object(controller, 'ProcessRef', return_value=parent), \
             mock.patch.object(controller.Job, 'open', return_value=job), \
             mock.patch.object(etw, 'Backend', return_value=backend):
            code = controller.api(self.run / 'claim.json', self.claim_hash, 'stop', output)
        return code, backend, load(output)

    def test_api_rejects_live_controller_before_any_raw_stop(self):
        code, backend, receipt = self._invoke_api(parent_alive=True)
        self.assertEqual(code, 1)
        backend.stop_exact.assert_not_called()
        backend.prove_absent.assert_not_called()
        self.assertIn('controller alive', receipt['error'])

    def test_api_rejects_nonzero_job_before_any_raw_stop(self):
        code, backend, receipt = self._invoke_api(active=2)
        self.assertEqual(code, 1)
        backend.stop_exact.assert_not_called()
        self.assertIn('Job not empty', receipt['error'])

    def test_unknown_case_variant_cannot_disappear_from_absence(self):
        sessions = [{'name': 'wpr_initiated_PD5F1Noise_' + 'a' * 32 + '_WPR Event Collector', 'log_file': ''}]
        code, backend, receipt = self._invoke_api(sessions=sessions)
        self.assertEqual(code, 0)
        self.assertFalse(receipt['sessions_absent'])
        self.assertEqual(len(receipt['unexpected_owned_prefix']), 1)
        backend.stop_exact.assert_not_called()

    def test_readonly_preflight_nonadmin_preserves_failure_without_backend_or_child(self):
        with mock.patch.object(etw, 'require_admin', side_effect=PermissionError('test token')), \
             mock.patch.object(etw, 'Backend') as backend, \
             mock.patch.object(controller.subprocess, 'Popen') as popen:
            with self.assertRaises(PermissionError):
                controller.preflight(self.run, {})
        backend.assert_not_called()
        popen.assert_not_called()
        receipt = load(self.run / 'preflight.json')
        self.assertFalse(receipt['wpr_start_attempted'])
        self.assertEqual(receipt['native_launches'], 0)


class EtwIdentityTests(unittest.TestCase):
    def setUp(self):
        self.trace = ROOT / 'out/etw-identity-test/trace-temp'
        self.owner = etw.Ownership('PD5F1Noise_' + 'b' * 32, self.trace)
        self.name = sorted(self.owner.names)[0]

    def test_empty_log_path_does_not_widen_exact_name_ownership(self):
        rows = [{'name': self.name, 'log_file': ''}, {'name': 'unrelated', 'log_file': ''}]
        self.assertEqual(self.owner.select(rows), rows[:1])

    def test_owned_case_variant_is_unresolved(self):
        with self.assertRaisesRegex(RuntimeError, 'case mismatch'):
            self.owner.select([{'name': self.name.lower(), 'log_file': ''}])

    def test_reported_path_outside_run_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'unexpected logfile'):
            self.owner.select([{'name': self.name, 'log_file': str(ROOT / 'out/other.etl')}])

    def test_absence_requires_inventory_and_both_exact_queries(self):
        backend = etw.Backend.__new__(etw.Backend)
        backend.sessions = mock.Mock(return_value=[])
        backend.query_exact = mock.Mock(side_effect=[None, {'name': self.name, 'log_file': ''}])
        result = backend.prove_absent(self.owner)
        self.assertFalse(result['sessions_absent'])
        self.assertEqual(backend.query_exact.call_count, 2)

    def test_inventory_failure_never_becomes_idle(self):
        backend = etw.Backend.__new__(etw.Backend)
        backend.sessions = mock.Mock(side_effect=OSError(5, 'Access denied'))
        backend.query_exact = mock.Mock()
        with self.assertRaises(OSError):
            backend.prove_absent(self.owner)
        backend.query_exact.assert_not_called()

    def test_exact_query_access_denied_not_absent(self):
        backend = etw.Backend.__new__(etw.Backend)
        backend.dll = mock.Mock()
        backend.dll.ControlTraceW.return_value = 5
        with self.assertRaises(OSError):
            backend.query_exact(self.name)
        backend.dll.ControlTraceW.return_value = 4201
        self.assertIsNone(backend.query_exact(self.name))


class LifecycleProfileTests(unittest.TestCase):
    def setUp(self):
        self.run = ROOT / 'out/pd5-etw-profile-tests' / uuid.uuid4().hex
        self.run.mkdir(parents=True)
        self.profile = ROOT / 'tools/verification/playback_performance/etw_safety/lifecycle.wprp'
        self.payload = self.profile.read_bytes()
        self.copy = self.run / 'lifecycle.wprp'
        self.copy.write_bytes(self.payload)
        self.binding = {'profile': {'file': 'lifecycle.wprp', 'name': 'PD5ToolLifecycle', 'sha256': sha(self.copy)},
                        'wpr': {'path': 'NEVER EXECUTE WPR IN UNIT TEST'}}

    def test_fixed_minimal_profile_has_two_expected_collectors(self):
        self.assertEqual(profile_argument(self.run, self.binding), str(self.copy.resolve()) + '!PD5ToolLifecycle')

    def test_profile_cannot_fall_back_to_power(self):
        self.binding['profile']['name'] = 'Power'
        with self.assertRaisesRegex(RuntimeError, 'fixed tool-lifecycle'):
            profile_argument(self.run, self.binding)

    def test_rebound_cpu_sampling_keyword_is_rejected(self):
        self.copy.write_bytes(self.payload.replace(b'Value="ProcessThread"', b'Value="CSwitch"'))
        self.binding['profile']['sha256'] = sha(self.copy)
        with self.assertRaisesRegex(RuntimeError, 'keywords/stacks'):
            profile_argument(self.run, self.binding)

    def test_tampered_xml_rejected_before_profile_parse(self):
        self.copy.write_bytes(b'not XML')
        with self.assertRaisesRegex(RuntimeError, 'hash changed'):
            profile_argument(self.run, self.binding)

    def test_preflight_uses_sealed_minimal_profile_without_starting_wpr(self):
        processes = []
        for payload in (b'SeSystemProfilePrivilege Disabled', b'unit-test-token', b'WPR is not recording', b'ProcessThread'):
            process = mock.Mock(returncode=0)
            process.communicate.return_value = (payload, None)
            processes.append(process)
        backend = mock.Mock()
        backend.sessions.return_value = []
        with mock.patch.object(etw, 'require_admin'), mock.patch.object(etw, 'Backend', return_value=backend), \
             mock.patch.object(controller, 'HERE', self.run), \
             mock.patch.object(controller.subprocess, 'Popen', side_effect=processes) as popen:
            controller.preflight(self.run, self.binding)
        argv = [call.args[0] for call in popen.call_args_list]
        self.assertEqual(argv[-1], [self.binding['wpr']['path'], '-profiledetails', str(self.copy.resolve()) + '!PD5ToolLifecycle'])
        self.assertFalse(any('-start' in row or '-stop' in row or '-cancel' in row for row in argv))


class InputPinTests(unittest.TestCase):
    def setUp(self):
        self.run = ROOT / 'out/pd5-etw-pin-tests' / uuid.uuid4().hex
        self.run.mkdir(parents=True)
        self.pinned = self.run / 'baseline.exe'
        self.pinned.write_bytes(b'sealed baseline')
        write(self.run / 'binding.json', {'schema': 'pd5.etw-tool-canary.binding.v3'})
        patch = mock.patch.object(controller, 'HERE', self.run)
        patch.start()
        self.addCleanup(patch.stop)
        self.binding = {'input_sha256': {'baseline_executable': {'path': str(self.pinned),
                                                                'sha256': sha(self.pinned)}}}

    def test_every_pinned_input_is_rehashed_before_the_case_runs(self):
        self.assertEqual(controller.verify_inputs(self.binding), 1)

    def test_a_changed_pinned_input_stops_the_case_before_it_starts(self):
        self.pinned.write_bytes(b'tampered baseline')
        with self.assertRaisesRegex(RuntimeError, 'pinned input changed: baseline_executable'):
            controller.verify_inputs(self.binding)

    def test_a_missing_pinned_input_is_not_silently_accepted(self):
        self.pinned.unlink()
        with self.assertRaisesRegex(RuntimeError, 'pinned input is missing'):
            controller.verify_inputs(self.binding)

    def test_a_binding_without_pins_cannot_capture(self):
        for binding in ({}, {'input_sha256': {}}, {'input_sha256': []}):
            with self.assertRaisesRegex(RuntimeError, 'no pinned inputs'):
                controller.verify_inputs(binding)

    def test_case_verifies_pins_before_any_job_or_capture(self):
        with mock.patch.object(controller, 'Job', mock.Mock(side_effect=AssertionError('job created'))), \
             mock.patch.object(controller, 'verify_inputs', side_effect=RuntimeError('pinned input changed')) as verify:
            with self.assertRaisesRegex(RuntimeError, 'pinned input changed'):
                controller.case(self.run, 'f1-scale', self.binding)
        verify.assert_called_once_with(self.binding)
        self.assertFalse((self.run / 'input-pins.json').exists())

    def test_case_writes_the_verified_pin_count(self):
        claim = self.run / 'case-run'
        claim.mkdir()
        with mock.patch.object(controller.Job, 'create', side_effect=RuntimeError('stop after pins')):
            with self.assertRaisesRegex(RuntimeError, 'stop after pins'):
                controller.case(claim, 'f1-scale', self.binding)
        receipt = load(claim / 'input-pins.json')
        self.assertTrue(receipt['verified_before_start'])
        self.assertEqual(receipt['pinned_inputs'], 1)


if __name__ == '__main__':
    unittest.main(verbosity=2)
