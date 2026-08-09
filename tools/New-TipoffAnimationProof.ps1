param(
    [string]$ProjectRoot,
    [string]$AssetPackPath,
    [string]$OutputRoot
)

$ErrorActionPreference = "Stop"
if (!$ProjectRoot) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }
$ProjectRoot = (Resolve-Path $ProjectRoot).Path
if (!$AssetPackPath) { $AssetPackPath = Join-Path $ProjectRoot "build\tecmo.assetpack" }
$AssetPackPath = (Resolve-Path $AssetPackPath).Path
if (!$OutputRoot) { $OutputRoot = Join-Path $ProjectRoot "build\proof\tipoff-animation" }
$Exe = Join-Path $ProjectRoot "build\tecmo_port.exe"
if (!(Test-Path -LiteralPath $Exe -PathType Leaf)) { throw "Missing executable: $Exe" }
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null
$env:TECMO_ASSETPACK = $AssetPackPath

$Specs = @(
    @{ name="bank04-input-capture"; frame=452; awayPose=517; awayPhase=0; awayState=0x22; awayAltitude=0; homePose=518; homePhase=0; homeState=0x13; homeAltitude=0 },
    @{ name="live-object-seed"; frame=481; awayPose=517; awayPhase=0; awayState=0x22; awayAltitude=0; homePose=518; homePhase=0; homeState=0x13; homeAltitude=0 },
    @{ name="cpu-jump-commit"; frame=483; awayPose=517; awayPhase=0; awayState=0x22; awayAltitude=0; homePose=581; homePhase=2; homeState=0x0B; homeAltitude=$null },
    @{ name="rising-phase4"; frame=490; awayPose=517; awayPhase=0; awayState=0x22; awayAltitude=0; homePose=583; homePhase=4; homeState=0x0C; homeAltitude=$null },
    @{ name="human-jump-commit"; frame=493; awayPose=549; awayPhase=2; awayState=0x0B; awayAltitude=$null; homePose=583; homePhase=4; homeState=0x0C; homeAltitude=$null },
    @{ name="apex-before-cinematic"; frame=499; awayPose=551; awayPhase=4; awayState=0x0C; awayAltitude=$null; homePose=583; homePhase=4; homeState=0x0C; homeAltitude=$null },
    @{ name="first-cinematic-screen1b"; frame=500; awayPose=551; awayPhase=4; awayState=0x0C; awayAltitude=$null; homePose=583; homePhase=4; homeState=0x0C; homeAltitude=$null },
    @{ name="cinematic-middle"; frame=530; awayPose=469; awayPhase=0; awayState=0x13; awayAltitude=0; homePose=501; homePhase=0; homeState=0x13; homeAltitude=0 },
    @{ name="cinematic-end"; frame=559; awayPose=469; awayPhase=0; awayState=0x13; awayAltitude=0; homePose=501; homePhase=0; homeState=0x13; homeAltitude=0 },
    @{ name="return-to-court-landed"; frame=560; awayPose=469; awayPhase=0; awayState=0x13; awayAltitude=0; homePose=501; homePhase=0; homeState=0x13; homeAltitude=0 },
    @{ name="no-late-restart"; frame=589; awayPose=469; awayPhase=0; awayState=0x13; awayAltitude=0; homePose=501; homePhase=0; homeState=0x13; homeAltitude=0 }
)

function Convert-Diagnostic([string]$Line) {
    $result = [ordered]@{}
    foreach ($part in ($Line -split ' ')) {
        if ($part -match '^([^=]+)=(.*)$') { $result[$Matches[1]] = $Matches[2] }
    }
    return $result
}

