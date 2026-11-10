"""F1 scale tool-acceptance tests. No real ETW, WPR, administrator, collector or Cuexis call."""
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

ROOT = Path(__file__).resolve().parents[2]
PACKAGE = ROOT / 'tools/verification/playback_performance'
SEALED = PACKAGE / 'etw_safety'
sys.path.insert(0, str(SEALED))
sys.path.insert(0, str(PACKAGE))
import cases as case_table
import controller
import etw
import f1_scale
import prepare_etw_tools
import profile_config
from common import load, write
from common import sha


class CaseTableTests(unittest.TestCase):
    def setUp(self):
        self.plan = json.loads((SEALED / 'plan.json').read_text(encoding='utf-8'))

    def test_sealed_plan_agrees_with_the_fixed_case_table(self):
        prepare_etw_tools.cross_check(self.plan, case_table, profile_config)
        self.assertEqual(self.plan['preparation_revision'], 4)
        self.assertTrue(self.plan['capture_enabled'])
        self.assertFalse(self.plan['formal'])
        self.assertIn('NO FORMAL MEASUREMENT', self.plan['review_status'])

    def test_plan_drift_is_a_failed_preparation(self):
        for key, value in (('hard_deadline_seconds', 421), ('guardian_emergency_seconds', 379),
                           ('disk_emergency_bytes', 134217728), ('stop_cli_timeout_seconds', 60),
                           ('start_cli_timeout_seconds', 15), ('collector_budget_seconds', 249)):
            broken = json.loads(json.dumps(self.plan))
            broken['wpr'][key] = value
            with self.assertRaises(RuntimeError, msg=key):
                prepare_etw_tools.cross_check(broken, case_table, profile_config)

    def test_hard_deadline_single_drift_is_refused(self):
        self.plan['wpr']['hard_deadline_seconds'] += 1
        with self.assertRaisesRegex(RuntimeError, 'F1 hard deadline'):
            prepare_etw_tools.cross_check(self.plan, case_table, profile_config)

    def test_bounded_capture_single_drift_is_refused(self):
        self.plan['wpr']['bounded_capture_seconds'] += 1
        with self.assertRaisesRegex(RuntimeError, 'F1 hard deadline'):
            prepare_etw_tools.cross_check(self.plan, case_table, profile_config)

    def test_case_deadline_single_drift_is_refused(self):
        original = case_table.spec

        def changed(name):
            value = dict(original(name))
            if name == 'f1-scale':
                value['deadline_seconds'] += 1
            return value

        with mock.patch.object(case_table, 'spec', side_effect=changed):
            with self.assertRaisesRegex(RuntimeError, 'F1 hard deadline'):
                prepare_etw_tools.cross_check(self.plan, case_table, profile_config)

    def test_workload_and_profile_drift_are_refused(self):
        for mutate in (lambda p: p['workload'].__setitem__('native_launches', 5),
                       lambda p: p['workload'].__setitem__('processes', 2),
                       lambda p: p['wpr'].__setitem__('profile', 'PD5ToolLifecycle'),
                       lambda p: p['wpr'].__setitem__('required_profile_details_tokens', ['CSwitch']),
                       lambda p: p.__setitem__('formal', True),
                       lambda p: p.__setitem__('capture_enabled', False)):
            broken = json.loads(json.dumps(self.plan))
            mutate(broken)
            with self.assertRaises(RuntimeError):
                prepare_etw_tools.cross_check(broken, case_table, profile_config)

    def test_f1_cases_share_one_envelope_and_never_claim_acceptance(self):
        names = list(case_table.F1_CASES)
        self.assertEqual(names, ['f1-scale', 'f1-scale-timeout', 'f1-scale-death'])
        envelopes = [case_table.envelope(name) for name in names]
        for entry in envelopes:
            self.assertFalse(entry['formal_acceptance'])
            self.assertFalse(entry['performance_acceptance'])
            self.assertEqual(entry['profile'], 'f1-scale')
            self.assertIsNone(entry['active_logger_cap_seconds'])
        differing = [{key: value for key, value in entry.items()
                      if key not in ('workload_budget_seconds', 'exit_codes')} for entry in envelopes]
        self.assertEqual(differing[0], differing[1])
        self.assertEqual(differing[0], differing[2])

    def test_case_lookup_fails_closed(self):
        with self.assertRaisesRegex(RuntimeError, 'unknown fixed tool case'):
            case_table.spec('not-a-case')
        with self.assertRaisesRegex(RuntimeError, 'unknown fixed tool profile'):
            case_table.cases_for('f1')


