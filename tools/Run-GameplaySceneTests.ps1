param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [switch]$Build
)

$ErrorActionPreference = "Stop"

$ExpectedRomSha256 =
    "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4"

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
if ((Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash -ne
    $ExpectedRomSha256) {
    throw "Gameplay scene tests require the supported Rev1 ROM fingerprint."
}

$BuildDir = Join-Path $ProjectRoot "build"
$Executable = Join-Path $BuildDir "tecmo_port.exe"
$Scratch = [IO.Path]::GetFullPath((Join-Path $BuildDir "gameplay_scene_test"))
$BuildPrefix = [IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/') +
    [IO.Path]::DirectorySeparatorChar
if (!$Scratch.StartsWith($BuildPrefix,
                         [StringComparison]::OrdinalIgnoreCase)) {
    throw "Gameplay scene scratch path escaped build\."
}
$PackPath = Join-Path $Scratch "gameplay-scene.assetpack"
$PreviousPack = $env:TECMO_ASSETPACK
$PreviousSkipShortcut = $env:TECMO_SKIP_SHORTCUT

function Get-ShortTail {
    param([string]$Path)
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { return "" }
    return (@(Get-Content -LiteralPath $Path | Select-Object -Last 12) -join
        [Environment]::NewLine)
}

function Invoke-Logged {
    param(
        [string]$Command,
        [string[]]$Arguments,
        [string]$LogPath
    )
    & $Command @Arguments *> $LogPath
    return [pscustomobject]@{
        exit_code = $LASTEXITCODE
        tail = Get-ShortTail $LogPath
    }
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
            $ByteCount -gt [uint64]$Bytes.Length - $PackOffset -or
            $ByteCount -gt [int]::MaxValue) {
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
                  $Result, 0, [int64]$Entry.byte_count)
    return $Result
}

function Assert-SceneRejected {
    param(
        [string]$AssetPack,
        [string]$Label,
        [string]$ExpectedStatus
    )
    $Log = Join-Path $Scratch ("reject-{0}.log" -f $Label)
    $Run = Invoke-Logged -Command $Executable -Arguments @(
        "--root", $ProjectRoot, "--gameplay-scene-test", $AssetPack
    ) -LogPath $Log
    if ($Run.exit_code -eq 0 -or
        $Run.tail -notmatch "Gameplay scene test failed" -or
        ($ExpectedStatus -and $Run.tail -notmatch $ExpectedStatus)) {
        throw "Gameplay scene accepted $Label fixture.`n$($Run.tail)"
    }
}

function Get-PngDimensions {
    param([string]$Path)
    $Bytes = [IO.File]::ReadAllBytes($Path)
    [byte[]]$Signature = 137,80,78,71,13,10,26,10
    if ($Bytes.Length -lt 24) { throw "PNG '$Path' is truncated." }
    for ($Index = 0; $Index -lt $Signature.Length; ++$Index) {
        if ($Bytes[$Index] -ne $Signature[$Index]) {
            throw "PNG '$Path' has an invalid signature."
        }
    }
    if ([Text.Encoding]::ASCII.GetString($Bytes, 12, 4) -ne "IHDR") {
        throw "PNG '$Path' has no leading IHDR chunk."
    }
    [uint32]$Width = ([uint32]$Bytes[16] -shl 24) -bor
        ([uint32]$Bytes[17] -shl 16) -bor
        ([uint32]$Bytes[18] -shl 8) -bor [uint32]$Bytes[19]
    [uint32]$Height = ([uint32]$Bytes[20] -shl 24) -bor
        ([uint32]$Bytes[21] -shl 16) -bor
        ([uint32]$Bytes[22] -shl 8) -bor [uint32]$Bytes[23]
    return [pscustomobject]@{ width = $Width; height = $Height }
}

function Invoke-RenderCheckpoint {
    param([string]$Mode, [string]$ExpectedState)
    $SafeName = $Mode -replace '[^A-Za-z0-9_-]', '_'
    $Hashes = @()
    for ($Pass = 1; $Pass -le 2; ++$Pass) {
        $Png = Join-Path $Scratch ("{0}-{1}.png" -f $SafeName, $Pass)
        $Log = Join-Path $Scratch ("render-{0}-{1}.log" -f $SafeName, $Pass)
        $Run = Invoke-Logged -Command $Executable -Arguments @(
            "--root", $ProjectRoot, "--render-test-mode", $Mode, $Png
        ) -LogPath $Log
        if ($Run.exit_code -ne 0 -or !(Test-Path -LiteralPath $Png) -or
            $Run.tail -notmatch $ExpectedState) {
            throw "Gameplay render '$Mode' failed.`n$($Run.tail)"
        }
        $Dimensions = Get-PngDimensions $Png
        if ($Dimensions.width -ne 640 -or $Dimensions.height -ne 480) {
            throw "Gameplay render '$Mode' is not 640x480."
        }
        $Hashes += (Get-FileHash -LiteralPath $Png -Algorithm SHA256).Hash
    }
    if ($Hashes[0] -ne $Hashes[1]) {
        throw "Gameplay render '$Mode' is not deterministic."
    }
    return $Hashes[0]
}

try {
    $env:TECMO_SKIP_SHORTCUT = "1"
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
    if ($Build) {
        $BuildLog = Join-Path $BuildDir "gameplay-scene-build.log"
        $BuildRun = Invoke-Logged `
            -Command (Join-Path $ProjectRoot "build.ps1") `
            -Arguments @() -LogPath $BuildLog
        if ($BuildRun.exit_code -ne 0 -or
            @(Select-String -LiteralPath $BuildLog `
                -Pattern 'warning [A-Z]+[0-9]+:').Count -ne 0) {
            throw "Warning-free gameplay scene build failed.`n$($BuildRun.tail)"
        }
    }
    if (!(Test-Path -LiteralPath $Executable -PathType Leaf)) {
        throw "Build output is missing; rerun with -Build."
    }
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
    New-Item -ItemType Directory -Path $Scratch | Out-Null

    $PackLog = Join-Path $Scratch "build-assetpack.log"
    $PackRun = Invoke-Logged -Command $Executable -Arguments @(
        "--build-assetpack", $RomPath, $PackPath
    ) -LogPath $PackLog
    if ($PackRun.exit_code -ne 0 -or
        !(Test-Path -LiteralPath $PackPath -PathType Leaf)) {
        throw "Strict gameplay asset-pack build failed.`n$($PackRun.tail)"
    }

    $PackBytes = [IO.File]::ReadAllBytes($PackPath)
    $Specs = @(
        [pscustomobject]@{ id="gameplay/core"; size=23416; hash="2047CCE0"; schema="tecmo.gameplay/TGPL-1" },
        [pscustomobject]@{ id="gameplay/pre-tip"; size=5888; hash="99ADFE3D"; schema="tecmo.gameplay-pre-tip/TPTI-1" },
        [pscustomobject]@{ id="gameplay/court"; size=6559; hash="ECAB7A93"; schema="tecmo.gameplay-court/TGCT-1" },
        [pscustomobject]@{ id="gameplay/camera-projection"; size=1536; hash="53247856"; schema="tecmo.gameplay-camera/TGCP-2" },
        [pscustomobject]@{ id="gameplay/court-orientation"; size=640; hash="F9152C0A"; schema="tecmo.gameplay-court-orientation/TGOR-1" },
        [pscustomobject]@{ id="gameplay/free-throw-lineup"; size=1216; hash="B17B9A3F"; schema="tecmo.gameplay-free-throw-lineup/TGFL-1" },
        [pscustomobject]@{ id="gameplay/close-shots"; size=3144; hash="DACDC976"; schema="tecmo.gameplay-close-shots/TGCS-1" },
        [pscustomobject]@{ id="gameplay/dunk-cutaway"; size=20272; hash="E02F2D21"; schema="tecmo.gameplay-dunk-cutaway/TGDK-1" },
        [pscustomobject]@{ id="gameplay/jump-shots"; size=2776; hash="A66EE873"; schema="tecmo.gameplay-jump-shots/TGJS-2" },
        [pscustomobject]@{ id="gameplay/shot-resolution"; size=512; hash="164DC568"; schema="tecmo.gameplay-shot-resolution/TGSR-3" },
        [pscustomobject]@{ id="audio/music"; size=36784; hash="05C00ECB"; schema="tecmo.music/TMUS-1" },
        [pscustomobject]@{ id="audio/gameplay-sfx"; size=2824; hash="968A5DE6"; schema="tecmo.gameplay-audio/TSFX-1" },
        [pscustomobject]@{ id="audio/gameplay-dmc"; size=2515; hash="AD70E6E8"; schema="tecmo.gameplay-audio/TDMC-1" },
        [pscustomobject]@{ id="chr/all"; size=262144; hash="F6F6E854"; schema=$null }
    )
    $Entries = @{}
    foreach ($Spec in $Specs) {
        $Entry = Get-AssetPackEntry $PackBytes $Spec.id
        $Payload = Get-EntryBytes $PackBytes $Entry
        if ($Entry.byte_count -ne $Spec.size -or
            (Get-Fnv1a32 $Payload) -ne $Spec.hash) {
            throw "Asset '$($Spec.id)' size or canonical fingerprint changed."
        }
        $Entries[$Spec.id] = $Entry
    }
    $SourceMapEntry = Get-AssetPackEntry $PackBytes "system/source-map"
    $SourceMapText = [Text.Encoding]::UTF8.GetString(
        (Get-EntryBytes $PackBytes $SourceMapEntry))
    $SourceMap = $SourceMapText | ConvertFrom-Json
    foreach ($Spec in @($Specs | Where-Object { $_.schema })) {
        $Mapped = @($SourceMap.logical_entries | Where-Object {
            $_.id -eq $Spec.id
        })
        if ($Mapped.Count -ne 1 -or $Mapped[0].schema -ne $Spec.schema) {
            throw "Source-map provenance for '$($Spec.id)' is missing or malformed."
        }
    }
    $CameraMaps = @($SourceMap.logical_entries | Where-Object {
        $_.id -eq "gameplay/camera-projection"
    })
    $CourtMaps = @($SourceMap.logical_entries | Where-Object {
        $_.id -eq "gameplay/court"
    })
    $LineupMaps = @($SourceMap.logical_entries | Where-Object {
        $_.id -eq "gameplay/free-throw-lineup"
    })
    if ($CameraMaps.Count -ne 1 -or $CourtMaps.Count -ne 1 -or
        ![bool]$CameraMaps[0].dependencies[0].same_pack_required -or
        ![bool]$CameraMaps[0].dependencies[1].same_pack_required -or
        $CameraMaps[0].state_contract.pure_initialize.layout_cursor -ne '$20' -or
        $CameraMaps[0].state_contract.live_prime.layout_cursor -ne '$21' -or
        @($CameraMaps[0].source_spans).Count -ne 7 -or
        ![bool]$CameraMaps[0].ordinary_movement_geometry.strict_tgcp2_source -or
        $CameraMaps[0].ordinary_movement_geometry.payload_offset -ne 1360 -or
        $CameraMaps[0].ordinary_movement_geometry.fingerprint_sha256 -ne
            "0B97A9AAC4DF35E4EDF7979C6C0355852B9DE7398844B2679CFAB298F0C0CBA6" -or
        $CameraMaps[0].live_runtime_contract.asset_pack -notmatch "TGFL-1" -or
        $CameraMaps[0].live_runtime_contract.update -notmatch
            "exactly one route-zero follow" -or
        $CameraMaps[0].live_runtime_contract.viewport -notmatch
            "32/33-column slice" -or
        $CameraMaps[0].live_runtime_contract.court_slice -notmatch
            "TGOR possession/direction/transition serial" -or
        $CameraMaps[0].live_runtime_contract.court_slice -notmatch
            "TGCP projection camera_x" -or
        ![bool]$CameraMaps[0].live_runtime_contract.court_frame.transactional -or
        $CameraMaps[0].live_runtime_contract.court_frame.stationary_actor_x -notmatch
            "inverse signed camera_x delta" -or
        $CameraMaps[0].live_runtime_contract.court_frame.stationary_actor_y -notmatch
            "unchanged by horizontal camera motion" -or
        $CameraMaps[0].live_runtime_contract.court_frame.visibility_transition -notmatch
            "visible=false with zero X/Y" -or
        [bool]$CameraMaps[0].live_runtime_contract.court_frame.integration_is_additional_rom_claim -or
        $CameraMaps[0].live_runtime_contract.coordinate_space.origin -notmatch
            "upper-left" -or
        $CameraMaps[0].live_runtime_contract.coordinate_space.ball_fractional_bits -ne
            8 -or
        $CameraMaps[0].live_runtime_contract.coordinate_adapter.follow -notmatch
            "floor canonical Q8" -or
        $CameraMaps[0].live_runtime_contract.coordinate_adapter.projection -notmatch
            "transactional TGCP projection adapters" -or
        $CameraMaps[0].live_runtime_contract.coordinate_adapter.scene_snapshot -notmatch
            "ten player projections" -or
        [bool]$CameraMaps[0].live_runtime_contract.coordinate_adapter.adapter_is_additional_rom_claim -or
        $CameraMaps[0].live_runtime_contract.projection -notmatch
            "one canonical scene snapshot" -or
        $CameraMaps[0].live_runtime_contract.shot_target.hoop_y -ne 148 -or
        $CameraMaps[0].live_runtime_contract.shot_target.launch_y -ne 143 -or
        ![bool]$CameraMaps[0].live_runtime_contract.shot_target.hoop_and_flight_y_are_distinct -or
        $CameraMaps[0].ordinary_movement_geometry.cpu_start -ne 0xF106 -or
        $CameraMaps[0].ordinary_movement_geometry.cpu_end -ne 0xF1B0 -or
        $CameraMaps[0].ordinary_movement_geometry.fingerprint_fnv1a32 -ne
            "CB1D4EAF" -or
        ![bool]$CourtMaps[0].native_contract.scene_slice.transactional -or
        $CourtMaps[0].native_contract.scene_slice.actor_binding -notmatch
            "combined transactional court frame" -or
        [int]$CourtMaps[0].native_contract.scene_slice.native_checkpoints.left_camera_x -ne
            102 -or
        [int]$CourtMaps[0].native_contract.scene_slice.native_checkpoints.center_camera_x -ne
            256 -or
        [int]$CourtMaps[0].native_contract.scene_slice.native_checkpoints.right_camera_x -ne
            408 -or
        $CourtMaps[0].native_contract.scene_slice.native_checkpoints.left_render_fnv1a32 -ne
            "4F52BCC1" -or
        $CourtMaps[0].native_contract.scene_slice.native_checkpoints.center_render_fnv1a32 -ne
            "9CC9CD31" -or
        $CourtMaps[0].native_contract.scene_slice.native_checkpoints.right_render_fnv1a32 -ne
            "033B45D5" -or
        [bool]$CourtMaps[0].native_contract.scene_slice.integration_is_additional_rom_claim -or
        $CourtMaps[0].native_contract.boundary -notmatch
            "production camera-positioned live viewport") {
        throw "Production TGCP-2/TGCT-1 scene provenance is incomplete."
    }
    if ($LineupMaps.Count -ne 1 -or
        $LineupMaps[0].live_scene_integration.orientation_source -notmatch
            "TGOR-1 current_direction" -or
        $LineupMaps[0].live_scene_integration.position_binding -notmatch
            "exact TGFL-1 raw world X/Y" -or
        $LineupMaps[0].live_scene_integration.camera_binding -notmatch
            "orientation 0 camera_x=102" -or
        $LineupMaps[0].live_scene_integration.camera_binding -notmatch
            "orientation 1 camera_x=408" -or
        $LineupMaps[0].live_scene_integration.render_binding -notmatch
            "combined TGCT-1 slice" -or
        $LineupMaps[0].live_scene_integration.pose_binding -notmatch
            "preserves existing actor poses" -or
        [bool]$LineupMaps[0].live_scene_integration.integration_is_additional_rom_claim -or
        $LineupMaps[0].supported_boundary -notmatch
            "native live positioning" -or
        $LineupMaps[0].supported_boundary -notmatch
            "no live pose-state override") {
        throw "Production TGFL-1 scene provenance is incomplete."
    }

    $DunkLog = Join-Path $Scratch "dunk-cutaway-assets.log"
    $DunkRun = Invoke-Logged -Command $Executable -Arguments @(
        "--gameplay-dunk-cutaway-test", $PackPath
    ) -LogPath $DunkLog
    if ($DunkRun.exit_code -ne 0 -or
        $DunkRun.tail -notmatch "TGDK-1 dunk cutaway passed") {
        throw "Strict dunk-cutaway asset test failed.`n$($DunkRun.tail)"
    }

    $JumpLog = Join-Path $Scratch "jump-shot-assets.log"
    $JumpRun = Invoke-Logged -Command $Executable -Arguments @(
        "--gameplay-jump-shots-test", $PackPath
    ) -LogPath $JumpLog
    if ($JumpRun.exit_code -ne 0 -or
        $JumpRun.tail -notmatch "TGJS-2 jump-shot assets passed") {
        throw "Strict jump-shot asset test failed.`n$($JumpRun.tail)"
    }

    $SceneLog = Join-Path $Scratch "scene-self-test.log"
    $SceneRun = Invoke-Logged -Command $Executable -Arguments @(
        "--root", $ProjectRoot, "--gameplay-scene-test", $PackPath
    ) -LogPath $SceneLog
    if ($SceneRun.exit_code -ne 0 -or
        $SceneRun.tail.Trim() -ne "GAMEPLAY SCENE SELF TEST PASS") {
        throw "Native gameplay scene self-test failed.`n$($SceneRun.tail)"
    }

    $MissingPath = Join-Path $Scratch "missing-court.assetpack"
    $Missing = [byte[]]$PackBytes.Clone()
    $Missing[[int]$Entries["gameplay/court"].directory_offset] =
        [byte][char]'x'
    [IO.File]::WriteAllBytes($MissingPath, $Missing)
    Assert-SceneRejected -AssetPack $MissingPath -Label "missing-court"

    $MalformedCourtPath = Join-Path $Scratch "malformed-court.assetpack"
    $MalformedCourt = [byte[]]$PackBytes.Clone()
    $CourtOffset = [int]$Entries["gameplay/court"].pack_offset
    $MalformedCourt[$CourtOffset] =
        $MalformedCourt[$CourtOffset] -bxor 1
    [IO.File]::WriteAllBytes($MalformedCourtPath, $MalformedCourt)
    Assert-SceneRejected -AssetPack $MalformedCourtPath `
        -Label "malformed-court" -ExpectedStatus "TGCT-1"

    $OversizedCourtPath = Join-Path $Scratch "oversized-court.assetpack"
    $OversizedCourt = [byte[]]$PackBytes.Clone()
    [BitConverter]::GetBytes([uint64]6560).CopyTo(
        $OversizedCourt,
        [int]$Entries["gameplay/court"].directory_offset + 92)
    [IO.File]::WriteAllBytes($OversizedCourtPath, $OversizedCourt)
    Assert-SceneRejected -AssetPack $OversizedCourtPath `
        -Label "oversized-court" -ExpectedStatus "TGCT-1"

    $MissingCameraPath =
        Join-Path $Scratch "missing-camera-projection.assetpack"
    $MissingCamera = [byte[]]$PackBytes.Clone()
    $MissingCamera[
        [int]$Entries["gameplay/camera-projection"].directory_offset] =
        [byte][char]'x'
    [IO.File]::WriteAllBytes($MissingCameraPath, $MissingCamera)
    Assert-SceneRejected -AssetPack $MissingCameraPath `
        -Label "missing-camera-projection" -ExpectedStatus "TGCP-2"

    $MalformedCameraPath =
        Join-Path $Scratch "malformed-camera-projection.assetpack"
    $MalformedCamera = [byte[]]$PackBytes.Clone()
    $CameraOffset =
        [int]$Entries["gameplay/camera-projection"].pack_offset
    $MalformedCamera[$CameraOffset] =
        $MalformedCamera[$CameraOffset] -bxor 1
    [IO.File]::WriteAllBytes($MalformedCameraPath, $MalformedCamera)
    Assert-SceneRejected -AssetPack $MalformedCameraPath `
        -Label "malformed-camera-projection" `
        -ExpectedStatus "TGCP-2 header/size/reserved contract rejected"

    $OversizedCameraPath =
        Join-Path $Scratch "oversized-camera-projection.assetpack"
    $OversizedCamera = [byte[]]$PackBytes.Clone()
    [BitConverter]::GetBytes([uint64]1537).CopyTo(
        $OversizedCamera,
        [int]$Entries["gameplay/camera-projection"].directory_offset + 92)
    [IO.File]::WriteAllBytes($OversizedCameraPath, $OversizedCamera)
    Assert-SceneRejected -AssetPack $OversizedCameraPath `
        -Label "oversized-camera-projection" -ExpectedStatus "TGCP-2"

    $CameraDependencyPath =
        Join-Path $Scratch "camera-dependency-corrupt.assetpack"
    $CameraDependency = [byte[]]$PackBytes.Clone()
    $CameraDependency[$CameraOffset + 24] =
        $CameraDependency[$CameraOffset + 24] -bxor 1
    [IO.File]::WriteAllBytes($CameraDependencyPath, $CameraDependency)
    Assert-SceneRejected -AssetPack $CameraDependencyPath `
        -Label "camera-dependency-corrupt" -ExpectedStatus "TGCP-2"

    $MissingOrientationPath =
        Join-Path $Scratch "missing-court-orientation.assetpack"
    $MissingOrientation = [byte[]]$PackBytes.Clone()
    $MissingOrientation[
        [int]$Entries["gameplay/court-orientation"].directory_offset] =
        [byte][char]'x'
    [IO.File]::WriteAllBytes($MissingOrientationPath, $MissingOrientation)
    Assert-SceneRejected -AssetPack $MissingOrientationPath `
        -Label "missing-court-orientation" `
        -ExpectedStatus "TGOR-1 gameplay/court-orientation entry missing or wrong-sized"

    $MalformedOrientationPath =
        Join-Path $Scratch "malformed-court-orientation.assetpack"
    $MalformedOrientation = [byte[]]$PackBytes.Clone()
    $OrientationOffset =
        [int]$Entries["gameplay/court-orientation"].pack_offset
    $MalformedOrientation[$OrientationOffset] =
        $MalformedOrientation[$OrientationOffset] -bxor 1
    [IO.File]::WriteAllBytes(
        $MalformedOrientationPath, $MalformedOrientation)
    Assert-SceneRejected -AssetPack $MalformedOrientationPath `
        -Label "malformed-court-orientation" `
        -ExpectedStatus "TGOR-1 header/size/reserved contract rejected"

    $MissingLineupPath =
        Join-Path $Scratch "missing-free-throw-lineup.assetpack"
    $MissingLineup = [byte[]]$PackBytes.Clone()
    $MissingLineup[
        [int]$Entries["gameplay/free-throw-lineup"].directory_offset] =
        [byte][char]'x'
    [IO.File]::WriteAllBytes($MissingLineupPath, $MissingLineup)
    Assert-SceneRejected -AssetPack $MissingLineupPath `
        -Label "missing-free-throw-lineup" -ExpectedStatus "TGFL-1"

    $MalformedLineupPath =
        Join-Path $Scratch "malformed-free-throw-lineup.assetpack"
    $MalformedLineup = [byte[]]$PackBytes.Clone()
    $LineupOffset =
        [int]$Entries["gameplay/free-throw-lineup"].pack_offset
    $MalformedLineup[$LineupOffset] =
        $MalformedLineup[$LineupOffset] -bxor 1
    [IO.File]::WriteAllBytes($MalformedLineupPath, $MalformedLineup)
    Assert-SceneRejected -AssetPack $MalformedLineupPath `
        -Label "malformed-free-throw-lineup" `
        -ExpectedStatus "TGFL-1 header/size/reserved contract rejected"

    $OversizedLineupPath =
        Join-Path $Scratch "oversized-free-throw-lineup.assetpack"
    $OversizedLineup = [byte[]]$PackBytes.Clone()
    [BitConverter]::GetBytes([uint64]1217).CopyTo(
        $OversizedLineup,
        [int]$Entries["gameplay/free-throw-lineup"].directory_offset + 92)
    [IO.File]::WriteAllBytes($OversizedLineupPath, $OversizedLineup)
    Assert-SceneRejected -AssetPack $OversizedLineupPath `
        -Label "oversized-free-throw-lineup" -ExpectedStatus "TGFL-1"

    $MalformedPath = Join-Path $Scratch "malformed-close-shots.assetpack"
    $Malformed = [byte[]]$PackBytes.Clone()
    $Malformed[[int]$Entries["gameplay/close-shots"].pack_offset] =
        $Malformed[[int]$Entries["gameplay/close-shots"].pack_offset] -bxor 1
    [IO.File]::WriteAllBytes($MalformedPath, $Malformed)
    Assert-SceneRejected -AssetPack $MalformedPath `
        -Label "malformed-close-shots"

    $MissingDunkPath = Join-Path $Scratch "missing-dunk-cutaway.assetpack"
    $MissingDunk = [byte[]]$PackBytes.Clone()
    $MissingDunk[[int]$Entries["gameplay/dunk-cutaway"].directory_offset] =
        [byte][char]'x'
    [IO.File]::WriteAllBytes($MissingDunkPath, $MissingDunk)
    Assert-SceneRejected -AssetPack $MissingDunkPath `
        -Label "missing-dunk-cutaway"

    $MalformedDunkPath = Join-Path $Scratch "malformed-dunk-cutaway.assetpack"
    $MalformedDunk = [byte[]]$PackBytes.Clone()
    $DunkOffset = [int]$Entries["gameplay/dunk-cutaway"].pack_offset
    $MalformedDunk[$DunkOffset] = $MalformedDunk[$DunkOffset] -bxor 1
    [IO.File]::WriteAllBytes($MalformedDunkPath, $MalformedDunk)
    Assert-SceneRejected -AssetPack $MalformedDunkPath `
        -Label "malformed-dunk-cutaway"

    $OversizedDunkPath = Join-Path $Scratch "oversized-dunk-cutaway.assetpack"
    $OversizedDunk = [byte[]]$PackBytes.Clone()
    [BitConverter]::GetBytes([uint64]20273).CopyTo(
        $OversizedDunk,
        [int]$Entries["gameplay/dunk-cutaway"].directory_offset + 92)
    [IO.File]::WriteAllBytes($OversizedDunkPath, $OversizedDunk)
    Assert-SceneRejected -AssetPack $OversizedDunkPath `
        -Label "oversized-dunk-cutaway"

    $MissingJumpPath = Join-Path $Scratch "missing-jump-shots.assetpack"
    $MissingJump = [byte[]]$PackBytes.Clone()
    $MissingJump[[int]$Entries["gameplay/jump-shots"].directory_offset] =
        [byte][char]'x'
    [IO.File]::WriteAllBytes($MissingJumpPath, $MissingJump)
    Assert-SceneRejected -AssetPack $MissingJumpPath `
        -Label "missing-jump-shots"

    $MalformedJumpPath = Join-Path $Scratch "malformed-jump-shots.assetpack"
    $MalformedJump = [byte[]]$PackBytes.Clone()
    $MalformedJump[[int]$Entries["gameplay/jump-shots"].pack_offset] =
        $MalformedJump[[int]$Entries["gameplay/jump-shots"].pack_offset] `
            -bxor 1
    [IO.File]::WriteAllBytes($MalformedJumpPath, $MalformedJump)
    Assert-SceneRejected -AssetPack $MalformedJumpPath `
        -Label "malformed-jump-shots"

    $OversizedJumpPath = Join-Path $Scratch "oversized-jump-shots.assetpack"
    $OversizedJump = [byte[]]$PackBytes.Clone()
    [BitConverter]::GetBytes([uint64]1649).CopyTo(
        $OversizedJump,
        [int]$Entries["gameplay/jump-shots"].directory_offset + 92)
    [IO.File]::WriteAllBytes($OversizedJumpPath, $OversizedJump)
    Assert-SceneRejected -AssetPack $OversizedJumpPath `
        -Label "oversized-jump-shots"

    $MissingResolutionPath = Join-Path $Scratch "missing-shot-resolution.assetpack"
    $MissingResolution = [byte[]]$PackBytes.Clone()
    $MissingResolution[[int]$Entries["gameplay/shot-resolution"].directory_offset] =
        [byte][char]'x'
    [IO.File]::WriteAllBytes($MissingResolutionPath, $MissingResolution)
    Assert-SceneRejected -AssetPack $MissingResolutionPath `
        -Label "missing-shot-resolution" -ExpectedStatus "TGSR-3"

    $MalformedResolutionPath = Join-Path $Scratch "malformed-shot-resolution.assetpack"
    $MalformedResolution = [byte[]]$PackBytes.Clone()
    $ResolutionOffset = [int]$Entries["gameplay/shot-resolution"].pack_offset
    $MalformedResolution[$ResolutionOffset] =
        $MalformedResolution[$ResolutionOffset] -bxor 1
    [IO.File]::WriteAllBytes($MalformedResolutionPath, $MalformedResolution)
    Assert-SceneRejected -AssetPack $MalformedResolutionPath `
        -Label "malformed-shot-resolution" -ExpectedStatus "TGSR-3"

    $OversizedResolutionPath = Join-Path $Scratch "oversized-shot-resolution.assetpack"
    $OversizedResolution = [byte[]]$PackBytes.Clone()
    [BitConverter]::GetBytes([uint64]513).CopyTo(
        $OversizedResolution,
        [int]$Entries["gameplay/shot-resolution"].directory_offset + 92)
    [IO.File]::WriteAllBytes($OversizedResolutionPath, $OversizedResolution)
    Assert-SceneRejected -AssetPack $OversizedResolutionPath `
        -Label "oversized-shot-resolution" -ExpectedStatus "TGSR-3"

    $WrongResolutionPath = Join-Path $Scratch "wrong-revision-shot-resolution.assetpack"
    $WrongResolution = [byte[]]$PackBytes.Clone()
    $WrongResolution[$ResolutionOffset + 80] =
        $WrongResolution[$ResolutionOffset + 80] -bxor 1
    [IO.File]::WriteAllBytes($WrongResolutionPath, $WrongResolution)
    Assert-SceneRejected -AssetPack $WrongResolutionPath `
        -Label "wrong-revision-shot-resolution" -ExpectedStatus "TGSR-3"

    $CrossPackResolutionPath = Join-Path $Scratch "cross-pack-shot-resolution.assetpack"
    $CrossPackResolution = [byte[]]$PackBytes.Clone()
    $CoreOffset = [int]$Entries["gameplay/core"].pack_offset
    $CrossPackResolution[$CoreOffset + 128] =
        $CrossPackResolution[$CoreOffset + 128] -bxor 1
    [IO.File]::WriteAllBytes($CrossPackResolutionPath, $CrossPackResolution)
    Assert-SceneRejected -AssetPack $CrossPackResolutionPath `
        -Label "cross-pack-shot-resolution" -ExpectedStatus "TGSR-3"

    $OversizedPath = Join-Path $Scratch "oversized-core.assetpack"
    $Oversized = [byte[]]$PackBytes.Clone()
    [BitConverter]::GetBytes([uint64]23417).CopyTo(
        $Oversized, [int]$Entries["gameplay/core"].directory_offset + 92)
    [IO.File]::WriteAllBytes($OversizedPath, $Oversized)
    Assert-SceneRejected -AssetPack $OversizedPath -Label "oversized-core"

    $ChrMismatchPath = Join-Path $Scratch "chr-mismatch.assetpack"
    $ChrMismatch = [byte[]]$PackBytes.Clone()
    $ChrOffset = [int]$Entries["chr/all"].pack_offset
    $ChrMismatch[$ChrOffset] = $ChrMismatch[$ChrOffset] -bxor 1
    [IO.File]::WriteAllBytes($ChrMismatchPath, $ChrMismatch)
    Assert-SceneRejected -AssetPack $ChrMismatchPath -Label "chr-mismatch"

    $env:TECMO_ASSETPACK = $PackPath
    $RenderSpecs = @(
        [pscustomobject]@{ mode="gameplay-start"; state='gameplay-state frame=0 shot=none phase=live' },
        [pscustomobject]@{ mode="gameplay-possession-left"; state='gameplay-state frame=691 shot=none phase=live' },
        [pscustomobject]@{ mode="gameplay-possession-center"; state='gameplay-state frame=691 shot=none phase=live' },
        [pscustomobject]@{ mode="gameplay-possession-right"; state='gameplay-state frame=691 shot=none phase=live' },
        [pscustomobject]@{ mode="gameplay-free-throw-left"; state='gameplay-state frame=696 shot=none phase=free-throw-sequence' },
        [pscustomobject]@{ mode="gameplay-free-throw-right"; state='gameplay-state frame=696 shot=none phase=free-throw-sequence' },
        [pscustomobject]@{ mode="gameplay-jump-frame1"; state='gameplay-state frame=1 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-frame2"; state='gameplay-state frame=2 shot=jump phase=live score=0/2' },
        [pscustomobject]@{ mode="gameplay-jump-frame4"; state='gameplay-state frame=4 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-frame21"; state='gameplay-state frame=21 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-frame22"; state='gameplay-state frame=22 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-frame39"; state='gameplay-state frame=39 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-frame40"; state='gameplay-state frame=40 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-frame45"; state='gameplay-state frame=45 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-frame46"; state='gameplay-state frame=46 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-frame72"; state='gameplay-state frame=72 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-frame73"; state='gameplay-state frame=73 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-frame74"; state='gameplay-state frame=74 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-frame75"; state='gameplay-state frame=75 shot=jump phase=live score=0/2' },
        [pscustomobject]@{ mode="gameplay-jump-frame86"; state='gameplay-state frame=86 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-frame87"; state='gameplay-state frame=87 shot=none phase=live score=0/2' },
        [pscustomobject]@{ mode="gameplay-jump-rattle-frame72"; state='gameplay-state frame=72 shot=jump phase=live score=0/2' },
        [pscustomobject]@{ mode="gameplay-jump-rattle-frame73"; state='gameplay-state frame=73 shot=jump phase=live score=0/2' },
        [pscustomobject]@{ mode="gameplay-jump-rattle-frame74"; state='gameplay-state frame=74 shot=jump phase=live score=0/2' },
        [pscustomobject]@{ mode="gameplay-jump-rattle-frame77"; state='gameplay-state frame=77 shot=jump phase=live score=0/2' },
        [pscustomobject]@{ mode="gameplay-jump-rattle-frame81"; state='gameplay-state frame=81 shot=jump phase=live score=0/2' },
        [pscustomobject]@{ mode="gameplay-jump-rattle-frame85"; state='gameplay-state frame=85 shot=jump phase=live score=0/2' },
        [pscustomobject]@{ mode="gameplay-jump-rattle-frame88"; state='gameplay-state frame=88 shot=jump phase=live score=0/2' },
        [pscustomobject]@{ mode="gameplay-jump-rattle-frame89"; state='gameplay-state frame=89 shot=jump phase=live score=0/2' },
        [pscustomobject]@{ mode="gameplay-jump-rattle-frame90"; state='gameplay-state frame=90 shot=jump phase=live score=0/2' },
        [pscustomobject]@{ mode="gameplay-jump-rattle-frame91"; state='gameplay-state frame=91 shot=jump phase=live score=0/2' },
        [pscustomobject]@{ mode="gameplay-jump-rattle-frame102"; state='gameplay-state frame=102 shot=jump phase=live score=0/2' },
        [pscustomobject]@{ mode="gameplay-jump-rattle-frame103"; state='gameplay-state frame=103 shot=none phase=live score=0/2' },
        [pscustomobject]@{ mode="gameplay-jump-make-frame1"; state='gameplay-state frame=1 shot=jump phase=live score=0/0' },
        [pscustomobject]@{ mode="gameplay-jump-make-frame5"; state='gameplay-state frame=5 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-make-frame9"; state='gameplay-state frame=9 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-make-frame19"; state='gameplay-state frame=19 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-make-frame20"; state='gameplay-state frame=20 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-make-frame39"; state='gameplay-state frame=39 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-make-frame57"; state='gameplay-state frame=57 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-make-frame63"; state='gameplay-state frame=63 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-make-frame85"; state='gameplay-state frame=85 shot=jump phase=live score=3/0' },
        [pscustomobject]@{ mode="gameplay-jump-make-frame110"; state='gameplay-state frame=110 shot=jump phase=live score=3/0' },
        [pscustomobject]@{ mode="gameplay-jump-make-frame111"; state='gameplay-state frame=111 shot=none phase=live score=3/0' },
        [pscustomobject]@{ mode="gameplay-dunk-frame1"; state='gameplay-state frame=1 shot=dunk phase=live' },
        [pscustomobject]@{ mode="gameplay-dunk-frame8"; state='gameplay-state frame=8 shot=dunk phase=live' },
        [pscustomobject]@{ mode="gameplay-dunk-frame16"; state='gameplay-state frame=16 shot=dunk phase=live' },
        [pscustomobject]@{ mode="gameplay-close-shot-frame16"; state='gameplay-state frame=16 shot=dunk phase=live' },
        [pscustomobject]@{ mode="gameplay-dunk-frame24"; state='gameplay-state frame=24 shot=dunk phase=live' },
        [pscustomobject]@{ mode="gameplay-dunk-frame32"; state='gameplay-state frame=32 shot=dunk phase=live' },
        [pscustomobject]@{ mode="gameplay-dunk-frame48"; state='gameplay-state frame=48 shot=dunk phase=live' },
        [pscustomobject]@{ mode="gameplay-dunk-frame64"; state='gameplay-state frame=64 shot=dunk phase=live' },
        [pscustomobject]@{ mode="gameplay-dunk-frame80"; state='gameplay-state frame=80 shot=dunk phase=live' },
        [pscustomobject]@{ mode="gameplay-dunk-frame87"; state='gameplay-state frame=87 shot=dunk phase=live' },
        [pscustomobject]@{ mode="gameplay-dunk-frame132"; state='gameplay-state frame=132 shot=dunk phase=live' }
    )
    $RenderHashes = @{}
    foreach ($Spec in $RenderSpecs) {
        $RenderHashes[$Spec.mode] = Invoke-RenderCheckpoint `
            -Mode $Spec.mode -ExpectedState $Spec.state
    }
    $ExpectedPossessionSliceHashes = @{
        "gameplay-possession-left" =
            "0AE98FF6043E4A719F65095CBFFE24C8E2CF602D0D393E22EC3EF636FE851A71"
        "gameplay-possession-center" =
            "6E709B5C008717196FAEA105F792C1C9745F4BABA09155BE963524842A139D7C"
        "gameplay-possession-right" =
            "3BDB7F1A83B03A8475C9F9CCADF9327693AF139EF234283B7F2B581EC09A4AB5"
    }
    foreach ($Mode in $ExpectedPossessionSliceHashes.Keys) {
        if ($RenderHashes[$Mode] -ne
            $ExpectedPossessionSliceHashes[$Mode]) {
            throw "Gameplay possession-slice render hash changed at '$Mode'."
        }
    }
    $PossessionSliceVisuals = @(
        $RenderHashes["gameplay-possession-left"],
        $RenderHashes["gameplay-possession-center"],
        $RenderHashes["gameplay-possession-right"]
    ) | Select-Object -Unique
    if ($PossessionSliceVisuals.Count -ne 3) {
        throw "Gameplay possession-slice visuals collapsed together."
    }
    $ExpectedFreeThrowHashes = @{
        "gameplay-free-throw-left" =
            "A20D20AF625C2FBEE07C36B4E25914E0A10E2DF818446252275CA1291D5B61B2"
        "gameplay-free-throw-right" =
            "7563B19DAFC5C51EE367A8C300F9056D91160E5674679B3DC886E5CA1A312F92"
    }
    foreach ($Mode in $ExpectedFreeThrowHashes.Keys) {
        if ($RenderHashes[$Mode] -ne
            $ExpectedFreeThrowHashes[$Mode]) {
            throw "Gameplay free-throw lineup render hash changed at '$Mode'."
        }
    }
    if ($RenderHashes["gameplay-free-throw-left"] -eq
        $RenderHashes["gameplay-free-throw-right"]) {
        throw "Gameplay free-throw orientation visuals collapsed together."
    }
    $ExpectedJumpHashes = @{
        "gameplay-jump-frame2" =
            "AB92C28774FAC9ABED55B79AE83410DAC0ACEF8F53DB16DAE70BCDEA36FB0F4C"
        "gameplay-jump-frame75" =
            "7BA90B4B84B00E39933C80EDD9FF22130CF0A05931334655EEF9A0FBF5453283"
        "gameplay-jump-frame87" =
            "52EE5F1364C001DD13BB8AED76F8595B97D6604EA99A7479A5FB7C5744EEF663"
    }
    foreach ($Mode in $ExpectedJumpHashes.Keys) {
        if ($RenderHashes[$Mode] -ne $ExpectedJumpHashes[$Mode]) {
            throw "Gameplay jump-miss render hash changed at '$Mode'."
        }
    }
    $ExpectedRattleHashes = @{
        "gameplay-jump-rattle-frame72" =
            "899E1B2BF536E636CEC2F88FC4786555DFC108E2226B94DE7460FB804AD250BD"
        "gameplay-jump-rattle-frame73" =
            "03989C875F81FA76362CF348F945394E583FD5106D40660FD32CE0ECAEA06796"
        "gameplay-jump-rattle-frame74" =
            "9D4C88914894CF05B0A848B9A1A3672D6C190D7CFCC39C6FC3DE846801F2EA69"
        "gameplay-jump-rattle-frame77" =
            "5554D57CBEE9A510DBFA0681CC46F76E8DBB6ABDF6F2D474A157B468B1D8718D"
        "gameplay-jump-rattle-frame81" =
            "D3BE64FED59EFAE8285DED65CC80B46BA1A65B97521CA8AB52158B9944B74612"
        "gameplay-jump-rattle-frame85" =
            "5554D57CBEE9A510DBFA0681CC46F76E8DBB6ABDF6F2D474A157B468B1D8718D"
        "gameplay-jump-rattle-frame88" =
            "C13D8C2A29DBF92098DED6421DC2E909253A121BC317A565E222B9F95C043C58"
        "gameplay-jump-rattle-frame89" =
            "D3BE64FED59EFAE8285DED65CC80B46BA1A65B97521CA8AB52158B9944B74612"
        "gameplay-jump-rattle-frame90" =
            "1B3489661E56D96CE492D3C5231ECAC6CAC2CD033BF97281B3B48AE5E6A4B99D"
        "gameplay-jump-rattle-frame103" =
            "56B9087FCC8C48B5C0BD15AA558B2BD8F7D63F722E1F9975AA0E00189000E881"
    }
    foreach ($Mode in $ExpectedRattleHashes.Keys) {
        if ($RenderHashes[$Mode] -ne $ExpectedRattleHashes[$Mode]) {
            throw "Gameplay rim-rattle render hash changed at '$Mode'."
        }
    }
    $ExpectedJumpMakeHashes = @{
        "gameplay-jump-make-frame9" =
            "C7F057C468EFEFC19F6AA82505FF113A8AFD237464EB15291968581740BD5729"
        "gameplay-jump-make-frame20" =
            "21FD5BCB165C1345ADD149948DB042E477D27EF00796FC08B9DA7140F47AB919"
        "gameplay-jump-make-frame57" =
            "71E1862754822071067873069B7D6C5440D549560156392E00FD2755965F76ED"
        "gameplay-jump-make-frame85" =
            "87ACCBD092955AFA59FACF8D8BD439E7576101B9FC54DD80BC0FEBE4207C32B3"
        "gameplay-jump-make-frame111" =
            "17967AAC8CB3948FE61CDD231AF32920DE241D2298FBD08E1CEF0A19F79F83A0"
    }
    foreach ($Mode in $ExpectedJumpMakeHashes.Keys) {
        if ($RenderHashes[$Mode] -ne $ExpectedJumpMakeHashes[$Mode]) {
            throw "Gameplay jump-make render hash changed at '$Mode'."
        }
    }
    $VisualSentinels = @(
        $RenderHashes["gameplay-start"],
        $RenderHashes["gameplay-jump-frame21"],
        $RenderHashes["gameplay-dunk-frame16"],
        $RenderHashes["gameplay-dunk-frame32"],
        $RenderHashes["gameplay-dunk-frame64"],
        $RenderHashes["gameplay-dunk-frame80"]
    ) | Select-Object -Unique
    if ($VisualSentinels.Count -ne 6) {
        throw "Gameplay live/cutaway/black/return visual sentinels collapsed together."
    }
    if ($RenderHashes["gameplay-close-shot-frame16"] -ne
        $RenderHashes["gameplay-dunk-frame16"]) {
        throw "Legacy close-shot render mode diverged from canonical dunk mode."
    }

    $global:LASTEXITCODE = 0
    Write-Output ("GAMEPLAY SCENE TEST PASS: Rev1 full-pack provenance " +
        "scene controls TGCP-2 full-world camera fine-scroll guarded-margins actor-camera-projection/possession-slice-render/freeze TGFL-1 orientation-lineup TGDK TGJS TGSR-3 jump-miss/jump-make/rim-rattle early-release/expiry shots dunk-cutaway frame75/audio state " +
        "halftime/final render-hashes/determinism missing malformed oversized " +
        "dependency-corrupt chr-mismatch")
} finally {
    $env:TECMO_ASSETPACK = $PreviousPack
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
}
