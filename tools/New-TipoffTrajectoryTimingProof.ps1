param(
    [string]$ProjectRoot,
    [string]$AssetPackPath,
    [string]$OutputRoot
)

$ErrorActionPreference = 'Stop'
if (!$ProjectRoot) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if (!$AssetPackPath) { $AssetPackPath = Join-Path $ProjectRoot 'build\tecmo.assetpack' }
$AssetPackPath = (Resolve-Path -LiteralPath $AssetPackPath).Path
if (!$OutputRoot) { $OutputRoot = Join-Path $ProjectRoot 'build\proof\tipoff-trajectory-timing' }
$Exe = Join-Path $ProjectRoot 'build\tecmo_port.exe'
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$env:TECMO_ASSETPACK = $AssetPackPath

function Convert-Diagnostic([string]$Line) {
    $Result = [ordered]@{}
    foreach ($Part in ($Line -split ' ')) {
        if ($Part -match '^([^=]+)=(.*)$') { $Result[$Matches[1]] = $Matches[2] }
    }
    return $Result
}

$Specs = @(
    @{ name='b-request'; frame=452 },
    @{ name='committed-early-jump'; frame=497 },
    @{ name='last-pre-cinematic'; frame=497 },
    @{ name='state17-cinematic-gate'; frame=498 },
    @{ name='early-receiver-flight'; frame=502 },
    @{ name='mid-receiver-flight'; frame=530 },
    @{ name='low-contact-attachment'; frame=555 },
    @{ name='first-live-continuation'; frame=598 }
)

$Records = @()
foreach ($Spec in $Specs) {
    $PassHashes = @()
    $Record = $null
    for ($Pass = 0; $Pass -lt 2; ++$Pass) {
        $Mode = "gameplay-tipoff-proof-frame$($Spec.frame)"
        $Png = Join-Path $OutputRoot ("{0:D3}-{1}-pass{2}.png" -f $Spec.frame,$Spec.name,$Pass)
        $Text = (& $Exe --root $ProjectRoot --render-test-mode $Mode $Png 2>&1 | Out-String)
        if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $Png)) {
            throw "Trajectory timing proof render failed for $Mode.`n$Text"
        }
        $PassHashes += (Get-FileHash -LiteralPath $Png -Algorithm SHA256).Hash
        $TimingLine = $Text -split "`r?`n" | Where-Object { $_ -like 'tipoff-timing *' } | Select-Object -First 1
        $FlightLine = $Text -split "`r?`n" | Where-Object { $_ -like 'tipball-trajectory *' } | Select-Object -First 1
        if (!$TimingLine -or !$FlightLine) { throw "Missing timing diagnostics for $Mode." }
        if ($Pass -eq 0) {
            $T = Convert-Diagnostic $TimingLine
            $F = Convert-Diagnostic $FlightLine
            $Record = [pscustomobject]@{
                name=$Spec.name; logical_frame=[int]$F['logical-frame']; total_frame=[int]$F['total-frame']
                simulation_tick=[int]$F['simulation-tick']; presentation_phase=$F['presentation-phase']
                cinematic_visible=[int]$F['cinematic-visible']; ball_state=[int]$F['ball-state']
                b_latch=[int]$T['away-latch']; b_countdown=[int]$T['away-countdown']
                away_state=[int]$T['away-state']; away_height_q8=[int]$T['away-altitude-q8']; away_velocity_q8=[int]$T['away-velocity-q8']
                home_state=[int]$T['home-state']; home_height_q8=[int]$T['home-altitude-q8']; home_velocity_q8=[int]$T['home-velocity-q8']
                ball_height_q8=[int]$F['ball-height-q8']; ball_velocity_height_q8=[int]$F['velocity-height-q8']
                target_x=[int]$F['target-x']; target_depth=[int]$F['target-depth']; receiver=[int]$F.receiver
                duration=[int]$F.duration; remaining=[int]$F.remaining
                position_x_q6=[int]$F['position-x-q6']; position_depth_q6=[int]$F['position-depth-q6']
                velocity_x_prehalf_q6=[int]$F['velocity-x-prehalf-q6']; velocity_depth_prehalf_q6=[int]$F['velocity-depth-prehalf-q6']
                velocity_x_q6=[int]$F['velocity-x-q6']; velocity_depth_q6=[int]$F['velocity-depth-q6']
                workspace_6768=[int]$F['workspace-6768']; event_bit20=[int]$F['event-bit20']
                claim_frame=[int]$F['claim-frame']; first_cinematic_frame=[int]$F['first-cinematic-frame']
                claimant=[int]$F.claimant; holder=[int]$F.holder; possession=[int]$F.possession
                attached=[int]$F.attached; in_flight=[int]$F['in-flight']; flight_tick=[int]$F['flight-tick']
                distance_q8=[int]$F['distance-q8']; png=$Png; png_sha256=$PassHashes[0]
                timing_diagnostic=$TimingLine; flight_diagnostic=$FlightLine
            }
        }
    }
    if ($PassHashes[0] -ne $PassHashes[1]) { throw "$($Spec.name) PNG was nondeterministic." }
    $Records += $Record
}

