"""Tool-only ETW cases for the sealed PD-5 package. Never launches Cuexis itself.

The controller owns normal WPR calls. Only its guardian can take over, after
signaling the exact controller dead and draining its shared worker Job. The
canary cases launch no workload at all; the f1-scale cases supervise the sealed
F1 collector as one Job member and touch nothing else. This module deliberately
has no formal-window, registration, or measurement CLI.
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import time
import uuid

from cases import (CANARY_CASES, F1_CASES, F1_NATIVE_LAUNCHES, PROFILE_PREFLIGHT_TOKENS,
                   spec as case_spec)
from common import load, remaining, say, sha, write
import f1_scale
from flow import released_clean, take_over
from job import Job, ProcessRef
from profile_config import profile_argument

HERE = Path(__file__).resolve().parent


def command(role, *args):
    return [sys.executable, '-B', '-I', '-S', '-u', str(HERE / 'bootstrap.py'), role, *map(str, args)]


def spawn(role, run, *args):
    log = (run / (role + '-' + uuid.uuid4().hex + '.log')).open('xb')
    try:
        return subprocess.Popen(command(role, *args), stdin=subprocess.DEVNULL,
                                stdout=log, stderr=subprocess.STDOUT, close_fds=True,
                                creationflags=subprocess.CREATE_NO_WINDOW)
    finally:
        log.close()


def checked_claim(path, expected):
    path = Path(path).resolve()
    if path.name != 'claim.json' or sha(path) != expected:
        raise RuntimeError('claim hash/path mismatch')
    claim = load(path)
    if claim['instance'] != 'PD5F1Noise_' + claim['nonce']:
        raise RuntimeError('instance mismatch')
    if claim['job'] != 'Local\\CuexisPD5_' + claim['nonce']:
        raise RuntimeError('Job mismatch')
    from etw import Ownership
    Ownership(claim['instance'], path.parent / 'trace-temp')
    spec = case_spec(claim['case'])
    if claim.get('profile') != spec['profile']:
        raise RuntimeError('claim profile differs from the fixed case table')
    if claim['binding_sha256'] != sha(HERE / 'binding.json'):
        raise RuntimeError('claim is bound to a different tool preparation')
    if claim['deadline'] - claim['started'] != spec['deadline_seconds']:
        raise RuntimeError('unexpected case deadline')
    if claim['takeover'] - claim['started'] != spec['takeover_seconds']:
        raise RuntimeError('unexpected case takeover point')
    if claim['disk_cap_bytes'] != spec['disk_cap_bytes']:
        raise RuntimeError('unexpected case disk cap')
    if claim['active_logger_cap_seconds'] != spec['active_logger_cap_seconds']:
        raise RuntimeError('unexpected case active-logger cap')
    if spec['workload'] == 'none':
        if claim.get('native_launches') != 0:
            raise RuntimeError('not a fixed no-workload case')
    elif 'native_launches' in claim:
        raise RuntimeError('the F1 cases never pre-declare a native launch count')
    return claim, path.parent


def api_call(run, claim_hash, operation, deadline):
    target = run / ('api-' + operation + '-' + uuid.uuid4().hex + '.json')
    mutating = operation == 'stop'
    if mutating:
        # One raw-stop helper per case, across guardian death and harness fallback.
        # A lost/unreaped helper can never be followed by another STOP authority.
        write(run / 'raw-stop-authority.json', {'claim_sha256': claim_hash, 'output': str(target)})
    child = None
    try:
        child = spawn('api', run, run / 'claim.json', claim_hash, operation, target)
        code = child.wait(timeout=remaining(deadline, 8))
    except BaseException:
        if child is not None:
            if child.poll() is None:
                child.kill()
            child.wait(timeout=3)  # Failure leaves no reap proof and prohibits completion.
        if mutating and child is not None:
            write(run / 'raw-stop-helper-exit.json', {'helper_exit_proved': True,
                  'pid': child.pid, 'exit_code': child.returncode})
        raise
    if mutating:
        write(run / 'raw-stop-helper-exit.json', {'helper_exit_proved': True,
              'pid': child.pid, 'exit_code': code})
    if code or not target.exists():
        raise RuntimeError('ETW API helper failed: ' + str(target))
    value = load(target)
    if value.get('error'):
        raise RuntimeError(value['error'])
    return value


def api(path, claim_hash, operation, output):
    from etw import Backend, Ownership
    claim, run = checked_claim(path, claim_hash)
    output = Path(output).resolve()
    if output.parent != run or not output.name.startswith('api-'):
        raise RuntimeError('API receipt outside this run')
    result = {'operation': operation, 'formal': False, 'native_launches': 0}
    try:
        backend = Backend()
        ownership = Ownership(claim['instance'], run / 'trace-temp')
        if operation == 'stop':
            authority = load(run / 'raw-stop-authority.json')
            if authority != {'claim_sha256': claim_hash, 'output': str(output)}:
                raise RuntimeError('raw STOP helper is not the single claimed authority')
            # Recheck prerequisites in the mutating helper, not just its caller.
            parent = ProcessRef(claim['parent_pid'], claim['parent_creation'])
            job = Job.open(claim['job'])
            try:
                if parent.alive() or job.active() != 0:
                    raise RuntimeError('raw STOP denied: controller alive or Job not empty')
                before = backend.prove_absent(ownership)
                names = sorted({row['name'] for row in before['owned'] + before['exact']})
                result['before'] = before
                result['codes'] = []
                # Publish intent before the first potentially mutating API call.
                write(output.with_suffix('.intent.json'), result)
                for name in names:
                    if name not in ownership.names:
                        raise RuntimeError('raw STOP name outside sealed ownership')
                    result['codes'].append({'name': name, 'code': backend.stop_exact(name)})
            finally:
                job.close()
                parent.close()
        result.update(backend.prove_absent(ownership))
        # A fresh nonce owns exactly the two declared loggers. Unknown suffixes
        # from this nonce are never stopped and prevent a clean result.
        prefix = 'WPR_initiated_' + claim['instance'] + '_'
        result['unexpected_owned_prefix'] = [row for row in result['sessions']
            if row['name'].casefold().startswith(prefix.casefold()) and row['name'] not in ownership.names]
        if result['unexpected_owned_prefix']:
            result['sessions_absent'] = False
        write(output, result)
        return 0
    except BaseException as error:
        result['error'] = repr(error)
        write(output, result)
        return 1


class Effects:
    def __init__(self, parent, job, run, claim_hash):
        self.parent, self.job, self.run, self.claim_hash = parent, job, run, claim_hash

    def stop_controller(self, deadline):
        self.parent.terminate_and_wait(min(deadline, time.monotonic() + 5))

    def controller_alive(self):
        return self.parent.alive()

    def drain_job(self, deadline):
        self.job.terminate_and_drain(min(deadline, time.monotonic() + 5))

    def active_processes(self):
        return self.job.active()

    def prove_absent(self, deadline):
        return api_call(self.run, self.claim_hash, 'query', deadline)['sessions_absent']

    def stop_owned_and_prove(self, deadline):
        errors = []
        while time.monotonic() < deadline:
            try:
                operation = 'query' if (self.run / 'raw-stop-authority.json').exists() else 'stop'
                proof = api_call(self.run, self.claim_hash, operation, deadline)
                reaped = self.run / 'raw-stop-helper-exit.json'
                if proof['sessions_absent'] and reaped.exists() and load(reaped).get('helper_exit_proved'):
                    return {'sessions_absent': True, 'active_descendants': 0,
                            'raw_trace_only': True, 'prior_errors': errors,
                            'proof': proof}
            except Exception as error:
                errors.append(repr(error))
            time.sleep(min(.2, max(0, deadline - time.monotonic())))
        raise TimeoutError('exact owned ETW absence unproved; ' + repr(errors))


def guardian(path, claim_hash):
    claim, run = checked_claim(path, claim_hash)
    if os.getppid() != claim['parent_pid']:
        raise RuntimeError('guardian was not started by the sealed controller')
    parent = ProcessRef(claim['parent_pid'], claim['parent_creation'])
    job = Job.open(claim['job'])
    effects = Effects(parent, job, run, claim_hash)
    result = {'released_clean': False, 'sessions_absent': False, 'native_launches': 0}
    try:
        identity = ProcessRef.from_pid(os.getpid())
        try:
            if job.contains(parent) or job.contains(identity):
                raise RuntimeError('controller/guardian must be outside worker Job')
            write(run / 'guardian-ready.json', {'pid': os.getpid(), 'creation_time': identity.creation_time,
                                               'claim_sha256': claim_hash})
        finally:
            identity.close()
        next_query = 0
        while True:
            now = time.monotonic()
            if not parent.alive():
                result['reason'] = 'controller exited'
                break
            if now >= claim['takeover']:
                result['reason'] = 'controller deadline'
                break
            if (run / 'release.json').exists():
                result.update(released_clean(effects, claim['deadline']))
                write(run / 'guardian-result.json', result)
                return 0
            try:
                disk_files = [{'name': str(p.relative_to(run)), 'bytes': p.stat().st_size}
                              for p in run.rglob('*') if p.is_file()]
                total = sum(item['bytes'] for item in disk_files)
                if total > claim['disk_cap_bytes']:
                    result['reason'] = 'tool-only disk cap ' + str(claim['disk_cap_bytes']) + ' bytes'
                    result['disk_trigger'] = {'bytes': total, 'threshold_bytes': claim['disk_cap_bytes'],
                                              'files': disk_files}
                    break
                # Query failures trigger takeover, never an idle inference.
                if now >= next_query:
                    probe = api_call(run, claim_hash, 'query', claim['deadline'])
                    cap = claim['active_logger_cap_seconds']
                    if cap is not None and now >= claim['started'] + cap and not probe['sessions_absent']:
                        result['reason'] = 'tool-only active logger cap ' + str(cap) + 's'
                        break
                    next_query = time.monotonic() + 2
            except Exception as error:
                result['reason'] = 'watcher uncertainty: ' + repr(error)
                break
            time.sleep(.1)
        write(run / 'takeover-intent.json', {'reason': result['reason'], 'time': time.monotonic()})
        result.update(take_over(effects, claim['deadline']))
        result['takeover'] = True
        write(run / 'guardian-result.json', result)
        return 0
    except BaseException as error:
        result['error'] = repr(error)
        result['status'] = 'UNRESOLVED; DO NOT RERUN'
        write(run / 'guardian-result.json', result)
        return 1
    finally:
        job.close()
        parent.close()


def run_wpr(job, guard, run, name, argv, deadline, cap):
    write(run / (name + '-command-intent.json'), {'argv': argv, 'time': time.monotonic(),
                                               'phase_deadline': min(deadline, time.monotonic() + cap)})
    with (run / (name + '.log')).open('xb') as output:
        child = job.spawn(argv, output)
        write(run / (name + '-spawn.json'), {'wrapper_pid': child.pid, 'time': time.monotonic()})
        limit = time.monotonic() + remaining(deadline, cap)
        try:
            while child.poll() is None:
                if guard.poll() is not None:
                    raise RuntimeError('guardian died during ' + name)
                if time.monotonic() >= limit:
                    raise TimeoutError(name + ' deadline')
                time.sleep(.05)
            code = child.wait()
            # Job accounting may lag the wrapper's signaled process handle.
            drain_by = min(deadline, time.monotonic() + 3)
            while job.active() and time.monotonic() < drain_by:
                if guard.poll() is not None:
                    raise RuntimeError('guardian died during worker exit proof')
                time.sleep(.02)
            if job.active() != 0:
                raise RuntimeError(name + ' exited with live descendants')
            write(run / (name + '-result.json'), {'exit_code': code, 'active_descendants': 0})
            if code:
                raise RuntimeError(name + ' nonzero exit: ' + str(code))
        except BaseException:
            job.terminate_and_drain(min(deadline, time.monotonic() + 5))
            child.wait(timeout=remaining(deadline, 3))
            raise


def verify_inputs(binding):
    """Re-hash every pinned input before this case is allowed to capture.

    The sealed collector repeats the same check after the controller starts WPR,
    which is already too late: a capture against changed inputs would exist. The
    controller therefore refuses first and writes the count it verified.
    """
    pins = binding.get('input_sha256')
    if not isinstance(pins, dict) or not pins:
        raise RuntimeError('binding carries no pinned inputs; no capture')
    for key in sorted(pins):
        item = pins[key]
        path = Path(item['path'])
        if not path.is_file():
            raise RuntimeError('pinned input is missing: ' + key + ': ' + str(path))
        if sha(path).lower() != str(item['sha256']).lower():
            raise RuntimeError('pinned input changed: ' + key + ': ' + str(path))
    return len(pins)


def case(run, mode, binding):
    run = Path(run).resolve()
    pinned_inputs = verify_inputs(binding)
    write(run / 'input-pins.json', {'verified_before_start': True, 'pinned_inputs': pinned_inputs,
                                    'binding_sha256': sha(HERE / 'binding.json'),
                                    'formal_acceptance': False})
    spec = case_spec(mode)
    nonce = uuid.uuid4().hex
    parent = ProcessRef.from_pid(os.getpid())
    started = time.monotonic()
    claim = {'nonce': nonce, 'instance': 'PD5F1Noise_' + nonce, 'job': 'Local\\CuexisPD5_' + nonce,
             'case': mode, 'profile': spec['profile'],
             'parent_pid': os.getpid(), 'parent_creation': parent.creation_time,
             'started': started, 'takeover': started + spec['takeover_seconds'],
             'deadline': started + spec['deadline_seconds'],
             'disk_cap_bytes': spec['disk_cap_bytes'],
             'active_logger_cap_seconds': spec['active_logger_cap_seconds'],
             'formal': False, 'binding_sha256': sha(HERE / 'binding.json')}
    if spec['workload'] == 'none':
        claim['native_launches'] = 0
    parent.close()
    job = Job.create(claim['job'])
    guard = None
    result = {'case': mode, 'native_launches': spec['native_launches'], 'formal': False,
              'status': 'FAILED'}
    try:
        current = ProcessRef.from_pid(os.getpid())
        try:
            if job.contains(current):
                raise RuntimeError('controller is inside worker Job')
        finally:
            current.close()
        (run / 'trace-temp').mkdir()
        write(run / 'claim.json', claim)
        claim_hash = sha(run / 'claim.json')
        guard = spawn('guardian', run, run / 'claim.json', claim_hash)
        ready_by = min(claim['deadline'], time.monotonic() + 12)
        while not (run / 'guardian-ready.json').exists():
            if guard.poll() is not None or time.monotonic() >= ready_by:
                raise RuntimeError('guardian not ready; no WPR start')
            time.sleep(.05)
        ready = load(run / 'guardian-ready.json')
        if ready['pid'] != guard.pid or ready['claim_sha256'] != claim_hash:
            raise RuntimeError('guardian handshake mismatch')
        attached_by = min(claim['deadline'], time.monotonic() + 12)
        while not (run / 'harness-attached.json').exists():
            if guard.poll() is not None or time.monotonic() >= attached_by:
                raise RuntimeError('independent harness not attached; no WPR start')
            time.sleep(.05)
        expected_attachment = {'controller': os.getpid(), 'controller_creation': claim['parent_creation'],
                               'guardian': guard.pid, 'guardian_creation': ready['creation_time'],
                               'job': claim['job'], 'claim_sha256': claim_hash}
        if load(run / 'harness-attached.json') != expected_attachment:
            raise RuntimeError('independent harness identity mismatch; no WPR start')
        wpr = binding['wpr']['path']
        write(run / 'start-intent.json', {'instance': claim['instance'], 'case': mode,
                                          'native_launches': spec['native_launches']})
        run_wpr(job, guard, run, 'start', [wpr, '-start', profile_argument(HERE, binding, spec['profile']),
                  '-filemode', '-recordtempto', str(run / 'trace-temp'), '-instancename', claim['instance']],
                claim['deadline'], spec['start_cap_seconds'])
        active = api_call(run, claim_hash, 'query', claim['deadline'])
        expected = {'WPR_initiated_' + claim['instance'] + '_' + suffix
                    for suffix in ('WPR Event Collector', 'WPR System Collector')}
        if {row['name'] for row in active['owned']} != expected or active['unexpected_owned_prefix']:
            raise RuntimeError('expected exact pair not proved active')
        write(run / 'active.json', active)
        hold_until = time.monotonic() + 2
        while time.monotonic() < hold_until:
            if guard.poll() is not None:
                raise RuntimeError('guardian died during hold')
            time.sleep(.05)
        workload = None
        if spec['workload'] == 'none':
            if mode == 'controller-death':
                # A harmless live Job member makes the real canary exercise drain,
                # independently of the separately tested nested-child primitives.
                worker_log = run / 'benign-worker.log'
                with worker_log.open('xb') as output:
                    worker = job.spawn([sys.executable, '-B', '-I', '-S', '-c',
                        "import time;print('PD5_BENIGN_READY',flush=True);time.sleep(60)"], output)
                ready_by = min(claim['deadline'], time.monotonic() + 5)
                while 'PD5_BENIGN_READY' not in worker_log.read_text(encoding='utf-8', errors='replace'):
                    if guard.poll() is not None or worker.poll() is not None or time.monotonic() >= ready_by:
                        raise RuntimeError('benign Job worker not ready for death canary')
                    time.sleep(.05)
                active_workers = job.active()
                if active_workers < 1:
                    raise RuntimeError('no live Job member at death-canary boundary')
                write(run / 'expected-controller-death.json', {'before_stop_request': True, 'exit_code': 73,
                      'benign_worker_pid': worker.pid, 'active_job_processes': active_workers})
                # Known exact process dies. Guardian retains the Job handle and owns recovery.
                os._exit(73)
        else:
            # The sealed collector runs as one Job member and keeps its own inner Job.
            workload = f1_scale.workload(job, guard, run, claim, claim_hash, binding)
        trace = run / ('canary.etl' if spec['workload'] == 'none' else 'f1-scale.etl')
        description = ('PD5 tool-only lifecycle canary' if spec['workload'] == 'none'
                       else 'PD5 F1 scale diagnostic capture; tool acceptance only')
        run_wpr(job, guard, run, 'stop', [wpr, '-stop', str(trace), description,
                  '-instancename', claim['instance']],
                  min(claim['deadline'], claim['takeover'] - 5), spec['stop_cap_seconds'])
        proof = api_call(run, claim_hash, 'query', claim['deadline'])
        if not proof['sessions_absent']:
            raise RuntimeError('owned ETW still present after stop')
        first = trace.stat()
        time.sleep(1)
        second = trace.stat()
        if not second.st_size or (first.st_size, first.st_mtime_ns) != (second.st_size, second.st_mtime_ns):
            raise RuntimeError('merged trace not nonempty and stable')
        if list((run / 'trace-temp').rglob('*.etl')):
            raise RuntimeError('temporary traces remain after normal stop')
        result['trace'] = {'bytes': second.st_size, 'sha256': sha(trace)}
        if workload is not None:
            result['workload'] = workload
        write(run / 'release.json', {'request': 'independently recheck Job0 and exact absence'})
        guard.wait(timeout=remaining(claim['deadline'], 12))
        final = load(run / 'guardian-result.json')
        if guard.returncode or not final.get('released_clean'):
            raise RuntimeError('guardian did not release cleanly')
        result.update(status=(workload or {}).get('status', 'COMPLETE'),
                      sessions_absent=True, active_descendants=0)
        write(run / 'result.json', result)
        return int((workload or {}).get('exit_code', 0))
    except BaseException as error:
        result['error'] = repr(error)
        # No raw STOP here: this controller never competes with a live guardian.
        # Exit after drain lets the guardian acquire sole cleanup authority.
        try:
            job.terminate_and_drain(min(claim['deadline'], time.monotonic() + 5))
            result['active_descendants'] = 0
        except Exception as drain_error:
            result['drain_error'] = repr(drain_error)
        write(run / 'result.json', result)
        return 1
    finally:
        job.close()


def preflight(run, binding, profile_key='canary'):
    from etw import Backend, require_admin
    from policy import classify
    receipt = {'native_launches': 0, 'wpr_start_attempted': False, 'formal': False,
               'profile_key': profile_key}
    try:
        require_admin()
        profile = profile_argument(HERE, binding, profile_key)
        receipt['sessions'] = Backend().sessions()
        receipt['classification'] = classify(receipt['sessions'])
        if receipt['classification']['conflicts']:
            raise RuntimeError('unknown/changed WPR or PD5 session; none touched')
        for label, argv in (
            ('privileges', [str(Path(os.environ['SystemRoot']) / 'System32/whoami.exe'), '/priv']),
            ('token', [str(Path(os.environ['SystemRoot']) / 'System32/whoami.exe'), '/all']),
            ('status', [binding['wpr']['path'], '-status']),
            ('profile', [binding['wpr']['path'], '-profiledetails', profile])):
            child = subprocess.Popen(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                     creationflags=subprocess.CREATE_NO_WINDOW, close_fds=True)
            try:
                raw, _ = child.communicate(timeout=10)
            except BaseException:
                child.kill()
                raw, _ = child.communicate(timeout=3)
                receipt[label] = {'exit_code': child.returncode, 'output': raw.decode(errors='replace')}
                raise
            receipt[label] = {'exit_code': child.returncode, 'output': raw.decode(errors='replace')}
            if child.returncode:
                raise RuntimeError('read-only ' + label + ' failed')
        if 'SeSystemProfilePrivilege' not in receipt['privileges']['output']:
            raise RuntimeError('SeSystemProfilePrivilege missing')
        for token in PROFILE_PREFLIGHT_TOKENS[profile_key]:
            if token not in receipt['profile']['output']:
                raise RuntimeError('sealed ' + profile_key + ' profile missing ' + token)
        if 'WPR is not recording' not in receipt['status']['output']:
            raise RuntimeError('unnamed WPR not proved idle; preserve output')
        receipt['status_result'] = 'PREFLIGHT PASSED; NO TRACE YET'
        return receipt
    except BaseException as error:
        receipt['error'] = repr(error)
        raise
    finally:
        write(run / 'preflight.json', receipt)


def group(output, binding, profile, fixed_cases, banner, passed_status):
    root = Path(output).resolve()
    root.mkdir(exist_ok=False)
    result = {'formal': False, 'native_launches': 0 if profile == 'canary' else None,
              'fixed_cases': list(fixed_cases), 'auto_retry': False, 'status': 'FAILED',
              'cases': []}
    if profile != 'canary':
        result['native_launches_expected'] = F1_NATIVE_LAUNCHES
    say(banner)
    try:
        preflight(root, binding, profile)
        for mode in result['fixed_cases']:
            spec = case_spec(mode)
            run = root / mode
            run.mkdir()
            preflight(run, binding, profile)
            say('Starting fixed case ' + mode + '; receipts: ' + str(run))
            child = spawn('case', run, run, mode)
            controller_ref = ProcessRef.from_pid(child.pid)
            guard_ref, worker_job = None, None
            case_deadline = time.monotonic() + spec['supervision_seconds']
            try:
                progress_at = 0
                while child.poll() is None or (guard_ref is not None and guard_ref.alive()):
                    now = time.monotonic()
                    if guard_ref is None and (run / 'guardian-ready.json').exists():
                        ready = load(run / 'guardian-ready.json')
                        guard_ref = ProcessRef.acquire(ready['pid'], ready['creation_time'])
                        if guard_ref is None:
                            raise RuntimeError('guardian exited before the harness attached; '
                                               'no success inference')
                        claim = load(run / 'claim.json')
                        worker_job = Job.open(claim['job'])
                        harness_ref = ProcessRef.from_pid(os.getpid())
                        try:
                            if (worker_job.contains(harness_ref) or worker_job.contains(controller_ref)
                                    or worker_job.contains(guard_ref)):
                                raise RuntimeError('harness/controller/guardian must be outside worker Job')
                        finally:
                            harness_ref.close()
                        write(run / 'harness-attached.json', {'controller': child.pid,
                              'controller_creation': controller_ref.creation_time,
                              'guardian': ready['pid'], 'guardian_creation': guard_ref.creation_time,
                              'job': claim['job'], 'claim_sha256': sha(run / 'claim.json')})
                    if now >= case_deadline:
                        raise TimeoutError('outer case deadline; state unproved')
                    if now >= progress_at:
                        say(mode + ': waiting for controller/guardian; no new samples')
                        progress_at = now + 10
                    time.sleep(.05)
                code = child.wait(timeout=3)
                if guard_ref is None or worker_job is None:
                    raise RuntimeError('guardian identity not attached; no success inference')
                guard = load(run / 'guardian-result.json')
                guardian_code = guard_ref.exit_code()
                if worker_job.active() != 0:
                    raise RuntimeError('harness sees live worker descendants')
                if mode == 'f1-scale-death':
                    # The native F1 child lived in the collector's inner Job, which
                    # outer Job accounting cannot see. Prove that exact identity died.
                    death_proof(run, load(run / 'f1-native-liveness-at-death.json'))
                # Independent final same-token proof, after both authorities exited.
                final_proof = api_call(run, sha(run / 'claim.json'), 'query', case_deadline)
                if not final_proof['sessions_absent']:
                    raise RuntimeError('harness final ETW absence proof failed')
            except BaseException:
                # Fallback after a guardian crash is recovery only: the case still
                # fails. Retained identity handles prevent PID reuse or overlap.
                if guard_ref is not None and worker_job is not None:
                    controller_ref.terminate_and_wait(time.monotonic() + 3)
                    guard_ref.terminate_and_wait(time.monotonic() + 3)
                    recovery = take_over(Effects(controller_ref, worker_job, run, sha(run / 'claim.json')),
                                         time.monotonic() + 15)
                    write(run / 'harness-recovery.json', recovery)
                raise
            finally:
                if worker_job is not None:
                    worker_job.close()
                if guard_ref is not None:
                    guard_ref.close()
                controller_ref.close()
            receipts = {}
            for name, receipt in (('result', 'result.json'),
                                  ('death', 'f1-native-liveness-at-death.json'),
                                  ('reclaimed', 'f1-native-reclaimed.json')):
                if (run / receipt).exists():
                    receipts[name] = load(run / receipt)
            passed = case_passed(mode, code, guardian_code, guard,
                                 (run / 'expected-controller-death.json').exists(), receipts)
            result['cases'].append({'case': mode, 'controller_exit': code, 'passed': bool(passed),
                                    'guardian_exit': guardian_code, 'guardian': guard})
            if not passed:
                raise RuntimeError(f'{mode} failed: controller={code}, guardian={guardian_code}, '
                                   f'reason={guard.get("reason") or guard.get("error") or "acceptance proof missing"}; '
                                   'no retry and no next case')
        result['status'] = passed_status
        write(root / 'result.json', result)
        say(result['status'] + ': ' + str(root / 'result.json'))
        return 0
    except BaseException as error:
        result['error'] = repr(error)
        result['instruction'] = 'DO NOT RERUN; preserve all receipts and any raw/merged traces'
        write(root / 'result.json', result)
        say('FAILED/UNRESOLVED: ' + repr(error) + '; receipt: ' + str(root / 'result.json'))
        return 1


CANARY_BANNER = 'TOOL CANARY ONLY: fixed minimal lifecycle profile; no Power/Cuexis/F1/40k process.'
CANARY_PASSED = 'TOOL LIFECYCLE CANARY PASSED; NO PERFORMANCE ACCEPTANCE'
F1_BANNER = ('F1 TOOL ACCEPTANCE ONLY: sealed F1 collector under one bounded diagnostic WPR '
             'capture; no formal measurement and no product budget.')
F1_PASSED = 'F1 TOOL ACCEPTANCE PASSED; DIAGNOSTIC TRACE ONLY; NO PERFORMANCE ACCEPTANCE'


def canary(output, binding):
    return group(output, binding, 'canary', CANARY_CASES, CANARY_BANNER, CANARY_PASSED)


def f1(output, binding):
    return group(output, binding, 'f1-scale', F1_CASES, F1_BANNER, F1_PASSED)


def death_proof(run, death):
    """Prove the native identity recorded at the death boundary is no longer running.

    The native F1 child lives in the collector's inner Job, which outer Job accounting
    cannot see, so the identity that was sealed while it was alive is re-acquired after
    the controller and the guardian are gone. An absent PID, or a PID that now names a
    different process, proves that identity died; a live match fails the case.
    """
    run = Path(run)
    reference = ProcessRef.acquire(death['native_pid'], death['native_creation_time'])
    identity_now = 'absent' if reference is None else 'live'
    try:
        alive = bool(reference is not None and reference.alive())
    finally:
        if reference is not None:
            reference.close()
    receipt = {
        'native_pid': death['native_pid'],
        'native_creation_time': death['native_creation_time'],
        'identity_now': identity_now,
        'alive_after_controller_and_guardian': alive,
        'proved_dead': not alive,
        'checked_after': 'controller exit and guardian drain',
        'basis': 'the (pid, creation_time) pair names one process instance for its whole '
                 'life; "absent" means no process with that pid exists now, and an opened '
                 'pid whose creation time differs is a different instance, so either one '
                 'proves the recorded instance died'}
    write(run / 'f1-native-reclaimed.json', receipt)
    if alive:
        raise RuntimeError('native F1 process survived controller-death recovery')
    return receipt


def case_passed(mode, controller_code, guardian_code, guard, death_intent, receipts=None):
    if guardian_code != 0 or guard.get('error') or guard.get('status'):
        return False
    receipts = receipts or {}
    try:
        workload = case_spec(mode)['workload']
    except RuntimeError:
        return False
    if workload != 'none':
        if not guard.get('sessions_absent'):
            return False
        work = receipts.get('result', {}).get('workload') or {}
        if mode == 'f1-scale':
            return bool(controller_code == 0 and guard.get('released_clean')
                        and work.get('status') == 'COMPLETE'
                        and work.get('native_launches') == F1_NATIVE_LAUNCHES)
        if mode == 'f1-scale-timeout':
            return bool(controller_code == 74 and guard.get('released_clean')
                        and work.get('reclamation_proved') is True
                        and work.get('native_liveness_proved') is True
                        and work.get('exit_code') == 74)
        if mode == 'f1-scale-death':
            death = receipts.get('death') or {}
            reclaimed = receipts.get('reclaimed') or {}
            return bool(controller_code == 73 and death_intent and guard.get('takeover')
                        and guard.get('active_descendants') == 0
                        and bool(death.get('native_pid'))
                        and reclaimed.get('proved_dead') is True)
        return False
    if mode == 'normal':
        return bool(controller_code == 0 and guard.get('released_clean') and guard.get('sessions_absent'))
    return bool(mode == 'controller-death' and controller_code == 73 and death_intent
                and guard.get('takeover') and guard.get('sessions_absent')
                and guard.get('active_descendants') == 0)


def main(here, binding):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('role', choices=['canary', 'case', 'guardian', 'api', 'f1'])
    parser.add_argument('args', nargs='*')
    args = parser.parse_args()
    if args.role == 'canary' and len(args.args) == 1:
        return canary(args.args[0], binding)
    if args.role == 'f1' and len(args.args) == 1:
        return f1(args.args[0], binding)
    if args.role == 'case' and len(args.args) == 2:
        return case(args.args[0], args.args[1], binding)
    if args.role == 'guardian' and len(args.args) == 2:
        return guardian(*args.args)
    if args.role == 'api' and len(args.args) == 4 and args.args[2] in ('query', 'stop'):
        return api(*args.args)
    parser.error('wrong fixed-role arguments; no measurement/capture CLI exists')
