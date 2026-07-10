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
$CheckpointOutputDir = Join-Path $BuildDir "arena_rom_exact"
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

function Test-PixelRectColor {
    param(
        [System.Drawing.Bitmap]$Bitmap,
        [int]$Left,
        [int]$Top,
        [int]$Right,
        [int]$Bottom,
        [int]$Red,
        [int]$Green,
        [int]$Blue
    )

    for ($Y = $Top; $Y -le $Bottom; ++$Y) {
        for ($X = $Left; $X -le $Right; ++$X) {
            $Pixel = $Bitmap.GetPixel($X, $Y)
            if ($Pixel.R -ne $Red -or $Pixel.G -ne $Green -or $Pixel.B -ne $Blue) {
                return $false
            }
        }
    }
    return $true
}

function Test-PixelRectHasNonBlack {
    param(
        [System.Drawing.Bitmap]$Bitmap,
        [int]$Left,
        [int]$Top,
        [int]$Right,
        [int]$Bottom
    )

    for ($Y = $Top; $Y -le $Bottom; ++$Y) {
        for ($X = $Left; $X -le $Right; ++$X) {
            $Pixel = $Bitmap.GetPixel($X, $Y)
            if ($Pixel.R -ne 0 -or $Pixel.G -ne 0 -or $Pixel.B -ne 0) {
                return $true
            }
        }
    }
    return $false
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
    [pscustomobject]@{ mode = "intro-arena-frame240"; expected_goal = 0; expected_jumbotron = $null; checkpoint = $true },
    [pscustomobject]@{ mode = "intro-arena-frame260"; expected_goal = 5; expected_jumbotron = $null; checkpoint = $true },
    [pscustomobject]@{ mode = "intro-arena-frame276"; expected_goal = 10; expected_jumbotron = $null; checkpoint = $true },
    [pscustomobject]@{ mode = "intro-arena-frame280"; expected_goal = 10; expected_jumbotron = $null; checkpoint = $true },
    [pscustomobject]@{ mode = "intro-arena-frame292"; expected_goal = 15; expected_jumbotron = $null; checkpoint = $true },
    [pscustomobject]@{ mode = "intro-arena-frame300"; expected_goal = 15; expected_jumbotron = $null; checkpoint = $true },
    [pscustomobject]@{ mode = "intro-arena-frame308"; expected_goal = 16; expected_jumbotron = $null; checkpoint = $true },
    [pscustomobject]@{ mode = "intro-arena-frame539"; expected_goal = 16; expected_jumbotron = $null; checkpoint = $true }
)
$RomOnlyArenaRenderModes = @($RomOnlyArenaRenderCases | ForEach-Object { $_.mode })
$LowerBandGeometryCases = @(
    [pscustomobject]@{ frame = 308; scroll = "50"; motion = "6A"; origin_y = 458; clip_y = 460 },
    [pscustomobject]@{ frame = 324; scroll = "58"; motion = "5A"; origin_y = 426; clip_y = 428 },
    [pscustomobject]@{ frame = 340; scroll = "60"; motion = "4A"; origin_y = 394; clip_y = 396 },
    [pscustomobject]@{ frame = 348; scroll = "64"; motion = "42"; origin_y = 378; clip_y = 380 }
)
$MalformedCleanArenaModes = @(
    "intro-arena-clean-frame",
    "intro-arena-clean-frame-1",
    "intro-arena-clean-frame1junk",
    "intro-arena-clean-frame4294967296"
)
$PostArenaRenderCases = @(
    [pscustomobject]@{ mode = "intro-ready-clean-frame0"; state = "intro-ready-state frame=0 palette=0 mask=0 black=0 handoff=0"; visual = "black" },
    [pscustomobject]@{ mode = "intro-ready-clean-frame28"; state = "intro-ready-state frame=28 palette=4 mask=2 black=0 handoff=0"; visual = "ready" },
    [pscustomobject]@{ mode = "intro-ready-clean-frame32"; state = "intro-ready-state frame=32 palette=4 mask=4 black=0 handoff=0"; visual = "ready" },
    [pscustomobject]@{ mode = "intro-ready-clean-frame35"; state = "intro-ready-state frame=35 palette=4 mask=5 black=0 handoff=0"; visual = "ready" },
    [pscustomobject]@{ mode = "intro-ready-clean-frame40"; state = "intro-ready-state frame=40 palette=4 mask=8 black=0 handoff=0"; visual = "ready" },
    [pscustomobject]@{ mode = "intro-ready-clean-frame56"; state = "intro-ready-state frame=56 palette=4 mask=11 black=1 handoff=0"; visual = "black" },
    [pscustomobject]@{ mode = "intro-warriors-clean-frame0"; state = "intro-warriors-state frame=0 phase=load palette=0 pan=0 wordmark=0 patches=0 black=0 handoff=0 next_screen=1B"; visual = "black" },
    [pscustomobject]@{ mode = "intro-warriors-clean-frame17"; state = "intro-warriors-state frame=17 phase=wordmark palette=3 pan=0 wordmark=1 patches=0 black=0 handoff=0 next_screen=1B"; visual = "warriors" },
    [pscustomobject]@{ mode = "intro-warriors-clean-frame24"; state = "intro-warriors-state frame=24 phase=wordmark palette=3 pan=0 wordmark=8 patches=0 black=0 handoff=0 next_screen=1B"; visual = "warriors" },
    [pscustomobject]@{ mode = "intro-warriors-clean-frame64"; state = "intro-warriors-state frame=64 phase=pan palette=3 pan=20 wordmark=8 patches=0 black=0 handoff=0 next_screen=1B"; visual = "warriors" },
    [pscustomobject]@{ mode = "intro-warriors-clean-frame160"; state = "intro-warriors-state frame=160 phase=hold palette=3 pan=25 wordmark=8 patches=0 black=0 handoff=0 next_screen=1B"; visual = "warriors" },
    [pscustomobject]@{ mode = "intro-warriors-clean-frame193"; state = "intro-warriors-state frame=193 phase=patch-one palette=3 pan=25 wordmark=8 patches=1 black=0 handoff=0 next_screen=1B"; visual = "warriors" },
    [pscustomobject]@{ mode = "intro-warriors-clean-frame200"; state = "intro-warriors-state frame=200 phase=patch-two palette=3 pan=25 wordmark=8 patches=2 black=0 handoff=0 next_screen=1B"; visual = "warriors" },
    [pscustomobject]@{ mode = "intro-warriors-clean-frame213"; state = "intro-warriors-state frame=213 phase=black palette=0 pan=0 wordmark=0 patches=0 black=1 handoff=0 next_screen=1B"; visual = "black" },
    [pscustomobject]@{ mode = "intro-warriors-clean-frame214"; state = "intro-warriors-state frame=214 phase=handoff palette=0 pan=0 wordmark=0 patches=0 black=1 handoff=1 next_screen=1B"; visual = "black" }
)

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
        "arena/intro/sprite-groups",
        "arena/intro/ready-screen",
        "arena/intro/warriors-transition"
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

    $PostArenaOutputs = New-Object System.Collections.Generic.List[object]
    $PostArenaPassed = $ListPassed
    $PreviousAssetPack = $env:TECMO_ASSETPACK
    $PreviousLooseArenaCapture = $env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE
    try {
        $env:TECMO_ASSETPACK = $AssetPackPath
        Remove-Item Env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE -ErrorAction SilentlyContinue
        foreach ($RenderCase in $PostArenaRenderCases) {
            $RenderPath = Join-Path $OutputDir "$($RenderCase.mode).png"
            if (Test-Path -LiteralPath $RenderPath) {
                Remove-Item -LiteralPath $RenderPath -Force
            }
            $RenderOutput = & $ExePath --root $ProjectRoot --render-test-mode $RenderCase.mode $RenderPath 2>&1
            $RenderExitCode = $LASTEXITCODE
            $RenderText = (@($RenderOutput) | ForEach-Object { [string]$_ }) -join "`n"
            $RenderCreated = Test-Path -LiteralPath $RenderPath
            $NativeSourceSeen = $RenderText -match "intro-post-render-source ready=1 warriors=1 chr=1 ready_schema=TRDY-1 warriors_schema=TWAR-1"
            $StateSeen = $RenderText.Contains([string]$RenderCase.state)
            $VisualSeen = $false
            if ($RenderCreated) {
                $Bitmap = [System.Drawing.Bitmap]::FromFile($RenderPath)
                try {
                    if ($RenderCase.visual -eq "black") {
                        $VisualSeen = Test-PixelRectColor $Bitmap 64 0 575 479 0 0 0
                    } elseif ($RenderCase.visual -eq "ready") {
                        $VisualSeen = Test-PixelRectHasNonBlack $Bitmap 192 240 447 319
                    } else {
                        $PlayersSeen = Test-PixelRectHasNonBlack $Bitmap 64 80 575 327
                        $WordmarkSeen = Test-PixelRectHasNonBlack $Bitmap 192 416 447 447
                        $VisualSeen = $PlayersSeen -and $WordmarkSeen
                    }
                } finally {
                    $Bitmap.Dispose()
                }
            }
            $ModePassed = $RenderExitCode -eq 0 -and $RenderCreated -and
                $NativeSourceSeen -and $StateSeen -and $VisualSeen
            if (!$ModePassed) {
                $PostArenaPassed = $false
            }
            $PostArenaOutputs.Add([pscustomobject]@{
                mode = $RenderCase.mode
                output = Get-RepoRelativePath $RenderPath
                passed = $ModePassed
                exit_code = $RenderExitCode
                native_source_seen = $NativeSourceSeen
                state_seen = $StateSeen
                visual_signature_seen = $VisualSeen
            })
        }
    } finally {
        $env:TECMO_ASSETPACK = $PreviousAssetPack
        $env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE = $PreviousLooseArenaCapture
    }
    if (!$PostArenaPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "intro-render-rom-only-post-arena"
        passed = $PostArenaPassed
        skipped = $false
        outputs = $PostArenaOutputs
        rom_only_asset_pack = $AssetPackRelative
        raw_output_persisted = $false
        coverage_status = "ready-and-warriors-native-checkpoints"
        error = if ($PostArenaPassed) { $null } else { "ROM-only READY/WARRIORS render or timing checkpoint failed" }
    })

    $LowerBandGeometryOutputs = New-Object System.Collections.Generic.List[object]
    $LowerBandGeometryPassed = $true
    $PreviousAssetPack = $env:TECMO_ASSETPACK
    $PreviousLooseArenaCapture = $env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE
    try {
        $env:TECMO_ASSETPACK = $AssetPackPath
        Remove-Item Env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE -ErrorAction SilentlyContinue
        foreach ($GeometryCase in $LowerBandGeometryCases) {
            $Mode = "intro-arena-clean-frame$($GeometryCase.frame)"
            $RenderPath = Join-Path $CheckpointOutputDir "$Mode.png"
            if (Test-Path -LiteralPath $RenderPath) {
                Remove-Item -LiteralPath $RenderPath -Force
            }
            $RenderOutput = & $ExePath `
                --root $ProjectRoot `
                --render-test-mode $Mode $RenderPath 2>&1
            $RenderExitCode = $LASTEXITCODE
            $RenderText = (@($RenderOutput) | ForEach-Object { [string]$_ }) -join "`n"
            $RenderCreated = Test-Path -LiteralPath $RenderPath
            $ExactLayerSeen = $RenderText -match
                "intro-arena-render-source kind=arena exact_layer=1 rendered=1 cells=1632 palette=16"
            $OriginDoesNotMatchBoundary = $false
            $BoundaryRed = $false
            $BoundaryBlack = $false

            if ($RenderCreated) {
                $Bitmap = [System.Drawing.Bitmap]::FromFile($RenderPath)
                try {
                    $OriginDoesNotMatchBoundary = !(Test-PixelRectColor `
                        $Bitmap 64 $GeometryCase.origin_y 191 ($GeometryCase.origin_y + 1) `
                        228 0 88)
                    $BoundaryRed = Test-PixelRectColor `
                        $Bitmap 64 $GeometryCase.clip_y 191 ($GeometryCase.clip_y + 1) `
                        228 0 88
                    $BoundaryBlack = Test-PixelRectColor `
                        $Bitmap 64 ($GeometryCase.clip_y + 2) 191 ($GeometryCase.clip_y + 3) `
                        0 0 0
                } finally {
                    $Bitmap.Dispose()
                }
            }
            $ModePassed = $RenderExitCode -eq 0 -and
                $RenderCreated -and
                $ExactLayerSeen -and
                $OriginDoesNotMatchBoundary -and
                $BoundaryRed -and
                $BoundaryBlack
            if (!$ModePassed) {
                $LowerBandGeometryPassed = $false
            }
            $LowerBandGeometryOutputs.Add([pscustomobject]@{
                frame = [int]$GeometryCase.frame
                mode = $Mode
                output = Get-RepoRelativePath $RenderPath
                passed = $ModePassed
                exit_code = $RenderExitCode
                scroll_0301_hex = $GeometryCase.scroll
                motion_counter_88_hex = $GeometryCase.motion
                expected_origin_output_y = [int]$GeometryCase.origin_y
                expected_clip_output_y = [int]$GeometryCase.clip_y
                origin_does_not_match_boundary = $OriginDoesNotMatchBoundary
                boundary_red_signature = $BoundaryRed
                boundary_black_signature = $BoundaryBlack
            })
        }
    } finally {
        $env:TECMO_ASSETPACK = $PreviousAssetPack
        $env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE = $PreviousLooseArenaCapture
    }
    if (!$LowerBandGeometryPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "intro-arena-dynamic-lower-band-geometry"
        passed = $LowerBandGeometryPassed
        skipped = $false
        outputs = $LowerBandGeometryOutputs
        raw_output_persisted = $false
        coverage_status = "clean-frame-irq-boundary-progression"
        error = if ($LowerBandGeometryPassed) { $null } else { "lower-band IRQ origin progression changed" }
    })

    $MalformedCleanModeOutputs = New-Object System.Collections.Generic.List[object]
    $MalformedCleanModesPassed = $true
    $PreviousAssetPack = $env:TECMO_ASSETPACK
    $PreviousLooseArenaCapture = $env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE
    try {
        $env:TECMO_ASSETPACK = $AssetPackPath
        Remove-Item Env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE -ErrorAction SilentlyContinue
        for ($CaseIndex = 0; $CaseIndex -lt $MalformedCleanArenaModes.Count; ++$CaseIndex) {
            $Mode = $MalformedCleanArenaModes[$CaseIndex]
            $RenderPath = Join-Path $OutputDir "malformed-clean-arena-mode-$CaseIndex.png"
            if (Test-Path -LiteralPath $RenderPath) {
                Remove-Item -LiteralPath $RenderPath -Force
            }
            $RenderOutput = & $ExePath `
                --root $ProjectRoot `
                --render-test-mode $Mode $RenderPath 2>&1
            $RenderExitCode = $LASTEXITCODE
            $RenderText = (@($RenderOutput) | ForEach-Object { [string]$_ }) -join "`n"
            $Rejected = $RenderExitCode -eq 1 -and
                !(Test-Path -LiteralPath $RenderPath) -and
                $RenderText -match "Unsupported render-test mode: $([regex]::Escape($Mode))"
            if (!$Rejected) {
                $MalformedCleanModesPassed = $false
            }
            $MalformedCleanModeOutputs.Add([pscustomobject]@{
                mode = $Mode
                passed = $Rejected
                exit_code = $RenderExitCode
                png_created = Test-Path -LiteralPath $RenderPath
            })
        }
    } finally {
        $env:TECMO_ASSETPACK = $PreviousAssetPack
        $env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE = $PreviousLooseArenaCapture
    }
    if (!$MalformedCleanModesPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "intro-arena-clean-frame-mode-validation"
        passed = $MalformedCleanModesPassed
        skipped = $false
        cases = $MalformedCleanModeOutputs
        raw_output_persisted = $false
        coverage_status = "strict-decimal-suffix"
        error = if ($MalformedCleanModesPassed) { $null } else { "malformed clean arena frame mode was accepted" }
    })

    $CleanFinalMode = "intro-arena-clean-frame539"
    $CleanFinalPath = Join-Path $CheckpointOutputDir "$CleanFinalMode.png"
    $CleanFinalPassed = $false
    $CleanFinalExitCode = -1
    $CleanFinalCreated = $false
    $CleanFinalSourceSeen = $false
    $CleanFinalGoalCountSeen = $false
    $PostGrayLeft = $false
    $PostWhiteCenter = $false
    $PostGrayRight = $false
    $OpeningBlack = $false
    $OpeningCreamLeft = $false
    $OpeningCreamRight = $false
    $CreamCap = $false
    $PreviousAssetPack = $env:TECMO_ASSETPACK
    $PreviousLooseArenaCapture = $env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE
    try {
        if (Test-Path -LiteralPath $CleanFinalPath) {
            Remove-Item -LiteralPath $CleanFinalPath -Force
        }
        $env:TECMO_ASSETPACK = $AssetPackPath
        Remove-Item Env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE -ErrorAction SilentlyContinue
        $CleanFinalOutput = & $ExePath `
            --root $ProjectRoot `
            --render-test-mode $CleanFinalMode $CleanFinalPath 2>&1
        $CleanFinalExitCode = $LASTEXITCODE
        $CleanFinalText = (@($CleanFinalOutput) | ForEach-Object { [string]$_ }) -join "`n"
        $CleanFinalCreated = Test-Path -LiteralPath $CleanFinalPath
        $CleanFinalSourceSeen = $CleanFinalText -match
            "intro-arena-render-source kind=arena exact_layer=1 rendered=1 cells=1632 palette=16"
        $CleanFinalGoalCountSeen = $CleanFinalText -match
            "visible_jumbotron=0 visible_goal=16"

        if ($CleanFinalCreated) {
            $Bitmap = [System.Drawing.Bitmap]::FromFile($CleanFinalPath)
            try {
                # Final ROM registration: post ends at 429, the opening starts
                # at 430, and the lower-band cream cap starts at 432.
                $PostGrayLeft = Test-PixelRectColor $Bitmap 430 428 431 429 124 124 124
                $PostWhiteCenter = Test-PixelRectColor $Bitmap 432 428 433 429 252 252 252
                $PostGrayRight = Test-PixelRectColor $Bitmap 434 428 435 429 124 124 124
                $OpeningBlack = Test-PixelRectColor $Bitmap 424 430 441 431 0 0 0
                $OpeningCreamLeft = Test-PixelRectColor $Bitmap 420 430 423 431 252 224 168
                $OpeningCreamRight = Test-PixelRectColor $Bitmap 442 430 445 431 252 224 168
                $CreamCap = Test-PixelRectColor $Bitmap 424 432 441 433 252 224 168
            } finally {
                $Bitmap.Dispose()
            }
        }
        $CleanFinalPassed = $CleanFinalExitCode -eq 0 -and
            $CleanFinalCreated -and
            $CleanFinalSourceSeen -and
            $CleanFinalGoalCountSeen -and
            $PostGrayLeft -and
            $PostWhiteCenter -and
            $PostGrayRight -and
            $OpeningBlack -and
            $OpeningCreamLeft -and
            $OpeningCreamRight -and
            $CreamCap
    } finally {
        $env:TECMO_ASSETPACK = $PreviousAssetPack
        $env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE = $PreviousLooseArenaCapture
    }
    if (!$CleanFinalPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "intro-arena-rom-exact-final-registration"
        passed = $CleanFinalPassed
        skipped = $false
        mode = $CleanFinalMode
        output = Get-RepoRelativePath $CleanFinalPath
        exit_code = $CleanFinalExitCode
        exact_layer_seen = $CleanFinalSourceSeen
        final_goal_count_seen = $CleanFinalGoalCountSeen
        post_gray_left = $PostGrayLeft
        post_white_center = $PostWhiteCenter
        post_gray_right = $PostGrayRight
        opening_black = $OpeningBlack
        opening_cream_left = $OpeningCreamLeft
        opening_cream_right = $OpeningCreamRight
        cream_cap = $CreamCap
        raw_output_persisted = $false
        coverage_status = "clean-frame-local-relational-pixels"
        error = if ($CleanFinalPassed) { $null } else { "frame 539 post/opening/cap registration changed" }
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
            status = "arena-rom-exact-d861-and-irq-band-checkpoints"
            render_modes = $RomOnlyArenaRenderModes
            clean_final_mode = $CleanFinalMode
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
