"""Supervise the sealed F1 scale collector inside the tool controller's Job.

This module adds no workload of its own. It writes the owned-capture receipt the
sealed collector requires, starts that collector as one member of the shared
worker Job, bounds it with the case envelope, and proves reclamation of the
native F1 processes the collector launched.

Those native processes are never members of the outer Job: the collector keeps
its own pinned inner Job with KILL_ON_JOB_CLOSE. Outer Job zero therefore cannot
prove they exited, so each live native PID identity is captured while alive and
re-checked after the collector is gone.
"""
import json
import os
from pathlib import Path
import sys
import time

from cases import F1_NATIVE_LAUNCHES, spec as case_spec
from common import load, remaining, say, sha, write
from job import GONE_WINERRORS, ProcessRef

HERE = Path(__file__).resolve().parent
MARKER = 'PD5_PROCESS_START '
CAPTURE_RECEIPT = 'f1-owned-capture.json'
DEATH_FACT = 'f1-native-liveness-at-death.json'
DEATH_INTENT = 'expected-controller-death.json'
IDENTITIES = 'f1-native-identities.json'
CAPTURE_SCHEMA = 'pd5.f1-controller-capture.v1'
CAPTURE_STATUS = 'OWNED WPR CAPTURE ACTIVE'


def native_directory(run):
    return Path(run) / 'f1-native'


def start_records(log):
    """Return PD5_PROCESS_START records in the order the collector wrote them."""
    try:
        text = Path(log).read_text(encoding='utf-8', errors='replace')
    except (FileNotFoundError, OSError):
        return []
    records = []
    for line in text.splitlines():
        index = line.find(MARKER)
        if index < 0:
            continue
        try:
            record = json.loads(line[index + len(MARKER):])
        except ValueError:
            continue
        if isinstance(record, dict) and isinstance(record.get('pid'), int) and record['pid'] > 0:
            records.append(record)
    return records


def all_start_records(native):
    return [record for path in sorted(Path(native).glob('*.log')) for record in start_records(path)]


def live_reference(pid):
    """Open the exact process identity, or return None when it is already gone.

    A pid outside the DWORD range cannot name a live process, so a rejected
    identity is reported as absent instead of aborting the reclamation proof.
    """
    try:
        return ProcessRef.from_pid(pid)
    except (OSError, RuntimeError, ValueError):
        return None


def observe_identities(native, seen):
    """Capture the exact identity of each native while it is still alive.

    A pid alone cannot be re-checked afterwards: Windows reuses pids, so a later
    pid-only OpenProcess proves nothing about the process that ran here. The
    (pid, creation_time) pair names one process instance for its whole life, so it
    can be re-checked exactly. No handle is kept open: an open handle itself keeps
    a terminated pid openable and would fake a survivor.
    """
    for record in all_start_records(native):
        pid = record['pid']
        if pid in seen:
            continue
        try:
            reference = ProcessRef.from_pid(pid)
        except OSError as error:
            if getattr(error, 'winerror', None) in GONE_WINERRORS:
                # Exited between its own start record and this observation; the
                # identity can never be proved now, so the case must fail closed.
                continue
            raise
        try:
            if reference.alive():
                seen[pid] = {'pid': pid, 'creation_time': reference.creation_time}
        finally:
            reference.close()
    return seen


def write_receipt(run, claim, claim_hash, binding):
    """Write the one receipt the sealed collector accepts as capture authority.

    The receipt names the controller source hash sealed beside the collector, so
    a different controller cannot issue it, and no wrapper hash is claimed.
    """
    controller = sha(HERE / 'controller.py')
    if controller != binding['controller_source_sha256']:
        raise RuntimeError('the running controller is not the sealed controller')
    receipt = {
        'schema': CAPTURE_SCHEMA,
        'status': CAPTURE_STATUS,
        'wpr_start_succeeded': True,
        'wpr_instance': claim['instance'],
        'plan_sha256': sha(HERE / 'plan.json'),
        'binding_sha256': sha(HERE / 'binding.json'),
        'controller_source_sha256': controller,
        'controller_claim_sha256': claim_hash,
        'written_monotonic': time.monotonic(),
        'formal_acceptance': False,
        'performance_acceptance': False,
    }
    write(Path(run) / CAPTURE_RECEIPT, receipt)
    return receipt


def collector_command(run, claim, claim_hash, binding, native):
    return [sys.executable, '-B', '-I', '-S', '-u', str(HERE / 'collect_f1_scale.py'),
            '--output', str(native),
            '--capture-receipt', str(Path(run) / CAPTURE_RECEIPT),
            '--wpr', binding['wpr']['path'],
            '--wpr-instance', claim['instance'],
            '--controller-claim-sha256', claim_hash]


