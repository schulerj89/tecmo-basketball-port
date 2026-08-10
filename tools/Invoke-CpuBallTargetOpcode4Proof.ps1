param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [string]$OutputDirectory,
    [switch]$Build
)

$ErrorActionPreference = "Stop"
$ExpectedRomSha256 =
    "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if (!$RomPath) { $RomPath = $env:TECMO_ROM_PATH }
if (!$RomPath -or !(Test-Path -LiteralPath $RomPath -PathType Leaf)) {
    throw "Pass -RomPath or set TECMO_ROM_PATH to the local Rev1 iNES ROM."
}
$RomPath = (Resolve-Path -LiteralPath $RomPath).Path
if ((Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash -ne
        $ExpectedRomSha256) {
    throw "Opcode-4 proof requires the exact Tecmo NBA Basketball Rev1 ROM."
}

$BuildRoot = [IO.Path]::GetFullPath((Join-Path $ProjectRoot "build"))
if (!$OutputDirectory) {
    $OutputDirectory = Join-Path $BuildRoot "cpu-ball-target-opcode4-proof"
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
        & (Join-Path $ProjectRoot "build.ps1")
        if ($LASTEXITCODE -ne 0) { throw "Build failed." }
    } finally {
        $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    }
}

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$PackPath = Join-Path $OutputDirectory "opcode4-ball-target.assetpack"
& $Executable --build-assetpack $RomPath $PackPath
if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $PackPath -PathType Leaf)) {
    throw "Could not build the ephemeral full asset pack for the opcode-4 proof."
}

$Records = @()
foreach ($Repeat in 1, 2) {
    $PngPath = Join-Path $OutputDirectory ("opcode4-ball-target-{0}.png" -f $Repeat)
    $StatePath = Join-Path $OutputDirectory ("opcode4-ball-target-{0}.json" -f $Repeat)
    $Output = @(& $Executable --root $ProjectRoot `
        --gameplay-live-foundation-proof $PackPath "cpu-target-deferred" `
        $PngPath 2>&1)
    $Text = ($Output -join [Environment]::NewLine).Trim()
    if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $PngPath -PathType Leaf)) {
        throw "Production opcode-4 proof run $Repeat failed.`n$Text"
    }
    try {
        $State = $Text | ConvertFrom-Json
    } catch {
        throw "Production opcode-4 proof run $Repeat emitted invalid JSON."
    }
    $Proof = $State.opcode4_ball_target
    if ($State.schema -ne "tecmo.live-proof/TGLP-1" -or
        $State.event -ne "cpu-target-deferred" -or
        ![bool]$Proof.executed -or
        [string]$Proof.record_offset -ne "0000" -or
        [int]$Proof.argument_c8 -ne 10 -or
        [int]$Proof.target_object -ne 10 -or
        [int]$Proof.snapshot_ball[0] -ne [int]$Proof.source_target[0] -or
        [int]$Proof.snapshot_ball[1] -ne [int]$Proof.source_target[1]) {
        throw "Production opcode-4 proof run $Repeat did not retain C8=$0A ball-target evidence."
    }
    Set-Content -LiteralPath $StatePath -Value $Text -Encoding UTF8
    $Records += [pscustomobject]@{
        png = $PngPath
        state = $StatePath
        png_sha256 = (Get-FileHash -LiteralPath $PngPath -Algorithm SHA256).Hash
        state_sha256 = (Get-FileHash -LiteralPath $StatePath -Algorithm SHA256).Hash
        snapshot_ball = @([int]$Proof.snapshot_ball[0], [int]$Proof.snapshot_ball[1])
    }
}
if ($Records[0].png_sha256 -ne $Records[1].png_sha256 -or
    $Records[0].state_sha256 -ne $Records[1].state_sha256) {
    throw "Opcode-4 proof was not deterministic across the two production runs."
}
$ManifestPath = Join-Path $OutputDirectory "opcode4-ball-target-proof.json"
([pscustomobject]@{
    schema = "tecmo.cpu-ball-target-opcode4-proof/TGOB-1"
    status = "PASS"
    asset_pack = $PackPath
    canonical_record = @{ offset = "0000"; opcode = 4; c8 = 10 }
    ball_target_snapshot = $Records[0].snapshot_ball
    deterministic_png_sha256 = $Records[0].png_sha256
    deterministic_state_sha256 = $Records[0].state_sha256
    records = $Records
} | ConvertTo-Json -Depth 5) |
    Set-Content -LiteralPath $ManifestPath -Encoding UTF8

Write-Output ("CPU opcode-4 ball-target proof passed: manifest={0} png_sha256={1}" -f
    $ManifestPath, $Records[0].png_sha256)