class ClaimContractTests(unittest.TestCase):
    def setUp(self):
        self.run = ROOT / 'out/pd5-etw-f1-mock-tests' / uuid.uuid4().hex
        self.run.mkdir(parents=True)
        self.binding_dir = self.run / 'binding'
        self.binding_dir.mkdir()
        write(self.binding_dir / 'binding.json', {'schema': 'pd5.etw-tool-canary.binding.v3'})
        self.serial = 0

    def claim(self, mode, started=1000.0, **override):
        spec = case_table.spec(mode)
        nonce = 'a' * 32
        claim = {'instance': 'PD5F1Noise_' + nonce, 'nonce': nonce,
                 'job': 'Local\\CuexisPD5_' + nonce, 'case': mode, 'profile': spec['profile'],
                 'started': started, 'deadline': started + spec['deadline_seconds'],
                 'takeover': started + spec['takeover_seconds'],
                 'disk_cap_bytes': spec['disk_cap_bytes'],
                 'active_logger_cap_seconds': spec['active_logger_cap_seconds'],
                 'binding_sha256': sha(self.binding_dir / 'binding.json')}
        if spec['workload'] == 'none':
            claim['native_launches'] = 0
        claim.update(override)
        return claim

    def write_claim(self, claim):
        # write() is create-new, so every claim gets its own run directory.
        self.serial += 1
        directory = self.run / ('claim-' + str(self.serial))
        directory.mkdir()
        path = directory / 'claim.json'
        write(path, claim)
        return directory, path, sha(path)

    def accepts(self, claim):
        directory, path, digest = self.write_claim(claim)
        with mock.patch.object(controller, 'HERE', self.binding_dir), \
                mock.patch.object(etw, 'Ownership', return_value=mock.Mock()):
            checked, root = controller.checked_claim(path, digest)
        self.assertEqual(root, directory.resolve())
        return checked

    def refuses(self, claim, message):
        directory, path, digest = self.write_claim(claim)
        with mock.patch.object(controller, 'HERE', self.binding_dir), \
                mock.patch.object(etw, 'Ownership', return_value=mock.Mock()):
            with self.assertRaisesRegex(RuntimeError, message):
                controller.checked_claim(path, digest)

    def test_every_fixed_case_claim_is_accepted(self):
        for mode in case_table.CASES:
            checked = self.accepts(self.claim(mode))
            self.assertEqual(checked['case'], mode)

    def test_claim_must_match_its_own_hash(self):
        directory, path, digest = self.write_claim(self.claim('f1-scale'))
        with mock.patch.object(controller, 'HERE', self.binding_dir):
            with self.assertRaisesRegex(RuntimeError, 'hash/path mismatch'):
                controller.checked_claim(path, 'f' * 64)

    def test_claim_cannot_rename_instance_or_job(self):
        self.refuses(self.claim('f1-scale', instance='PD5F1Noise_' + 'b' * 32), 'instance mismatch')
        self.refuses(self.claim('f1-scale', job='Local\\CuexisPD5_' + 'b' * 32), 'Job mismatch')

    def test_claim_profile_must_be_the_table_profile(self):
        self.refuses(self.claim('f1-scale', profile='canary'),
                     'profile differs from the fixed case table')
        self.refuses(self.claim('normal', profile='f1-scale'),
                     'profile differs from the fixed case table')

    def test_claim_is_bound_to_one_preparation(self):
        self.refuses(self.claim('f1-scale', binding_sha256='0' * 64),
                     'bound to a different tool preparation')

    def test_claim_cannot_widen_any_envelope_number(self):
        self.refuses(self.claim('f1-scale', deadline=1000.0 + 421), 'unexpected case deadline')
        self.refuses(self.claim('f1-scale', takeover=1000.0 + 379), 'unexpected case takeover point')
        self.refuses(self.claim('f1-scale', disk_cap_bytes=134217728), 'unexpected case disk cap')
        self.refuses(self.claim('normal', active_logger_cap_seconds=120),
                     'unexpected case active-logger cap')

    def test_native_launch_count_rules_differ_by_profile(self):
        self.refuses(self.claim('normal', native_launches=6), 'not a fixed no-workload case')
        self.refuses(self.claim('f1-scale', native_launches=6),
                     'never pre-declare a native launch count')

    def test_unknown_case_is_refused_before_any_envelope(self):
        self.refuses(self.claim('f1-scale', case='f1-scale-extra'), 'unknown fixed tool case')


class F1ReceiptTests(unittest.TestCase):
    def setUp(self):
        self.run = ROOT / 'out/pd5-etw-f1-mock-tests' / uuid.uuid4().hex
        self.run.mkdir(parents=True)
        self.sealed = self.run / 'sealed'
        self.sealed.mkdir()
        for name in ('controller.py', 'plan.json', 'collect_f1_scale.py'):
            (self.sealed / name).write_bytes((SEALED / name).read_bytes())
        write(self.sealed / 'binding.json', {'schema': 'pd5.etw-tool-canary.binding.v3'})
        self.binding = {'controller_source_sha256': sha(self.sealed / 'controller.py'),
                        'wpr': {'path': 'NEVER EXECUTE WPR IN UNIT TEST'}}
        self.claim = {'instance': 'PD5F1Noise_' + 'c' * 32, 'case': 'f1-scale',
                      'deadline': time.monotonic() + 420.0,
                      'workload_budget_seconds': 300}

    def test_receipt_names_the_sealed_controller_and_the_claim(self):
        with mock.patch.object(f1_scale, 'HERE', self.sealed):
            receipt = f1_scale.write_receipt(self.run, self.claim, 'd' * 64, self.binding)
        self.assertEqual(receipt['schema'], 'pd5.f1-controller-capture.v1')
        self.assertEqual(receipt['status'], 'OWNED WPR CAPTURE ACTIVE')
        self.assertTrue(receipt['wpr_start_succeeded'])
        self.assertEqual(receipt['wpr_instance'], self.claim['instance'])
        self.assertEqual(receipt['controller_source_sha256'], self.binding['controller_source_sha256'])
        self.assertEqual(receipt['controller_claim_sha256'], 'd' * 64)
        self.assertEqual(receipt['plan_sha256'], sha(self.sealed / 'plan.json'))
        self.assertEqual(receipt['binding_sha256'], sha(self.sealed / 'binding.json'))
        self.assertFalse(receipt['formal_acceptance'])
        self.assertFalse(receipt['performance_acceptance'])
        self.assertEqual(load(self.run / f1_scale.CAPTURE_RECEIPT), receipt)

    def test_a_foreign_controller_cannot_issue_the_receipt(self):
        self.binding['controller_source_sha256'] = 'e' * 64
        with mock.patch.object(f1_scale, 'HERE', self.sealed):
            with self.assertRaisesRegex(RuntimeError, 'not the sealed controller'):
                f1_scale.write_receipt(self.run, self.claim, 'd' * 64, self.binding)
        self.assertFalse((self.run / f1_scale.CAPTURE_RECEIPT).exists())

    def test_collector_command_is_sealed_python_bound_to_the_claim(self):
        native = self.run / 'f1-native'
        with mock.patch.object(f1_scale, 'HERE', self.sealed):
            argv = f1_scale.collector_command(self.run, self.claim, 'd' * 64, self.binding, native)
        self.assertEqual(argv[1:5], ['-B', '-I', '-S', '-u'])
        self.assertEqual(argv[5], str(self.sealed / 'collect_f1_scale.py'))
        self.assertEqual(argv[argv.index('--output') + 1], str(native))
        self.assertEqual(argv[argv.index('--capture-receipt') + 1],
                         str(self.run / f1_scale.CAPTURE_RECEIPT))
        self.assertEqual(argv[argv.index('--wpr-instance') + 1], self.claim['instance'])
        self.assertEqual(argv[argv.index('--controller-claim-sha256') + 1], 'd' * 64)
        self.assertEqual(argv[argv.index('--wpr') + 1], self.binding['wpr']['path'])


