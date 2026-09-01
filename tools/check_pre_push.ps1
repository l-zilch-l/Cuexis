#Requires -Version 7.0

[CmdletBinding()]
param(
    [ValidateSet("Quick", "Linux", "Full")]
    [string]$Mode = "Full",

    [string]$WslDistribution = "Ubuntu",

    [string]$WslVcpkgRoot = "",

    [string]$WindowsVcpkgRoot = $env:VCPKG_ROOT,

    [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$script:RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$script:BuildRoot = Join-Path $script:RepoRoot "out/build/pre-push"
$script:PinnedVcpkgCommit = "40f3c709db80acf154ac4b17a1f83c564ebd022e"
$script:StartTime = Get-Date

function Format-CommandArgument {
    param([Parameter(Mandatory)][string]$Value)

    if ($Value -match '[\s"]') {
        return '"' + $Value.Replace('"', '\"') + '"'
    }
    return $Value
}

function Write-CommandLine {
    param(
        [Parameter(Mandatory)][string]$Executable,
        [Parameter(Mandatory)][string[]]$Arguments
    )

    $displayArguments = $Arguments
    if ($Arguments.Count -gt 12) {
        $displayArguments = @($Arguments[0..9]) + "... [$($Arguments.Count - 10) more arguments]"
    }
    $rendered = @($Executable) + ($displayArguments | ForEach-Object { Format-CommandArgument $_ })
    Write-Host ("  > " + ($rendered -join " ")) -ForegroundColor DarkGray
}

function Invoke-ExternalCommand {
    param(
        [Parameter(Mandatory)][string]$Label,
        [Parameter(Mandatory)][string]$Executable,
        [Parameter(Mandatory)][string[]]$Arguments,
        [string]$WorkingDirectory = $script:RepoRoot
    )

    Write-Host "`n[pre-push] $Label" -ForegroundColor Cyan
    Write-CommandLine -Executable $Executable -Arguments $Arguments
    if ($DryRun) {
        return
    }

    Push-Location $WorkingDirectory
    try {
        & $Executable @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "$Label failed with exit code $LASTEXITCODE"
        }
    }
    finally {
        Pop-Location
    }
}

function Get-CommandOutput {
    param(
        [Parameter(Mandatory)][string]$Label,
        [Parameter(Mandatory)][string]$Executable,
        [Parameter(Mandatory)][string[]]$Arguments,
        [string]$WorkingDirectory = $script:RepoRoot
    )

    Push-Location $WorkingDirectory
    try {
        $output = & $Executable @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "$Label failed with exit code $LASTEXITCODE"
        }
        return ($output -join "`n").Trim()
    }
    finally {
        Pop-Location
    }
}

function Require-Command {
    param([Parameter(Mandatory)][string]$Name)

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -eq $command) {
        throw "Required command was not found: $Name"
    }
    return $command.Source
}

function Assert-NoMergeConflicts {
    $conflicts = Get-CommandOutput -Label "Unmerged path check" -Executable "git" -Arguments @(
        "diff", "--name-only", "--diff-filter=U"
    )
    if ($conflicts) {
        throw "Unmerged paths are present:`n$conflicts"
    }
}

function Get-FormatFiles {
    $files = [System.Collections.Generic.List[string]]::new()
    foreach ($directory in @("app", "engine", "tests", "tools")) {
        $path = Join-Path $script:RepoRoot $directory
        if (-not (Test-Path -LiteralPath $path)) {
            continue
        }
        Get-ChildItem -LiteralPath $path -Recurse -File | Where-Object {
            $_.Name.EndsWith(".cpp", [StringComparison]::OrdinalIgnoreCase) -or
            $_.Name.EndsWith(".hpp", [StringComparison]::OrdinalIgnoreCase)
        } | ForEach-Object { $files.Add($_.FullName) }
    }

    $cmakePath = Join-Path $script:RepoRoot "cmake"
    Get-ChildItem -LiteralPath $cmakePath -Recurse -File -Filter "*.hpp.in" |
        ForEach-Object { $files.Add($_.FullName) }

    return @($files | Sort-Object -Unique)
}

