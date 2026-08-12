param(
    [Parameter(Mandatory=$true)][string]$PackPath,
    [switch]$Build
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if ($Build) {
    & (Join-Path $repo 'build.ps1')
    if ($LASTEXITCODE) { exit $LASTEXITCODE }
}
$exe = Join-Path $repo 'build\tecmo_port.exe'
$out = Join-Path $repo 'build\proof\shot-direction-bank05'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$env:TECMO_ASSETPACK = (Resolve-Path $PackPath).Path

# Bank05 $8DD3 -> $BF6C direction slots, then $842C -> $8D3D/$8D5D.
# The proof uses production (non-lab) source-shaped launches for both hoops.
$cases = @(
    [pscustomobject]@{ token='away-horizontal'; orientation=0; direction=1; facing=0; dxSign=-1; dySign=0; endX=40960 },
    [pscustomobject]@{ token='home-horizontal'; orientation=1; direction=0; facing=1; dxSign=1; dySign=0; endX=155648 },
    [pscustomobject]@{ token='away-diagonal'; orientation=0; direction=7; facing=0; dxSign=-1; dySign=-1; endX=40960 },
    [pscustomobject]@{ token='home-diagonal'; orientation=1; direction=3; facing=1; dxSign=1; dySign=1; endX=155648 }
)
$poseMatrix = @(
    @(245,213,229,237,221,197,253,205),
    @(309,277,293,301,285,261,317,269)
)

function Invoke-ShotDirectionPass([string]$suffix) {
    $records = @()
    foreach ($case in $cases) {
        foreach ($frame in 0,1) {
            $mode = "gameplay-shot-direction-$($case.token)-frame$frame"
            $png = Join-Path $out ($mode + $suffix + '.png')
            $all = & $exe --root $repo --render-test-mode $mode $png 2>&1
            if ($LASTEXITCODE -or !(Test-Path -LiteralPath $png)) {
                throw "native production render failed for $mode"
            }
            $line = $all | Where-Object { $_ -match '^shot-direction-proof ' } |
                Select-Object -Last 1
            if (!$line) { throw "$mode omitted native selector metadata" }
            $state = ([string]$line -replace '^shot-direction-proof ','') |
                ConvertFrom-Json
            if ($state.case -ne $case.token -or
                $state.checkpoint -ne $frame -or
                $state.orientation -ne $case.orientation -or
                $state.family -ne 0 -or
                $state.direction -ne $case.direction -or
                $state.profile -lt 0 -or $state.profile -gt 1 -or
                $state.pose -ne $poseMatrix[$state.profile][$case.direction] -or
                $state.facing_right -ne $case.facing -or
                $state.pose_orientation_encoded -ne 0 -or
                $state.end_x_q8 -ne $case.endX -or $state.end_y_q8 -ne 36608 -or
                [Math]::Sign([int]$state.target_dx) -ne $case.dxSign -or
                [Math]::Sign([int]$state.target_dy) -ne $case.dySign -or
                $state.compositor_mirror -lt 0 -or $state.compositor_mirror -gt 1 -or
                $state.released -ne $frame) {
                throw "$mode violated Bank05 selector, endpoint, facing, or one-mirror contract"
            }
            $records += [pscustomobject]@{
                case=$case.token; checkpoint=$frame; mode=$mode; png=$png
                png_sha256=(Get-FileHash -LiteralPath $png -Algorithm SHA256).Hash
                state=$state; state_json=([string]$line -replace '^shot-direction-proof ','')
            }
        }
    }
    return $records
}

$first = Invoke-ShotDirectionPass ''
$second = Invoke-ShotDirectionPass '-repeat'
for ($i=0; $i -lt $first.Count; ++$i) {
    if ($first[$i].png_sha256 -ne $second[$i].png_sha256 -or
        $first[$i].state_json -ne $second[$i].state_json) {
        throw "shot direction proof is nondeterministic for $($first[$i].mode)"
    }
}
foreach ($case in $cases) {
    $entry = $first | Where-Object { $_.case -eq $case.token -and $_.checkpoint -eq 0 } |
        Select-Object -First 1
    $release = $first | Where-Object { $_.case -eq $case.token -and $_.checkpoint -eq 1 } |
        Select-Object -First 1
    if (!$entry -or !$release -or $entry.png_sha256 -eq $release.png_sha256) {
        throw "$($case.token) entry/release visual lifecycle collapsed"
    }
}

$metadata = Join-Path $out 'shot-direction-bank05.json'
$first | Select-Object case,checkpoint,mode,png,png_sha256,state |
    ConvertTo-Json -Depth 8 | Set-Content -Encoding utf8 $metadata

Add-Type -AssemblyName System.Drawing
$images = $first | ForEach-Object { [Drawing.Image]::FromFile($_.png) }
$width = [int](($images | Measure-Object -Property Width -Maximum).Maximum)
$height = 0
foreach ($image in $images) { $height += $image.Height + 26 }
$sheet = [Drawing.Bitmap]::new([int]$width, [int]$height)
$graphics = [Drawing.Graphics]::FromImage($sheet)
$graphics.Clear([Drawing.Color]::Black)
$font = [Drawing.Font]::new('Arial', [single]10, [Drawing.FontStyle]::Bold)
$y = 0
for ($i = 0; $i -lt $images.Count; ++$i) {
    $graphics.DrawString(
        ("{0} frame {1}: {2}" -f $first[$i].case,
         $first[$i].checkpoint, $first[$i].state.direction),
        $font, [Drawing.Brushes]::White, 4, $y + 4)
    $y += 26
    $graphics.DrawImage($images[$i], 0, $y)
    $y += $images[$i].Height
}
$contact = Join-Path $out 'shot-direction-bank05-contact-sheet.png'
$sheet.Save($contact, [Drawing.Imaging.ImageFormat]::Png)
$graphics.Dispose(); $sheet.Dispose(); $font.Dispose()
$images | ForEach-Object { $_.Dispose() }

Write-Host "SHOT DIRECTION BANK05 PROOF PASS metadata=$metadata contact=$contact"