class _Child:
    def __init__(self, code=0, pid=4321, live_polls=0):
        self.pid = pid
        self._code = code
        self._live_polls = live_polls

    def poll(self):
        if self._live_polls:
            self._live_polls -= 1
            return None
        return self._code

    def wait(self, timeout=None):
        return 0 if self._code is None else self._code

    def kill(self):
        pass


class _Guard:
    def poll(self):
        return None


class _Job:
    def __init__(self, active=(0,), spawn=None):
        self._active = list(active)
        self._spawn = spawn
        self.commands = []
        self.drained = False

    def spawn(self, argv, stream):
        self.commands.append(argv)
        return self._spawn(argv) if self._spawn else _Child()

    def active(self):
        return self._active.pop(0) if len(self._active) > 1 else self._active[0]

    def terminate_and_drain(self, deadline):
        self.drained = True


class _Ref:
    def __init__(self, pid, alive, creation_time=987654):
        self.pid = pid
        self.creation_time = creation_time
        self._alive = list(alive)
        self.closed = False

    def alive(self):
        return self._alive.pop(0) if len(self._alive) > 1 else self._alive[0]

    def close(self):
        self.closed = True


class _Clock:
    def __init__(self, start=0.0, step=1.0):
        self.now = start
        self.step = step

    def __call__(self):
        value = self.now
        self.now += self.step
        return value


