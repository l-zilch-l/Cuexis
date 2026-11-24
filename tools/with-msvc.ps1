# Load the Visual Studio x64 developer environment into this PowerShell process, then run a command.
#
# A plain shell on this machine has MSYS2 GCC ahead of MSVC on PATH and no Visual Studio variables,
# so `cmake --preset debug` detects GCC and the project rejects the x64-windows triplet. This helper
# finds Visual Studio through vswhere, imports the VsDevCmd environment, keeps the process
# VCPKG_ROOT, and then runs the command unchanged.
#
# Examples:
#   pwsh -NoProfile -File tools/with-msvc.ps1
#   pwsh -NoProfile -File tools/with-msvc.ps1 cmake --preset debug --fresh
#   pwsh -NoProfile -File tools/with-msvc.ps1 ctest --preset debug -R F135
#
# Run it with no command to only print the resolved compiler and vcpkg root. Set
# CUEXIS_VCPKG_ROOT to override the vcpkg checkout used for the run.

param()

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'msvc-env.ps1')
$compiler = Import-MsvcEnvironment

$Command = @($args)
if ($Command.Count -eq 0) {
    Write-Host "MSVC environment ready: $compiler"
    Write-Host "VCPKG_ROOT=$env:VCPKG_ROOT"
    exit 0
}

$arguments = @()
if ($Command.Count -gt 1) {
    $arguments = $Command[1..($Command.Count - 1)]
}
& $Command[0] @arguments
exit $LASTEXITCODE
