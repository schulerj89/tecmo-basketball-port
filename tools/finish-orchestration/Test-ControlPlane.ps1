[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$python = (Get-Command python -ErrorAction Stop).Source
$script = Join-Path $PSScriptRoot 'orchestration.py'

& $python $script --repo-root $repoRoot self-test
if ($LASTEXITCODE -ne 0) {
    throw "Control-plane self-test failed with exit code $LASTEXITCODE."
}

& $python $script --repo-root $repoRoot validate --git
if ($LASTEXITCODE -ne 0) {
    throw "Control-plane validation failed with exit code $LASTEXITCODE."
}

& $python $script --repo-root $repoRoot dashboard --git --check
if ($LASTEXITCODE -ne 0) {
    throw "Control-plane dashboard check failed with exit code $LASTEXITCODE."
}
