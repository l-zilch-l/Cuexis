$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/prepare_sdk_approval.ps1"

$script:scenario = 'valid'
$script:pullReads = 0
$script:baseSha = 'a' * 40
$script:headSha = 'b' * 40
$script:treeSha = 'c' * 40
$script:today = [datetime]::UtcNow.Date

function Read-GitHubJson([string]$Endpoint) {
    switch -Regex ($Endpoint) {
        '/pulls/32$' {
            $script:pullReads++
            $head = if ($script:scenario -eq 'race' -and $script:pullReads -gt 1) { 'd' * 40 } else { $script:headSha }
            return @{ state = $(if ($script:scenario -eq 'closed') { 'closed' } else { 'open' });
                base = @{ ref = 'master'; sha = $script:baseSha; repo = @{ full_name = 'l-zilch-l/Cuexis' } };
                head = @{ sha = $head } }
        }
        '^user$' {
            return @{ login = $(if ($script:scenario -eq 'wrong-owner') { 'someone-else' } else { 'l-zilch-l' }); type = 'User' }
        }
        '/git/commits/' { return @{ sha = $script:headSha; tree = @{ sha = $script:treeSha } } }
        '/compare/' {
            return @{ merge_base_commit = @{ sha = $(if ($script:scenario -eq 'outdated-base') { 'e' * 40 } else { $script:baseSha }) } }
        }
        default { throw "Unexpected API access: $Endpoint" }
    }
}

function Read-RemoteText([string]$Repo, [string]$Path, [string]$Ref) {
    if ($Path -eq '.github/sdk-api-owners.json') {
        return '{"format":"cuexis.sdk-api-owners","version":1,"repository":"l-zilch-l/Cuexis","owners":["l-zilch-l"]}'
    }
    $isHead = $Ref -eq $script:headSha
    $date = if ($script:scenario -eq 'stale-date' -and $isHead) { $script:today.AddDays(-1) } else { $script:today }
    $build = if ($isHead) { 2 } else { 1 }
    if ($script:scenario -eq 'skipped-build' -and $isHead) { $build = 3 }
    $version = '{0:00}.{1:00}.{2:00}-{3}' -f ($date.Year - 2000), $date.Month, $date.Day, $build
    if ($Path -eq 'vcpkg.json') {
        if ($script:scenario -eq 'manifest-mismatch' -and $isHead) { $version = '00.01.01-1' }
        return '{"version-string":"' + $version + '"}'
    }
    if ($Path -ne 'cmake/CuexisVersion.cmake') { throw "Unexpected file: $Path" }
    $sdk = if ($isHead -and $script:scenario -ne 'unchanged-sdk') { '0.7.1' } else { '0.7.0' }
    return @"
set(CUEXIS_VERSION_YEAR $($date.Year - 2000))
set(CUEXIS_VERSION_MONTH $($date.Month))
set(CUEXIS_VERSION_DAY $($date.Day))
set(CUEXIS_VERSION_BUILD $build)
set(CUEXIS_SDK_API_VERSION "$sdk")
"@
}

$record = Read-ApprovalRecord 'l-zilch-l/Cuexis' 32
if ($record.candidate_sha -cne $script:headSha -or $record.candidate_tree_sha -cne $script:treeSha -or
    $record.from -cne '0.7.0' -or $record.to -cne '0.7.1' -or $record.pr -ne 32 -or
    $record.utc_date -cne $script:today.ToString('yyyy-MM-dd') -or $record.Count -ne 8) {
    throw 'Approval tuple does not match the remote fixture.'
}
$draftRoot = Join-Path $PSScriptRoot '../out/sdk-approval-test path with spaces'
$path = Save-ApprovalDraft $record $draftRoot
$bytes = [IO.File]::ReadAllBytes($path)
if ($bytes -contains 13 -or $bytes[0] -ne 99) { throw 'Draft must be LF-only UTF-8 without BOM.' }
$body = [IO.File]::ReadAllText($path)
if (-not $body.StartsWith("cuexis-sdk-api-approval-v1`n")) { throw 'Wrong approval marker.' }
$decoded = $body.Substring($body.IndexOf("`n") + 1) | ConvertFrom-Json
if ($decoded.candidate_sha -cne $script:headSha -or $decoded.pr -isnot [int]) { throw 'Wrong JSON field types.' }

foreach ($case in @(
    @('race', 'PR or UTC date changed'), @('closed', 'An open PR'),
    @('wrong-owner', 'human owner'), @('outdated-base', 'include its current base'),
    @('stale-date', 'date is stale'), @('skipped-build', 'advance exactly'),
    @('manifest-mismatch', 'version identities differ'), @('unchanged-sdk', 'not needed')
)) {
    $script:scenario = $case[0]
    $script:pullReads = 0
    $caught = $false
    try { $null = Read-ApprovalRecord 'l-zilch-l/Cuexis' 32 }
    catch {
        if (-not $_.Exception.Message.Contains($case[1])) { throw }
        $caught = $true
    }
    if (-not $caught) { throw "Scenario was not rejected: $($case[0])" }
}
$script:scenario = 'valid'
$script:pullReads = 0
$script:originalRead = ${function:Read-GitHubJson}
$script:originalRequest = ${function:Invoke-GhRequest}
$script:comments = @()
$script:posts = 0
$script:postMode = 'normal'

