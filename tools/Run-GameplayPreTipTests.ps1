param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [switch]$Build
)

$ErrorActionPreference = "Stop"
if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if (!$RomPath) { $RomPath = $env:TECMO_ROM_PATH }
if (!$RomPath -or !(Test-Path -LiteralPath $RomPath -PathType Leaf)) {
    throw "Pass -RomPath or set TECMO_ROM_PATH to the local Rev1 iNES ROM."
}
$RomPath = (Resolve-Path -LiteralPath $RomPath).Path
$ExpectedRomSha256 =
    "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4"
if ((Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash -ne
    $ExpectedRomSha256) {
    throw "TPTI-1 tests require the supported Rev1 ROM fingerprint."
}

$Scratch = Join-Path $ProjectRoot "build\gameplay-pretip-tests"
New-Item -ItemType Directory -Force -Path $Scratch | Out-Null
$Executable = Join-Path $ProjectRoot "build\tecmo_port.exe"
if ($Build) {
    $BuildLog = Join-Path $Scratch "build.log"
    & (Join-Path $ProjectRoot "build.ps1") *> $BuildLog
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed.`n$((Get-Content $BuildLog -Tail 40) -join "`n")"
    }
}
if (!(Test-Path -LiteralPath $Executable -PathType Leaf)) {
    throw "Native executable is missing. Pass -Build."
}

function Get-Entry {
    param([byte[]]$Bytes, [string]$Id)
    if ([Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -ne "TAP1") {
        throw "Malformed asset-pack header."
    }
    $Count = [BitConverter]::ToUInt32($Bytes, 16)
    $Directory = [BitConverter]::ToUInt64($Bytes, 20)
    for ($Index = 0; $Index -lt $Count; ++$Index) {
        $EntryOffset = [int]($Directory + [uint64]$Index * 128)
        $Name = [Text.Encoding]::ASCII.GetString($Bytes, $EntryOffset, 64).
            Split([char]0)[0]
        if ($Name -eq $Id) {
            return [pscustomobject]@{
                directory_offset = $EntryOffset
                pack_offset = [BitConverter]::ToUInt64($Bytes, $EntryOffset + 84)
                size = [BitConverter]::ToUInt64($Bytes, $EntryOffset + 92)
            }
        }
    }
    throw "Asset-pack entry '$Id' was not found."
}

function Get-Fnv32 {
    param([byte[]]$Bytes)
    [uint32]$Hash = 2166136261
    foreach ($Byte in $Bytes) {
        [uint64]$Product = [uint64]($Hash -bxor [uint32]$Byte) *
            [uint64]16777619
        $Hash = [uint32]($Product % [uint64]4294967296)
    }
    return $Hash.ToString("X8")
}

function Get-EntryBytes {
    param([byte[]]$Bytes, $Entry)
    $Result = New-Object byte[] ([int]$Entry.size)
    [Array]::Copy($Bytes, [int]$Entry.pack_offset,
                  $Result, 0, $Result.Length)
    return $Result
}

function Invoke-Native {
    param([string[]]$Arguments, [string]$LogName)
    $Log = Join-Path $Scratch $LogName
    & $Executable @Arguments *> $Log
    return [pscustomobject]@{
        code = $LASTEXITCODE
        text = (Get-Content -LiteralPath $Log -Raw)
        tail = ((Get-Content -LiteralPath $Log -Tail 30) -join "`n")
    }
}

function Assert-Rejected {
    param([string]$Pack, [string]$Label)
    $Run = Invoke-Native -Arguments @("--gameplay-pretip-test", $Pack) `
                         -LogName "$Label.log"
    if ($Run.code -eq 0 -or $Run.text -notmatch "TPTI-1") {
        throw "Malformed TPTI-1 pack '$Label' was accepted.`n$($Run.tail)"
    }
}