class F1WorkloadTests(unittest.TestCase):
    def setUp(self):
        self.run = ROOT / 'out/pd5-etw-f1-mock-tests' / uuid.uuid4().hex
        self.run.mkdir(parents=True)
        self.sealed = self.run / 'sealed'
        self.sealed.mkdir()
        for name in ('controller.py', 'plan.json', 'collect_f1_scale.py'):
            (self.sealed / name).write_bytes((SEALED / name).read_bytes())
        write(self.sealed / 'binding.json', {'schema': 'pd5.etw-tool-canary.binding.v3'})
        self.binding = {'controller_source_sha256': sha(self.sealed / 'controller.py'),
                        'wpr': {'path': 'NEVER EXECUTE WPR IN UNIT TEST'}}
        # Identity capture and the re-check both cross a real process boundary; the
        # workload tests drive the surrounding logic with an explicit stand-in.
        self.identity_state = 'absent'
        self.patches = [mock.patch.object(f1_scale, 'HERE', self.sealed),
                        mock.patch.object(f1_scale, 'observe_identities', self.observe),
                        mock.patch.object(f1_scale.ProcessRef, 'acquire',
                                          lambda pid, creation_time: self.acquire(pid, creation_time))]
        for patch in self.patches:
            patch.start()
            self.addCleanup(patch.stop)

    def observe(self, native, seen):
        for record in f1_scale.all_start_records(native):
            seen[record['pid']] = {'pid': record['pid'], 'creation_time': 9000 + record['pid']}
        return seen

    def acquire(self, pid, creation_time):
        return None if self.identity_state == 'absent' else _Ref(pid, [True])

    def claim(self, mode='f1-scale', **override):
        claim = {'instance': 'PD5F1Noise_' + 'c' * 32, 'case': mode,
                 'started': time.monotonic(), 'deadline': time.monotonic() + 420.0}
        claim['deadline'] = claim['started'] + 420.0
        claim['workload_budget_seconds'] = case_table.spec(mode)['workload_budget_seconds']
        claim.update(override)
        return claim

    def fixture(self, records=6, status='COMPLETE', processes=3):
        def spawn(argv):
            native = Path(argv[argv.index('--output') + 1])
            lines = ['noise'] + [f1_scale.MARKER + json.dumps({'pid': 5000 + index, 'qpc': index,
                                                               'frequency': 1000})
                                 for index in range(records)]
            (native / 'f1-scale-0.log').write_text('\n'.join(lines) + '\n', encoding='utf-8')
            write(native / 'collector-result.json', {'status': status, 'process_count_completed': processes,
                                                     'native_start_records': records, 'failures': []})
            # Captured once right after the spawn and again on every supervisor
            # poll, exactly as it happens against a real collector.
            return _Child()
        return spawn

    def test_completed_workload_records_every_launch_and_reclaims(self):
        job = _Job(spawn=self.fixture())
        outcome = f1_scale.workload(job, _Guard(), self.run, self.claim(), 'd' * 64, self.binding)
        self.assertEqual(outcome, {'status': 'COMPLETE', 'exit_code': 0, 'native_launches': 6,
                                   'reclamation_proved': True, 'native_liveness_proved': True})
        record = load(self.run / 'f1-workload.json')
        self.assertEqual(record['native_pids'], [5000, 5001, 5002, 5003, 5004, 5005])
        self.assertEqual(record['process_count_completed'], 3)
        self.assertFalse(record['formal_acceptance'])
        self.assertEqual(len(record['native_logs']), 1)
        intent = load(self.run / 'f1-collector-intent.json')
        self.assertEqual(intent['supervisor_budget_seconds'], 300)
        self.assertEqual(intent['receipt_sha256'], sha(self.run / f1_scale.CAPTURE_RECEIPT))
        self.assertEqual(len(job.commands), 1)
        identities = load(self.run / f1_scale.IDENTITIES)
        self.assertEqual(len(identities['captured_while_alive']), 6)
        self.assertFalse(identities['handles_retained'])
        reclaimed = load(self.run / 'f1-native-reclaimed.json')
        self.assertTrue(reclaimed['proved_dead'])
        self.assertEqual(reclaimed['native_pids_alive_after_collector'], [])

    def test_a_native_that_was_never_identified_fails_instead_of_passing(self):
        with mock.patch.object(f1_scale, 'observe_identities', lambda native, seen: seen):
            job = _Job(spawn=self.fixture())
            with self.assertRaisesRegex(RuntimeError, 'death cannot be proved'):
                f1_scale.workload(job, _Guard(), self.run, self.claim(), 'd' * 64, self.binding)
        self.assertFalse((self.run / 'f1-workload.json').exists())
        self.assertFalse((self.run / 'f1-native-reclaimed.json').exists())
        identities = load(self.run / f1_scale.IDENTITIES)
        self.assertEqual(identities['captured_while_alive'], [])

    def test_a_live_native_identity_fails_the_workload(self):
        self.identity_state = 'live'
        job = _Job(spawn=self.fixture())
        with self.assertRaisesRegex(RuntimeError, 'survived the sealed collector'):
            f1_scale.workload(job, _Guard(), self.run, self.claim(), 'd' * 64, self.binding)
        reclaimed = load(self.run / 'f1-native-reclaimed.json')
        self.assertFalse(reclaimed['proved_dead'])
        self.assertEqual(reclaimed['native_pids_alive_after_collector'], [5000, 5001, 5002, 5003, 5004, 5005])
        self.assertFalse((self.run / 'f1-workload.json').exists())

    def test_completed_workload_requires_every_declared_launch(self):
        for records, processes, status, message in (
                (5, 3, 'COMPLETE', 'launch count differs'),
                (6, 2, 'COMPLETE', 'unexpected process count'),
                (6, 3, 'FAILED', 'did not complete')):
            run = self.run / ('case-' + str(records) + '-' + str(processes) + '-' + status)
            run.mkdir()
            job = _Job(spawn=self.fixture(records=records, status=status, processes=processes))
            with self.assertRaisesRegex(RuntimeError, message):
                f1_scale.workload(job, _Guard(), run, self.claim(), 'd' * 64, self.binding)
            self.assertFalse((run / 'f1-workload.json').exists())

    def test_native_logs_must_contain_the_sealed_launch_count(self):
        def spawn(argv):
            native = Path(argv[argv.index('--output') + 1])
            (native / 'f1-scale-0.log').write_text(
                f1_scale.MARKER + json.dumps({'pid': 5000}) + '\n', encoding='utf-8')
            write(native / 'collector-result.json', {'status': 'COMPLETE', 'process_count_completed': 3,
                                                     'native_start_records': 6, 'failures': []})
            return _Child()
        with self.assertRaisesRegex(RuntimeError, 'do not contain the sealed launch count'):
            f1_scale.workload(_Job(spawn=spawn), _Guard(), self.run, self.claim(), 'd' * 64, self.binding)

    def test_a_nonzero_collector_exit_is_a_failure(self):
        with self.assertRaisesRegex(RuntimeError, 'exited nonzero'):
            f1_scale.workload(_Job(spawn=lambda argv: _Child(3)), _Guard(), self.run,
                              self.claim(), 'd' * 64, self.binding)

    def test_live_job_descendants_after_collector_exit_are_a_failure(self):
        run = self.run / 'descendants'
        run.mkdir()
        claim = self.claim()
        with mock.patch.object(f1_scale.time, 'monotonic', side_effect=_Clock(step=400)), \
                mock.patch.object(f1_scale.time, 'sleep'):
            with self.assertRaisesRegex(RuntimeError, 'live Job descendants'):
                f1_scale.workload(_Job(active=(1,), spawn=self.fixture()), _Guard(), run,
                                  claim, 'd' * 64, self.binding)
        self.assertFalse((run / 'f1-workload.json').exists())

    def test_guardian_death_during_workload_is_not_recovery(self):
        class Dead(_Guard):
            def poll(self):
                return 1
        with self.assertRaisesRegex(RuntimeError, 'guardian died'):
            f1_scale.workload(_Job(spawn=self.fixture()), Dead(), self.run, self.claim(),
                              'd' * 64, self.binding)

    def test_death_case_dispatches_before_the_budget(self):
        seen = {}
        sentinel = {'status': 'DEATH BOUNDARY', 'exit_code': 73}

        def fake_death(run, job, guard, child, claim, native):
            seen['run'] = Path(run)
            return sentinel

        with mock.patch.object(f1_scale, '_die_with_live_native', side_effect=fake_death):
            outcome = f1_scale.workload(_Job(spawn=self.fixture()), _Guard(), self.run,
                                        self.claim('f1-scale-death'), 'd' * 64, self.binding)
        self.assertEqual(outcome, sentinel)
        self.assertEqual(seen['run'], self.run)
        self.assertTrue((self.run / 'f1-collector-spawn.json').exists())
        self.assertEqual(len(load(self.run / 'f1-collector-spawn.json')), 2)

    def test_a_case_without_a_bounded_workload_is_refused(self):
        with self.assertRaisesRegex(RuntimeError, 'no bounded F1 workload'):
            f1_scale.workload(_Job(), _Guard(), self.run, self.claim('normal'), 'd' * 64, self.binding)


