param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [string]$DecompRoot,
    [switch]$Build
)

$ErrorActionPreference = "Stop"

$ExpectedParent = "f1b04193405d1c87f21e80ee51d3790499ea0cf8"
$FoundationBase = "6d8f9c7a99a7ce188f1a523247d3a9b9093860fb"
$ExpectedRomSha256 =
    "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4"
$ExpectedChangedPaths = @(
    "docs/finish-tasks/R4-audio/COMMANDS.md",
    "docs/finish-tasks/R4-audio/EVIDENCE.md",
    "docs/finish-tasks/R4-audio/LINEAGE.md",
    "docs/finish-tasks/R4-audio/README.md",
    "tools/Run-AudioRouteProofTests.ps1"
)
$FrozenFiles = [ordered]@{
    "tools/Run-MusicTests.ps1" =
        "C69CDC7747DCDF3E73C677BFEC0F6B2DB1F905BC222E43B7496CE7EF7117C2A5"
    "tools/Run-FrontendAudioTests.ps1" =
        "9DB10A03EAA592D518938FAD419F84595FF804773F95AA1EFDDDF39B8061E86A"
    "tools/Run-GameplayAudioTests.ps1" =
        "24427E142A22A8A32657880B97F064958057ADC65B4216A50849B3944F16FCE2"
    "src/tecmo_flow_test.c" =
        "39AF8AB830B0D3546A9D168BECE38E6F39DAC09E593C6BF3A9A2DA5E465AB8FA"
    "src/tecmo_game.c" =
        "67F9E8F8C31DBD95BD75E0F197E527A8ADEBBA0A6C8945525D15CA9D8D063AA2"
    "src/tecmo_gameplay_scene.c" =
        "67F4B71A85274D8C2B4668F1141C71B817F667A63BE263A218838705D8AC01F6"
    "src/tecmo_gameplay_scene_shots.c" =
        "5E1E835A852E16E3BF489287A1599D8A0505A305E0AC38572C3DB0F379270673"
    "src/win32_platform.c" =
        "10043B4E2D28F5114B717643216FD748F2DB0AE88EB35D14FB3690A005F8AB45"
    "include/tecmo_gameplay_audio.h" =
        "7301838A8293F86C9AC5CE98108632DFFAFCEEEC20D143F09C4BD019BBE36815"
    "docs/finish-tasks/R4-audio-foundation/EVIDENCE.md" =
        "67F334DC07555AA11C6251B099CE2F81CD512C0C73D150D5194657816620511F"
    "docs/finish-tasks/R4-audio-foundation/PROOF.md" =
        "A1D17179585CD4B8B74D508F4CD607338B9B9635A9BE8EDBF8FB22A3F5B7158C"
    "docs/finish-tasks/R4A-audio-integration-qa/README.md" =
        "FFCF6675EE4391DC82B55E1A0C6DD527C5B8A608A3EE0058408DA5BD661C37B4"
}
$ExpectedSuiteOutput = [ordered]@{
    music = "MUSIC TEST PASS: TMUS-1 provenance parser sequencer synth cadence startup envelope null-sink frozen-fallback malformed missing oversized source-mutations"
    frontend = "FRONTEND AUDIO TEST PASS: TFSX-1 exact-provenance parser stable-PCM title-stop-frame5 SFX10 frame1 track6 frame127 accepted-A-release SFX8 same-pack malformed missing oversized dependency frontend-source-mutations"
    gameplay = "GAMEPLAY AUDIO TEST PASS: TSFX-1 TDMC-1 provenance parser mixer override cadence music-gate mailbox DMC-independence DMC-continuity clear-all malformed missing oversized cross-pack source-mutations"
}
$ExpectedFoundationProof = [ordered]@{
    manifest = "TECMAUDIOPROOF-SCRIPT-2"
    base_sha = $FoundationBase
    canonical_rom_revision = "Rev1"
    canonical_rom_sha256 = $ExpectedRomSha256
    canonical_rom_visibility = "local_private"
    pack_sha256 =
        "8916A549E804AFF083B42989E898A92189A1226C192A644660B19812519C8141"
    tmus_payload_size = "36784"
    tmus_payload_fnv1a32 = "05C00ECB"
    tfsx_payload_size = "1792"
    tfsx_payload_fnv1a32 = "985DC7ED"
    tsfx_payload_size = "2824"
    tsfx_payload_fnv1a32 = "968A5DE6"
    tdmc_payload_size = "2515"
    tdmc_payload_fnv1a32 = "AD70E6E8"
    command = "--audio-proof PACK OUTPUT_DIR"
    source = "explicit-validated-asset-pack"
    vector_count = "23"
    sample_format = "44100Hz_mono_s16le"
    audio_proof_wav_sha256 =
        "57573ABE791F4277AF6DCFC6E7AE22C7A7F319BC64554B0D7FDD8F16AFBC5D6B"
    audio_proof_events_sha256 =
        "3E8FB445B0774F847A529B2BC9670F81862F7C6C04B77AEFE7AB7D7D024674AA"
    audio_proof_manifest_sha256 =
        "47EA2304FFF12C9348E821423E8E0806C9E00FA79DBE8344ED44E3C245B24298"
    waveform_csv_sha256 =
        "76642CA7B52835301EEE0BA6185D50103C6DBC2A411D452A7FBFDBDCCFD5F4E2"
    waveform_svg_sha256 =
        "6A6ED51A4BB1A77A76ACAA50DF1FA30D367AF5A273C9AB20D5C553EBD2A5A66E"
    waveform_run2_csv_sha256 =
        "76642CA7B52835301EEE0BA6185D50103C6DBC2A411D452A7FBFDBDCCFD5F4E2"
    waveform_run2_svg_sha256 =
        "6A6ED51A4BB1A77A76ACAA50DF1FA30D367AF5A273C9AB20D5C553EBD2A5A66E"
}

