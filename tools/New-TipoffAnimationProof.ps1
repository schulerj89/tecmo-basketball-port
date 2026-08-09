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

# The prior proof restarted one synthetic checkpoint per frame.  Delegate to
# the continuous native runtime proof so animation/freeze evidence is taken
# from the same run that crosses state $17 into LIVE.
& (Join-Path $PSScriptRoot 'New-TipoffRegressionFinalProof.ps1') `
    -ProjectRoot $ProjectRoot -AssetPackPath $AssetPackPath -OutputRoot $OutputRoot
if ($LASTEXITCODE -ne 0) { throw 'Continuous tip-off animation proof failed.' }

Write-Output 'TIP-OFF ANIMATION/JUMP-FREEZE PROOF PASS: continuous native lifecycle evidence'
