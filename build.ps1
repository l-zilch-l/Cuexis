# One-command local build entry point.
#
# Configure, build and test Cuexis without remembering the Visual Studio environment or the preset
# commands. Standard presets load MSVC through tools/with-msvc.cmd; the mingw-* presets run in the
# current shell because they expect the MSYS2 toolchain instead.
#
# Examples:
#   .\build.ps1
#   .\build.ps1 -Preset release
#   .\build.ps1 -Action test -Filter F135
#   .\build.ps1 -Preset mingw-debug -Action all
#   .\build.ps1 -Fresh

[CmdletBinding()]
param(
    [string]$Preset = 'debug',
    [ValidateSet('configure', 'build', 'test', 'all')]
    [string]$Action = 'build',
    [string]$Filter = '',
    [switch]$Fresh
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $root
try {
    function Invoke-Tool([string]$Executable, [string[]]$Arguments) {
        if ($Preset -like 'mingw*') {
            & $Executable @Arguments
        } else {
            & (Join-Path $root 'tools\with-msvc.cmd') $Executable @Arguments
        }
        if ($LASTEXITCODE -ne 0) {
            throw "$Executable failed with exit code $LASTEXITCODE"
        }
    }

    $configured = Test-Path (Join-Path $root "out/build/$Preset/CMakeCache.txt")
    if ($Fresh -or $Action -eq 'configure' -or -not $configured) {
        Write-Host "== configure $Preset"
        $arguments = @('--preset', $Preset)
        if ($Fresh) {
            $arguments += '--fresh'
        }
        Invoke-Tool 'cmake' $arguments
    }

    if ($Action -in @('build', 'test', 'all')) {
        Write-Host "== build $Preset"
        Invoke-Tool 'cmake' @('--build', '--preset', $Preset)
    }

    if ($Action -in @('test', 'all')) {
        Write-Host "== test $Preset"
        $arguments = @('--preset', $Preset, '--no-tests=error', '--output-on-failure')
        if ($Filter) {
            $arguments += @('-R', $Filter)
        }
        Invoke-Tool 'ctest' $arguments
    }
} finally {
    Pop-Location
}