function Get-ShortTail([object[]]$Lines) {
    return (@($Lines | Select-Object -Last 12) -join [Environment]::NewLine)
}

function Get-GitSingleLine([string[]]$GitArguments, [string]$Label) {
    $Output = @(& git -C $ProjectRoot @GitArguments 2>&1)
    if ($LASTEXITCODE -ne 0 -or $Output.Count -ne 1) {
        throw "$Label failed.`n$(Get-ShortTail $Output)"
    }
    return ([string]$Output[0]).Trim()
}

function Assert-Contains([string]$RelativePath, [string[]]$Needles) {
    $Path = Join-Path $ProjectRoot $RelativePath
    $Text = [IO.File]::ReadAllText($Path)
    foreach ($Needle in $Needles) {
        if ($Text.IndexOf($Needle, [StringComparison]::Ordinal) -lt 0) {
            throw "Frozen route assertion '$Needle' is absent from '$RelativePath'."
        }
    }
}

function Assert-NotContains([string[]]$RelativePaths, [string[]]$Needles) {
    $Text = ($RelativePaths | ForEach-Object {
        [IO.File]::ReadAllText((Join-Path $ProjectRoot $_))
    }) -join "`n"
    foreach ($Needle in $Needles) {
        if ($Text.IndexOf($Needle, [StringComparison]::Ordinal) -ge 0) {
            throw "Unproven-route absence assertion '$Needle' is no longer true."
        }
    }
}

function Invoke-FrozenSuite([string]$Name, [string]$Script,
                            [string[]]$SuiteArguments,
                            [string]$ExpectedLine) {
    $Output = @(& $Script @SuiteArguments 2>&1)
    $ExitCode = $LASTEXITCODE
    $MatchingLines = @($Output | Where-Object {
        ([string]$_).Trim() -eq $ExpectedLine
    })
    if ($ExitCode -ne 0 -or $MatchingLines.Count -ne 1) {
        throw "Frozen $Name suite failed or did not emit its exact terminal line.`n$(Get-ShortTail $Output)"
    }
    Write-Output "$Name suite: PASS"
}