def workload(job, guard, run, claim, claim_hash, binding):
    """Run one bounded F1 workload and return its durable outcome record."""
    run = Path(run)
    spec = case_spec(claim['case'])
    budget = spec['workload_budget_seconds']
    if spec['workload'] != 'sealed-f1-collector' or not budget:
        raise RuntimeError('this case has no bounded F1 workload')
    native = native_directory(run)
    native.mkdir()
    write_receipt(run, claim, claim_hash, binding)
    argv = collector_command(run, claim, claim_hash, binding, native)
    write(run / 'f1-collector-intent.json', {
        'argv': argv,
        'supervisor_budget_seconds': budget,
        'receipt_sha256': sha(run / CAPTURE_RECEIPT),
        'formal_acceptance': False,
    })
    log = run / 'f1-collector.log'
    with log.open('xb') as stream:
        child = job.spawn(argv, stream)
    write(run / 'f1-collector-spawn.json', {'gate_pid': child.pid, 'time': time.monotonic()})
    if claim['case'] == 'f1-scale-death':
        return _die_with_live_native(run, job, guard, child, claim, native)
    limit = min(claim['deadline'], time.monotonic() + budget)
    seen = {}
    observe_identities(native, seen)
    while True:
        if guard.poll() is not None:
            raise RuntimeError('guardian died during the F1 workload')
        if child.poll() is not None:
            break
        observe_identities(native, seen)
        if time.monotonic() >= limit:
            return _expire(run, job, guard, child, claim, native, budget)
        time.sleep(.05)
    code = child.wait()
    drain_by = min(claim['deadline'], time.monotonic() + 10)
    while job.active() and time.monotonic() < drain_by:
        if guard.poll() is not None:
            raise RuntimeError('guardian died during the F1 collector exit proof')
        time.sleep(.02)
    if job.active():
        raise RuntimeError('the F1 collector exited with live Job descendants')
    if code:
        raise RuntimeError('the F1 collector exited nonzero: ' + str(code))
    return _completed(run, native, seen)


def _completed(run, native, seen):
    """Prove the normal workload ended with every native identity gone.

    The natives live in the collector's inner Job, so outer Job zero proves
    nothing about them. Each identity captured while alive is re-checked after the
    collector is gone; a missing capture or a live identity fails the case instead
    of being reported as proved.
    """
    result_path = native / 'collector-result.json'
    if not result_path.exists():
        raise RuntimeError('the F1 collector wrote no collector-result.json')
    result = load(result_path)
    records = all_start_records(native)
    if result.get('status') != 'COMPLETE':
        raise RuntimeError('the F1 collector did not complete: ' + repr(result.get('failures')))
    if result.get('process_count_completed') != 3:
        raise RuntimeError('the F1 collector completed an unexpected process count')
    if result.get('native_start_records') != F1_NATIVE_LAUNCHES:
        raise RuntimeError('the F1 collector launch count differs from the sealed plan')
    if len(records) != F1_NATIVE_LAUNCHES:
        raise RuntimeError('the F1 native logs do not contain the sealed launch count')
    captured = [seen[pid] for pid in sorted(seen)]
    write(run / IDENTITIES, {
        'captured_while_alive': captured,
        'expected': F1_NATIVE_LAUNCHES,
        'handles_retained': False,
        'basis': 'a (pid, creation_time) pair names one process instance for its whole life; '
                 'no handle is kept open, so a terminated pid cannot read as openable',
        'formal_acceptance': False,
    })
    if len(captured) != F1_NATIVE_LAUNCHES:
        raise RuntimeError('native identity captured for only ' + str(len(captured)) + ' of '
                           + str(F1_NATIVE_LAUNCHES) + ' launches; death cannot be proved')
    rechecked = []
    for item in captured:
        reference = ProcessRef.acquire(item['pid'], item['creation_time'])
        try:
            alive = bool(reference is not None and reference.alive())
        finally:
            if reference is not None:
                reference.close()
        rechecked.append({'pid': item['pid'], 'creation_time': item['creation_time'],
                          'identity_now': 'live' if alive else 'absent',
                          'proved_dead': not alive})
    survivors = [item['pid'] for item in rechecked if not item['proved_dead']]
    write(run / 'f1-native-reclaimed.json', {
        'native_launches': len(records),
        'native_identities': rechecked,
        'native_pids_alive_after_collector': survivors,
        'proved_dead': not survivors,
        'checked_after': 'sealed collector exit and outer Job drain',
        'formal_acceptance': False,
    })
    if survivors:
        raise RuntimeError('native F1 processes survived the sealed collector: ' + repr(survivors))
    write(run / 'f1-workload.json', {
        'status': 'COMPLETE',
        'exit_code': 0,
        'native_launches': len(records),
        'native_pids': [record['pid'] for record in records],
        'process_count_completed': result.get('process_count_completed'),
        'collector_result_sha256': sha(result_path),
        'native_logs': [{'name': path.name, 'sha256': sha(path)}
                        for path in sorted(Path(native).glob('*.log'))],
        'formal_acceptance': False,
        'performance_acceptance': False,
    })
    return {'status': 'COMPLETE', 'exit_code': 0, 'native_launches': len(records),
            'reclamation_proved': True, 'native_liveness_proved': True}


