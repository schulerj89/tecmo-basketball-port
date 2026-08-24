param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [string]$OutputDirectory,
    [switch]$Build
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
    throw "CPU catch proof requires the exact Tecmo NBA Basketball Rev1 ROM."
}

$BuildRoot = [IO.Path]::GetFullPath((Join-Path $ProjectRoot "build"))
if (!$OutputDirectory) {
    $OutputDirectory = Join-Path $BuildRoot "cpu-catch-state0-proof"
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
            throw "Warning-clean CPU catch proof build failed."
        }
    } finally {
        $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    }
}

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$PackPath = Join-Path $OutputDirectory "cpu-catch-state0.assetpack"
& $Executable --build-assetpack $RomPath $PackPath
if ($LASTEXITCODE -ne 0 -or
    !(Test-Path -LiteralPath $PackPath -PathType Leaf)) {
    throw "Could not build the ephemeral full asset pack for the CPU catch proof."
}

$Records = @()
foreach ($Repeat in 1, 2) {
    $PngPath = Join-Path $OutputDirectory ("cpu-catch-state0-{0}.png" -f $Repeat)
    $StatePath = Join-Path $OutputDirectory ("cpu-catch-state0-{0}.json" -f $Repeat)
    $Output = @(& $Executable --root $ProjectRoot `
        --gameplay-live-foundation-proof $PackPath "cpu-catch-state0" `
        $PngPath 2>&1)
    $Text = ($Output -join [Environment]::NewLine).Trim()
    if ($LASTEXITCODE -ne 0 -or
        !(Test-Path -LiteralPath $PngPath -PathType Leaf)) {
        throw "CPU catch production proof run $Repeat failed.`n$Text"
    }
    try { $State = $Text | ConvertFrom-Json } catch {
        throw "CPU catch production proof run $Repeat emitted invalid JSON."
    }
    $Proof = $State.cpu_catch_state0
    $CatchStream = [Convert]::ToInt32([string]$Proof.catch.stream, 16)
    $AfterFetch = [Convert]::ToInt32(
        [string]$Proof.progression.stream_after_fetch, 16)
    $LastStep = [Convert]::ToInt32(
        [string]$Proof.progression.last_step_after_fetch, 16)
    $WaitBefore = [Convert]::ToInt32(
        [string]$Proof.wait_state6.stream[0], 16)
    $WaitAfter = [Convert]::ToInt32(
        [string]$Proof.wait_state6.stream[1], 16)
    if ($State.schema -ne "tecmo.live-proof/TGLP-1" -or
        $State.event -ne "cpu-catch-state0" -or
        ![bool]$Proof.proved -or
        ![bool]$Proof.automatic_pass -or
        ![bool]$Proof.automatic_inbound -or
        ![bool]$Proof.human_state0_endpoint -or
        ![bool]$Proof.selected_wait_state6 -or
        [bool]$Proof.state0_intermediate_runtime_observable -or
        [string]$Proof.route_choice -ne
            'chosen source-valid $00D7 long-route approximation; raw $0373/$0095/$0094 unavailable' -or
        [int]$Proof.catch.source_state0 -ne 0 -or
        [int]$Proof.catch.automatic_state -ne 4 -or
        [int]$Proof.catch.human_state -ne 0 -or
        [int]$Proof.catch.automatic_action_046e -ne 24 -or
        [int]$Proof.catch.human_action_046e -ne 0 -or
        $CatchStream -ne 0x00D7 -or $AfterFetch -ne 0x00DC -or
        $LastStep -ne 0x00DC -or
        [int]$Proof.progression.decision_serial[1] -ne
            ([int]$Proof.progression.decision_serial[0] + 1) -or
        [int]$Proof.progression.decision_serial[2] -ne
            ([int]$Proof.progression.decision_serial[1] + 1) -or
        [int]$Proof.progression.position[0][0] -ne
            [int]$Proof.progression.position[1][0] -or
        [int]$Proof.progression.position[0][1] -ne
            [int]$Proof.progression.position[1][1] -or
        ([int]$Proof.progression.position[1][0] -eq
             [int]$Proof.progression.position[2][0] -and
         [int]$Proof.progression.position[1][1] -eq
             [int]$Proof.progression.position[2][1]) -or
        ![bool]$Proof.progression.opcode21.exact_typed_time_inputs -or
        ![bool]$Proof.progression.opcode21.raw_007e_bit1_exact -or
        ![bool]$Proof.progression.opcode21.whole_gate_exact -or
        (@($Proof.progression.opcode21.plus5_input) -join ',') -ne '3,1,30' -or
        (@($Proof.progression.opcode21.plus10_input) -join ',') -ne '4,0,4' -or
        [Convert]::ToInt32(
            [string]$Proof.progression.opcode21.plus5_stream, 16) -ne 0x00E1 -or
        [Convert]::ToInt32(
            [string]$Proof.progression.opcode21.plus10_stream, 16) -ne 0x00E6 -or
        ![bool]$Proof.controller_assignment.automatic_handoff_unchanged -or
        (Compare-Object $Proof.controller_assignment.controlled_before `
            $Proof.controller_assignment.controlled_after) -or
        (Compare-Object $Proof.controller_assignment.team_before `
            $Proof.controller_assignment.team_after) -or
        (@($Proof.wait_state6.sequence) -join ',') -ne '7,6,5,4,3,2,1,0' -or
        $WaitBefore -ne 0x0082 -or $WaitAfter -ne 0x0087 -or
        ![bool]$Proof.action17.close_shot -or
        ![bool]$Proof.action17.far_recovery -or
        ![bool]$Proof.action17.nonmatch_unaffected -or
        [string]$Proof.action17.close_admission -ne
            'native adapter; $8ACE raw $0478/$0499/$007E gates unavailable' -or
        [int]$Proof.action17.close.action_serial[1] -ne
            ([int]$Proof.action17.close.action_serial[0] + 1) -or
        [int]$Proof.action17.close.shot_actor -ne
            [int]$Proof.receivers.pass -or
        [int]$Proof.action17.close.ball_holder -ne 255 -or
        [int]$Proof.action17.far.state -ne 4 -or
        [int]$Proof.action17.far.action_046e -ne 0) {
        throw "CPU catch production proof run $Repeat violated its bounded contract."
    }
    Set-Content -LiteralPath $StatePath -Value $Text -Encoding UTF8
    $Records += [pscustomobject]@{
        png = $PngPath
        state = $StatePath
        png_sha256 = (Get-FileHash -LiteralPath $PngPath -Algorithm SHA256).Hash
        state_sha256 = (Get-FileHash -LiteralPath $StatePath -Algorithm SHA256).Hash
    }
}
if ($Records[0].png_sha256 -ne $Records[1].png_sha256 -or
    $Records[0].state_sha256 -ne $Records[1].state_sha256) {
    throw "CPU catch proof was not deterministic across two production runs."
}

$ManifestPath = Join-Path $OutputDirectory "cpu-catch-state0-proof.json"
([pscustomobject]@{
    schema = "tecmo.cpu-catch-state0-proof/TGCH-1"
    status = "PASS"
    assertion_authority = "structured TGLP assertions"
    screenshot_scope = "presentation-only; not ROM-frame parity"
    automatic_route = 'chosen source-valid $00D7 long-route approximation'
    opcode21 = 'typed clocks and fixed $F07E-$F0B9 raw $007E bit1 predicate exact'
    action17 = "source-backed action17 dispatch; native close-admission adapter; exact imported phase assets within the existing playback seam; far recovery approximation"
    deterministic_png_sha256 = $Records[0].png_sha256
    deterministic_state_sha256 = $Records[0].state_sha256
    records = $Records
} | ConvertTo-Json -Depth 5) |
    Set-Content -LiteralPath $ManifestPath -Encoding UTF8

Write-Output ("CPU catch state-0 proof passed: manifest={0} png_sha256={1}" -f
    $ManifestPath, $Records[0].png_sha256)