function Read-KeyValueManifest([string]$Path) {
    $Values = [Collections.Generic.Dictionary[string,string]]::new(
        [StringComparer]::Ordinal)
    $Lines = @([IO.File]::ReadAllLines($Path))
    if ($Lines.Count -eq 0) { throw "Foundation proof manifest is empty." }
    foreach ($Line in $Lines) {
        $Parts = $Line -split "=", 2
        if ($Parts.Count -ne 2 -or !$Parts[0] -or
            $Values.ContainsKey($Parts[0])) {
            throw "Foundation proof manifest has a malformed or duplicate field."
        }
        $Values.Add($Parts[0], $Parts[1])
    }
    return ,$Values
}

function New-Route([string]$Id, [string]$Classification,
                   [string]$Request, [string]$Evidence,
                   [string]$Boundary) {
    if ($Classification -notin @("proven", "source-present-only", "unproven")) {
        throw "Route '$Id' has an invalid classification."
    }
    return [pscustomobject][ordered]@{
        route_id = $Id
        classification = $Classification
        request = $Request
        evidence = $Evidence
        boundary = $Boundary
    }
}

if (!$ProjectRoot) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$GitRoot = Get-GitSingleLine @("rev-parse", "--show-toplevel") "Git root"
if (![string]::Equals([IO.Path]::GetFullPath($GitRoot),
                      [IO.Path]::GetFullPath($ProjectRoot),
                      [StringComparison]::OrdinalIgnoreCase)) {
    throw "ProjectRoot is not the exact repository root."
}
if (!$RomPath) { $RomPath = $env:TECMO_ROM_PATH }
if (!$RomPath -or !(Test-Path -LiteralPath $RomPath -PathType Leaf)) {
    throw "Pass -RomPath or set TECMO_ROM_PATH to the private Rev1 ROM."
}
$RomPath = (Resolve-Path -LiteralPath $RomPath).Path
if ((Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash -ne
    $ExpectedRomSha256) {
    throw "Audio route proof requires the canonical private Rev1 ROM."
}
if (!$DecompRoot) { $DecompRoot = $env:TECMO_DECOMP_ROOT }
if (!$DecompRoot -or !(Test-Path -LiteralPath $DecompRoot -PathType Container)) {
    throw "Pass -DecompRoot or set TECMO_DECOMP_ROOT; terminal route proof requires the real --flow-test path."
}
$DecompRoot = (Resolve-Path -LiteralPath $DecompRoot).Path

$Head = Get-GitSingleLine @("rev-parse", "HEAD") "HEAD resolution"
$MergeBase = Get-GitSingleLine @("merge-base", $ExpectedParent, "HEAD") `
    "Expected-parent ancestry"
if ($MergeBase -ne $ExpectedParent) {
    throw "HEAD is not descended from the exact R4-AUDIO expected parent."
}
$Status = @(& git -C $ProjectRoot status --porcelain=v1 --untracked-files=all)
if ($LASTEXITCODE -ne 0 -or $Status.Count -ne 0) {
    throw "Audio route proof requires a completely clean worktree and index."
}
$ChangedPaths = @(& git -C $ProjectRoot diff --name-only `
    "$ExpectedParent..HEAD")
if ($LASTEXITCODE -ne 0 -or
    (@($ChangedPaths | Sort-Object) -join "`n") -ne
        (@($ExpectedChangedPaths | Sort-Object) -join "`n")) {
    throw "R4-AUDIO candidate paths are not exactly the five proof-only paths."
}
$ProofBase = [IO.Path]::GetFullPath((Join-Path $ProjectRoot "build/proof"))
$ProofRoot = [IO.Path]::GetFullPath((Join-Path $ProofBase "r4-audio-route"))
$ProofPrefix = $ProofBase.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
if (!$ProofRoot.StartsWith($ProofPrefix,
                           [StringComparison]::OrdinalIgnoreCase)) {
    throw "Route proof output escaped build/proof."
}
if (Test-Path -LiteralPath $ProofRoot) {
    Remove-Item -LiteralPath $ProofRoot -Recurse -Force
}

