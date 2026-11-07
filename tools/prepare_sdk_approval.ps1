param(
    [string]$Repository = 'l-zilch-l/Cuexis',
    [int]$PullRequest = 32,
    [switch]$PrepareOnly
)

$ErrorActionPreference = 'Stop'

function Invoke-GhRequest([string]$Method, [string]$Endpoint, [string]$InputFile = '') {
    $arguments = @('api', '--hostname', 'github.com', '--method', $Method, $Endpoint)
    if ($InputFile) { $arguments += @('--input', $InputFile) }
    $attempts = if ($Method -eq 'GET') { 3 } else { 1 }
    $errorFile = [IO.Path]::GetTempFileName()
    try {
        for ($attempt = 1; $attempt -le $attempts; $attempt++) {
            # Windows PowerShell treats native stderr as error records. Keep the
            # real exit code and diagnostic instead of reporting every failure as auth.
            $ErrorActionPreference = 'Continue'
            $output = & gh @arguments 2> $errorFile
            $code = $LASTEXITCODE
            $ErrorActionPreference = 'Stop'
            if ($code -eq 0) { return ($output -join "`n" | ConvertFrom-Json) }
            $detail = [IO.File]::ReadAllText($errorFile)
            if ($Method -ne 'GET' -or $attempt -eq $attempts -or
                $detail -notmatch 'timeout|timed out|connectex|connection|EOF|HTTP 50[234]') {
                throw "GitHub $Method failed: $Endpoint`n$detail"
            }
            Write-Host "Temporary GitHub connection failure; retry $attempt of $($attempts - 1) ..."
            Start-Sleep -Seconds 2
        }
    } finally { Remove-Item -LiteralPath $errorFile -Force }
}

function Read-GitHubJson([string]$Endpoint) {
    return (Invoke-GhRequest 'GET' $Endpoint)
}

function Enable-ApprovalProxy {
    # Process-local only; never rewrite the user's Windows or Git settings.
    $env:HTTPS_PROXY = 'http://127.0.0.1:7890'
    $env:NO_PROXY = ''
    Write-Host 'Using proxy http://127.0.0.1:7890 for this run.'
}

function Read-RemoteText([string]$Repo, [string]$Path, [string]$Ref) {
    $file = Read-GitHubJson "repos/$Repo/contents/${Path}?ref=$Ref"
    if ($file.encoding -ne 'base64' -or $file.type -ne 'file') {
        throw "Expected a GitHub file: $Path"
    }
    return [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($file.content))
}

function Read-Version([string]$Cmake, [string]$Manifest) {
    $parts = @()
    foreach ($name in @('YEAR', 'MONTH', 'DAY', 'BUILD')) {
        $matches = [regex]::Matches($Cmake, "(?m)^set\(CUEXIS_VERSION_$name ([0-9]+)\)\r?$")
        if ($matches.Count -ne 1) { throw "Missing or ambiguous version component: $name" }
        $parts += [int]$matches[0].Groups[1].Value
    }
    $sdk = [regex]::Matches($Cmake, '(?m)^set\(CUEXIS_SDK_API_VERSION "([0-9]+\.[0-9]+\.[0-9]+)"\)\r?$')
    if ($sdk.Count -ne 1) { throw 'Missing or ambiguous SDK API version.' }
    if ($parts[0] -gt 99 -or $parts[3] -lt 1) { throw 'Invalid date build identity.' }
    $date = [datetime]::new(2000 + $parts[0], $parts[1], $parts[2])
    $version = '{0:00}.{1:00}.{2:00}-{3}' -f $parts[0], $parts[1], $parts[2], $parts[3]
    if (($Manifest | ConvertFrom-Json).'version-string' -cne $version) {
        throw 'Remote CMake and vcpkg version identities differ.'
    }
    return @{ Date = $date; Build = $parts[3]; Sdk = $sdk[0].Groups[1].Value }
}

