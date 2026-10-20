"""Build dedicated PD5 measurement/observer trees; historical builds are never reused."""
import argparse
import ctypes
import json
import os
from pathlib import Path
import subprocess
import sys
import time

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[3]))
from tools.verification.playback_performance import prepare_profiles as prepare


def build(args):
    root = prepare.ROOT
    args.output.mkdir(parents=True, exist_ok=True)
    setup = args.output / ('environment-' + str(time.time_ns()) + '.cmd')
    setup.write_text('@echo off\ncall "' + str(args.vcvars) + '" >nul\n'
                     'if errorlevel 1 exit /b %errorlevel%\nset\n', encoding='utf-8')
    for line in subprocess.check_output(['cmd.exe', '/d', '/c', str(setup)]).decode('utf-8', errors='replace').splitlines():
        if '=' in line:
            key, value = line.split('=', 1)
            if key:
                os.environ[key] = value
    os.environ.update(VCPKG_ROOT=str(args.vcpkg), VSLANG='2052')
    metrics = args.mode == 'observer'
    config = 'Debug' if args.mode == 'debug' else 'Release'
    name = 'pd5-ready-' + args.mode
    directory = root / 'out/build' / name
    preset = 'headless-debug' if config == 'Debug' else 'release'
    targets = ['cuexis_pd_baseline_probe', 'cuexis_pd_host_probe']
    if config == 'Release':
        targets += ['cuexis_execution_device_probe', 'cuexis_chart_capacity_probe', 'cuexis_player']
    commands = [('configure', ['cmake', '--preset', preset, '-B', str(directory), '--fresh',
        '-DCUEXIS_ENABLE_CHART_V5_CANDIDATE=ON', '-DCUEXIS_LIBRARY_TYPE=STATIC',
        '-DCUEXIS_PD_METRICS_TEST_ONLY=' + ('ON' if metrics else 'OFF'),
        '-DCUEXIS_ALLOCATION_LIFETIME_OBSERVER=' + ('ON' if args.mode == 'memory' else 'OFF'),
        '-DCUEXIS_PLAYBACK_TRACE_HARNESS=' + ('ON' if args.mode == 'release' else 'OFF'),
        '-DCUEXIS_WARNINGS_AS_ERRORS=ON']),
        ('build', ['cmake', '--build', str(directory), '--clean-first', '--target', *targets, '--parallel', '4'])]
    kernel = ctypes.windll.kernel32
    old = kernel.GetConsoleCP(), kernel.GetConsoleOutputCP()
    results = []
    try:
        for phase, command in commands:
            if args.incremental:
                if phase == 'configure':
                    continue
                command.remove('--clean-first')
            kernel.SetConsoleCP(65001)
            kernel.SetConsoleOutputCP(65001)
            path = args.output / (args.mode + '-' + phase + '-' + str(time.time_ns()) + '.log')
            print(name, phase, flush=True)
            with path.open('xb') as stream:
                result = subprocess.run(command, cwd=root, stdout=stream, stderr=subprocess.STDOUT)
            results.append({'command': command, 'phase': phase, 'exit_code': result.returncode,
                            'log': str(path), 'sha256': prepare.sha(path)})
            prepare.save_new(args.output / (args.mode + '-' + phase + '-' + str(time.time_ns()) + '.json'), results)
            print('exit=' + str(result.returncode) + ' log=' + str(path), flush=True)
            result.check_returncode()
    finally:
        kernel.SetConsoleCP(old[0])
        kernel.SetConsoleOutputCP(old[1])


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('mode', choices=['release', 'debug', 'observer', 'memory'])
    p.add_argument('--vcvars', type=Path, default=Path('D:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat'))
    p.add_argument('--vcpkg', type=Path, default=Path('D:/vcpkg'))
    p.add_argument('--output', type=Path, default=prepare.ROOT / 'out/pd5-complete-preparation-20261019/build')
    p.add_argument('--incremental', action='store_true')
    build(p.parse_args())


if __name__ == '__main__':
    main()