class F1TimeoutTests(unittest.TestCase):
    def setUp(self):
        self.run = ROOT / 'out/pd5-etw-f1-mock-tests' / uuid.uuid4().hex
        self.native = self.run / 'f1-native'
        self.native.mkdir(parents=True)
        records = [{'pid': 6000, 'qpc': 1, 'frequency': 1000},
                   {'pid': 6001, 'qpc': 2, 'frequency': 1000}]
        (self.native / 'f1-scale-0.log').write_text(
            ''.join(f1_scale.MARKER + json.dumps(record) + '\n' for record in records),
            encoding='utf-8')

    def claim(self):
        return {'case': 'f1-scale-timeout', 'started': 0.0, 'deadline': 100.0,
                'workload_budget_seconds': 20}

    def test_timeout_kills_then_proves_both_job_and_native_death(self):
        reference = _Ref(6000, [True, False])
        job = _Job(active=(1, 0))
        with mock.patch.object(f1_scale, 'live_reference',
                               side_effect=lambda pid: reference if pid == 6000 else None), \
                mock.patch.object(f1_scale.time, 'monotonic', side_effect=_Clock()), \
                mock.patch.object(f1_scale.time, 'sleep'):
            outcome = f1_scale._expire(self.run, job, _Guard(), _Child(), self.claim(),
                                       self.native, 20)
        self.assertEqual(outcome['exit_code'], 74)
        self.assertEqual(outcome['status'], 'F1 WORKLOAD TIMEOUT; RECLAIMED; ORDERLY STOP COMPLETED')
        self.assertTrue(outcome['reclamation_proved'])
        self.assertTrue(outcome['native_liveness_proved'])
        self.assertEqual(outcome['native_pids'], [6000, 6001])
        self.assertTrue(job.drained)
        self.assertTrue(reference.closed)
        expiry = load(self.run / 'f1-workload-timeout.json')
        self.assertEqual(expiry['native_pids_alive_at_kill'], [6000])
        self.assertEqual(expiry['active_job_processes_at_expiry'], 1)
        self.assertTrue(expiry['native_liveness_proved'])
        reclamation = load(self.run / 'f1-reclamation.json')
        self.assertEqual(reclamation['job_active_after_drain'], 0)
        self.assertEqual(reclamation['native_pids_alive_after_drain'], [])
        self.assertTrue(reclamation['proved_dead'])

    def test_a_surviving_native_process_fails_the_timeout_case(self):
        reference = _Ref(6000, [True, True])
        job = _Job(active=(1, 0))
        with mock.patch.object(f1_scale, 'live_reference',
                               side_effect=lambda pid: reference if pid == 6000 else None), \
                mock.patch.object(f1_scale.time, 'monotonic', side_effect=_Clock()), \
                mock.patch.object(f1_scale.time, 'sleep'):
            with self.assertRaisesRegex(RuntimeError, 'reclamation unproved'):
                f1_scale._expire(self.run, job, _Guard(), _Child(), self.claim(), self.native, 20)
        self.assertEqual(load(self.run / 'f1-reclamation.json')['native_pids_alive_after_drain'],
                         [6000])

    def test_a_live_job_after_drain_fails_the_timeout_case(self):
        reference = _Ref(6000, [True, False])
        with mock.patch.object(f1_scale, 'live_reference',
                               side_effect=lambda pid: reference if pid == 6000 else None), \
                mock.patch.object(f1_scale.time, 'monotonic', side_effect=_Clock()), \
                mock.patch.object(f1_scale.time, 'sleep'):
            with self.assertRaisesRegex(RuntimeError, 'reclamation unproved'):
                f1_scale._expire(self.run, _Job(active=(1, 1)), _Guard(), _Child(),
                                 self.claim(), self.native, 20)


