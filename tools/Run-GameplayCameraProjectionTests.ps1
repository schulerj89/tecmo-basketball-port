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
    throw "TGCP-1 tests require the exact Tecmo NBA Basketball Rev1 ROM."
}

$BuildDir = Join-Path $ProjectRoot "build"
$Executable = Join-Path $BuildDir "tecmo_port.exe"
$Scratch = [IO.Path]::GetFullPath(
    (Join-Path $BuildDir "gameplay_camera_projection_test"))
$BuildPrefix = [IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/') +
    [IO.Path]::DirectorySeparatorChar
if (!$Scratch.StartsWith($BuildPrefix,
                         [StringComparison]::OrdinalIgnoreCase)) {
    throw "Gameplay-camera scratch path escaped build\."
}
$PackPath = Join-Path $Scratch "gameplay-camera.assetpack"
$SuccessPrefix = "TGCP-1 gameplay camera self-test passed"
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

function Invoke-CameraAssetTest {
    param(
        [string]$AssetPack,
        [bool]$ExpectSuccess,
        [string]$ExpectedFailure
    )
    $Output = @(& $Executable --gameplay-camera-projection-test `
        $AssetPack 2>&1)
    $ExitCode = $LASTEXITCODE
    $Text = ($Output -join [Environment]::NewLine).Trim()
    if ($ExpectSuccess) {
        if ($ExitCode -ne 0 -or !$Text.StartsWith($SuccessPrefix,
                [StringComparison]::Ordinal)) {
            throw "TGCP-1 loader/API goldens failed.`n$(Get-ShortTail $Output)"
        }
    } elseif ($ExpectedFailure -and
              ($ExitCode -eq 0 -or $Text -ne
                  ("Gameplay camera asset test failed: " +
                   $ExpectedFailure))) {
        throw "TGCP-1 loader failure changed.`n$(Get-ShortTail $Output)"
    } elseif (!$ExpectedFailure -and
              ($ExitCode -eq 0 -or
               $Text -notmatch "TGCP-1|Gameplay camera asset")) {
        throw "Malformed TGCP-1 pack was accepted.`n$(Get-ShortTail $Output)"
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
    Invoke-CameraAssetTest $Path $false
}

function Invoke-RejectedRomMutation {
    param(
        [byte[]]$Original,
        [string]$Id,
        [int]$Offset,
        [string]$ExpectedText,
        [string]$ExpectedSchema = "TGCP-1",
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
            --gameplay-camera-projection-source-test $MutatedRom 2>&1)
    }
    $ExitCode = $LASTEXITCODE
    if (Test-Path -LiteralPath $MutatedPack) {
        Remove-Item -LiteralPath $MutatedPack -Force
    }
    $Text = $Output -join [Environment]::NewLine
    if ($ExitCode -eq 0 -or
        $Text -notmatch [regex]::Escape($ExpectedSchema) -or
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
        --gameplay-camera-projection-source-test $RomPath 2>&1)
    if ($LASTEXITCODE -ne 0 -or
        ($DirectOutput -join [Environment]::NewLine) -notmatch "TGCP-1") {
        throw "Direct Rev1 TGCP-1 source test failed.`n$(Get-ShortTail $DirectOutput)"
    }
    $PackOutput = @(& $Executable --build-assetpack `
        $RomPath $PackPath 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "Rev1 TGCP-1 asset-pack build failed.`n$(Get-ShortTail $PackOutput)"
    }
    $PackBytes = [IO.File]::ReadAllBytes($PackPath)
    $CameraEntry = Get-AssetPackEntry $PackBytes `
        "gameplay/camera-projection"
    $GameplayEntry = Get-AssetPackEntry $PackBytes "gameplay/core"
    $CourtEntry = Get-AssetPackEntry $PackBytes "gameplay/court"
    $SourceMapEntry = Get-AssetPackEntry $PackBytes "system/source-map"
    $Payload = Get-EntryBytes $PackBytes $CameraEntry
    if ($CameraEntry.byte_count -ne 1344 -or
        (Get-Fnv1a32 $Payload) -ne "B3721B17") {
        throw "gameplay/camera-projection size or fingerprint changed."
    }
    Invoke-CameraAssetTest $PackPath $true

    $ListOutput = @(& $Executable --assetpack-list $PackPath 2>&1)
    if ($LASTEXITCODE -ne 0 -or
        @($ListOutput | Where-Object {
            $_ -match '^gameplay/camera-projection\s' -and
            $_ -match 'bank=7' -and $_ -match 'cpu=0xDE13' -and
            $_ -match 'bytes=1344'
        }).Count -ne 1) {
        throw "Asset-pack listing omitted the exact TGCP-1 entry.`n$(Get-ShortTail $ListOutput)"
    }

    $ExpectedSpans = @(
        @{ start=0xDE13; size=26;  hash="A5CF7665"; payload=448 },
        @{ start=0xDF05; size=251; hash="7BC5351D"; payload=480 },
        @{ start=0xE0E7; size=85;  hash="7FE800D4"; payload=736 },
        @{ start=0xE168; size=383; hash="19038AEA"; payload=832 },
        @{ start=0xEB4F; size=62;  hash="AF5725C0"; payload=1216 },
        @{ start=0xF1CB; size=39;  hash="CB8BD081"; payload=1280 }
    )
    $SourceMap = ([Text.Encoding]::UTF8.GetString(
        (Get-EntryBytes $PackBytes $SourceMapEntry))) | ConvertFrom-Json
    $Maps = @($SourceMap.logical_entries | Where-Object {
        $_.id -eq "gameplay/camera-projection"
    })
    $MapOk = $Maps.Count -eq 1
    if ($MapOk) {
        $Map = $Maps[0]
        $MapOk =
            $Map.schema -eq "tecmo.gameplay-camera/TGCP-1" -and
            $Map.size -eq 1344 -and
            $Map.fingerprint_fnv1a32 -eq "B3721B17" -and
            $Map.revision_sha256_identity -eq
                "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4" -and
            $Map.revision_full_rom_fnv1a32 -eq "0650F5B0" -and
            [bool]$Map.revision_full_rom_fnv1a32_verified -and
            [bool]$Map.revision_source_fingerprints_verified -and
            @($Map.dependencies).Count -eq 2 -and
            $Map.dependencies[0].entry -eq "gameplay/core" -and
            $Map.dependencies[0].size -eq 23416 -and
            $Map.dependencies[0].fingerprint_fnv1a32 -eq "2047CCE0" -and
            $Map.dependencies[1].entry -eq "gameplay/court" -and
            $Map.dependencies[1].size -eq 6559 -and
            $Map.dependencies[1].fingerprint_fnv1a32 -eq "ECAB7A93" -and
            @($Map.source_spans).Count -eq 6 -and
            $Map.state_contract.camera_x -eq '$01:$00' -and
            $Map.state_contract.focus_world_x -eq '$F2:$7D' -and
            $Map.follow_contract.normal_speed_cap -eq 7 -and
            $Map.follow_contract.endpoint_speed_cap -eq 2 -and
            $Map.follow_contract.left_cursor_bound -eq 12 -and
            $Map.follow_contract.right_cursor_bound -eq 52 -and
            (@($Map.follow_contract.suppressed_action_routes) -join ',') -eq
                "1,18,19" -and
            $Map.projection_contract.visible -match "0..255 viewport" -and
            $Map.projection_contract.orientation_transform -eq $false -and
            $Map.projection_contract.vertical_camera -eq $false -and
            $Map.supported_boundary -match "no PPU commit" -and
            $Map.supported_boundary -match "no .*live-scene mutation"
        if ($MapOk) {
            for ($Index = 0; $Index -lt $ExpectedSpans.Count; ++$Index) {
                $Expected = $ExpectedSpans[$Index]
                $Actual = $Map.source_spans[$Index]
                $ExpectedOffset = [uint64]$SourceMap.source.prg_offset +
                    7 * 0x4000 + ($Expected.start - 0xC000)
                if ($Actual.source_entry -ne "prg/fixed" -or
                    $Actual.bank -ne 7 -or
                    ![bool]$Actual.fixed_bank -or
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
        throw "TGCP-1 source-map provenance is incomplete or malformed."
    }

    foreach ($Mutation in @(
        @{ id="magic"; offset=0 },
        @{ id="version"; offset=4 },
        @{ id="header-size"; offset=6 },
        @{ id="declared-size"; offset=8 },
        @{ id="source-count"; offset=12 },
        @{ id="source-stride"; offset=14 },
        @{ id="sources-offset"; offset=16 },
        @{ id="core-size"; offset=20 },
        @{ id="core-fingerprint"; offset=24 },
        @{ id="court-size"; offset=28 },
        @{ id="court-fingerprint"; offset=32 },
        @{ id="revision-size"; offset=36 },
        @{ id="revision-fnv"; offset=40 },
        @{ id="revision-sha"; offset=44 },
        @{ id="source-descriptor"; offset=76 },
        @{ id="state-contract"; offset=148 },
        @{ id="threshold-table-copy"; offset=164 },
        @{ id="header-reserved"; offset=170 },
        @{ id="source-record"; offset=256 },
        @{ id="source-record-fixed"; offset=259 },
        @{ id="source-record-reserved"; offset=276 },
        @{ id="initialize-source"; offset=448 },
        @{ id="initialize-padding"; offset=474 },
        @{ id="stream-source"; offset=480 },
        @{ id="stream-padding"; offset=731 },
        @{ id="attribute-source"; offset=736 },
        @{ id="attribute-padding"; offset=821 },
        @{ id="follow-source"; offset=832 },
        @{ id="follow-padding"; offset=1215 },
        @{ id="settle-source"; offset=1216 },
        @{ id="settle-padding"; offset=1278 },
        @{ id="projection-source"; offset=1280 },
        @{ id="trailing-reserved"; offset=1319 }
    )) {
        Write-PayloadMutationAndReject $PackBytes $CameraEntry `
            $Mutation.id $Mutation.offset
    }

    foreach ($Case in @(
        @{ id="undersized-camera"; entry=$CameraEntry; size=1343;
           status="TGCP-1 gameplay/camera-projection entry missing or wrong-sized" },
        @{ id="oversized-camera"; entry=$CameraEntry; size=1345;
           status="TGCP-1 gameplay/camera-projection entry missing or wrong-sized" },
        @{ id="undersized-core"; entry=$GameplayEntry; size=23415;
           status="TGCP-1 gameplay/core dependency missing or wrong-sized" },
        @{ id="oversized-core"; entry=$GameplayEntry; size=23417;
           status="TGCP-1 gameplay/core dependency missing or wrong-sized" },
        @{ id="undersized-court"; entry=$CourtEntry; size=6558;
           status="TGCP-1 gameplay/court dependency missing or wrong-sized" },
        @{ id="oversized-court"; entry=$CourtEntry; size=6560;
           status="TGCP-1 gameplay/court dependency missing or wrong-sized" }
    )) {
        $Path = Join-Path $Scratch ($Case.id + ".assetpack")
        $Bytes = [byte[]]$PackBytes.Clone()
        [BitConverter]::GetBytes([uint64]$Case.size).CopyTo(
            $Bytes, [int]$Case.entry.directory_offset + 92)
        [IO.File]::WriteAllBytes($Path, $Bytes)
        Invoke-CameraAssetTest $Path $false $Case.status
    }
    foreach ($Case in @(
        @{ id="missing-camera"; entry=$CameraEntry;
           status="TGCP-1 gameplay/camera-projection entry missing or wrong-sized" },
        @{ id="missing-core"; entry=$GameplayEntry;
           status="TGCP-1 gameplay/core dependency missing or wrong-sized" },
        @{ id="missing-court"; entry=$CourtEntry;
           status="TGCP-1 gameplay/court dependency missing or wrong-sized" }
    )) {
        $Path = Join-Path $Scratch ($Case.id + ".assetpack")
        $Bytes = [byte[]]$PackBytes.Clone()
        $Bytes[[int]$Case.entry.directory_offset] = [byte][char]'x'
        [IO.File]::WriteAllBytes($Path, $Bytes)
        Invoke-CameraAssetTest $Path $false $Case.status
    }
    foreach ($Case in @(
        @{ id="malformed-core"; entry=$GameplayEntry; offset=0 },
        @{ id="cross-pack-core"; entry=$GameplayEntry; offset=184 },
        @{ id="malformed-court"; entry=$CourtEntry; offset=0 },
        @{ id="cross-pack-court"; entry=$CourtEntry; offset=256 }
    )) {
        $Path = Join-Path $Scratch ($Case.id + ".assetpack")
        $Bytes = [byte[]]$PackBytes.Clone()
        $Absolute = [int]$Case.entry.pack_offset + $Case.offset
        $Bytes[$Absolute] = $Bytes[$Absolute] -bxor 1
        [IO.File]::WriteAllBytes($Path, $Bytes)
        Invoke-CameraAssetTest $Path $false
    }

    $RomBytes = [IO.File]::ReadAllBytes($RomPath)
    # The combined builder may reject through an earlier strict logical asset.
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
            $Offset = $Prg + 7 * 0x4000 + ($Point.cpu - 0xC000)
            Invoke-RejectedRomMutation $RomBytes $Id $Offset $Range
            ++$RomMutationCount
        }
    }

    Write-Host ("TGCP-1 focused tests passed: direct exact Rev1 iNES/FNV/SHA, " +
        "six canonical fixed-bank spans, strict source map, camera follow/" +
        "settle/projection goldens, missing/malformed/undersized/oversized/" +
        "cross-pack dependency rejection, $RomMutationCount source mutations")
    $global:LASTEXITCODE = 0
} finally {
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
}
