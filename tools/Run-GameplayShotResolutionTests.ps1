param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [switch]$Build
)

$ErrorActionPreference = "Stop"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if (!$RomPath) {
    $RomPath = $env:TECMO_ROM_PATH
}
if (!$RomPath -or !(Test-Path -LiteralPath $RomPath -PathType Leaf)) {
    throw "Pass -RomPath or set TECMO_ROM_PATH to the local Rev1 iNES ROM."
}
$RomPath = (Resolve-Path -LiteralPath $RomPath).Path
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $RomPath).Hash -ne
    "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4") {
    throw "TGSR-1 tests require the exact supported Rev1 ROM revision."
}

$BuildDir = Join-Path $ProjectRoot "build"
$Executable = Join-Path $BuildDir "tecmo_port.exe"
$Scratch = [IO.Path]::GetFullPath(
    (Join-Path $BuildDir "gameplay_shot_resolution_test"))
$BuildPrefix = [IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/') +
    [IO.Path]::DirectorySeparatorChar
if (!$Scratch.StartsWith($BuildPrefix,
                         [StringComparison]::OrdinalIgnoreCase)) {
    throw "Shot-resolution test scratch path escaped build\."
}
$PackPath = Join-Path $Scratch "shot-resolution.assetpack"
$ExpectedOutput =
    "TGSR-1 shot resolution passed: sources=4 polarity=clear:make,set:miss routes=A708/A7A9/A8E9/A708 claimant=bounded settlement=team-driven"
$PreviousSkipShortcut = $env:TECMO_SKIP_SHORTCUT

function Get-ShortTail {
    param([object[]]$Lines)
    return (@($Lines | Select-Object -Last 10) -join
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

function Get-Fnv1a64 {
    param([byte[]]$Bytes)
    [Numerics.BigInteger]$Hash = [uint64]14695981039346656037
    [Numerics.BigInteger]$Prime = 1099511628211
    [Numerics.BigInteger]$Modulus = [Numerics.BigInteger]::Pow(2, 64)
    foreach ($Byte in $Bytes) {
        $Hash = (($Hash -bxor [uint32]$Byte) * $Prime) % $Modulus
    }
    return ("{0:X16}" -f [uint64]$Hash)
}

function Get-AssetPackDirectory {
    param([byte[]]$Bytes)
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
    $Entries = @()
    for ($Index = 0; $Index -lt $EntryCount; ++$Index) {
        $Offset = [int]$DirectoryOffset + $Index * 128
        $Terminator = [Array]::IndexOf($Bytes, [byte]0, $Offset, 64)
        if ($Terminator -lt 0) { $Terminator = $Offset + 64 }
        $Entries += [pscustomobject]@{
            id = [Text.Encoding]::ASCII.GetString(
                $Bytes, $Offset, $Terminator - $Offset)
            directory_offset = $Offset
            pack_offset = [BitConverter]::ToUInt64($Bytes, $Offset + 84)
            byte_count = [BitConverter]::ToUInt64($Bytes, $Offset + 92)
        }
    }
    foreach ($Entry in $Entries) {
        if ($Entry.pack_offset -gt [uint64]$Bytes.Length -or
            $Entry.byte_count -gt
                [uint64]$Bytes.Length - $Entry.pack_offset) {
            throw "Asset pack entry '$($Entry.id)' is out of bounds."
        }
    }
    return $Entries
}

function Get-EntryBytes {
    param([byte[]]$PackBytes, [object]$Entry)
    $Result = New-Object byte[] ([int]$Entry.byte_count)
    [Array]::Copy($PackBytes, [int]$Entry.pack_offset,
                  $Result, 0, $Result.Length)
    return $Result
}

function Invoke-ShotResolutionTest {
    param([string]$AssetPack, [bool]$ExpectSuccess)
    $Output = @(& $Executable --gameplay-shot-resolution-test `
        $AssetPack 2>&1)
    $ExitCode = $LASTEXITCODE
    if ($ExpectSuccess) {
        if ($ExitCode -ne 0 -or
            ($Output -join [Environment]::NewLine).Trim() -ne
                $ExpectedOutput) {
            throw "TGSR-1 loader/API golden failed.`n$(Get-ShortTail $Output)"
        }
    } elseif ($ExitCode -eq 0 -or
              @($Output | Where-Object {
                  $_ -match "TGSR-1|Shot-resolution asset"
              }).Count -eq 0) {
        throw "Malformed TGSR-1 pack was accepted.`n$(Get-ShortTail $Output)"
    }
}

function Write-PayloadMutationAndReject {
    param([byte[]]$Original, [object]$Entry,
          [string]$Id, [int]$PayloadOffset)
    $Path = Join-Path $Scratch ("payload-" + $Id + ".assetpack")
    $Bytes = [byte[]]$Original.Clone()
    $Absolute = [int]$Entry.pack_offset + $PayloadOffset
    $Bytes[$Absolute] = $Bytes[$Absolute] -bxor 1
    [IO.File]::WriteAllBytes($Path, $Bytes)
    Invoke-ShotResolutionTest $Path $false
}

function Write-MissingEntryAndReject {
    param([byte[]]$Original, [object]$Entry, [string]$Id)
    $Path = Join-Path $Scratch ("missing-" + $Id + ".assetpack")
    $Bytes = [byte[]]$Original.Clone()
    $Bytes[[int]$Entry.directory_offset] = [byte][char]'x'
    [IO.File]::WriteAllBytes($Path, $Bytes)
    Invoke-ShotResolutionTest $Path $false
}

function Write-WrongSizeAndReject {
    param([byte[]]$Original, [object]$Entry,
          [string]$Id, [uint64]$ByteCount)
    $Path = Join-Path $Scratch ("size-" + $Id + ".assetpack")
    $Bytes = [byte[]]$Original.Clone()
    [Array]::Copy([BitConverter]::GetBytes($ByteCount), 0, $Bytes,
                  [int]$Entry.directory_offset + 92, 8)
    [IO.File]::WriteAllBytes($Path, $Bytes)
    Invoke-ShotResolutionTest $Path $false
}

function Invoke-RejectedRomMutation {
    param([byte[]]$Original, [string]$Id, [int]$Offset,
          [string]$ExpectedRange)
    $MutatedRom = Join-Path $Scratch ("rom-" + $Id + ".nes")
    $MutatedPack = Join-Path $Scratch ("rom-" + $Id + ".assetpack")
    $Bytes = [byte[]]$Original.Clone()
    $Bytes[$Offset] = $Bytes[$Offset] -bxor 1
    [IO.File]::WriteAllBytes($MutatedRom, $Bytes)
    $Output = @(& $Executable --build-assetpack `
        $MutatedRom $MutatedPack 2>&1)
    $Text = $Output -join [Environment]::NewLine
    if ($LASTEXITCODE -eq 0 -or $Text -notmatch "TGSR-1" -or
        $Text -notmatch [regex]::Escape($ExpectedRange)) {
        throw "Rev1 source mutation '$Id' was not rejected.`n$(Get-ShortTail $Output)"
    }
}

try {
    $env:TECMO_SKIP_SHORTCUT = "1"
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
    if ($Build) {
        $BuildOutput = @(& (Join-Path $ProjectRoot "build.ps1") 2>&1)
        if ($LASTEXITCODE -ne 0 -or
            @($BuildOutput | Where-Object {
                $_ -match "warning [A-Z]+[0-9]+"
            }).Count -ne 0) {
            throw "Warning-free build failed.`n$(Get-ShortTail $BuildOutput)"
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
        throw "Rev1 TGSR-1 asset-pack build failed.`n$(Get-ShortTail $PackOutput)"
    }
    $PackBytes = [IO.File]::ReadAllBytes($PackPath)
    $Directory = @(Get-AssetPackDirectory $PackBytes)
    $ResolutionEntries = @($Directory | Where-Object {
        $_.id -eq "gameplay/shot-resolution"
    })
    if ($ResolutionEntries.Count -ne 1 -or
        @($Directory | Group-Object id | Where-Object Count -gt 1).Count -ne 0) {
        throw "TGSR-1 entry registration is missing or duplicated."
    }
    $ResolutionEntry = $ResolutionEntries[0]
    $CoreEntry = @($Directory | Where-Object {
        $_.id -eq "gameplay/core"
    } | Select-Object -First 1)
    $SourceMapEntry = @($Directory | Where-Object {
        $_.id -eq "system/source-map"
    } | Select-Object -First 1)
    if ($CoreEntry.Count -ne 1 -or $SourceMapEntry.Count -ne 1) {
        throw "TGSR-1 dependencies are missing from the pack."
    }
    $Payload = Get-EntryBytes $PackBytes $ResolutionEntry
    if ($ResolutionEntry.byte_count -ne 384 -or
        (Get-Fnv1a32 $Payload) -ne "8486DB33" -or
        (Get-Fnv1a64 $Payload) -ne "3DDF28659B192273") {
        throw "TGSR-1 canonical payload contract changed."
    }
    Invoke-ShotResolutionTest $PackPath $true

    $ListOutput = @(& $Executable --assetpack-list $PackPath 2>&1)
    if ($LASTEXITCODE -ne 0 -or
        @($ListOutput | Where-Object {
            $_ -match '^gameplay/shot-resolution\s' -and
            $_ -match 'bank=5' -and $_ -match 'cpu=0x91BC' -and
            $_ -match 'bytes=384'
        }).Count -ne 1) {
        throw "Asset-pack listing omitted the exact TGSR-1 entry.`n$(Get-ShortTail $ListOutput)"
    }

    $SourceMapText = [Text.Encoding]::UTF8.GetString(
        (Get-EntryBytes $PackBytes $SourceMapEntry))
    $SourceMap = $SourceMapText | ConvertFrom-Json
    $Maps = @($SourceMap.logical_entries | Where-Object {
        $_.id -eq "gameplay/shot-resolution"
    })
    $ExpectedSpans = @(
        @{ start=0x91BC; end=0x943A; size=639; f32="4A0C68AC"; f64="1AE095FB719E110C" },
        @{ start=0xA6EE; end=0xA9D9; size=748; f32="21A416FD"; f64="95A583E4DC801DFD" },
        @{ start=0xB73E; end=0xB87B; size=318; f32="574FEE44"; f64="B2E76E7A39990624" },
        @{ start=0xB87C; end=0xB8F5; size=122; f32="9E2F1F28"; f64="C4F3A0BCC17BFCA8" }
    )
    $MapOk = $Maps.Count -eq 1
    if ($MapOk) {
        $Map = $Maps[0]
        $MapOk = $Map.schema -eq
                "tecmo.gameplay-shot-resolution/TGSR-1" -and
            [int]$Map.size -eq 384 -and
            $Map.fingerprint_fnv1a32 -eq "8486DB33" -and
            $Map.fingerprint_fnv1a64 -eq "3DDF28659B192273" -and
            @($Map.dependencies).Count -eq 1 -and
            $Map.dependencies[0].entry -eq "gameplay/core" -and
            @($Map.source_spans).Count -eq 4 -and
            $Map.outcome.terminal_context_required -eq $true -and
            $Map.outcome.clear -eq "make" -and
            $Map.outcome.set -eq "miss" -and
            (@($Map.rim_routes.targets_cpu) -join ",") -eq
                "42760,42921,43241,42760" -and
            (@($Map.claimant_thresholds.horizontal_delta) -join ",") -eq
                "-11,10" -and
            (@($Map.claimant_thresholds.depth_delta) -join ",") -eq
                "-7,6" -and
            $Map.limits -match "not terminal" -and
            $Map.limits -match "not labeled rebound"
    }
    if ($MapOk) {
        for ($Index = 0; $Index -lt $ExpectedSpans.Count; ++$Index) {
            $Actual = $Map.source_spans[$Index]
            $Expected = $ExpectedSpans[$Index]
            if ([int]$Actual.bank -ne 5 -or
                [int]$Actual.cpu_start -ne $Expected.start -or
                [int]$Actual.cpu_end -ne $Expected.end -or
                [int]$Actual.size -ne $Expected.size -or
                $Actual.fingerprint_fnv1a32 -ne $Expected.f32 -or
                $Actual.fingerprint_fnv1a64 -ne $Expected.f64) {
                $MapOk = $false
                break
            }
        }
    }
    if (!$MapOk) {
        throw "TGSR-1 source-map provenance/semantic contract changed."
    }

    @(
        @{ id="magic"; offset=0 },
        @{ id="header-reserved"; offset=85 },
        @{ id="source"; offset=128 },
        @{ id="metadata"; offset=256 },
        @{ id="route"; offset=320 },
        @{ id="padding"; offset=352 }
    ) | ForEach-Object {
        Write-PayloadMutationAndReject $PackBytes $ResolutionEntry `
            $_.id $_.offset
    }
    Write-MissingEntryAndReject $PackBytes $ResolutionEntry "tgsr"
    Write-MissingEntryAndReject $PackBytes $CoreEntry "core"
    Write-WrongSizeAndReject $PackBytes $ResolutionEntry "short" 383
    Write-WrongSizeAndReject $PackBytes $ResolutionEntry "oversized" 385
    Write-PayloadMutationAndReject $PackBytes $CoreEntry "cross-pack-core" 128

    $RomBytes = [IO.File]::ReadAllBytes($RomPath)
    if ($RomBytes.Length -lt 16 -or $RomBytes[4] -ne 8 -or
        $RomBytes[5] -ne 32) {
        throw "ROM is not the expected Rev1 8-PRG/32-CHR iNES layout."
    }
    $Trainer = if (($RomBytes[6] -band 4) -ne 0) { 512 } else { 0 }
    $PrgOffset = 16 + $Trainer
    for ($Index = 0; $Index -lt $ExpectedSpans.Count; ++$Index) {
        $Span = $ExpectedSpans[$Index]
        $Offset = $PrgOffset + 5 * 0x4000 + ($Span.start - 0x8000)
        if ($Index -eq 2) {
            # $B7AD is inside TGSR's $B73E-$B87B claimant span but outside
            # TGCS's $B775-$B7AC and TGJS's $B7C1-$B87B spans.
            $Offset = $PrgOffset + 5 * 0x4000 + (0xB7AD - 0x8000)
        }
        $Range = '${0:X4}-${1:X4}' -f $Span.start, $Span.end
        Invoke-RejectedRomMutation $RomBytes ("source-{0}" -f $Index) `
            $Offset $Range
    }

    Write-Host $ExpectedOutput
    Write-Host "Gameplay shot-resolution focused tests passed."
    $global:LASTEXITCODE = 0
}
finally {
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
}