class F1DeathBoundaryTests(unittest.TestCase):
    def setUp(self):
        self.run = ROOT / 'out/pd5-etw-f1-mock-tests' / uuid.uuid4().hex
        self.native = self.run / 'f1-native'
        self.native.mkdir(parents=True)

    def claim(self):
        return {'case': 'f1-scale-death', 'started': time.monotonic(),
                'deadline': time.monotonic() + 420.0, 'workload_budget_seconds': 300}

    def test_death_boundary_records_the_live_native_identity(self):
        (self.native / 'f1-scale-0.log').write_text(
            f1_scale.MARKER + json.dumps({'pid': 7000}) + '\n', encoding='utf-8')
        reference = _Ref(7000, [True])
        exits = []

        class Stop(Exception):
            pass

        def fake_exit(code):
            exits.append(code)
            raise Stop()

        with mock.patch.object(f1_scale, 'live_reference', side_effect=lambda pid: reference), \
                mock.patch.object(f1_scale.os, '_exit', side_effect=fake_exit):
            with self.assertRaises(Stop):
                f1_scale._die_with_live_native(self.run, _Job(), _Guard(), _Child(None),
                                               self.claim(), self.native)
        self.assertEqual(exits, [73])
        self.assertTrue(reference.closed)
        receipt = load(self.run / 'f1-native-liveness-at-death.json')
        self.assertEqual(receipt['native_pid'], 7000)
        self.assertEqual(receipt['native_creation_time'], 987654)
        self.assertTrue(receipt['before_stop_request'])
        self.assertEqual(receipt['exit_code'], 73)
        self.assertFalse((self.run / 'f1-reclamation.json').exists())

    def test_death_boundary_also_declares_the_intended_controller_death(self):
        (self.native / 'f1-scale-0.log').write_text(
            f1_scale.MARKER + json.dumps({'pid': 7002}) + '\n', encoding='utf-8')
        reference = _Ref(7002, [True])

        class Stop(Exception):
            pass

        with mock.patch.object(f1_scale, 'live_reference', side_effect=lambda pid: reference), \
                mock.patch.object(f1_scale.os, '_exit', side_effect=Stop()):
            with self.assertRaises(Stop):
                f1_scale._die_with_live_native(self.run, _Job(), _Guard(), _Child(None),
                                               self.claim(), self.native)
        # An exit code alone cannot separate an intended death from a crash, so the F1
        # death case must publish the same intent receipt the canary publishes.
        intent = load(self.run / 'expected-controller-death.json')
        self.assertTrue(intent['before_stop_request'])
        self.assertEqual(intent['exit_code'], 73)
        self.assertEqual(intent['native_pid'], 7002)
        self.assertEqual(intent['native_creation_time'], 987654)

    def test_no_live_native_process_is_a_failure_not_a_death_proof(self):
        (self.native / 'f1-scale-0.log').write_text(
            f1_scale.MARKER + json.dumps({'pid': 7001}) + '\n', encoding='utf-8')
        claim = self.claim()
        with mock.patch.object(f1_scale, 'live_reference', return_value=None), \
                mock.patch.object(f1_scale.time, 'monotonic', side_effect=[0, 2e9]), \
                mock.patch.object(f1_scale.time, 'sleep'):
            with self.assertRaisesRegex(RuntimeError, 'no live native F1 process'):
                f1_scale._die_with_live_native(self.run, _Job(), _Guard(), _Child(None),
                                               claim, self.native)
        self.assertFalse((self.run / 'f1-native-liveness-at-death.json').exists())


class ReclaimedDeathTests(unittest.TestCase):
    """The post-recovery proof must read a vanished PID as death, not as an error."""

    def setUp(self):
        self.run = ROOT / 'out/pd5-etw-f1-mock-tests' / uuid.uuid4().hex
        self.run.mkdir(parents=True)
        self.death = {'native_pid': 7003, 'native_creation_time': 424242}

    def test_absent_identity_proves_the_native_died(self):
        with mock.patch.object(controller.ProcessRef, 'acquire', return_value=None):
            receipt = controller.death_proof(self.run, self.death)
        self.assertTrue(receipt['proved_dead'])
        self.assertEqual(receipt['identity_now'], 'absent')
        self.assertFalse(receipt['alive_after_controller_and_guardian'])
        self.assertEqual(load(self.run / 'f1-native-reclaimed.json'), receipt)

    def test_a_live_identity_fails_the_case_and_is_recorded(self):
        reference = _Ref(7003, [True])
        with mock.patch.object(controller.ProcessRef, 'acquire', return_value=reference):
            with self.assertRaisesRegex(RuntimeError, 'survived controller-death recovery'):
                controller.death_proof(self.run, self.death)
        self.assertTrue(reference.closed)
        receipt = load(self.run / 'f1-native-reclaimed.json')
        self.assertFalse(receipt['proved_dead'])
        self.assertEqual(receipt['identity_now'], 'live')

    def test_a_refused_identity_is_not_read_as_death(self):
        with mock.patch.object(controller.ProcessRef, 'acquire',
                               side_effect=OSError(22, 'OpenProcess', None, 5)):
            with self.assertRaises(OSError):
                controller.death_proof(self.run, self.death)
        self.assertFalse((self.run / 'f1-native-reclaimed.json').exists())


class StartRecordTests(unittest.TestCase):
    def setUp(self):
        self.run = ROOT / 'out/pd5-etw-f1-mock-tests' / uuid.uuid4().hex
        self.run.mkdir(parents=True)

    def test_only_well_formed_anchors_are_counted(self):
        log = self.run / 'f1-scale-0.log'
        log.write_text('\n'.join([
            'plain output',
            f1_scale.MARKER + '{"pid": 11, "qpc": 1, "frequency": 1000}',
            'prefix ' + f1_scale.MARKER + '{"pid": 12}',
            f1_scale.MARKER + 'not json',
            f1_scale.MARKER + '{"pid": "13"}',
            f1_scale.MARKER + '{"pid": 0}',
            f1_scale.MARKER + '{"pid": 14',
        ]) + '\n', encoding='utf-8')
        self.assertEqual([record['pid'] for record in f1_scale.start_records(log)], [11, 12])
        self.assertEqual(f1_scale.start_records(self.run / 'missing.log'), [])

    def test_records_are_read_in_file_order(self):
        for index, pid in enumerate((21, 22)):
            (self.run / ('f1-scale-' + str(index) + '.log')).write_text(
                f1_scale.MARKER + json.dumps({'pid': pid}) + '\n', encoding='utf-8')
        self.assertEqual([record['pid'] for record in f1_scale.all_start_records(self.run)], [21, 22])

    def test_absent_process_yields_no_live_reference(self):
        self.assertIsNone(f1_scale.live_reference(0))


