param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [string]$OutputDirectory,
    [switch]$Build
)

$ErrorActionPreference = "Stop"
$ExpectedRomSha256 =
    "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if (!$RomPath) { $RomPath = $env:TECMO_ROM_PATH }
if (!$RomPath -or !(Test-Path -LiteralPath $RomPath -PathType Leaf)) {
    throw "Pass -RomPath or set TECMO_ROM_PATH to the local Rev1 iNES ROM."
}
$RomPath = (Resolve-Path -LiteralPath $RomPath).Path
if ((Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash -ne
        $ExpectedRomSha256) {
    throw "State-5 proof requires the exact Tecmo NBA Basketball Rev1 ROM."
}

$BuildRoot = [IO.Path]::GetFullPath((Join-Path $ProjectRoot "build"))
if (!$OutputDirectory) {
    $OutputDirectory = Join-Path $BuildRoot "cpu-route-state5-proof"
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$BuildPrefix = $BuildRoot.TrimEnd('\', '/') +
    [IO.Path]::DirectorySeparatorChar
if (!$OutputDirectory.StartsWith($BuildPrefix,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw "Proof output must stay under the ignored build directory."
}

$Executable = Join-Path $BuildRoot "tecmo_port.exe"
if ($Build -or !(Test-Path -LiteralPath $Executable -PathType Leaf)) {
    $PreviousSkipShortcut = $env:TECMO_SKIP_SHORTCUT
    try {
        $env:TECMO_SKIP_SHORTCUT = "1"
        & (Join-Path $ProjectRoot "build.ps1")
        if ($LASTEXITCODE -ne 0) { throw "Build failed." }
    } finally {
        $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    }
}

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$PackPath = Join-Path $OutputDirectory "cpu-route-state5.assetpack"
& $Executable --build-assetpack $RomPath $PackPath
if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $PackPath -PathType Leaf)) {
    throw "Could not build the ephemeral full asset pack for the state-5 proof."
}

$Records = @()
foreach ($Repeat in 1, 2) {
    $PngPath = Join-Path $OutputDirectory ("cpu-route-state5-{0}.png" -f $Repeat)
    $StatePath = Join-Path $OutputDirectory ("cpu-route-state5-{0}.json" -f $Repeat)
    $Output = @(& $Executable --root $ProjectRoot `
        --gameplay-live-foundation-proof $PackPath "cpu-route-state5" `
        $PngPath 2>&1)
    $Text = ($Output -join [Environment]::NewLine).Trim()
    if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $PngPath -PathType Leaf)) {
        throw "Production state-5 proof run $Repeat failed.`n$Text"
    }
    try {
        $State = $Text | ConvertFrom-Json
    } catch {
        throw "Production state-5 proof run $Repeat emitted invalid JSON."
    }
    $Proof = $State.cpu_route_state5
    $StreamBefore = [Convert]::ToInt32([string]$Proof.stream[0], 16)
    $StreamAfter = [Convert]::ToInt32([string]$Proof.stream[1], 16)
    if ($State.schema -ne "tecmo.live-proof/TGLP-1" -or
        $State.event -ne "cpu-route-state5" -or
        ![bool]$Proof.proved -or [int]$Proof.actor -ne 0 -or
        [string]$Proof.record_offset -ne "0000" -or
        $StreamAfter -ne ($StreamBefore + 5) -or
        [int]$Proof.duration -lt 3 -or
        [int]$Proof.timer_mid -ne ([int]$Proof.duration - 2) -or
        [int]$Proof.target_snapshot[0] -eq [int]$Proof.ball_after_launch[0] -and
            [int]$Proof.target_snapshot[1] -eq [int]$Proof.ball_after_launch[1] -or
        ![bool]$Proof.target_frozen -or
        [int]$Proof.actor_launch[0] -eq [int]$Proof.actor_mid[0] -and
            [int]$Proof.actor_launch[1] -eq [int]$Proof.actor_mid[1] -or
        [int]$Proof.horizontal_q6[0] -eq [int]$Proof.horizontal_q6[1] -and
            [int]$Proof.depth_q6[0] -eq [int]$Proof.depth_q6[1] -or
        [int]$Proof.decision_serial[1] -ne
            ([int]$Proof.decision_serial[0] + 1) -or
        ![bool]$Proof.no_tgmo_double_step -or
        ![bool]$Proof.parity.low_bit1_finish -or
        ![bool]$Proof.parity.low_bit0_extra_tick -or
        ![bool]$Proof.parity.high_bit0_finish -or
        ![bool]$Proof.parity.high_bit1_extra_tick -or
        [string]$Proof.scope -ne
            "native LIVE integration; not ROM-frame parity" -or
        [string]$Proof.extra_adjust_admission -ne
            "typed no-controller native approximation; not raw `$030C/`$030D parity") {
        throw "Production state-5 proof run $Repeat did not satisfy its bounded contract."
    }
    Set-Content -LiteralPath $StatePath -Value $Text -Encoding UTF8
    $Records += [pscustomobject]@{
        png = $PngPath
        state = $StatePath
        png_sha256 = (Get-FileHash -LiteralPath $PngPath -Algorithm SHA256).Hash
        state_sha256 = (Get-FileHash -LiteralPath $StatePath -Algorithm SHA256).Hash
        route_target_snapshot = @(
            [int]$Proof.target_snapshot[0], [int]$Proof.target_snapshot[1])
        mid_route_actor = @([int]$Proof.actor_mid[0], [int]$Proof.actor_mid[1])
    }
}
if ($Records[0].png_sha256 -ne $Records[1].png_sha256 -or
    $Records[0].state_sha256 -ne $Records[1].state_sha256) {
    throw "State-5 proof was not deterministic across the two production runs."
}
$ManifestPath = Join-Path $OutputDirectory "cpu-route-state5-proof.json"
([pscustomobject]@{
    schema = "tecmo.cpu-route-state5-proof/TGRP-1"
    status = "PASS"
    scope = "native LIVE integration; not ROM-frame parity"
    extra_adjust_admission =
        "typed no-controller native approximation; not raw `$030C/`$030D parity"
    asset_pack = $PackPath
    canonical_record = @{ offset = "0000"; opcode = 4; c8 = 10 }
    deterministic_png_sha256 = $Records[0].png_sha256
    deterministic_state_sha256 = $Records[0].state_sha256
    records = $Records
} | ConvertTo-Json -Depth 5) |
    Set-Content -LiteralPath $ManifestPath -Encoding UTF8

Write-Output ("CPU route state-5 proof passed: manifest={0} png_sha256={1}" -f
    $ManifestPath, $Records[0].png_sha256)
