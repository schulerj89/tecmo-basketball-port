param(
    [Parameter(Mandatory=$true)][string]$PackPath,
    [switch]$Build
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if ($Build) { & (Join-Path $repo 'build.ps1'); if ($LASTEXITCODE) { exit $LASTEXITCODE } }
$exe = Join-Path $repo 'build\tecmo_port.exe'
$env:TECMO_ASSETPACK = (Resolve-Path $PackPath).Path
$out = Join-Path $repo 'build\proof'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$names = @('before-pass','immediate-handoff','after-defender-selection')
$records = @()
for ($stage = 0; $stage -lt 3; $stage++) {
    $png = Join-Path $out ($names[$stage] + '.png')
    $line = & $exe --root $repo --render-test-mode `
        "gameplay-pass-handoff-proof-frame$stage" $png 2>&1 | `
        Where-Object { $_ -match '^pass-handoff-proof ' } | Select-Object -Last 1
    if ($LASTEXITCODE -or !$line -or !(Test-Path $png)) { throw "native proof stage $stage failed" }
    $values = @{}
    ([string]$line -split ' ') | Select-Object -Skip 1 | ForEach-Object {
        $pair = $_ -split '=',2; if ($pair.Count -eq 2) { $values[$pair[0]] = $pair[1] }
    }
    $records += [pscustomobject]@{ stage=$stage; label=$names[$stage]; png=$png; state=$values }
}
if ($records[0].state.selected_offense -ne '0' -or $records[0].state.selected_defense -ne '5' -or
    $records[1].state.selected_offense -ne '1' -or $records[1].state.prior_offense -ne '0' -or
    $records[1].state.old_holder_state -ne '4' -or $records[1].state.old_holder_cursor -ne '0B63' -or
    $records[2].state.selected_defense -ne '6' -or $records[2].state.prior_defense -ne '5' -or
    $records[2].state.linked -ne '1' -or $records[2].state.eligible -ne '1') {
    throw 'proof metadata did not establish offense 0->1, state4/$0B63, and linked eligible defense 5->6'
}
$metadata = Join-Path $out 'pass-defender-handoff.json'
$records | ConvertTo-Json -Depth 5 | Set-Content -Encoding utf8 $metadata
Add-Type -AssemblyName System.Drawing
$images = $records | ForEach-Object { [Drawing.Image]::FromFile($_.png) }
$width = [int]$images[0].Width
$height = 90
foreach ($img in $images) { if ($img.Width -gt $width) { $width = $img.Width }; $height += $img.Height }
$sheet = New-Object Drawing.Bitmap $width,$height
$g = [Drawing.Graphics]::FromImage($sheet); $g.Clear([Drawing.Color]::Black)
$font = [Drawing.Font]::new('Arial',[single]14,[Drawing.FontStyle]::Bold)
$y = 0
for ($i=0; $i -lt $images.Count; $i++) {
    $g.DrawString(("{0}. {1}" -f ($i+1),$names[$i]),$font,[Drawing.Brushes]::White,8,$y+4)
    $y += 30; $g.DrawImage($images[$i],0,$y); $y += $images[$i].Height
}
$contact = Join-Path $out 'pass-defender-handoff-contact-sheet.png'
$sheet.Save($contact,[Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $sheet.Dispose(); $font.Dispose(); $images | ForEach-Object { $_.Dispose() }
Write-Host "PASS DEFENDER HANDOFF PROOF PASS metadata=$metadata contact=$contact"
