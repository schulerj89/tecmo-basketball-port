param(
    [string]$ProjectRoot,
    [string]$DecompRoot,
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

function Find-LocalDecompRoot {
    if ($DecompRoot -and (Test-Path $DecompRoot)) {
        return (Resolve-Path $DecompRoot).Path
    }
    if ($env:TECMO_DECOMP_ROOT -and (Test-Path $env:TECMO_DECOMP_ROOT)) {
        return (Resolve-Path $env:TECMO_DECOMP_ROOT).Path
    }

    $ProjectsRoot = Split-Path -Parent $ProjectRoot
    $Candidates = @(
        (Join-Path $ProjectsRoot "disassem\tecmo-basketball-decompilation"),
        (Join-Path $ProjectsRoot "tecmo-basketball-decompilation")
    )
    foreach ($Candidate in $Candidates) {
        if (Test-Path $Candidate) {
            return (Resolve-Path $Candidate).Path
        }
    }

    return $null
}

function Find-ReferenceRom {
    param([string]$LocalDecompRoot)

    if ($RomPath) {
        if (!(Test-Path $RomPath)) {
            throw "RomPath does not exist."
        }
        return (Resolve-Path $RomPath).Path
    }
    if ($env:TECMO_ROM_PATH -and (Test-Path $env:TECMO_ROM_PATH)) {
        return (Resolve-Path $env:TECMO_ROM_PATH).Path
    }
    if (!$LocalDecompRoot) {
        return $null
    }

    $Candidates = @(
        (Join-Path $LocalDecompRoot "build\out\tecmo_basketball_disassem.nes"),
        (Join-Path $LocalDecompRoot "build\out\tecmo_basketball_split_compile.nes"),
        (Join-Path $LocalDecompRoot "build\out\tecmo_basketball_split_bank01.nes"),
        (Join-Path $LocalDecompRoot "build\split_compile\build\out\tecmo_basketball_split_compile.nes")
    )
    foreach ($Candidate in $Candidates) {
        if (Test-Path $Candidate) {
            return (Resolve-Path $Candidate).Path
        }
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

function Move-PathAside {
    param(
        [string]$Path,
        [string]$Suffix
    )

    if (!(Test-Path -LiteralPath $Path)) {
        return $null
    }

    $FullPath = [System.IO.Path]::GetFullPath($Path)
    $AsidePath = "$FullPath.$Suffix"
    $Index = 0
    while (Test-Path -LiteralPath $AsidePath) {
        ++$Index
        $AsidePath = "$FullPath.$Suffix.$Index"
    }
    Move-Item -LiteralPath $FullPath -Destination $AsidePath
    return [pscustomobject]@{
        original = $FullPath
        aside = $AsidePath
    }
}

function Restore-MovedPath {
    param([object]$MovedPath)

    if (!$MovedPath) {
        return
    }
    if ((Test-Path -LiteralPath $MovedPath.aside) -and !(Test-Path -LiteralPath $MovedPath.original)) {
        Move-Item -LiteralPath $MovedPath.aside -Destination $MovedPath.original
    }
}

function Get-PngInspection {
    param(
        [string]$Path
    )

    $Item = Get-Item $Path
    $Source = [System.Drawing.Image]::FromFile($Path)
    $Width = $Source.Width
    $Height = $Source.Height
    $Bitmap = $null
    $Graphics = $null
    $Data = $null
    try {
        $Bitmap = New-Object System.Drawing.Bitmap -ArgumentList $Width, $Height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $Graphics = [System.Drawing.Graphics]::FromImage($Bitmap)
        $Graphics.DrawImage($Source, 0, 0, $Width, $Height)

        $Rect = New-Object System.Drawing.Rectangle 0, 0, $Bitmap.Width, $Bitmap.Height
        $Data = $Bitmap.LockBits($Rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $Stride = [Math]::Abs($Data.Stride)
        $Bytes = New-Object byte[] ($Stride * $Bitmap.Height)
        [System.Runtime.InteropServices.Marshal]::Copy($Data.Scan0, $Bytes, 0, $Bytes.Length)

        $NonzeroRgbaPixels = 0
        $VisibleNonblackPixels = 0
        $MainAreaVisiblePixels = 0
        $TopDebugPixels = 0
        $BottomDebugPixels = 0
        $CenterWarningBandPixels = 0
        $UniqueColors = @{}
        $MinX = $Bitmap.Width
        $MinY = $Bitmap.Height
        $MaxX = -1
        $MaxY = -1
        $MainMinY = $Bitmap.Height
        $MainMaxY = -1
        for ($Y = 0; $Y -lt $Bitmap.Height; ++$Y) {
            $Row = $Y * $Stride
            for ($X = 0; $X -lt $Bitmap.Width; ++$X) {
                $Offset = $Row + ($X * 4)
                $Blue = [int]$Bytes[$Offset]
                $Green = [int]$Bytes[$Offset + 1]
                $Red = [int]$Bytes[$Offset + 2]
                $Alpha = [int]$Bytes[$Offset + 3]
                if (($Red -bor $Green -bor $Blue -bor $Alpha) -ne 0) {
                    ++$NonzeroRgbaPixels
                }
                if ($Alpha -ne 0 -and (($Red -bor $Green -bor $Blue) -ne 0)) {
                    ++$VisibleNonblackPixels
                    if ($X -lt $MinX) {
                        $MinX = $X
                    }
                    if ($Y -lt $MinY) {
                        $MinY = $Y
                    }
                    if ($X -gt $MaxX) {
                        $MaxX = $X
                    }
                    if ($Y -gt $MaxY) {
                        $MaxY = $Y
                    }
                    if ($Y -lt 24) {
                        ++$TopDebugPixels
                    } elseif ($Y -ge 456) {
                        ++$BottomDebugPixels
                    } else {
                        ++$MainAreaVisiblePixels
                        if ($Y -lt $MainMinY) {
                            $MainMinY = $Y
                        }
                        if ($Y -gt $MainMaxY) {
                            $MainMaxY = $Y
                        }
                    }
                    if ($X -ge 64 -and $X -lt 576 -and $Y -ge 150 -and $Y -lt 285) {
                        ++$CenterWarningBandPixels
                    }
                    if ($UniqueColors.Count -lt 256) {
                        $ColorKey = "{0:X2}{1:X2}{2:X2}{3:X2}" -f $Alpha, $Red, $Green, $Blue
                        $UniqueColors[$ColorKey] = $true
                    }
                }
            }
        }
    } finally {
        if ($Data -ne $null -and $Bitmap -ne $null) {
            $Bitmap.UnlockBits($Data)
        }
        if ($Graphics -ne $null) {
            $Graphics.Dispose()
        }
        if ($Bitmap -ne $null) {
            $Bitmap.Dispose()
        }
        $Source.Dispose()
    }

    return [pscustomobject]@{
        width = $Width
        height = $Height
        bytes = $Item.Length
        nonzero_rgba_pixels = $NonzeroRgbaPixels
        visible_nonblack_pixels = $VisibleNonblackPixels
        unique_color_sample_count = $UniqueColors.Count
        visible_min_x = if ($MaxX -ge 0) { $MinX } else { $null }
        visible_min_y = if ($MaxY -ge 0) { $MinY } else { $null }
        visible_max_x = if ($MaxX -ge 0) { $MaxX } else { $null }
        visible_max_y = if ($MaxY -ge 0) { $MaxY } else { $null }
        main_area_visible_pixels = $MainAreaVisiblePixels
        main_content_height = if ($MainMaxY -ge 0) { $MainMaxY - $MainMinY + 1 } else { 0 }
        top_debug_pixels = $TopDebugPixels
        bottom_debug_pixels = $BottomDebugPixels
        center_warning_band_pixels = $CenterWarningBandPixels
    }
}

function Get-InspectionValidation {
    param(
        [object]$Test,
        [object]$Inspection,
        [object]$CaptureStatus,
        [object]$CaptureSource
    )

    $MinVisiblePixels = [int]$Test.min_visible_nonblack_pixels
    $CaptureLoaded = !$Test.requires_capture
    $CaptureStatusPresent = $false
    if ($Test.requires_capture -and $CaptureStatus) {
        $CaptureStatusPresent = $true
        $CaptureLoaded = [bool]$CaptureStatus.available -and
            [int]$CaptureStatus.nt -gt 0 -and
            [int]$CaptureStatus.pal -gt 0
    }
    $CaptureSourceAssetPack = !$Test.requires_capture
    if ($Test.requires_capture -and $CaptureSource) {
        $CaptureSourceAssetPack = [bool]$CaptureSource.assetpack
    }
    $AllowLowColorCapture = [bool]$Test.allow_low_color_capture -and $CaptureLoaded
    $LooksLikeMissingCaptureWarning = [bool]$Test.requires_capture -and
        !$AllowLowColorCapture -and
        $Inspection.unique_color_sample_count -le 4 -and
        $Inspection.center_warning_band_pixels -ge 1000 -and
        $Inspection.main_content_height -le 96
    $Passed = $Inspection.width -eq 640 -and
        $Inspection.height -eq 480 -and
        $Inspection.bytes -gt 0 -and
        $Inspection.nonzero_rgba_pixels -gt 0 -and
        $Inspection.visible_nonblack_pixels -ge $MinVisiblePixels -and
        $CaptureLoaded -and
        $CaptureSourceAssetPack -and
        !$LooksLikeMissingCaptureWarning

    $Error = $null
    if (!$Passed) {
        if ($Test.requires_capture -and !$CaptureStatusPresent) {
            $Error = "renderer did not report capture status"
        } elseif ($Test.requires_capture -and !$CaptureLoaded) {
            $Error = "required intro capture was not loaded"
        } elseif ($Test.requires_capture -and !$CaptureSourceAssetPack) {
            $Error = "required intro capture was not loaded from the test asset pack"
        } elseif ($LooksLikeMissingCaptureWarning) {
            $Error = "PNG resembles a missing-capture diagnostic frame"
        } else {
            $Error = "PNG failed dimension, visibility, or capture-strength assertions"
        }
    }

    return [pscustomobject]@{
        passed = $Passed
        capture_status_present = $CaptureStatusPresent
        capture_loaded = $CaptureLoaded
        capture_source_assetpack = $CaptureSourceAssetPack
        diagnostic_warning_like = $LooksLikeMissingCaptureWarning
        error = $Error
    }
}

function Get-IntroCaptureStatus {
    param(
        [object[]]$RenderOutput
    )

    foreach ($Line in @($RenderOutput)) {
        $Text = [string]$Line
        if ($Text -match '^intro-capture-status kind=(?<kind>[a-z-]+) available=(?<available>[01]) nt=(?<nt>\d+) attr=(?<attr>\d+) pal=(?<pal>\d+) oam=(?<oam>\d+)$') {
            return [pscustomobject]@{
                kind = $Matches.kind
                available = $Matches.available -eq "1"
                nt = [int]$Matches.nt
                attr = [int]$Matches.attr
                pal = [int]$Matches.pal
                oam = [int]$Matches.oam
            }
        }
    }
    return $null
}

function Get-IntroCaptureSource {
    param(
        [object[]]$RenderOutput
    )

    foreach ($Line in @($RenderOutput)) {
        $Text = [string]$Line
        if ($Text -match '^intro-capture-source kind=(?<kind>[a-z-]+) assetpack=(?<assetpack>[01]) entry=(?<entry>.+)$') {
            return [pscustomobject]@{
                kind = $Matches.kind
                assetpack = $Matches.assetpack -eq "1"
                entry = $Matches.entry
            }
        }
    }
    return $null
}

$ExePath = Join-Path $BuildDir "tecmo_port.exe"
$LocalDecompRoot = Find-LocalDecompRoot

if (!$LocalDecompRoot) {
    throw "Could not find a local decomp root. Pass -DecompRoot or set TECMO_DECOMP_ROOT."
}
$ReferenceRom = Find-ReferenceRom $LocalDecompRoot
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
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $AssetPackPath) | Out-Null
Add-Type -AssemblyName System.Drawing

$Tests = @(
    [pscustomobject]@{
        id = "intro-license"
        mode = "intro-license"
        output = "intro_license.png"
        optional = $false
        requires_capture = $false
        min_visible_nonblack_pixels = 12000
    },
    [pscustomobject]@{
        id = "intro-arena-frame320"
        mode = "intro-arena-frame320"
        output = "intro_arena_frame320.png"
        optional = $false
        requires_capture = $true
        min_visible_nonblack_pixels = 3000
    },
    [pscustomobject]@{
        id = "intro-ready-frame35"
        mode = "intro-ready-frame35"
        output = "intro_ready_frame35.png"
        optional = $false
        requires_capture = $true
        min_visible_nonblack_pixels = 1000
        allow_low_color_capture = $true
    },
    [pscustomobject]@{
        id = "intro-warriors-frame74"
        mode = "intro-warriors-frame74"
        output = "intro_warriors_frame74.png"
        optional = $false
        requires_capture = $true
        min_visible_nonblack_pixels = 2500
    },
    [pscustomobject]@{
        id = "play-step7"
        mode = "play-step7"
        output = "play_step7.png"
        optional = $false
        requires_capture = $false
        min_visible_nonblack_pixels = 12000
    },
    [pscustomobject]@{
        id = "play-step8"
        mode = "play-step8"
        output = "play_step8.png"
        optional = $false
        requires_capture = $true
        min_visible_nonblack_pixels = 3000
    },
    [pscustomobject]@{
        id = "play-step9"
        mode = "play-step9"
        output = "play_step9.png"
        optional = $false
        requires_capture = $true
        min_visible_nonblack_pixels = 1000
        allow_low_color_capture = $true
    },
    [pscustomobject]@{
        id = "play-step10"
        mode = "play-step10"
        output = "play_step10.png"
        optional = $false
        requires_capture = $true
        min_visible_nonblack_pixels = 2500
    }
)

$Results = New-Object System.Collections.Generic.List[object]
$Failures = 0
$Skipped = 0
$PreviousAssetPack = $env:TECMO_ASSETPACK
$MovedFallbacks = New-Object System.Collections.Generic.List[object]
$IsolationSuffix = "intro-sequence-test-isolated"
$AssetPackRelative = Get-RepoRelativePath $AssetPackPath

try {
    if (Test-Path -LiteralPath $AssetPackPath) {
        Remove-Item -LiteralPath $AssetPackPath -Force
    }

    Write-Host "Building intro sequence test asset pack -> $AssetPackRelative"
    $PackBuildOutput = & $ExePath --root $LocalDecompRoot --build-assetpack $ReferenceRom $AssetPackPath 2>&1
    $PackBuildExitCode = $LASTEXITCODE
    $PackCreated = Test-Path -LiteralPath $AssetPackPath
    $PackBuildOutputText = (@($PackBuildOutput) | ForEach-Object { [string]$_ }) -join "`n"
    $PackReportedImports = $null
    if ($PackBuildOutputText -match "imported decomp-derived entries from .+ and ([0-9]+) intro captures") {
        $PackReportedImports = [int]$Matches[1]
    } elseif ($PackBuildOutputText -match "imported ([0-9]+) intro captures") {
        $PackReportedImports = [int]$Matches[1]
    }
    $PackBuildPassed = $PackBuildExitCode -eq 0 -and $PackCreated
    if (!$PackBuildPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "intro-test-assetpack-build"
        command = ".\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --build-assetpack <LOCAL_ROM.nes> $($AssetPackRelative.Replace('/', '\'))"
        output = $AssetPackRelative
        passed = $PackBuildPassed
        skipped = $false
        exit_code = $PackBuildExitCode
        created = $PackCreated
        reported_intro_captures_imported = $PackReportedImports
        raw_output_persisted = $false
        error = if ($PackBuildPassed) { $null } else { "fresh test asset pack was not built" }
    })

    $ListOutput = & $ExePath --assetpack-list $AssetPackPath 2>&1
    $ListExitCode = $LASTEXITCODE
    $ListText = (@($ListOutput) | ForEach-Object { [string]$_ }) -join "`n"
    $RequiredCaptureEntries = @("intro/arena/capture.ndjson", "intro/post-arena/capture.ndjson")
    $MissingCaptureEntries = @($RequiredCaptureEntries | Where-Object { $ListText -notmatch [regex]::Escape($_) })
    $ListPassed = $ListExitCode -eq 0 -and $MissingCaptureEntries.Count -eq 0
    if (!$ListPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "intro-test-assetpack-capture-entries"
        output = $AssetPackRelative
        passed = $ListPassed
        skipped = $false
        exit_code = $ListExitCode
        required_entries = $RequiredCaptureEntries
        missing_entries = $MissingCaptureEntries
        raw_output_persisted = $false
        error = if ($ListPassed) { $null } else { "fresh test asset pack is missing required capture entries" }
    })

    $FallbackCandidates = @(
        (Join-Path $BuildDir "intro_arena_capture.ndjson"),
        (Join-Path $ProjectRoot "intro_arena_capture.ndjson"),
        (Join-Path $BuildDir "emu_intro_memory_watch.ndjson"),
        (Join-Path $ProjectRoot "emu_intro_memory_watch.ndjson"),
        (Join-Path $BuildDir "emu_intro_arena_irq_watch.ndjson"),
        (Join-Path $ProjectRoot "emu_intro_arena_irq_watch.ndjson"),
        (Join-Path $BuildDir "tecmo.assetpack"),
        (Join-Path $LocalDecompRoot "build\tecmo.assetpack")
    )
    foreach ($Candidate in $FallbackCandidates) {
        $FullCandidate = [System.IO.Path]::GetFullPath($Candidate)
        if ($FullCandidate.Equals($AssetPackPath, [System.StringComparison]::OrdinalIgnoreCase)) {
            continue
        }
        $Moved = Move-PathAside -Path $FullCandidate -Suffix $IsolationSuffix
        if ($Moved) {
            [void]$MovedFallbacks.Add($Moved)
        }
    }
    $Results.Add([pscustomobject]@{
        id = "intro-capture-fallbacks-isolated"
        passed = $true
        skipped = $false
        moved_count = $MovedFallbacks.Count
        asset_pack_env = $AssetPackRelative
        raw_output_persisted = $false
    })

    $env:TECMO_ASSETPACK = $AssetPackPath

    foreach ($Test in $Tests) {
        $OutputPath = [System.IO.Path]::GetFullPath((Join-Path $OutputDir $Test.output))
        if (!(Test-PathUnder $OutputPath $BuildDir)) {
            throw "Intro sequence output for '$($Test.id)' must stay under build\."
        }
        if (Test-Path $OutputPath) {
            Remove-Item -LiteralPath $OutputPath -Force
        }

        $OutputRelative = Get-RepoRelativePath $OutputPath
        $Command = ".\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode $($Test.mode) $($OutputRelative.Replace('/', '\'))"
        Write-Host "Rendering $($Test.id) ($($Test.mode)) -> $OutputRelative"

        $RenderOutput = & $ExePath --root $LocalDecompRoot --render-test-mode $Test.mode $OutputPath 2>&1
        $ExitCode = $LASTEXITCODE
        $Unsupported = $false
        foreach ($Line in @($RenderOutput)) {
            if ([string]$Line -match '^Unsupported render-test mode:') {
                $Unsupported = $true
            }
        }

        if ($Unsupported -and $Test.optional) {
            ++$Skipped
            $Results.Add([pscustomobject]@{
                id = $Test.id
                mode = $Test.mode
                command = $Command
                output = $OutputRelative
                optional = [bool]$Test.optional
                supported = $false
                passed = $true
                skipped = $true
                raw_output_persisted = $false
                error = $null
            })
            continue
        }

        if ($ExitCode -ne 0 -or $Unsupported) {
            ++$Failures
            $Results.Add([pscustomobject]@{
                id = $Test.id
                mode = $Test.mode
                command = $Command
                output = $OutputRelative
                optional = [bool]$Test.optional
                supported = !$Unsupported
                passed = $false
                skipped = $false
                exit_code = $ExitCode
                raw_output_persisted = $false
                error = if ($Unsupported) { "renderer mode is unsupported" } else { "renderer exited with $ExitCode; rerun the sanitized command locally for stdout/stderr" }
            })
            continue
        }

        if (!(Test-Path $OutputPath)) {
            ++$Failures
            $Results.Add([pscustomobject]@{
                id = $Test.id
                mode = $Test.mode
                command = $Command
                output = $OutputRelative
                optional = [bool]$Test.optional
                supported = $true
                passed = $false
                skipped = $false
                exit_code = $ExitCode
                raw_output_persisted = $false
                error = "PNG was not created"
            })
            continue
        }

        $Inspection = $null
        $InspectionErrorType = $null
        try {
            $Inspection = Get-PngInspection $OutputPath
        } catch {
            $InspectionErrorType = Get-SafeExceptionName $_
        }

        if ($InspectionErrorType) {
            ++$Failures
            $Results.Add([pscustomobject]@{
                id = $Test.id
                mode = $Test.mode
                command = $Command
                output = $OutputRelative
                optional = [bool]$Test.optional
                supported = $true
                passed = $false
                skipped = $false
                exit_code = $ExitCode
                raw_output_persisted = $false
                error = "PNG inspection failed"
                error_type = $InspectionErrorType
            })
            continue
        }

        $CaptureStatus = Get-IntroCaptureStatus -RenderOutput @($RenderOutput)
        $CaptureSource = Get-IntroCaptureSource -RenderOutput @($RenderOutput)
        $Validation = Get-InspectionValidation -Test $Test -Inspection $Inspection -CaptureStatus $CaptureStatus -CaptureSource $CaptureSource
        if (!$Validation.passed) {
            ++$Failures
        }

        $Results.Add([pscustomobject]@{
            id = $Test.id
            mode = $Test.mode
            command = $Command
            output = $OutputRelative
            optional = [bool]$Test.optional
            supported = $true
            requires_capture = [bool]$Test.requires_capture
            passed = $Validation.passed
            skipped = $false
            exit_code = $ExitCode
            width = $Inspection.width
            height = $Inspection.height
            bytes = $Inspection.bytes
            nonzero_rgba_pixels = $Inspection.nonzero_rgba_pixels
            visible_nonblack_pixels = $Inspection.visible_nonblack_pixels
            minimum_visible_nonblack_pixels = [int]$Test.min_visible_nonblack_pixels
            unique_color_sample_count = $Inspection.unique_color_sample_count
            main_area_visible_pixels = $Inspection.main_area_visible_pixels
            main_content_height = $Inspection.main_content_height
            top_debug_pixels = $Inspection.top_debug_pixels
            bottom_debug_pixels = $Inspection.bottom_debug_pixels
            center_warning_band_pixels = $Inspection.center_warning_band_pixels
            capture_status_present = $Validation.capture_status_present
            capture_loaded = $Validation.capture_loaded
            capture_source_assetpack = $Validation.capture_source_assetpack
            capture_kind = if ($CaptureStatus) { $CaptureStatus.kind } else { $null }
            capture_source_kind = if ($CaptureSource) { $CaptureSource.kind } else { $null }
            capture_source_entry = if ($CaptureSource) { $CaptureSource.entry } else { $null }
            capture_nt = if ($CaptureStatus) { $CaptureStatus.nt } else { $null }
            capture_attr = if ($CaptureStatus) { $CaptureStatus.attr } else { $null }
            capture_pal = if ($CaptureStatus) { $CaptureStatus.pal } else { $null }
            capture_oam = if ($CaptureStatus) { $CaptureStatus.oam } else { $null }
            diagnostic_warning_like = $Validation.diagnostic_warning_like
            raw_output_persisted = $false
            error = $Validation.error
        })
    }
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
    if ($null -eq $PreviousAssetPack) {
        Remove-Item Env:\TECMO_ASSETPACK -ErrorAction SilentlyContinue
    } else {
        $env:TECMO_ASSETPACK = $PreviousAssetPack
    }
    for ($Index = $MovedFallbacks.Count - 1; $Index -ge 0; $Index--) {
        Restore-MovedPath $MovedFallbacks[$Index]
    }

    $Report = [pscustomobject]@{
        schema_version = 1
        generated_by = "tools/Run-IntroSequenceTests.ps1"
        generated_at_utc = [DateTime]::UtcNow.ToString("o")
        data_policy = "Sanitized intro-sequence smoke report only; raw stdout/stderr, local paths, ROM bytes, ASM, CHR bytes, trace payloads, and screenshots are not embedded."
        passed = $Failures -eq 0
        decomp_root_used = "<local>"
        build_requested = [bool]$Build
        output_directory = (Get-RepoRelativePath $OutputDir)
        asset_pack = [pscustomobject]@{
            output = $AssetPackRelative
            env_pack_set_for_renders = $true
            loose_fallbacks_isolated = $MovedFallbacks.Count
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

$Results | Format-Table id, mode, passed, skipped, diagnostic_warning_like, width, height, visible_nonblack_pixels, output -AutoSize
Write-Host "Wrote intro sequence test report: $(Get-RepoRelativePath $ReportPath)"

if ($Failures -ne 0) {
    throw "$Failures intro sequence smoke test(s) failed."
}
Write-Host "All intro sequence smoke tests passed."
