param(
    [string]$ProjectRoot,
    [string]$AssetPackPath,
    [string]$OutputRoot
)
$ErrorActionPreference = "Stop"
if (!$ProjectRoot) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }
if (!$OutputRoot) { $OutputRoot = Join-Path $ProjectRoot "build\proof\tipoff-jump-freeze" }
& (Join-Path $PSScriptRoot "New-TipoffAnimationProof.ps1") `
    -ProjectRoot $ProjectRoot -AssetPackPath $AssetPackPath -OutputRoot $OutputRoot
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
