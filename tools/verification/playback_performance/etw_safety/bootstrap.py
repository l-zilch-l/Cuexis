"""No local import or trace action until every delivered source/input hash matches."""
import hashlib
import json
from pathlib import Path
import sys

EXPECTED_PROFILES = {'canary': ('lifecycle.wprp', 'PD5ToolLifecycle'),
                     'f1-scale': ('f1_scale.wprp', 'PD5F1Scale')}


def main():
    here = Path(__file__).resolve().parent
    binding = json.loads((here / 'binding.json').read_text(encoding='utf-8'))
    if binding.get('schema') != 'pd5.etw-tool-canary.binding.v3':
        raise RuntimeError('unexpected sealed tool binding schema')
    sources = {p.name for p in here.glob('*.py')}
    if sources != set(binding['sources']):
        raise RuntimeError('prepared Python source set changed')
    profiles = binding['profiles']
    if set(profiles) != set(EXPECTED_PROFILES):
        raise RuntimeError('unexpected sealed profile set')
    for key, (file_name, name) in EXPECTED_PROFILES.items():
        entry = profiles[key]
        if entry['file'] != file_name or entry['name'] != name:
            raise RuntimeError('unexpected sealed profile binding: ' + key)
        with (here / entry['file']).open('rb') as stream:
            if hashlib.file_digest(stream, 'sha256').hexdigest() != entry['sha256']:
                raise RuntimeError('prepared profile changed: ' + entry['file'])
    if binding['controller_source_sha256'] != binding['sources'].get('controller.py'):
        raise RuntimeError('sealed controller hash is inconsistent with the source set')
    for relative, expected in binding['artifact_sha256'].items():
        with (here / relative).open('rb') as stream:
            if hashlib.file_digest(stream, 'sha256').hexdigest() != expected:
                raise RuntimeError('prepared artifact changed: ' + relative)
    cases = binding.get('cases')
    if not isinstance(cases, list) or not {'normal', 'controller-death'} <= set(cases):
        raise RuntimeError('unexpected sealed case set')
    for name, expected in binding['sources'].items():
        with (here / name).open('rb') as stream:
            if hashlib.file_digest(stream, 'sha256').hexdigest() != expected:
                raise RuntimeError('prepared source changed: ' + name)
    if Path(sys.executable).resolve() != Path(binding['python']['path']).resolve():
        raise RuntimeError('different Python interpreter')
    for item in (binding['python'], binding['wpr']):
        with Path(item['path']).open('rb') as stream:
            if hashlib.file_digest(stream, 'sha256').hexdigest() != item['sha256']:
                raise RuntimeError('pinned executable changed: ' + item['path'])
    sys.path.insert(0, str(here))
    import controller
    return controller.main(here, binding)


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except Exception as error:
        print('[PD5 tools] FAILED BEFORE/AT ROLE: ' + repr(error), flush=True)
        raise SystemExit(2)