$Records = @()
foreach ($Spec in $Specs) {
    $Mode = "gameplay-tipoff-proof-frame$($Spec.frame)"
    $Png = Join-Path $OutputRoot ("{0:D3}-{1}.png" -f $Spec.frame, $Spec.name)
    $Text = (& $Exe --root $ProjectRoot --render-test-mode $Mode $Png 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $Png)) {
        throw "Tip-off proof render failed for $Mode.`n$Text"
    }
    $Line = ($Text -split "`r?`n" | Where-Object { $_ -like 'tipoff-proof *' } | Select-Object -First 1)
    if (!$Line) { throw "Missing tipoff-proof diagnostic for $Mode." }
    $Data = Convert-Diagnostic $Line
    $TimingLine = ($Text -split "`r?`n" | Where-Object { $_ -like 'tipoff-timing *' } | Select-Object -First 1)
    if (!$TimingLine) { throw "Missing tipoff-timing diagnostic for $Mode." }
    $Timing = Convert-Diagnostic $TimingLine
    $Expected = @{
        'away-actor'='4'; 'away-selector'='1'; 'away-pose'=[string]$Spec.awayPose;
        'away-phase'=[string]$Spec.awayPhase; 'away-state'=[string]$Spec.awayState;
        'away-pose-encoded'='1'; 'away-renderer-mirror'='0';
        'home-actor'='9'; 'home-selector'='0'; 'home-pose'=[string]$Spec.homePose;
        'home-phase'=[string]$Spec.homePhase; 'home-state'=[string]$Spec.homeState;
        'home-pose-encoded'='1'; 'home-renderer-mirror'='0'
    }
    foreach ($Key in $Expected.Keys) {
        if ($Data[$Key] -ne $Expected[$Key]) {
            throw "$Mode expected $Key=$($Expected[$Key]), got '$($Data[$Key])'."
        }
    }
    if ($null -ne $Spec.awayAltitude -and [int]$Data['away-altitude-q8'] -ne $Spec.awayAltitude) {
        throw "$Mode Away altitude mismatch."
    }
    if ($null -ne $Spec.homeAltitude -and [int]$Data['home-altitude-q8'] -ne $Spec.homeAltitude) {
        throw "$Mode Home altitude mismatch."
    }
    $Records += [pscustomobject]@{
        name=$Spec.name; frame=$Spec.frame; png=$Png; mode=$Mode;
        away=[pscustomobject]@{ actor=4; selector=1; state=[int]$Data['away-state']; phase=[int]$Data['away-phase']; pose=[int]$Data['away-pose']; altitude_q8=[int]$Data['away-altitude-q8']; orientation_encoded=$true; renderer_mirror=$false };
        home=[pscustomobject]@{ actor=9; selector=0; state=[int]$Data['home-state']; phase=[int]$Data['home-phase']; pose=[int]$Data['home-pose']; altitude_q8=[int]$Data['home-altitude-q8']; orientation_encoded=$true; renderer_mirror=$false };
        timing=[pscustomobject]@{ total_frame=[int]$Timing['total-frame']; simulation_tick=[int]$Timing['simulation-tick']; presentation_phase=$Timing['presentation-phase']; cinematic_visible=[int]$Timing['cinematic-visible']; ball_screen_y=[int]$Timing['ball-screen-y']; ball_raw_height=[int]$Timing['ball-raw-height']; rng_threshold=[int]$Timing['rng-threshold']; away_latch=[int]$Timing['away-latch']; away_countdown=[int]$Timing['away-countdown']; away_commit_frame=[int]$Timing['away-commit-frame']; away_committed=[int]$Timing['away-committed']; away_fraction=[int]$Timing['away-fraction']; away_velocity_q8=[int]$Timing['away-velocity-q8']; away_apex_frame=[int]$Timing['away-apex-frame']; away_commit_count=[int]$Timing['away-commit-count']; home_latch=[int]$Timing['home-latch']; home_countdown=[int]$Timing['home-countdown']; home_commit_frame=[int]$Timing['home-commit-frame']; home_committed=[int]$Timing['home-committed']; home_fraction=[int]$Timing['home-fraction']; home_velocity_q8=[int]$Timing['home-velocity-q8']; home_apex_frame=[int]$Timing['home-apex-frame']; home_commit_count=[int]$Timing['home-commit-count']; ball_state=[int]$Timing['ball-state']; contact_state17=[int]$Timing['contact-state17']; event_0588_bit20=[int]$Timing['event-0588-bit20']; first_cinematic_frame=[int]$Timing['first-cinematic-frame'] };
        diagnostic=$Line; timing_diagnostic=$TimingLine
    }
}