foreach ($Entry in $FrozenFiles.GetEnumerator()) {
    $Path = Join-Path $ProjectRoot $Entry.Key
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Frozen evidence file '$($Entry.Key)' is missing."
    }
    $Actual = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
    if ($Actual -ne $Entry.Value) {
        throw "Frozen evidence file '$($Entry.Key)' drifted: $Actual."
    }
}

Assert-Contains "src/tecmo_flow_test.c" @(
    "normal opening did not queue track 7 at arena entry",
    "title setup frame 5 did not hard-stop opening music",
    "second START did not queue frontend SFX 10 exactly once",
    "blue-menu entry did not queue presentation track 6",
    "accepted root A-release did not queue menu SFX 8 exactly once"
)
Assert-Contains "src/tecmo_game.c" @(
    "tecmo_music_queue_opening_once(&runtime->music_player)",
    "tecmo_frontend_audio_queue_title_confirm(",
    "tecmo_frontend_audio_queue_menu_accept(",
    "TECMO_MUSIC_TRACK_PRESENTATION"
)
Assert-Contains "src/tecmo_gameplay_scene.c" @(
    "tecmo_gameplay_audio_queue_pregame_matchup_stinger(",
    "tecmo_gameplay_audio_queue_game_music(",
    "TECMO_GAMEPLAY_EVENT_MUSIC_REQUEST",
    "TECMO_GAMEPLAY_AUDIO_CLOCK_BUZZER",
    "TECMO_GAMEPLAY_AUDIO_COUNTDOWN",
    "TECMO_GAMEPLAY_AUDIO_BANK05_9FEC_CUE",
    "TECMO_GAMEPLAY_AUDIO_HELD_BALL_DRIBBLE",
    "presentation.presentation_sfx_id"
)
Assert-Contains "src/tecmo_gameplay_scene_shots.c" @(
    "TECMO_GAMEPLAY_AUDIO_CROWD_RESPONSE",
    "TECMO_GAMEPLAY_AUDIO_SIDE_RESULT_12",
    "TECMO_GAMEPLAY_AUDIO_SIDE_RESULT_13",
    "TECMO_GAMEPLAY_DMC_BANK05_A8D6_SHORT",
    "TECMO_GAMEPLAY_DMC_BANK05_A9C5"
)
Assert-Contains "include/tecmo_gameplay_audio.h" @(
    "TECMO_GAMEPLAY_DMC_BANK05_A8D6_SHORT = 0",
    "TECMO_GAMEPLAY_DMC_BANK05_A8D6_LONG = 1",
    "TECMO_GAMEPLAY_DMC_BANK05_A9C5 = 2",
    "TECMO_GAMEPLAY_DMC_LAYUP_SEQUENCE_ABF5 = 3",
    "TECMO_GAMEPLAY_DMC_HELD_BALL_DRIBBLE = 4"
)
Assert-Contains "src/win32_platform.c" @(
    "tecmo_audio_output_init(&audio_output, &runtime->music_player)",
    "tecmo_audio_output_select_frontend_player(",
    "tecmo_audio_output_select_gameplay_player(",
    "tecmo_audio_output_service(&audio_output)"
)
Assert-Contains "docs/finish-tasks/R4-audio-foundation/EVIDENCE.md" @(
    "DMC IDs 0/1/2 | unresolved/address-bound; not overclaimed",
    "Effect 5 | neutral/unresolved",
    "Effect 6 | bounded correlation only",
    "Nonlinear/cycle-exact NES APU mixing and DMC reader bit/IRQ phase"
)
Assert-NotContains @(
    "src/tecmo_gameplay_scene.c",
    "src/tecmo_gameplay_scene_shots.c"
) @(
    "TECMO_GAMEPLAY_DMC_BANK05_A8D6_LONG",
    "TECMO_GAMEPLAY_DMC_LAYUP_SEQUENCE_ABF5"
)