$Request=$Records[0]; $Commit=$Records[1]; $Near=$Records[2]; $Gate=$Records[3]
$Early=$Records[4]; $Mid=$Records[5]; $Contact=$Records[6]; $Live=$Records[7]
if (!$Request.b_latch -or $Request.cinematic_visible -or $Request.ball_state -eq 0x17) { throw 'B request incorrectly started the cinematic.' }
if ($Commit.away_state -ne 0x0B -or $Commit.away_height_q8 -le 0 -or $Commit.cinematic_visible) { throw 'Committed human jumper timing checkpoint failed.' }
if ($Near.cinematic_visible -or $Near.away_height_q8 -ne $Commit.away_height_q8) { throw 'Last pre-cinematic checkpoint failed.' }
if (!$Gate.cinematic_visible -or $Gate.ball_state -ne 0x17 -or !$Gate.event_bit20 -or $Gate.first_cinematic_frame -ne $Gate.total_frame) { throw 'State `$17 cinematic gate failed.' }
if ($Gate.claim_frame -eq 65535 -or $Gate.claim_frame -le 15 -or $Gate.total_frame -eq $Request.total_frame) { throw 'Claim happened on the request/commit frame.' }
if ($Gate.duration -le 0 -or $Gate.duration -gt 60) { throw 'Dynamic `$B32C duration is out of source range.' }
if ($Gate.velocity_x_prehalf_q6 -eq 0 -or $Gate.velocity_x_q6 -ne [Math]::Floor($Gate.velocity_x_prehalf_q6 / 2.0)) { throw '`$AA84 X halving mismatch.' }
if ($Gate.velocity_depth_q6 -ne [Math]::Floor($Gate.velocity_depth_prehalf_q6 / 2.0)) { throw '`$AA84 depth halving mismatch.' }
if (!$Early.in_flight -or !$Mid.in_flight -or $Mid.flight_tick -le $Early.flight_tick -or $Mid.distance_q8 -ge $Early.distance_q8) { throw 'State `$17 fixed-point flight did not progress.' }
if ($Contact.in_flight -or !$Contact.attached -or $Contact.flight_tick -le 28 -or $Contact.holder -ne 255) { throw 'Low/contact settlement timing failed.' }
if (!$Live.attached -or $Live.holder -ne $Live.receiver -or $Live.presentation_phase -ne 'live') { throw 'Live receiver attachment failed.' }

$MetadataPath = Join-Path $OutputRoot 'tipoff-trajectory-timing.json'
[pscustomobject]@{
    schema='tecmo.tipoff-trajectory-timing-proof/1'; assertions_passed=$true
    deterministic_passes=2; records=$Records
} | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath $MetadataPath -Encoding UTF8

Add-Type -AssemblyName System.Drawing
$W=320; $H=240; $Label=34; $Cols=4; $Rows=2
$Sheet=New-Object System.Drawing.Bitmap ($W*$Cols),(($H+$Label)*$Rows)
$G=[System.Drawing.Graphics]::FromImage($Sheet);$G.Clear([System.Drawing.Color]::Black)
$Font=New-Object System.Drawing.Font('Consolas',9,[System.Drawing.FontStyle]::Bold)
try {
    for($i=0;$i -lt $Records.Count;$i++) {
        $R=$Records[$i];$x=($i%$Cols)*$W;$y=[Math]::Floor($i/$Cols)*($H+$Label)
        $Img=[System.Drawing.Image]::FromFile($R.png)
        try{$G.DrawImage($Img,$x,$y,$W,$H)}finally{$Img.Dispose()}
        $G.DrawString("f$($R.logical_frame) $($R.name)",$Font,[System.Drawing.Brushes]::White,$x+3,$y+$H+3)
        $G.DrawString("state=$($R.ball_state) h=$($R.ball_height_q8) tick=$($R.flight_tick)",$Font,[System.Drawing.Brushes]::LightGray,$x+3,$y+$H+17)
    }
    $ContactSheet=Join-Path $OutputRoot 'tipoff-trajectory-timing-contact-sheet.png'
    $Sheet.Save($ContactSheet,[System.Drawing.Imaging.ImageFormat]::Png)
} finally {$Font.Dispose();$G.Dispose();$Sheet.Dispose()}

Write-Host 'TIPOFF TRAJECTORY TIMING PROOF PASS'
Write-Host "Metadata: $MetadataPath"
Write-Host "Contact sheet: $ContactSheet"
