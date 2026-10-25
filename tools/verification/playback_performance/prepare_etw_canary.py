"""Seal a new machine-bound, tool-only ETW canary package; never run it."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def prepare(output, wpr):
    output = output.resolve()
    wpr = wpr.resolve(strict=True)
    source = Path(__file__).resolve().with_name('etw_safety')
    # A preparation is create-new, never an update of an existing sealed package.
    output.mkdir(exist_ok=False)
    for item in sorted(source.glob('*.py')):
        shutil.copyfile(item, output / item.name)
    shutil.copyfile(source / 'README.md', output / 'README.md')
    shutil.copyfile(source / 'lifecycle.wprp', output / 'lifecycle.wprp')
    binding = {'schema': 'pd5.etw-tool-canary.binding.v2', 'formal': False,
               'profile': {'file': 'lifecycle.wprp', 'name': 'PD5ToolLifecycle', 'sha256': sha(output / 'lifecycle.wprp')},
               'native_launches': 0, 'sources': {p.name: sha(p) for p in output.glob('*.py')},
               'python': {'path': str(Path(sys.executable).resolve()), 'sha256': sha(sys.executable)},
               'wpr': {'path': str(wpr), 'sha256': sha(wpr)}}
    (output / 'binding.json').write_text(json.dumps(binding, indent=2) + '\n', encoding='utf-8')
    binding_sha, bootstrap_sha = sha(output / 'binding.json'), sha(output / 'bootstrap.py')
    python = str(Path(sys.executable).resolve()).replace("'", "''")
    # Launcher checks hashes before it executes any delivered Python source.
    launcher = f"""$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
if ((Get-FileHash -LiteralPath (Join-Path $taskRoot 'binding.json') -Algorithm SHA256).Hash.ToLowerInvariant() -ne '{binding_sha}') {{ throw 'Binding changed; do not run.' }}
if ((Get-FileHash -LiteralPath (Join-Path $taskRoot 'bootstrap.py') -Algorithm SHA256).Hash.ToLowerInvariant() -ne '{bootstrap_sha}') {{ throw 'Bootstrap changed; do not run.' }}
$taskPython = '{python}'
if ((Get-FileHash -LiteralPath $taskPython -Algorithm SHA256).Hash.ToLowerInvariant() -ne '{binding['python']['sha256']}') {{ throw 'Python changed; do not execute.' }}
$taskOutput = Join-Path $taskRoot ('canary-' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ') + '-' + [Guid]::NewGuid().ToString('N'))
Write-Host '[PD5 tools] No automatic elevation. Two minimal-profile lifecycle cases; no Power/Cuexis/F1/40k launches.'
& $taskPython -B -I -S -u (Join-Path $taskRoot 'bootstrap.py') canary $taskOutput
$taskExit = $LASTEXITCODE
Write-Host ('[PD5 tools] Exit ' + $taskExit + '; evidence: ' + $taskOutput)
exit $taskExit
"""
    (output / 'run_tool_canary.ps1').write_text(launcher, encoding='utf-8-sig')
    print(json.dumps({'package': str(output), 'binding_sha256': binding_sha,
                      'launcher_sha256': sha(output / 'run_tool_canary.ps1'),
                      'executed': False}, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--wpr', required=True, type=Path)
    args = parser.parse_args()
    prepare(args.output, args.wpr)
