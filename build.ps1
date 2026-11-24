# One-command local build entry point.
#
# Configure, build and test Cuexis without remembering the Visual Studio environment or the preset
# commands. Standard presets import the MSVC environment into this process; the mingw-* presets run
# in the current shell because they expect the MSYS2 toolchain instead.
#
# Examples:
#   .\build.ps1
#   .\build.ps1 -Target cuexis_core_tests
#   .\build.ps1 -Action test -Filter F135
#   .\build.ps1 -Preset candidate-debug -Action all
#   .\build.ps1 -Preset mingw-debug -Action build
#   .\build.ps1 -Fresh

[CmdletBinding()]
param(
    [string]$Preset = 'debug',
    [ValidateSet('configure', 'build', 'test', 'all')]
    [string]$Action = 'build',
    [string]$Target = '',
    [string]$Filter = '',
    [string]$Label = '',
    [int]$Parallel = 0,
    [string[]]$CMakeArg = @(),
    [switch]$Fresh
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $root
try {
    if ($Preset -notlike 'mingw*') {
        . (Join-Path $root 'tools\msvc-env.ps1')
        [void](Import-MsvcEnvironment)
    }

    function Invoke-Tool([string]$Executable, [string[]]$Arguments) {
        & $Executable @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "$Executable failed with exit code $LASTEXITCODE"
        }
    }

    $configured = Test-Path (Join-Path $root "out/build/$Preset/CMakeCache.txt")
    $needConfigure = $Fresh -or $Action -eq 'configure' -or
        (-not $configured -and $Action -in @('build', 'test', 'all'))
    if ($needConfigure) {
        Write-Host "== configure $Preset"
        $arguments = @('--preset', $Preset)
        if ($Fresh) {
            $arguments += '--fresh'
        }
        $arguments += $CMakeArg
        Invoke-Tool 'cmake' $arguments
    }

    if ($Action -in @('build', 'all')) {
        Write-Host "== build $Preset"
        $arguments = @('--build', '--preset', $Preset)
        if ($Target) {
            $arguments += @('--target', $Target)
        }
        if ($Parallel -gt 0) {
            $arguments += @('-j', $Parallel)
        }
        Invoke-Tool 'cmake' $arguments
    }

    if ($Action -in @('test', 'all')) {
        Write-Host "== test $Preset"
        $arguments = @('--preset', $Preset, '--no-tests=error', '--output-on-failure')
        if ($Filter) {
            $arguments += @('-R', $Filter)
        }
        if ($Label) {
            $arguments += @('-L', $Label)
        }
        if ($Parallel -gt 0) {
            $arguments += @('-j', $Parallel)
        }
        Invoke-Tool 'ctest' $arguments
    }
} finally {
    Pop-Location
}
