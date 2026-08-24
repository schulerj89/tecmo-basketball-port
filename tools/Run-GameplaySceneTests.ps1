param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [switch]$Build,
    [switch]$RequirePass,
    [string]$ProofRootPath,
    [string]$OriginalReferenceManifestPath
)

$ErrorActionPreference = "Stop"

$ExpectedRomSha256 =
    "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4"
$ExpectedBaseSha =
    "ad0f005673692b04772bce3c3b4d3ac4b2624731"
$ExpectedBranch = "codex/r1-live-foundation-luna"
$ExpectedOriginalContactSheetSha =
    "2EE377C3A97A2C415ED223A4E81C468230BCC6E4A987BABFC7F622E928B22B37"
$NativeFrameRate = "39375000/655171"
$NativeVideoTimeBase = "1/39375000"
$NativeVideoTrackTimescale = 39375000

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
$ProofRoot = if ($ProofRootPath) {
    if ([IO.Path]::IsPathRooted($ProofRootPath)) {
        [IO.Path]::GetFullPath($ProofRootPath)
    } else {
        [IO.Path]::GetFullPath((Join-Path $ProjectRoot $ProofRootPath))
    }
} else {
    Join-Path $BuildDir ("live-proof-{0}Z" -f
        [DateTime]::UtcNow.ToString("yyyyMMddTHHmmssfff"))
}
if (!$ProofRoot.StartsWith($BuildPrefix,
                           [StringComparison]::OrdinalIgnoreCase) -or
    $ProofRoot.TrimEnd('\', '/') -eq $BuildDir.TrimEnd('\', '/')) {
    throw "LIVE proof root must be a child of build\."
}
if (Test-Path -LiteralPath $ProofRoot) {
    if (@(Get-ChildItem -LiteralPath $ProofRoot -Force -ErrorAction Stop).Count -ne 0) {
        throw "LIVE proof root must be new or empty: $ProofRoot"
    }
} else {
    New-Item -ItemType Directory -Force -Path $ProofRoot | Out-Null
}
if ($OriginalReferenceManifestPath) {
    if ([IO.Path]::IsPathRooted($OriginalReferenceManifestPath)) {
        $OriginalReferenceManifestPath =
            [IO.Path]::GetFullPath($OriginalReferenceManifestPath)
    } else {
        $OriginalReferenceManifestPath = [IO.Path]::GetFullPath(
            (Join-Path $ProjectRoot $OriginalReferenceManifestPath))
    }
}
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
    $ExitCode = $LASTEXITCODE
    if (!(Test-Path -LiteralPath $LogPath -PathType Leaf) -or
        (Get-Item -LiteralPath $LogPath).Length -eq 0) {
        Set-Content -LiteralPath $LogPath `
            -Value ("[runner] no stdout/stderr emitted; exit={0}" -f $ExitCode) `
            -Encoding UTF8
    }
    return [pscustomobject]@{
        exit_code = $ExitCode
        tail = Get-ShortTail $LogPath
    }
}

function Get-LiveProofGitState {
    $Head = (@(& git -C $ProjectRoot rev-parse HEAD 2>&1) -join "").Trim()
    if ($LASTEXITCODE -ne 0 -or $Head -notmatch '^[0-9a-f]{40}$') {
        throw "LIVE proof could not read Git HEAD."
    }
    $Branch = (@(& git -C $ProjectRoot branch --show-current 2>&1) -join "").Trim()
    if ($LASTEXITCODE -ne 0 -or !$Branch) {
        throw "LIVE proof could not read the Git branch."
    }
    $Status = @(& git -C $ProjectRoot status --porcelain=v1 --untracked-files=all 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "LIVE proof could not read Git status."
    }
    & git -C $ProjectRoot merge-base --is-ancestor $ExpectedBaseSha $Head 2>$null
    $Ancestor = $LASTEXITCODE -eq 0
    return [pscustomobject]@{
        head = $Head
        branch = $Branch
        clean = $Status.Count -eq 0
        status = @($Status)
        base_sha = $ExpectedBaseSha
        base_is_ancestor = $Ancestor
    }
}

