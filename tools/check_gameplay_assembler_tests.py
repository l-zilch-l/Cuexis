"""Exercise the real Gameplay assembler CLI and compare complete artifacts independently."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import zipfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--tool', type=Path, required=True)
    parser.add_argument('--dispatch', action='store_true')
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--log-dir', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    args.log_dir.mkdir(parents=True, exist_ok=True)
    common = [str(args.tool.resolve())] + (['--gameplay'] if args.dispatch else []) + ['--base', str(root/'tests/fixtures/chart_format_update/golden/cxc_v1_v4_static.cxc'),
              '--foundation', str(args.fixture.resolve()/'foundation.packed'),
              '--configuration', str(args.fixture.resolve()/'configuration.json'),
              '--entry-id', 'chart.entry.one', '--test-only', 'true',
              '--configuration-budget', '131072,64,8192,16384,8192',
              '--graph-budget', '131072,64,8192,16384,8192,8192,65536']
    ordinal = 0

    def run(source, output, mode='cxc', parameters=(), failure=None, expect_success=True):
        nonlocal ordinal
        ordinal += 1
        cxt = source.endswith('.cxt')
        command = common + ['--source', str(root/'tests/fixtures/gameplay_author'/source),
                            '--source-kind', 'cxt' if cxt else 'inline', '--publish', mode, '--output', str(output)]
        if cxt:
            command += ['--binding', 'invocation.one', '--module', 'module.one', '--export', 'export.one']
        for parameter in parameters:
            command += ['--parameter', parameter]
        environment = os.environ.copy()
        if failure:
            environment.update(failure)
        completed = subprocess.run(command, capture_output=True, env=environment, timeout=120)
        log = args.log_dir/f'{ordinal:02d}-{Path(source).name.replace(".", "-")}.log'
        log.write_bytes(('command='+json.dumps(command)+'\nexit='+str(completed.returncode)+'\n').encode()+completed.stdout+completed.stderr)
        assert (completed.returncode == 0) == expect_success, str(log)
        return json.loads(completed.stdout) if expect_success else None

    def artifacts(path):
        with zipfile.ZipFile(path) as archive:
            values = {name: archive.read(name) for name in ('compiled/gameplay.packed','compiled/gameplay.graph.json')}
            metadata = json.loads(archive.read('cuexis.project.json'))['extensions']['cuexis.gameplay-entry.v1']['entries']
            assert len(metadata) == 2
            for entry in metadata:
                assert entry['artifactIdentity'] == hashlib.sha256(values[entry['path']]).hexdigest()
                assert entry['sourceOf']['kind'] == 'not-packaged'
            assert metadata[0]['compiledSemanticIdentity'] == metadata[1]['compiledSemanticIdentity']
            return values, metadata

    with tempfile.TemporaryDirectory(prefix='cuexis-gameplay-cli-') as temporary:
        work = Path(temporary)
        inline, cxt = work/'inline.cxc', work/'cxt.cxc'
        first = run('inline_prototype.json', inline)
        assert first['counts']['requirements'] == 1
        assert first['productionBudgetAccepted'] is False
        original = inline.read_bytes()
        run('inline_prototype.json', inline)
        assert inline.read_bytes() == original, 'Repeat publication changed exact CXC bytes'
        run('prototype.cxt', cxt)
        assert artifacts(inline) == artifacts(cxt), 'Inline/CXT complete carrier artifacts differ'
        canonical = None
        for source, parameters in [('inline_2x3.json',()), ('inline_2x3_permuted.json',()),
                                   ('pattern_2x3.cxt',('outerCount=2',)), ('pattern_2x3_permuted.cxt',('outerCount=2',))]:
            target = work/(source+'.cxc')
            report = run(source,target,parameters=parameters)
            assert report['counts']['requirements'] == 6
            current = artifacts(target)
            if canonical is None:
                canonical = current
            else:
                assert current == canonical, 'Whole artifact equivalence failed: '+source
        run('inline_prototype.json',inline,failure={'CUEXIS_ASSET_PUBLISH_FAIL_BEFORE_REPLACE':'1'},expect_success=False)
        assert inline.read_bytes() == original, 'Failed CXC publication replaced old bytes'
        # A malformed author input goes through the real compiler before any publication.
        bad = work/'bad.json'
        good_source = (root/'tests/fixtures/gameplay_author/inline_prototype.json').read_text(encoding='utf-8')
        bad.write_text(good_source.replace('"version": 5','"version": 4',1),encoding='utf-8')
        # Absolute source paths are accepted by pathlib composition in run().
        run(str(bad),inline,expect_success=False)
        assert inline.read_bytes() == original
        generations = work/'generations'
        run('inline_prototype.json',generations,mode='filesystem')
        adopted_before = (generations/'cuexis.adopted').read_bytes()
        captured_id = adopted_before.decode('ascii').splitlines()[1]
        captured = generations/'generations'/captured_id
        assert captured.is_dir(), captured
        captured_files = {path.relative_to(captured).as_posix():path.read_bytes() for path in captured.rglob('*') if path.is_file()}
        run('inline_2x3.json',generations,mode='filesystem')
        assert (generations/'cuexis.adopted').read_bytes() != adopted_before
        assert {path.relative_to(captured).as_posix():path.read_bytes() for path in captured.rglob('*') if path.is_file()} == captured_files
        current_adopted = (generations/'cuexis.adopted').read_bytes()
        run('inline_prototype.json',generations,mode='filesystem',failure={'CUEXIS_ASSET_PUBLISH_FAIL_BEFORE_ADOPT':'1'},expect_success=False)
        assert (generations/'cuexis.adopted').read_bytes() == current_adopted
        run('inline_prototype.json',generations,mode='filesystem',failure={'CUEXIS_ASSET_PUBLISH_FAIL_AFTER_ADOPT':'1'},expect_success=False)
        assert (generations/'cuexis.adopted').read_bytes() == adopted_before
        assert 'commitVisible=true' in (args.log_dir/f'{ordinal:02d}-inline_prototype-json.log').read_text(encoding='utf-8')
        with zipfile.ZipFile(root/'tests/fixtures/chart_format_update/golden/cxc_v1_v4_static.cxc') as archive:
            music_entries = {name:archive.read(name) for name in archive.namelist() if name != 'cuexis.cxc.json'}
        chart_path = 'assets/charts/main.cuexis.chart.json'
        chart = json.loads(music_entries[chart_path])
        chart['audio'] = {'version':1,'mainMusic':{'domain':'asset','id':'audio.main'}}
        music_entries[chart_path] = json.dumps(chart).encode()
        music_entries['assets/cuexis.asset-index.json'] = (root/'assets/projects/stage1d_project/assets/cuexis.asset-index.json').read_bytes()
        music_entries['assets/audio/main.wav'] = (root/'assets/projects/stage1d_project/assets/audio/main.wav').read_bytes()
        manifest = {'format':'cuexis.cxc','version':1,'project':'cuexis.project.json',
                    'entries':[{'path':name,'byteCount':len(value),'sha256':hashlib.sha256(value).hexdigest()}
                               for name,value in sorted(music_entries.items())],
                    'requiredExtensions':[], 'extensions':{}}
        music_base = work/'music-base.cxc'
        with zipfile.ZipFile(music_base,'w',compression=zipfile.ZIP_STORED) as archive:
            archive.writestr('cuexis.cxc.json',json.dumps(manifest).encode())
            for name,value in sorted(music_entries.items()):
                archive.writestr(name,value)
        common[common.index('--base')+1] = str(music_base)
        common[common.index('--foundation')+1] = str(args.fixture.resolve()/'music-foundation.packed')
        music_output = work/'music.cxc'
        music_report = run('inline_prototype.json',music_output)
        assert music_report['actualPrepareValidated'] is True
        with zipfile.ZipFile(music_output) as archive:
            assert archive.read('assets/audio/main.wav') == music_entries['assets/audio/main.wav']
        artifacts(music_output)

    print(f'Gameplay assembler: {ordinal} real CLI runs; complete Graph/Packed equivalence, deterministic CXC, rejected author, atomic CXC/generation and pinned reader passed')


if __name__ == '__main__':
    main()
