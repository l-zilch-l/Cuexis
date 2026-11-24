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

function Get-VsInstallationPath {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw 'vswhere.exe was not found; install Visual Studio with the x64 C++ toolchain.'
    }
    $path = (@(& $vswhere -latest -products '*' `
                -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
                -property installationPath) -join "`n").Trim()
    if (-not $path) {
        throw 'No Visual Studio installation with the x64 C++ toolchain was found.'
    }
    return $path
}

$previousVcpkgRoot = $env:VCPKG_ROOT
$installation = Get-VsInstallationPath
$devCmd = Join-Path $installation 'Common7/Tools/VsDevCmd.bat'
if (-not (Test-Path -LiteralPath $devCmd)) {
    throw "VsDevCmd.bat was not found under $installation"
}

$environment = & $env:ComSpec /d /c "call `"$devCmd`" -no_logo -arch=x64 -host_arch=x64 >nul && set"
if ($LASTEXITCODE -ne 0) {
    throw "VsDevCmd.bat failed with exit code $LASTEXITCODE"
}
foreach ($line in $environment) {
    $separator = $line.IndexOf('=')
    if ($separator -le 0) {
        continue
    }
    [Environment]::SetEnvironmentVariable(
        $line.Substring(0, $separator), $line.Substring($separator + 1), 'Process')
}
if ($previousVcpkgRoot) {
    $env:VCPKG_ROOT = $previousVcpkgRoot
}
if ($env:CUEXIS_VCPKG_ROOT) {
    if (-not (Test-Path -LiteralPath (Join-Path $env:CUEXIS_VCPKG_ROOT 'scripts/buildsystems/vcpkg.cmake'))) {
        throw "CUEXIS_VCPKG_ROOT does not look like a vcpkg checkout: $env:CUEXIS_VCPKG_ROOT"
    }
    $env:VCPKG_ROOT = $env:CUEXIS_VCPKG_ROOT
}

$compiler = Get-Command cl.exe -ErrorAction SilentlyContinue
if (-not $compiler) {
    throw 'cl.exe is still missing after loading the Visual Studio environment.'
}

$Command = @($args)
if ($Command.Count -eq 0) {
    Write-Host "MSVC environment ready: $($compiler.Source)"
    Write-Host "VCPKG_ROOT=$env:VCPKG_ROOT"
    exit 0
}

$arguments = @()
if ($Command.Count -gt 1) {
    $arguments = $Command[1..($Command.Count - 1)]
}
& $Command[0] @arguments
exit $LASTEXITCODE
