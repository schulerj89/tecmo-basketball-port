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
    throw "TGCA-1 tests require the exact Tecmo NBA Basketball Rev1 ROM."
}

$BuildDir = [IO.Path]::GetFullPath((Join-Path $ProjectRoot "build"))
$BuildPrefix = $BuildDir.TrimEnd('\', '/') +
    [IO.Path]::DirectorySeparatorChar
$Scratch = [IO.Path]::GetFullPath(
    (Join-Path $BuildDir "gameplay_actor_command_assignment_test"))
if (!$Scratch.StartsWith($BuildPrefix,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw "TGCA-1 scratch path escaped the project build directory."
}
$Executable = Join-Path $BuildDir "tecmo_port.exe"
$PackPath = Join-Path $Scratch "actor-command-assignment.assetpack"
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
    $Count = [BitConverter]::ToUInt32($Bytes, 16)
    $Directory = [BitConverter]::ToUInt64($Bytes, 20)
    if ($Directory -gt [uint64]$Bytes.Length -or
        [uint64]$Count * 128 -gt [uint64]$Bytes.Length - $Directory) {
        throw "Asset pack directory is out of bounds."
    }
    for ($Index = 0; $Index -lt $Count; ++$Index) {
        $Offset = [int]$Directory + $Index * 128
        $End = [Array]::IndexOf($Bytes, [byte]0, $Offset, 64)
        if ($End -lt 0) { $End = $Offset + 64 }
        if ([Text.Encoding]::ASCII.GetString(
                $Bytes, $Offset, $End - $Offset) -ne $Id) {
            continue
        }
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
    [Array]::Copy($PackBytes, [int64]$Entry.pack_offset,
                  $Result, 0, $Result.Length)
    return $Result
}

function Invoke-Tgca {
    param([string]$AssetPack, [bool]$ExpectSuccess,
          [string]$ExpectedFailure = "")
    $Output = @(& $Executable --gameplay-actor-command-assignment-test `
        $AssetPack 2>&1)
    $Text = ($Output -join [Environment]::NewLine).Trim()
    if ($ExpectSuccess) {
        if ($LASTEXITCODE -ne 0 -or $Text -ne
            "TGCA-1 focused command-assignment contract passed.") {
            throw "Valid TGCA-1 pack failed.`n$(Get-ShortTail $Output)"
        }
    } else {
        if ($LASTEXITCODE -eq 0) {
            throw "Malformed TGCA-1 pack was accepted: $AssetPack"
        }
        if ($Text -notmatch "Actor command assignment test failed") {
            throw "TGCA-1 rejection used the wrong command path for $AssetPack.`n$(Get-ShortTail $Output)"
        }
        if ($ExpectedFailure -and
            $Text -notmatch [regex]::Escape($ExpectedFailure)) {
            throw "TGCA-1 rejected $AssetPack but lost expected '$ExpectedFailure'.`n$(Get-ShortTail $Output)"
        }
    }
}

function Write-PayloadMutationAndReject {
    param([byte[]]$Original, [object]$Entry, [string]$Name,
          [int]$PayloadOffset)
    $Bytes = [byte[]]$Original.Clone()
    $Absolute = [int]$Entry.pack_offset + $PayloadOffset
    if ($Absolute -lt [int]$Entry.pack_offset -or
        $Absolute -ge [int]$Entry.pack_offset + [int]$Entry.byte_count) {
        throw "TGCA-1 payload mutation '$Name' escaped the entry."
    }
    $Bytes[$Absolute] = $Bytes[$Absolute] -bxor 1
    $Path = Join-Path $Scratch ("payload-" + $Name + ".assetpack")
    [IO.File]::WriteAllBytes($Path, $Bytes)
    Invoke-Tgca $Path $false "TGCA-1"
}

function Invoke-RawSpanMutationAndReject {
    param([object]$Span)
    $Base = if ($Span.fixed_bank) { 0xC000 } else { 0x8000 }
    $Offset = 16 + [int]$Span.bank * 0x4000 +
        ([int]$Span.cpu_start - $Base)
    $Original = [IO.File]::ReadAllBytes($RomPath)
    if ($Offset -lt 16 -or $Offset -ge $Original.Length) {
        throw "TGCA-1 source span '$($Span.name)' mapped outside the ROM."
    }
    $Original[$Offset] = $Original[$Offset] -bxor 1
    $Path = Join-Path $Scratch ("rom-" + $Span.name + ".nes")
    [IO.File]::WriteAllBytes($Path, $Original)
    $Output = @(& $Executable --gameplay-actor-command-assignment-test `
        $PackPath $Path 2>&1)
    $Text = $Output -join [Environment]::NewLine
    if ($LASTEXITCODE -eq 0 -or
        $Text -notmatch "Actor command assignment source test failed" -or
        $Text -notmatch "TGCA-1 import requires the exact Rev1 ROM fingerprint") {
        throw ("TGCA-1 importer accepted raw ROM mutation for " +
            "'$($Span.name)' at ROM offset 0x{0:X}.`n{1}" -f
            $Offset, (Get-ShortTail $Output))
    }
}

