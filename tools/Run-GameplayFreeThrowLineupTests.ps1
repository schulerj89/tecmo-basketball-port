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
    throw "TGFL-1 tests require the exact Tecmo NBA Basketball Rev1 ROM."
}

$BuildDir = Join-Path $ProjectRoot "build"
$Executable = Join-Path $BuildDir "tecmo_port.exe"
$Scratch = [IO.Path]::GetFullPath(
    (Join-Path $BuildDir "gameplay_free_throw_lineup_test"))
$BuildPrefix = [IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/') +
    [IO.Path]::DirectorySeparatorChar
if (!$Scratch.StartsWith($BuildPrefix,
                         [StringComparison]::OrdinalIgnoreCase)) {
    throw "Free-throw lineup scratch path escaped build\."
}
$PackPath = Join-Path $Scratch "free-throw-lineup.assetpack"
$ExpectedOutput =
    "TGFL-1 free-throw lineup passed: orientations=2 actors=10 policies=4 poses=040A/040C/040E/0410 indices=517-520"
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

function Invoke-LineupAssetTest {
    param(
        [string]$AssetPack,
        [bool]$ExpectSuccess,
        [string]$ExpectedFailure
    )
    $Output = @(& $Executable --gameplay-free-throw-lineup-test `
        $AssetPack 2>&1)
    $ExitCode = $LASTEXITCODE
    $Text = ($Output -join [Environment]::NewLine).Trim()
    if ($ExpectSuccess) {
        if ($ExitCode -ne 0 -or $Text -ne $ExpectedOutput) {
            throw "TGFL-1 loader/API golden failed.`n$(Get-ShortTail $Output)"
        }
    } elseif ($ExpectedFailure -and
              ($ExitCode -eq 0 -or $Text -ne
                  ("Free-throw lineup asset test failed: " +
                   $ExpectedFailure))) {
        throw "TGFL-1 loader failure changed.`n$(Get-ShortTail $Output)"
    } elseif (!$ExpectedFailure -and
              ($ExitCode -eq 0 -or
               $Text -notmatch "TGFL-1|Free-throw lineup asset")) {
        throw "Malformed TGFL-1 pack was accepted.`n$(Get-ShortTail $Output)"
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
    Invoke-LineupAssetTest $Path $false
}

function Invoke-RejectedRomMutation {
    param(
        [byte[]]$Original,
        [string]$Id,
        [int]$Offset,
        [string]$ExpectedText,
        [string]$ExpectedSchema = "TGFL-1",
        [switch]$CombinedBuilder
    )
    $MutatedRom = Join-Path $Scratch ("rom-" + $Id + ".nes")
    $MutatedPack = Join-Path $Scratch ("rom-" + $Id + ".assetpack")
    $Bytes = [byte[]]$Original.Clone()
    $Bytes[$Offset] = $Bytes[$Offset] -bxor 1
    [IO.File]::WriteAllBytes($MutatedRom, $Bytes)
    if ($CombinedBuilder) {
        $Output = @(& $Executable --build-assetpack `
            $MutatedRom $MutatedPack 2>&1)
    } else {
        $Output = @(& $Executable `
            --gameplay-free-throw-lineup-source-test $MutatedRom 2>&1)
    }
    $ExitCode = $LASTEXITCODE
    if (Test-Path -LiteralPath $MutatedPack) {
        Remove-Item -LiteralPath $MutatedPack -Force
    }
    $Text = $Output -join [Environment]::NewLine
    if ($ExitCode -eq 0 -or $Text -notmatch [regex]::Escape($ExpectedSchema) -or
        $Text -notmatch [regex]::Escape($ExpectedText)) {
        throw "Rev1 source mutation '$Id' was not rejected by $ExpectedSchema.`n$(Get-ShortTail $Output)"
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

    $DirectOutput = @(& $Executable `
        --gameplay-free-throw-lineup-source-test $RomPath 2>&1)
    if ($LASTEXITCODE -ne 0 -or
        ($DirectOutput -join [Environment]::NewLine) -notmatch "TGFL-1") {
        throw "Direct Rev1 TGFL-1 source test failed.`n$(Get-ShortTail $DirectOutput)"
    }
    $PackOutput = @(& $Executable --build-assetpack `
        $RomPath $PackPath 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "Rev1 TGFL-1 asset-pack build failed.`n$(Get-ShortTail $PackOutput)"
    }
    $PackBytes = [IO.File]::ReadAllBytes($PackPath)
    $LineupEntry = Get-AssetPackEntry $PackBytes `
        "gameplay/free-throw-lineup"
    $GameplayEntry = Get-AssetPackEntry $PackBytes "gameplay/core"
    $SourceMapEntry = Get-AssetPackEntry $PackBytes "system/source-map"
    $Payload = Get-EntryBytes $PackBytes $LineupEntry
    if ($LineupEntry.byte_count -ne 1216 -or
        (Get-Fnv1a32 $Payload) -ne "B17B9A3F") {
        throw "gameplay/free-throw-lineup size or fingerprint changed."
    }
    Invoke-LineupAssetTest $PackPath $true

    $ListOutput = @(& $Executable --assetpack-list $PackPath 2>&1)
    if ($LASTEXITCODE -ne 0 -or
        @($ListOutput | Where-Object {
            $_ -match '^gameplay/free-throw-lineup\s' -and
            $_ -match 'bank=6' -and $_ -match 'cpu=0x88B0' -and
            $_ -match 'bytes=1216'
        }).Count -ne 1) {
        throw "Asset-pack listing omitted the exact TGFL-1 entry.`n$(Get-ShortTail $ListOutput)"
    }

    $SourceMap = ([Text.Encoding]::UTF8.GetString(
        (Get-EntryBytes $PackBytes $SourceMapEntry))) | ConvertFrom-Json
    $Maps = @($SourceMap.logical_entries | Where-Object {
        $_.id -eq "gameplay/free-throw-lineup"
    })
    $ExpectedSpans = @(
        @{ start=0x88B0; size=42;  hash="AD834719"; payload=384 },
        @{ start=0x9621; size=334; hash="998D84B8"; payload=432 },
        @{ start=0x976F; size=238; hash="FB7680EF"; payload=768 },
        @{ start=0x985D; size=188; hash="AFB31306"; payload=1008 }
    )
    $MapOk = $Maps.Count -eq 1
    if ($MapOk) {
        $Map = $Maps[0]
        $MapOk =
            $Map.schema -eq
                "tecmo.gameplay-free-throw-lineup/TGFL-1" -and
            $Map.size -eq 1216 -and
            $Map.fingerprint_fnv1a32 -eq "B17B9A3F" -and
            $Map.revision_sha256_identity -eq
                "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4" -and
            $Map.revision_full_rom_fnv1a32 -eq "0650F5B0" -and
            [bool]$Map.revision_full_rom_fnv1a32_verified -and
            [bool]$Map.revision_source_fingerprints_verified -and
            @($Map.dependencies).Count -eq 1 -and
            $Map.dependencies[0].entry -eq "gameplay/core" -and
            $Map.dependencies[0].size -eq 23416 -and
            $Map.dependencies[0].fingerprint_fnv1a32 -eq
                "2047CCE0" -and
            @($Map.source_spans).Count -eq 4 -and
            $Map.lineup_contract.pose_index_rule -eq
                "raw_pose_offset/2" -and
            (@($Map.lineup_contract.validated_pose_indexes) -join ',') -eq
                "517,518,519,520" -and
            $Map.lineup_contract.shooter_pose -match
                "preserved/undefined" -and
            $Map.lineup_contract.base_nonshooter_state.raw_046E -eq 0 -and
            $Map.lineup_contract.base_nonshooter_state.raw_057C -eq 1 -and
            $Map.lineup_contract.base_nonshooter_state.raw_0458 -eq 48 -and
            $Map.lineup_contract.base_nonshooter_state.raw_0479 -eq 193 -and
            $Map.lineup_contract.base_shooter_state.raw_046E -eq 32 -and
            $Map.lineup_contract.base_shooter_state.raw_0479 -eq 65 -and
            $Map.live_scene_integration.orientation_source -match
                "TGOR-1 attack_direction" -and
            $Map.live_scene_integration.position_binding -match
                "exact TGFL-1 raw world X/Y" -and
            $Map.live_scene_integration.camera_binding -match
                "orientation 0 camera_x=102" -and
            $Map.live_scene_integration.camera_binding -match
                "orientation 1 camera_x=408" -and
            $Map.live_scene_integration.render_binding -match
                "combined TGCT-1 slice" -and
            $Map.live_scene_integration.pose_binding -match
                "preserves existing actor poses" -and
            ![bool]$Map.live_scene_integration.integration_is_additional_rom_claim -and
            $Map.supported_boundary -match "native live positioning" -and
            $Map.supported_boundary -match "not additional ROM claims" -and
            $Map.supported_boundary -match "no live pose-state override" -and
            $Map.supported_boundary -match "no .*attempt decrement"
        if ($MapOk) {
            for ($Index = 0; $Index -lt $ExpectedSpans.Count; ++$Index) {
                $Expected = $ExpectedSpans[$Index]
                $Actual = $Map.source_spans[$Index]
                $ExpectedOffset = [uint64]$SourceMap.source.prg_offset +
                    6 * 0x4000 + ($Expected.start - 0x8000)
                if ($Actual.source_entry -ne "prg/bank06" -or
                    $Actual.bank -ne 6 -or
                    [bool]$Actual.fixed_bank -or
                    [uint64]$Actual.source_offset -ne $ExpectedOffset -or
                    $Actual.cpu_start -ne $Expected.start -or
                    $Actual.cpu_end -ne
                        ($Expected.start + $Expected.size - 1) -or
                    $Actual.size -ne $Expected.size -or
                    $Actual.fingerprint_fnv1a32 -ne $Expected.hash -or
                    $Actual.payload_offset -ne $Expected.payload) {
                    $MapOk = $false
                    break
                }
            }
        }
    }
    if (!$MapOk) {
        throw "TGFL-1 source-map provenance is incomplete or malformed."
    }

    foreach ($Mutation in @(
        @{ id="magic"; offset=0 },
        @{ id="version"; offset=4 },
        @{ id="declared-size"; offset=8 },
        @{ id="source-stride"; offset=14 },
        @{ id="core-dependency"; offset=28 },
        @{ id="revision-fnv"; offset=36 },
        @{ id="revision-sha"; offset=40 },
        @{ id="pose-descriptor"; offset=72 },
        @{ id="semantic-reserved"; offset=125 },
        @{ id="source-record"; offset=256 },
        @{ id="source-reserved"; offset=276 },
        @{ id="pose-source"; offset=384 },
        @{ id="pose-padding"; offset=426 },
        @{ id="round-combined-source"; offset=432 },
        @{ id="round-padding"; offset=766 },
        @{ id="followup-source"; offset=768 },
        @{ id="followup-padding"; offset=1006 },
        @{ id="tables-source"; offset=1008 },
        @{ id="trailing-reserved"; offset=1196 }
    )) {
        Write-PayloadMutationAndReject $PackBytes $LineupEntry `
            $Mutation.id $Mutation.offset
    }

    foreach ($Case in @(
        @{ id="undersized-lineup"; entry=$LineupEntry; size=1215;
           status="TGFL-1 gameplay/free-throw-lineup entry missing or wrong-sized" },
        @{ id="oversized-lineup"; entry=$LineupEntry; size=1217;
           status="TGFL-1 gameplay/free-throw-lineup entry missing or wrong-sized" },
        @{ id="undersized-core"; entry=$GameplayEntry; size=23415;
           status="TGFL-1 gameplay/core dependency missing or wrong-sized" },
        @{ id="oversized-core"; entry=$GameplayEntry; size=23417;
           status="TGFL-1 gameplay/core dependency missing or wrong-sized" }
    )) {
        $Path = Join-Path $Scratch ($Case.id + ".assetpack")
        $Bytes = [byte[]]$PackBytes.Clone()
        [BitConverter]::GetBytes([uint64]$Case.size).CopyTo(
            $Bytes, [int]$Case.entry.directory_offset + 92)
        [IO.File]::WriteAllBytes($Path, $Bytes)
        Invoke-LineupAssetTest $Path $false $Case.status
    }
    foreach ($Case in @(
        @{ id="missing-lineup"; entry=$LineupEntry;
           status="TGFL-1 gameplay/free-throw-lineup entry missing or wrong-sized" },
        @{ id="missing-core"; entry=$GameplayEntry;
           status="TGFL-1 gameplay/core dependency missing or wrong-sized" }
    )) {
        $Path = Join-Path $Scratch ($Case.id + ".assetpack")
        $Bytes = [byte[]]$PackBytes.Clone()
        $Bytes[[int]$Case.entry.directory_offset] = [byte][char]'x'
        [IO.File]::WriteAllBytes($Path, $Bytes)
        Invoke-LineupAssetTest $Path $false $Case.status
    }
    foreach ($Case in @(
        @{ id="malformed-core"; offset=0 },
        @{ id="cross-pack-core"; offset=184 }
    )) {
        $Path = Join-Path $Scratch ($Case.id + ".assetpack")
        $Bytes = [byte[]]$PackBytes.Clone()
        $Absolute = [int]$GameplayEntry.pack_offset + $Case.offset
        $Bytes[$Absolute] = $Bytes[$Absolute] -bxor 1
        [IO.File]::WriteAllBytes($Path, $Bytes)
        Invoke-LineupAssetTest $Path $false
    }

    $RomBytes = [IO.File]::ReadAllBytes($RomPath)
    # The normal combined builder preserves TPNL's earlier shared Rev1 gate.
    Invoke-RejectedRomMutation $RomBytes "full-rom-header-reserved" 15 `
        "full-ROM SHA-256 mismatch" "TPNL-1" -CombinedBuilder
    $Trainer = if (($RomBytes[6] -band 4) -ne 0) { 512 } else { 0 }
    $Prg = 16 + $Trainer
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
            $Offset = $Prg + 6 * 0x4000 + ($Point.cpu - 0x8000)
            Invoke-RejectedRomMutation $RomBytes $Id $Offset `
                "full-ROM SHA-256 mismatch"
            ++$RomMutationCount
        }
    }

    Write-Host ("TGFL-1 focused tests passed: direct exact Rev1 iNES/FNV/SHA, " +
        "canonical raw Bank06 spans, " +
        "both orientations, shooter-dependent stream skip, raw state seeds, " +
        "TGPL pose indexes 517..520, strict source map, missing/malformed/" +
        "undersized/oversized/cross-pack rejection, $RomMutationCount " +
        "source mutations")
    $global:LASTEXITCODE = 0
} finally {
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
}