$PreviousPack = $env:TECMO_ASSETPACK
try {
$Pack = Join-Path $Scratch "tecmo.assetpack"
$BuildPack = Invoke-Native -Arguments @("--build-assetpack", $RomPath, $Pack) `
                           -LogName "pack-build.log"
if ($BuildPack.code -ne 0) {
    throw "TPTI-1 pack build failed.`n$($BuildPack.tail)"
}
$PackBytes = [IO.File]::ReadAllBytes($Pack)
$PreTip = Get-Entry $PackBytes "gameplay/pre-tip"
$SourceMap = Get-Entry $PackBytes "system/source-map"
if ($PreTip.size -ne 5376) { throw "TPTI-1 directory size changed." }
$Payload = Get-EntryBytes $PackBytes $PreTip
if ((Get-Fnv32 $Payload) -ne "91FD7B32" -or
    [Text.Encoding]::ASCII.GetString($Payload, 0, 4) -ne "TPTI" -or
    [BitConverter]::ToUInt16($Payload, 4) -ne 1 -or
    [BitConverter]::ToUInt16($Payload, 6) -ne 256 -or
    [BitConverter]::ToUInt32($Payload, 8) -ne 5376 -or
    [BitConverter]::ToUInt16($Payload, 12) -ne 17 -or
    [BitConverter]::ToUInt16($Payload, 14) -ne 32 -or
    [BitConverter]::ToUInt32($Payload, 16) -ne 256 -or
    $Payload[20] -ne 0x15 -or $Payload[21] -ne 0x3B -or
    $Payload[22] -ne 0x7D -or $Payload[23] -ne 8) {
    throw "TPTI-1 canonical header changed."
}
if (@($Payload[185..255] | Where-Object { $_ -ne 0 }).Count -ne 0) {
    throw "TPTI-1 reserved header bytes are nonzero."
}
for ($Index = 0; $Index -lt 17; ++$Index) {
    $Record = 256 + $Index * 32
    if ([BitConverter]::ToUInt16($Payload, $Record) -ne $Index + 1 -or
        @($Payload[($Record + 28)..($Record + 31)] |
            Where-Object { $_ -ne 0 }).Count -ne 0 -or
        [BitConverter]::ToUInt64($Payload, $Record + 16) -eq 0) {
        throw "TPTI-1 source record $Index is malformed."
    }
}
$MapText = [Text.Encoding]::UTF8.GetString(
    (Get-EntryBytes $PackBytes $SourceMap))
$Map = $MapText | ConvertFrom-Json
$Mapped = @($Map.logical_entries | Where-Object id -eq "gameplay/pre-tip")
$ExpectedRoles = @(
    "blank-screen-descriptor","blank-screen-stream","blank-screen-palette",
    "presentation-screen-wait-helpers","matchup-sequence-and-team-text",
    "mode-and-versus-strings","mode-string-pointer-table",
    "character-to-tile-map","tipoff-closeup-entry",
    "tipoff-closeup-palettes","tipoff-closeup-control",
    "tipoff-closeup-timing","center-tip-object-setup",
    "center-tip-object-update","pregame-launch-bridge","live-handoff",
    "tipoff-orientation-select"
)
if ($Mapped.Count -ne 1 -or
    $Mapped[0].schema -ne "tecmo.gameplay-pre-tip/TPTI-1" -or
    @($Mapped[0].dependencies).Count -ne 5 -or
    (@($Mapped[0].sources.role) -join ",") -ne ($ExpectedRoles -join ",") -or
    @($Mapped[0].sources | Where-Object {
        $_.fingerprint_fnv1a32 -notmatch "^[0-9A-F]{8}$" -or
        $_.fingerprint_fnv1a64 -notmatch "^[0-9A-F]{16}$"
    }).Count -ne 0 -or
    $Mapped[0].native_contract.music -notmatch "track 8" -or
    $Mapped[0].native_contract.cancel -notmatch "NES B" -or
    $Mapped[0].native_contract.toss_cut_in -notmatch "nametable page 1" -or
    $Mapped[0].native_contract.ball_descent -notmatch "71..145") {
    throw "TPTI-1 source-map provenance is incomplete or malformed."
}

