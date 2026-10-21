"""Read-only historical evidence audit; never certifies current-source registration."""
import argparse
import hashlib
import json
from pathlib import Path


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def audit(root):
    root = Path(root).resolve()
    formal = root / 'out/performance-formal-20261020'
    result = {'scope': 'historical-integrity-only', 'current_source_eligible': None,
              'snapshots': {}, 'bound_build_files': 0}
    for name in ('pause-evidence-hashes.json', 'pause-after-window2-evidence-hashes.json'):
        files = json.loads((formal / name).read_text(encoding='utf-8-sig'))['files']
        for relative, expected in files.items():
            path = (formal / relative).resolve()
            path.relative_to(formal)
            if sha(path) != expected:
                raise ValueError('Historical hash mismatch: ' + str(path))
        result['snapshots'][name] = len(files)
    preparation = formal / 'owner-prepare-c422bfbfc996/preparation.json'
    manifest = json.loads(preparation.read_text(encoding='utf-8-sig'))
    checked = set()
    for config in manifest['builds'].values():
        for binding in config.values():
            for side in ('A', 'B'):
                saved = binding[side]
                exe = Path(saved['executable'])
                targets = {exe: saved['executable_sha256']}
                if side == 'A':
                    targets[Path(saved['implementation_archive'])] = saved['implementation_archive_sha256']
                else:
                    for key in ('metadata_hashes', 'object_hashes', 'library_hashes'):
                        targets.update({exe.parent.parent / p: h for p, h in
                                        saved['transitive_inputs'][key].items()})
                for path, expected in targets.items():
                    identity = (str(path.resolve()).casefold(), expected)
                    if identity not in checked:
                        if sha(path) != expected:
                            raise ValueError('Historical build mismatch: ' + str(path))
                        checked.add(identity)
    result['bound_build_files'] = len(checked)
    registration = preparation.parent / 'registration-071516aef126'
    if (registration / 'window-3.claim.json').exists():
        raise ValueError('Stopped window 3 has a claim')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[3])
    print(json.dumps(audit(parser.parse_args().root), indent=2))
