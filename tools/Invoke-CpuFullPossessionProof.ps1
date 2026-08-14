param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [string]$OutputDirectory,
    [switch]$Build,
    [switch]$ExpectBaselineFailure
)

$ErrorActionPreference = "Stop"
$ExpectedRomSha256 =
    "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4"

if (!$ProjectRoot) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if (!$RomPath) { $RomPath = $env:TECMO_ROM_PATH }
if (!$RomPath -or !(Test-Path -LiteralPath $RomPath -PathType Leaf)) {
    throw "Pass -RomPath or set TECMO_ROM_PATH to the local Rev1 iNES ROM."
}
$RomPath = (Resolve-Path -LiteralPath $RomPath).Path
if ((Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash -ne
        $ExpectedRomSha256) {
    throw "CPU full-possession proof requires the exact Rev1 ROM."
}

$BuildRoot = [IO.Path]::GetFullPath((Join-Path $ProjectRoot "build"))
if (!$OutputDirectory) {
    $OutputDirectory = Join-Path $BuildRoot "cpu-full-possession-proof"
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
        $BuildOutput = @(& (Join-Path $ProjectRoot "build.ps1") 2>&1)
        if ($LASTEXITCODE -ne 0 -or
            @($BuildOutput | Where-Object {
                $_ -match 'warning [A-Z]+[0-9]+:'
            }).Count -ne 0) {
            throw "Warning-clean CPU full-possession proof build failed."
        }
    } finally {
        $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    }
}

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$PackPath = Join-Path $OutputDirectory "cpu-full-possession.assetpack"
& $Executable --build-assetpack $RomPath $PackPath | Out-Null
if ($LASTEXITCODE -ne 0 -or
    !(Test-Path -LiteralPath $PackPath -PathType Leaf)) {
    throw "Could not build the ephemeral proof asset pack."
}

