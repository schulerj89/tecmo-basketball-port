param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [switch]$Build
)

$ErrorActionPreference = "Stop"
$ExpectedRomSha256 =
    "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4"

if (!$ProjectRoot) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if (!$RomPath) { $RomPath = $env:TECMO_ROM_PATH }
if (!$RomPath -or !(Test-Path -LiteralPath $RomPath -PathType Leaf)) {
    throw "Pass -RomPath or set TECMO_ROM_PATH to the local Rev1 iNES ROM."
}
$RomPath = (Resolve-Path -LiteralPath $RomPath).Path
if ((Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash -ne
    $ExpectedRomSha256) {
    throw "TGVR-1 tests require the exact Tecmo NBA Basketball Rev1 ROM."
}

$BuildDir = Join-Path $ProjectRoot "build"
$Executable = Join-Path $BuildDir "tecmo_port.exe"
$Scratch = [IO.Path]::GetFullPath(
    (Join-Path $BuildDir "gameplay_violation_referee_test"))
$BuildPrefix = [IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/') +
    [IO.Path]::DirectorySeparatorChar
if (!$Scratch.StartsWith($BuildPrefix,
                         [StringComparison]::OrdinalIgnoreCase)) {
    throw "Violation-referee scratch path escaped build\."
}
$PackPath = Join-Path $Scratch "violation-referee.assetpack"
$PreviousPack = $env:TECMO_ASSETPACK
$PreviousSkipShortcut = $env:TECMO_SKIP_SHORTCUT

function Get-ShortTail {
    param([object[]]$Lines)
    return (@($Lines | Select-Object -Last 12) -join
        [Environment]::NewLine)
}

function Get-Fnv1a32 {
    param([byte[]]$Bytes)
    [uint32]$Hash = 2166136261
    foreach ($Byte in $Bytes) {
        [uint64]$Product = [uint64]($Hash -bxor [uint32]$Byte) *
            [uint64]16777619
        $Hash = [uint32]($Product % [uint64]4294967296)
    }
    return ("{0:X8}" -f $Hash)
}

function Get-AssetPackEntry {
    param([byte[]]$Bytes, [string]$Id)
    if ($Bytes.Length -lt 40 -or
        [Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -ne "TAP1" -or
        [BitConverter]::ToUInt32($Bytes, 4) -ne 1 -or
        [BitConverter]::ToUInt32($Bytes, 8) -ne 40 -or
        [BitConverter]::ToUInt32($Bytes, 12) -ne 128) {
        throw "Asset pack header is not TAP1 v1."
    }
    $EntryCount = [BitConverter]::ToUInt32($Bytes, 16)
    $DirectoryOffset = [BitConverter]::ToUInt64($Bytes, 20)
    if ($DirectoryOffset -gt [uint64]$Bytes.Length -or
        [uint64]$EntryCount * 128 -gt
            [uint64]$Bytes.Length - $DirectoryOffset) {
        throw "Asset pack directory is out of bounds."
    }
    for ($Index = 0; $Index -lt $EntryCount; ++$Index) {
        $Offset = [int]$DirectoryOffset + $Index * 128
        $Terminator = [Array]::IndexOf($Bytes, [byte]0, $Offset, 64)
        if ($Terminator -lt 0) { $Terminator = $Offset + 64 }
        $EntryId = [Text.Encoding]::ASCII.GetString(
            $Bytes, $Offset, $Terminator - $Offset)
        if ($EntryId -ne $Id) { continue }
        $PackOffset = [BitConverter]::ToUInt64($Bytes, $Offset + 84)
        $ByteCount = [BitConverter]::ToUInt64($Bytes, $Offset + 92)
        if ($PackOffset -gt [uint64]$Bytes.Length -or
            $ByteCount -gt [uint64]$Bytes.Length - $PackOffset) {
            throw "Asset pack entry '$Id' is out of bounds."
        }
        return [pscustomobject]@{
            directory_offset = $Offset
            pack_offset = $PackOffset
            byte_count = $ByteCount
        }
    }
    throw "Asset pack entry '$Id' was not found."
}

function Get-EntryBytes {
    param([byte[]]$PackBytes, [object]$Entry)
    $Result = New-Object byte[] ([int]$Entry.byte_count)
    [Array]::Copy($PackBytes, [int]$Entry.pack_offset,
                  $Result, 0, $Result.Length)
    return $Result
}

function Invoke-RefereeTest {
    param([string]$AssetPack, [bool]$ExpectSuccess,
          [string]$ExpectedFailure = "")
    $Output = @(& $Executable --gameplay-violation-referee-test `
        $AssetPack 2>&1)
    $Text = ($Output -join [Environment]::NewLine).Trim()
    if ($ExpectSuccess) {
        if ($LASTEXITCODE -ne 0 -or $Text -ne
            "TGVR-1 native violation referee self-test passed") {
            throw "TGVR-1 loader golden failed.`n$(Get-ShortTail $Output)"
        }
    } elseif ($LASTEXITCODE -eq 0 -or $Text -notmatch "TGVR-1" -or
              ($ExpectedFailure -and $Text -notmatch $ExpectedFailure)) {
        throw "Malformed TGVR-1 fixture was accepted.`n$(Get-ShortTail $Output)"
    }
}

function Write-MutatedPayloadAndReject {
    param([byte[]]$Original, [object]$Entry,
          [string]$Id, [int]$PayloadOffset)
    $Path = Join-Path $Scratch ("payload-" + $Id + ".assetpack")
    $Bytes = [byte[]]$Original.Clone()
    $Absolute = [int]$Entry.pack_offset + $PayloadOffset
    $Bytes[$Absolute] = $Bytes[$Absolute] -bxor 1
    [IO.File]::WriteAllBytes($Path, $Bytes)
    Invoke-RefereeTest $Path $false
}

function Invoke-Render {
    param([int]$Frame)
    $Mode = "gameplay-shot-clock-violation-frame$Frame"
    $Hashes = @()
    for ($Pass = 0; $Pass -lt 2; ++$Pass) {
        $Png = Join-Path $Scratch ("shot-clock-$Frame-$Pass.png")
        $Output = @(& $Executable --root $ProjectRoot `
            --render-test-mode $Mode $Png 2>&1)
        $Text = $Output -join [Environment]::NewLine
        if ($LASTEXITCODE -ne 0 -or
            !(Test-Path -LiteralPath $Png -PathType Leaf) -or
            $Text -notmatch "phase=violation-presentation" -or
            $Text -notmatch "phase-frame=$Frame violation=SHOT CLOCK") {
            throw "TGVR-1 render frame $Frame failed.`n$(Get-ShortTail $Output)"
        }
        $Hashes += (Get-FileHash -LiteralPath $Png -Algorithm SHA256).Hash
    }
    if ($Hashes[0] -ne $Hashes[1]) {
        throw "TGVR-1 render frame $Frame was nondeterministic."
    }
    return $Hashes[0]
}

try {
    $env:TECMO_SKIP_SHORTCUT = "1"
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
    if ($Build) {
        $BuildOutput = @(& (Join-Path $ProjectRoot "build.ps1") 2>&1)
        if ($LASTEXITCODE -ne 0 -or
            @($BuildOutput | Where-Object {
                $_ -match 'warning [A-Z]+[0-9]+:'
            }).Count -ne 0) {
            throw "Warning-free TGVR-1 build failed.`n$(Get-ShortTail $BuildOutput)"
        }
    }
    if (!(Test-Path -LiteralPath $Executable -PathType Leaf)) {
        throw "Build output is missing; rerun with -Build."
    }
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
    New-Item -ItemType Directory -Path $Scratch | Out-Null

    $PackOutput = @(& $Executable --build-assetpack `
        $RomPath $PackPath 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "Rev1 TGVR-1 asset-pack build failed.`n$(Get-ShortTail $PackOutput)"
    }
    $SourceOutput = @(& $Executable --gameplay-violation-referee-test `
        $PackPath $RomPath 2>&1)
    if ($LASTEXITCODE -ne 0 -or
        ($SourceOutput -join [Environment]::NewLine).Trim() -ne
            "TGVR-1 native violation referee self-test passed") {
        throw "TGVR-1 source/loader golden failed.`n$(Get-ShortTail $SourceOutput)"
    }

    $PackBytes = [IO.File]::ReadAllBytes($PackPath)
    $RefEntry = Get-AssetPackEntry $PackBytes "gameplay/violation-referee"
    $ChrEntry = Get-AssetPackEntry $PackBytes "chr/all"
    $PenaltyEntry = Get-AssetPackEntry $PackBytes "gameplay/penalties"
    $SourceMapEntry = Get-AssetPackEntry $PackBytes "system/source-map"
    $Payload = Get-EntryBytes $PackBytes $RefEntry
    if ($RefEntry.byte_count -ne 4752 -or
        (Get-Fnv1a32 $Payload) -ne "2EB08CF0" -or
        [Text.Encoding]::ASCII.GetString($Payload, 0, 4) -ne "TGVR" -or
        [BitConverter]::ToUInt16($Payload, 4) -ne 1 -or
        [BitConverter]::ToUInt32($Payload, 8) -ne 4752 -or
        [BitConverter]::ToUInt32($Payload, 84) -ne
            [uint32]3705316404 -or
        [BitConverter]::ToUInt32($Payload, 104) -ne
            [uint32]3910811095 -or
        [BitConverter]::ToUInt32($Payload, 108) -ne
            [uint32]3915611831 -or
        [BitConverter]::ToUInt32($Payload, 112) -ne
            [uint32]4119138476) {
        throw "TGVR-1 canonical payload contract changed."
    }

    $SourceMap = ([Text.Encoding]::UTF8.GetString(
        (Get-EntryBytes $PackBytes $SourceMapEntry))) | ConvertFrom-Json
    $Maps = @($SourceMap.logical_entries | Where-Object {
        $_.id -eq "gameplay/violation-referee"
    })
    if ($Maps.Count -ne 1 -or
        $Maps[0].schema -ne
            "tecmo.gameplay-violation-referee/TGVR-1" -or
        $Maps[0].fingerprint_fnv1a32 -ne "2EB08CF0" -or
        @($Maps[0].source_spans).Count -ne 10 -or
        (@($Maps[0].native_contract.shot_clock_groups) -join ',') -ne
            '9,10,10,10' -or
        $Maps[0].native_contract.message -ne "SHOT CLOCK VIOLATION" -or
        $Maps[0].capture_bounded_alignment.black_frames -ne 9 -or
        $Maps[0].capture_bounded_alignment.sequence_visible_start_frame -ne
            23) {
        throw "TGVR-1 source-map provenance is incomplete or malformed."
    }

    foreach ($Mutation in @(
        @{ id="magic"; offset=0 },
        @{ id="version"; offset=4 },
        @{ id="declared-size"; offset=8 },
        @{ id="revision"; offset=148 },
        @{ id="header-reserved"; offset=180 },
        @{ id="source-record"; offset=256 },
        @{ id="raw-controller"; offset=1203 },
        @{ id="raw-padding"; offset=3205 },
        @{ id="decoded-screen"; offset=3216 },
        @{ id="message"; offset=4240 },
        @{ id="message-padding"; offset=4436 },
        @{ id="sequence"; offset=4448 },
        @{ id="sequence-padding"; offset=4508 },
        @{ id="group"; offset=4512 },
        @{ id="group-reserved"; offset=4524 }
    )) {
        Write-MutatedPayloadAndReject $PackBytes $RefEntry `
            $Mutation.id $Mutation.offset
    }

    foreach ($Case in @(
        @{ id="undersized"; size=4751 },
        @{ id="oversized"; size=4753 }
    )) {
        $Path = Join-Path $Scratch ($Case.id + ".assetpack")
        $Bytes = [byte[]]$PackBytes.Clone()
        [BitConverter]::GetBytes([uint64]$Case.size).CopyTo(
            $Bytes, [int]$RefEntry.directory_offset + 92)
        [IO.File]::WriteAllBytes($Path, $Bytes)
        Invoke-RefereeTest $Path $false "missing or wrong-sized"
    }
    foreach ($Case in @(
        @{ id="missing-referee"; entry=$RefEntry },
        @{ id="missing-chr"; entry=$ChrEntry },
        @{ id="missing-penalties"; entry=$PenaltyEntry }
    )) {
        $Path = Join-Path $Scratch ($Case.id + ".assetpack")
        $Bytes = [byte[]]$PackBytes.Clone()
        $Bytes[[int]$Case.entry.directory_offset] = [byte][char]'x'
        [IO.File]::WriteAllBytes($Path, $Bytes)
        Invoke-RefereeTest $Path $false
    }
    foreach ($Case in @(
        @{ id="cross-pack-chr"; entry=$ChrEntry; offset=0 },
        @{ id="cross-pack-penalties"; entry=$PenaltyEntry; offset=0 }
    )) {
        $Path = Join-Path $Scratch ($Case.id + ".assetpack")
        $Bytes = [byte[]]$PackBytes.Clone()
        $Absolute = [int]$Case.entry.pack_offset + $Case.offset
        $Bytes[$Absolute] = $Bytes[$Absolute] -bxor 1
        [IO.File]::WriteAllBytes($Path, $Bytes)
        Invoke-RefereeTest $Path $false "dependenc"
    }

    $MutatedRomPath = Join-Path $Scratch "mutated-referee-source.nes"
    $MutatedRom = [IO.File]::ReadAllBytes($RomPath)
    $MutatedRom[16 + 4 * 0x4000 + (0xBA1F - 0x8000)] =
        $MutatedRom[16 + 4 * 0x4000 + (0xBA1F - 0x8000)] -bxor 1
    [IO.File]::WriteAllBytes($MutatedRomPath, $MutatedRom)
    $MutationOutput = @(& $Executable --gameplay-violation-referee-test `
        $PackPath $MutatedRomPath 2>&1)
    if ($LASTEXITCODE -eq 0 -or
        ($MutationOutput -join [Environment]::NewLine) -notmatch
            "TGVR-1 import requires the exact Rev1 ROM fingerprint") {
        throw "TGVR-1 direct importer accepted a mutated controller source."
    }

    $env:TECMO_ASSETPACK = $PackPath
    $RenderHashes = @{}
    foreach ($Frame in @(0, 9, 23, 27, 80)) {
        $RenderHashes[$Frame] = Invoke-Render $Frame
    }
    if ($RenderHashes[23] -eq $RenderHashes[27] -or
        $RenderHashes[9] -eq $RenderHashes[23] -or
        $RenderHashes[0] -eq $RenderHashes[9]) {
        throw "TGVR-1 blackout/fade/group-9/group-10 visuals collapsed together."
    }

    Write-Host (
        "TGVR-1 focused tests passed: exact screen 05, ROM text, " +
        "shot-clock groups 9->10, 168-frame settlement, deterministic " +
        "render checkpoints, strict provenance and fail-closed dependencies")
    $global:LASTEXITCODE = 0
} finally {
    $env:TECMO_ASSETPACK = $PreviousPack
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
}
