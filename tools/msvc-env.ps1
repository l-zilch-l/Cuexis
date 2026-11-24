# Import the Visual Studio x64 developer environment into the current PowerShell process.
#
# Dot-source this file, then call Import-MsvcEnvironment:
#   . (Join-Path $PSScriptRoot 'msvc-env.ps1')
#   $compiler = Import-MsvcEnvironment
#
# The function keeps the process VCPKG_ROOT and rejects a CUEXIS_VCPKG_ROOT that is not a checkout.
# It returns the resolved cl.exe path.

function Import-MsvcEnvironment {
    $previousVcpkgRoot = $env:VCPKG_ROOT

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw 'vswhere.exe was not found; install Visual Studio with the x64 C++ toolchain.'
    }
    $installation = (@(& $vswhere -latest -products '*' `
                -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
                -property installationPath) -join "`n").Trim()
    if (-not $installation) {
        throw 'No Visual Studio installation with the x64 C++ toolchain was found.'
    }

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
        $vcpkgToolchain = Join-Path $env:CUEXIS_VCPKG_ROOT 'scripts/buildsystems/vcpkg.cmake'
        if (-not (Test-Path -LiteralPath $vcpkgToolchain)) {
            throw "CUEXIS_VCPKG_ROOT does not look like a vcpkg checkout: $env:CUEXIS_VCPKG_ROOT"
        }
        $env:VCPKG_ROOT = $env:CUEXIS_VCPKG_ROOT
    }

    $compiler = Get-Command cl.exe -ErrorAction SilentlyContinue
    if (-not $compiler) {
        throw 'cl.exe is still missing after loading the Visual Studio environment.'
    }
    return $compiler.Source
}
