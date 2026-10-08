"""Run the installed reference host's public Gameplay command path against manual goldens."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--host', type=Path, required=True)
    parser.add_argument('--tool', type=Path, required=True)
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--log-dir', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    args.log_dir.mkdir(parents=True, exist_ok=True)
    ordinal = 0
    host_processes = 0
    assembler_processes = 0

    def run(command, success=True):
        nonlocal ordinal, host_processes, assembler_processes
        ordinal += 1
        if command[0] == str(args.host.resolve()):
            host_processes += 1
        else:
            assembler_processes += 1
        result = subprocess.run(command, capture_output=True, timeout=120)
        output = (result.stdout + result.stderr).decode('utf-8', errors='replace')
        (args.log_dir/f'{ordinal:02d}.log').write_text(
            'command='+json.dumps(command)+'\nexit='+str(result.returncode)+'\n'+output,
            encoding='utf-8')
        assert (result.returncode == 0) == success, str(args.log_dir/f'{ordinal:02d}.log')
        return output

    with tempfile.TemporaryDirectory(prefix='cuexis-gameplay-host-') as directory:
        work = Path(directory)
        configuration = json.loads((args.fixture/'configuration.json').read_text(encoding='utf-8'))
        configuration['configuration']['domains'][0]['maximum'] = 2000
        config = work/'configuration.json'
        config.write_text(json.dumps(configuration),encoding='utf-8')
        observation = work/'observations.txt'
        observation.write_text('900 1 press lane.one domain.binding.one keyboard 100 200 300 400 0 0 0\n',encoding='ascii')
        commands = work/'commands.txt'
        commands.write_text('open\nplay\ntick 4\npause\ntick 2\nseek 1000\nreload\nplay\ntick 4\nquit\n',encoding='ascii')
        for carrier in ['cxc','filesystem']:
            target = work/('main.cxc' if carrier == 'cxc' else 'generations')
            run([str(args.tool.resolve()),'--gameplay','--base',
                 str(root/'tests/fixtures/chart_format_update/golden/cxc_v1_v4_static.cxc'),
                 '--foundation',str(args.fixture.resolve()/'foundation.packed'),
                 '--configuration',str(config),'--entry-id','chart.entry.one','--test-only','true',
                 '--configuration-budget','131072,64,8192,16384,8192',
                 '--graph-budget','131072,64,8192,16384,8192,8192,65536',
                 '--source',str(root/'tests/fixtures/gameplay_author/inline_prototype.json'),
                 '--source-kind','inline','--publish',carrier,'--output',str(target)])
            captured = target if carrier == 'cxc' else target/'generations'/(target/'cuexis.adopted').read_text(encoding='ascii').splitlines()[1]
            for encoding in ['packed','graph.json']:
                for hit in [False,True]:
                    command = [str(args.host.resolve()),'--content',str(captured),
                               '--candidate-entry','compiled/gameplay.'+encoding,
                               '--command-file',str(commands),'--gameplay-configuration',str(config),
                               '--gameplay-config-budget','131072,64,8192,16384,8192',
                               '--gameplay-h-step','250','--gameplay-presentation-step','7']
                    if hit:
                        command += ['--gameplay-observations',str(observation)]
                    output = run(command)
                    expected = 'score=2 combo=1 hits=1 misses=0' if hit else 'score=-1 combo=0 hits=0 misses=1'
                    assert output.count(expected+' completeReplay=same') >= 2, output
                    assert 'suppressedTicks=2 emittedTickFrames=8' in output, output
                    assert 'host.summary outcome=ok' in output, output
                    assert 'completeReplay=different' not in output
            # Parser rejects a noninteger horizon before opening content.
            bad = command.copy()
            bad[bad.index('--gameplay-h-step')+1] = '250.0'
            output = run(bad, False)
            assert 'host.open' not in output
            observation.write_text('900 1 press lane.one domain.binding.one keyboard 0 0 0 0 0 0 0\n900 1 release lane.one domain.binding.one keyboard 0 0 0 0 0 0 0\n',encoding='ascii')
            output = run(command, False)
            assert 'sequence must increase' in output
            observation.write_text('900 1 press lane.one domain.binding.one keyboard 100 200 300 400 0 0 0\n',encoding='ascii')
    print(f'Installed reference host matrix: {ordinal} real processes ({host_processes} installed Host + {assembler_processes} assembler); Packed/Graph CXC/captured-generation Hit/Miss, six verbs+tick, complete Replay, paused suppression and parser failures passed')


if __name__ == '__main__':
    main()