function Read-ApprovalRecord([string]$Repo, [int]$Pr) {
    if ($Repo -notmatch '^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$' -or $Pr -lt 1) {
        throw 'Invalid repository or PR number.'
    }
    $today = [datetime]::UtcNow.Date
    $pull = Read-GitHubJson "repos/$Repo/pulls/$Pr"
    if ($pull.state -ne 'open' -or $pull.base.ref -ne 'master' -or
        $pull.base.repo.full_name -cne $Repo) {
        throw 'An open PR targeting this repository master is required.'
    }
    $base = $pull.base.sha
    $head = $pull.head.sha
    foreach ($sha in @($base, $head)) {
        if ($sha -cnotmatch '^[0-9a-f]{40}$') { throw 'Expected full Git commit SHA.' }
    }
    $owners = Read-RemoteText $Repo '.github/sdk-api-owners.json' $base | ConvertFrom-Json
    if ($owners.format -cne 'cuexis.sdk-api-owners' -or $owners.version -ne 1 -or
        $owners.repository -cne $Repo -or @($owners.owners).Count -lt 1) {
        throw 'Trusted base owner registry is invalid.'
    }
    $actor = Read-GitHubJson 'user'
    if ($actor.type -cne 'User' -or $owners.owners -cnotcontains $actor.login) {
        throw 'Log gh in as a human owner listed at the trusted base.'
    }
    $commit = Read-GitHubJson "repos/$Repo/git/commits/$head"
    if ($commit.sha -cne $head -or $commit.tree.sha -cnotmatch '^[0-9a-f]{40}$') {
        throw 'Candidate commit/tree could not be verified.'
    }
    $comparison = Read-GitHubJson "repos/$Repo/compare/$base...$head"
    if ($comparison.merge_base_commit.sha -cne $base) {
        throw 'The PR must include its current base. Update the branch first.'
    }
    $baseVersion = Read-Version (Read-RemoteText $Repo 'cmake/CuexisVersion.cmake' $base) `
        (Read-RemoteText $Repo 'vcpkg.json' $base)
    $headVersion = Read-Version (Read-RemoteText $Repo 'cmake/CuexisVersion.cmake' $head) `
        (Read-RemoteText $Repo 'vcpkg.json' $head)
    if ($headVersion.Sdk -ceq $baseVersion.Sdk) { throw 'No SDK transition: owner approval is not needed.' }
    if ($headVersion.Date -ne $today -or $baseVersion.Date -gt $today) {
        throw 'Remote candidate date is stale or base date is in the future. Update the version and push first.'
    }
    $expectedBuild = if ($baseVersion.Date -eq $today) { $baseVersion.Build + 1 } else { 1 }
    if ($headVersion.Build -ne $expectedBuild) { throw 'Remote candidate build must advance exactly from the base.' }

    # Re-read mutable state after fetching immutable commit-bound files.
    $final = Read-GitHubJson "repos/$Repo/pulls/$Pr"
    if ($final.state -ne 'open' -or $final.base.ref -ne 'master' -or
        $final.base.repo.full_name -cne $Repo -or $final.base.sha -cne $base -or
        $final.head.sha -cne $head -or [datetime]::UtcNow.Date -ne $today) {
        throw 'PR or UTC date changed while preparing. Run the assistant again.'
    }
    return [ordered]@{
        repository = $Repo; pr = $Pr; base_sha = $base; candidate_sha = $head
        candidate_tree_sha = $commit.tree.sha; from = $baseVersion.Sdk
        to = $headVersion.Sdk; utc_date = $today.ToString('yyyy-MM-dd')
    }
}

function Save-ApprovalDraft($Record, [string]$Directory) {
    [void][IO.Directory]::CreateDirectory($Directory)
    $path = Join-Path $Directory "pr-$($Record.pr)-$($Record.candidate_sha).txt"
    $body = "cuexis-sdk-api-approval-v1`n" + ($Record | ConvertTo-Json -Compress) + "`n"
    [IO.File]::WriteAllText($path, $body, [Text.UTF8Encoding]::new($false))
    return $path
}