$Capture = $Records | Where-Object name -eq 'bank04-input-capture'
$Seed = $Records | Where-Object name -eq 'live-object-seed'
$Apex = $Records | Where-Object name -eq 'apex-before-cinematic'
$FirstCinematic = $Records | Where-Object name -eq 'first-cinematic-screen1b'
$Late = $Records | Where-Object name -eq 'no-late-restart'
if ($Capture.timing.away_latch -ne 1 -or $Capture.timing.away_countdown -ne 12) { throw "Bank04 input was not captured before simulation." }
if ($Seed.timing.simulation_tick -ne 0 -or $Seed.timing.ball_state -ne 0x1A) { throw "Slot/ball live objects were not seeded before the cinematic." }
if ($Apex.timing.home_velocity_q8 -le 0 -or $FirstCinematic.timing.home_velocity_q8 -ge 0 -or $FirstCinematic.timing.home_apex_frame -ge $FirstCinematic.timing.simulation_tick) { throw "Jumper apex did not precede the first cinematic frame." }
if ($FirstCinematic.timing.cinematic_visible -ne 1 -or $FirstCinematic.timing.contact_state17 -ne 1 -or $FirstCinematic.timing.event_0588_bit20 -ne 1) { throw "Cinematic was not triggered from contact/state17/bit20." }
if ($Late.timing.away_commit_count -ne 1 -or $Late.timing.home_commit_count -ne 1 -or $Late.away.pose -ne 469 -or $Late.home.pose -ne 501) { throw "Cinematic exit restarted or reset the tip jump." }

$MetadataPath = Join-Path $OutputRoot "tipoff-animation-metadata.json"
[pscustomobject]@{
    schema="tecmo.tipoff-animation-proof/1"; generated_utc=(Get-Date).ToUniversalTime().ToString('o');
    asset_pack=$AssetPackPath; assertions_passed=$true; records=$Records
} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $MetadataPath -Encoding UTF8

Add-Type -AssemblyName System.Drawing
$CellWidth = 320; $ImageHeight = 240; $LabelHeight = 28; $Columns = 4; $Rows = 3
$Sheet = New-Object System.Drawing.Bitmap ($CellWidth * $Columns), (($ImageHeight + $LabelHeight) * $Rows)
$Graphics = [System.Drawing.Graphics]::FromImage($Sheet)
$Graphics.Clear([System.Drawing.Color]::Black)
$Font = New-Object System.Drawing.Font('Consolas', 11, [System.Drawing.FontStyle]::Bold)
$Brush = [System.Drawing.Brushes]::White
try {
    for ($Index = 0; $Index -lt $Records.Count; ++$Index) {
        $Record = $Records[$Index]; $Column = $Index % $Columns; $Row = [Math]::Floor($Index / $Columns)
        $X = $Column * $CellWidth; $Y = $Row * ($ImageHeight + $LabelHeight)
        $Image = [System.Drawing.Image]::FromFile($Record.png)
        try { $Graphics.DrawImage($Image, $X, $Y, $CellWidth, $ImageHeight) } finally { $Image.Dispose() }
        $Label = "f$($Record.frame) $($Record.name) A$($Record.away.pose)/H$($Record.home.pose)"
        $Graphics.DrawString($Label, $Font, $Brush, $X + 3, $Y + $ImageHeight + 4)
    }
    $ContactSheetPath = Join-Path $OutputRoot "tipoff-animation-contact-sheet.png"
    $Sheet.Save($ContactSheetPath, [System.Drawing.Imaging.ImageFormat]::Png)
} finally {
    $Font.Dispose(); $Graphics.Dispose(); $Sheet.Dispose()
}

Write-Host "TIP-OFF ANIMATION PROOF PASS"
Write-Host "Metadata: $MetadataPath"
Write-Host "Contact sheet: $ContactSheetPath"