function Invoke-FormatCheck {
    $clangFormat = Require-Command "clang-format"
    $files = Get-FormatFiles
    if ($files.Count -eq 0) {
        throw "No source files were found for the format check"
    }

    $formatRoot = Join-Path $script:BuildRoot "format-$PID"
    $fullBuildRoot = [IO.Path]::GetFullPath($script:BuildRoot).TrimEnd('\', '/')
    $fullFormatRoot = [IO.Path]::GetFullPath($formatRoot)
    $expectedPrefix = $fullBuildRoot + [IO.Path]::DirectorySeparatorChar
    if (-not $fullFormatRoot.StartsWith($expectedPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe temporary format directory: $fullFormatRoot"
    }

    $normalizedFiles = foreach ($file in $files) {
        $relativePath = [IO.Path]::GetRelativePath($script:RepoRoot, $file)
        Join-Path $fullFormatRoot $relativePath
    }

    try {
        if (-not $DryRun) {
            if (Test-Path -LiteralPath $fullFormatRoot) {
                Remove-Item -LiteralPath $fullFormatRoot -Recurse -Force
            }
            New-Item -ItemType Directory -Path $fullFormatRoot -Force | Out-Null
            Copy-Item -LiteralPath (Join-Path $script:RepoRoot ".clang-format") `
                -Destination $fullFormatRoot

            for ($index = 0; $index -lt $files.Count; ++$index) {
                $destination = $normalizedFiles[$index]
                New-Item -ItemType Directory -Path (Split-Path $destination -Parent) `
                    -Force | Out-Null
                $contents = [IO.File]::ReadAllText($files[$index])
                $contents = $contents.Replace("`r`n", "`n").Replace("`r", "`n")
                [IO.File]::WriteAllText(
                    $destination,
                    $contents,
                    [Text.UTF8Encoding]::new($false)
                )
            }
        }

        $batchSize = 80
        for ($offset = 0; $offset -lt $normalizedFiles.Count; $offset += $batchSize) {
            $last = [Math]::Min($offset + $batchSize - 1, $normalizedFiles.Count - 1)
            $batch = @($normalizedFiles[$offset..$last])
            Invoke-ExternalCommand -Label "clang-format batch $($offset / $batchSize + 1)" `
                -Executable $clangFormat -Arguments (@("--dry-run", "--Werror") + $batch)
        }
    }
    finally {
        if (-not $DryRun -and (Test-Path -LiteralPath $fullFormatRoot)) {
            Remove-Item -LiteralPath $fullFormatRoot -Recurse -Force
        }
    }
}

function Invoke-QuickChecks {
    $python = Require-Command "python"
    Require-Command "git" | Out-Null

    Invoke-ExternalCommand -Label "Version consistency" -Executable $python -Arguments @(
        "-B", "tools/update_version.py", "--check"
    )
    Invoke-ExternalCommand -Label "Documentation contracts" -Executable $python -Arguments @(
        "-B", "tools/check_docs.py"
    )
    Invoke-ExternalCommand -Label "Staged whitespace check" -Executable "git" -Arguments @(
        "diff", "--cached", "--check"
    )
    Invoke-ExternalCommand -Label "Working tree whitespace check" -Executable "git" -Arguments @(
        "diff", "--check"
    )
    if (-not $DryRun) {
        Assert-NoMergeConflicts
    }
    Invoke-FormatCheck
}

function Initialize-MsvcEnvironment {
    if ($null -ne (Get-Command "cl.exe" -ErrorAction SilentlyContinue)) {
        return
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio/Installer/vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw "MSVC was not found and vswhere.exe is unavailable"
    }

    $installationPath = (& $vswhere -latest -products "*" `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath | Select-Object -First 1).Trim()
    if (-not $installationPath) {
        throw "Visual Studio with the x64 C++ toolchain was not found"
    }

    $devCmd = Join-Path $installationPath "Common7/Tools/VsDevCmd.bat"
    $command = "call `"$devCmd`" -no_logo -arch=x64 -host_arch=x64 >nul && set"
    $environment = & $env:ComSpec /d /c $command
    if ($LASTEXITCODE -ne 0) {
        throw "VsDevCmd.bat failed with exit code $LASTEXITCODE"
    }

    foreach ($line in $environment) {
        $separator = $line.IndexOf("=")
        if ($separator -le 0) {
            continue
        }
        $name = $line.Substring(0, $separator)
        $value = $line.Substring($separator + 1)
        [Environment]::SetEnvironmentVariable($name, $value, "Process")
    }

    Require-Command "cl.exe" | Out-Null
}

function Assert-WindowsVcpkg {
    if (-not $WindowsVcpkgRoot) {
        throw "Windows VCPKG_ROOT is not set; pass -WindowsVcpkgRoot or set VCPKG_ROOT"
    }

    $vcpkg = Join-Path $WindowsVcpkgRoot "vcpkg.exe"
    $toolchain = Join-Path $WindowsVcpkgRoot "scripts/buildsystems/vcpkg.cmake"
    if (-not (Test-Path -LiteralPath $vcpkg) -or -not (Test-Path -LiteralPath $toolchain)) {
        throw "Windows vcpkg is incomplete: $WindowsVcpkgRoot"
    }

    $commit = Get-CommandOutput -Label "Windows vcpkg revision" -Executable "git" -Arguments @(
        "-C", $WindowsVcpkgRoot, "rev-parse", "HEAD"
    )
    if ($commit -ne $script:PinnedVcpkgCommit) {
        throw "Windows vcpkg must be at $script:PinnedVcpkgCommit, found $commit"
    }

    $env:VCPKG_ROOT = $WindowsVcpkgRoot
}

function Invoke-WindowsLane {
    param(
        [Parameter(Mandatory)][ValidateSet("debug", "release")][string]$Preset,
        [Parameter(Mandatory)][string]$HeadSha
    )

    $buildDirectory = Join-Path $script:BuildRoot "windows-msvc-$Preset"
    Invoke-ExternalCommand -Label "Configure Windows MSVC $Preset" -Executable "cmake" -Arguments @(
        "--preset", $Preset,
        "--fresh",
        "-B", $buildDirectory,
        "-DCMAKE_CXX_COMPILER=cl",
        "-DCUEXIS_IMPLEMENTATION_SHA=$HeadSha",
        "-DVCPKG_TARGET_TRIPLET=x64-windows"
    )
    Invoke-ExternalCommand -Label "Build Windows MSVC $Preset" -Executable "cmake" -Arguments @(
        "--build", $buildDirectory, "--clean-first"
    )
    Invoke-ExternalCommand -Label "Test Windows MSVC $Preset" -Executable "ctest" -Arguments @(
        "--test-dir", $buildDirectory, "--no-tests=error", "--output-on-failure"
    )
}

function Get-WslPath {
    param([Parameter(Mandatory)][string]$WindowsPath)

    # wsl.exe strips backslashes while forwarding arguments; normalize first so
    # wslpath receives a valid Windows path.
    $normalizedPath = $WindowsPath -replace '\\', '/'
    return Get-CommandOutput -Label "WSL path conversion" -Executable "wsl.exe" -Arguments @(
        "-d", $WslDistribution, "--", "wslpath", "-a", "-u", $normalizedPath
    )
}

function Test-WslExecutable {
    param([Parameter(Mandatory)][string]$Path)

    & wsl.exe -d $WslDistribution -- test -x $Path
    return $LASTEXITCODE -eq 0
}

function Resolve-WslVcpkgRoot {
    if ($WslVcpkgRoot) {
        return $WslVcpkgRoot.TrimEnd("/")
    }

    $userName = Get-CommandOutput -Label "WSL user lookup" -Executable "wsl.exe" -Arguments @(
        "-d", $WslDistribution, "--", "id", "-un"
    )
    foreach ($candidate in @(
            "/home/$userName/vcpkg-cuexis",
            "/home/$userName/cuexis-vcpkg",
            "/opt/vcpkg"
        )) {
        if (Test-WslExecutable "$candidate/vcpkg") {
            return $candidate
        }
    }

    throw "WSL vcpkg was not found; pass -WslVcpkgRoot"
}

function Assert-WslToolchain {
    param([Parameter(Mandatory)][string]$VcpkgRoot)

    foreach ($tool in @("cmake", "ctest", "ninja", "gcc-13", "g++-13", "git")) {
        & wsl.exe -d $WslDistribution -- which $tool | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "Required WSL command was not found: $tool"
        }
    }

    if (-not (Test-WslExecutable "$VcpkgRoot/vcpkg")) {
        throw "WSL vcpkg executable was not found: $VcpkgRoot/vcpkg"
    }

    $commit = Get-CommandOutput -Label "WSL vcpkg revision" -Executable "wsl.exe" -Arguments @(
        "-d", $WslDistribution, "--", "git", "-C", $VcpkgRoot, "rev-parse", "HEAD"
    )
    if ($commit -ne $script:PinnedVcpkgCommit) {
        throw "WSL vcpkg must be at $script:PinnedVcpkgCommit, found $commit"
    }
}

function Invoke-WslCommand {
    param(
        [Parameter(Mandatory)][string]$Label,
        [Parameter(Mandatory)][string]$LinuxRepoRoot,
        [Parameter(Mandatory)][string]$VcpkgRoot,
        [Parameter(Mandatory)][string[]]$Arguments,
        [string[]]$EnvironmentVariables = @()
    )

    Invoke-ExternalCommand -Label $Label -Executable "wsl.exe" -Arguments (@(
            "-d", $WslDistribution,
            "--cd", $LinuxRepoRoot,
            "--", "env", "VCPKG_ROOT=$VcpkgRoot"
        ) + $EnvironmentVariables + $Arguments)
}

function Invoke-LinuxLane {
    param(
        [Parameter(Mandatory)][ValidateSet("headless-release", "headless-shared-release")]
        [string]$Preset,
        [Parameter(Mandatory)][string]$LinuxRepoRoot,
        [Parameter(Mandatory)][string]$VcpkgRoot,
        [Parameter(Mandatory)][string]$HeadSha
    )

    $suffix = if ($Preset -eq "headless-release") { "static" } else { "shared" }
    $buildDirectory = "$LinuxRepoRoot/out/build/pre-push/linux-gcc-$suffix-release"
    $developerTools = if ($Preset -eq "headless-release") { "ON" } else { "OFF" }
    $compilerEnvironment = @("CC=gcc-13", "CXX=g++-13")

    Invoke-WslCommand -Label "Configure WSL GCC 13 $suffix Release" `
        -LinuxRepoRoot $LinuxRepoRoot -VcpkgRoot $VcpkgRoot `
        -EnvironmentVariables $compilerEnvironment -Arguments @(
            "cmake", "--preset", $Preset, "--fresh",
            "-B", $buildDirectory,
            "-DCMAKE_CXX_COMPILER=g++-13",
            "-DCMAKE_MAKE_PROGRAM=ninja",
            "-DCUEXIS_BUILD_DEVELOPER_TOOLS=$developerTools",
            "-DCUEXIS_IMPLEMENTATION_SHA=$HeadSha",
            "-DVCPKG_TARGET_TRIPLET=x64-linux"
        )

    Invoke-WslCommand -Label "Build WSL GCC 13 $suffix Release" `
        -LinuxRepoRoot $LinuxRepoRoot -VcpkgRoot $VcpkgRoot `
        -EnvironmentVariables $compilerEnvironment -Arguments @(
            "cmake", "--build", $buildDirectory, "--clean-first"
        )
    Invoke-WslCommand -Label "Test WSL GCC 13 $suffix Release" `
        -LinuxRepoRoot $LinuxRepoRoot -VcpkgRoot $VcpkgRoot `
        -EnvironmentVariables $compilerEnvironment -Arguments @(
            "ctest", "--test-dir", $buildDirectory, "--no-tests=error", "--output-on-failure"
        )
}

try {
    Write-Host "Cuexis pre-push checks" -ForegroundColor Green
    Write-Host "  mode: $Mode"
    Write-Host "  repository: $script:RepoRoot"
    Write-Host "  dry run: $DryRun"

    Invoke-QuickChecks

    $headSha = ""
    if ($Mode -ne "Quick") {
        $headSha = Get-CommandOutput -Label "HEAD revision" -Executable "git" -Arguments @(
            "rev-parse", "HEAD"
        )
    }

    if ($Mode -eq "Full") {
        Require-Command "cmake" | Out-Null
        Require-Command "ctest" | Out-Null
        Initialize-MsvcEnvironment
        Assert-WindowsVcpkg
        Invoke-WindowsLane -Preset "debug" -HeadSha $headSha
        Invoke-WindowsLane -Preset "release" -HeadSha $headSha
    }

    if ($Mode -in @("Linux", "Full")) {
        Require-Command "wsl.exe" | Out-Null
        $linuxRepoRoot = Get-WslPath $script:RepoRoot
        $resolvedWslVcpkgRoot = Resolve-WslVcpkgRoot
        Assert-WslToolchain -VcpkgRoot $resolvedWslVcpkgRoot
        Invoke-LinuxLane -Preset "headless-release" -LinuxRepoRoot $linuxRepoRoot `
            -VcpkgRoot $resolvedWslVcpkgRoot -HeadSha $headSha
        Invoke-LinuxLane -Preset "headless-shared-release" -LinuxRepoRoot $linuxRepoRoot `
            -VcpkgRoot $resolvedWslVcpkgRoot -HeadSha $headSha
    }

    $elapsed = (Get-Date) - $script:StartTime
    Write-Host "`n[pre-push] All requested checks passed in $($elapsed.ToString('hh\:mm\:ss'))." `
        -ForegroundColor Green
}
catch {
    $elapsed = (Get-Date) - $script:StartTime
    Write-Error "Pre-push checks failed after $($elapsed.ToString('hh\:mm\:ss')): $($_.Exception.Message)"
    exit 1
}
