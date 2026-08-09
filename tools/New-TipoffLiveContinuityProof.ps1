param(
    [string]$ProjectRoot,
    [string]$AssetPackPath,
    [string]$OutputRoot
)

$ErrorActionPreference = 'Stop'
if (!$ProjectRoot) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if (!$AssetPackPath) {
    $AssetPackPath = Join-Path $ProjectRoot 'build\gameplay-pretip-tests\tecmo.assetpack'
}
$AssetPackPath = (Resolve-Path -LiteralPath $AssetPackPath).Path
if (!$OutputRoot) {
    $OutputRoot = Join-Path $ProjectRoot 'build\proof\tipoff-live-continuity'
}
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

$Frames = @(589,590,591,596,604)
$Names = @('before-handoff','handoff','first-live','live-motion-a','live-motion-b')
$Records = @()
for ($Index=0; $Index -lt $Frames.Count; ++$Index) {
    $Frame=$Frames[$Index]; $Name=$Names[$Index]
    $Png=Join-Path $OutputRoot ("{0:D3}-{1}.png" -f $Frame,$Name)
    $Mode="gameplay-tipoff-continuity-frame$Frame"
    $Text=(& $Exe --root $ProjectRoot --render-test-mode $Mode $Png 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $Png)) {
        throw "Continuity render failed for $Mode.`n$Text"
    }
    $Line=$Text -split "`r?`n" | Where-Object { $_ -like 'tipoff-continuity *' } | Select-Object -First 1
    if (!$Line) { throw "Missing continuity diagnostic for $Mode." }
    $D=Convert-Diagnostic $Line
    $Actors=@()
    for($Actor=0;$Actor -lt 10;++$Actor) {
        $V=@($D["actor$Actor"] -split ',')
        if($V.Count -ne 19){throw "Malformed actor $Actor diagnostic at frame $Frame."}
        $Actors += [pscustomobject]@{
            id=$Actor; x=[int]$V[0]; depth=[int]$V[1]; anchor_x=[int]$V[2]; anchor_depth=[int]$V[3]
            pose=[int]$V[4]; facing_right=[int]$V[5]; orientation_encoded=[int]$V[6]
            movement_action=[int]$V[7]; movement_direction=[int]$V[8]; movement_fraction=[int]$V[9]
            movement_animation=[int]$V[10]; cpu_decision_serial=[uint32]$V[11]
            cpu_command_offset=[uint16]$V[12]; cpu_link=[int]$V[13]; cpu_target_valid=[int]$V[14]
            cpu_target_x=[int]$V[15]; cpu_target_depth=[int]$V[16]
            live_foundation_x=[int]$V[17]; live_foundation_depth=[int]$V[18]
        }
    }
    $Records += [pscustomobject]@{
        name=$Name; frame=$Frame; png=$Png; logical_frame=[int]$D['logical-frame']; phase=$D.phase
        holder=[int]$D.holder; possession=[int]$D.possession; camera_x=[int]$D['camera-x']
        camera_follow_serial=[int]$D['follow-serial']; claimant=[int]$D.claimant; receiver=[int]$D.receiver
        selected_0308=[int]$D['selected-0308']; selected_0309=[int]$D['selected-0309']; ball_state=[int]$D['ball-state']
        ball_x_q8=[int]$D['ball-x-q8']; ball_y_q8=[int]$D['ball-y-q8']; ball_attached=[int]$D['ball-attached']
        away_state=[int]$D['away-state']; away_altitude_q8=[int]$D['away-altitude-q8']; away_commits=[int]$D['away-commits']
        home_state=[int]$D['home-state']; home_altitude_q8=[int]$D['home-altitude-q8']; home_commits=[int]$D['home-commits']
        live_foundation_sync_serial=[uint32]$D['foundation-sync-serial']; actors=$Actors; diagnostic=$Line
    }
}

