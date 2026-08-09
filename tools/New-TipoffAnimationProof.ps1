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
    @{ name="precommit-standing"; frame=661; awayPose=517; awayPhase=0; awayState=0x22; awayAltitude=$null; homePose=518; homePhase=0; homeState=0x13; homeAltitude=$null },
    @{ name="slot4-phase2"; frame=662; awayPose=549; awayPhase=2; awayState=0x0B; awayAltitude=$null; homePose=518; homePhase=0; homeState=0x13; homeAltitude=$null },
    @{ name="slot4-phase3"; frame=663; awayPose=550; awayPhase=3; awayState=0x0B; awayAltitude=$null; homePose=518; homePhase=0; homeState=0x13; homeAltitude=$null },
    @{ name="slot4-phase4-low"; frame=664; awayPose=551; awayPhase=4; awayState=0x0C; awayAltitude=$null; homePose=518; homePhase=0; homeState=0x13; homeAltitude=$null },
    @{ name="slot4-phase4-high"; frame=670; awayPose=551; awayPhase=4; awayState=0x0C; awayAltitude=$null; homePose=518; homePhase=0; homeState=0x13; homeAltitude=$null },
    @{ name="slot9-phase2"; frame=683; awayPose=551; awayPhase=4; awayState=0x0C; awayAltitude=$null; homePose=581; homePhase=2; homeState=0x0B; homeAltitude=$null },
    @{ name="slot9-phase3"; frame=684; awayPose=551; awayPhase=4; awayState=0x0C; awayAltitude=$null; homePose=582; homePhase=3; homeState=0x0B; homeAltitude=$null },
    @{ name="slot9-phase4-low"; frame=685; awayPose=551; awayPhase=4; awayState=0x0C; awayAltitude=$null; homePose=583; homePhase=4; homeState=0x0C; homeAltitude=$null },
    @{ name="slot9-phase4-high"; frame=690; awayPose=551; awayPhase=4; awayState=0x0C; awayAltitude=$null; homePose=583; homePhase=4; homeState=0x0C; homeAltitude=$null },
    @{ name="class-landing-live"; frame=721; awayPose=469; awayPhase=0; awayState=0x13; awayAltitude=0; homePose=501; homePhase=0; homeState=0x13; homeAltitude=0 }
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
        diagnostic=$Line
    }
}

$AwayLow = ($Records | Where-Object name -eq 'slot4-phase4-low').away.altitude_q8
$AwayHigh = ($Records | Where-Object name -eq 'slot4-phase4-high').away.altitude_q8
$HomeLow = ($Records | Where-Object name -eq 'slot9-phase4-low').home.altitude_q8
$HomeHigh = ($Records | Where-Object name -eq 'slot9-phase4-high').home.altitude_q8
if ($AwayLow -eq $AwayHigh) { throw "Slot 4 phase 4 altitude did not evolve." }
if ($HomeLow -eq $HomeHigh) { throw "Slot 9 phase 4 altitude did not evolve." }

$MetadataPath = Join-Path $OutputRoot "tipoff-animation-metadata.json"
[pscustomobject]@{
    schema="tecmo.tipoff-animation-proof/1"; generated_utc=(Get-Date).ToUniversalTime().ToString('o');
    asset_pack=$AssetPackPath; assertions_passed=$true; records=$Records
} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $MetadataPath -Encoding UTF8

Add-Type -AssemblyName System.Drawing
$CellWidth = 320; $ImageHeight = 240; $LabelHeight = 28; $Columns = 5; $Rows = 2
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