$Self = Invoke-Native -Arguments @("--gameplay-pretip-test", $Pack) `
                      -LogName "self-test.log"
if ($Self.code -ne 0 -or $Self.text -notmatch "self-test passed") {
    throw "TPTI-1 self-test failed.`n$($Self.tail)"
}
$Scene = Invoke-Native -Arguments @("--gameplay-scene-test", $Pack) `
                       -LogName "scene-test.log"
if ($Scene.code -ne 0 -or $Scene.text -notmatch "SELF TEST PASS") {
    throw "TPTI-1 scene integration failed.`n$($Scene.tail)"
}

$env:TECMO_ASSETPACK = $Pack
$Modes = @(
    [pscustomobject]@{ mode="gameplay-start"; phase="preseason"; frame=0; hash="F7D88436B94D9946CCB90FAA40460B0DA5D97EF3CFFFB9ADEE4D992686E20A07" },
    [pscustomobject]@{ mode="gameplay-pretip-frame61"; phase="matchup"; frame=61; hash="0C9C5182A4E89DDDA82D255847D7EC1CABB2F50692129320A7B40CAD9D3CD91A" },
    [pscustomobject]@{ mode="gameplay-pretip-frame182"; phase="first-period"; frame=182; hash="2377B0FF24274E21F5963CC35E43D0F666B7626E890A23C01A7621B842055F9A" },
    [pscustomobject]@{ mode="gameplay-pretip-frame271"; phase="closeup"; frame=271; hash="2219FDA9C5AE15A0CBC5441FB11800DBF0C8195C18BF672E1F74D31AA0D502AB" },
    [pscustomobject]@{ mode="gameplay-pretip-frame481"; phase="ball-descent"; frame=481; hash="55D7AA6E15B95182992067230D2FFF7EF8453C51F50E879C5FE7D9BA5EC6461B" },
    [pscustomobject]@{ mode="gameplay-pretip-frame631"; phase="toss-closeup"; frame=631; hash="CDE4C17159C79207CA82281204547FD2794E81858A52A6FB312E937CEEDF162C" },
    [pscustomobject]@{ mode="gameplay-pretip-frame661"; phase="jump-contest"; frame=661; hash="0718185E3D996990F52DE481F9C99F9A8B68845FB837B5EAFEC90DBC390812AD" },
    [pscustomobject]@{ mode="gameplay-live-start"; phase="live"; frame=691; hash="5C3F0F756B52895D3F30CB82A94F2C1CB3BD7FA618CE6C8684EE2E802CB730D0" }
)
foreach ($Spec in $Modes) {
    $Hashes = @()
    for ($Pass = 0; $Pass -lt 2; ++$Pass) {
        $Png = Join-Path $Scratch "$($Spec.mode)-$Pass.png"
        $Run = Invoke-Native -Arguments @(
            "--root", $ProjectRoot, "--render-test-mode", $Spec.mode, $Png
        ) -LogName "$($Spec.mode)-$Pass.log"
        if ($Run.code -ne 0 -or !(Test-Path -LiteralPath $Png) -or
            $Run.text -notmatch "frame=$($Spec.frame).*clock=3:00.*pretip=$($Spec.phase)") {
            throw "TPTI-1 render checkpoint '$($Spec.mode)' failed.`n$($Run.tail)"
        }
        $Hashes += (Get-FileHash -LiteralPath $Png -Algorithm SHA256).Hash
    }
    if ($Hashes[0] -ne $Hashes[1]) {
        throw "TPTI-1 render checkpoint '$($Spec.mode)' is nondeterministic."
    }
    if ($Hashes[0] -ne $Spec.hash) {
        throw "TPTI-1 render checkpoint '$($Spec.mode)' changed: $($Hashes[0])."
    }
}

