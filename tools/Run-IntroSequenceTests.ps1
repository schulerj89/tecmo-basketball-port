param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [switch]$Build,
    [string]$AssetPackPath,
    [string]$ReportPath
)

$ErrorActionPreference = "Stop"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path $ProjectRoot).Path

$BuildDir = Join-Path $ProjectRoot "build"
$OutputDir = Join-Path $BuildDir "intro_sequence"
$CheckpointOutputDir = Join-Path $BuildDir "arena_goal_motion_fixed"
if (!$AssetPackPath) {
    $AssetPackPath = Join-Path $OutputDir "tecmo_intro_sequence_test.assetpack"
}
if (![System.IO.Path]::IsPathRooted($AssetPackPath)) {
    $AssetPackPath = Join-Path $ProjectRoot $AssetPackPath
}
$AssetPackPath = [System.IO.Path]::GetFullPath($AssetPackPath)
if (!$ReportPath) {
    $ReportPath = Join-Path $BuildDir "intro_sequence_test_report.json"
}
if (![System.IO.Path]::IsPathRooted($ReportPath)) {
    $ReportPath = Join-Path $ProjectRoot $ReportPath
}
$ReportPath = [System.IO.Path]::GetFullPath($ReportPath)

function Find-ReferenceRom {
    if ($RomPath) {
        if (!(Test-Path $RomPath)) {
            throw "RomPath does not exist."
        }
        return (Resolve-Path $RomPath).Path
    }
    if ($env:TECMO_ROM_PATH -and (Test-Path $env:TECMO_ROM_PATH)) {
        return (Resolve-Path $env:TECMO_ROM_PATH).Path
    }

    return $null
}

function Test-PathUnder {
    param(
        [string]$Path,
        [string]$Parent
    )

    $FullPath = [System.IO.Path]::GetFullPath($Path)
    $FullParent = [System.IO.Path]::GetFullPath($Parent).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    return $FullPath.StartsWith($FullParent, [System.StringComparison]::OrdinalIgnoreCase)
}