$PreviousSkipShortcut = $env:TECMO_SKIP_SHORTCUT
try {
    $env:TECMO_SKIP_SHORTCUT = "1"
    $MusicArguments = @("-ProjectRoot", $ProjectRoot, "-RomPath", $RomPath)
    if ($Build) { $MusicArguments += "-Build" }
    Invoke-FrozenSuite "Music" `
        (Join-Path $ProjectRoot "tools/Run-MusicTests.ps1") `
        $MusicArguments $ExpectedSuiteOutput.music
    Invoke-FrozenSuite "FrontendAudio" `
        (Join-Path $ProjectRoot "tools/Run-FrontendAudioTests.ps1") `
        @("-ProjectRoot", $ProjectRoot, "-RomPath", $RomPath,
          "-DecompRoot", $DecompRoot) $ExpectedSuiteOutput.frontend
    Invoke-FrozenSuite "GameplayAudio" `
        (Join-Path $ProjectRoot "tools/Run-GameplayAudioTests.ps1") `
        @("-ProjectRoot", $ProjectRoot, "-RomPath", $RomPath) `
        $ExpectedSuiteOutput.gameplay
} finally {
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
}

$FoundationProofPath = Join-Path $ProjectRoot `
    "build/proof/r4-audio-foundation/proof-manifest.txt"
if (!(Test-Path -LiteralPath $FoundationProofPath -PathType Leaf)) {
    throw "GameplayAudio did not regenerate the foundation proof manifest."
}
$FoundationProof = Read-KeyValueManifest $FoundationProofPath
foreach ($Entry in $ExpectedFoundationProof.GetEnumerator()) {
    if (!$FoundationProof.ContainsKey($Entry.Key) -or
        ![string]::Equals($FoundationProof[$Entry.Key],
                         [string]$Entry.Value,
                         [StringComparison]::Ordinal)) {
        throw "Foundation proof field '$($Entry.Key)' drifted or is missing."
    }
}
if (!$FoundationProof.ContainsKey("proof_generation_head") -or
    ![string]::Equals($FoundationProof["proof_generation_head"], $Head,
                     [StringComparison]::Ordinal)) {
    throw "Foundation proof was not generated from the current candidate HEAD."
}

$Routes = @(
    New-Route "opening-track-7-license-to-arena" "proven" "TMUS ID 7" `
        "Frozen FrontendAudio suite runs --flow-test and observes pending track 7 at the normal license-to-arena handoff; Music proves the track program." `
        "Native semantic timing, not cycle/APU parity."
    New-Route "title-opening-stop-frame-5" "proven" "stop all active music" `
        "Frozen --flow-test observes unchanged opening state through frame 4 and a hard stop at title frame 5." `
        "Does not claim waveOut queue flush; the device ring is intentionally not flushed."
    New-Route "title-confirm-frame-1" "proven" "TFSX SFX 10" `
        "Frozen --flow-test observes exactly one SFX 10 request on the fresh second START confirmation frame." `
        "Frontend route only."
    New-Route "title-frame-127-blue-menu" "proven" "TMUS ID 6" `
        "Frozen --flow-test holds confirmation through frame 126 and observes pending presentation track 6 at frame-127 handoff." `
        "Generic menu returns do not imply a restart."
    New-Route "menu-accepted-player1-a-release" "proven" "TFSX SFX 8" `
        "Frozen --flow-test observes exactly-once accepted A-release requests and negative START/direction/B/held/chord cases." `
        "Proven for the existing blue-menu state machine."
    New-Route "pregame-matchup-entry" "source-present-only" "TMUS ID 8" `
        "The native scene caller and validated queue API exist; the three allowed terminal suites do not drive the gameplay scene entry." `
        "No production-route execution claim."
    New-Route "gameplay-live-handoff" "source-present-only" "TMUS ID 5 when GAME MUSIC is enabled" `
        "The native scene caller exists and GameplayAudio proves API gating; the allowed gates do not execute this scene handoff." `
        "GAME MUSIC gates future ID-5 queues only."
    New-Route "gameplay-qualifying-restarts" "source-present-only" "TMUS ID 5" `
        "Violation/foul/period/free-throw restart callers exist in the native scene; they are not production-executed by this proof runner." `
        "Free-throw entry and live-return conditions remain distinct."
    New-Route "halftime-final-presentation" "source-present-only" "TMUS ID 6" `
        "The gameplay MUSIC_REQUEST-to-presentation caller exists; the allowed gates do not complete a game to execute it." `
        "No halftime/final end-to-end capture claim."
    New-Route "clock-and-period-expiry" "source-present-only" "TSFX SFX 3" `
        "The native event caller and proven event-to-ID mapping exist; this runner does not drive the production gameplay event." `
        "Source presence is not caller-order parity."
    New-Route "late-clock-countdown" "source-present-only" "TSFX SFX 14" `
        "The native event caller and proven event-to-ID mapping exist; this runner does not drive the production countdown." `
        "No full-game cadence capture."
    New-Route "violation-foul-presentation" "source-present-only" "TSFX SFX 6" `
        "The penalty-presentation numeric caller and validated SFX 6 asset exist." `
        "Meaning is bounded/incomplete/unknown; complete caller-order parity is not claimed."
    New-Route "score-crowd-and-side-result" "source-present-only" "TSFX SFX 11 then clock-gated 12/13 mailbox result" `
        "The transactional shot-result callers and last-write-wins engine behavior exist; this runner does not execute those production callers." `
        "No universal scoring-route claim."
    New-Route "restart-bank05-9fec" "source-present-only" "TSFX SFX 5" `
        "The qualifying restart caller exists and GameplayAudio proves numeric ID 5 queueing." `
        "Effect 5 remains neutral; no whistle/foul/collision/shot/rim/dunk name."
    New-Route "held-ball-dribble" "source-present-only" "TDMC ID 4" `
        "The TGBD-triggered caller and proven event-to-DMC4 mapping exist; this runner does not execute the live scene caller." `
        "Complete 6502 caller scheduling is not claimed."
    New-Route "state15-a8d6-short-repeat" "source-present-only" "TDMC ID 0" `
        "A bounded diagnostic repeat caller exists in scene shot code." `
        "DMC ID 0 remains address-bound; semantic meaning and exclusivity are unknown."
    New-Route "dunk-frame87-a9c5" "source-present-only" "TDMC ID 2" `
        "The current native dunk caller queues address-bound A9C5 at its bounded frame-87 seam." `
        "DMC ID 2 remains address-bound; no impact/rim/dunk semantic meaning is inferred."
    New-Route "win32-output-selection-service" "source-present-only" "music plus frontend/gameplay output selection" `
        "The Win32 init/select/service/shutdown source exists and portable output transactions pass." `
        "No device capture or end-to-end hardware playback proof is produced here."
    New-Route "a8d6-long-production-queue" "unproven" "TDMC ID 1" `
        "No production queue appears in the frozen gameplay scene sources." `
        "DMC ID 1 remains address-bound and unresolved."
    New-Route "abf5-live-layup-production-queue" "unproven" "TDMC ID 3" `
        "The clip is imported with bounded sequence correlation, but no production queue appears in the frozen gameplay scene sources." `
        "No impact/rim meaning or live exclusivity claim."
    New-Route "dmc-ids-0-through-2-semantics" "unproven" "semantic names for TDMC IDs 0, 1, and 2" `
        "The foundation explicitly preserves these IDs as address-bound and unresolved." `
        "Numeric identity and PCM are proven; meaning is not."
    New-Route "effect-5-semantics" "unproven" "semantic name for TSFX SFX 5" `
        "The foundation proves numeric effect 5 and its bounded caller, not a semantic label." `
        "Must remain neutral BANK05_9FEC_CUE."
    New-Route "effect-6-complete-meaning" "unproven" "exclusive semantic meaning and complete route for TSFX SFX 6" `
        "Only numeric/bounded presentation correlation and source presence exist." `
        "Incomplete/unknown; no full original caller-order parity."
    New-Route "cycle-apu-device-parity" "unproven" "cycle-exact/nonlinear APU, DMC reader phase/IRQ, and device-output parity" `
        "The accepted proof is deterministic native PCM plus source/state evidence." `
        "No cycle/APU parity or captured device output is claimed."
    New-Route "full-product-audio-route" "unproven" "end-to-end opening/menu/gameplay/halftime/final production capture" `
        "This proof-only scope does not create or alter production routing and does not drive a complete game." `
        "A later production rescope requires concrete source/capture mapping and separate signed authority."
)

$Counts = [ordered]@{
    proven = @($Routes | Where-Object classification -eq "proven").Count
    source_present_only =
        @($Routes | Where-Object classification -eq "source-present-only").Count
    unproven = @($Routes | Where-Object classification -eq "unproven").Count
}
[void](New-Item -ItemType Directory -Path $ProofRoot)
$LedgerPath = Join-Path $ProofRoot "route-ledger.json"
$Ledger = [pscustomobject][ordered]@{
    schema = "tecmo.audio-route-ledger/1"
    proof_generation_head = $Head
    expected_parent = $ExpectedParent
    policy = "proven requires production-route execution by the allowed frozen terminal commands; source-present-only requires a frozen native caller without route execution; unproven covers absent production queues or missing semantic/cycle/device evidence"
    frozen_suite_sha256 = [pscustomobject][ordered]@{
        music = $FrozenFiles["tools/Run-MusicTests.ps1"]
        frontend = $FrozenFiles["tools/Run-FrontendAudioTests.ps1"]
        gameplay = $FrozenFiles["tools/Run-GameplayAudioTests.ps1"]
    }
    foundation_proof_manifest_sha256 =
        (Get-FileHash -LiteralPath $FoundationProofPath -Algorithm SHA256).Hash
    foundation = [pscustomobject]$ExpectedFoundationProof
    counts = [pscustomobject]$Counts
    routes = $Routes
}
$Utf8NoBom = New-Object Text.UTF8Encoding($false)
$LedgerJson = ($Ledger | ConvertTo-Json -Depth 8)
$LedgerJson = $LedgerJson.Replace("`r`n", "`n").Replace("`r", "`n")
[IO.File]::WriteAllText(
    $LedgerPath, ($LedgerJson + "`n"), $Utf8NoBom)
$LedgerSha = (Get-FileHash -LiteralPath $LedgerPath -Algorithm SHA256).Hash
$ManifestPath = Join-Path $ProofRoot "proof-manifest.txt"
$ManifestLines = @(
    "manifest=TECMOAUDIOROUTEPROOF-1",
    "proof_generation_head=$Head",
    "expected_parent=$ExpectedParent",
    "route_ledger_sha256=$LedgerSha",
    "foundation_proof_manifest_sha256=$($Ledger.foundation_proof_manifest_sha256)",
    "music_suite_sha256=$($FrozenFiles['tools/Run-MusicTests.ps1'])",
    "frontend_suite_sha256=$($FrozenFiles['tools/Run-FrontendAudioTests.ps1'])",
    "gameplay_suite_sha256=$($FrozenFiles['tools/Run-GameplayAudioTests.ps1'])",
    "proven=$($Counts.proven)",
    "source_present_only=$($Counts.source_present_only)",
    "unproven=$($Counts.unproven)",
    "cycle_apu_parity=not_claimed",
    "dmc_ids_0_1_2=address_bound_unresolved",
    "effect_5=neutral_unresolved",
    "effect_6=bounded_incomplete_unknown"
)
[IO.File]::WriteAllText(
    $ManifestPath, (($ManifestLines -join "`n") + "`n"),
    [Text.Encoding]::ASCII)

$global:LASTEXITCODE = 0
Write-Output ("AUDIO ROUTE PROOF PASS: proven={0} source-present-only={1} unproven={2} ledger={3}" -f
    $Counts.proven, $Counts.source_present_only, $Counts.unproven, $LedgerSha)