class F1ProfileTests(unittest.TestCase):
    def setUp(self):
        self.run = ROOT / 'out/pd5-etw-f1-mock-tests' / uuid.uuid4().hex
        self.run.mkdir(parents=True)
        self.copy = self.run / 'f1_scale.wprp'
        self.payload = (SEALED / 'f1_scale.wprp').read_bytes()
        self.copy.write_bytes(self.payload)
        self.binding = {'profiles': {
            'canary': {'file': 'lifecycle.wprp', 'name': 'PD5ToolLifecycle', 'sha256': '0' * 64},
            'f1-scale': {'file': 'f1_scale.wprp', 'name': 'PD5F1Scale', 'sha256': sha(self.copy)}},
            'wpr': {'path': 'NEVER EXECUTE WPR IN UNIT TEST'}}

    def test_sealed_f1_profile_is_selected_by_name(self):
        self.assertEqual(profile_config.profile_argument(self.run, self.binding, 'f1-scale'),
                         str(self.copy.resolve()) + '!PD5F1Scale')

    def test_unknown_profile_key_is_refused(self):
        with self.assertRaisesRegex(RuntimeError, 'unknown fixed profile'):
            profile_config.profile_argument(self.run, self.binding, 'canary2')

    def test_f1_profile_cannot_fall_back_to_the_lifecycle_profile(self):
        self.binding['profiles']['f1-scale']['name'] = 'PD5ToolLifecycle'
        with self.assertRaisesRegex(RuntimeError, 'fixed F1 scale'):
            profile_config.profile_argument(self.run, self.binding, 'f1-scale')

    def test_tampered_f1_xml_is_rejected_before_parse(self):
        self.copy.write_bytes(b'not XML')
        with self.assertRaisesRegex(RuntimeError, 'F1 scale profile hash changed'):
            profile_config.profile_argument(self.run, self.binding, 'f1-scale')

    def test_rebound_processor_power_keyword_is_rejected(self):
        for old, new in ((b'0x2ff', b'0xff'), (b'CSwitch', b'ProcessThread')):
            self.copy.write_bytes(self.payload.replace(old, new, 1))
            self.binding['profiles']['f1-scale']['sha256'] = sha(self.copy)
            with self.assertRaisesRegex(RuntimeError, 'F1 scale'):
                profile_config.profile_argument(self.run, self.binding, 'f1-scale')


class PreflightProfileTests(unittest.TestCase):
    def setUp(self):
        self.run = ROOT / 'out/pd5-etw-f1-mock-tests' / uuid.uuid4().hex
        self.run.mkdir(parents=True)
        for name in ('lifecycle.wprp', 'f1_scale.wprp'):
            (self.run / name).write_bytes((SEALED / name).read_bytes())
        self.binding = {'profiles': {
            'canary': {'file': 'lifecycle.wprp', 'name': 'PD5ToolLifecycle',
                       'sha256': sha(self.run / 'lifecycle.wprp')},
            'f1-scale': {'file': 'f1_scale.wprp', 'name': 'PD5F1Scale',
                         'sha256': sha(self.run / 'f1_scale.wprp')}},
            'wpr': {'path': 'NEVER EXECUTE WPR IN UNIT TEST'}}

    def preflight(self, details):
        processes = []
        for payload in (b'SeSystemProfilePrivilege Disabled', b'unit-test-token',
                        b'WPR is not recording', details):
            process = mock.Mock(returncode=0)
            process.communicate.return_value = (payload, None)
            processes.append(process)
        backend = mock.Mock()
        backend.sessions.return_value = []
        with mock.patch.object(etw, 'require_admin'), \
                mock.patch.object(etw, 'Backend', return_value=backend), \
                mock.patch.object(controller, 'HERE', self.run), \
                mock.patch.object(controller.subprocess, 'Popen', side_effect=processes) as popen:
            controller.preflight(self.run, self.binding, 'f1-scale')
        return [call.args[0] for call in popen.call_args_list]

    def test_f1_preflight_reads_the_sealed_f1_profile_only(self):
        argv = self.preflight(b'CSwitch ReadyThread DPC Interrupt Microsoft-Windows-Kernel-Processor-Power')
        self.assertEqual(argv[-1], ['NEVER EXECUTE WPR IN UNIT TEST', '-profiledetails',
                                    str((self.run / 'f1_scale.wprp').resolve()) + '!PD5F1Scale'])
        self.assertFalse(any('-start' in row or '-stop' in row or '-cancel' in row for row in argv))

    def test_f1_preflight_fails_when_a_required_marker_is_missing(self):
        with self.assertRaisesRegex(RuntimeError, 'sealed f1-scale profile missing CSwitch'):
            self.preflight(b'ReadyThread DPC Interrupt Microsoft-Windows-Kernel-Processor-Power')
        receipt = load(self.run / 'preflight.json')
        self.assertFalse(receipt['wpr_start_attempted'])
        self.assertEqual(receipt['profile_key'], 'f1-scale')


