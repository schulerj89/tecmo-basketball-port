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
$BuildDir = Join-Path $ProjectRoot "build"
$Executable = Join-Path $BuildDir "tecmo_port.exe"
$Scratch = [IO.Path]::GetFullPath(
    (Join-Path $BuildDir "gameplay_close_shot_test"))
$BuildPrefix = [IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/') +
    [IO.Path]::DirectorySeparatorChar
if (!$Scratch.StartsWith($BuildPrefix,
                         [StringComparison]::OrdinalIgnoreCase)) {
    throw "Close-shot test scratch path escaped build\."
}
$PackPath = Join-Path $Scratch "close-shots.assetpack"
$ExpectedOutput =
    "TGCS-1 close-shot assets passed: sources=13 variants=2 steps=48 poses=208 phases=0445C745 pose-sequence=BFDB4095"
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

function Invoke-CloseShotAssetTest {
    param([string]$AssetPack, [bool]$ExpectSuccess)
    $Output = @(& $Executable --gameplay-close-shots-test $AssetPack 2>&1)
    $ExitCode = $LASTEXITCODE
    if ($ExpectSuccess) {
        if ($ExitCode -ne 0 -or
            ($Output -join [Environment]::NewLine).Trim() -ne
                $ExpectedOutput) {
            throw "TGCS-1 loader golden failed.`n$(Get-ShortTail $Output)"
        }
    } elseif ($ExitCode -eq 0 -or
              @($Output | Where-Object {
                  $_ -match "TGCS-1|Close-shot asset"
              }).Count -eq 0) {
        throw "Malformed TGCS-1 pack was accepted.`n$(Get-ShortTail $Output)"
    }
}

function Write-MutatedPackAndReject {
    param([byte[]]$Original, [object]$Entry,
          [string]$Id, [int]$PayloadOffset)
    $Path = Join-Path $Scratch ("payload-" + $Id + ".assetpack")
    $Bytes = [byte[]]$Original.Clone()
    $Absolute = [int]$Entry.pack_offset + $PayloadOffset
    $Bytes[$Absolute] = $Bytes[$Absolute] -bxor 1
    [IO.File]::WriteAllBytes($Path, $Bytes)
    Invoke-CloseShotAssetTest $Path $false
}

function Invoke-RejectedRomMutation {
    param(
        [byte[]]$Original,
        [string]$Id,
        [int]$Offset,
        [string]$ExpectedRange
    )
    $MutatedRom = Join-Path $Scratch ("rom-" + $Id + ".nes")
    $MutatedPack = Join-Path $Scratch ("rom-" + $Id + ".assetpack")
    $Bytes = [byte[]]$Original.Clone()
    $Bytes[$Offset] = $Bytes[$Offset] -bxor 1
    [IO.File]::WriteAllBytes($MutatedRom, $Bytes)
    $Output = @(& $Executable --build-assetpack `
        $MutatedRom $MutatedPack 2>&1)
    $ExitCode = $LASTEXITCODE
    if (Test-Path -LiteralPath $MutatedPack) {
        Remove-Item -LiteralPath $MutatedPack -Force
    }
    $Text = $Output -join [Environment]::NewLine
    if ($ExitCode -eq 0 -or $Text -notmatch "TGCS-1" -or
        $Text -notmatch [regex]::Escape($ExpectedRange)) {
        throw "Rev1 source mutation '$Id' was not rejected by its TGCS-1 span.`n$(Get-ShortTail $Output)"
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
        throw "Rev1 TGCS-1 asset-pack build failed.`n$(Get-ShortTail $PackOutput)"
    }
    $PackBytes = [IO.File]::ReadAllBytes($PackPath)
    $CloseShotEntry = Get-AssetPackEntry $PackBytes "gameplay/close-shots"
    $GameplayEntry = Get-AssetPackEntry $PackBytes "gameplay/core"
    $SourceMapEntry = Get-AssetPackEntry $PackBytes "system/source-map"
    $Payload = Get-EntryBytes $PackBytes $CloseShotEntry
    if ($CloseShotEntry.byte_count -ne 3144 -or
        (Get-Fnv1a32 $Payload) -ne "DACDC976") {
        throw "gameplay/close-shots size or canonical fingerprint changed."
    }
    Invoke-CloseShotAssetTest $PackPath $true

    $ListOutput = @(& $Executable --assetpack-list $PackPath 2>&1)
    if ($LASTEXITCODE -ne 0 -or
        @($ListOutput | Where-Object {
            $_ -match '^gameplay/close-shots\s' -and
            $_ -match 'bank=5' -and $_ -match 'cpu=0x8542' -and
            $_ -match 'bytes=3144'
        }).Count -ne 1) {
        throw "Asset-pack listing omitted the exact TGCS-1 directory entry.`n$(Get-ShortTail $ListOutput)"
    }

    $SourceMapText = [Text.Encoding]::UTF8.GetString(
        (Get-EntryBytes $PackBytes $SourceMapEntry))
    $SourceMap = $SourceMapText | ConvertFrom-Json
    $CloseShotMap = @($SourceMap.logical_entries | Where-Object {
        $_.id -eq "gameplay/close-shots"
    })
    $ExpectedSpans = @(
        @{ start=0x8542; size=339; hash="7FF62153" },
        @{ start=0x919C; size=32; hash="8FBAE6F7" },
        @{ start=0x98E1; size=383; hash="0A2F945A" },
        @{ start=0xA214; size=75; hash="4FC82BF8" },
        @{ start=0xA503; size=491; hash="0EBCAB02" },
        @{ start=0xAB36; size=212; hash="161533A4" },
        @{ start=0xB100; size=63; hash="1B3B5F1D" },
        @{ start=0xB32C; size=502; hash="82A40926" },
        @{ start=0xB678; size=109; hash="026830BD" },
        @{ start=0xB775; size=56; hash="2E13AB9D" },
        @{ start=0xBDEF; size=8; hash="9D46B0CD" },
        @{ start=0xBFC2; size=7; hash="D2AABFC3" },
        @{ start=0x8CED; size=80; hash="9BFCCE7C" }
    )
    $MapOk = $CloseShotMap.Count -eq 1
    if ($MapOk) {
        $Map = $CloseShotMap[0]
        $MapOk = $Map.schema -eq "tecmo.gameplay-close-shots/TGCS-1" -and
            $Map.size -eq 3144 -and
            $Map.fingerprint_fnv1a32 -eq "DACDC976" -and
            $Map.dependency.entry -eq "gameplay/core" -and
            $Map.dependency.size -eq 23416 -and
            $Map.dependency.fingerprint_fnv1a32 -eq "2047CCE0" -and
            $Map.dependency.reason -eq "actor-pointer-index resolution" -and
            @($Map.source_spans).Count -eq 13 -and
            @($Map.supported_variants).Count -eq 2 -and
            $Map.raw_aggregate.size -eq 2357 -and
            $Map.raw_aggregate.fingerprint_fnv1a32 -eq "9CDEB66F" -and
            $Map.phase_tables_fingerprint_fnv1a32 -eq "0445C745" -and
            $Map.supported_variants[0].numeric_id -eq 0 -and
            $Map.supported_variants[0].step_count -eq 32 -and
            $Map.supported_variants[0].pose_phase_count -eq 7 -and
            $Map.supported_variants[1].numeric_id -eq 2 -and
            $Map.supported_variants[1].step_count -eq 16 -and
            $Map.supported_variants[1].pose_phase_count -eq 6 -and
            (@($Map.supported_variants[0].phase_table) -join ",") -eq
                "1,2,3,3,3,4,4,4,4,4,4,4,4,4,4,4,6,6,6,6,6,6,6,5,5,5,5,5,5,5,5,5" -and
            (@($Map.supported_variants[1].phase_table) -join ",") -eq
                "0,1,2,3,3,4,4,4,5,5,5,5,5,5,5,5" -and
            (@($Map.supported_variants[0].family) -join ",") -eq
                "direct,held-release" -and
            (@($Map.supported_variants[1].family) -join ",") -eq
                "arc,longer-trajectory,contactable" -and
            (@($Map.pose_contract.source_cpu_range) -join ",") -eq
                "36077,36156" -and
            $Map.pose_contract.raw_fingerprint_fnv1a32 -eq "9BFCCE7C" -and
            $Map.pose_contract.encoding -eq
                "40 low bytes followed by 40 high bytes; even byte offsets divided by two" -and
            $Map.pose_contract.profile_count -eq 2 -and
            $Map.pose_contract.direction_count -eq 8 -and
            $Map.pose_contract.resolved_pointer_count -eq 208 -and
            $Map.pose_contract.resolved_sequence_fingerprint_fnv1a32 -eq
                "BFDB4095" -and
            (@($Map.pose_contract.variant0_bases[0]) -join ",") -eq
                "637,609,623,630,616,595,644,602" -and
            (@($Map.pose_contract.variant0_bases[1]) -join ",") -eq
                "693,665,679,686,672,651,700,658" -and
            (@($Map.pose_contract.variant2_bases[0]) -join ",") -eq
                "807,783,795,801,789,771,813,777" -and
            (@($Map.pose_contract.variant2_bases[1]) -join ",") -eq
                "855,831,843,849,837,819,861,825" -and
            (@($Map.pose_contract.unsupported_numeric_ids) -join ",") -eq "1" -and
            $Map.pose_contract.unsupported_raw_group_policy -eq
                "intentionally unexposed"
        if ($MapOk) {
            for ($Index = 0; $Index -lt $ExpectedSpans.Count; ++$Index) {
                $Expected = $ExpectedSpans[$Index]
                $Actual = $Map.source_spans[$Index]
                $End = $Expected.start + $Expected.size - 1
                $ExpectedSourceOffset =
                    [uint64]$SourceMap.source.prg_offset + 5 * 0x4000 +
                    ($Expected.start - 0x8000)
                if ($Actual.source_entry -ne "prg/bank05" -or
                    $Actual.bank -ne 5 -or
                    [uint64]$Actual.source_offset -ne $ExpectedSourceOffset -or
                    $Actual.cpu_start -ne $Expected.start -or
                    $Actual.cpu_end -ne $End -or
                    $Actual.size -ne $Expected.size -or
                    $Actual.fingerprint_fnv1a32 -ne $Expected.hash) {
                    $MapOk = $false
                    break
                }
            }
        }
    }
    if (!$MapOk) {
        throw "TGCS-1 source-map provenance is incomplete or malformed."
    }

    $PayloadMutations = @(
        @{ id="magic"; offset=0 },
        @{ id="declared-size"; offset=8 },
        @{ id="dependency-size"; offset=56 },
        @{ id="dependency-hash"; offset=60 },
        @{ id="header-reserved"; offset=112 },
        @{ id="source-record-first"; offset=256 },
        @{ id="source-record-last"; offset=256 + 12 * 32 + 16 },
        @{ id="raw-source-first"; offset=672 },
        @{ id="raw-source-last"; offset=3028 },
        @{ id="padding"; offset=3029 },
        @{ id="variant0-phase"; offset=3032 },
        @{ id="variant2-phase"; offset=3064 },
        @{ id="pose-base"; offset=3080 }
    )
    foreach ($Mutation in $PayloadMutations) {
        Write-MutatedPackAndReject $PackBytes $CloseShotEntry `
            $Mutation.id $Mutation.offset
    }

    $OversizedPath = Join-Path $Scratch "oversized.assetpack"
    $Oversized = [byte[]]$PackBytes.Clone()
    [BitConverter]::GetBytes([uint64]3145).CopyTo(
        $Oversized, [int]$CloseShotEntry.directory_offset + 92)
    [IO.File]::WriteAllBytes($OversizedPath, $Oversized)
    Invoke-CloseShotAssetTest $OversizedPath $false

    $MissingPath = Join-Path $Scratch "missing-close-shots.assetpack"
    $Missing = [byte[]]$PackBytes.Clone()
    $Missing[[int]$CloseShotEntry.directory_offset] = [byte][char]'x'
    [IO.File]::WriteAllBytes($MissingPath, $Missing)
    Invoke-CloseShotAssetTest $MissingPath $false

    $MissingDependencyPath = Join-Path $Scratch "missing-core.assetpack"
    $MissingDependency = [byte[]]$PackBytes.Clone()
    $MissingDependency[[int]$GameplayEntry.directory_offset] = [byte][char]'x'
    [IO.File]::WriteAllBytes($MissingDependencyPath, $MissingDependency)
    Invoke-CloseShotAssetTest $MissingDependencyPath $false

    $MalformedDependencyPath = Join-Path $Scratch "malformed-core.assetpack"
    $MalformedDependency = [byte[]]$PackBytes.Clone()
    $MalformedDependency[[int]$GameplayEntry.pack_offset] =
        $MalformedDependency[[int]$GameplayEntry.pack_offset] -bxor 1
    [IO.File]::WriteAllBytes($MalformedDependencyPath, $MalformedDependency)
    Invoke-CloseShotAssetTest $MalformedDependencyPath $false

    $CrossPackPath = Join-Path $Scratch "cross-pack-core.assetpack"
    $CrossPack = [byte[]]$PackBytes.Clone()
    $CrossPack[[int]$GameplayEntry.pack_offset + 184] =
        $CrossPack[[int]$GameplayEntry.pack_offset + 184] -bxor 1
    [IO.File]::WriteAllBytes($CrossPackPath, $CrossPack)
    Invoke-CloseShotAssetTest $CrossPackPath $false

    $RomBytes = [IO.File]::ReadAllBytes($RomPath)
    $Trainer = if (($RomBytes[6] -band 4) -ne 0) { 512 } else { 0 }
    $Prg = 16 + $Trainer
    $Bank05 = $Prg + 5 * 0x4000
    $RomMutationCount = 0
    foreach ($Span in $ExpectedSpans) {
        $End = $Span.start + $Span.size - 1
        $Middle = $Span.start + [Math]::Floor(($Span.size - 1) / 2)
        $Range = ('$' + ("{0:X4}" -f $Span.start) + '-$' +
            ("{0:X4}" -f $End))
        foreach ($Point in @(
            @{ label="start"; cpu=$Span.start },
            @{ label="middle"; cpu=$Middle },
            @{ label="end"; cpu=$End }
        )) {
            $Id = ("{0:X4}-{1}" -f $Span.start, $Point.label)
            $Offset = $Bank05 + ($Point.cpu - 0x8000)
            Invoke-RejectedRomMutation $RomBytes $Id $Offset $Range
            ++$RomMutationCount
        }
    }
    foreach ($Point in @(
        @{ id="variant0-phase-start"; cpu=0x85D3; range='$8542-$8694' },
        @{ id="variant0-phase-end"; cpu=0x85F2; range='$8542-$8694' },
        @{ id="variant2-phase-start"; cpu=0x8685; range='$8542-$8694' },
        @{ id="pose-high-start"; cpu=0x8D15; range='$8CED-$8D3C' }
    )) {
        Invoke-RejectedRomMutation $RomBytes $Point.id `
            ($Bank05 + ($Point.cpu - 0x8000)) $Point.range
        ++$RomMutationCount
    }

    Write-Host ("TGCS-1 focused tests passed: canonical, provenance, " +
        "reload, 208 poses, malformed/missing/cross-pack dependency, " +
        "$RomMutationCount Rev1 endpoint/interior mutations")
    $global:LASTEXITCODE = 0
} finally {
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
}