$Records = @()
foreach ($Repeat in 1, 2) {
    $RunDirectory = Join-Path $OutputDirectory ("run-{0}" -f $Repeat)
    New-Item -ItemType Directory -Force -Path $RunDirectory | Out-Null
    $TracePath = Join-Path $RunDirectory "frames.ndjson"
    $MidHorizonPath = Join-Path $RunDirectory "mid-horizon.png"
    $TerminalPath = Join-Path $RunDirectory "terminal.png"
    $Output = @(& $Executable --root $ProjectRoot `
        --gameplay-cpu-possession-proof $PackPath $TracePath `
        $MidHorizonPath $TerminalPath 2>&1)
    $ExitCode = $LASTEXITCODE
    $Text = ($Output -join [Environment]::NewLine).Trim()
    $JsonStart = $Text.IndexOf('{')
    if ($JsonStart -lt 0) {
        throw "CPU full-possession run $Repeat emitted no structured summary.`n$Text"
    }
    try { $Summary = $Text.Substring($JsonStart) | ConvertFrom-Json } catch {
        throw "CPU full-possession run $Repeat emitted invalid summary JSON."
    }
    if (!(Test-Path -LiteralPath $TracePath -PathType Leaf) -or
        !(Test-Path -LiteralPath $MidHorizonPath -PathType Leaf) -or
        !(Test-Path -LiteralPath $TerminalPath -PathType Leaf) -or
        $Summary.schema -ne "tecmo.cpu-possession-proof/TGPH-6" -or
        ![bool]$Summary.structured_state_authority -or
        [int]$Summary.outer_update_limit -ne 20000 -or
        [string]$Summary.fixture -ne
            'COM VS COM controllerless setup feeding production inbound; deterministic clocks 24/50' -or
        ![bool]$Summary.first_shot.captured -or
        [int]$Summary.first_shot.kind -ne 1 -or
        [string]$Summary.first_shot.kind_name -ne 'jump' -or
        [int]$Summary.first_shot.outcome -ne 2 -or
        [string]$Summary.first_shot.outcome_name -ne 'miss' -or
        ![bool]$Summary.first_settlement.captured -or
        ![bool]$Summary.first_settlement.scores_unchanged -or
        ([string]::Join(',', $Summary.first_shot.score_before) -ne
            [string]::Join(',', $Summary.first_settlement.score_after)) -or
        ![bool]$Summary.inbound_promotion_0627.adversarial_fixture_valid -or
        ![bool]$Summary.inbound_promotion_0627.inbound_started -or
        ![bool]$Summary.inbound_promotion_0627.stale_suppressed_before_ai -or
        ![bool]$Summary.inbound_promotion_0627.catch_promoted_d7_state4_action18 -or
        ![bool]$Summary.inbound_promotion_0488.adversarial_fixture_valid -or
        ![bool]$Summary.inbound_promotion_0488.inbound_started -or
        ![bool]$Summary.inbound_promotion_0488.stale_suppressed_before_ai -or
        ![bool]$Summary.inbound_promotion_0488.catch_promoted_d7_state4_action18 -or
        ![bool]$Summary.ownership_fixtures.controllerless_automatic -or
        ![bool]$Summary.ownership_fixtures.p1_direct_holder -or
        ![bool]$Summary.ownership_fixtures.p2_direct_holder -or
        ![bool]$Summary.ownership_fixtures.invalid_same_team_other_actor_unowned -or
        ![bool]$Summary.source_progression_059b.opcode3 -or
        ![bool]$Summary.source_progression_059b.state6_wait30_cursor05a0 -or
        [int]$Summary.source_progression_059b.countdown_ticks -ne 30 -or
        ![bool]$Summary.source_progression_059b.state4_cursor05a0 -or
        ![bool]$Summary.source_progression_059b.opcode2_to05a5 -or
        [bool]$Summary.selected_state0b_observed -or
        [bool]$Summary.scene_update_failed -or
        [bool]$Summary.ownership_failure -or
        [bool]$Summary.anchor_oob) {
        throw "CPU full-possession run $Repeat violated its evidence contract."
    }
    if ((Get-Item -LiteralPath $TracePath).Length -gt 64MB -or
        (Get-Item -LiteralPath $MidHorizonPath).Length -gt 8MB -or
        (Get-Item -LiteralPath $TerminalPath).Length -gt 8MB) {
        throw "CPU full-possession run $Repeat exceeded its artifact cap."
    }
    $ExpectedClaimantSerial =
        if ([uint32]$Summary.first_shot.claimant_serial_before -eq
            [uint32]::MaxValue) {
            [uint32]1
        } else {
            [uint32]$Summary.first_shot.claimant_serial_before + 1
        }
    if ($ExpectBaselineFailure) {
        $ExpectedLongHorizonFailure =
            [string]$Summary.outcome -eq 'generic-miss-fallback' -and
            [string]$Summary.first_outcome_classification -eq
                'jump-miss-generic-compatibility-handoff' -and
            [int]$Summary.generic_fallbacks -eq 1 -and
            [int]$Summary.source_backed_outcomes -eq 0 -and
            [bool]$Summary.first_settlement.claimant_unchanged_invalid -and
            ![bool]$Summary.first_settlement.claimant_tied_to_new_holder -and
            ![bool]$Summary.movement_coverage.completed
        if ($ExitCode -eq 0 -or [bool]$Summary.passed -or
            !$ExpectedLongHorizonFailure) {
            throw "Run $Repeat did not reproduce the bounded baseline failure."
        }
    } elseif ($ExitCode -ne 0 -or ![bool]$Summary.passed -or
             [string]$Summary.first_outcome_classification -ne
                'jump-miss-claimant-settlement' -or
             ![bool]$Summary.first_settlement.claimant_valid_after -or
             ![bool]$Summary.first_settlement.claimant_tied_to_new_holder -or
             [uint32]$Summary.first_settlement.claimant_serial_after -ne
                $ExpectedClaimantSerial -or
             [string]$Summary.first_settlement.claimant_relation -ne
                'SAME_TEAM' -or
             [bool]$Summary.first_settlement.possession_changed -or
             ![bool]$Summary.first_settlement.automatic_new_holder -or
             [int]$Summary.first_settlement.cursor -ne 125 -or
             [int]$Summary.first_settlement.last_step_offset -ne 125 -or
             [int]$Summary.first_settlement.state -ne 4 -or
             ![bool]$Summary.first_settlement.source_claimant_tuple -or
             [int]$Summary.possession_changes -lt 1 -or
             [int]$Summary.source_backed_outcomes -lt 2 -or
             [int]$Summary.legitimate_terminal_outcomes -ne
                [int]$Summary.source_backed_outcomes -or
             [int]$Summary.claimant_settlement_outcomes -lt 1 -or
             [int]$Summary.same_team_claimant_outcomes -lt 1 -or
             [int]$Summary.scored_inbound_outcomes -lt 1 -or
             [int]$Summary.generic_fallbacks -ne 0 -or
             [int]$Summary.unknown_possession_edges -ne 0 -or
             [int]$Summary.shot_launches -lt 2 -or
             [string]$Summary.movement_coverage.kind -ne
                'loose-ball-chase' -or
             ![bool]$Summary.movement_coverage.typed_active_observed -or
             ![bool]$Summary.movement_coverage.movement_observed -or
             ![bool]$Summary.movement_coverage.visible_legal_movement -or
             ![bool]$Summary.movement_coverage.exact_claimant_settlement -or
             ![bool]$Summary.movement_coverage.completed -or
             [bool]$Summary.movement_coverage.invalid -or
             [bool]$Summary.state5_movement_lifecycle.invalid -or
             [bool]$Summary.selected_state0b_observed -or
             [bool]$Summary.no_effect_failure -or
             [int]$Summary.max_no_effect_streak -gt 1 -or
             [int]$Summary.violation_code -ne 0 -or
             [string]$Summary.violation_name -ne "NONE") {
        throw "CPU full-possession run $Repeat did not resolve legitimately."
    }
    $SummaryPath = Join-Path $RunDirectory "summary.json"
    $Summary | ConvertTo-Json -Depth 8 |
        Set-Content -LiteralPath $SummaryPath -Encoding UTF8
    $Records += [pscustomobject]@{
        trace_sha256 = (Get-FileHash $TracePath -Algorithm SHA256).Hash
        mid_horizon_png_sha256 =
            (Get-FileHash $MidHorizonPath -Algorithm SHA256).Hash
        terminal_png_sha256 =
            (Get-FileHash $TerminalPath -Algorithm SHA256).Hash
        summary_sha256 = (Get-FileHash $SummaryPath -Algorithm SHA256).Hash
        summary = $Summary
    }
}

foreach ($Field in 'trace_sha256','mid_horizon_png_sha256',
                   'terminal_png_sha256','summary_sha256') {
    if ($Records[0].$Field -ne $Records[1].$Field) {
        throw "CPU full-possession proof was not deterministic: $Field."
    }
}

$ManifestPath = Join-Path $OutputDirectory "manifest.json"
([pscustomobject]@{
    schema = "tecmo.cpu-possession-proof-run/TGPH-6"
    status = if ($ExpectBaselineFailure) {
        "EXPECTED_BASELINE_FAILURE"
    } else { "PASS" }
    assertion_authority = "structured per-frame NDJSON and summary"
    screenshot_scope = "presentation-only"
    records = $Records
} | ConvertTo-Json -Depth 12) |
    Set-Content -LiteralPath $ManifestPath -Encoding UTF8

Write-Output ("CPU full-possession proof complete: status={0} manifest={1}" -f
    $(if ($ExpectBaselineFailure) { "EXPECTED_BASELINE_FAILURE" } else { "PASS" }),
    $ManifestPath)
exit 0
