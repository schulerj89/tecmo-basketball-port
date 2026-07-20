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
    throw "TPNL-1 tests require the exact Tecmo NBA Basketball Rev1 ROM."
}

$BuildDir = Join-Path $ProjectRoot "build"
$Executable = Join-Path $BuildDir "tecmo_port.exe"
$Scratch = [IO.Path]::GetFullPath(
    (Join-Path $BuildDir "gameplay_penalty_test"))
$BuildPrefix = [IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/') +
    [IO.Path]::DirectorySeparatorChar
if (!$Scratch.StartsWith($BuildPrefix,
                         [StringComparison]::OrdinalIgnoreCase)) {
    throw "Penalty test scratch path escaped build\."
}
$PackPath = Join-Path $Scratch "penalties.assetpack"
$ExpectedOutput =
    "TPNL-1 penalty assets passed: sources=8 classes=3 violations=7 caps=6/5 bonus=5/4 selectors=2FT:8,1FT:2 cue=6@16"
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

function Invoke-PenaltyAssetTest {
    param(
        [string]$AssetPack,
        [bool]$ExpectSuccess,
        [string]$ExpectedFailure
    )
    $Output = @(& $Executable --gameplay-penalties-test $AssetPack 2>&1)
    $ExitCode = $LASTEXITCODE
    $Text = ($Output -join [Environment]::NewLine).Trim()
    if ($ExpectSuccess) {
        if ($ExitCode -ne 0 -or $Text -ne $ExpectedOutput) {
            throw "TPNL-1 loader golden failed.`n$(Get-ShortTail $Output)"
        }
    } elseif ($ExpectedFailure -and
              ($ExitCode -eq 0 -or $Text -ne
                  ("Penalty asset test failed: " + $ExpectedFailure))) {
        throw "TPNL-1 loader failure changed.`n$(Get-ShortTail $Output)"
    } elseif (!$ExpectedFailure -and
              ($ExitCode -eq 0 -or $Text -notmatch "TPNL-1|Penalty asset")) {
        throw "Malformed TPNL-1 pack was accepted.`n$(Get-ShortTail $Output)"
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
    Invoke-PenaltyAssetTest $Path $false
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
    if ($ExitCode -eq 0 -or $Text -notmatch "TPNL-1" -or
        $Text -notmatch [regex]::Escape($ExpectedRange)) {
        throw "Rev1 source mutation '$Id' was not rejected by TPNL-1.`n$(Get-ShortTail $Output)"
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
        throw "Rev1 TPNL-1 asset-pack build failed.`n$(Get-ShortTail $PackOutput)"
    }
    $PackBytes = [IO.File]::ReadAllBytes($PackPath)
    $PenaltyEntry = Get-AssetPackEntry $PackBytes "gameplay/penalties"
    $GameplayEntry = Get-AssetPackEntry $PackBytes "gameplay/core"
    $SfxEntry = Get-AssetPackEntry $PackBytes "audio/gameplay-sfx"
    $SourceMapEntry = Get-AssetPackEntry $PackBytes "system/source-map"
    $Payload = Get-EntryBytes $PackBytes $PenaltyEntry
    if ($PenaltyEntry.byte_count -ne 768 -or
        (Get-Fnv1a32 $Payload) -ne "980DDC76") {
        throw "gameplay/penalties size or canonical fingerprint changed."
    }
    Invoke-PenaltyAssetTest $PackPath $true

    $ListOutput = @(& $Executable --assetpack-list $PackPath 2>&1)
    if ($LASTEXITCODE -ne 0 -or
        @($ListOutput | Where-Object {
            $_ -match '^gameplay/penalties\s' -and
            $_ -match 'bank=5' -and $_ -match 'cpu=0x9571' -and
            $_ -match 'bytes=768'
        }).Count -ne 1) {
        throw "Asset-pack listing omitted the exact TPNL-1 directory entry.`n$(Get-ShortTail $ListOutput)"
    }

    $SourceMap = ([Text.Encoding]::UTF8.GetString(
        (Get-EntryBytes $PackBytes $SourceMapEntry))) | ConvertFrom-Json
    $Ids = @($SourceMap.logical_entries | ForEach-Object { $_.id })
    if (@($Ids | Group-Object | Where-Object { $_.Count -ne 1 }).Count -ne 0) {
        throw "Asset-pack source map contains duplicate logical entry IDs."
    }
    $PenaltyMaps = @($SourceMap.logical_entries | Where-Object {
        $_.id -eq "gameplay/penalties"
    })
    $ExpectedSpans = @(
        @{ bank=5; fixed=$false; start=0x9571; size=217; hash="C83877F7" },
        @{ bank=2; fixed=$false; start=0xB0F8; size=673; hash="A06E397C" },
        @{ bank=7; fixed=$true;  start=0xE95E; size=180; hash="9AFB64FE" },
        @{ bank=7; fixed=$true;  start=0xEA14; size=28;  hash="D9D3C9CE" },
        @{ bank=7; fixed=$true;  start=0xEC5B; size=186; hash="288C2162" },
        @{ bank=7; fixed=$true;  start=0xD2B9; size=22;  hash="0DDA3C9A" },
        @{ bank=3; fixed=$false; start=0xBE87; size=290; hash="C8FFCCED" },
        @{ bank=4; fixed=$false; start=0xBA1F; size=32;  hash="F56AD5D8" }
    )
    $MapOk = $PenaltyMaps.Count -eq 1
    if ($MapOk) {
        $Map = $PenaltyMaps[0]
        $MapOk = $Map.schema -eq "tecmo.gameplay-penalties/TPNL-1" -and
            $Map.size -eq 768 -and
            $Map.fingerprint_fnv1a32 -eq "980DDC76" -and
            $Map.revision_sha256 -eq
                "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4" -and
            [bool]$Map.revision_sha256_verified -and
            @($Map.dependencies).Count -eq 2 -and
            $Map.dependencies[0].entry -eq "gameplay/core" -and
            $Map.dependencies[0].size -eq 23416 -and
            $Map.dependencies[0].fingerprint_fnv1a32 -eq "2047CCE0" -and
            $Map.dependencies[1].entry -eq "audio/gameplay-sfx" -and
            $Map.dependencies[1].size -eq 2824 -and
            $Map.dependencies[1].fingerprint_fnv1a32 -eq "968A5DE6" -and
            @($Map.source_spans).Count -eq 8 -and
            $Map.rules.individual_foul_cap -eq 6 -and
            $Map.rules.team_foul_cap -eq 5 -and
            $Map.rules.regulation_bonus_threshold -eq 5 -and
            $Map.rules.overtime_bonus_threshold -eq 4 -and
            (@($Map.rules.two_attempt_saved_route_selectors) -join ',') -eq
                '1,5,11,12,13,14,15,18' -and
            (@($Map.rules.one_attempt_current_route_selectors) -join ',') -eq
                '8,9' -and
            $Map.presentations.foul.lead_in_frames -eq 4 -and
            $Map.presentations.foul.maximum_wait_frames -eq 160 -and
            $Map.presentations.foul.screen_selector -eq 34 -and
            $Map.presentations.foul.presentation_sfx_id -eq 6 -and
            $Map.presentations.foul.presentation_sfx_delay_frames -eq 16 -and
            $Map.presentations.foul.live_restart_sfx_id -eq 5 -and
            $Map.presentations.foul.live_restart_music_id -eq 5 -and
            $Map.presentations.violation.five_seconds_selector -eq 3 -and
            $Map.presentations.violation.lead_in_frames -eq 4 -and
            $Map.presentations.violation.maximum_wait_frames -eq 120 -and
            $Map.presentations.violation.screen_selector -eq 34 -and
            $Map.presentations.violation.presentation_sfx_id -eq 6 -and
            $Map.presentations.violation.presentation_sfx_delay_frames -eq 16 -and
            $Map.presentations.violation.live_restart_sfx_id -eq 5 -and
            $Map.presentations.violation.live_restart_music_id -eq 5 -and
            $Map.presentations.release.initial_delay_frames -eq 4 -and
            $Map.presentations.release.poll_interval_frames -eq 1 -and
            $Map.presentations.release.nes_a_mask -eq 128 -and
            $Map.presentations.release.controller_count -eq 2
        if ($MapOk) {
            for ($Index = 0; $Index -lt $ExpectedSpans.Count; ++$Index) {
                $Expected = $ExpectedSpans[$Index]
                $Actual = $Map.source_spans[$Index]
                $End = $Expected.start + $Expected.size - 1
                $CpuBase = if ($Expected.fixed) { 0xC000 } else { 0x8000 }
                $ExpectedOffset = [uint64]$SourceMap.source.prg_offset +
                    $Expected.bank * 0x4000 + ($Expected.start - $CpuBase)
                $ExpectedEntry = if ($Expected.fixed) {
                    'prg/fixed'
                } else {
                    'prg/bank' + ("{0:D2}" -f $Expected.bank)
                }
                if ($Actual.source_entry -ne $ExpectedEntry -or
                    $Actual.bank -ne $Expected.bank -or
                    [bool]$Actual.fixed_bank -ne $Expected.fixed -or
                    [uint64]$Actual.source_offset -ne $ExpectedOffset -or
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
        throw "TPNL-1 source-map provenance is incomplete or malformed."
    }

    foreach ($Mutation in @(
        @{ id="magic"; offset=0 },
        @{ id="version"; offset=4 },
        @{ id="declared-size"; offset=8 },
        @{ id="source-stride"; offset=14 },
        @{ id="core-dependency"; offset=64 },
        @{ id="sfx-dependency"; offset=76 },
        @{ id="revision"; offset=104 },
        @{ id="header-reserved"; offset=136 },
        @{ id="source-record"; offset=256 },
        @{ id="source-reserved"; offset=274 },
        @{ id="shared-cue-source-record"; offset=452 },
        @{ id="rule-cap"; offset=480 },
        @{ id="foul-class"; offset=544 },
        @{ id="attempt-selector"; offset=592 },
        @{ id="five-second-selector"; offset=640 },
        @{ id="violation-cue-delay"; offset=654 },
        @{ id="foul-presentation"; offset=720 },
        @{ id="foul-presentation-cue"; offset=723 },
        @{ id="foul-presentation-cue-delay"; offset=734 },
        @{ id="violation-presentation"; offset=744 },
        @{ id="violation-presentation-cue-delay"; offset=758 }
    )) {
        Write-MutatedPackAndReject $PackBytes $PenaltyEntry `
            $Mutation.id $Mutation.offset
    }

    foreach ($Case in @(
        @{ id="undersized-penalties"; entry=$PenaltyEntry; size=767;
           status="TPNL-1 gameplay/penalties entry missing or wrong-sized" },
        @{ id="oversized-penalties"; entry=$PenaltyEntry; size=769;
           status="TPNL-1 gameplay/penalties entry missing or wrong-sized" },
        @{ id="undersized-core"; entry=$GameplayEntry; size=23415;
           status="TPNL-1 gameplay/core dependency missing or wrong-sized" },
        @{ id="oversized-core"; entry=$GameplayEntry; size=23417;
           status="TPNL-1 gameplay/core dependency missing or wrong-sized" },
        @{ id="undersized-sfx"; entry=$SfxEntry; size=2823;
           status="TPNL-1 audio/gameplay-sfx dependency missing or wrong-sized" },
        @{ id="oversized-sfx"; entry=$SfxEntry; size=2825;
           status="TPNL-1 audio/gameplay-sfx dependency missing or wrong-sized" }
    )) {
        $Path = Join-Path $Scratch ($Case.id + ".assetpack")
        $Bytes = [byte[]]$PackBytes.Clone()
        [BitConverter]::GetBytes([uint64]$Case.size).CopyTo(
            $Bytes, [int]$Case.entry.directory_offset + 92)
        [IO.File]::WriteAllBytes($Path, $Bytes)
        Invoke-PenaltyAssetTest $Path $false $Case.status
    }
    foreach ($Case in @(
        @{ id="missing-penalties"; entry=$PenaltyEntry;
           status="TPNL-1 gameplay/penalties entry missing or wrong-sized" },
        @{ id="missing-core"; entry=$GameplayEntry;
           status="TPNL-1 gameplay/core dependency missing or wrong-sized" },
        @{ id="missing-sfx"; entry=$SfxEntry;
           status="TPNL-1 audio/gameplay-sfx dependency missing or wrong-sized" }
    )) {
        $Path = Join-Path $Scratch ($Case.id + ".assetpack")
        $Bytes = [byte[]]$PackBytes.Clone()
        $Bytes[[int]$Case.entry.directory_offset] = [byte][char]'x'
        [IO.File]::WriteAllBytes($Path, $Bytes)
        Invoke-PenaltyAssetTest $Path $false $Case.status
    }
    foreach ($Case in @(
        @{ id="malformed-core"; entry=$GameplayEntry; offset=0 },
        @{ id="cross-pack-core"; entry=$GameplayEntry; offset=184 },
        @{ id="malformed-sfx"; entry=$SfxEntry; offset=0 },
        @{ id="cross-pack-sfx"; entry=$SfxEntry; offset=128 }
    )) {
        $Path = Join-Path $Scratch ($Case.id + ".assetpack")
        $Bytes = [byte[]]$PackBytes.Clone()
        $Absolute = [int]$Case.entry.pack_offset + $Case.offset
        $Bytes[$Absolute] = $Bytes[$Absolute] -bxor 1
        [IO.File]::WriteAllBytes($Path, $Bytes)
        Invoke-PenaltyAssetTest $Path $false
    }

    $RomBytes = [IO.File]::ReadAllBytes($RomPath)
    Invoke-RejectedRomMutation $RomBytes "full-rom-header-reserved" 15 `
        "full-ROM SHA-256 mismatch"
    $Trainer = if (($RomBytes[6] -band 4) -ne 0) { 512 } else { 0 }
    $Prg = 16 + $Trainer
    $RomMutationCount = 0
    foreach ($Span in $ExpectedSpans) {
        $End = $Span.start + $Span.size - 1
        $Middle = $Span.start + [Math]::Floor(($Span.size - 1) / 2)
        $CpuBase = if ($Span.fixed) { 0xC000 } else { 0x8000 }
        $Range = ('$' + ("{0:X4}" -f $Span.start) + '-$' +
            ("{0:X4}" -f $End))
        foreach ($Point in @(
            @{ label="start"; cpu=$Span.start },
            @{ label="middle"; cpu=$Middle },
            @{ label="end"; cpu=$End }
        )) {
            $Id = ("{0:X4}-{1}" -f $Span.start, $Point.label)
            $Offset = $Prg + $Span.bank * 0x4000 +
                ($Point.cpu - $CpuBase)
            Invoke-RejectedRomMutation $RomBytes $Id $Offset $Range
            ++$RomMutationCount
        }
    }

    Write-Host ("TPNL-1 focused tests passed: canonical, pure M01/M06/M07 " +
        "rules, shared cue 6 at 16 frames, full-ROM SHA-256, strict " +
        "provenance, malformed/missing/wrong-sized/cross-pack dependencies, " +
        "$RomMutationCount source mutations")
    $global:LASTEXITCODE = 0
} finally {
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
}