class CaseVerdictTests(unittest.TestCase):
    clean = {'released_clean': True, 'sessions_absent': True}
    f1_clean = {'released_clean': True, 'sessions_absent': True, 'active_descendants': 0}

    def test_canary_verdicts_are_unchanged(self):
        self.assertTrue(controller.case_passed('normal', 0, 0, dict(self.clean), False))
        self.assertFalse(controller.case_passed('normal', 1, 0, dict(self.clean), False))
        self.assertFalse(controller.case_passed('normal', 0, 0, {'released_clean': True}, False))
        self.assertFalse(controller.case_passed('normal', 0, 1, dict(self.clean), False))

    def test_controller_death_still_requires_intent_takeover_and_absence(self):
        guard = {'takeover': True, 'sessions_absent': True, 'active_descendants': 0}
        self.assertTrue(controller.case_passed('controller-death', 73, 0, dict(guard), True))
        self.assertFalse(controller.case_passed('controller-death', 73, 0, dict(guard), False))
        self.assertFalse(controller.case_passed('controller-death', 0, 0, dict(guard), True))

    def test_f1_scale_needs_a_complete_six_launch_workload(self):
        complete = {'result': {'workload': {'status': 'COMPLETE', 'native_launches': 6}}}
        self.assertTrue(controller.case_passed('f1-scale', 0, 0, dict(self.f1_clean), False, complete))
        short = {'result': {'workload': {'status': 'COMPLETE', 'native_launches': 5}}}
        self.assertFalse(controller.case_passed('f1-scale', 0, 0, dict(self.f1_clean), False, short))
        self.assertFalse(controller.case_passed('f1-scale', 0, 0, dict(self.f1_clean), False, {}))
        self.assertFalse(controller.case_passed('f1-scale', 1, 0, dict(self.f1_clean), False, complete))
        failed = {'released_clean': True, 'sessions_absent': False, 'active_descendants': 0}
        self.assertFalse(controller.case_passed('f1-scale', 0, 0, failed, False, complete))

    def test_f1_timeout_needs_reclamation_and_liveness(self):
        receipts = {'result': {'workload': {'status': 'F1 WORKLOAD TIMEOUT; RECLAIMED; ORDERLY STOP '
                                                      'COMPLETED', 'exit_code': 74,
                                            'reclamation_proved': True, 'native_liveness_proved': True}}}
        self.assertTrue(controller.case_passed('f1-scale-timeout', 74, 0, dict(self.f1_clean), False,
                                               receipts))
        self.assertFalse(controller.case_passed('f1-scale-timeout', 0, 0, dict(self.f1_clean), False,
                                                receipts))
        unproved = {'result': {'workload': {'status': 'x', 'exit_code': 74,
                                            'reclamation_proved': False, 'native_liveness_proved': True}}}
        self.assertFalse(controller.case_passed('f1-scale-timeout', 74, 0, dict(self.f1_clean), False,
                                                unproved))
        idle = {'result': {'workload': {'status': 'x', 'exit_code': 74,
                                        'reclamation_proved': True, 'native_liveness_proved': False}}}
        self.assertFalse(controller.case_passed('f1-scale-timeout', 74, 0, dict(self.f1_clean), False,
                                                idle))

    def test_f1_death_needs_the_pid_proof_after_the_drain(self):
        guard = dict(self.f1_clean, takeover=True)
        receipts = {'result': {'workload': {'status': 'DEATH', 'exit_code': 73}},
                    'death': {'native_pid': 7000, 'exit_code': 73, 'before_stop_request': True},
                    'reclaimed': {'native_pid': 7000, 'proved_dead': True,
                                  'alive_after_controller_and_guardian': False}}
        self.assertTrue(controller.case_passed('f1-scale-death', 73, 0, dict(guard), True, receipts))
        survived = {'death': receipts['death'],
                    'reclaimed': {'native_pid': 7000, 'proved_dead': False,
                                  'alive_after_controller_and_guardian': True}}
        self.assertFalse(controller.case_passed('f1-scale-death', 73, 0, dict(guard), True,
                                                survived))
        self.assertFalse(controller.case_passed('f1-scale-death', 73, 0, dict(guard), False,
                                                receipts))
        self.assertFalse(controller.case_passed('f1-scale-death', 0, 0, dict(guard), True,
                                                receipts))

    def test_unknown_mode_never_passes(self):
        self.assertFalse(controller.case_passed('f2-scale', 0, 0, dict(self.f1_clean), False, {}))


class DispatchTests(unittest.TestCase):
    def test_canary_and_f1_use_their_own_fixed_case_lists(self):
        with mock.patch.object(controller, 'group', return_value=0) as group:
            self.assertEqual(controller.canary('out', {}), 0)
            group.assert_called_once_with('out', {}, 'canary', case_table.CANARY_CASES,
                                          controller.CANARY_BANNER, controller.CANARY_PASSED)
        with mock.patch.object(controller, 'group', return_value=0) as group:
            self.assertEqual(controller.f1('out', {}), 0)
            group.assert_called_once_with('out', {}, 'f1-scale', case_table.F1_CASES,
                                          controller.F1_BANNER, controller.F1_PASSED)

    def test_main_dispatches_the_f1_role_and_nothing_else(self):
        with mock.patch.object(controller, 'f1', return_value=0) as f1, \
                mock.patch.object(sys, 'argv', ['bootstrap.py', 'f1', 'out']):
            self.assertEqual(controller.main(SEALED, {}), 0)
            f1.assert_called_once_with('out', {})
        with mock.patch.object(controller, 'canary', return_value=0) as canary, \
                mock.patch.object(sys, 'argv', ['bootstrap.py', 'canary', 'out']):
            self.assertEqual(controller.main(SEALED, {}), 0)
            canary.assert_called_once_with('out', {})
        for argv in (['bootstrap.py', 'f1'], ['bootstrap.py', 'bogus', 'out']):
            with mock.patch.object(sys, 'argv', argv):
                with self.assertRaises(SystemExit):
                    controller.main(SEALED, {})


if __name__ == '__main__':
    unittest.main(verbosity=2)
