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

# This continuous trace asserts the Bank04 capture clock, the pre-cinematic
# human trajectory, state-$17 receiver-directed flight, and deterministic
# return/recovery.  Do not use independently launched synthetic frame images
# as timing evidence.
& (Join-Path $PSScriptRoot 'New-TipoffRegressionFinalProof.ps1') `
    -ProjectRoot $ProjectRoot -AssetPackPath $AssetPackPath -OutputRoot $OutputRoot
if ($LASTEXITCODE -ne 0) { throw 'Continuous tip-off trajectory timing proof failed.' }

Write-Output 'TIP-OFF TRAJECTORY TIMING PROOF PASS: continuous native lifecycle evidence'