function Test-ApprovalComment($Comment, $Record, $Owners) {
    if (-not $Comment -or $Comment.user.type -cne 'User' -or
        $Owners -cnotcontains $Comment.user.login -or
        ($Comment.id -isnot [int] -and $Comment.id -isnot [long]) -or $Comment.id -le 0 -or
        $Comment.created_at -cne $Comment.updated_at -or
        -not ([string]$Comment.created_at).StartsWith($Record.utc_date + 'T')) { return $false }
    if ($Comment.body -isnot [string] -or $Comment.body.Length -gt 8192) { return $false }
    $body = $Comment.body.Replace("`r`n", "`n")
    $prefix = "cuexis-sdk-api-approval-v1`n"
    if (-not $body.StartsWith($prefix)) { return $false }
    try { $value = $body.Substring($prefix.Length) | ConvertFrom-Json }
    catch { return $false }
    if (@($value.PSObject.Properties).Count -ne $Record.Count) { return $false }
    foreach ($key in $Record.Keys) {
        if (@($value.PSObject.Properties.Name) -cnotcontains $key) { return $false }
        if ($key -eq 'pr') {
            if ($value.$key -isnot [int] -or $value.$key -ne $Record[$key]) { return $false }
        } elseif ($value.$key -isnot [string] -or $value.$key -cne $Record[$key]) { return $false }
    }
    return $true
}

function Read-LatestApproval($Record, $Owners) {
    $latest = $null
    for ($page = 1; $page -le 20; $page++) {
        $comments = @(Read-GitHubJson "repos/$($Record.repository)/issues/$($Record.pr)/comments?per_page=100&page=$page")
        foreach ($comment in $comments) {
            if ($Owners -ccontains $comment.user.login -and
                ([string]$comment.body).Replace("`r`n", "`n").StartsWith("cuexis-sdk-api-approval-v1`n")) {
                $latest = $comment
            }
        }
        if ($comments.Count -lt 100) { return $latest }
    }
    throw 'Approval history exceeds the Version Gate bound.'
}

function Assert-CurrentApproval($Record) {
    $pull = Read-GitHubJson "repos/$($Record.repository)/pulls/$($Record.pr)"
    if ($pull.state -ne 'open' -or $pull.base.ref -ne 'master' -or
        $pull.base.repo.full_name -cne $Record.repository -or
        $pull.base.sha -cne $Record.base_sha -or $pull.head.sha -cne $Record.candidate_sha -or
        [datetime]::UtcNow.ToString('yyyy-MM-dd') -cne $Record.utc_date) {
        throw 'PR or UTC date changed. Run the assistant again for the new candidate.'
    }
}

function Publish-Approval($Record, [string]$DraftPath) {
    $owners = (Read-RemoteText $Record.repository '.github/sdk-api-owners.json' $Record.base_sha | ConvertFrom-Json).owners
    $actor = Read-GitHubJson 'user'
    if ($actor.type -cne 'User' -or $owners -cnotcontains $actor.login) {
        throw 'Log gh in as a human owner listed at the trusted base.'
    }
    $latest = Read-LatestApproval $Record $owners
    Assert-CurrentApproval $Record
    if (Test-ApprovalComment $latest $Record $owners) {
        Write-Host 'The latest owner approval already matches. No duplicate comment posted.'
        return $latest
    }
    $payloadPath = $DraftPath + '.request.json'
    $payload = @{ body = [IO.File]::ReadAllText($DraftPath) } | ConvertTo-Json -Compress
    [IO.File]::WriteAllText($payloadPath, $payload, [Text.UTF8Encoding]::new($false))
    Write-Host "Publishing approval as $($actor.login) ..."
    try {
        $comment = Invoke-GhRequest 'POST' "repos/$($Record.repository)/issues/$($Record.pr)/comments" $payloadPath
    } catch {
        $failure = $_.Exception.Message
        # A lost response can hide a successful POST. Reconcile through GET;
        # never retry the POST automatically.
        $comment = Read-LatestApproval $Record $owners
        if (-not (Test-ApprovalComment $comment $Record $owners)) {
            throw "Comment submission was not confirmed; no automatic repost attempted. Check the PR before retrying.`n$failure"
        }
    }
    if (-not (Test-ApprovalComment $comment $Record $owners)) {
        throw 'The returned comment is not a valid approval. Check the PR before retrying.'
    }
    $observed = Read-GitHubJson "repos/$($Record.repository)/issues/comments/$($comment.id)"
    if (-not (Test-ApprovalComment $observed $Record $owners)) {
        throw 'Posted comment failed API read-back validation. Check the PR.'
    }
    Write-Host "Approval published and verified: $($observed.html_url)"
    Assert-CurrentApproval $Record
    return $observed
}

