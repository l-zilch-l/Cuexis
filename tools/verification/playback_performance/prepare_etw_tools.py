"""Seal a new machine-bound tool package: lifecycle canary plus the F1 scale cases.

The package is create-new and immutable. It never runs anything: sealing only
copies the delivered sources and inputs, cross-checks the case table against
plan.json, and writes hash-pinned launchers. Both the canary and the F1 cases are
tool acceptance only; no formal measurement, registration, or product budget.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys

sys.dont_write_bytecode = True

SCHEMA = 'pd5.etw-tool-canary.binding.v3'
DELIVERED = ('README.md', 'lifecycle.wprp', 'f1_scale.wprp', 'plan.json')


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def pinned_inputs(plan):
    anchor, support = plan['input_binding'], plan['support']
    entries = {
        'input_plan': (anchor['plan_path'], anchor['plan_sha256']),
        'baseline_executable': (anchor['baseline_executable'], anchor['baseline_executable_sha256']),
        'frozen_process_control': (support['frozen_process_control'], support['frozen_process_control_sha256']),
        'frozen_finite_followup': (support['frozen_finite_followup'], support['frozen_finite_followup_sha256']),
        'read_scaling_probe': (support['read_scaling_probe'], support['read_scaling_probe_sha256']),
        'measurement_environment': (support['measurement_environment'],
                                    support['measurement_environment_sha256']),
        'python_executable': (support['python_executable'], support['python_executable_sha256']),
    }
    for asset in support['runtime_assets']:
        entries['runtime_asset_' + asset['role']] = (asset['path'], asset['sha256'])
    for index, item in enumerate(support['runtime_resolution_evidence'], 1):
        entries['resolution_evidence_' + str(index)] = (item['path'], item['sha256'])
    return {key: {'path': str(path), 'sha256': digest} for key, (path, digest) in entries.items()}


def check_pinned_inputs(pins):
    # A pinned input that is already missing or changed must fail at seal time,
    # not in the middle of a supervised capture.
    for key, item in pins.items():
        path = Path(item['path'])
        if not path.is_file():
            raise RuntimeError('missing pinned input ' + key + ': ' + str(path))
        if sha(path) != item['sha256']:
            raise RuntimeError('pinned input changed ' + key + ': ' + str(path))


def cross_check(plan, cases, profile_config):
    """plan.json is the single source of truth; the case table must agree exactly."""
    f1, timed, dead = (cases.spec(name) for name in ('f1-scale', 'f1-scale-timeout', 'f1-scale-death'))
    wpr = plan['wpr']
    if plan['preparation_revision'] != 4 or plan['formal'] or plan['capture_enabled'] is not True:
        raise RuntimeError('plan preparation revision/state changed')
    if list(cases.CANARY_CASES) != ['normal', 'controller-death'] or \
            list(cases.F1_CASES) != ['f1-scale', 'f1-scale-timeout', 'f1-scale-death']:
        raise RuntimeError('fixed case order changed')
    if f1['profile'] != 'f1-scale' or wpr['profile'] != profile_config.F1_PROFILE_NAME:
        raise RuntimeError('fixed F1 profile name changed')
    if tuple(wpr['required_profile_details_tokens']) != cases.PROFILE_PREFLIGHT_TOKENS['f1-scale']:
        raise RuntimeError('F1 profile preflight tokens changed')
    if f1['deadline_seconds'] != wpr['hard_deadline_seconds'] != wpr['bounded_capture_seconds']:
        raise RuntimeError('F1 hard deadline differs from the plan')
    if f1['takeover_seconds'] != wpr['guardian_emergency_seconds']:
        raise RuntimeError('F1 guardian takeover differs from the plan')
    if f1['disk_cap_bytes'] != wpr['disk_emergency_bytes']:
        raise RuntimeError('F1 disk cap differs from the plan')
    if f1['start_cap_seconds'] != wpr['start_cli_timeout_seconds']:
        raise RuntimeError('F1 start timeout differs from the plan')
    if f1['stop_cap_seconds'] != wpr['stop_cli_timeout_seconds']:
        raise RuntimeError('F1 stop timeout differs from the plan')
    if f1['workload_budget_seconds'] != wpr['collector_budget_seconds'] + cases.F1_SUPERVISOR_GRACE_SECONDS:
        raise RuntimeError('F1 supervisor budget must outlive the collector budget by the fixed grace')
    if cases.F1_NATIVE_LAUNCHES != plan['workload']['native_launches']:
        raise RuntimeError('F1 native launch count differs from the plan')
    if plan['workload']['processes'] != 3 or plan['workload']['mask'] != 4:
        raise RuntimeError('fixed three-process mask-4 workload was changed')
    if timed['workload_budget_seconds'] != cases.F1_TIMEOUT_BUDGET_SECONDS or \
            cases.F1_TIMEOUT_BUDGET_SECONDS >= plan['workload']['warmup_seconds_before_each_measured_process']:
        raise RuntimeError('the timeout case must fire inside the first fixed warmup')
    if timed['exit_codes'] != (74,) or dead['exit_codes'] != (73,):
        raise RuntimeError('fixed F1 case exit codes changed')
    if timed['native_launches'] != dead['native_launches'] or \
            dead['native_launches'] != cases.F1_NATIVE_LAUNCHES:
        raise RuntimeError('declared native launch count must be identical across F1 cases')
    for item in (f1, timed, dead):
        if item['formal_acceptance'] or item['performance_acceptance']:
            raise RuntimeError('a tool case may never claim formal or performance acceptance')


def launcher(binding_sha, bootstrap_sha, python, python_sha, prefix, role, notice):
    return f"""$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