function New-TestComment([string]$Body) {
    return @{ id = 123; body = $Body; user = @{ login = 'l-zilch-l'; type = 'User' };
        created_at = $script:today.ToString('yyyy-MM-dd') + 'T12:00:00Z';
        updated_at = $script:today.ToString('yyyy-MM-dd') + 'T12:00:00Z';
        html_url = 'https://github.com/l-zilch-l/Cuexis/pull/32#issuecomment-123' }
}

function Read-GitHubJson([string]$Endpoint) {
    if ($Endpoint -match '/issues/32/comments\?') { return $script:comments }
    if ($Endpoint -match '/issues/comments/123$') { return $script:comments[-1] }
    return (& $script:originalRead $Endpoint)
}

function Invoke-GhRequest([string]$Method, [string]$Endpoint, [string]$InputFile = '') {
    if ($Method -cne 'POST' -or $Endpoint -cne 'repos/l-zilch-l/Cuexis/issues/32/comments') {
        throw "Unexpected write: $Method $Endpoint"
    }
    $script:posts++
    $payload = [IO.File]::ReadAllText($InputFile) | ConvertFrom-Json
    if ($payload.body -cne [IO.File]::ReadAllText($path)) { throw 'Posted body differs from LF draft.' }
    if ($script:postMode -eq 'not-accepted') { throw 'Simulated network failure before acceptance.' }
    $comment = New-TestComment $payload.body
    $script:comments += $comment
    if ($script:postMode -eq 'lost-response') { throw 'Simulated connection lost after acceptance.' }
    return $comment
}

$wrongCase = New-TestComment ([IO.File]::ReadAllText($path).Replace('"pr":', '"PR":'))
if (Test-ApprovalComment $wrongCase $record @('l-zilch-l')) { throw 'Approval field names must match exact case.' }
$oversized = New-TestComment ([IO.File]::ReadAllText($path) + ("`r`n" * 4100))
if (Test-ApprovalComment $oversized $record @('l-zilch-l')) { throw 'Raw comment size must be bounded before normalization.' }
$wrongId = New-TestComment ([IO.File]::ReadAllText($path))
$wrongId.id = '123'
if (Test-ApprovalComment $wrongId $record @('l-zilch-l')) { throw 'Comment identity must be numeric.' }

$published = Publish-Approval $record $path
if ($script:posts -ne 1 -or $published.id -ne 123) { throw 'Expected exactly one verified POST.' }
$null = Publish-Approval $record $path
if ($script:posts -ne 1) { throw 'Repeated execution duplicated the approval.' }

# CRLF / whitespace variants remain semantically identical approvals.
$script:comments[-1].body = "cuexis-sdk-api-approval-v1`r`n" + ($record | ConvertTo-Json)
$null = Publish-Approval $record $path
if ($script:posts -ne 1) { throw 'Equivalent CRLF approval was reposted.' }

# Only the latest owner record matters: an invalid newer record revokes the old one.
$script:comments += (New-TestComment "cuexis-sdk-api-approval-v1`n{}")
$null = Publish-Approval $record $path
if ($script:posts -ne 2) { throw 'Newer invalid approval was incorrectly skipped.' }

$script:comments = @()
$script:postMode = 'lost-response'
$null = Publish-Approval $record $path
if ($script:posts -ne 3) { throw 'A lost POST response caused duplicate submission.' }

$script:comments = @()
$script:postMode = 'not-accepted'
$caught = $false
try { $null = Publish-Approval $record $path }
catch {
    if (-not $_.Exception.Message.Contains('not confirmed')) { throw }
    $caught = $true
}
if (-not $caught -or $script:posts -ne 4) { throw 'Failed POST must stop without retry.' }

$script:scenario = 'race'
$script:pullReads = 1
$caught = $false
try { $null = Publish-Approval $record $path }
catch {
    if (-not $_.Exception.Message.Contains('PR or UTC date changed')) { throw }
    $caught = $true
}
if (-not $caught -or $script:posts -ne 4) { throw 'Changed HEAD must block POST.' }

# Test the real transport argument/exit handling without accessing GitHub.
$script:ghCalls = 0
function gh {
    $script:ghCalls++
    if (($args -join ' ') -notmatch 'api --hostname github.com --method GET user') {
        throw 'Wrong gh API method/host/endpoint.'
    }
    if ($script:ghCalls -eq 1) {
        $global:LASTEXITCODE = 1
        Write-Error 'Simulated connection timeout'
        return
    }
    $global:LASTEXITCODE = 0
    return '{"login":"fixture-owner"}'
}
$transport = & $script:originalRequest 'GET' 'user'
if ($script:ghCalls -ne 2 -or $transport.login -cne 'fixture-owner') { throw 'GET retry failed.' }

Write-Host 'PASS: tuple/LF/path, 8 rejection cases, direct POST/read-back, duplicate/CRLF handling, revoked approval, lost/failed POST, pre-POST race, GET retry. No live comments posted.'