$Before=$Records[0]; $Handoff=$Records[1]
for($Actor=0;$Actor -lt 10;++$Actor) {
    $A=$Before.actors[$Actor]; $B=$Handoff.actors[$Actor]
    foreach($Field in 'x','depth','anchor_x','anchor_depth','pose','facing_right','orientation_encoded','movement_action','movement_direction','movement_fraction','movement_animation','cpu_decision_serial','cpu_command_offset','cpu_link','cpu_target_valid','cpu_target_x','cpu_target_depth') {
        if($A.$Field -ne $B.$Field){throw "Actor $Actor field $Field changed solely at handoff."}
    }
}
if($Handoff.phase -ne 'live' -or $Handoff.holder -ne 7 -or $Handoff.possession -ne 1){throw 'Receiver/holder/possession handoff failed.'}
if(!$Handoff.ball_attached -or $Handoff.holder -ne $Handoff.receiver){throw 'Ball attachment is not bound to the resolved receiver.'}
if($Handoff.actors[4].pose -ne 469 -or $Handoff.actors[9].pose -ne 501){throw 'Jumper landing poses restarted or reset.'}
$StartupX=@(528,448,362,364,392,176,320,408,400,372)
$AllStartup=$true
for($Actor=0;$Actor -lt 10;++$Actor){if($Handoff.actors[$Actor].x -ne $StartupX[$Actor]){$AllStartup=$false}}
if($AllStartup){throw 'Handoff replayed the cold startup arrangement.'}
$Moved=$false
for($Actor=0;$Actor -lt 10;++$Actor){
    if($Handoff.actors[$Actor].live_foundation_x -ne $Handoff.actors[$Actor].x -or $Handoff.actors[$Actor].live_foundation_depth -ne $Handoff.actors[$Actor].depth){throw "Live foundation diverged for actor $Actor."}
    if($Records[-1].actors[$Actor].x -ne $Handoff.actors[$Actor].x -or $Records[-1].actors[$Actor].depth -ne $Handoff.actors[$Actor].depth){$Moved=$true}
}
if(!$Moved){throw 'Ordinary live movement remained frozen after handoff.'}
foreach($Record in ($Records | Select-Object -Skip 1)){
    if($Record.away_altitude_q8 -ne 0 -or $Record.home_altitude_q8 -ne 0 -or $Record.away_commits -ne 1 -or $Record.home_commits -ne 1){throw "Jumper restarted at frame $($Record.frame)."}
    for($Actor=0;$Actor -lt 10;++$Actor){
        if($Record.actors[$Actor].live_foundation_x -ne $Record.actors[$Actor].x -or $Record.actors[$Actor].live_foundation_depth -ne $Record.actors[$Actor].depth){throw "Live foundation diverged for actor $Actor at frame $($Record.frame)."}
    }
}

$Metadata=Join-Path $OutputRoot 'tipoff-live-continuity.json'
[pscustomobject]@{schema='tecmo.tipoff-live-continuity-proof/1';assertions_passed=$true;records=$Records} |
    ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $Metadata -Encoding UTF8

Add-Type -AssemblyName System.Drawing
$W=320;$H=240;$Label=34;$Cols=5
$Sheet=New-Object System.Drawing.Bitmap ($W*$Cols),($H+$Label)
$G=[System.Drawing.Graphics]::FromImage($Sheet);$G.Clear([System.Drawing.Color]::Black)
$Font=New-Object System.Drawing.Font('Consolas',9,[System.Drawing.FontStyle]::Bold)
try {
    for($i=0;$i -lt $Records.Count;++$i){
        $R=$Records[$i];$x=$i*$W
        $Img=[System.Drawing.Image]::FromFile($R.png)
        try{$G.DrawImage($Img,$x,0,$W,$H)}finally{$Img.Dispose()}
        foreach($A in $R.actors){
            $sx=$x+$A.x-$R.camera_x; $sy=$A.depth-18
            if($sx -ge $x -and $sx -lt $x+$W -and $sy -ge 0 -and $sy -lt $H){$G.DrawString("$($A.id)",$Font,[System.Drawing.Brushes]::Yellow,$sx,$sy)}
        }
        $G.DrawString("f$($R.frame) $($R.name) h=$($R.holder)",$Font,[System.Drawing.Brushes]::White,$x+3,$H+4)
    }
    $Contact=Join-Path $OutputRoot 'tipoff-live-continuity-contact-sheet.png'
    $Sheet.Save($Contact,[System.Drawing.Imaging.ImageFormat]::Png)
} finally {$Font.Dispose();$G.Dispose();$Sheet.Dispose()}

$FirstHashes=@{}
Get-ChildItem -LiteralPath $OutputRoot -Filter '*.png' | ForEach-Object {$FirstHashes[$_.Name]=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}
foreach($Record in $Records){
    $Text=(& $Exe --root $ProjectRoot --render-test-mode ("gameplay-tipoff-continuity-frame$($Record.frame)") $Record.png 2>&1 | Out-String)
    if($LASTEXITCODE -ne 0){throw "Repeat render failed for frame $($Record.frame).`n$Text"}
}
foreach($Record in $Records){if((Get-FileHash $Record.png -Algorithm SHA256).Hash -ne $FirstHashes[(Split-Path -Leaf $Record.png)]){throw "Nondeterministic PNG at frame $($Record.frame)."}}

Write-Host 'TIP-OFF LIVE CONTINUITY PROOF PASS'
Write-Host "Metadata: $Metadata"
Write-Host "Contact sheet: $Contact"