if ((Get-FileHash -LiteralPath (Join-Path $taskRoot 'binding.json') -Algorithm SHA256).Hash.ToLowerInvariant() -ne '{binding_sha}') {{ throw 'Binding changed; do not run.' }}
if ((Get-FileHash -LiteralPath (Join-Path $taskRoot 'bootstrap.py') -Algorithm SHA256).Hash.ToLowerInvariant() -ne '{bootstrap_sha}') {{ throw 'Bootstrap changed; do not run.' }}
$taskPython = '{python}'
if ((Get-FileHash -LiteralPath $taskPython -Algorithm SHA256).Hash.ToLowerInvariant() -ne '{python_sha}') {{ throw 'Python changed; do not execute.' }}
$taskOutput = Join-Path $taskRoot ('{prefix}' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ') + '-' + [Guid]::NewGuid().ToString('N'))
Write-Host '{notice}'
& $taskPython -B -I -S -u (Join-Path $taskRoot 'bootstrap.py') {role} $taskOutput
$taskExit = $LASTEXITCODE
Write-Host ('[PD5 tools] Exit ' + $taskExit + '; evidence: ' + $taskOutput)
exit $taskExit
"""


def prepare(output, wpr):
    output = output.resolve()
    wpr = wpr.resolve(strict=True)
    source = Path(__file__).resolve().with_name('etw_safety')
    plan = json.loads((source / 'plan.json').read_text(encoding='utf-8'))
    sys.path.insert(0, str(source))
    import cases
    import profile_config
    cross_check(plan, cases, profile_config)
    if str(wpr).lower() != str(Path(plan['wpr']['executable'])).lower():
        raise RuntimeError('--wpr is not the plan pinned WPR executable')
    wpr_sha = sha(wpr)
    pins = pinned_inputs(plan)
    check_pinned_inputs(pins)
    # A preparation is create-new, never an update of an existing sealed package.
    output.mkdir(exist_ok=False)
    for item in sorted(source.glob('*.py')):
        shutil.copyfile(item, output / item.name)
    for name in DELIVERED:
        shutil.copyfile(source / name, output / name)
    python = Path(sys.executable).resolve()
    binding = {
        'schema': SCHEMA,
        'formal': False,
        'formal_acceptance': False,
        'performance_acceptance': False,
        'prepared_on': plan['prepared_on'],
        'preparation_revision': plan['preparation_revision'],
        'native_launches': {'canary': 0, 'f1-scale': cases.F1_NATIVE_LAUNCHES},
        'profiles': {
            'canary': {'file': 'lifecycle.wprp', 'name': 'PD5ToolLifecycle',
                       'sha256': sha(output / 'lifecycle.wprp')},
            'f1-scale': {'file': 'f1_scale.wprp', 'name': 'PD5F1Scale',
                         'sha256': sha(output / 'f1_scale.wprp')},
        },
        'cases': list(cases.CASES),
        'case_envelopes': {name: cases.envelope(name) for name in cases.CASES},
        'artifact_sha256': {p.name: sha(p) for p in sorted(output.iterdir()) if p.is_file()},
        'input_sha256': pins,
        'sources': {p.name: sha(p) for p in output.glob('*.py')},
        'controller_source_sha256': sha(output / 'controller.py'),
        'wpr': {'path': str(wpr), 'sha256': wpr_sha, 'observed_version': plan['wpr']['observed_version']},
        'python': {'path': str(python), 'sha256': sha(python)},
    }
    (output / 'binding.json').write_text(json.dumps(binding, indent=2) + '\n', encoding='utf-8')
    binding_sha, bootstrap_sha = sha(output / 'binding.json'), sha(output / 'bootstrap.py')
    quote = str(python).replace("'", "''")
    (output / 'run_tool_canary.ps1').write_text(launcher(
        binding_sha, bootstrap_sha, quote, binding['python']['sha256'], 'canary-', 'canary',
        '[PD5 tools] No automatic elevation. Two minimal-profile lifecycle cases; '
        'no Power/Cuexis/F1/40k launches.'), encoding='utf-8-sig')
    (output / 'run_f1_scale.ps1').write_text(launcher(
        binding_sha, bootstrap_sha, quote, binding['python']['sha256'], 'f1-', 'f1',
        '[PD5 tools] No automatic elevation. Three fixed F1 cases; one bounded diagnostic capture '
        'per case; launcher exit 0=all cases passed, 1=FAILED/UNRESOLVED; per-case controller '
        'exits 0/74/73 are recorded in result.json; no retry; not a formal measurement.'), encoding='utf-8-sig')
    print(json.dumps({'package': str(output), 'binding_sha256': binding_sha,
                      'launchers': {name: sha(output / name)
                                    for name in ('run_tool_canary.ps1', 'run_f1_scale.ps1')},
                      'executed': False}, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--wpr', required=True, type=Path)
    args = parser.parse_args()
    prepare(args.output, args.wpr)
