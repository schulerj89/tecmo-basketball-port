[CmdletBinding()]
param(
    [switch]$WriteDashboard,
    [switch]$CheckDashboard,
    [switch]$IncludeGit
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$python = (Get-Command python -ErrorAction Stop).Source
$script = Join-Path $PSScriptRoot 'orchestration.py'

if ($WriteDashboard -or $CheckDashboard) {
    $arguments = @($script, '--repo-root', $repoRoot, 'dashboard')
    if ($IncludeGit) {
        $arguments += '--git'
    }
    if ($CheckDashboard) {
        $arguments += '--check'
    }
} else {
    $arguments = @($script, '--repo-root', $repoRoot, 'status')
}

& $python @arguments
if ($LASTEXITCODE -ne 0) {
    throw "Control-plane status/dashboard command failed with exit code $LASTEXITCODE."
}
