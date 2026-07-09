param(
    [string]$ProjectRoot,
    [string]$DecompRoot,
    [switch]$Build,
    [string]$ReportPath
)

$ErrorActionPreference = "Stop"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path $ProjectRoot).Path

$BuildDir = Join-Path $ProjectRoot "build"
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
        $UniqueColors = @{}
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
                }
                if ($UniqueColors.Count -lt 256) {
                    $ColorKey = "{0:X2}{1:X2}{2:X2}{3:X2}" -f $Alpha, $Red, $Green, $Blue
                    $UniqueColors[$ColorKey] = $true
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
    }
}

$ExePath = Join-Path $BuildDir "tecmo_port.exe"
$OutputDir = Join-Path $BuildDir "intro_sequence"
$LocalDecompRoot = Find-LocalDecompRoot

if (!$LocalDecompRoot) {
    throw "Could not find a local decomp root. Pass -DecompRoot or set TECMO_DECOMP_ROOT."
}

if (!(Test-PathUnder $ReportPath $BuildDir)) {
    throw "Intro sequence report must stay under build\."
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
Add-Type -AssemblyName System.Drawing

$Tests = @(
    [pscustomobject]@{
        id = "intro-license"
        mode = "intro-license"
        output = "intro_license.png"
        optional = $false
    },
    [pscustomobject]@{
        id = "intro-arena-frame320"
        mode = "intro-arena-frame320"
        output = "intro_arena_frame320.png"
        optional = $false
    },
    [pscustomobject]@{
        id = "intro-ready-frame35"
        mode = "intro-ready-frame35"
        output = "intro_ready_frame35.png"
        optional = $false
    },
    [pscustomobject]@{
        id = "intro-warriors-frame74"
        mode = "intro-warriors-frame74"
        output = "intro_warriors_frame74.png"
        optional = $false
    },
    [pscustomobject]@{
        id = "play-step7"
        mode = "play-step7"
        output = "play_step7.png"
        optional = $true
    },
    [pscustomobject]@{
        id = "play-step8"
        mode = "play-step8"
        output = "play_step8.png"
        optional = $true
    }
)

$Results = New-Object System.Collections.Generic.List[object]
$Failures = 0
$Skipped = 0

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

    if ($ExitCode -ne 0) {
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
            error = "renderer exited with $ExitCode; rerun the sanitized command locally for stdout/stderr"
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

    $Inspection = Get-PngInspection $OutputPath
    $Passed = $Inspection.width -eq 640 -and
              $Inspection.height -eq 480 -and
              $Inspection.bytes -gt 0 -and
              $Inspection.nonzero_rgba_pixels -gt 0 -and
              $Inspection.visible_nonblack_pixels -gt 0
    if (!$Passed) {
        ++$Failures
    }

    $Results.Add([pscustomobject]@{
        id = $Test.id
        mode = $Test.mode
        command = $Command
        output = $OutputRelative
        optional = [bool]$Test.optional
        supported = $true
        passed = $Passed
        skipped = $false
        exit_code = $ExitCode
        width = $Inspection.width
        height = $Inspection.height
        bytes = $Inspection.bytes
        nonzero_rgba_pixels = $Inspection.nonzero_rgba_pixels
        visible_nonblack_pixels = $Inspection.visible_nonblack_pixels
        unique_color_sample_count = $Inspection.unique_color_sample_count
        raw_output_persisted = $false
        error = if ($Passed) { $null } else { "PNG failed dimension or nonzero pixel assertions" }
    })
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
$Report | ConvertTo-Json -Depth 6 | Set-Content -Path $ReportPath -Encoding ASCII

$Results | Format-Table id, mode, passed, skipped, width, height, visible_nonblack_pixels, output -AutoSize
Write-Host "Wrote intro sequence test report: $ReportPath"

if ($Failures -ne 0) {
    throw "$Failures intro sequence smoke test(s) failed."
}
Write-Host "All intro sequence smoke tests passed."
