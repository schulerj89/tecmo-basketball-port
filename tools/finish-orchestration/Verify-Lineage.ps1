[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$python = (Get-Command python -ErrorAction Stop).Source
& $python (Join-Path $PSScriptRoot 'orchestration.py') --repo-root $repoRoot lineage
if ($LASTEXITCODE -ne 0) {
    throw "Git lineage validation failed with exit code $LASTEXITCODE."
}
