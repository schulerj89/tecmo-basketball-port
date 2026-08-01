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
    throw "TGCP-2 tests require the exact Tecmo NBA Basketball Rev1 ROM."
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
$SuccessPrefix = "TGCP-2 gameplay camera self-test passed"
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
            throw "TGCP-2 loader/API goldens failed.`n$(Get-ShortTail $Output)"
        }
    } elseif ($ExpectedFailure -and
              ($ExitCode -eq 0 -or $Text -ne
                  ("Gameplay camera asset test failed: " +
                   $ExpectedFailure))) {
        throw "TGCP-2 loader failure changed.`n$(Get-ShortTail $Output)"
    } elseif (!$ExpectedFailure -and
              ($ExitCode -eq 0 -or
               $Text -notmatch "TGCP-2|Gameplay camera asset")) {
        throw "Malformed TGCP-2 pack was accepted.`n$(Get-ShortTail $Output)"
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
        [string]$ExpectedSchema = "TGCP-2",
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
        ($DirectOutput -join [Environment]::NewLine) -notmatch "TGCP-2") {
        throw "Direct Rev1 TGCP-2 source test failed.`n$(Get-ShortTail $DirectOutput)"
    }
    $PackOutput = @(& $Executable --build-assetpack `
        $RomPath $PackPath 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "Rev1 TGCP-2 asset-pack build failed.`n$(Get-ShortTail $PackOutput)"
    }
    $PackBytes = [IO.File]::ReadAllBytes($PackPath)
    $CameraEntry = Get-AssetPackEntry $PackBytes `
        "gameplay/camera-projection"
    $GameplayEntry = Get-AssetPackEntry $PackBytes "gameplay/core"
    $CourtEntry = Get-AssetPackEntry $PackBytes "gameplay/court"
    $SourceMapEntry = Get-AssetPackEntry $PackBytes "system/source-map"
    $Payload = Get-EntryBytes $PackBytes $CameraEntry
    if ($CameraEntry.byte_count -ne 1536 -or
        (Get-Fnv1a32 $Payload) -ne "53247856") {
        throw "gameplay/camera-projection size or fingerprint changed."
    }
    Invoke-CameraAssetTest $PackPath $true
    $IntegrationOutput = @(& $Executable `
        --gameplay-free-throw-projection-test $PackPath 2>&1)
    if ($LASTEXITCODE -ne 0 -or
        ($IntegrationOutput -join [Environment]::NewLine) -notmatch
            "^TGFL-1 -> TGCP-2 projection test passed") {
        throw "TGFL-1 -> TGCP-2 integration test failed.`n$(Get-ShortTail $IntegrationOutput)"
    }

    $ListOutput = @(& $Executable --assetpack-list $PackPath 2>&1)
    if ($LASTEXITCODE -ne 0 -or
        @($ListOutput | Where-Object {
            $_ -match '^gameplay/camera-projection\s' -and
            $_ -match 'bank=7' -and $_ -match 'cpu=0xDE13' -and
            $_ -match 'bytes=1536'
        }).Count -ne 1) {
        throw "Asset-pack listing omitted the exact TGCP-2 entry.`n$(Get-ShortTail $ListOutput)"
    }

    $ExpectedSpans = @(
        @{ start=0xDE13; size=26;  hash="A5CF7665"; payload=480 },
        @{ start=0xDF05; size=251; hash="7BC5351D"; payload=512 },
        @{ start=0xE0E7; size=85;  hash="7FE800D4"; payload=768 },
        @{ start=0xE168; size=383; hash="19038AEA"; payload=864 },
        @{ start=0xEB4F; size=62;  hash="AF5725C0"; payload=1248 },
        @{ start=0xF1CB; size=39;  hash="CB8BD081"; payload=1312 },
        @{ start=0xF106; size=171; hash="CB1D4EAF"; payload=1360;
           sha="0B97A9AAC4DF35E4EDF7979C6C0355852B9DE7398844B2679CFAB298F0C0CBA6" }
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
            $Map.schema -eq "tecmo.gameplay-camera/TGCP-2" -and
            $Map.size -eq 1536 -and
            $Map.fingerprint_fnv1a32 -eq "53247856" -and
            $Map.revision_sha256_identity -eq
                "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4" -and
            $Map.revision_full_rom_fnv1a32 -eq "0650F5B0" -and
            [bool]$Map.revision_full_rom_fnv1a32_verified -and
            [bool]$Map.revision_source_fingerprints_verified -and
            @($Map.dependencies).Count -eq 2 -and
            $Map.dependencies[0].entry -eq "gameplay/core" -and
            [bool]$Map.dependencies[0].same_pack_required -and
            $Map.dependencies[0].size -eq 23416 -and
            $Map.dependencies[0].fingerprint_fnv1a32 -eq "2047CCE0" -and
            $Map.dependencies[1].entry -eq "gameplay/court" -and
            [bool]$Map.dependencies[1].same_pack_required -and
            $Map.dependencies[1].size -eq 6559 -and
            $Map.dependencies[1].fingerprint_fnv1a32 -eq "ECAB7A93" -and
            @($Map.source_spans).Count -eq 7 -and
            $Map.state_contract.camera_x -eq '$01:$00' -and
            $Map.state_contract.focus_world_x -eq '$F2:$7D' -and
            $Map.state_contract.pure_initialize.layout_cursor -eq '$20' -and
            $Map.state_contract.live_prime.layout_cursor -eq '$21' -and
            [bool]$Map.state_contract.live_prime.single_use -and
            $Map.follow_contract.normal_speed_cap -eq 7 -and
            $Map.follow_contract.endpoint_speed_cap -eq 2 -and
            $Map.follow_contract.left_cursor_bound -eq 12 -and
            $Map.follow_contract.right_cursor_bound -eq 52 -and
            $Map.follow_contract.live_state_validator.right_cursor -eq
                'min((camera_x >> 3)+1,$34)' -and
            $Map.follow_contract.live_state_validator.left_cursor -eq
                'max((camera_x >> 3)-1,$0B)' -and
            (@($Map.follow_contract.live_state_validator.unlatched_thresholds) -join ',') -eq
                '80,160' -and
            (@($Map.follow_contract.live_state_validator.left_endpoint_thresholds) -join ',') -eq
                '216,232' -and
            (@($Map.follow_contract.live_state_validator.right_endpoint_thresholds) -join ',') -eq
                '32,4' -and
            [bool]$Map.follow_contract.live_state_validator.thresholds_invalid_requires_latch_clear -and
            (@($Map.follow_contract.suppressed_action_routes) -join ',') -eq
                "1,18,19" -and
            $Map.projection_contract.visible -match "0..255 viewport" -and
            $Map.projection_contract.offscreen_sentinel -match
                "screen_x=0, screen_y=0" -and
            $Map.projection_contract.screen_y -match "visible actors" -and
            $Map.projection_contract.orientation_transform -eq $false -and
            $Map.projection_contract.vertical_camera -eq $false -and
            $Map.live_runtime_contract.asset_pack -match "TGFL-1" -and
            $Map.live_runtime_contract.update -match
                "exactly one route-zero follow" -and
            $Map.live_runtime_contract.freeze -match
                "TGFL-1 typed forced settle.*TGDK" -and
            $Map.live_runtime_contract.possession -match
                "camera position is continuous" -and
            $Map.live_runtime_contract.viewport -match
                "32/33-column slice" -and
            $Map.live_runtime_contract.court_slice -match
                "TGOR possession/direction/transition serial" -and
            $Map.live_runtime_contract.court_slice -match
                "TGCP projection camera_x" -and
            [bool]$Map.live_runtime_contract.court_frame.transactional -and
            $Map.live_runtime_contract.court_frame.composition -match
                "TGCT possession slice.*TGCP player/ball projection" -and
            $Map.live_runtime_contract.court_frame.stationary_actor_x -match
                "inverse signed camera_x delta" -and
            $Map.live_runtime_contract.court_frame.stationary_actor_y -match
                "unchanged by horizontal camera motion" -and
            $Map.live_runtime_contract.court_frame.visibility_transition -match
                "visible=false with zero X/Y" -and
            (@($Map.live_runtime_contract.court_frame.covered_transitions) -join
                ',') -eq
                "fine-scroll,coarse-tile,possession-reversal,left-endpoint,right-endpoint" -and
            ![bool]$Map.live_runtime_contract.court_frame.integration_is_additional_rom_claim -and
            $Map.live_runtime_contract.coordinate_space.origin -match
                "upper-left" -and
            (@($Map.live_runtime_contract.coordinate_space.integer_bounds[0]) -join
                ',') -eq "0,767" -and
            (@($Map.live_runtime_contract.coordinate_space.integer_bounds[1]) -join
                ',') -eq "0,239" -and
            $Map.live_runtime_contract.coordinate_space.ball_fractional_bits -eq
                8 -and
            $Map.live_runtime_contract.coordinate_adapter.follow -match
                "floor canonical Q8" -and
            $Map.live_runtime_contract.coordinate_adapter.projection -match
                "transactional TGCP projection adapters" -and
            $Map.live_runtime_contract.coordinate_adapter.scene_snapshot -match
                "ten player projections" -and
            ![bool]$Map.live_runtime_contract.coordinate_adapter.adapter_is_additional_rom_claim -and
            $Map.live_runtime_contract.projection -match
                "one canonical scene snapshot" -and
            $Map.live_runtime_contract.shot_target.orientation_0_x -eq 160 -and
            $Map.live_runtime_contract.shot_target.orientation_1_x -eq 608 -and
            $Map.live_runtime_contract.shot_target.hoop_y -eq 148 -and
            $Map.live_runtime_contract.shot_target.launch_y -eq 143 -and
            [bool]$Map.live_runtime_contract.shot_target.hoop_and_flight_y_are_distinct -and
            [bool]$Map.ordinary_movement_geometry.strict_tgcp2_source -and
            $Map.ordinary_movement_geometry.source_kind -eq
                "ordinary-actor-dispatch-and-clamp" -and
            $Map.ordinary_movement_geometry.source_entry -eq "prg/fixed" -and
            $Map.ordinary_movement_geometry.source_offset -eq 127254 -and
            $Map.ordinary_movement_geometry.bank -eq 7 -and
            [bool]$Map.ordinary_movement_geometry.fixed_bank -and
            $Map.ordinary_movement_geometry.cpu_start -eq 0xF106 -and
            $Map.ordinary_movement_geometry.cpu_end -eq 0xF1B0 -and
            $Map.ordinary_movement_geometry.size -eq 171 -and
            $Map.ordinary_movement_geometry.fingerprint_fnv1a32 -eq
                "CB1D4EAF" -and
            $Map.ordinary_movement_geometry.fingerprint_sha256 -eq
                "0B97A9AAC4DF35E4EDF7979C6C0355852B9DE7398844B2679CFAB298F0C0CBA6" -and
            $Map.ordinary_movement_geometry.payload_offset -eq 1360 -and
            (@($Map.ordinary_movement_geometry.dispatcher_exceptions_not_implemented) -join ',') -eq
                '$0478,$046E,$0588,$0463' -and
            $Map.ordinary_movement_geometry.boundary_settlement -match
                'TPNL-1 as OUT OF BOUNDS' -and
            $Map.supported_boundary -match "production live camera" -and
            $Map.supported_boundary -match "no staged PPU" -and
            $Map.supported_boundary -match
                "typed free-throw settle/projection of TGFL-1-owned coordinates" -and
            $Map.supported_boundary -match
                "does not own TGFL-1 positions"
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
                    $Actual.payload_offset -ne $Expected.payload -or
                    ($Expected.sha -and
                     $Actual.fingerprint_sha256 -ne $Expected.sha)) {
                    $MapOk = $false
                    break
                }
            }
        }
    }
    if (!$MapOk) {
        throw "TGCP-2 source-map provenance is incomplete or malformed."
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
        @{ id="clamp-header-descriptor-start"; offset=148 },
        @{ id="clamp-header-descriptor-middle"; offset=154 },
        @{ id="clamp-header-descriptor-end"; offset=159 },
        @{ id="state-contract"; offset=160 },
        @{ id="threshold-table-copy"; offset=176 },
        @{ id="header-pre-sha-reserved"; offset=182 },
        @{ id="clamp-sha"; offset=184 },
        @{ id="header-reserved"; offset=216 },
        @{ id="source-record"; offset=256 },
        @{ id="source-record-fixed"; offset=259 },
        @{ id="source-record-reserved"; offset=276 },
        @{ id="clamp-source-record-start"; offset=448 },
        @{ id="clamp-source-record-middle"; offset=463 },
        @{ id="clamp-source-record-end"; offset=479 },
        @{ id="initialize-source"; offset=480 },
        @{ id="initialize-padding"; offset=506 },
        @{ id="stream-source"; offset=512 },
        @{ id="stream-padding"; offset=763 },
        @{ id="attribute-source"; offset=768 },
        @{ id="attribute-padding"; offset=853 },
        @{ id="follow-source"; offset=864 },
        @{ id="follow-padding"; offset=1247 },
        @{ id="settle-source"; offset=1248 },
        @{ id="settle-padding"; offset=1310 },
        @{ id="projection-source"; offset=1312 },
        @{ id="projection-padding"; offset=1351 },
        @{ id="clamp-payload-start"; offset=1360 },
        @{ id="clamp-payload-middle"; offset=1445 },
        @{ id="clamp-payload-end"; offset=1530 },
        @{ id="trailing-reserved"; offset=1531 }
    )) {
        Write-PayloadMutationAndReject $PackBytes $CameraEntry `
            $Mutation.id $Mutation.offset
    }

    foreach ($Case in @(
        @{ id="undersized-camera"; entry=$CameraEntry; size=1535;
           status="TGCP-2 gameplay/camera-projection entry missing or wrong-sized" },
        @{ id="oversized-camera"; entry=$CameraEntry; size=1537;
           status="TGCP-2 gameplay/camera-projection entry missing or wrong-sized" },
        @{ id="undersized-core"; entry=$GameplayEntry; size=23415;
           status="TGCP-2 gameplay/core dependency missing or wrong-sized" },
        @{ id="oversized-core"; entry=$GameplayEntry; size=23417;
           status="TGCP-2 gameplay/core dependency missing or wrong-sized" },
        @{ id="undersized-court"; entry=$CourtEntry; size=6558;
           status="TGCP-2 gameplay/court dependency missing or wrong-sized" },
        @{ id="oversized-court"; entry=$CourtEntry; size=6560;
           status="TGCP-2 gameplay/court dependency missing or wrong-sized" }
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
           status="TGCP-2 gameplay/camera-projection entry missing or wrong-sized" },
        @{ id="missing-core"; entry=$GameplayEntry;
           status="TGCP-2 gameplay/core dependency missing or wrong-sized" },
        @{ id="missing-court"; entry=$CourtEntry;
           status="TGCP-2 gameplay/court dependency missing or wrong-sized" }
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

    Write-Host ("TGCP-2 focused tests passed: direct exact Rev1 iNES/FNV/SHA, " +
        "seven canonical fixed-bank spans including clamp SHA, strict source map, camera follow/" +
        "settle/projection goldens, live-prime/endpoints/state invariants, " +
        "TGFL-derived slot-3 integration, " +
        "missing/malformed/undersized/oversized/" +
        "cross-pack dependency rejection, $RomMutationCount source mutations")
    $global:LASTEXITCODE = 0
} finally {
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
}