$Cases = @(
    [pscustomobject]@{ label="payload-mutation"; mutate={
        param($Bytes) $Bytes[[int]$PreTip.pack_offset + 2944] =
            $Bytes[[int]$PreTip.pack_offset + 2944] -bxor 1
    }},
    [pscustomobject]@{ label="reserved-byte"; mutate={
        param($Bytes) $Bytes[[int]$PreTip.pack_offset + 185] = 1
    }},
    [pscustomobject]@{ label="oversized-entry"; mutate={
        param($Bytes) [BitConverter]::GetBytes([uint64]5377).CopyTo(
            $Bytes, [int]$PreTip.directory_offset + 92)
    }},
    [pscustomobject]@{ label="missing-entry"; mutate={
        param($Bytes) $Name = [Text.Encoding]::ASCII.GetBytes("gameplay/pre-missing")
        [Array]::Copy($Name, 0, $Bytes, [int]$PreTip.directory_offset,
                      $Name.Length)
    }},
    [pscustomobject]@{ label="cross-pack-core"; mutate={
        param($Bytes) $Core = Get-Entry $Bytes "gameplay/core"
        $Bytes[[int]$Core.pack_offset] = $Bytes[[int]$Core.pack_offset] -bxor 1
    }},
    [pscustomobject]@{ label="cross-pack-team-data"; mutate={
        param($Bytes) $Team = Get-Entry $Bytes "menu/team-data"
        $Bytes[[int]$Team.pack_offset] = $Bytes[[int]$Team.pack_offset] -bxor 1
    }},
    [pscustomobject]@{ label="cross-pack-music"; mutate={
        param($Bytes) $Music = Get-Entry $Bytes "audio/music"
        $Bytes[[int]$Music.pack_offset] = $Bytes[[int]$Music.pack_offset] -bxor 1
    }},
    [pscustomobject]@{ label="cross-pack-closeup"; mutate={
        param($Bytes) $Closeup = Get-Entry $Bytes "arena/intro/warriors-transition"
        $Bytes[[int]$Closeup.pack_offset] =
            $Bytes[[int]$Closeup.pack_offset] -bxor 1
    }},
    [pscustomobject]@{ label="cross-pack-chr"; mutate={
        param($Bytes) $Chr = Get-Entry $Bytes "chr/all"
        $Bytes[[int]$Chr.pack_offset] = $Bytes[[int]$Chr.pack_offset] -bxor 1
    }}
)
foreach ($Case in $Cases) {
    $Bytes = [byte[]]$PackBytes.Clone()
    & $Case.mutate $Bytes
    $Path = Join-Path $Scratch "$($Case.label).assetpack"
    [IO.File]::WriteAllBytes($Path, $Bytes)
    Assert-Rejected -Pack $Path -Label $Case.label
}

$MutatedRom = Join-Path $Scratch "mutated-rev1.nes"
$RomBytes = [IO.File]::ReadAllBytes($RomPath)
$A125Offset = 16 + 6 * 0x4000 + (0xA125 - 0x8000)
$RomBytes[$A125Offset] = $RomBytes[$A125Offset] -bxor 1
[IO.File]::WriteAllBytes($MutatedRom, $RomBytes)
$MutatedPack = Join-Path $Scratch "mutated.assetpack"
$Mutation = Invoke-Native -Arguments @(
    "--build-assetpack", $MutatedRom, $MutatedPack
) -LogName "rom-mutation.log"
if ($Mutation.code -eq 0 -or $Mutation.text -notmatch "TPTI-1|Rev1") {
    throw "TPTI-1 Rev1 source mutation was accepted."
}

$global:LASTEXITCODE = 0
Write-Output ("TPTI-1 PRE-TIP TEST PASS: canonical/revision/FNV32+64/source-map " +
    "same-pack TGPL/TTDT/TMUS/TWAR/CHR missing/malformed/oversized/cross-pack " +
    "NES-B abort/freeze/track8-to-track5 scene integration and deterministic " +
    "preseason/matchup/closeup/toss/jump/live renders")
} finally {
    if ($null -eq $PreviousPack) {
        Remove-Item Env:TECMO_ASSETPACK -ErrorAction SilentlyContinue
    } else {
        $env:TECMO_ASSETPACK = $PreviousPack
    }
}
