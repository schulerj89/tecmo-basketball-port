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
    if ($Entry.byte_count -ne 1360 -or (Get-Fnv1a32 $Payload) -ne "B38C93F5" -or
        [Text.Encoding]::ASCII.GetString($Payload, 0, 4) -ne "TGCA" -or
        [BitConverter]::ToUInt16($Payload, 4) -ne 1 -or
        [BitConverter]::ToUInt32($Payload, 8) -ne 1360 -or
        [BitConverter]::ToUInt16($Payload, 12) -ne 8 -or
        [BitConverter]::ToUInt16($Payload, 14) -ne 32 -or
        [BitConverter]::ToUInt32($Payload, 16) -ne 128 -or
        [BitConverter]::ToUInt32($Payload, 20) -ne 384 -or
        [BitConverter]::ToUInt32($Payload, 24) -ne 969 -or
        ("{0:X8}" -f [BitConverter]::ToUInt32($Payload, 28)) -ne
            "CE60861F") {
        throw "TGCA-1 canonical payload contract changed."
    }
    $ExpectedSpans = @(
        @{ name="distance-helper"; bank=5; fixed_bank=$false; cpu_start=0x9DF6; bytes=110; f32="BE56D3D7" },
        @{ name="caller-and-assignment"; bank=5; fixed_bank=$false; cpu_start=0x9F2F; bytes=430; f32="13F3A41B" },
        @{ name="object-state10"; bank=5; fixed_bank=$false; cpu_start=0xB6E5; bytes=144; f32="6AD67C6A" },
        @{ name="object-state17"; bank=5; fixed_bank=$false; cpu_start=0xB775; bytes=56; f32="2E13AB9D" },
        @{ name="action-dispatch"; bank=7; fixed_bank=$true; cpu_start=0xC711; bytes=43; f32="AF434105" },
        @{ name="action-selector"; bank=7; fixed_bank=$true; cpu_start=0xCAF5; bytes=62; f32="798F7231" },
        @{ name="action-table-low"; bank=7; fixed_bank=$true; cpu_start=0xCB33; bytes=62; f32="CB4B3C42" },
        @{ name="action-table-high"; bank=7; fixed_bank=$true; cpu_start=0xCB71; bytes=62; f32="CD228EDD" }
    )
    for ($Index = 0; $Index -lt $ExpectedSpans.Count; ++$Index) {
        $Span = $ExpectedSpans[$Index]
        $Record = 128 + $Index * 32
        $PayloadOffset = [BitConverter]::ToUInt32($Payload, $Record + 12)
        if ([BitConverter]::ToUInt16($Payload, $Record) -ne ($Index + 1) -or
            $Payload[$Record + 2] -ne $Span.bank -or
            ($Payload[$Record + 3] -ne 0) -ne $Span.fixed_bank -or
            [BitConverter]::ToUInt16($Payload, $Record + 4) -ne $Span.cpu_start -or
            [BitConverter]::ToUInt32($Payload, $Record + 8) -ne $Span.bytes -or
            ("{0:X8}" -f [BitConverter]::ToUInt32($Payload, $Record + 16)) -ne
                $Span.f32 -or
            $PayloadOffset -lt 384 -or
            $PayloadOffset + $Span.bytes -gt 1353) {
            throw "TGCA-1 descriptor $Index is malformed."
        }
    }
    $ProvenancePath = Join-Path $ProjectRoot "docs\a023-actor-command-assignment-provenance.json"
    $Provenance = Get-Content -LiteralPath $ProvenancePath -Raw | ConvertFrom-Json
    if ($Provenance.schema -ne "tecmo.actor-command-assignment-provenance/TGCA-1" -or
        @($Provenance.authoritative_spans).Count -ne 8 -or
        $Provenance.production_proof_contract.expected.emitted -or
        $Provenance.production_proof_contract.expected.production_mutated -or
        $Provenance.fixture_boundary.resolver_inputs -notmatch "Synthetic") {
        throw "TGCA-1 machine-readable provenance is incomplete."
    }
    Invoke-Tgca $PackPath $true

    foreach ($Mutation in @(
        @{ name="payload-magic"; offset=0 },
        @{ name="payload-header-reserved"; offset=72 },
        @{ name="payload-descriptor"; offset=144 },
        @{ name="payload-raw-first"; offset=384 },
        @{ name="payload-raw-middle"; offset=800 },
        @{ name="payload-padding"; offset=1353 }
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
    foreach ($Size in @(1359, 1361)) {
        $Sized = [byte[]]$PackBytes.Clone()
        [BitConverter]::GetBytes([uint64]$Size).CopyTo(
            $Sized, [int]$Entry.directory_offset + 92)
        $SizedPath = Join-Path $Scratch ("size-" + $Size + ".assetpack")
        [IO.File]::WriteAllBytes($SizedPath, $Sized)
        Invoke-Tgca $SizedPath $false
    }

    Write-Host (
        "TGCA-1 tests passed: exact 8-span Rev1 fingerprints, independent " +
        "raw-ROM rejection for every authoritative span, parser copied-byte/" +
        "payload rejection, fixture-only A023 resolver gates, descending tie, " +
        "selected exclusions, synthetic no-candidate, both-side control modes, " +
        "and transactional rollback.")
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