function Get-RepoRelativePath {
    param(
        [string]$Path
    )

    $FullPath = [System.IO.Path]::GetFullPath($Path)
    $FullRoot = [System.IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\', '/')
    $RootWithSlash = $FullRoot + [System.IO.Path]::DirectorySeparatorChar
    if ($FullPath.StartsWith($RootWithSlash, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $FullPath.Substring($RootWithSlash.Length).Replace('\', '/')
    }
    if ($FullPath.Equals($FullRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        return "."
    }
    return "<outside-project>"
}

function Get-SafeExceptionName {
    param($ErrorRecord)

    if ($ErrorRecord -and $ErrorRecord.Exception) {
        return $ErrorRecord.Exception.GetType().Name
    }
    return "Error"
}

function Get-GroundedGoalWhiteMinY {
    param([string]$Path)

    $Bitmap = [System.Drawing.Bitmap]::FromFile($Path)
    try {
        # The goal outline is the only exact NES white in this arena crop.
        $MinY = [int]::MaxValue
        for ($Y = 300; $Y -lt 455; ++$Y) {
            for ($X = 390; $X -lt 475; ++$X) {
                $Pixel = $Bitmap.GetPixel($X, $Y)
                if ($Pixel.R -eq 252 -and $Pixel.G -eq 252 -and $Pixel.B -eq 252) {
                    $MinY = [Math]::Min($MinY, $Y)
                }
            }
        }
        if ($MinY -eq [int]::MaxValue) {
            throw "Grounded goal marker pixels were not found."
        }
        return $MinY
    } finally {
        $Bitmap.Dispose()
    }
}

function Get-AssetPackEntryPayloadOffset {
    param(
        [byte[]]$Bytes,
        [string]$EntryId
    )

    if ($Bytes.Length -lt 40 -or
        [System.Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -ne "TAP1") {
        throw "Malformed asset-pack header."
    }

    $EntryCount = [System.BitConverter]::ToUInt32($Bytes, 16)
    $DirectoryOffset = [System.BitConverter]::ToUInt64($Bytes, 20)
    for ($EntryIndex = 0; $EntryIndex -lt $EntryCount; ++$EntryIndex) {
        $EntryOffset64 = $DirectoryOffset + [uint64]($EntryIndex * 128)
        if ($EntryOffset64 + 128 -gt [uint64]$Bytes.Length -or
            $EntryOffset64 -gt [int]::MaxValue) {
            throw "Malformed asset-pack directory."
        }

        $EntryOffset = [int]$EntryOffset64
        $EntryNameLength = 0
        while ($EntryNameLength -lt 64 -and $Bytes[$EntryOffset + $EntryNameLength] -ne 0) {
            ++$EntryNameLength
        }
        $EntryName = [System.Text.Encoding]::ASCII.GetString(
            $Bytes,
            $EntryOffset,
            $EntryNameLength)
        if ($EntryName -eq $EntryId) {
            $PayloadOffset = [System.BitConverter]::ToUInt64($Bytes, $EntryOffset + 84)
            $PayloadSize = [System.BitConverter]::ToUInt64($Bytes, $EntryOffset + 92)
            if ($PayloadOffset + $PayloadSize -gt [uint64]$Bytes.Length -or
                $PayloadOffset -gt [int]::MaxValue) {
                throw "Malformed asset-pack entry payload."
            }
            return [int]$PayloadOffset
        }
    }

    throw "Asset-pack entry was not found."
}

$ExePath = Join-Path $BuildDir "tecmo_port.exe"

$ReferenceRom = Find-ReferenceRom
if (!$ReferenceRom) {
    throw "Could not find a local iNES ROM. Pass -RomPath or set TECMO_ROM_PATH."
}

if (!(Test-PathUnder $AssetPackPath $BuildDir)) {
    throw "Intro sequence asset pack output must stay under build\."
}
if (!(Test-PathUnder $ReportPath $BuildDir)) {
    throw "Intro sequence report must stay under build\."
}
if (!$AssetPackPath.EndsWith(".assetpack", [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Intro sequence asset pack output must use the .assetpack extension."
}

if ($Build) {
    $PreviousSkipShortcut = $env:TECMO_SKIP_SHORTCUT
    $env:TECMO_SKIP_SHORTCUT = "1"
    try {
        & (Join-Path $ProjectRoot "build.ps1")
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed with exit code $LASTEXITCODE."
        }
    } finally {
        $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    }
}
if (!(Test-Path $ExePath)) {
    throw "Executable not found at $ExePath. Run .\build.ps1 or pass -Build."
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
New-Item -ItemType Directory -Force -Path $CheckpointOutputDir | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $AssetPackPath) | Out-Null
Add-Type -AssemblyName System.Drawing
$RomOnlyArenaRenderCases = @(
    [pscustomobject]@{ mode = "intro-arena-frame0"; expected_goal = 0; expected_jumbotron = 55; checkpoint = $false },
    [pscustomobject]@{ mode = "intro-arena-frame240"; expected_goal = 10; expected_jumbotron = $null; checkpoint = $true },
    [pscustomobject]@{ mode = "intro-arena-frame260"; expected_goal = 10; expected_jumbotron = $null; checkpoint = $true },
    [pscustomobject]@{ mode = "intro-arena-frame280"; expected_goal = 15; expected_jumbotron = $null; checkpoint = $true },
    [pscustomobject]@{ mode = "intro-arena-frame300"; expected_goal = 15; expected_jumbotron = $null; checkpoint = $true },
    [pscustomobject]@{ mode = "intro-arena-frame320"; expected_goal = 16; expected_jumbotron = $null; checkpoint = $true },
    [pscustomobject]@{ mode = "intro-arena-frame340"; expected_goal = 16; expected_jumbotron = $null; checkpoint = $true },
    [pscustomobject]@{ mode = "intro-arena-frame386"; expected_goal = 16; expected_jumbotron = $null; checkpoint = $true },
    [pscustomobject]@{ mode = "intro-arena-frame539"; expected_goal = 16; expected_jumbotron = $null; checkpoint = $true }
)
$RomOnlyArenaRenderModes = @($RomOnlyArenaRenderCases | ForEach-Object { $_.mode })

$Results = New-Object System.Collections.Generic.List[object]
$Failures = 0
$Skipped = 0
$AssetPackRelative = Get-RepoRelativePath $AssetPackPath

try {
    if (Test-Path -LiteralPath $AssetPackPath) {
        Remove-Item -LiteralPath $AssetPackPath -Force
    }

    Write-Host "Building intro sequence test asset pack -> $AssetPackRelative"
    $PackBuildOutput = & $ExePath --build-assetpack $ReferenceRom $AssetPackPath 2>&1
    $PackBuildExitCode = $LASTEXITCODE
    $PackCreated = Test-Path -LiteralPath $AssetPackPath
    $PackBuildOutputText = (@($PackBuildOutput) | ForEach-Object { [string]$_ }) -join "`n"
    $RomOnlyMessageSeen = $PackBuildOutputText -match "from iNES ROM"
    $PackBuildPassed = $PackBuildExitCode -eq 0 -and $PackCreated -and $RomOnlyMessageSeen
    if (!$PackBuildPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "intro-test-assetpack-build"
        command = ".\build\tecmo_port.exe --build-assetpack <LOCAL_ROM.nes> $($AssetPackRelative.Replace('/', '\'))"
        output = $AssetPackRelative
        passed = $PackBuildPassed
        skipped = $false
        exit_code = $PackBuildExitCode
        created = $PackCreated
        rom_only_message_seen = $RomOnlyMessageSeen
        raw_output_persisted = $false
        error = if ($PackBuildPassed) { $null } else { "fresh test asset pack was not built" }
    })

    $ListOutput = & $ExePath --assetpack-list $AssetPackPath 2>&1
    $ListExitCode = $LASTEXITCODE
    $ListText = (@($ListOutput) | ForEach-Object { [string]$_ }) -join "`n"
    $RequiredNativeEntries = @(
        "arena/intro/script",
        "arena/intro/background-layer",
        "arena/intro/palette-cycle",
        "arena/intro/sprite-groups"
    )
    $ForbiddenCaptureEntries = @("intro/arena/capture.ndjson", "intro/post-arena/capture.ndjson", "intro/captures/source-map")
    $MissingNativeEntries = @($RequiredNativeEntries | Where-Object { $ListText -notmatch [regex]::Escape($_) })
    $PresentForbiddenCaptureEntries = @($ForbiddenCaptureEntries | Where-Object { $ListText -match [regex]::Escape($_) })
    $ListPassed = $ListExitCode -eq 0 -and
        $MissingNativeEntries.Count -eq 0 -and
        $PresentForbiddenCaptureEntries.Count -eq 0
    if (!$ListPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "intro-test-assetpack-rom-only"
        output = $AssetPackRelative
        passed = $ListPassed
        skipped = $false
        exit_code = $ListExitCode
        required_native_entries = $RequiredNativeEntries
        missing_native_entries = $MissingNativeEntries
        forbidden_capture_entries = $ForbiddenCaptureEntries
        present_forbidden_capture_entries = $PresentForbiddenCaptureEntries
        raw_output_persisted = $false
        error = if ($ListPassed) { $null } else { "ROM-only test asset pack is missing native arena entries or contains capture entries" }
    })

    $PreviousAssetPack = $env:TECMO_ASSETPACK
    $PreviousLooseArenaCapture = $env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE
    $RenderOutputs = New-Object System.Collections.Generic.List[object]
    $RenderPassed = $ListPassed
    try {
        $env:TECMO_ASSETPACK = $AssetPackPath
        Remove-Item Env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE -ErrorAction SilentlyContinue

        foreach ($RenderCase in $RomOnlyArenaRenderCases) {
            $Mode = $RenderCase.mode
            $RenderDirectory = if ($RenderCase.checkpoint) { $CheckpointOutputDir } else { $OutputDir }
            $RenderPath = Join-Path $RenderDirectory "$Mode.png"
            if (Test-Path -LiteralPath $RenderPath) {
                Remove-Item -LiteralPath $RenderPath -Force
            }
            $RenderOutput = & $ExePath --root $ProjectRoot --render-test-mode $Mode $RenderPath 2>&1
            $RenderExitCode = $LASTEXITCODE
            $RenderText = (@($RenderOutput) | ForEach-Object { [string]$_ }) -join "`n"
            $RenderCreated = Test-Path -LiteralPath $RenderPath
            $CaptureUnavailableSeen = $RenderText -match "intro-capture-status kind=arena available=0"
            $NoCaptureSourceSeen = $RenderText -match "intro-capture-source kind=arena assetpack=0 entry=none"
            $ExactLayerSeen = $RenderText -match "intro-arena-render-source kind=arena exact_layer=1 rendered=1 cells=1632 palette=16"
            $TasgContractSeen = $RenderText -match "sprite_groups=2 jumbotron_pieces=55 goal_pieces=16"
            $VisibilityMatch = [regex]::Match(
                $RenderText,
                "visible_jumbotron=([0-9]+) visible_goal=([0-9]+)")
            $ActualJumbotron = if ($VisibilityMatch.Success) {
                [int]$VisibilityMatch.Groups[1].Value
            } else {
                -1
            }
            $ActualGoal = if ($VisibilityMatch.Success) {
                [int]$VisibilityMatch.Groups[2].Value
            } else {
                -1
            }
            $ExactGoalSeen = $ActualGoal -eq [int]$RenderCase.expected_goal
            $ExactJumbotronSeen = $null -eq $RenderCase.expected_jumbotron -or
                $ActualJumbotron -eq [int]$RenderCase.expected_jumbotron
            $FrameVisibilitySeen = $VisibilityMatch.Success -and
                $ExactGoalSeen -and $ExactJumbotronSeen
            $ModePassed = $RenderExitCode -eq 0 -and
                $RenderCreated -and
                $CaptureUnavailableSeen -and
                $NoCaptureSourceSeen -and
                $ExactLayerSeen -and
                $TasgContractSeen -and
                $FrameVisibilitySeen
            if (!$ModePassed) {
                $RenderPassed = $false
            }
            $RenderOutputs.Add([pscustomobject]@{
                mode = $Mode
                output = Get-RepoRelativePath $RenderPath
                passed = $ModePassed
                exit_code = $RenderExitCode
                capture_unavailable_seen = $CaptureUnavailableSeen
                no_capture_source_seen = $NoCaptureSourceSeen
                exact_layer_seen = $ExactLayerSeen
                tasg_contract_seen = $TasgContractSeen
                visible_jumbotron = $ActualJumbotron
                expected_jumbotron = $RenderCase.expected_jumbotron
                visible_goal = $ActualGoal
                expected_goal = $RenderCase.expected_goal
                frame_visibility_seen = $FrameVisibilitySeen
                checkpoint = [bool]$RenderCase.checkpoint
            })
        }
    } finally {
        $env:TECMO_ASSETPACK = $PreviousAssetPack
        $env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE = $PreviousLooseArenaCapture
    }
    if (!$RenderPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "intro-render-rom-only-arena"
        passed = $RenderPassed
        skipped = $false
        render_modes = $RomOnlyArenaRenderModes
        outputs = $RenderOutputs
        rom_only_asset_pack = $AssetPackRelative
        raw_output_persisted = $false
        coverage_status = "covered"
        error = if ($RenderPassed) { $null } else { "ROM-only arena render did not complete without capture data" }
    })

    $GroundedFrame260Path = Join-Path $CheckpointOutputDir "intro-arena-frame260.png"
    $GroundedFrame280Path = Join-Path $CheckpointOutputDir "intro-arena-frame280.png"
    $GroundedFinalPath = Join-Path $CheckpointOutputDir "intro-arena-frame539.png"
    $GroundedFrame260MinY = Get-GroundedGoalWhiteMinY $GroundedFrame260Path
    $GroundedFrame280MinY = Get-GroundedGoalWhiteMinY $GroundedFrame280Path
    $GroundedGoalPixelDelta = $GroundedFrame280MinY - $GroundedFrame260MinY
    # Transition state moves scroll $38->$42 and stream1 $0117->$010D.
    $GroundedBackgroundNativeDelta = -(0x42 - 0x38)
    $GroundedBackgroundPixelDelta = 2 * $GroundedBackgroundNativeDelta
    $GroundedStreamNativeDelta = 0x010D - 0x0117
    # Verified against the untouched cbd37cc final frame.
    $ExpectedFinalHash = "96994C5EF4919AB1FBA63B98BB7CEE874F8109F437700DCE60D8EB44B5C1EBB8"
    $ActualFinalHash = (Get-FileHash -Algorithm SHA256 $GroundedFinalPath).Hash
    $GroundedContractPassed = $GroundedGoalPixelDelta -eq $GroundedBackgroundPixelDelta -and
        $GroundedStreamNativeDelta -eq $GroundedBackgroundNativeDelta -and
        $ActualFinalHash -eq $ExpectedFinalHash
    if (!$GroundedContractPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "intro-arena-native-grounded-goal-contract"
        passed = $GroundedContractPassed
        skipped = $false
        frame260_goal_marker_min_y = $GroundedFrame260MinY
        frame280_goal_marker_min_y = $GroundedFrame280MinY
        goal_pixel_delta = $GroundedGoalPixelDelta
        background_pixel_delta = $GroundedBackgroundPixelDelta
        stream1_native_delta = $GroundedStreamNativeDelta
        background_native_delta = $GroundedBackgroundNativeDelta
        final539_sha256 = $ActualFinalHash
        expected_final539_sha256 = $ExpectedFinalHash
        raw_output_persisted = $false
        coverage_status = "grounded-ease-delta-and-rom-identical-final"
        error = if ($GroundedContractPassed) { $null } else { "native grounded goal delta or final pose changed" }
    })

    $Page01BaselinePath = Join-Path $CheckpointOutputDir "intro-arena-frame240.png"
    $Page01MarkerPackPath = Join-Path $OutputDir "page01-marker.assetpack"
    $Page01MarkerRenderPath = Join-Path $OutputDir "page01-marker-frame240.png"
    $Page01MarkerPassed = $false
    $Page01PieceCount = 0
    $Page01MarkerExitCode = -1
    $PreviousAssetPack = $env:TECMO_ASSETPACK
    $PreviousLooseArenaCapture = $env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE
    try {
        [byte[]]$MarkerBytes = [System.IO.File]::ReadAllBytes($AssetPackPath)
        $TasgPayloadOffset = Get-AssetPackEntryPayloadOffset `
            -Bytes $MarkerBytes `
            -EntryId "arena/intro/sprite-groups"
        $GroupCount = [System.BitConverter]::ToUInt16($MarkerBytes, $TasgPayloadOffset + 8)
        $GroupStride = [System.BitConverter]::ToUInt16($MarkerBytes, $TasgPayloadOffset + 10)
        $PieceStride = [System.BitConverter]::ToUInt16($MarkerBytes, $TasgPayloadOffset + 16)
        $GroupsOffset = [System.BitConverter]::ToUInt32($MarkerBytes, $TasgPayloadOffset + 24)
        $PiecesOffset = [System.BitConverter]::ToUInt32($MarkerBytes, $TasgPayloadOffset + 28)
        $Page01PieceOffset = -1

        for ($GroupIndex = 0; $GroupIndex -lt $GroupCount; ++$GroupIndex) {
            $GroupOffset = $TasgPayloadOffset + [int]$GroupsOffset + $GroupIndex * $GroupStride
            if ([System.BitConverter]::ToUInt16($MarkerBytes, $GroupOffset) -ne 2) {
                continue
            }
            $FirstPiece = [System.BitConverter]::ToUInt32($MarkerBytes, $GroupOffset + 4)
            $GoalPieceCount = [System.BitConverter]::ToUInt32($MarkerBytes, $GroupOffset + 8)
            for ($PieceIndex = 0; $PieceIndex -lt $GoalPieceCount; ++$PieceIndex) {
                $AbsolutePieceIndex = $FirstPiece + $PieceIndex
                $PieceOffset = $TasgPayloadOffset + [int]$PiecesOffset +
                    [int]$AbsolutePieceIndex * $PieceStride
                $Dy = [System.BitConverter]::ToInt16($MarkerBytes, $PieceOffset + 2)
                $Projected = (0x011E + $Dy - 0x40) -band 0xFFFF
                if (($Projected -shr 8) -eq 0x01) {
                    ++$Page01PieceCount
                    if ($Page01PieceOffset -lt 0) {
                        $Page01PieceOffset = $PieceOffset
                    }
                }
            }
        }
        if ($Page01PieceOffset -lt 0) {
            throw "The frame 240 TASG stream has no page01 goal marker candidate."
        }

        $OriginalTopChr = [System.BitConverter]::ToUInt32($MarkerBytes, $Page01PieceOffset + 4)
        $MarkerTopChr = if ($OriginalTopChr -eq 8192) { 8224 } else { 8192 }
        [System.BitConverter]::GetBytes([uint32]$MarkerTopChr).CopyTo(
            $MarkerBytes,
            $Page01PieceOffset + 4)
        $MarkerBytes[$Page01PieceOffset + 8] = [byte](
            ([int]$MarkerBytes[$Page01PieceOffset + 8] + 1) % 4)
        $MarkerBytes[$Page01PieceOffset + 9] = [byte](
            $MarkerBytes[$Page01PieceOffset + 9] -bxor 0x01)
        [System.IO.File]::WriteAllBytes($Page01MarkerPackPath, $MarkerBytes)

        if (Test-Path -LiteralPath $Page01MarkerRenderPath) {
            Remove-Item -LiteralPath $Page01MarkerRenderPath -Force
        }
        $env:TECMO_ASSETPACK = $Page01MarkerPackPath
        Remove-Item Env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE -ErrorAction SilentlyContinue
        $Page01MarkerOutput = & $ExePath `
            --root $ProjectRoot `
            --render-test-mode intro-arena-frame240 $Page01MarkerRenderPath 2>&1
        $Page01MarkerExitCode = $LASTEXITCODE
        $Page01MarkerText = (@($Page01MarkerOutput) | ForEach-Object { [string]$_ }) -join "`n"
        $Page01MarkerCreated = Test-Path -LiteralPath $Page01MarkerRenderPath
        $Page01MarkerCountSeen = $Page01MarkerText -match
            "visible_jumbotron=[0-9]+ visible_goal=10"
        $Page01PngIdentical = $Page01MarkerCreated -and
            (Test-Path -LiteralPath $Page01BaselinePath) -and
            (Get-FileHash -Algorithm SHA256 $Page01MarkerRenderPath).Hash -eq
                (Get-FileHash -Algorithm SHA256 $Page01BaselinePath).Hash
        $Page01MarkerPassed = $Page01MarkerExitCode -eq 0 -and
            $Page01PieceCount -gt 0 -and
            $Page01MarkerCountSeen -and
            $Page01PngIdentical
    } finally {
        $env:TECMO_ASSETPACK = $PreviousAssetPack
        $env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE = $PreviousLooseArenaCapture
        Remove-Item -LiteralPath $Page01MarkerPackPath -Force -ErrorAction SilentlyContinue
    }
    if (!$Page01MarkerPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "intro-arena-native-grounded-goal-page01-no-wrap"
        passed = $Page01MarkerPassed
        skipped = $false
        baseline = Get-RepoRelativePath $Page01BaselinePath
        marker_output = Get-RepoRelativePath $Page01MarkerRenderPath
        page01_goal_piece_count = $Page01PieceCount
        marker_exit_code = $Page01MarkerExitCode
        png_identical = $Page01PngIdentical
        raw_output_persisted = $false
        coverage_status = "count-marker-and-png"
        error = if ($Page01MarkerPassed) { $null } else { "a frame 240 native grounded page01 goal value wrapped into the visible PNG" }
    })

    $MalformedTasgCases = @(
        [pscustomobject]@{
            id = "top-tile-before-page-8"
            mutation = "u32"
            payload_offset = 108
            value = 8176
        },
        [pscustomobject]@{
            id = "bottom-tile-after-page-9"
            mutation = "u32"
            payload_offset = 108
            value = 10224
        },
        [pscustomobject]@{
            id = "priority-flag-unsupported"
            mutation = "or-byte"
            payload_offset = 113
            value = 0x04
        },
        [pscustomobject]@{
            id = "reserved-flag-unsupported"
            mutation = "or-byte"
            payload_offset = 113
            value = 0x08
        },
        [pscustomobject]@{
            id = "jumbotron-connector-overlay-unsupported"
            mutation = "i16"
            payload_offset = 114
            value = -1
        },
        [pscustomobject]@{
            id = "goal-connector-overlay-required"
            mutation = "i16"
            payload_offset = 858
            value = 0
        },
        [pscustomobject]@{
            id = "goal-connector-overlay-value-strict"
            mutation = "i16"
            payload_offset = 858
            value = -2
        }
    )
    $MalformedPackPath = Join-Path $OutputDir "malformed-tasg.assetpack"
    $MalformedRenderPath = Join-Path $OutputDir "malformed-tasg.png"
    $MalformedCaseResults = New-Object System.Collections.Generic.List[object]
    $MalformedContractPassed = $PackBuildPassed
    $PreviousAssetPack = $env:TECMO_ASSETPACK
    try {
        if ($PackBuildPassed) {
            foreach ($Case in $MalformedTasgCases) {
                [byte[]]$MalformedBytes = [System.IO.File]::ReadAllBytes($AssetPackPath)
                $TasgPayloadOffset = Get-AssetPackEntryPayloadOffset `
                    -Bytes $MalformedBytes `
                    -EntryId "arena/intro/sprite-groups"
                $MutationOffset = $TasgPayloadOffset + $Case.payload_offset
                if ($Case.mutation -eq "u32") {
                    [System.BitConverter]::GetBytes([uint32]$Case.value).CopyTo(
                        $MalformedBytes,
                        $MutationOffset)
                } elseif ($Case.mutation -eq "i16") {
                    [System.BitConverter]::GetBytes([int16]$Case.value).CopyTo(
                        $MalformedBytes,
                        $MutationOffset)
                } else {
                    $MalformedBytes[$MutationOffset] = [byte](
                        $MalformedBytes[$MutationOffset] -bor [byte]$Case.value)
                }
                [System.IO.File]::WriteAllBytes($MalformedPackPath, $MalformedBytes)

                if (Test-Path -LiteralPath $MalformedRenderPath) {
                    Remove-Item -LiteralPath $MalformedRenderPath -Force
                }
                $env:TECMO_ASSETPACK = $MalformedPackPath
                $MalformedRenderOutput = & $ExePath `
                    --root $ProjectRoot `
                    --render-test-mode intro-arena-frame0 $MalformedRenderPath 2>&1
                $MalformedRenderExitCode = $LASTEXITCODE
                $MalformedRenderText = (@($MalformedRenderOutput) | ForEach-Object { [string]$_ }) -join "`n"
                $Rejected = $MalformedRenderExitCode -eq 1 -and
                    !(Test-Path -LiteralPath $MalformedRenderPath) -and
                    $MalformedRenderText -match "intro-arena-render-source kind=arena exact_layer=1 rendered=0 cells=1632 palette=16 sprite_groups=0 jumbotron_pieces=0 goal_pieces=0 visible_jumbotron=0 visible_goal=0"
                if (!$Rejected) {
                    $MalformedContractPassed = $false
                }
                $MalformedCaseResults.Add([pscustomobject]@{
                    id = $Case.id
                    passed = $Rejected
                    exit_code = $MalformedRenderExitCode
                    rejected_by_runtime = $Rejected
                })
            }
        }
    } finally {
        $env:TECMO_ASSETPACK = $PreviousAssetPack
        Remove-Item -LiteralPath $MalformedPackPath -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $MalformedRenderPath -Force -ErrorAction SilentlyContinue
    }
    if (!$MalformedContractPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "intro-tasg-malformed-contract"
        passed = $MalformedContractPassed
        skipped = $false
        cases = $MalformedCaseResults
        raw_output_persisted = $false
        coverage_status = "covered"
        error = if ($MalformedContractPassed) { $null } else { "runtime accepted a malformed TASG sprite contract" }
    })
} catch {
    ++$Failures
    $Results.Add([pscustomobject]@{
        id = "intro-sequence-runner-error"
        passed = $false
        raw_output_persisted = $false
        private_paths_included = $false
        error = "intro sequence runner failed before completing all checks"
        error_type = Get-SafeExceptionName $_
    })
} finally {
    $Report = [pscustomobject]@{
        schema_version = 1
        generated_by = "tools/Run-IntroSequenceTests.ps1"
        generated_at_utc = [DateTime]::UtcNow.ToString("o")
        data_policy = "Sanitized intro-sequence smoke report only; raw stdout/stderr, local paths, ROM bytes, ASM, CHR bytes, trace payloads, and screenshots are not embedded."
        passed = $Failures -eq 0
        reference_rom_used = "<local>"
        build_requested = [bool]$Build
        output_directory = (Get-RepoRelativePath $OutputDir)
        asset_pack = [pscustomobject]@{
            output = $AssetPackRelative
            rom_only_contract = $true
        }
        render_coverage = [pscustomobject]@{
            status = "arena-native-grounded-goal-checkpoints"
            render_modes = $RomOnlyArenaRenderModes
            checkpoint_directory = Get-RepoRelativePath $CheckpointOutputDir
        }
        private_paths_included = $false
        raw_output_persisted = $false
        test_count = $Results.Count
        skipped_count = $Skipped
        failure_count = $Failures
        tests = $Results
    }

    $ReportDir = Split-Path -Parent $ReportPath
    if ($ReportDir) {
        New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null
    }
    $Report | ConvertTo-Json -Depth 8 | Set-Content -Path $ReportPath -Encoding ASCII
}

$Results | Format-Table id, passed, skipped, output -AutoSize
Write-Host "Wrote intro sequence test report: $(Get-RepoRelativePath $ReportPath)"

if ($Failures -ne 0) {
    throw "$Failures intro sequence smoke test(s) failed."
}
Write-Host "All intro sequence smoke tests passed."