function Start-ApprovalAssistant {
    if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
        throw 'GitHub CLI (gh) is required. Install it and run gh auth login once.'
    }
    Write-Host "Reading current PR #$PullRequest from $Repository ..."
    $record = Read-ApprovalRecord $Repository $PullRequest
    $root = Split-Path -Parent $PSScriptRoot
    $path = Save-ApprovalDraft $record (Join-Path $root 'out/sdk-approval')
    Write-Host "Draft: $path"
    Write-Host "Candidate: $($record.candidate_sha)"
    Write-Host "SDK: $($record.from) -> $($record.to); UTC date: $($record.utc_date)"
    if ($PrepareOnly) {
        Write-Host 'PrepareOnly: no comment has been posted.'
        return
    }
    $comment = Publish-Approval $record $path
    Write-Host "Comment: $($comment.html_url)"
    Write-Host 'Done. No copy/paste needed. Rerun the failed Version Gate for this PR.'
    Write-Host 'If the PR changes or the UTC date rolls over, run this assistant again.'
}

if ($MyInvocation.InvocationName -ne '.') {
    $previousProxy = $env:HTTPS_PROXY
    $previousNoProxy = $env:NO_PROXY
    $lock = $null
    $locked = $false
    $transcribing = $false
    $exitCode = 1
    try {
        $logDirectory = Join-Path (Split-Path -Parent $PSScriptRoot) 'out/sdk-approval'
        [void][IO.Directory]::CreateDirectory($logDirectory)
        $logPath = Join-Path $logDirectory ('run-' + [datetime]::Now.ToString('yyyyMMdd-HHmmss') + "-$PID.log")
        Start-Transcript -LiteralPath $logPath -Force | Out-Null
        $transcribing = $true
        Write-Host "Run log: $logPath"
        $lock = [Threading.Mutex]::new($false, ('Local\CuexisSdkApproval-' + ($Repository -replace '[^A-Za-z0-9]', '_') + '-' + $PullRequest))
        try { $locked = $lock.WaitOne(0) }
        catch [Threading.AbandonedMutexException] { $locked = $true }
        if (-not $locked) { throw 'Another approval assistant is already running for this PR.' }
        Enable-ApprovalProxy
        Start-ApprovalAssistant
        $exitCode = 0
    }
    catch {
        Write-Host "ERROR: $($_.Exception.Message)" -ForegroundColor Red
        Write-Host $_.InvocationInfo.PositionMessage
        Write-Host $_.ScriptStackTrace
        if ($transcribing) { Write-Host "Full error saved in: $logPath" }
    }
    finally {
        $env:HTTPS_PROXY = $previousProxy
        $env:NO_PROXY = $previousNoProxy
        if ($locked) { $lock.ReleaseMutex() }
        if ($lock) { $lock.Dispose() }
        if ($transcribing) { Stop-Transcript | Out-Null }
    }
    exit $exitCode
}
