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

# Both winning sides are exercised from one uninterrupted native run.  The
# final proof asserts state-$17 receiver-directed flight and fails if the
# freeze, render contract, or deterministic output regresses.
& (Join-Path $PSScriptRoot 'New-TipoffRegressionFinalProof.ps1') `
    -ProjectRoot $ProjectRoot -AssetPackPath $AssetPackPath -OutputRoot $OutputRoot
if ($LASTEXITCODE -ne 0) { throw 'Continuous tip-ball trajectory proof failed.' }

Write-Output 'TIP-BALL TRAJECTORY PROOF PASS: continuous native lifecycle evidence'
