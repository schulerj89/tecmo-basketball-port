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
if (!$OutputRoot) { $OutputRoot = Join-Path $ProjectRoot 'build\proof\tip-ball-trajectory' }
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
    @{side='home'; name='claim-contact-center'; frame=498},
    @{side='home'; name='state17-launch'; frame=499},
    @{side='home'; name='early-flight'; frame=505},
    @{side='home'; name='mid-flight'; frame=530},
    @{side='home'; name='low-near-receiver'; frame=550},
    @{side='home'; name='receiver-live'; frame=598},
    @{side='away'; name='claim-contact-center'; frame=498},
    @{side='away'; name='state17-launch'; frame=499},
    @{side='away'; name='early-flight'; frame=505},
    @{side='away'; name='mid-flight'; frame=530},
    @{side='away'; name='contact-near-receiver'; frame=550},
    @{side='away'; name='receiver-live'; frame=598}
)

$Records = @()
foreach ($Spec in $Specs) {
    $Mode = "gameplay-tip-ball-$($Spec.side)-frame$($Spec.frame)"
    $Png = Join-Path $OutputRoot ("{0}-{1:D3}-{2}.png" -f $Spec.side,$Spec.frame,$Spec.name)
    $Text = (& $Exe --root $ProjectRoot --render-test-mode $Mode $Png 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $Png)) {
        throw "Tip-ball proof render failed for $Mode.`n$Text"
    }
    $Line = $Text -split "`r?`n" | Where-Object { $_ -like 'tipball-trajectory *' } | Select-Object -First 1
    if (!$Line) { throw "Missing trajectory diagnostic for $Mode." }
    $D = Convert-Diagnostic $Line
    $Records += [pscustomobject]@{
        side=$Spec.side; name=$Spec.name; frame=$Spec.frame; png=$Png; mode=$Mode
        logical_frame=[int]$D['logical-frame']; total_frame=[int]$D['total-frame']; simulation_tick=[int]$D['simulation-tick']
        presentation_phase=$D['presentation-phase']; cinematic_visible=[int]$D['cinematic-visible']; claimant=[int]$D.claimant
        selector_037f=[int]$D['selector-037f']; selector_0380=[int]$D['selector-0380']; receiver=[int]$D.receiver
        target_x=[int]$D['target-x']; target_depth=[int]$D['target-depth']; ball_state=[int]$D['ball-state']
        ball_x_q8=[int]$D['ball-x-q8']; ball_depth_q8=[int]$D['ball-depth-q8']; ball_height_q8=[int]$D['ball-height-q8']
        velocity_x_q8=[int]$D['velocity-x-q8']; velocity_depth_q8=[int]$D['velocity-depth-q8']; velocity_height_q8=[int]$D['velocity-height-q8']
        distance_q8=[int]$D['distance-q8']; holder=[int]$D.holder; possession=[int]$D.possession
        attached=[int]$D.attached; in_flight=[int]$D['in-flight']; flight_tick=[int]$D['flight-tick']; diagnostic=$Line
    }
}

foreach ($Side in 'home','away') {
    $Row = @($Records | Where-Object side -eq $Side)
    $Launch = $Row[0]; $Mid = $Row[3]; $Final = $Row[-1]
    if ($Launch.ball_state -ne 0x17 -or !$Launch.in_flight -or $Launch.attached) { throw "$Side did not launch in state `$17." }
    if ($Mid.distance_q8 -ge $Launch.distance_q8) { throw "$Side distance did not decrease." }
    if ([Math]::Abs($Mid.ball_x_q8 - $Launch.ball_x_q8) -le (8 * 256)) { throw "$Side flight did not visibly exceed eight pixels." }
    foreach ($Flight in ($Row | Select-Object -First 5)) {
        if ($Flight.holder -ne 255) { throw "$Side awarded a holder before live attachment." }
    }
    if (!$Final.attached -or $Final.in_flight -or $Final.holder -ne $Final.receiver -or $Final.distance_q8 -ne 0) {
        throw "$Side final receiver attachment failed."
    }
}
$HomeLaunch = $Records | Where-Object { $_.side -eq 'home' -and $_.name -eq 'claim-contact-center' }
$AwayLaunch = $Records | Where-Object { $_.side -eq 'away' -and $_.name -eq 'claim-contact-center' }
if ($HomeLaunch.velocity_x_q8 -ge 0 -or $AwayLaunch.velocity_x_q8 -le 0) { throw 'Side trajectories are not oppositely target-derived.' }
foreach ($Launch in $HomeLaunch,$AwayLaunch) {
    $ExpectedReceiver = if ($Launch.claimant -eq 0) {
        $Launch.selector_037f
    } else {
        $Launch.selector_0380
    }
    if ($Launch.receiver -ne $ExpectedReceiver) { throw 'Receiver selector-array semantics failed.' }
}

$MetadataPath = Join-Path $OutputRoot 'tip-ball-trajectory.json'
[pscustomobject]@{ schema='tecmo.tip-ball-trajectory-proof/1'; generated_utc=(Get-Date).ToUniversalTime().ToString('o'); assertions_passed=$true; records=$Records } |
    ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $MetadataPath -Encoding UTF8

Add-Type -AssemblyName System.Drawing
$W=320; $H=240; $Label=28; $Cols=6; $Rows=2
$Sheet = New-Object System.Drawing.Bitmap ($W*$Cols),(($H+$Label)*$Rows)
$G=[System.Drawing.Graphics]::FromImage($Sheet); $G.Clear([System.Drawing.Color]::Black)
$Font=New-Object System.Drawing.Font('Consolas',10,[System.Drawing.FontStyle]::Bold)
try {
    for($i=0;$i -lt $Records.Count;$i++) {
        $R=$Records[$i]; $x=($i%$Cols)*$W; $y=[Math]::Floor($i/$Cols)*($H+$Label)
        $Img=[System.Drawing.Image]::FromFile($R.png)
        try{$G.DrawImage($Img,$x,$y,$W,$H)}finally{$Img.Dispose()}
        $G.DrawString("$($R.side) f$($R.frame) $($R.name)",$Font,[System.Drawing.Brushes]::White,$x+3,$y+$H+4)
    }
    $Contact=Join-Path $OutputRoot 'tip-ball-trajectory-contact-sheet.png'
    $Sheet.Save($Contact,[System.Drawing.Imaging.ImageFormat]::Png)
} finally { $Font.Dispose(); $G.Dispose(); $Sheet.Dispose() }

Write-Host 'TIP-BALL TRAJECTORY PROOF PASS'
Write-Host "Metadata: $MetadataPath"
Write-Host "Contact sheet: $Contact"