try {
    $env:TECMO_SKIP_SHORTCUT = "1"
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
    if ($Build) {
        $BuildOutput = @(& (Join-Path $ProjectRoot "build.ps1") 2>&1)
        if ($LASTEXITCODE -ne 0 -or @($BuildOutput | Where-Object {
                $_ -match 'warning [A-Z]+[0-9]+:'
            }).Count -ne 0) {
            throw "Warning-clean TGCA-1 build failed.`n$(Get-ShortTail $BuildOutput)"
        }
    }
    if (!(Test-Path -LiteralPath $Executable -PathType Leaf)) {
        throw "Build output is missing; rerun with -Build."
    }
    if (Test-Path -LiteralPath $Scratch) {
        $ResolvedScratch = (Resolve-Path -LiteralPath $Scratch).Path
        if (!$ResolvedScratch.StartsWith($BuildPrefix,
                [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to clear a TGCA-1 scratch path outside build\."
        }
        Remove-Item -LiteralPath $ResolvedScratch -Recurse -Force
    }
    New-Item -ItemType Directory -Path $Scratch | Out-Null

    $PackOutput = @(& $Executable --build-assetpack $RomPath $PackPath 2>&1)
    if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $PackPath)) {
        throw "TGCA-1 asset-pack build failed.`n$(Get-ShortTail $PackOutput)"
    }
    $SourceOutput = @(& $Executable --gameplay-actor-command-assignment-test `
        $PackPath $RomPath 2>&1)
    if ($LASTEXITCODE -ne 0 -or
        ($SourceOutput -join [Environment]::NewLine).Trim() -ne
            "TGCA-1 focused command-assignment contract passed.") {
        throw "TGCA-1 source/parser/resolver vectors failed.`n$(Get-ShortTail $SourceOutput)"
    }

    $PackBytes = [IO.File]::ReadAllBytes($PackPath)
    $Entry = Get-AssetPackEntry $PackBytes "gameplay/actor-command-assignment"
    $Payload = Get-EntryBytes $PackBytes $Entry
    if ($Entry.byte_count -ne 1488 -or (Get-Fnv1a32 $Payload) -ne "4C7C2B34" -or
        [Text.Encoding]::ASCII.GetString($Payload, 0, 4) -ne "TGCA" -or
        [BitConverter]::ToUInt16($Payload, 4) -ne 1 -or
        [BitConverter]::ToUInt32($Payload, 8) -ne 1488 -or
        [BitConverter]::ToUInt16($Payload, 12) -ne 9 -or
        [BitConverter]::ToUInt16($Payload, 14) -ne 32 -or
        [BitConverter]::ToUInt32($Payload, 16) -ne 128 -or
        [BitConverter]::ToUInt32($Payload, 20) -ne 416 -or
        [BitConverter]::ToUInt32($Payload, 24) -ne 1064 -or
        ("{0:X8}" -f [BitConverter]::ToUInt32($Payload, 28)) -ne
            "741A149E" -or
        (([BitConverter]::ToString([byte[]]$Payload[40..71])) -replace '-', '') -ne
            $ExpectedRomSha256) {
        throw "TGCA-1 canonical payload contract changed."
    }
    $ExpectedSpans = @(
        @{ name="distance-helper"; bank=5; fixed_bank=$false; cpu_start=0x9DF6; bytes=110; payload_offset=416; f32="BE56D3D7"; f64="BF24D84A61C2F497" },
        @{ name="caller-and-assignment"; bank=5; fixed_bank=$false; cpu_start=0x9F2F; bytes=430; payload_offset=526; f32="13F3A41B"; f64="0C3DED79B8BCFADB" },
        @{ name="object-dispatch"; bank=5; fixed_bank=$false; cpu_start=0xA214; bytes=75; payload_offset=956; f32="4FC82BF8"; f64="84059724738A3C78" },
        @{ name="object-state10"; bank=5; fixed_bank=$false; cpu_start=0xB6E5; bytes=144; payload_offset=1031; f32="6AD67C6A"; f64="C4DDB2D7A58EF6AA" },
        @{ name="object-state17-18"; bank=5; fixed_bank=$false; cpu_start=0xB775; bytes=76; payload_offset=1175; f32="D90B723B"; f64="9EBF758818C65AFB" },
        @{ name="action-dispatch"; bank=7; fixed_bank=$true; cpu_start=0xC711; bytes=43; payload_offset=1251; f32="AF434105"; f64="453F5F11B98924E5" },
        @{ name="action-selector"; bank=7; fixed_bank=$true; cpu_start=0xCAF5; bytes=62; payload_offset=1294; f32="798F7231"; f64="B65EBECE5F5ECFB1" },
        @{ name="action-table-low"; bank=7; fixed_bank=$true; cpu_start=0xCB33; bytes=62; payload_offset=1356; f32="CB4B3C42"; f64="25A0A0DFEDDD4702" },
        @{ name="action-table-high"; bank=7; fixed_bank=$true; cpu_start=0xCB71; bytes=62; payload_offset=1418; f32="CD228EDD"; f64="F8DCBA38D20596DD" }
    )
    $RomBytes = [IO.File]::ReadAllBytes($RomPath)
    $B721Offset = 16 + 5 * 0x4000 + (0xB721 - 0x8000)
    $B783Offset = 16 + 5 * 0x4000 + (0xB783 - 0x8000)
    $LoopOffset = 16 + 6 * 0x4000 + (0x8284 - 0x8000)
    $MainLoopOffset = 16 + 7 * 0x4000 + (0xF031 - 0xC000)
    $B721 = ([BitConverter]::ToString(
        $RomBytes[$B721Offset..($B721Offset + 28)])) -replace '-', ''
    $B783 = ([BitConverter]::ToString(
        $RomBytes[$B783Offset..($B783Offset + 30)])) -replace '-', ''
    $Loop = ([BitConverter]::ToString(
        $RomBytes[$LoopOffset..($LoopOffset + 34)])) -replace '-', ''
    $MainLoop = ([BitConverter]::ToString(
        $RomBytes[$MainLoopOffset..($MainLoopOffset + 34)])) -replace '-', ''
    if ($B721 -ne
            "A5670568F017A57D8D8D03A5F28D8E03A5FD8D8F03A9008D90032023A0" -or
        $B783 -ne
            "A57D8D8D03A5F28D8E03A5FD8D8F03A9008D90032023A0AD880529DF8D8805" -or
        $Loop -ne
            "A209EC0803F019EC0903F0148A48BC7C05B9B68285A4B9C48285A520B38268AACA10DF" -or
        $MainLoop -ne
            "20F28120AD9720BAF0207EF02014A2206EE1205CF020B1F1A906206AD32039B12004B1") {
        throw "TGCA-1 B721/B783, fixed Bank05-before-Bank06, or 9..0 loop anchor changed."
    }
    for ($Index = 0; $Index -lt $ExpectedSpans.Count; ++$Index) {
        $Span = $ExpectedSpans[$Index]
        $Record = 128 + $Index * 32
        $PayloadOffset = [BitConverter]::ToUInt32($Payload, $Record + 12)
        if ([BitConverter]::ToUInt16($Payload, $Record) -ne ($Index + 1) -or
            $Payload[$Record + 2] -ne $Span.bank -or
            ($Payload[$Record + 3] -ne 0) -ne $Span.fixed_bank -or
            [BitConverter]::ToUInt16($Payload, $Record + 4) -ne $Span.cpu_start -or
            [BitConverter]::ToUInt32($Payload, $Record + 8) -ne $Span.bytes -or
            $PayloadOffset -ne $Span.payload_offset -or
            ("{0:X8}" -f [BitConverter]::ToUInt32($Payload, $Record + 16)) -ne
                $Span.f32 -or
            ("{0:X16}" -f [BitConverter]::ToUInt64($Payload, $Record + 20)) -ne
                $Span.f64 -or
            $PayloadOffset -lt 416 -or
            $PayloadOffset + $Span.bytes -gt 1480) {
            throw "TGCA-1 descriptor $Index is malformed."
        }
    }
    $SourceMapEntry = Get-AssetPackEntry $PackBytes "system/source-map"
    $SourceMap = ([Text.Encoding]::UTF8.GetString(
        (Get-EntryBytes $PackBytes $SourceMapEntry))) | ConvertFrom-Json
    $SourceMaps = @($SourceMap.logical_entries | Where-Object {
        $_.id -eq "gameplay/actor-command-assignment"
    })
    if ($SourceMaps.Count -ne 1 -or
        [string]$SourceMaps[0].schema -ne
            "tecmo.gameplay-actor-command-assignment/TGCA-1" -or
        [int]$SourceMaps[0].size -ne 1488 -or
        [string]$SourceMaps[0].fingerprint_fnv1a32 -ne "4C7C2B34" -or
        [bool]$SourceMaps[0].production_attachment -or
        @($SourceMaps[0].source_spans).Count -ne $ExpectedSpans.Count) {
        throw "TGCA-1 logical source-map entry is missing or malformed."
    }
    for ($Index = 0; $Index -lt $ExpectedSpans.Count; ++$Index) {
        $Span = $ExpectedSpans[$Index]
        $MapSpan = @($SourceMaps[0].source_spans)[$Index]
        $CpuBase = if ($Span.fixed_bank) { 0xC000 } else { 0x8000 }
        $ExpectedSourceOffset = [uint64]$SourceMap.source.prg_offset +
            [uint64]$Span.bank * [uint64]$SourceMap.source.prg_bank_bytes +
            [uint64]($Span.cpu_start - $CpuBase)
        $ExpectedSourceEntry = if ($Span.fixed_bank) {
            "prg/fixed"
        } else {
            "prg/bank05"
        }
        if ([uint64]$MapSpan.source_offset -ne $ExpectedSourceOffset -or
            [string]$MapSpan.source_entry -ne $ExpectedSourceEntry -or
            [int]$MapSpan.bank -ne $Span.bank -or
            [bool]$MapSpan.fixed_bank -ne $Span.fixed_bank -or
            [int]$MapSpan.cpu_start -ne $Span.cpu_start -or
            [int]$MapSpan.cpu_end -ne ($Span.cpu_start + $Span.bytes - 1) -or
            [int]$MapSpan.size -ne $Span.bytes -or
            [int]$MapSpan.payload_offset -ne $Span.payload_offset -or
            [string]$MapSpan.fingerprint_fnv1a32 -ne $Span.f32 -or
            [string]$MapSpan.fingerprint_fnv1a64 -ne $Span.f64) {
            throw "TGCA-1 source-map span $Index lost an exact range/hash."
        }
    }
    $ProvenancePath = Join-Path $ProjectRoot "docs\a023-actor-command-assignment-provenance.json"
    $Provenance = Get-Content -LiteralPath $ProvenancePath -Raw | ConvertFrom-Json
    if ($Provenance.schema -ne "tecmo.actor-command-assignment-provenance/TGCA-1" -or
        @($Provenance.authoritative_spans).Count -ne 9 -or
        $Provenance.production_proof_contract.expected.emitted -or
        $Provenance.production_proof_contract.expected.production_mutated -or
        $Provenance.fixture_boundary.resolver_inputs -notmatch "Synthetic") {
        throw "TGCA-1 machine-readable provenance is incomplete."
    }
    Invoke-Tgca $PackPath $true

    foreach ($Mutation in @(
        @{ name="payload-magic"; offset=0 },
        @{ name="payload-rom-sha"; offset=40 },
        @{ name="payload-header-reserved"; offset=72 },
        @{ name="payload-descriptor"; offset=144 },
        @{ name="payload-raw-first"; offset=416 },
        @{ name="payload-raw-middle"; offset=900 },
        @{ name="payload-padding"; offset=1480 }
    )) {
        Write-PayloadMutationAndReject $PackBytes $Entry $Mutation.name `
            $Mutation.offset
    }
    foreach ($Span in $ExpectedSpans) {
        Invoke-RawSpanMutationAndReject $Span
    }

    $Missing = [byte[]]$PackBytes.Clone()
    $Missing[[int]$Entry.directory_offset] = [byte][char]'x'
    $MissingPath = Join-Path $Scratch "missing.assetpack"
    [IO.File]::WriteAllBytes($MissingPath, $Missing)
    Invoke-Tgca $MissingPath $false
    foreach ($Size in @(1487, 1489)) {
        $Sized = [byte[]]$PackBytes.Clone()
        [BitConverter]::GetBytes([uint64]$Size).CopyTo(
            $Sized, [int]$Entry.directory_offset + 92)
        $SizedPath = Join-Path $Scratch ("size-" + $Size + ".assetpack")
        [IO.File]::WriteAllBytes($SizedPath, $Sized)
        Invoke-Tgca $SizedPath $false
    }

    Write-Host (
        "TGCA-1 tests passed: exact 9-span Rev1 fingerprints and source map; " +
        "whole-ROM importer rejection for nine targeted raw mutations; bounded " +
        "per-span FNV32/FNV64/descriptor branches; parser copied-byte/SHA/" +
        "payload rejection; unsigned depth distance; fixture-only A023 gates, " +
        "descending tie, selected exclusions, human/automatic modes, and " +
        "transactional rollback.")
    $global:LASTEXITCODE = 0
} finally {
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    if (Test-Path -LiteralPath $Scratch) {
        $ResolvedScratch = (Resolve-Path -LiteralPath $Scratch).Path
        if ($ResolvedScratch.StartsWith($BuildPrefix,
                [StringComparison]::OrdinalIgnoreCase)) {
            Remove-Item -LiteralPath $ResolvedScratch -Recurse -Force
        }
    }
}
