param(
    [Parameter(Mandatory=$true)][string]$PackPath,
    [switch]$Build
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if ($Build) { & (Join-Path $repo 'build.ps1'); if ($LASTEXITCODE) { exit $LASTEXITCODE } }
$exe = Join-Path $repo 'build\tecmo_port.exe'
$out = Join-Path $repo 'build\proof\directional-pass-defender-selection'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$env:TECMO_ASSETPACK = (Resolve-Path $PackPath).Path
$labels = @(
    'offense RIGHT chooses slot2 (not sequential slot1)',
    'offense LEFT chooses slot4',
    'pass consumes slot2; handoff selects linked defender7',
    'human defense RIGHT chooses slot2',
    'human defense LEFT chooses slot4',
    'defensive switch consumes slot4'
)
function Invoke-ProofPass([string]$suffix) {
    $items = @()
    for ($stage=0; $stage -lt 6; $stage++) {
        $png = Join-Path $out ("stage-$stage$suffix.png")
        $all = & $exe --root $repo --render-test-mode `
            "gameplay-directional-selection-frame$stage" $png 2>&1
        if ($LASTEXITCODE -or !(Test-Path $png)) { throw "native render stage $stage failed" }
        $line = $all | Where-Object { $_ -match '^directional-selection-proof ' } | Select-Object -Last 1
        if (!$line) { throw "stage $stage omitted machine-readable native metadata" }
        $json = ([string]$line -replace '^directional-selection-proof ','')
        $state = $json | ConvertFrom-Json
        $items += [pscustomobject]@{
            logical_frame=$stage; label=$labels[$stage]; png=$png
            png_sha256=(Get-FileHash $png -Algorithm SHA256).Hash
            state=$state; state_json=$json
        }
    }
    return $items
}
$first = Invoke-ProofPass ''
$second = Invoke-ProofPass '-repeat'
for ($i=0; $i -lt 6; $i++) {
    if ($first[$i].png_sha256 -ne $second[$i].png_sha256 -or
        $first[$i].state_json -ne $second[$i].state_json) {
        throw "directional proof is nondeterministic at stage $i"
    }
}
if ($first[0].state.chosen_candidate -ne 2 -or $first[1].state.chosen_candidate -ne 4 -or
    $first[2].state.pass_target -ne 2 -or $first[2].state.final_holder -ne 2 -or
    $first[2].state.final_defender -ne 7 -or
    $first[3].state.chosen_candidate -ne 2 -or $first[4].state.chosen_candidate -ne 4 -or
    $first[5].state.final_defender -ne 4) {
    throw 'directional receiver/pass/handoff/defensive-switch facts failed'
}
foreach ($index in 0,1,2) {
    $s=$first[$index].state
    if ($s.direction_nibble -ne $s.mapped_sector -or
        $s.actors[$s.chosen_candidate].polarity -ne 0 -or
        ($index -lt 2 -and $s.chosen_candidate -eq $s.raw_000e)) {
        throw "offensive filter/sector failure stage $index"
    }
}
foreach ($index in 3,4,5) {
    $s=$first[$index].state
    if ($s.direction_nibble -ne $s.mapped_sector -or
        $s.actors[$s.chosen_candidate].polarity -ne 1 -or
        $s.chosen_candidate -eq 0) { throw "defensive filter/sector failure stage $index" }
}
if ((@($first[0].state.chosen_candidate,$first[1].state.chosen_candidate) |
      Where-Object { $_ -ne 1 }).Count -eq 0) {
    throw 'all demonstrated offensive choices collapsed to sequential cycling'
}
$metadata = Join-Path $out 'directional-pass-defender-selection.json'
$first | Select-Object logical_frame,label,png,png_sha256,state | ConvertTo-Json -Depth 12 |
    Set-Content -Encoding utf8 $metadata
Add-Type -AssemblyName System.Drawing
$images = $first | ForEach-Object { [Drawing.Image]::FromFile($_.png) }
$width = [int]$images[0].Width; $height = 0
foreach ($img in $images) { if ($img.Width -gt $width) {$width=$img.Width}; $height += $img.Height + 34 }
$sheet=[Drawing.Bitmap]::new($width,$height); $g=[Drawing.Graphics]::FromImage($sheet)
$g.Clear([Drawing.Color]::Black); $font=[Drawing.Font]::new('Arial',[single]12,[Drawing.FontStyle]::Bold)
$y=0
for($i=0;$i-lt$images.Count;$i++){$g.DrawString(("{0}. {1}"-f($i+1),$labels[$i]),$font,[Drawing.Brushes]::White,6,$y+5);$y+=34;$g.DrawImage($images[$i],0,$y);$y+=$images[$i].Height}
$contact=Join-Path $out 'directional-pass-defender-selection-contact-sheet.png'
$sheet.Save($contact,[Drawing.Imaging.ImageFormat]::Png)
$g.Dispose();$sheet.Dispose();$font.Dispose();$images|ForEach-Object{$_.Dispose()}
Write-Host "DIRECTIONAL PASS/DEFENDER SELECTION PROOF PASS metadata=$metadata contact=$contact"
