param(
    [ValidateSet('keyboard', 'audio')][string]$Mode = 'keyboard',
    [string]$Player = "$PSScriptRoot\..\out\build\candidate-debug\bin\cuexis_player.exe",
    [string]$Content = "$PSScriptRoot\..\out\device-acceptance",
    [string]$Repository = "$PSScriptRoot\.."
)
$ErrorActionPreference = 'Stop'
$playerPath = (Resolve-Path -LiteralPath $Player).Path
$contentPath = (Resolve-Path -LiteralPath $Content).Path
$runPath = Join-Path $contentPath ('runs\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $runPath -Force | Out-Null
$arguments = @('--cxc', (Join-Path $contentPath "$Mode.cxc"),
    '--candidate-entry', 'compiled/gameplay.packed', '--mode', $(if ($Mode -eq 'audio') { 'audio' } else { 'chart' }),
    '--gameplay-configuration', (Join-Path $contentPath "$Mode-configuration.json"),
    '--gameplay-config-budget', '131072,64,8192,16384,8192',
    '--gameplay-h-step', '1', '--gameplay-presentation-step', '1',
    '--gameplay-guide', (Join-Path $contentPath "$Mode-guide.txt"),
    '--gameplay-key', '7:lane.d:domain.binding.one', '--gameplay-key', '9:lane.f:domain.binding.one',
    '--gameplay-key', '13:lane.j:domain.binding.one', '--gameplay-key', '14:lane.k:domain.binding.one')
Write-Host "Four lanes: D F J K. Player waits for Space to start; Space also pauses/resumes."
Write-Host "Press the matching key when a falling note reaches the horizontal line."
Write-Host "Long K note: press at the head, HOLD, release when its tail reaches the line."
Write-Host "Hit/Miss and score appear in the Player window. No intentional Miss is required."
Write-Host "Test-only clock: each mapped transition uses one H Tick; idle frame uses one; T advances once per frame."
Write-Host "Listen to clicks separately; timing is not audio-calibrated."
Write-Host "R reload; arrows seek one Gameplay Tick; S stop; B rebuild; Esc quit. Logs: $runPath"
Read-Host 'Press Enter when ready to open the Player window' | Out-Null
$record = [ordered]@{ player = $playerPath; arguments = $arguments; mode = $Mode;
    started = (Get-Date).ToString('o'); computer = $env:COMPUTERNAME;
    playerSha256 = (Get-FileHash -LiteralPath $playerPath -Algorithm SHA256).Hash;
    packageSha256 = (Get-FileHash -LiteralPath (Join-Path $contentPath "$Mode.cxc") -Algorithm SHA256).Hash;
    physicalInput = 'manual; not injected'; outcome = 'pending owner observation' }
try { $record['sourceHead'] = (& git -C $Repository rev-parse HEAD); if ($LASTEXITCODE -ne 0) { $record['sourceHead'] = 'unavailable' } }
catch { $record['sourceHead'] = 'unavailable' }
try {
    $record['graphics'] = @(Get-CimInstance Win32_VideoController | Select-Object Name, DriverVersion, CurrentRefreshRate)
    $record['audioDevices'] = @(Get-CimInstance Win32_SoundDevice | Select-Object Name)
    $record['keyboardDevices'] = @(Get-CimInstance Win32_Keyboard | Select-Object Name, Description)
} catch { $record['deviceInventory'] = 'unavailable; fill manually' }
$record | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $runPath 'run.json') -Encoding utf8
$stdoutPath = Join-Path $runPath 'stdout.log'
$stderrPath = Join-Path $runPath 'stderr.log'
# PowerShell Start-Process joins ArgumentList; explicitly quote filesystem arguments.
$quotedArguments = $arguments | ForEach-Object { '"' + $_ + '"' }
$process = Start-Process -FilePath $playerPath -ArgumentList $quotedArguments -WindowStyle Normal -PassThru `
    -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
# Cache the handle before exit; Windows PowerShell otherwise may report a null ExitCode.
$processHandle = $process.Handle
try {
    while (-not $process.HasExited) {
        if (Test-Path -LiteralPath $stdoutPath) {
            $line = Get-Content -LiteralPath $stdoutPath -Tail 15 | Where-Object { $_ -match 'H=\d+.*score=' } | Select-Object -Last 1
            if ($line) { Write-Host "`r$line                     " -NoNewline }
        }
        Start-Sleep -Milliseconds 150
        $process.Refresh()
    }
} finally {
    if (-not $process.HasExited) { Stop-Process -Id $process.Id; $process.WaitForExit() }
    $process.WaitForExit()
    $process.Refresh()
    $record['ended'] = (Get-Date).ToString('o')
    $record['exitCode'] = $process.ExitCode
    $record | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $runPath 'run.json') -Encoding utf8
    Write-Host "`nSaved device run: $runPath"
}
