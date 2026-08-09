param(
    [string]$ProjectRoot,
    [string]$AssetPackPath,
    [string]$OutputRoot
)

$ErrorActionPreference = 'Stop'
if (!$ProjectRoot) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }
if (!$OutputRoot) {
    $OutputRoot = Join-Path $ProjectRoot 'build\proof\tipoff-regression-final'
}

# The renderer and actor ownership are meaningful only across a continuous
# cinematic-to-LIVE handoff.  This proof includes that seam, recovery, and
# post-landing live frames for human, CPU-only, and no-input routes.
& (Join-Path $PSScriptRoot 'New-TipoffRegressionFinalProof.ps1') `
    -ProjectRoot $ProjectRoot -AssetPackPath $AssetPackPath -OutputRoot $OutputRoot
if ($LASTEXITCODE -ne 0) { throw 'Continuous tip-off live continuity proof failed.' }

Write-Output 'TIP-OFF LIVE CONTINUITY PROOF PASS: continuous native lifecycle evidence'