function Test-LiveProofRequirePassState {
    param([object]$GitState)
    return $null -ne $GitState -and
        [bool]$GitState.clean -and
        $GitState.branch -eq $ExpectedBranch -and
        [bool]$GitState.base_is_ancestor
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

function Assert-LiveProofRejected {
    param(
        [string[]]$Arguments,
        [string]$Label
    )
    $Log = Join-Path $Scratch ("live-proof-reject-{0}.log" -f $Label)
    $Run = Invoke-Logged -Command $Executable -Arguments $Arguments -LogPath $Log
    if ($Run.exit_code -eq 0 -or $Run.tail -notmatch "LIVE proof failed") {
        throw "LIVE proof accepted $Label negative input.`n$($Run.tail)"
    }
}

function Assert-Opcode15PassiveTrace {
    param(
        [object]$Snapshot,
        [string]$Label
    )
    if ($null -eq $Snapshot -or $null -eq $Snapshot.opcode15) {
        throw "LIVE proof $Label has no TGPS opcode-15 trace object."
    }
    $Trace = $Snapshot.opcode15
    $Gate = $Trace.raw_gate_available
    $Typed = $Trace.typed_before_after
    if ($null -eq $Gate -or $null -eq $Typed) {
        throw "LIVE proof $Label has a malformed opcode-15 trace object."
    }
    if ([bool]$Trace.observed) {
        if ($Trace.branch -ne "deferred-missing-raw" -or
            $Trace.missing_raw_mask -ne "00001FFF" -or
            [bool]$Gate.'$0499' -or [bool]$Gate.'$04B0' -or
            [bool]$Gate.'$007E' -or [bool]$Gate.'$06D5_$06D6' -or
            [bool]$Gate.'$0479' -or [bool]$Gate.'$0442_$044D' -or
            [bool]$Gate.'$059E' -or [bool]$Gate.actor_lifecycle -or
            [int]$Typed.'$0308'[0] -ne [int]$Typed.'$0308'[1] -or
            [int]$Typed.'$0309'[0] -ne [int]$Typed.'$0309'[1] -or
            [int]$Typed.'$0547_$0551'[0] -ne [int]$Typed.'$0547_$0551'[1] -or
            [int]$Typed.'$057C'[0] -ne [int]$Typed.'$057C'[1]) {
            throw "LIVE proof $Label opcode-15 passive-defer contract regressed."
        }
    } elseif ($Trace.branch -ne "none" -or
              $Trace.missing_raw_mask -ne "00000000") {
        throw "LIVE proof $Label has an invalid unobserved opcode-15 trace."
    }
}

function Test-LiveProofManifest {
    param(
        [string]$ManifestPath,
        [string]$ExpectedRomSha256,
        [string]$ExpectedPackSha256,
        [string[]]$ExpectedEvents
    )
    if (!(Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
        return $false
    }
    try {
        $Manifest = Get-Content -LiteralPath $ManifestPath -Raw |
            ConvertFrom-Json
        $ExpectedSheetHeight = [int]([Math]::Ceiling(
            $ExpectedEvents.Count / 3.0) * 480)
        if ($Manifest.schema -ne "tecmo.live-proof-manifest/TGLP-1" -or
            $Manifest.rom_sha256 -ne $ExpectedRomSha256 -or
            $Manifest.pack_sha256 -ne $ExpectedPackSha256 -or
            [int]$Manifest.native_resolution[0] -ne 640 -or
            [int]$Manifest.native_resolution[1] -ne 480 -or
            [int]$Manifest.stored_frame_count -ne $ExpectedEvents.Count * 2 -or
             [int]$Manifest.decoded_frame_count -ne $ExpectedEvents.Count * 2) {
            return $false
        }
        if ($null -eq $Manifest.asset_pack -or
            !(Test-Path -LiteralPath $Manifest.asset_pack.path -PathType Leaf) -or
            [string]$Manifest.asset_pack.replay_path -ne
                [string]$Manifest.asset_pack.path -or
            [string]$Manifest.proof_pack_replay_path -ne
                [string]$Manifest.asset_pack.path -or
            [string]$Manifest.asset_pack.sha256 -ne $ExpectedPackSha256 -or
            (Get-FileHash -LiteralPath $Manifest.asset_pack.path -Algorithm SHA256).Hash -ne
                $ExpectedPackSha256) {
            return $false
        }
        $Records = @($Manifest.frame_records)
        if ($Records.Count -ne $ExpectedEvents.Count * 2) {
            return $false
        }
        foreach ($Event in $ExpectedEvents) {
            $Matches = @($Records | Where-Object { $_.event -eq $Event })
            if ($Matches.Count -ne 2 -or
                $Matches[0].sha256 -ne $Matches[1].sha256 -or
                $Matches[0].frame_fingerprint_fnv1a32 -ne
                    $Matches[1].frame_fingerprint_fnv1a32) {
                return $false
            }
            foreach ($Record in $Matches) {
                if (!(Test-Path -LiteralPath $Record.path -PathType Leaf) -or
                    (Get-FileHash -LiteralPath $Record.path -Algorithm SHA256).Hash -ne
                        $Record.sha256 -or
                    !(Test-Path -LiteralPath $Record.state_path -PathType Leaf) -or
                    (Get-Item -LiteralPath $Record.state_path).Length -le 0) {
                    return $false
                }
            }
        }
        $Sheets = @($Manifest.contact_sheets)
        if ($Sheets.Count -ne 2) { return $false }
        foreach ($Sheet in $Sheets) {
            $Dimensions = Get-PngDimensions $Sheet.path
            if ($Dimensions.width -ne 1920 -or
                $Dimensions.height -ne $ExpectedSheetHeight -or
                [int]$Sheet.width -ne $Dimensions.width -or
                [int]$Sheet.height -ne $Dimensions.height -or
                [int]$Sheet.frame_count -ne $ExpectedEvents.Count -or
                (Get-FileHash -LiteralPath $Sheet.path -Algorithm SHA256).Hash -ne
                    $Sheet.sha256) {
                return $false
            }
        }
        $Videos = @($Manifest.native_videos)
        if ($Videos.Count -ne 2) { return $false }
        foreach ($Video in $Videos) {
            if (!(Test-Path -LiteralPath $Video.path -PathType Leaf) -or
                (Get-FileHash -LiteralPath $Video.path -Algorithm SHA256).Hash -ne
                    $Video.sha256 -or
                [int]$Video.probe.width -ne 640 -or
                [int]$Video.probe.height -ne 480 -or
                [string]$Video.probe.r_frame_rate -ne $NativeFrameRate -or
                [string]$Video.probe.avg_frame_rate -ne $NativeFrameRate -or
                [string]$Video.probe.time_base -ne $NativeVideoTimeBase -or
                [int]$Video.probe.nb_frames -ne $ExpectedEvents.Count -or
                [int]$Video.probe.nb_read_frames -ne $ExpectedEvents.Count -or
                [int]$Video.stored_frame_count -ne $ExpectedEvents.Count -or
                [int]$Video.decoded_frame_count -ne $ExpectedEvents.Count) {
                return $false
            }
        }
        if ($Videos[0].decoded_frame_sha256 -ne
                $Videos[1].decoded_frame_sha256 -or
            (@($Videos[0].decoded_frame_hashes) -join ',') -ne
                (@($Videos[1].decoded_frame_hashes) -join ',')) {
            return $false
        }
        if ([bool]$Manifest.suites_complete) {
            $RequiredLogs = @($Manifest.required_logs)
            if ($RequiredLogs.Count -lt 3) {
                return $false
            }
            foreach ($Log in $RequiredLogs) {
                if (!(Test-Path -LiteralPath $Log.path -PathType Leaf) -or
                    [int64]$Log.bytes -le 0 -or
                    [int64](Get-Item -LiteralPath $Log.path).Length -ne
                        [int64]$Log.bytes -or
                    (Get-FileHash -LiteralPath $Log.path -Algorithm SHA256).Hash -ne
                        $Log.sha256) {
                    return $false
                }
            }
            $Inventory = @($Manifest.artifact_inventory)
            if ($Inventory.Count -le 0 -or
                [int]$Manifest.artifact_inventory_count -ne
                    $Inventory.Count) {
                return $false
            }
            foreach ($Artifact in $Inventory) {
                if (!(Test-Path -LiteralPath $Artifact.path -PathType Leaf) -or
                    [int64]$Artifact.bytes -le 0 -or
                    [int64](Get-Item -LiteralPath $Artifact.path).Length -ne
                        [int64]$Artifact.bytes -or
                    (Get-FileHash -LiteralPath $Artifact.path -Algorithm SHA256).Hash -ne
                        $Artifact.sha256) {
                    return $false
                }
            }
        }
        $Status = [string]$Manifest.status
        $ShaPattern = '^[0-9a-fA-F]{40}$'
        if ($Status -eq "PASS") {
            if (![bool]$Manifest.require_pass -or
                ![bool]$Manifest.clean -or
                [string]$Manifest.current_sha -notmatch $ShaPattern -or
                [string]$Manifest.final_sha -notmatch $ShaPattern -or
                [string]$Manifest.current_sha -ne
                    [string]$Manifest.final_sha -or
                [string]$Manifest.current_sha -eq $ExpectedBaseSha) {
                return $false
            }
            $GitState = Get-LiveProofGitState
            if (!$GitState.clean -or
                $GitState.branch -ne [string]$Manifest.branch -or
                $GitState.head -ne [string]$Manifest.current_sha -or
                !$GitState.base_is_ancestor) {
                return $false
            }
        } elseif ($Status -eq "DRAFT") {
            if ([string]$Manifest.final_sha -ne "PENDING_CLEAN_COMMIT") {
                return $false
            }
        } else {
            return $false
        }
        return $true
    } catch {
        return $false
    }
}

function New-LiveProofContactSheet {
    param(
        [object[]]$Records,
        [string]$OutputPath
    )
    Add-Type -AssemblyName System.Drawing
    $Columns = 3
    $Rows = [int][Math]::Ceiling($Records.Count / [double]$Columns)
    [int]$Width = 640 * $Columns
    [int]$Height = 480 * $Rows
    $Bitmap = [Drawing.Bitmap]::new($Width, $Height)
    $Graphics = [Drawing.Graphics]::FromImage($Bitmap)
    try {
        $Graphics.Clear([Drawing.Color]::Black)
        for ($Index = 0; $Index -lt $Records.Count; ++$Index) {
            $Image = [Drawing.Image]::FromFile($Records[$Index].path)
            try {
                $X = ($Index % $Columns) * 640
                [int]$Y = [Math]::Floor($Index / $Columns) * 480
                $Graphics.DrawImage($Image, [int]$X, $Y, 640, 480)
            } finally {
                $Image.Dispose()
            }
        }
        $Bitmap.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $Graphics.Dispose()
        $Bitmap.Dispose()
    }
    $Dimensions = Get-PngDimensions $OutputPath
    if ($Dimensions.width -ne $Width -or $Dimensions.height -ne $Height) {
        throw "LIVE proof contact sheet IHDR is $($Dimensions.width)x$($Dimensions.height); expected ${Width}x${Height}."
    }
    return [pscustomobject]@{
        path = [IO.Path]::GetFullPath($OutputPath)
        sha256 = (Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash
        width = $Dimensions.width
        height = $Dimensions.height
        frame_count = $Records.Count
    }
}

function New-LiveProofVideo {
    param(
        [string]$FramesRoot,
        [string]$OutputPath,
        [string]$Label,
        [int]$FrameCount
    )
    $Ffmpeg = Get-Command ffmpeg -ErrorAction SilentlyContinue
    $Ffprobe = Get-Command ffprobe -ErrorAction SilentlyContinue
    if ($null -eq $Ffmpeg -or $null -eq $Ffprobe) {
        throw "LIVE proof requires both ffmpeg and ffprobe for native MP4 validation."
    }
    $Pattern = Join-Path $FramesRoot "frame-%04d.png"
    $EncodeLog = Join-Path $FramesRoot ("{0}-ffmpeg.log" -f $Label)
    $EncodeArgs = @(
        "-y", "-hide_banner", "-loglevel", "error",
        "-framerate", $NativeFrameRate, "-start_number", "1",
        "-i", $Pattern, "-frames:v", [string]$FrameCount,
        "-an", "-pix_fmt", "yuv420p",
        "-video_track_timescale", [string]$NativeVideoTrackTimescale,
        $OutputPath
    )
    $EncodeRun = Invoke-Logged -Command $Ffmpeg.Source -Arguments $EncodeArgs `
        -LogPath $EncodeLog
    if ($EncodeRun.exit_code -ne 0 -or
        !(Test-Path -LiteralPath $OutputPath -PathType Leaf) -or
        (Get-Item -LiteralPath $OutputPath).Length -le 0) {
        throw "ffmpeg failed to encode native LIVE proof video '$Label'."
    }
    $ProbeLog = Join-Path $FramesRoot ("{0}-ffprobe.json" -f $Label)
    $ProbeArgs = @(
        "-v", "error", "-count_frames", "-select_streams", "v:0",
        "-show_entries",
        "stream=width,height,r_frame_rate,avg_frame_rate,time_base,nb_frames,nb_read_frames",
        "-of", "json", $OutputPath
    )
    $ProbeRun = Invoke-Logged -Command $Ffprobe.Source -Arguments $ProbeArgs `
        -LogPath $ProbeLog
    if ($ProbeRun.exit_code -ne 0) {
        throw "ffprobe rejected native LIVE proof video '$Label'."
    }
    $ProbeJson = Get-Content -LiteralPath $ProbeLog -Raw | ConvertFrom-Json
    $Streams = @($ProbeJson.streams)
    if ($Streams.Count -ne 1) {
        throw "ffprobe returned an unexpected video stream count for '$Label'."
    }
    $Stream = $Streams[0]
    if ([int]$Stream.width -ne 640 -or [int]$Stream.height -ne 480 -or
        [string]$Stream.r_frame_rate -ne $NativeFrameRate -or
        [string]$Stream.avg_frame_rate -ne $NativeFrameRate -or
        [string]$Stream.time_base -ne $NativeVideoTimeBase -or
        [int]$Stream.nb_frames -ne $FrameCount -or
        [int]$Stream.nb_read_frames -ne $FrameCount) {
        throw "ffprobe native LIVE proof cadence/count contract failed for '$Label'."
    }
    $DecodeLog = Join-Path $FramesRoot ("{0}-decoded-framemd5.log" -f $Label)
    $DecodeArgs = @("-v", "error", "-i", $OutputPath, "-map", "0:v:0",
        "-f", "framemd5", "-")
    $DecodeRun = Invoke-Logged -Command $Ffmpeg.Source -Arguments $DecodeArgs `
        -LogPath $DecodeLog
    if ($DecodeRun.exit_code -ne 0) {
        throw "ffmpeg failed to decode native LIVE proof video '$Label'."
    }
    $FrameLines = @(Get-Content -LiteralPath $DecodeLog | Where-Object {
        $_ -match '^\s*\d+,.*,[ \t]*[0-9a-fA-F]{32}\s*$'
    })
    if ($FrameLines.Count -ne $FrameCount) {
        throw "Decoded native LIVE proof video '$Label' has $($FrameLines.Count) frames; expected $FrameCount."
    }
    $FrameHashes = @($FrameLines | ForEach-Object {
        (($_ -split ',')[-1]).Trim().ToUpperInvariant()
    })
    $DecodedHashPath = Join-Path $FramesRoot ("{0}-decoded-frame-hashes.txt" -f $Label)
    [IO.File]::WriteAllLines($DecodedHashPath, [string[]]$FrameHashes)
    return [pscustomobject]@{
        label = $Label
        path = [IO.Path]::GetFullPath($OutputPath)
        sha256 = (Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash
        stored_frame_count = [int]$Stream.nb_frames
        decoded_frame_count = [int]$Stream.nb_read_frames
        decoded_frame_sha256 = (Get-FileHash -LiteralPath $DecodedHashPath -Algorithm SHA256).Hash
        decoded_frame_hashes = $FrameHashes
        probe = [pscustomobject]@{
            width = [int]$Stream.width
            height = [int]$Stream.height
            r_frame_rate = [string]$Stream.r_frame_rate
            avg_frame_rate = [string]$Stream.avg_frame_rate
            time_base = [string]$Stream.time_base
            nb_frames = [int]$Stream.nb_frames
            nb_read_frames = [int]$Stream.nb_read_frames
        }
        commands = @{
            ffmpeg = ((@($Ffmpeg.Source) + $EncodeArgs) -join " ")
            ffprobe = ((@($Ffprobe.Source) + $ProbeArgs) -join " ")
            decode = ((@($Ffmpeg.Source) + $DecodeArgs) -join " ")
        }
        tools = [pscustomobject]@{
            ffmpeg = (@(& $Ffmpeg.Source "-hide_banner" "-version" 2>&1 |
                Select-Object -First 1) -join "")
            ffprobe = (@(& $Ffprobe.Source "-hide_banner" "-version" 2>&1 |
                Select-Object -First 1) -join "")
        }
    }
}

function Get-OriginalReferenceProof {
    param([string]$ManifestPath)
    if (!$ManifestPath) {
        return [pscustomobject]@{
            status = "PENDING_ORIGINAL_REFERENCE_MANIFEST"
            classification = "immutable CPU formal proof; PNG 256x224 raster and separate 256x240 video contract; no AVI required"
            manifest_path = $null
            video_contract = "256x240 original AVI/video contract (separate from PNG raster)"
            runs = @()
        }
    }
    if (!(Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
        throw "Original CPU proof manifest is missing: $ManifestPath"
    }
    try {
        $Manifest = Get-Content -LiteralPath $ManifestPath -Raw |
            ConvertFrom-Json
    } catch {
        throw "Original CPU proof manifest is malformed."
    }
    $Original = $Manifest.original_reference
    if ($null -eq $Original -or
        [string]$Original.video_resolution -ne
            "256x240 original AVI/video contract (separate from PNG raster)" -or
        [string]$Original.contact_sheet_dimensions -ne "768x896" -or
        [int]$Original.run_count -ne 2) {
        throw "Original CPU proof manifest lacks the accepted separate video/contact contract."
    }
    $ManifestRoot = Split-Path -Parent $ManifestPath
    $Runs = @($Original.runs)
    if ($Runs.Count -ne 2) { throw "Original CPU proof manifest lacks two runs." }
    $RunRecords = @()
    foreach ($Run in $Runs) {
        $RunRoot = Join-Path $ManifestRoot ([string]$Run.label)
        $Frames = @($Run.reference_frames)
        if ($Frames.Count -ne 12) {
            throw "Original CPU proof run '$($Run.label)' lacks 12 PNG records."
        }
        $FrameRecords = @()
        for ($Index = 1; $Index -le 12; ++$Index) {
            $ExpectedName = "reference-frame-{0:D4}.png" -f $Index
            $Record = @($Frames | Where-Object { $_.name -eq $ExpectedName })
            if ($Record.Count -ne 1 -or $Record[0].dimensions -ne "256x224") {
                throw "Original CPU proof run '$($Run.label)' has an invalid $ExpectedName record."
            }
            $Path = Join-Path $RunRoot $ExpectedName
            if (!(Test-Path -LiteralPath $Path -PathType Leaf) -or
                (Get-PngDimensions $Path).width -ne 256 -or
                (Get-PngDimensions $Path).height -ne 224 -or
                (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne
                    $Record[0].sha256) {
                throw "Original CPU proof PNG '$Path' failed presence/dimension/hash validation."
            }
            $FrameRecords += [pscustomobject]@{
                path = [IO.Path]::GetFullPath($Path)
                sha256 = $Record[0].sha256
                dimensions = "256x224"
            }
        }
        $Contact = $Run.contact_sheet
        $ContactPath = Join-Path $RunRoot ([string]$Contact.name)
        if ($Contact.name -ne "reference-contact-sheet.png" -or
            $Contact.dimensions -ne "768x896" -or
            !(Test-Path -LiteralPath $ContactPath -PathType Leaf) -or
            (Get-PngDimensions $ContactPath).width -ne 768 -or
            (Get-PngDimensions $ContactPath).height -ne 896 -or
            (Get-FileHash -LiteralPath $ContactPath -Algorithm SHA256).Hash -ne
                $Contact.sha256 -or
            $Contact.sha256 -ne $ExpectedOriginalContactSheetSha) {
            throw "Original CPU proof contact sheet '$ContactPath' failed accepted hash/dimension validation."
        }
        $RunRecords += [pscustomobject]@{
            label = $Run.label
            contact_sheet = [pscustomobject]@{
                path = [IO.Path]::GetFullPath($ContactPath)
                sha256 = $Contact.sha256
                dimensions = "768x896"
            }
            reference_frames = $FrameRecords
        }
    }
    if ($RunRecords[0].contact_sheet.sha256 -ne
            $RunRecords[1].contact_sheet.sha256) {
        throw "Original CPU proof contact sheets are not deterministic."
    }
    return [pscustomobject]@{
        status = "validated"
        classification = "immutable CPU formal proof; PNG raster 256x224 and separate 256x240 video contract; no original AVI required"
        manifest_path = [IO.Path]::GetFullPath($ManifestPath)
        video_contract = [string]$Original.video_resolution
        runs = $RunRecords
    }
}

function Get-LiveProofArtifactInventory {
    param([string]$Root, [string]$ManifestName)
    $Records = @()
    foreach ($Item in Get-ChildItem -LiteralPath $Root -File -Recurse |
        Sort-Object FullName) {
        if ($Item.Name -eq $ManifestName) { continue }
        if ($Item.Length -le 0) {
            throw "LIVE proof artifact is empty: $($Item.FullName)"
        }
        $Records += [pscustomobject]@{
            path = [IO.Path]::GetFullPath($Item.FullName)
            bytes = [int64]$Item.Length
            sha256 = (Get-FileHash -LiteralPath $Item.FullName -Algorithm SHA256).Hash
        }
    }
    if ($Records.Count -eq 0) {
        throw "LIVE proof artifact inventory is empty."
    }
    return $Records
}

function Copy-LiveProofLogs {
    param(
        [string]$ScratchRoot,
        [string]$BuildLogSource,
        [bool]$BuildWasRun,
        [string]$ProofRoot
    )
    $LogRoot = Join-Path $ProofRoot "logs"
    New-Item -ItemType Directory -Force -Path $LogRoot | Out-Null
    $Sources = @()
    if ($BuildWasRun) {
        if (!(Test-Path -LiteralPath $BuildLogSource -PathType Leaf) -or
            (Get-Item -LiteralPath $BuildLogSource).Length -le 0) {
            throw "LIVE proof warning-clean build log is missing or empty."
        }
        $Sources += $BuildLogSource
    }
    foreach ($Item in Get-ChildItem -LiteralPath $ScratchRoot -File -Filter '*.log' |
        Sort-Object FullName) {
        if ($Item.Length -le 0) {
            throw "LIVE proof scene/negative log is empty: $($Item.FullName)"
        }
        $Sources += $Item.FullName
    }
    if (!$BuildWasRun) {
        $NoBuildLog = Join-Path $LogRoot "build-not-requested.log"
        Set-Content -LiteralPath $NoBuildLog -Encoding UTF8 `
            -Value "[proof] build.ps1 was not requested; draft only; rerun with -Build."
    }
    if ($Sources.Count -lt 2) {
        throw "LIVE proof lacks the required asset-pack and scene/negative logs."
    }
    $Records = @()
    foreach ($Source in $Sources) {
        $Destination = Join-Path $LogRoot ([IO.Path]::GetFileName($Source))
        Copy-Item -LiteralPath $Source -Destination $Destination -Force
        if (!(Test-Path -LiteralPath $Destination -PathType Leaf) -or
            (Get-Item -LiteralPath $Destination).Length -le 0) {
            throw "LIVE proof failed to preserve log '$Source'."
        }
        $Records += [pscustomobject]@{
            path = [IO.Path]::GetFullPath($Destination)
            bytes = [int64](Get-Item -LiteralPath $Destination).Length
            sha256 = (Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash
        }
    }
    if (!$BuildWasRun) {
        $NoBuildLogPath = Join-Path $LogRoot "build-not-requested.log"
        $NoBuildLogItem = Get-Item -LiteralPath $NoBuildLogPath
        $Records += [pscustomobject]@{
            path = [IO.Path]::GetFullPath($NoBuildLogItem.FullName)
            bytes = [int64]$NoBuildLogItem.Length
            sha256 = (Get-FileHash -LiteralPath $NoBuildLogItem.FullName -Algorithm SHA256).Hash
        }
    }
    return $Records
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
    $ProofStartUtc = [DateTime]::UtcNow.ToString("o")
    $InitialGitProofState = Get-LiveProofGitState
    $SyntheticDirtyRequirePassState = [pscustomobject]@{
        head = ('a' * 40)
        branch = $ExpectedBranch
        clean = $false
        status = @(' M synthetic-proof-input')
        base_sha = $ExpectedBaseSha
        base_is_ancestor = $true
    }
    $SyntheticWrongBranchRequirePassState = [pscustomobject]@{
        head = ('b' * 40)
        branch = 'codex/unrelated-proof-input'
        clean = $true
        status = @()
        base_sha = $ExpectedBaseSha
        base_is_ancestor = $true
    }
    $SyntheticWrongBaseRequirePassState = [pscustomobject]@{
        head = ('c' * 40)
        branch = $ExpectedBranch
        clean = $true
        status = @()
        base_sha = $ExpectedBaseSha
        base_is_ancestor = $false
    }
    $SyntheticDirtyAccepted =
        Test-LiveProofRequirePassState $SyntheticDirtyRequirePassState
    $SyntheticWrongBranchAccepted =
        Test-LiveProofRequirePassState $SyntheticWrongBranchRequirePassState
    $SyntheticWrongBaseAccepted =
        Test-LiveProofRequirePassState $SyntheticWrongBaseRequirePassState
    if ($SyntheticDirtyAccepted -or $SyntheticWrongBranchAccepted -or
        $SyntheticWrongBaseAccepted) {
        throw "LIVE proof synthetic RequirePass rejection negative was accepted."
    }
    $RequirePassDirtyNegative = $true
    $RequirePassWrongBranchNegative = $true
    $RequirePassWrongBaseNegative = $true
    if ($RequirePass -and !$Build) {
        throw "LIVE proof -RequirePass requires -Build for a preserved warning-clean build log."
    }
    if ($RequirePass) {
        if (!(Test-LiveProofRequirePassState $InitialGitProofState)) {
            throw ("LIVE proof -RequirePass rejected dirty or non-ancestral input: " +
                "branch=$($InitialGitProofState.branch) " +
                "clean=$($InitialGitProofState.clean) " +
                "base_is_ancestor=$($InitialGitProofState.base_is_ancestor)")
        }
    }
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
    $BuildLog = Join-Path $BuildDir "gameplay-scene-build.log"
    if ($Build) {
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
    $ProofPackDirectory = Join-Path $ProofRoot "asset-pack"
    New-Item -ItemType Directory -Force -Path $ProofPackDirectory | Out-Null
    $ProofPackPath = Join-Path $ProofPackDirectory "gameplay-proof.assetpack"
    Copy-Item -LiteralPath $PackPath -Destination $ProofPackPath -Force
    if (!(Test-Path -LiteralPath $ProofPackPath -PathType Leaf) -or
        (Get-Item -LiteralPath $ProofPackPath).Length -le 0 -or
        (Get-FileHash -LiteralPath $ProofPackPath -Algorithm SHA256).Hash -ne
            (Get-FileHash -LiteralPath $PackPath -Algorithm SHA256).Hash) {
        throw "Preserved LIVE proof asset-pack copy failed exact hash validation."
    }

    $PackBytes = [IO.File]::ReadAllBytes($PackPath)
    $Specs = @(
        [pscustomobject]@{ id="gameplay/core"; size=23416; hash="2047CCE0"; schema="tecmo.gameplay/TGPL-1" },
        [pscustomobject]@{ id="gameplay/pre-tip"; size=7680; hash="8E6367FC"; schema="tecmo.gameplay-pre-tip/TPTI-2" },
        [pscustomobject]@{ id="gameplay/court"; size=6559; hash="ECAB7A93"; schema="tecmo.gameplay-court/TGCT-1" },
        [pscustomobject]@{ id="gameplay/camera-projection"; size=1536; hash="53247856"; schema="tecmo.gameplay-camera/TGCP-2" },
        [pscustomobject]@{ id="gameplay/movement"; size=1664; hash="6C82A137"; schema="tecmo.gameplay-movement/TGMO-1" },
        [pscustomobject]@{ id="gameplay/ball-dribble"; size=608; hash="E2CE6BFF"; schema="tecmo.gameplay-ball-dribble/TGBD-1" },
        [pscustomobject]@{ id="gameplay/fatigue"; size=512; hash="F80F170D"; schema="tecmo.gameplay-fatigue/TGFT-1" },
        [pscustomobject]@{ id="gameplay/cpu-steering"; size=8016; hash="D56EE070"; schema="tecmo.gameplay-cpu-steering/TGAI-3" },
        [pscustomobject]@{ id="gameplay/hud"; size=864; hash="3D13AA89"; schema="tecmo.gameplay-hud/THUD-1" },
        [pscustomobject]@{ id="gameplay/court-orientation"; size=640; hash="44B0C44E"; schema="tecmo.gameplay-court-orientation/TGOR-1" },
        [pscustomobject]@{ id="gameplay/backcourt"; size=512; hash="810886EF"; schema="tecmo.gameplay-backcourt/TGBC-1" },
        [pscustomobject]@{ id="gameplay/free-throw-lineup"; size=1216; hash="B17B9A3F"; schema="tecmo.gameplay-free-throw-lineup/TGFL-1" },
        [pscustomobject]@{ id="gameplay/close-shots"; size=3144; hash="DACDC976"; schema="tecmo.gameplay-close-shots/TGCS-1" },
        [pscustomobject]@{ id="gameplay/dunk-cutaway"; size=20272; hash="E02F2D21"; schema="tecmo.gameplay-dunk-cutaway/TGDK-1" },
        [pscustomobject]@{ id="gameplay/jump-shots"; size=2776; hash="A66EE873"; schema="tecmo.gameplay-jump-shots/TGJS-2" },
        [pscustomobject]@{ id="gameplay/shot-resolution"; size=608; hash="5376E82B"; schema="tecmo.gameplay-shot-resolution/TGSR-4" },
        [pscustomobject]@{ id="gameplay/penalties"; size=768; hash="980DDC76"; schema="tecmo.gameplay-penalties/TPNL-1" },
        [pscustomobject]@{ id="gameplay/violation-referee"; size=4752; hash="2EB08CF0"; schema="tecmo.gameplay-violation-referee/TGVR-1" },
        [pscustomobject]@{ id="audio/music"; size=36784; hash="05C00ECB"; schema="tecmo.music/TMUS-1" },
        [pscustomobject]@{ id="audio/gameplay-sfx"; size=2824; hash="968A5DE6"; schema="tecmo.gameplay-audio/TSFX-1" },
        [pscustomobject]@{ id="audio/gameplay-dmc"; size=2515; hash="AD70E6E8"; schema="tecmo.gameplay-audio/TDMC-1" },
        [pscustomobject]@{ id="chr/all"; size=262144; hash="F6F6E854"; schema=$null }
    )
    $SourceMapEntry = Get-AssetPackEntry $PackBytes "system/source-map"
    $SourceMapText = [Text.Encoding]::UTF8.GetString(
        (Get-EntryBytes $PackBytes $SourceMapEntry))
    $SourceMap = $SourceMapText | ConvertFrom-Json
    $PreTipStaleEntry = Get-AssetPackEntry $PackBytes "gameplay/pre-tip"
    $PreTipStaleMap = @($SourceMap.logical_entries | Where-Object {
        $_.id -eq "gameplay/pre-tip"
    })
    if ($PreTipStaleEntry.byte_count -eq 5888 -or
        (Get-Fnv1a32 (Get-EntryBytes $PackBytes $PreTipStaleEntry)) -eq
            "99ADFE3D" -or
        ($PreTipStaleMap.Count -eq 1 -and
         $PreTipStaleMap[0].schema -eq
             "tecmo.gameplay-pre-tip/TPTI-1")) {
        throw "Named stale-TPTI-1 metadata rejection failed."
    }
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
    $MovementMaps = @($SourceMap.logical_entries | Where-Object {
        $_.id -eq "gameplay/movement"
    })
    $CpuMaps = @($SourceMap.logical_entries | Where-Object {
        $_.id -eq "gameplay/cpu-steering"
    })
    $LiveEvidence = if ($CpuMaps.Count -eq 1) {
        $CpuMaps[0].live_foundation_integration.evidence
    } else { $null }
    $Opcode15Contract = if ($CpuMaps.Count -eq 1) {
        $CpuMaps[0].opcode15_source_contract
    } else { $null }
    $BallDribbleMaps = @($SourceMap.logical_entries | Where-Object {
        $_.id -eq "gameplay/ball-dribble"
    })
    $FatigueMaps = @($SourceMap.logical_entries | Where-Object {
        $_.id -eq "gameplay/fatigue"
    })
    $CourtMaps = @($SourceMap.logical_entries | Where-Object {
        $_.id -eq "gameplay/court"
    })
    $LineupMaps = @($SourceMap.logical_entries | Where-Object {
        $_.id -eq "gameplay/free-throw-lineup"
    })
    $HudMaps = @($SourceMap.logical_entries | Where-Object {
        $_.id -eq "gameplay/hud"
    })
    $ShotResolutionMaps = @($SourceMap.logical_entries | Where-Object {
        $_.id -eq "gameplay/shot-resolution"
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
            "770FAE95" -or
        $CourtMaps[0].native_contract.scene_slice.native_checkpoints.center_render_fnv1a32 -ne
            "6E530421" -or
        $CourtMaps[0].native_contract.scene_slice.native_checkpoints.right_render_fnv1a32 -ne
            "2DBDF155" -or
        [bool]$CourtMaps[0].native_contract.scene_slice.integration_is_additional_rom_claim -or
        $CourtMaps[0].native_contract.live_palette_matchup -notmatch
            "3/7/11/15" -or
        $CourtMaps[0].native_contract.boundary -notmatch
            "production camera-positioned live viewport") {
        throw "Production TGCP-2/TGCT-1 scene provenance is incomplete."
    }
    if ($MovementMaps.Count -ne 1 -or
        $MovementMaps[0].fingerprint_fnv1a32 -ne "6C82A137" -or
        @($MovementMaps[0].dependencies).Count -ne 3 -or
        @($MovementMaps[0].source_spans).Count -ne 7 -or
        $MovementMaps[0].native_contract.direction_change_latency_updates -ne 1 -or
        $MovementMaps[0].native_contract.movement_fractional_bits -ne 4 -or
        $MovementMaps[0].native_contract.condition_formula -notmatch
            "adjusted_rating" -or
        (@($MovementMaps[0].native_contract.game_speed_adjustments) -join ',') -ne
            '5,-1,-6' -or
        ![bool]$MovementMaps[0].native_contract.transactional -or
        ![bool]$MovementMaps[0].native_contract.overflow_rejected -or
        $MovementMaps[0].live_adapter.scope -notmatch "user-controlled" -or
        $MovementMaps[0].live_adapter.scope -notmatch "TGAI-directed CPU" -or
        $MovementMaps[0].live_adapter.condition -notmatch "TGFT-1 evolves" -or
        $MovementMaps[0].live_adapter.boundary_latch_reset_and_settlement -notmatch
            "TPNL selector 1" -or
        $MovementMaps[0].live_adapter.pose_half_selection -notmatch '\$8F02' -or
        $MovementMaps[0].live_adapter.starting_layout -notmatch
            "Bank04 AC76.*exact source evidence.*native post-tip stable layout.*native-faithful/inferred" -or
        $MovementMaps[0].live_adapter.roster_binding -notmatch
            "production binds selected TTDT starters" -or
        $MovementMaps[0].live_adapter.matchup_link -notmatch
            "fixed-link seed values.*dynamic matchup.*inferred" -or
        $MovementMaps[0].live_adapter.cpu_target_and_shot_policy -notmatch
            "live-wired" -or
        $MovementMaps[0].live_adapter.cpu_target_and_shot_policy -notmatch
            "deferred/non-launch" -or
        ![bool]$MovementMaps[0].developer_harness.deterministic -or
        [bool]$MovementMaps[0].developer_harness.normal_game_flow_exposed) {
        throw "Production TGMO-1 movement provenance is incomplete."
    }
    if ($CpuMaps.Count -ne 1 -or
        $CpuMaps[0].fingerprint_fnv1a32 -ne "D56EE070" -or
        @($CpuMaps[0].source_spans).Count -ne 12 -or
        $LiveEvidence.rom.revision -ne "Rev1" -or
        $LiveEvidence.rom.length -ne 393232 -or
        $LiveEvidence.rom.sha256 -ne $ExpectedRomSha256 -or
        $LiveEvidence.bank03_starter_commit.bank -ne 3 -or
        $LiveEvidence.bank03_starter_commit.address -ne '$8FC2-$9102' -or
        $LiveEvidence.bank03_starter_commit.byte_count -ne 321 -or
        $LiveEvidence.bank03_starter_commit.sha256 -ne
            "FA3B396D01581451717CEB44A0F5628560FC664191E8F15F5843B0EAB316A9F5" -or
        $LiveEvidence.bank04_static_setup.bank -ne 4 -or
        $LiveEvidence.bank04_static_setup.address -ne '$AC76-$ADDF' -or
        $LiveEvidence.bank04_static_setup.byte_count -ne 362 -or
        $LiveEvidence.bank04_static_setup.sha256 -ne
            "E123614333986D9D5084678C9AE32DD3A1A28ABF52F6D6265FE749FC0070C6E0" -or
        $LiveEvidence.bank04_starter_stage.bank -ne 4 -or
        $LiveEvidence.bank04_starter_stage.address -ne '$ADE0-$ADF3' -or
        $LiveEvidence.bank04_starter_stage.byte_count -ne 20 -or
        $LiveEvidence.bank04_starter_stage.sha256 -ne
            "B4CC98CF95216620E6DAAB21C71BC1D9A679AFE9BB8BE5DC455A239E07640A3B" -or
        (@($LiveEvidence.positions | ForEach-Object {
            "{0},{1}" -f $_[0], $_[1]
        }) -join ';') -ne
            '528,144;448,144;362,112;364,192;392,144;176,144;320,144;408,112;400,192;372,144' -or
        (@($LiveEvidence.directions) -join ',') -ne '1,1,2,5,1,0,0,2,5,0' -or
        (@($LiveEvidence.fixed_links) -join ',') -ne '5,6,7,8,9,0,1,2,3,4' -or
        $LiveEvidence.static_seeds.primary -ne 4 -or
        $LiveEvidence.static_seeds.defender -ne 9 -or
        (@($LiveEvidence.static_seeds.matchup) -join ',') -ne '2,7' -or
        $LiveEvidence.lineup_binding.staging -ne 'exact' -or
        $LiveEvidence.lineup_binding.session_to_slot -notmatch 'native-faithful/inferred' -or
        $LiveEvidence.lineup_binding.one_for_one_staged_to_slot -notmatch
            'not directly proven' -or
        ![bool]$CpuMaps[0].live_foundation_integration.live_wired -or
        ![bool]$CpuMaps[0].live_foundation_integration.transactional -or
        $CpuMaps[0].live_foundation_integration.formation_selector.source_pinned_starts -ne 46 -or
        $CpuMaps[0].live_foundation_integration.formation_selector.theoretical_count -ne 48 -or
        (@($CpuMaps[0].live_foundation_integration.formation_selector.rejected_indices) -join ',') -ne
            '46,47' -or
        $CpuMaps[0].live_foundation_integration.formation_selector.index_formula -notmatch
            'depth_row\*12\+x_bucket' -or
        !$CpuMaps[0].live_foundation_integration.play_state.source_command_advance -or
        $CpuMaps[0].live_foundation_integration.fixed_opposing_link_use -notmatch
            'fixed links.*not claimed as ROM dynamic assignment' -or
        $CpuMaps[0].live_foundation_integration.source_direction_application -notmatch
            'target-to-direction equivalence' -or
        $CpuMaps[0].live_foundation_integration.source_target_policy -notmatch
            'current LIVE compatibility movement.*canonical opcode 4 captures once.*bound state-5 kernel' -or
        ![bool]$CpuMaps[0].live_foundation_integration.state5_route.live_wired -or
        ![bool]$CpuMaps[0].live_foundation_integration.state5_route.selected_primary_first -or
        (@($CpuMaps[0].live_foundation_integration.state5_route.ordinary_order) -join ',') -ne
            '9,8,7,6,5,4,3,2,1,0' -or
        ![bool]$CpuMaps[0].live_foundation_integration.state5_route.selected_defender_excluded -or
        ![bool]$CpuMaps[0].live_foundation_integration.state5_route.tgmo_bypassed -or
        ![bool]$CpuMaps[0].live_foundation_integration.state5_route.target_frozen -or
        ![bool]$CpuMaps[0].live_foundation_integration.state5_route.transactional_lifecycle_cancel -or
        $CpuMaps[0].live_foundation_integration.shot_request_adapter -notmatch
            'deferred/non-launch' -or
        ![bool]$CpuMaps[0].live_foundation_integration.classifications.formation_source_pinned -or
        ![bool]$CpuMaps[0].live_foundation_integration.classifications.native_matchup_inferred -or
        ![bool]$CpuMaps[0].live_foundation_integration.classifications.workspace_native_approximation -or
        ![bool]$CpuMaps[0].live_foundation_integration.classifications.shot_request_native_approximation -or
        $CpuMaps[0].live_foundation_integration.play_state.step_budget -ne 1 -or
        ![bool]$CpuMaps[0].live_foundation_integration.play_state.deferred_effects_explicit -or
        ![bool]$CpuMaps[0].live_foundation_integration.normal_game_flow_exposed) {
        throw "Production TGAI-3 LIVE adapter provenance is incomplete."
    }
    if ($null -eq $Opcode15Contract -or
        $Opcode15Contract.scope -ne "harness-only; LIVE opcode 15 remains deferred" -or
        $Opcode15Contract.dispatch.bank -ne 6 -or
        $Opcode15Contract.dispatch.address -ne '$8B90-$8BE0' -or
        $Opcode15Contract.dispatch.handler -ne '$9172' -or
        @($Opcode15Contract.canonical_records).Count -ne 2 -or
        $Opcode15Contract.canonical_records[0].stream_offset -ne '$0037' -or
        $Opcode15Contract.canonical_records[1].stream_offset -ne '$004B' -or
        @($Opcode15Contract.semantic_anchors).Count -ne 3 -or
        $Opcode15Contract.semantic_anchors[0].address -ne '$9146-$9216' -or
        $Opcode15Contract.semantic_anchors[1].address -ne '$9208-$9216' -or
        ![bool]$Opcode15Contract.semantic_anchors[1].overlaps_parent_source -or
        $Opcode15Contract.semantic_anchors[2].address -ne '$88B0-$88D9' -or
        $Opcode15Contract.lifted_source_discrepancy.authority -ne "canonical Rev1 ROM" -or
        $Opcode15Contract.lifted_source_discrepancy.lifted_listing_omits -ne '$9208-$9211' -or
        $Opcode15Contract.c711.selector -ne 4 -or
        ![bool]$Opcode15Contract.c711.observed_unexecuted -or
        $Opcode15Contract.conditional_06d5.gate -ne
            '$91F1-$91F5: CPX $06D5; BNE' -or
        $Opcode15Contract.conditional_06d5.store -ne
            '$91F6-$91F8: STY $06D5' -or
        $Opcode15Contract.conditional_06d5.'when' -ne 'new X == $06D5' -or
        $Opcode15Contract.conditional_06d5.'then' -ne '$06D5=old Y' -or
        $Opcode15Contract.conditional_06d5.'otherwise' -ne 'preserve $06D5' -or
        $Opcode15Contract.live_missing_raw_reason -notmatch 'deferred_missing_raw_0499' -or
        $Opcode15Contract.natural_fceux_capture -notmatch
            'synthetic.*not a natural \$91C8 capture') {
        throw "TGAI-3 opcode-15 raw-owner provenance is incomplete."
    }
    if ($BallDribbleMaps.Count -ne 1 -or
        $BallDribbleMaps[0].fingerprint_fnv1a32 -ne "E2CE6BFF" -or
        @($BallDribbleMaps[0].dependencies).Count -ne 2 -or
        ![bool]$BallDribbleMaps[0].dependencies[0].same_pack_required -or
        ![bool]$BallDribbleMaps[0].dependencies[1].same_pack_required -or
        @($BallDribbleMaps[0].source_spans).Count -ne 2 -or
        $BallDribbleMaps[0].source_spans[0].fingerprint_fnv1a32 -ne
            "DB540670" -or
        $BallDribbleMaps[0].source_spans[1].fingerprint_fnv1a32 -ne
            "E9784D28" -or
        $BallDribbleMaps[0].native_contract.bounce_height -notmatch
            '\$B5C8' -or
        $BallDribbleMaps[0].native_contract.sound_trigger -notmatch
            "low nibble 3" -or
        ![bool]$BallDribbleMaps[0].native_contract.transactional -or
        $BallDribbleMaps[0].live_adapter.scope -notmatch "human and CPU" -or
        $BallDribbleMaps[0].live_adapter.altitude_projection -notmatch
            "flattened into canonical visible Y" -or
        $BallDribbleMaps[0].live_adapter.matchup_link -notmatch
            "fixed-link seed values.*dynamic matchup.*inferred") {
        throw "Production TGBD-1 held-ball provenance is incomplete."
    }
    if ($FatigueMaps.Count -ne 1 -or
        $FatigueMaps[0].live_adapter.active_roster_binding -notmatch
            "selected TTDT roster indices 0\.\.11.*stable local slots 0\.\.4.*native-faithful/inferred") {
        throw "Production TGFT-1 active-roster provenance is incomplete."
    }
    if ($LineupMaps.Count -ne 1 -or
        $LineupMaps[0].live_scene_integration.orientation_source -notmatch
            "TGOR-1 attack_direction" -or
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
    if ($HudMaps.Count -ne 1 -or
        $HudMaps[0].fingerprint_fnv1a32 -ne "3D13AA89" -or
        @($HudMaps[0].dependencies).Count -ne 3 -or
        (@($HudMaps[0].dependencies | ForEach-Object {
            [string]$_.entry }) -join ',') -ne
            'gameplay/core,menu/team-data,chr/all' -or
        @($HudMaps[0].source_spans).Count -ne 3 -or
        [int]$HudMaps[0].exact_contract.team_mark_rows -ne 29 -or
        [int]$HudMaps[0].exact_contract.team_mark_width_tiles -ne 5 -or
        [int]$HudMaps[0].exact_contract.font_chr_selector -ne 0xFA -or
        $HudMaps[0].exact_contract.font_chr_dependency -notmatch
            'TTDT-1.*\$FA' -or
        $HudMaps[0].live_scene_integration.row_2 -notmatch 'clock' -or
        $HudMaps[0].live_scene_integration.row_3 -notmatch
            'jersey number.*selected player' -or
        $HudMaps[0].live_scene_integration.row_3 -notmatch
            'no shot clock or period label' -or
        $HudMaps[0].live_scene_integration.placement_status -notmatch
            'reference-verified' -or
        $HudMaps[0].live_scene_integration.selected_cpu_actor_policy -notmatch
            'native adapter' -or
        $HudMaps[0].runtime_inputs -match
            'decompilation file|capture file|screenshot file') {
        throw "Production THUD-1 live HUD provenance is incomplete."
    }
    $ClaimantBridge = if ($ShotResolutionMaps.Count -eq 1) {
        $ShotResolutionMaps[0].claimant_settlement_bridge
    } else { $null }
    if ($ShotResolutionMaps.Count -ne 1 -or
        $ClaimantBridge.source -notmatch 'Bank05 \$BA56-\$BA9C.*\$B87C-\$B98A.*\$9042-\$9053.*\$B98B-\$B994' -or
        $ClaimantBridge.caller_paths -notmatch '\$A214 state-\$11 dispatch -> \$BA56' -or
        $ClaimantBridge.caller_paths -notmatch '\$B751 -> \$BA65' -or
        $ClaimantBridge.caller_paths -notmatch '\$B180 -> \$BA8C' -or
        $ClaimantBridge.fingerprints.caller_BA56_BA9C -ne 'B779AC48' -or
        $ClaimantBridge.fingerprints.settlement_B87C_B8F5 -ne '9E2F1F28' -or
        $ClaimantBridge.fingerprints.caller_prefix_B87C_B888 -ne 'E903D8F9' -or
        $ClaimantBridge.fingerprints.claimant_context_B73E_B87B -ne '574FEE44' -or
        $ClaimantBridge.fingerprints.toggle_9042_9053 -ne 'CE6C9466' -or
        $ClaimantBridge.fingerprints.remap_B98B_B994 -ne '404311FE' -or
        $ClaimantBridge.caller_predicates -notmatch '\$B8C1.*candidate != old \$0308' -or
        $ClaimantBridge.caller_predicates -notmatch '\$B8CE.*\$04B0 bit \$10' -or
        $ClaimantBridge.native_entrypoint -notmatch 'terminal miss claimant only' -or
        $ClaimantBridge.native_entrypoint -notmatch 'scene_finish_shot/scene_finish_jump_miss' -or
        $ClaimantBridge.typed_effects -notmatch '\$9042 X=9\.\.0 \$04B0 bit-\$10 XOR' -or
        $ClaimantBridge.typed_effects -notmatch '\$B98B candidate remap' -or
        $ClaimantBridge.diagnostic -notmatch 'TGPS-1.*TGLP-1.*console-only' -or
        $ClaimantBridge.not_wired -notmatch 'generic/made/restart/tip/steal/foul/unproven recovery' -or
        $ClaimantBridge.unsupported -notmatch '\$035A/\$035B mutation' -or
        [bool]$ClaimantBridge.integration_is_additional_rom_claim) {
        throw "Bank05 B87C claimant bridge provenance is incomplete."
    }

    $HudLog = Join-Path $Scratch "gameplay-hud-assets.log"
    $HudRun = Invoke-Logged -Command $Executable -Arguments @(
        "--gameplay-hud-test", $PackPath
    ) -LogPath $HudLog
    if ($HudRun.exit_code -ne 0 -or
        $HudRun.tail -notmatch
            "THUD-1 strict gameplay HUD self-test passed") {
        throw "Strict gameplay HUD asset test failed.`n$($HudRun.tail)"
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

    # TGLP-1 executable LIVE proof seam. Each invocation starts a fresh bound
    # non-identity direct scene launch, runs real PRETIP (no skip hook), and
    # renders through the production TecmoRuntime court renderer. The flow
    # wrapper separately proves the game.c production launch bridge.
    $ProofEvents = @(
        "pretip-start",
        "live-handoff",
        "human-movement",
        "offensive-pass",
        "defensive-switch",
        "cpu-target-deferred",
        "actor-command-assignment-deferred",
        "cpu-primary-stream-step",
        "cpu-auto-pass-opcode5",
        "cpu-auto-pass-action10",
        "cpu-auto-pass-gather",
        "cpu-auto-pass-stream",
        "shot-path",
        "claimant-settlement",
        "defensive-foul-presentation"
    )
    New-Item -ItemType Directory -Force -Path $ProofRoot | Out-Null
    $ProofRecords = @()
    for ($Repeat = 1; $Repeat -le 2; ++$Repeat) {
        $RepeatRoot = Join-Path $ProofRoot ("repeat-{0}" -f $Repeat)
        New-Item -ItemType Directory -Force -Path $RepeatRoot | Out-Null
        foreach ($Event in $ProofEvents) {
            $Png = Join-Path $RepeatRoot ("{0}.png" -f $Event)
            $Log = Join-Path $RepeatRoot ("{0}.jsonl" -f $Event)
            $Run = Invoke-Logged -Command $Executable -Arguments @(
                "--root", $ProjectRoot,
                "--gameplay-live-foundation-proof", $PackPath,
                $Event, $Png
            ) -LogPath $Log
            if ($Run.exit_code -ne 0 -or
                !(Test-Path -LiteralPath $Png -PathType Leaf)) {
                throw "LIVE proof event '$Event' repeat $Repeat failed.`n$($Run.tail)"
            }
            $Lines = @(
                Get-Content -LiteralPath $Log |
                    Where-Object { $_.Trim().Length -ne 0 }
            )
            if ($Lines.Count -ne 1) {
                throw "LIVE proof event '$Event' did not emit exactly one JSON state line."
            }
            try {
                $State = $Lines[0] | ConvertFrom-Json
            } catch {
                throw "LIVE proof event '$Event' emitted malformed JSON state."
            }
            $Dimensions = Get-PngDimensions $Png
            if ($State.schema -ne "tecmo.live-proof/TGLP-1" -or
                $State.event -ne $Event -or
                [int]$State.resolution[0] -ne 640 -or
                [int]$State.resolution[1] -ne 480 -or
                [bool]$State.pretip_skip_hook -or
                $Dimensions.width -ne 640 -or $Dimensions.height -ne 480 -or
                ![bool]$State.live.valid -or
                @($State.actors).Count -ne 10 -or
                @($State.starter_roster_index.away).Count -ne 5 -or
                @($State.starter_roster_index.home).Count -ne 5) {
                throw "LIVE proof event '$Event' state/frame contract failed."
            }
            if ($Event -eq "pretip-start" -and
                (![bool]$State.pretip.in_presentation -or
                 ![bool]$State.pretip.is_presentation -or
                 [bool]$State.pretip.live_handoff -or
                 ![bool]$State.pretip.first_sync_pending -or
                 [bool]$State.pretip.synchronized -or
                 [int]$State.live.sync_serial -ne 0 -or
                 [int]$State.ball_holder -ne 255)) {
                throw "LIVE proof pretip-start premature-sync regression failed."
            }
            if ($Event -ne "pretip-start" -and
                ([bool]$State.pretip.in_presentation -or
                 [bool]$State.live.first_sync_pending)) {
                throw "LIVE proof event '$Event' did not reach synchronized LIVE."
            }
            if ($Event -eq "cpu-target-deferred" -and
                ([int]$State.live.source_target_count -lt 1 -or
                 [int]$State.live.deferred_count -lt 1 -or
                 ![bool]$State.opcode4_ball_target.executed -or
                 [string]$State.opcode4_ball_target.record_offset -ne "0000" -or
                 [int]$State.opcode4_ball_target.argument_c8 -ne 10 -or
                 [int]$State.opcode4_ball_target.target_object -ne 10 -or
                 [int]$State.opcode4_ball_target.snapshot_ball[0] -ne
                    [int]$State.opcode4_ball_target.source_target[0] -or
                 [int]$State.opcode4_ball_target.snapshot_ball[1] -ne
                    [int]$State.opcode4_ball_target.source_target[1])) {
                throw "LIVE proof CPU event did not retain target/deferred evidence."
            }
            if ($Event -eq "actor-command-assignment-deferred") {
                $ActorCommandAssignment = $State.actor_command_assignment
                if (![bool]$ActorCommandAssignment.deferred_diagnostic -or
                    [bool]$ActorCommandAssignment.emitted -or
                    [bool]$ActorCommandAssignment.production_mutated -or
                    [bool]$ActorCommandAssignment.direct_fixture_input -or
                    [string]$ActorCommandAssignment.caller_identity -ne "none" -or
                    [string]$ActorCommandAssignment.no_op_reason -ne
                        "missing-source-shaped-object-dispatch-inputs" -or
                    [string]$ActorCommandAssignment.asm -notmatch
                        'Bank05:\$A023-\$A0DC' -or
                    ![bool]$ActorCommandAssignment.missing.object_slot10_state -or
                    ![bool]$ActorCommandAssignment.missing.object_slot10_coordinate -or
                    ![bool]$ActorCommandAssignment.missing.raw_ba_05a1_0499_0588_0067_0068_04af -or
                    ![bool]$ActorCommandAssignment.missing.interaction_9f2f_predecessors -or
                    [int]$ActorCommandAssignment.scene_frame[0] -ne
                        [int]$ActorCommandAssignment.scene_frame[1] -or
                    [int]$ActorCommandAssignment.sync_serial[0] -ne
                        [int]$ActorCommandAssignment.sync_serial[1] -or
                    [int]$ActorCommandAssignment.exclusions.primary -ne
                        [int]$State.live.primary_actor -or
                    [int]$ActorCommandAssignment.exclusions.defender -ne
                        [int]$State.live.defender_actor -or
                    [bool]$ActorCommandAssignment.scans.side10.executed -or
                    [bool]$ActorCommandAssignment.scans.side00.executed -or
                    $null -ne $ActorCommandAssignment.scans.side10.winner -or
                    $null -ne $ActorCommandAssignment.scans.side10.score -or
                    $null -ne $ActorCommandAssignment.scans.side00.winner -or
                    $null -ne $ActorCommandAssignment.scans.side00.score -or
                    [int]$ActorCommandAssignment.selected_before_after.primary.stream[0] -ne
                        [int]$ActorCommandAssignment.selected_before_after.primary.stream[1] -or
                    [int]$ActorCommandAssignment.selected_before_after.primary.state[0] -ne
                        [int]$ActorCommandAssignment.selected_before_after.primary.state[1] -or
                    [int]$ActorCommandAssignment.selected_before_after.defender.stream[0] -ne
                        [int]$ActorCommandAssignment.selected_before_after.defender.stream[1] -or
                    [int]$ActorCommandAssignment.selected_before_after.defender.state[0] -ne
                        [int]$ActorCommandAssignment.selected_before_after.defender.state[1] -or
                    [string]$ActorCommandAssignment.screenshot_scope -notmatch
                        'not A023 gameplay parity') {
                    throw "LIVE proof A023 event fabricated a production command-assignment path."
                }
            }
            if ($Event -eq "shot-path" -and
                (![bool]$State.live.last_shot_request -or
                 ![bool]$State.live.last_shot_playback_supported -or
                 [bool]$State.live.last_shot_deferred -or
                 [int]$State.action_serial -ne 1 -or
                 [int]$State.shot_frame -lt 1)) {
                throw "LIVE proof shot event did not retain exact-once playback state."
            }
            if ($Event -eq "claimant-settlement") {
                $Claimant = $State.claimant_settlement
                $Transaction = $Claimant.transaction
                $Fixture = $Claimant.fixture
                if (![bool]$Claimant.emitted -or
                    [bool]$Claimant.direct_handoff_injection -or
                    $Claimant.entrypoint -ne
                        "tecmo_gameplay_scene_update/normal-B-miss" -or
                    $Claimant.asm -notmatch 'Bank05:\$BA56-\$BA9C' -or
                    $Claimant.asm -notmatch '\$B87C-\$B98A' -or
                    $Claimant.asm -notmatch '\$9042' -or
                    [int]$Claimant.event_serial -le 0 -or
                    [int]$Claimant.updates -le 0 -or
                    $null -eq $Fixture -or
                    ![bool]$Fixture.starts_from_native_pretip_handoff -or
                    [int]$Fixture.shooting_actor -lt 0 -or
                    [int]$Fixture.shooting_actor -ge 10 -or
                    [int]$Fixture.claimant_actor -lt 0 -or
                    [int]$Fixture.claimant_actor -ge 10 -or
                    [int]$Fixture.shooting_actor -eq [int]$Fixture.claimant_actor -or
                    [int]$Transaction.raw_0308_before -ne
                        [int]$Fixture.shooting_actor -or
                    [int]$Transaction.raw_0308_after -ne
                        [int]$Fixture.claimant_actor -or
                    ![bool]$Transaction.side_context_swapped -or
                    ![bool]$Transaction.raw_04b0_bit10_toggled -or
                    ![bool]$Transaction.raw_035a_save_and_toggle_observed -or
                    [bool]$Transaction.automatic_defender_scan_ran -or
                    [bool]$Transaction.automatic_defender_match_found -or
                    $null -eq $Claimant.before -or $null -eq $Claimant.after -or
                    $Claimant.before.contract -ne "TGPS-1" -or
                    $Claimant.after.contract -ne "TGPS-1" -or
                    [int]$Claimant.before.raw.'$0308' -ne
                        [int]$Fixture.shooting_actor -or
                    [int]$Claimant.after.raw.'$0308' -ne
                        [int]$Fixture.claimant_actor -or
                    [int]$Claimant.before.semantic.scene_possession -eq
                        [int]$Claimant.after.semantic.scene_possession -or
                    [int]$Claimant.after.semantic.ball_holder -ne
                        [int]$Fixture.claimant_actor -or
                    ![bool]$Claimant.after.semantic.live_synchronized -or
                    ((@($Claimant.before.raw.'$030C_$030D') -join ',') -ne
                        '0,0') -or
                    @($Claimant.before.raw.'$04B0_bit10_flags').Count -ne 10 -or
                    @($Claimant.after.raw.'$0547_$0551_stream_offset').Count -ne 10 -or
                    @($Claimant.after.raw.'$057C').Count -ne 10) {
                    throw "LIVE proof claimant settlement trace/source-state contract regressed."
                }
                # This event is an existing production scene path, not an
                # opcode-15 fixture. If it naturally fetches a canonical
                # opcode-15 record, the trace must remain a passive raw-owner
                # diagnostic and leave the typed LIVE state unchanged.
                Assert-Opcode15PassiveTrace -Snapshot $Claimant.before -Label "claimant-settlement/before"
                Assert-Opcode15PassiveTrace -Snapshot $Claimant.after -Label "claimant-settlement/after"
            } elseif ([bool]$State.claimant_settlement.emitted) {
                throw "Non-claimant LIVE proof event unexpectedly emitted Bank05 B87C diagnostics."
            }
            if ($Event -eq "cpu-primary-stream-step") {
                $PrimaryStep = $State.cpu_primary_stream_step
                $AsmEvidence = $State.asm_evidence
                if (![bool]$PrimaryStep.proved -or
                    [string]$PrimaryStep.record_offset -ne "0000" -or
                    [int]$PrimaryStep.wait_frames -ne 0 -or
                    ((@($PrimaryStep.stream) -join ',') -ne
                        '0000,0005') -or
                    ((@($PrimaryStep.last_step) -join ',') -ne
                        '0000,0005') -or
                    ((@($PrimaryStep.action) -join ',') -ne '0,0') -or
                    ((@($PrimaryStep.action_serial) -join ',') -ne
                        '0,0') -or
                    [bool]$State.opcode4_ball_target.executed -or
                    [bool]$State.live.last_shot_request -or
                    [bool]$State.live.last_shot_playback_supported -or
                    [bool]$State.live.last_shot_deferred -or
                    [int]$State.action_serial -ne 0 -or
                    [int]$State.shot_kind -ne 0 -or
                    [string]$AsmEvidence.formation_refresh -ne
                        "Bank06 C-0039 `$944D-`$9465" -or
                    [string]$AsmEvidence.command_stream -ne
                        "Bank04 `$9F2E five-byte records" -or
                    [string]$AsmEvidence.primary_dispatch -ne
                        "Bank06 `$8374-`$83F3 -> `$8491" -or
                    [string]$AsmEvidence.cpu_shot_gate -ne
                        "Bank06 C-0011 `$8431-`$8475") {
                    throw "LIVE proof selected-primary stream step regressed."
                }
            }
            if ($Event -like "cpu-auto-pass-*") {
                $AutoPass = $State.cpu_auto_pass_stream
                $ExpectedCheckpoint = switch ($Event) {
                    "cpu-auto-pass-opcode5" { 1 }
                    "cpu-auto-pass-action10" { 2 }
                    "cpu-auto-pass-gather" { 3 }
                    "cpu-auto-pass-stream" { 4 }
                    default { 0 }
                }
                if ($null -eq $AutoPass -or ![bool]$AutoPass.proved -or
                    [int]$AutoPass.checkpoint -ne $ExpectedCheckpoint -or
                    [bool]$AutoPass.upstream_play_selection_claimed -or
                    ![bool]$AutoPass.nondeferred -or
                    [int]$AutoPass.passer -lt 0 -or
                    [int]$AutoPass.passer -ge 10 -or
                    [int]$AutoPass.receiver -lt 0 -or
                    [int]$AutoPass.receiver -ge 10 -or
                    [int]$AutoPass.passer -eq [int]$AutoPass.receiver -or
                    ((@($AutoPass.records | ForEach-Object { $_[0] }) -join ',') -ne
                        '017C,018B,0190') -or
                    ((@($AutoPass.records | ForEach-Object { [int]$_[1] }) -join ',') -ne
                        '5,23,6') -or
                    [string]$AutoPass.stream[0] -ne '017C' -or
                    [string]$AutoPass.stream[1] -ne '0181' -or
                    [int]$AutoPass.actions.opcode5 -ne 24) {
                    throw "LIVE proof CPU automatic pass base checkpoint regressed."
                }
                if ($ExpectedCheckpoint -ge 2 -and
                    (((@($AutoPass.stream) -join ',') -ne
                        '017C,0181,0186,018B,0190,0190') -or
                     ((@($AutoPass.wait) -join ',') -ne '6,5,4,3,2,1,0') -or
                     [int]$AutoPass.actions.opcode23 -ne 25 -or
                     [int]$AutoPass.actions.opcode6 -ne 16 -or
                     ![bool]$AutoPass.opcode6_object10_state.written -or
                     [int]$AutoPass.opcode6_object10_state.value -ne 19)) {
                    throw "LIVE proof CPU automatic pass source cadence regressed."
                }
                if ($ExpectedCheckpoint -eq 3 -and
                    ([int]$AutoPass.pass.phase -ne 1 -or
                     [int]$AutoPass.pass.packed -ne 50 -or
                     [int]$AutoPass.actions.gather -ne 15)) {
                    throw "LIVE proof CPU automatic pass gather checkpoint regressed."
                }
                if ($ExpectedCheckpoint -eq 4) {
                    $BallGather = @($AutoPass.positions.ball_gather_q8)
                    $BallFlight = @($AutoPass.positions.ball_checkpoint_q8)
                    $PasserStart = @($AutoPass.positions.passer_start)
                    $PasserAfter = @($AutoPass.positions.passer_checkpoint)
                    $ReceiverStart = @($AutoPass.positions.receiver_start)
                    $ReceiverAfter = @($AutoPass.positions.receiver_checkpoint)
                    if ([int]$AutoPass.pass.phase -ne 2 -or
                        [int]$AutoPass.pass.flight_frame -le 0 -or
                        [int]$AutoPass.pass.flight_duration -le
                            [int]$AutoPass.pass.flight_frame -or
                        (($BallGather -join ',') -eq ($BallFlight -join ',')) -or
                        ((($PasserStart -join ',') -eq ($PasserAfter -join ',')) -and
                         (($ReceiverStart -join ',') -eq ($ReceiverAfter -join ',')))) {
                        throw "LIVE proof CPU automatic pass visible flight/movement regressed."
                    }
                }
            }
            if ($Event -eq "defensive-foul-presentation") {
                $Foul = $State.live_foul
                $Presentation = $Foul.presentation
                if (![bool]$Foul.active -or
                    [bool]$Foul.direct_phase_injection -or
                    $Foul.entrypoint -ne
                        "tecmo_gameplay_scene_update/human-defensive-B" -or
                    $Foul.contact_gate -ne "Bank05:`$9968-`$999D" -or
                    $Foul.commit -ne "Bank05:`$9571-`$9649:C83877F7" -or
                    $Foul.classifier -ne "Bank02:`$B0F8-`$B398:A06E397C" -or
                    ![bool]$Presentation.retained -or
                    [int]$Presentation.foul_class -ne 3 -or
                    [int]$Presentation.individual_delta -ne 1 -or
                    [int]$Presentation.team_delta -ne 1 -or
                    [int]$Presentation.individual_after -ne 1 -or
                    [int]$Presentation.team_after -ne 5 -or
                    [int]$Presentation.attempts -ne 2 -or
                    ![bool]$Presentation.team_in_bonus -or
                    [bool]$Presentation.fouled_out -or
                    [int]$Presentation.visible_phase_frame -ne 23 -or
                    ![bool]$Presentation.overlay_visible -or
                    [int]$Presentation.referee_group -ne 1 -or
                    ![bool]$Presentation.court_actors_suppressed -or
                    $Presentation.overlay_writer -ne "Bank02:`$B0F8-`$B398" -or
                    $Presentation.referee_script -ne
                        "fixed:`$E95E-`$EA11:`$2C-then-`$22" -or
                    $Presentation.timing -notmatch
                        "completion frame unavailable") {
                    throw "LIVE proof defensive-foul overlay/gesture state regressed."
                }
            }
            $ProofRecords += [pscustomobject]@{
                repeat = $Repeat
                event = $Event
                path = [IO.Path]::GetFullPath($Png)
                state_path = [IO.Path]::GetFullPath($Log)
                sha256 = (Get-FileHash -LiteralPath $Png -Algorithm SHA256).Hash
                frame_fingerprint_fnv1a32 = [string]$State.frame_fingerprint_fnv1a32
                state = $State
            }
        }
    }
    foreach ($Event in $ProofEvents) {
        $Pair = @($ProofRecords | Where-Object { $_.event -eq $Event })
        if ($Pair.Count -ne 2 -or $Pair[0].sha256 -ne $Pair[1].sha256 -or
            $Pair[0].frame_fingerprint_fnv1a32 -ne
                $Pair[1].frame_fingerprint_fnv1a32) {
            throw "LIVE proof event '$Event' was not deterministic across repeats."
        }
    }
    $ContactSheets = @()
    foreach ($Repeat in 1, 2) {
        $RepeatRecords = @($ProofRecords | Where-Object { $_.repeat -eq $Repeat })
        $ContactPath = Join-Path $ProofRoot ("contact-repeat-{0}.png" -f $Repeat)
        $Sheet = New-LiveProofContactSheet -Records $RepeatRecords -OutputPath $ContactPath
        $ContactSheets += [pscustomobject]@{
            repeat = $Repeat
            path = $Sheet.path
            sha256 = $Sheet.sha256
            width = $Sheet.width
            height = $Sheet.height
            frame_count = $Sheet.frame_count
        }
    }
    if ($ContactSheets[0].sha256 -ne $ContactSheets[1].sha256) {
        throw "LIVE proof contact sheets were not deterministic across repeats."
    }
    $NativeVideos = @()
    foreach ($Repeat in 1, 2) {
        $RepeatRoot = Join-Path $ProofRoot ("repeat-{0}" -f $Repeat)
        for ($Index = 0; $Index -lt $ProofEvents.Count; ++$Index) {
            $Event = $ProofEvents[$Index]
            $Record = @($ProofRecords | Where-Object {
                $_.repeat -eq $Repeat -and $_.event -eq $Event
            })[0]
            $NumberedPath = Join-Path $RepeatRoot ("frame-{0:D4}.png" -f ($Index + 1))
            Copy-Item -LiteralPath $Record.path -Destination $NumberedPath
        }
        $VideoPath = Join-Path $ProofRoot ("native-repeat-{0}.mp4" -f $Repeat)
        $NativeVideos += New-LiveProofVideo -FramesRoot $RepeatRoot `
            -OutputPath $VideoPath -Label ("native-repeat-{0}" -f $Repeat) `
            -FrameCount $ProofEvents.Count
    }
    if ($NativeVideos[0].sha256 -ne $NativeVideos[1].sha256 -or
        $NativeVideos[0].decoded_frame_sha256 -ne
            $NativeVideos[1].decoded_frame_sha256 -or
        (@($NativeVideos[0].decoded_frame_hashes) -join ',') -ne
            (@($NativeVideos[1].decoded_frame_hashes) -join ',')) {
        throw "Native LIVE proof MP4 repeats were not deterministic after decode."
    }
    $RomSha = (Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash
    $PackSha = (Get-FileHash -LiteralPath $PackPath -Algorithm SHA256).Hash
    $TgaiPayload = Get-EntryBytes $PackBytes $Entries["gameplay/cpu-steering"]
    $TgmoPayload = Get-EntryBytes $PackBytes $Entries["gameplay/movement"]
    $OriginalReferenceProof = Get-OriginalReferenceProof `
        -ManifestPath $OriginalReferenceManifestPath
    if ($RequirePass -and $OriginalReferenceProof.status -ne "validated") {
        throw "LIVE proof -RequirePass requires the accepted original CPU proof manifest and artifacts."
    }
    $GitProofState = Get-LiveProofGitState
    if ($RequirePass -and !(Test-LiveProofRequirePassState $GitProofState)) {
        throw ("LIVE proof -RequirePass rejected the worktree before PASS: " +
            "branch=$($GitProofState.branch) clean=$($GitProofState.clean) " +
            "base_is_ancestor=$($GitProofState.base_is_ancestor)")
    }
    $ManifestPath = Join-Path $ProofRoot "PROOF-MANIFEST.json"
    $Manifest = [ordered]@{
        schema = "tecmo.live-proof-manifest/TGLP-1"
        status = "DRAFT"
        task = "R1-LIVE-FOUNDATION"
        proof_root = [IO.Path]::GetFullPath($ProofRoot)
        base_sha = $ExpectedBaseSha
        current_sha = $GitProofState.head
        final_sha = "PENDING_CLEAN_COMMIT"
        branch = $GitProofState.branch
        clean = [bool]$GitProofState.clean
        native_resolution = @(640, 480)
        native_cadence = $NativeFrameRate
        native_time_base = $NativeVideoTimeBase
        native_video_track_timescale = $NativeVideoTrackTimescale
        rom = [ordered]@{
            revision = "Rev1"
            length = 393232
            sha256 = $RomSha
        }
        asset_pack = [ordered]@{
            path = [IO.Path]::GetFullPath($ProofPackPath)
            replay_path = [IO.Path]::GetFullPath($ProofPackPath)
            source_path_ephemeral = [IO.Path]::GetFullPath($PackPath)
            sha256 = $PackSha
            tgai = [ordered]@{
                bytes = [int]$Entries["gameplay/cpu-steering"].byte_count
                fnv1a32 = Get-Fnv1a32 $TgaiPayload
            }
            tgmo = [ordered]@{
                bytes = [int]$Entries["gameplay/movement"].byte_count
                fnv1a32 = Get-Fnv1a32 $TgmoPayload
            }
        }
        rom_revision = "Rev1"
        rom_length = 393232
        rom_sha256 = $RomSha
        pack_sha256 = $PackSha
        tgai_bytes = [int]$Entries["gameplay/cpu-steering"].byte_count
        tgai_fnv1a32 = Get-Fnv1a32 $TgaiPayload
        tgmo_bytes = [int]$Entries["gameplay/movement"].byte_count
        tgmo_fnv1a32 = Get-Fnv1a32 $TgmoPayload
        event_order = $ProofEvents
        input_schedule = @(
            "pretip-start: real preTIP initial presentation, no updates"
            "live-handoff: neutral PRETIP updates to LIVE"
            "human-movement: P1 held RIGHT for two updates"
            "offensive-pass: P1 NES A"
            "defensive-switch: P1 NES A with home possession"
            "cpu-target-deferred: deterministic source-offset fixture"
            "actor-command-assignment-deferred: real PRETIP/live handoff, then no source-shaped A023 caller or mutation"
            "cpu-primary-stream-step: automatic selected `$0308` primary consumes one Bank04 opcode-4 record before ordinary-loop exclusion"
            "cpu-auto-pass-opcode5: selected automatic holder parked at canonical `$017C; upstream play selection explicitly unclaimed"
            "cpu-auto-pass-action10: exact opcode9/wait6/opcode23/opcode6 cadence reaches retained `$0190 action `$10/object-slot-10 `$13"
            "cpu-auto-pass-gather: following native scene update enters packed `$32 gather with passer action `$0F"
            "cpu-auto-pass-stream: gather releases into visible pass flight with deterministic ball/player position deltas"
            "shot-path: deterministic supported close-shot fixture"
            "claimant-settlement: native pre-tip handoff then deterministic coordinate/frame fixture, normal controller-B miss and production terminal claimant handoff (no direct claimant/phase/possession injection)"
            "defensive-foul-presentation: real PRETIP/live handoff, optional human A switch, human defensive-B, then neutral capture at TGVR visible group 1"
        )
        repeat_count = 2
        stored_frame_count = $ProofRecords.Count
        decoded_frame_count = ($NativeVideos | Measure-Object -Property decoded_frame_count -Sum).Sum
        frame_records = @($ProofRecords | ForEach-Object {
            [ordered]@{
                repeat = $_.repeat
                event = $_.event
                path = $_.path
                state_path = $_.state_path
                sha256 = $_.sha256
                frame_fingerprint_fnv1a32 = $_.frame_fingerprint_fnv1a32
                state = $_.state
            }
        })
        contact_sheets = $ContactSheets
        native_videos = $NativeVideos
        original_reference = $OriginalReferenceProof
        timestamps_utc = [ordered]@{
            proof_started = $ProofStartUtc
            manifest_draft = [DateTime]::UtcNow.ToString("o")
            proof_finished = $null
        }
        fixture_classification = @{
            force_possession = "deterministic test fixture; not original or normal-policy evidence"
            source_offset_injection = "deterministic test fixture; not original or normal-policy evidence"
            close_position_injection = "deterministic test fixture; not original or normal-policy evidence"
            actor_command_assignment = "ordinary pretip-to-LIVE observation; emits a missing-input diagnostic and no A023 fixture or production mutation"
            lineup_binding = "bound production-style scene launch; game.c bridge separately proven by flow tests"
        }
        proof_pack_replay_path = [IO.Path]::GetFullPath($ProofPackPath)
        ephemeral_pack_path = [IO.Path]::GetFullPath($PackPath)
        build_requested = [bool]$Build
        build_warning_clean = [bool]$Build
        required_logs = @()
        negative_regressions = @(
            "dirty RequirePass rejects before PASS"
            "wrong-branch RequirePass rejects before PASS"
            "non-ancestral-base RequirePass rejects before PASS"
            "stale ROM SHA rejects"
            "stale asset-pack SHA rejects"
            "stale contact-sheet IHDR/metadata rejects (1920x960 is invalid for seven frames)"
            "stale video rate rejects"
            "stale video frame-count rejects"
            "stale video time-base rejects"
            "missing/malformed/cross-pack proof inputs reject"
        )
        commands = @()
        tool_versions = @{}
        suites_complete = $false
        require_pass = [bool]$RequirePass
        artifact_inventory = @()
        artifact_inventory_count = 0
    }
    $Manifest | ConvertTo-Json -Depth 14 | Set-Content -LiteralPath $ManifestPath -Encoding UTF8
    if (!(Test-LiveProofManifest -ManifestPath $ManifestPath `
            -ExpectedRomSha256 $RomSha -ExpectedPackSha256 $PackSha `
            -ExpectedEvents $ProofEvents)) {
        throw "LIVE proof manifest self-validation failed."
    }
    $StaleManifestPath = Join-Path $ProofRoot "PROOF-MANIFEST-stale.json"
    $StaleManifest = Get-Content -LiteralPath $ManifestPath -Raw |
        ConvertFrom-Json
    $StaleManifest.rom_sha256 = ('0' * 64)
    $StaleManifest | ConvertTo-Json -Depth 14 |
        Set-Content -LiteralPath $StaleManifestPath -Encoding UTF8
    if (Test-LiveProofManifest -ManifestPath $StaleManifestPath `
            -ExpectedRomSha256 $RomSha -ExpectedPackSha256 $PackSha `
            -ExpectedEvents $ProofEvents) {
        throw "LIVE proof stale-metadata negative was accepted."
    }
    $StalePackManifestPath = Join-Path $ProofRoot "PROOF-MANIFEST-stale-pack.json"
    $StalePackManifest = Get-Content -LiteralPath $ManifestPath -Raw |
        ConvertFrom-Json
    $StalePackManifest.pack_sha256 = ('0' * 64)
    $StalePackManifest | ConvertTo-Json -Depth 14 |
        Set-Content -LiteralPath $StalePackManifestPath -Encoding UTF8
    if (Test-LiveProofManifest -ManifestPath $StalePackManifestPath `
            -ExpectedRomSha256 $RomSha -ExpectedPackSha256 $PackSha `
            -ExpectedEvents $ProofEvents) {
        throw "LIVE proof stale-pack metadata negative was accepted."
    }
    $StaleContactManifestPath = Join-Path $ProofRoot "PROOF-MANIFEST-stale-contact.json"
    $StaleContactManifest = Get-Content -LiteralPath $ManifestPath -Raw |
        ConvertFrom-Json
    $StaleContactManifest.contact_sheets[0].height = 960
    $StaleContactManifest | ConvertTo-Json -Depth 14 |
        Set-Content -LiteralPath $StaleContactManifestPath -Encoding UTF8
    if (Test-LiveProofManifest -ManifestPath $StaleContactManifestPath `
            -ExpectedRomSha256 $RomSha -ExpectedPackSha256 $PackSha `
            -ExpectedEvents $ProofEvents) {
        throw "LIVE proof stale-contact metadata negative was accepted."
    }
    $StaleVideoManifestPath = Join-Path $ProofRoot "PROOF-MANIFEST-stale-video.json"
    $StaleVideoManifest = Get-Content -LiteralPath $ManifestPath -Raw |
        ConvertFrom-Json
    $StaleVideoManifest.native_videos[0].probe.r_frame_rate = "1/60"
    $StaleVideoManifest | ConvertTo-Json -Depth 14 |
        Set-Content -LiteralPath $StaleVideoManifestPath -Encoding UTF8
    if (Test-LiveProofManifest -ManifestPath $StaleVideoManifestPath `
            -ExpectedRomSha256 $RomSha -ExpectedPackSha256 $PackSha `
            -ExpectedEvents $ProofEvents) {
        throw "LIVE proof stale-video cadence negative was accepted."
    }
    $StaleCountManifestPath = Join-Path $ProofRoot "PROOF-MANIFEST-stale-count.json"
    $StaleCountManifest = Get-Content -LiteralPath $ManifestPath -Raw |
        ConvertFrom-Json
    $StaleCountManifest.native_videos[0].probe.nb_frames = 6
    $StaleCountManifest | ConvertTo-Json -Depth 14 |
        Set-Content -LiteralPath $StaleCountManifestPath -Encoding UTF8
    if (Test-LiveProofManifest -ManifestPath $StaleCountManifestPath `
            -ExpectedRomSha256 $RomSha -ExpectedPackSha256 $PackSha `
            -ExpectedEvents $ProofEvents) {
        throw "LIVE proof stale-count metadata negative was accepted."
    }
    $StaleCadenceManifestPath = Join-Path $ProofRoot "PROOF-MANIFEST-stale-timebase.json"
    $StaleCadenceManifest = Get-Content -LiteralPath $ManifestPath -Raw |
        ConvertFrom-Json
    $StaleCadenceManifest.native_videos[0].probe.time_base = "1/90000"
    $StaleCadenceManifest | ConvertTo-Json -Depth 14 |
        Set-Content -LiteralPath $StaleCadenceManifestPath -Encoding UTF8
    if (Test-LiveProofManifest -ManifestPath $StaleCadenceManifestPath `
            -ExpectedRomSha256 $RomSha -ExpectedPackSha256 $PackSha `
            -ExpectedEvents $ProofEvents) {
        throw "LIVE proof stale-timebase metadata negative was accepted."
    }
    $MissingProofPack = Join-Path $ProofRoot "missing-proof.assetpack"
    Assert-LiveProofRejected -Label "missing-pack" -Arguments @(
        "--root", $ProjectRoot,
        "--gameplay-live-foundation-proof", $MissingProofPack,
        "live-handoff", (Join-Path $ProofRoot "missing.png")
    )
    Assert-LiveProofRejected -Label "malformed-event" -Arguments @(
        "--root", $ProjectRoot,
        "--gameplay-live-foundation-proof", $PackPath,
        "not-an-event", (Join-Path $ProofRoot "malformed.png")
    )
    $CrossPackProofPath = Join-Path $ProofRoot "cross-pack-proof.assetpack"
    $CrossPackProof = [byte[]]$PackBytes.Clone()
    $ProofResolutionEntry = Get-AssetPackEntry $PackBytes "gameplay/shot-resolution"
    $CrossPackProof[[int]$ProofResolutionEntry.pack_offset + 24] =
        $CrossPackProof[[int]$ProofResolutionEntry.pack_offset + 24] -bxor 1
    [IO.File]::WriteAllBytes($CrossPackProofPath, $CrossPackProof)
    Assert-LiveProofRejected -Label "cross-pack" -Arguments @(
        "--root", $ProjectRoot,
        "--gameplay-live-foundation-proof", $CrossPackProofPath,
        "live-handoff", (Join-Path $ProofRoot "cross-pack.png")
    )

    $MissingHudPath = Join-Path $Scratch "missing-gameplay-hud.assetpack"
    $MissingHud = [byte[]]$PackBytes.Clone()
    $MissingHud[[int]$Entries["gameplay/hud"].directory_offset] =
        [byte][char]'x'
    [IO.File]::WriteAllBytes($MissingHudPath, $MissingHud)
    Assert-SceneRejected -AssetPack $MissingHudPath `
        -Label "missing-gameplay-hud" -ExpectedStatus "THUD-1"

    $HudOffset = [int]$Entries["gameplay/hud"].pack_offset
    $MalformedHudPath = Join-Path $Scratch "malformed-gameplay-hud.assetpack"
    $MalformedHud = [byte[]]$PackBytes.Clone()
    $MalformedHud[$HudOffset] = $MalformedHud[$HudOffset] -bxor 1
    [IO.File]::WriteAllBytes($MalformedHudPath, $MalformedHud)
    Assert-SceneRejected -AssetPack $MalformedHudPath `
        -Label "malformed-gameplay-hud" -ExpectedStatus "THUD-1"

    $OversizedHudPath = Join-Path $Scratch "oversized-gameplay-hud.assetpack"
    $OversizedHud = [byte[]]$PackBytes.Clone()
    [BitConverter]::GetBytes([uint64]865).CopyTo(
        $OversizedHud,
        [int]$Entries["gameplay/hud"].directory_offset + 92)
    [IO.File]::WriteAllBytes($OversizedHudPath, $OversizedHud)
    Assert-SceneRejected -AssetPack $OversizedHudPath `
        -Label "oversized-gameplay-hud" -ExpectedStatus "THUD-1"

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

    $MissingMovementPath = Join-Path $Scratch "missing-movement.assetpack"
    $MissingMovement = [byte[]]$PackBytes.Clone()
    $MissingMovement[
        [int]$Entries["gameplay/movement"].directory_offset] =
        [byte][char]'x'
    [IO.File]::WriteAllBytes($MissingMovementPath, $MissingMovement)
    Assert-SceneRejected -AssetPack $MissingMovementPath `
        -Label "missing-movement" -ExpectedStatus "TGMO-1"

    $MovementOffset = [int]$Entries["gameplay/movement"].pack_offset
    $MalformedMovementPath = Join-Path $Scratch "malformed-movement.assetpack"
    $MalformedMovement = [byte[]]$PackBytes.Clone()
    $MalformedMovement[$MovementOffset] =
        $MalformedMovement[$MovementOffset] -bxor 1
    [IO.File]::WriteAllBytes($MalformedMovementPath, $MalformedMovement)
    Assert-SceneRejected -AssetPack $MalformedMovementPath `
        -Label "malformed-movement" -ExpectedStatus "TGMO-1"

    $OversizedMovementPath = Join-Path $Scratch "oversized-movement.assetpack"
    $OversizedMovement = [byte[]]$PackBytes.Clone()
    [BitConverter]::GetBytes([uint64]1665).CopyTo(
        $OversizedMovement,
        [int]$Entries["gameplay/movement"].directory_offset + 92)
    [IO.File]::WriteAllBytes($OversizedMovementPath, $OversizedMovement)
    Assert-SceneRejected -AssetPack $OversizedMovementPath `
        -Label "oversized-movement" -ExpectedStatus "TGMO-1"

    $MissingBallDribblePath =
        Join-Path $Scratch "missing-ball-dribble.assetpack"
    $MissingBallDribble = [byte[]]$PackBytes.Clone()
    $MissingBallDribble[
        [int]$Entries["gameplay/ball-dribble"].directory_offset] =
        [byte][char]'x'
    [IO.File]::WriteAllBytes($MissingBallDribblePath, $MissingBallDribble)
    Assert-SceneRejected -AssetPack $MissingBallDribblePath `
        -Label "missing-ball-dribble" -ExpectedStatus "TGBD-1"

    $BallDribbleOffset =
        [int]$Entries["gameplay/ball-dribble"].pack_offset
    $MalformedBallDribblePath =
        Join-Path $Scratch "malformed-ball-dribble.assetpack"
    $MalformedBallDribble = [byte[]]$PackBytes.Clone()
    $MalformedBallDribble[$BallDribbleOffset] =
        $MalformedBallDribble[$BallDribbleOffset] -bxor 1
    [IO.File]::WriteAllBytes(
        $MalformedBallDribblePath, $MalformedBallDribble)
    Assert-SceneRejected -AssetPack $MalformedBallDribblePath `
        -Label "malformed-ball-dribble" -ExpectedStatus "TGBD-1"

    $OversizedBallDribblePath =
        Join-Path $Scratch "oversized-ball-dribble.assetpack"
    $OversizedBallDribble = [byte[]]$PackBytes.Clone()
    [BitConverter]::GetBytes([uint64]609).CopyTo(
        $OversizedBallDribble,
        [int]$Entries["gameplay/ball-dribble"].directory_offset + 92)
    [IO.File]::WriteAllBytes(
        $OversizedBallDribblePath, $OversizedBallDribble)
    Assert-SceneRejected -AssetPack $OversizedBallDribblePath `
        -Label "oversized-ball-dribble" -ExpectedStatus "TGBD-1"

    $MissingFatiguePath = Join-Path $Scratch "missing-fatigue.assetpack"
    $MissingFatigue = [byte[]]$PackBytes.Clone()
    $MissingFatigue[
        [int]$Entries["gameplay/fatigue"].directory_offset] =
        [byte][char]'x'
    [IO.File]::WriteAllBytes($MissingFatiguePath, $MissingFatigue)
    Assert-SceneRejected -AssetPack $MissingFatiguePath `
        -Label "missing-fatigue" -ExpectedStatus "TGFT-1"

    $FatigueOffset = [int]$Entries["gameplay/fatigue"].pack_offset
    $MalformedFatiguePath = Join-Path $Scratch "malformed-fatigue.assetpack"
    $MalformedFatigue = [byte[]]$PackBytes.Clone()
    $MalformedFatigue[$FatigueOffset] = $MalformedFatigue[$FatigueOffset] -bxor 1
    [IO.File]::WriteAllBytes($MalformedFatiguePath, $MalformedFatigue)
    Assert-SceneRejected -AssetPack $MalformedFatiguePath `
        -Label "malformed-fatigue" -ExpectedStatus "TGFT-1"

    $OversizedFatiguePath = Join-Path $Scratch "oversized-fatigue.assetpack"
    $OversizedFatigue = [byte[]]$PackBytes.Clone()
    [BitConverter]::GetBytes([uint64]513).CopyTo(
        $OversizedFatigue,
        [int]$Entries["gameplay/fatigue"].directory_offset + 92)
    [IO.File]::WriteAllBytes($OversizedFatiguePath, $OversizedFatigue)
    Assert-SceneRejected -AssetPack $OversizedFatiguePath `
        -Label "oversized-fatigue" -ExpectedStatus "TGFT-1"

    $MissingPenaltyPath = Join-Path $Scratch "missing-penalties.assetpack"
    $MissingPenalty = [byte[]]$PackBytes.Clone()
    $MissingPenalty[
        [int]$Entries["gameplay/penalties"].directory_offset] =
        [byte][char]'x'
    [IO.File]::WriteAllBytes($MissingPenaltyPath, $MissingPenalty)
    Assert-SceneRejected -AssetPack $MissingPenaltyPath `
        -Label "missing-penalties" -ExpectedStatus "TPNL-1"

    $PenaltyOffset = [int]$Entries["gameplay/penalties"].pack_offset
    $MalformedPenaltyPath = Join-Path $Scratch "malformed-penalties.assetpack"
    $MalformedPenalty = [byte[]]$PackBytes.Clone()
    $MalformedPenalty[$PenaltyOffset] = $MalformedPenalty[$PenaltyOffset] -bxor 1
    [IO.File]::WriteAllBytes($MalformedPenaltyPath, $MalformedPenalty)
    Assert-SceneRejected -AssetPack $MalformedPenaltyPath `
        -Label "malformed-penalties" -ExpectedStatus "TPNL-1"

    $OversizedPenaltyPath = Join-Path $Scratch "oversized-penalties.assetpack"
    $OversizedPenalty = [byte[]]$PackBytes.Clone()
    [BitConverter]::GetBytes([uint64]769).CopyTo(
        $OversizedPenalty,
        [int]$Entries["gameplay/penalties"].directory_offset + 92)
    [IO.File]::WriteAllBytes($OversizedPenaltyPath, $OversizedPenalty)
    Assert-SceneRejected -AssetPack $OversizedPenaltyPath `
        -Label "oversized-penalties" -ExpectedStatus "TPNL-1"

    $MissingViolationRefereePath =
        Join-Path $Scratch "missing-violation-referee.assetpack"
    $MissingViolationReferee = [byte[]]$PackBytes.Clone()
    $MissingViolationReferee[
        [int]$Entries["gameplay/violation-referee"].directory_offset] =
        [byte][char]'x'
    [IO.File]::WriteAllBytes(
        $MissingViolationRefereePath, $MissingViolationReferee)
    Assert-SceneRejected -AssetPack $MissingViolationRefereePath `
        -Label "missing-violation-referee" -ExpectedStatus "TGVR-1"

    $ViolationRefereeOffset =
        [int]$Entries["gameplay/violation-referee"].pack_offset
    $MalformedViolationRefereePath =
        Join-Path $Scratch "malformed-violation-referee.assetpack"
    $MalformedViolationReferee = [byte[]]$PackBytes.Clone()
    $MalformedViolationReferee[$ViolationRefereeOffset] =
        $MalformedViolationReferee[$ViolationRefereeOffset] -bxor 1
    [IO.File]::WriteAllBytes(
        $MalformedViolationRefereePath, $MalformedViolationReferee)
    Assert-SceneRejected -AssetPack $MalformedViolationRefereePath `
        -Label "malformed-violation-referee" -ExpectedStatus "TGVR-1"

    $OversizedViolationRefereePath =
        Join-Path $Scratch "oversized-violation-referee.assetpack"
    $OversizedViolationReferee = [byte[]]$PackBytes.Clone()
    [BitConverter]::GetBytes([uint64]4753).CopyTo(
        $OversizedViolationReferee,
        [int]$Entries["gameplay/violation-referee"].directory_offset + 92)
    [IO.File]::WriteAllBytes(
        $OversizedViolationRefereePath, $OversizedViolationReferee)
    Assert-SceneRejected -AssetPack $OversizedViolationRefereePath `
        -Label "oversized-violation-referee" -ExpectedStatus "TGVR-1"

    $MissingSteeringPath = Join-Path $Scratch "missing-cpu-steering.assetpack"
    $MissingSteering = [byte[]]$PackBytes.Clone()
    $MissingSteering[
        [int]$Entries["gameplay/cpu-steering"].directory_offset] =
        [byte][char]'x'
    [IO.File]::WriteAllBytes($MissingSteeringPath, $MissingSteering)
    Assert-SceneRejected -AssetPack $MissingSteeringPath `
        -Label "missing-cpu-steering" -ExpectedStatus "TGAI-3"

    $SteeringOffset =
        [int]$Entries["gameplay/cpu-steering"].pack_offset
    $MalformedSteeringPath =
        Join-Path $Scratch "malformed-cpu-steering.assetpack"
    $MalformedSteering = [byte[]]$PackBytes.Clone()
    $MalformedSteering[$SteeringOffset] =
        $MalformedSteering[$SteeringOffset] -bxor 1
    [IO.File]::WriteAllBytes($MalformedSteeringPath, $MalformedSteering)
    Assert-SceneRejected -AssetPack $MalformedSteeringPath `
        -Label "malformed-cpu-steering" -ExpectedStatus "TGAI-3"

    $OversizedSteeringPath =
        Join-Path $Scratch "oversized-cpu-steering.assetpack"
    $OversizedSteering = [byte[]]$PackBytes.Clone()
    [BitConverter]::GetBytes([uint64]7633).CopyTo(
        $OversizedSteering,
        [int]$Entries["gameplay/cpu-steering"].directory_offset + 92)
    [IO.File]::WriteAllBytes($OversizedSteeringPath, $OversizedSteering)
    Assert-SceneRejected -AssetPack $OversizedSteeringPath `
        -Label "oversized-cpu-steering" -ExpectedStatus "TGAI-3"

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

    $MissingBackcourtPath = Join-Path $Scratch "missing-backcourt.assetpack"
    $MissingBackcourt = [byte[]]$PackBytes.Clone()
    $MissingBackcourt[
        [int]$Entries["gameplay/backcourt"].directory_offset] =
        [byte][char]'x'
    [IO.File]::WriteAllBytes($MissingBackcourtPath, $MissingBackcourt)
    Assert-SceneRejected -AssetPack $MissingBackcourtPath `
        -Label "missing-backcourt" -ExpectedStatus "TGBC-1"

    $MalformedBackcourtPath =
        Join-Path $Scratch "malformed-backcourt.assetpack"
    $MalformedBackcourt = [byte[]]$PackBytes.Clone()
    $BackcourtOffset = [int]$Entries["gameplay/backcourt"].pack_offset
    $MalformedBackcourt[$BackcourtOffset] =
        $MalformedBackcourt[$BackcourtOffset] -bxor 1
    [IO.File]::WriteAllBytes(
        $MalformedBackcourtPath, $MalformedBackcourt)
    Assert-SceneRejected -AssetPack $MalformedBackcourtPath `
        -Label "malformed-backcourt" -ExpectedStatus "TGBC-1"

    $OversizedBackcourtPath =
        Join-Path $Scratch "oversized-backcourt.assetpack"
    $OversizedBackcourt = [byte[]]$PackBytes.Clone()
    [BitConverter]::GetBytes([uint64]513).CopyTo(
        $OversizedBackcourt,
        [int]$Entries["gameplay/backcourt"].directory_offset + 92)
    [IO.File]::WriteAllBytes(
        $OversizedBackcourtPath, $OversizedBackcourt)
    Assert-SceneRejected -AssetPack $OversizedBackcourtPath `
        -Label "oversized-backcourt" -ExpectedStatus "TGBC-1"

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
        -Label "missing-shot-resolution" -ExpectedStatus "TGSR-4"

    $MalformedResolutionPath = Join-Path $Scratch "malformed-shot-resolution.assetpack"
    $MalformedResolution = [byte[]]$PackBytes.Clone()
    $ResolutionOffset = [int]$Entries["gameplay/shot-resolution"].pack_offset
    $MalformedResolution[$ResolutionOffset] =
        $MalformedResolution[$ResolutionOffset] -bxor 1
    [IO.File]::WriteAllBytes($MalformedResolutionPath, $MalformedResolution)
    Assert-SceneRejected -AssetPack $MalformedResolutionPath `
        -Label "malformed-shot-resolution" -ExpectedStatus "TGSR-4"

    $OversizedResolutionPath = Join-Path $Scratch "oversized-shot-resolution.assetpack"
    $OversizedResolution = [byte[]]$PackBytes.Clone()
    [BitConverter]::GetBytes([uint64]609).CopyTo(
        $OversizedResolution,
        [int]$Entries["gameplay/shot-resolution"].directory_offset + 92)
    [IO.File]::WriteAllBytes($OversizedResolutionPath, $OversizedResolution)
    Assert-SceneRejected -AssetPack $OversizedResolutionPath `
        -Label "oversized-shot-resolution" -ExpectedStatus "TGSR-4"

    $WrongResolutionPath = Join-Path $Scratch "wrong-revision-shot-resolution.assetpack"
    $WrongResolution = [byte[]]$PackBytes.Clone()
    $WrongResolution[$ResolutionOffset + 80] =
        $WrongResolution[$ResolutionOffset + 80] -bxor 1
    [IO.File]::WriteAllBytes($WrongResolutionPath, $WrongResolution)
    Assert-SceneRejected -AssetPack $WrongResolutionPath `
        -Label "wrong-revision-shot-resolution" -ExpectedStatus "TGSR-4"

    $CrossPackResolutionPath = Join-Path $Scratch "cross-pack-shot-resolution.assetpack"
    $CrossPackResolution = [byte[]]$PackBytes.Clone()
    $CoreOffset = [int]$Entries["gameplay/core"].pack_offset
    $CrossPackResolution[$CoreOffset + 128] =
        $CrossPackResolution[$CoreOffset + 128] -bxor 1
    [IO.File]::WriteAllBytes($CrossPackResolutionPath, $CrossPackResolution)
    Assert-SceneRejected -AssetPack $CrossPackResolutionPath `
        -Label "cross-pack-shot-resolution" -ExpectedStatus "TGSR-4"

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
        [pscustomobject]@{ mode="gameplay-live-f3-overlay"; state='gameplay-state frame=606 shot=none phase=live' },
        [pscustomobject]@{ mode="gameplay-live-f3-auto-overlay"; state='gameplay-state frame=606 shot=none phase=live' },
        [pscustomobject]@{ mode="gameplay-facing-away-left"; state='gameplay-state frame=606 shot=none phase=live' },
        [pscustomobject]@{ mode="gameplay-ball-bounce-frame1"; state='gameplay-state frame=607 shot=none phase=live' },
        [pscustomobject]@{ mode="gameplay-ball-bounce-frame12"; state='gameplay-state frame=618 shot=none phase=live' },
        [pscustomobject]@{ mode="gameplay-cpu-steering-frame25"; state='gameplay-state frame=631 shot=none phase=live' },
        [pscustomobject]@{ mode="gameplay-shot-clock-violation-frame0"; state='gameplay-state frame=607 shot=none phase=violation-presentation.*phase-frame=0 violation=SHOT CLOCK' },
        [pscustomobject]@{ mode="gameplay-shot-clock-violation-frame9"; state='gameplay-state frame=616 shot=none phase=violation-presentation.*phase-frame=9 violation=SHOT CLOCK' },
        [pscustomobject]@{ mode="gameplay-shot-clock-violation-frame23"; state='gameplay-state frame=630 shot=none phase=violation-presentation.*phase-frame=23 violation=SHOT CLOCK' },
        [pscustomobject]@{ mode="gameplay-shot-clock-violation-frame27"; state='gameplay-state frame=634 shot=none phase=violation-presentation.*phase-frame=27 violation=SHOT CLOCK' },
        [pscustomobject]@{ mode="gameplay-shot-clock-violation-frame80"; state='gameplay-state frame=687 shot=none phase=violation-presentation.*phase-frame=80 violation=SHOT CLOCK' },
        [pscustomobject]@{ mode="gameplay-possession-left"; state='gameplay-state frame=606 shot=none phase=live' },
        [pscustomobject]@{ mode="gameplay-possession-center"; state='gameplay-state frame=606 shot=none phase=live' },
        [pscustomobject]@{ mode="gameplay-possession-right"; state='gameplay-state frame=606 shot=none phase=live' },
        [pscustomobject]@{ mode="gameplay-uniform-pacers"; state='gameplay-state frame=606 shot=none phase=live' },
        [pscustomobject]@{ mode="gameplay-free-throw-left"; state='gameplay-state frame=611 shot=none phase=free-throw-sequence' },
        [pscustomobject]@{ mode="gameplay-free-throw-right"; state='gameplay-state frame=611 shot=none phase=free-throw-sequence' },
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
    $ExpectedCpuOwnershipOverlayHashes = @{
        "gameplay-live-f3-overlay" =
            "E656D6226C7E87E6AE572B2CB72F4C897DDC2C47B546F05E4413C340B15E3B12"
        "gameplay-live-f3-auto-overlay" =
            "DFDA22260A31E094121CB4E240ADBFA0B24F5466FFC24E5B6FDCD7015D3BEC5E"
    }
    foreach ($Mode in $ExpectedCpuOwnershipOverlayHashes.Keys) {
        if ($RenderHashes[$Mode] -ne
            $ExpectedCpuOwnershipOverlayHashes[$Mode]) {
            throw ("Gameplay CPU ownership overlay render hash changed at '{0}': expected {1}, actual {2}." -f
                $Mode, $ExpectedCpuOwnershipOverlayHashes[$Mode],
                $RenderHashes[$Mode])
        }
    }
    $ExpectedShotClockViolationHashes = @{
        "gameplay-shot-clock-violation-frame0" =
            "2377B0FF24274E21F5963CC35E43D0F666B7626E890A23C01A7621B842055F9A"
        "gameplay-shot-clock-violation-frame9" =
            "E86D772078629184BCAC4CA453171604F99E79B97EDFA3A536494FC9A8A27CF9"
        "gameplay-shot-clock-violation-frame23" =
            "295D375638728700408EB6322503C2642A4A75A0533A5AEC9D07FBE0B58BCA61"
        "gameplay-shot-clock-violation-frame27" =
            "6DF34112CB82609A46BFAFEEAEA426C982DD5A4C87463B5DE5E49100D541F4A3"
        "gameplay-shot-clock-violation-frame80" =
            "6DF34112CB82609A46BFAFEEAEA426C982DD5A4C87463B5DE5E49100D541F4A3"
    }
    foreach ($Mode in $ExpectedShotClockViolationHashes.Keys) {
        if ($RenderHashes[$Mode] -ne
            $ExpectedShotClockViolationHashes[$Mode]) {
            throw "Gameplay shot-clock referee render hash changed at '$Mode'."
        }
    }
    if ($RenderHashes["gameplay-facing-away-left"] -ne
            "616BD0E26393BCDE59FFAD7718413084B66D1F88CFAC355D65630EDF811C74A5") {
        throw "Gameplay Away-left facing checkpoint render hash changed."
    }
    if ($RenderHashes["gameplay-shot-clock-violation-frame23"] -eq
        $RenderHashes["gameplay-shot-clock-violation-frame27"]) {
        throw "Gameplay shot-clock referee gesture frames collapsed together."
    }
    $ExpectedBallBounceHashes = @{
        "gameplay-ball-bounce-frame1" =
            "7D68B8DDF6B8DC5E680BDEABD8EB93588D319599C01AE1E0EEE0F7D86C9A6939"
        "gameplay-ball-bounce-frame12" =
            "B13E6BD0EFB55055CD4D1240FC9E0ED8EE2EAE83373939332B9073187F88DA7A"
    }
    foreach ($Mode in $ExpectedBallBounceHashes.Keys) {
        if ($RenderHashes[$Mode] -ne $ExpectedBallBounceHashes[$Mode]) {
            throw "Gameplay held-ball bounce render hash changed at '$Mode'."
        }
    }
    if ($RenderHashes["gameplay-ball-bounce-frame1"] -eq
        $RenderHashes["gameplay-ball-bounce-frame12"]) {
        throw "Gameplay held-ball high/low bounce visuals collapsed together."
    }
    if ($RenderHashes["gameplay-cpu-steering-frame25"] -ne
            "8480A5FEBEC0E8B151F74F8015CA887A434FB56F569C2838519192F4057C08F1") {
        throw "Gameplay live TGAI/TGMO movement render hash changed."
    }
    $ExpectedPossessionSliceHashes = @{
        "gameplay-possession-left" =
            "6318B161D9F184444FC2A35193D832AAA0B8F1DE3AC70B0F1AF6AD70B4AF0F0F"
        "gameplay-possession-center" =
            "2A7A67F6006379D79974A8F8DA1DC1943D686E526B8D94700E10AFB5F2E2292D"
        "gameplay-possession-right" =
            "D0E92F8ED440E8F26AB11B12253F649FFC6A2FF9A98E7D35A0D4287899ED96F3"
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
    if ($RenderHashes["gameplay-uniform-pacers"] -ne
            "6821B3B5E608CA3ECDA5EC4465C46D18BC8E1B5E517CB71DAF4E82CADA46C53D" -or
        $RenderHashes["gameplay-uniform-pacers"] -eq
            $RenderHashes["gameplay-possession-center"]) {
        throw "Gameplay home-team uniform-color visual contract changed."
    }
    $ExpectedFreeThrowHashes = @{
        "gameplay-free-throw-left" =
            "D3012C809DC01D409A2C97A74FF45B6301358F2CCBADC2BAC4B64A4997AEC711"
        "gameplay-free-throw-right" =
            "3E6418B4D5D6447F647451D2EA58C3C675D227D74316DE6FFEB2275AD8505294"
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
        "gameplay-jump-frame1" =
            "BC149B0798D242B88C73A7C1C10B5994BC131A51ACCB9FFF65B9C367EF7F3AFF"
        "gameplay-jump-frame2" =
            "FF74C08F9FCAD96468860DD20953D97AAD6BF4302C9330F0DE3AAA82FD57A8EF"
        "gameplay-jump-frame4" =
            "777F258BC7D41327E7C70C7A6B4701CC440BB4BE91295BCDAC177392C913514F"
        "gameplay-jump-frame75" =
            "2F01E74DCE0EC0E4551DBAA564D9B928B22BD0FA8C310504D075B67E7F20D058"
        "gameplay-jump-frame87" =
            "C4D82A29026532740886CC1AB5F43F583FD1C455E3600730E23C204E901BE3E7"
    }
    foreach ($Mode in $ExpectedJumpHashes.Keys) {
        if ($RenderHashes[$Mode] -ne $ExpectedJumpHashes[$Mode]) {
            throw (("Gameplay jump-miss render hash changed at '{0}': " +
                "expected {1}, actual {2}.") -f $Mode,
                $ExpectedJumpHashes[$Mode], $RenderHashes[$Mode])
        }
    }
    $JumpPoseVisuals = @(
        $RenderHashes["gameplay-jump-frame1"],
        $RenderHashes["gameplay-jump-frame2"],
        $RenderHashes["gameplay-jump-frame4"]
    ) | Select-Object -Unique
    if ($JumpPoseVisuals.Count -ne 3) {
        throw "Gameplay jump entry/release/flight visuals collapsed together."
    }
    $ExpectedRattleHashes = @{
        "gameplay-jump-rattle-frame72" =
            "4D190DD826282A18E68B7657BD033AF5FD73CF79E3C3446E33B83292B1D911B1"
        "gameplay-jump-rattle-frame73" =
            "1FDF4E453E11472903DEEC41368749B27C166764CA592ED221EC0D1BE7ED7259"
        "gameplay-jump-rattle-frame74" =
            "3BF88B742EC383E5A9872E972F632A172F912BF1C663673C2F73C376A41C412C"
        "gameplay-jump-rattle-frame77" =
            "58B2BA17AEFFC157B134EA41671D1C225E62BE321BBEDD8A608794570D7DEBBB"
        "gameplay-jump-rattle-frame81" =
            "C3CDBC9DED013917B74E811CA2A69ABB914BD785E4C4B7BACFE065EFBC69306E"
        "gameplay-jump-rattle-frame85" =
            "6A8283F3E719C4A4F2DC979434D1209C956B9645E5C68150CE1419C6D254B56D"
        "gameplay-jump-rattle-frame88" =
            "E7D0425A450AF295D411E977CF44D3A90B272E7CE56A0B2F4A827054AA9E3BE7"
        "gameplay-jump-rattle-frame89" =
            "C3CDBC9DED013917B74E811CA2A69ABB914BD785E4C4B7BACFE065EFBC69306E"
        "gameplay-jump-rattle-frame90" =
            "2CAC7F160A53F1D75A1F24932D499A648EE80CDD04CAF4DE100DA75079A376B9"
        "gameplay-jump-rattle-frame103" =
            "07B3A399BFD1E2FA6CB68BA886FF8AE9A59E71C5DBF55494E7C21F2F198859F5"
    }
    foreach ($Mode in $ExpectedRattleHashes.Keys) {
        if ($RenderHashes[$Mode] -ne $ExpectedRattleHashes[$Mode]) {
            throw (("Gameplay rim-rattle render hash changed at '{0}': " +
                "expected {1}, actual {2}.") -f $Mode,
                $ExpectedRattleHashes[$Mode], $RenderHashes[$Mode])
        }
    }
    $ExpectedJumpMakeHashes = @{
        "gameplay-jump-make-frame9" =
            "BEA3F2298220570CEEF80101C6174BFEB0ADC4539F84162704A5BBB28334092B"
        "gameplay-jump-make-frame20" =
            "5C89D0E685E86AE7EC659E343C6260CEF4614029FE4993C6337700E89245C73B"
        "gameplay-jump-make-frame57" =
            "EF590F4EE16BD2DB3F8C49336013D38ABE0B2D61F621439DD426604960AE65DF"
        "gameplay-jump-make-frame85" =
            "BD6F98B8ED82E8B5CA18A1B9A960C6BBE60BAB1AA2CDFF21C9A0FA89C29C29BB"
        "gameplay-jump-make-frame111" =
            "347B1BEF838E4F2A743E3F6C0E8F6626718DEF6790A0C82E8D8CF47542EA5DCE"
    }
    foreach ($Mode in $ExpectedJumpMakeHashes.Keys) {
        if ($RenderHashes[$Mode] -ne $ExpectedJumpMakeHashes[$Mode]) {
            throw (("Gameplay jump-make render hash changed at '{0}': " +
                "expected {1}, actual {2}.") -f $Mode,
                $ExpectedJumpMakeHashes[$Mode], $RenderHashes[$Mode])
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

    $ProofLogRecords = Copy-LiveProofLogs -ScratchRoot $Scratch `
        -BuildLogSource $BuildLog -BuildWasRun ([bool]$Build) `
        -ProofRoot $ProofRoot
    if ($ProofLogRecords.Count -lt 3) {
        throw "LIVE proof did not preserve the required build/asset-pack/scene logs."
    }
    $FinalGitProofState = Get-LiveProofGitState
    if ($RequirePass -and !(Test-LiveProofRequirePassState $FinalGitProofState)) {
        throw ("LIVE proof -RequirePass detected a changed or dirty final worktree: " +
            "branch=$($FinalGitProofState.branch) clean=$($FinalGitProofState.clean) " +
            "base_is_ancestor=$($FinalGitProofState.base_is_ancestor)")
    }
    $FinalManifest = Get-Content -LiteralPath $ManifestPath -Raw |
        ConvertFrom-Json
    $FinalManifest.status = if ($RequirePass) { "PASS" } else { "DRAFT" }
    $FinalManifest.current_sha = $FinalGitProofState.head
    $FinalManifest.final_sha = if ($RequirePass) {
        $FinalGitProofState.head
    } else { "PENDING_CLEAN_COMMIT" }
    $FinalManifest.branch = $FinalGitProofState.branch
    $FinalManifest.clean = [bool]$FinalGitProofState.clean
    $FinalManifest.suites_complete = $true
    $FinalManifest.require_pass = [bool]$RequirePass
    $FinalManifest.build_requested = [bool]$Build
    $FinalManifest.build_warning_clean = [bool]$Build
    $FinalManifest.asset_pack.path = [IO.Path]::GetFullPath($ProofPackPath)
    $FinalManifest.asset_pack.replay_path = [IO.Path]::GetFullPath($ProofPackPath)
    $FinalManifest.asset_pack.source_path_ephemeral =
        [IO.Path]::GetFullPath($PackPath)
    $FinalManifest.required_logs = $ProofLogRecords
    $FinalManifest.timestamps_utc.proof_finished =
        [DateTime]::UtcNow.ToString("o")
    $FinalCommands = @(
        ("build: {0}" -f (Join-Path $ProjectRoot "build.ps1"))
        ("asset-pack: {0} --build-assetpack [LOCAL_REV1_ROM] " +
            "[EPHEMERAL_SCRATCH_PACK] (preserved replay pack: {1})" -f
            $Executable, $ProofPackPath)
        ("scene-wrapper: powershell.exe -NoProfile -ExecutionPolicy Bypass " +
            "-File tools\\Run-GameplaySceneTests.ps1 -ProjectRoot [PROJECT] " +
            "-RomPath [LOCAL_REV1_ROM] -OriginalReferenceManifestPath " +
            "[ACCEPTED_CPU_PROOF_MANIFEST]")
    )
    foreach ($Repeat in 1, 2) {
        foreach ($Event in $ProofEvents) {
            $FinalCommands += ("native-proof: {0} --root [PROJECT] " +
                "--gameplay-live-foundation-proof {1} {2} {3}" -f
                $Executable, $ProofPackPath, $Event,
                (Join-Path $ProofRoot ("repeat-{0}\\{1}.png" -f $Repeat, $Event)))
        }
    }
    foreach ($Video in $NativeVideos) {
        $FinalCommands += "ffmpeg: $($Video.commands.ffmpeg)"
        $FinalCommands += "ffprobe: $($Video.commands.ffprobe)"
        $FinalCommands += "decode: $($Video.commands.decode)"
    }
    $FinalManifest.commands = $FinalCommands
    $FinalManifest.tool_versions = [ordered]@{
        powershell = $PSVersionTable.PSVersion.ToString()
        ffmpeg = $NativeVideos[0].tools.ffmpeg
        ffprobe = $NativeVideos[0].tools.ffprobe
    }
    $FinalManifest.artifact_inventory = Get-LiveProofArtifactInventory `
        -Root $ProofRoot -ManifestName "PROOF-MANIFEST.json"
    $FinalManifest.artifact_inventory_count =
        @($FinalManifest.artifact_inventory).Count
    $InventoryNegativeBase = $FinalManifest | ConvertTo-Json -Depth 20 |
        ConvertFrom-Json
    $InventoryNegativeBase.artifact_inventory[0].path =
        (Join-Path $ProofRoot "missing-inventory-artifact.bin")
    $InventoryPathNegativePath =
        Join-Path $ProofRoot "PROOF-MANIFEST-negative-inventory-path.json"
    $InventoryNegativeBase | ConvertTo-Json -Depth 20 |
        Set-Content -LiteralPath $InventoryPathNegativePath -Encoding UTF8
    if (Test-LiveProofManifest -ManifestPath $InventoryPathNegativePath `
            -ExpectedRomSha256 $RomSha -ExpectedPackSha256 $PackSha `
            -ExpectedEvents $ProofEvents) {
        throw "LIVE proof artifact-inventory path negative was accepted."
    }
    $InventoryNegativeBase = $FinalManifest | ConvertTo-Json -Depth 20 |
        ConvertFrom-Json
    $InventoryNegativeBase.artifact_inventory[0].bytes =
        [int64]$InventoryNegativeBase.artifact_inventory[0].bytes + 1
    $InventoryBytesNegativePath =
        Join-Path $ProofRoot "PROOF-MANIFEST-negative-inventory-bytes.json"
    $InventoryNegativeBase | ConvertTo-Json -Depth 20 |
        Set-Content -LiteralPath $InventoryBytesNegativePath -Encoding UTF8
    if (Test-LiveProofManifest -ManifestPath $InventoryBytesNegativePath `
            -ExpectedRomSha256 $RomSha -ExpectedPackSha256 $PackSha `
            -ExpectedEvents $ProofEvents) {
        throw "LIVE proof artifact-inventory byte negative was accepted."
    }
    $InventoryNegativeBase = $FinalManifest | ConvertTo-Json -Depth 20 |
        ConvertFrom-Json
    $InventoryNegativeBase.artifact_inventory[0].sha256 = '0' * 64
    $InventoryShaNegativePath =
        Join-Path $ProofRoot "PROOF-MANIFEST-negative-inventory-sha.json"
    $InventoryNegativeBase | ConvertTo-Json -Depth 20 |
        Set-Content -LiteralPath $InventoryShaNegativePath -Encoding UTF8
    if (Test-LiveProofManifest -ManifestPath $InventoryShaNegativePath `
            -ExpectedRomSha256 $RomSha -ExpectedPackSha256 $PackSha `
            -ExpectedEvents $ProofEvents) {
        throw "LIVE proof artifact-inventory SHA negative was accepted."
    }
    $FinalManifest.negative_regressions += @(
        "artifact-inventory missing path rejects",
        "artifact-inventory byte mismatch rejects",
        "artifact-inventory SHA mismatch rejects"
    )
    $FinalManifest.artifact_inventory = Get-LiveProofArtifactInventory `
        -Root $ProofRoot -ManifestName "PROOF-MANIFEST.json"
    $FinalManifest.artifact_inventory_count =
        @($FinalManifest.artifact_inventory).Count
    $FinalManifest | ConvertTo-Json -Depth 20 |
        Set-Content -LiteralPath $ManifestPath -Encoding UTF8
    if (!(Test-LiveProofManifest -ManifestPath $ManifestPath `
            -ExpectedRomSha256 $RomSha -ExpectedPackSha256 $PackSha `
            -ExpectedEvents $ProofEvents)) {
        throw "LIVE proof final manifest self-validation failed."
    }
    if ($RequirePass -and $FinalManifest.status -ne "PASS") {
        throw "LIVE proof -RequirePass did not publish status=PASS."
    }

    $global:LASTEXITCODE = 0
    Write-Output ("GAMEPLAY SCENE TEST PASS: Rev1 full-pack provenance " +
        "scene controls THUD-1 clean jersey/name HUD TGMO-1 human/CPU walking poses TGBD-1 held-ball bounce/sound TGFT-1 fatigue TPNL-1 out-of-bounds settlement TGBC-1 live backcourt settlement TGVR-1 native violation referee TGAI-3/TGMO-1 transactional ordinary CPU movement with opcode-15 raw-owner defer diagnostics TGCP-2 full-world camera fine-scroll guarded-margins actor-camera-projection/possession-slice-render/freeze TGFL-1 orientation-lineup TGOR two-basket shot ownership TGDK TGJS TGSR-4 jump entry/turn/release/flight poses jump-miss/jump-make/rim-rattle early-release/expiry shots dunk-cutaway frame75/audio state " +
        "halftime/final render-hashes/determinism missing malformed oversized " +
        "dependency-corrupt chr-mismatch")
    $ProofSummary = ("LIVE PROOF {0}: root={1} manifest={2} native_videos=2 frames={3} contact_sheet=1920x{4}" -f
        $FinalManifest.status, $ProofRoot, $ManifestPath,
        ([int]$ProofEvents.Count * 2),
        [int]([Math]::Ceiling($ProofEvents.Count / 3.0) * 480))
    Write-Output $ProofSummary
} finally {
    $env:TECMO_ASSETPACK = $PreviousPack
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
}
