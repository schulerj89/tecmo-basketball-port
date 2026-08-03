[CmdletBinding()]
param(
    [switch]$IncludeGit
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$python = (Get-Command python -ErrorAction Stop).Source
$arguments = @(
    (Join-Path $PSScriptRoot 'orchestration.py'),
    '--repo-root',
    $repoRoot,
    'validate'
)
if ($IncludeGit) {
    $arguments += '--git'
}

& $python @arguments
if ($LASTEXITCODE -ne 0) {
    throw "Control-plane validation failed with exit code $LASTEXITCODE."
}