def _expire(run, job, guard, child, claim, native, budget):
    """Enforce the supervisor budget, then prove both Jobs and every native PID released."""
    active = job.active()
    observed = list(dict.fromkeys(record['pid'] for record in all_start_records(native)))
    references = [reference for reference in (live_reference(pid) for pid in observed)
                  if reference is not None]
    alive_at_kill = [reference.pid for reference in references if reference.alive()]
    write(run / 'f1-workload-timeout.json', {
        'reason': 'supervisor budget expired; the sealed collector budget is unchanged',
        'supervisor_budget_seconds': budget,
        'active_job_processes_at_expiry': active,
        'native_pids_observed': observed,
        'native_pids_alive_at_kill': alive_at_kill,
        'native_liveness_proved': bool(alive_at_kill),
        'formal_acceptance': False,
        'performance_acceptance': False,
    })
    say('supervisor budget expired; terminating the F1 workload Job')
    drain_error = None
    try:
        job.terminate_and_drain(min(claim['deadline'], time.monotonic() + 10))
        child.wait(timeout=remaining(claim['deadline'], 5))
    except BaseException as error:
        drain_error = repr(error)
    until = min(claim['deadline'], time.monotonic() + 5)
    while True:
        survivors = [reference.pid for reference in references if reference.alive()]
        if not survivors or time.monotonic() >= until:
            break
        time.sleep(.05)
    for reference in references:
        reference.close()
    after = job.active()
    write(run / 'f1-reclamation.json', {
        'job_active_after_drain': after,
        'native_pids_alive_at_kill': alive_at_kill,
        'native_pids_alive_after_drain': survivors,
        'drain_error': drain_error,
        'proved_dead': not survivors,
        'formal_acceptance': False,
    })
    if drain_error or after or survivors:
        raise RuntimeError('reclamation unproved: ' + repr(
            {'drain_error': drain_error, 'job_active': after, 'survivors': survivors}))
    return {'status': 'F1 WORKLOAD TIMEOUT; RECLAIMED; ORDERLY STOP COMPLETED',
            'exit_code': 74,
            'native_launches': len(observed),
            'native_pids': observed,
            'native_pids_alive_at_kill': alive_at_kill,
            'reclamation_proved': True,
            'native_liveness_proved': bool(alive_at_kill)}


def write_death_intent(run, fact):
    """Declare the controller-death condition the harness has to accept as intended.

    The canary writes the same receipt before its death boundary. The F1 death case has
    to declare it too: an exit code alone is indistinguishable from a crash, so
    ``case_passed`` refuses a death case whose intent receipt is missing.
    """
    intent = {
        'before_stop_request': True,
        'exit_code': 73,
        'native_pid': fact['native_pid'],
        'native_creation_time': fact['native_creation_time'],
        'active_job_processes': fact['active_job_processes'],
        'scope': fact['scope'],
    }
    write(Path(run) / DEATH_INTENT, intent)
    return intent


def _die_with_live_native(run, job, guard, child, claim, native):
    """Die with a real native F1 child alive, leaving recovery to the guardian."""
    while time.monotonic() < claim['deadline']:
        if guard.poll() is not None:
            raise RuntimeError('guardian died before the F1 death boundary')
        if child.poll() is not None:
            raise RuntimeError('the F1 collector exited before the F1 death boundary')
        for record in all_start_records(native):
            reference = live_reference(record['pid'])
            if reference is None:
                continue
            try:
                if not reference.alive():
                    continue
                fact = {
                    'native_pid': record['pid'],
                    'native_creation_time': reference.creation_time,
                    'active_job_processes': job.active(),
                    'collector_gate_pid': child.pid,
                    'before_stop_request': True,
                    'exit_code': 73,
                    'scope': 'native F1 process launched by the sealed collector',
                }
                write(run / DEATH_FACT, fact)
                write_death_intent(run, fact)
                say('F1 death boundary: native pid ' + str(record['pid']) + ' proved alive')
                os._exit(73)
            finally:
                reference.close()
        time.sleep(.05)
    raise RuntimeError('no live native F1 process before the death deadline')
