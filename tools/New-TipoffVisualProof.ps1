param(
    [string]$ProjectRoot,
    [Parameter(Mandatory = $true)]
    [string]$RomPath,
    [string]$OutputRoot,
    [string]$FfmpegPath,
    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$ExpectedRomSha256 =
    "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4"
$ExpectedTptiFnv32 = "99ADFE3D"
$ExpectedTptiBytes = 5888
$ProofFirstFrame = 661
$ProofLastFrame = 695
$OutputWidth = 640
$OutputHeight = 480
$ActiveLeft = 64
$ActiveRight = 575
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent (Split-Path -Parent `
        $MyInvocation.MyCommand.Path)
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$RomPath = (Resolve-Path -LiteralPath $RomPath).Path
$ScriptPath = $MyInvocation.MyCommand.Path
$Executable = Join-Path $ProjectRoot "build\tecmo_port.exe"
$BuildScript = Join-Path $ProjectRoot "build.ps1"

function Invoke-GitText {
    param([string[]]$GitArguments)
    $Result = @(& git -C $ProjectRoot @GitArguments 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "git $($GitArguments -join ' ') failed: $($Result -join [Environment]::NewLine)"
    }
    return ($Result -join [Environment]::NewLine).Trim()
}

$Status = Invoke-GitText -GitArguments @(
    "status", "--porcelain", "--untracked-files=all")
if ($Status.Length -ne 0) {
    throw "Tip-off proof generation requires a clean worktree.`n$Status"
}
$Commit = Invoke-GitText -GitArguments @("rev-parse", "HEAD")
$ShortCommit = Invoke-GitText -GitArguments @("rev-parse", "--short=12", "HEAD")
$Branch = Invoke-GitText -GitArguments @("branch", "--show-current")

if (!$OutputRoot) {
    $OutputRoot = Join-Path $ProjectRoot `
        "build\proof\tipoff-visual-orientation-$ShortCommit"
} elseif (![IO.Path]::IsPathRooted($OutputRoot)) {
    $OutputRoot = Join-Path $ProjectRoot $OutputRoot
}
if (Test-Path -LiteralPath $OutputRoot) {
    $Existing = @(Get-ChildItem -LiteralPath $OutputRoot -Force)
    if ($Existing.Count -ne 0) {
        throw "Proof output already contains files: $OutputRoot"
    }
} else {
    New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null
}
$OutputRoot = (Resolve-Path -LiteralPath $OutputRoot).Path
$FramesRoot = Join-Path $OutputRoot "frames"
$VerifyRoot = Join-Path $OutputRoot "determinism-pass-2"
$LogsRoot = Join-Path $OutputRoot "logs"
New-Item -ItemType Directory -Force -Path `
    $FramesRoot, $VerifyRoot, $LogsRoot | Out-Null

if ((Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash -ne
    $ExpectedRomSha256) {
    throw "Tip-off proof requires the supported Rev1 ROM fingerprint."
}

$BuildLog = Join-Path $LogsRoot "build.log"
if (!$SkipBuild) {
    & $BuildScript *> $BuildLog
    if ($LASTEXITCODE -ne 0) {
        throw "Warning-clean build failed.`n$((Get-Content -LiteralPath $BuildLog -Tail 50) -join [Environment]::NewLine)"
    }
} elseif (!(Test-Path -LiteralPath $Executable -PathType Leaf)) {
    throw "Native executable is missing. Omit -SkipBuild."
} else {
    [IO.File]::WriteAllText(
        $BuildLog, "Build skipped by explicit request.`r`n", $Utf8NoBom)
}

function Invoke-Native {
    param(
        [string[]]$NativeArguments,
        [string]$LogPath
    )
    $Lines = @(& $Executable @NativeArguments 2>&1 | ForEach-Object {
        $_.ToString()
    })
    $Code = $LASTEXITCODE
    [IO.File]::WriteAllLines($LogPath, [string[]]$Lines, $Utf8NoBom)
    return [pscustomobject]@{
        code = $Code
        lines = $Lines
        text = ($Lines -join [Environment]::NewLine)
    }
}

function Get-AssetPackEntry {
    param([byte[]]$Bytes, [string]$Id)
    if ($Bytes.Length -lt 32 -or
        [Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -ne "TAP1") {
        throw "Malformed asset-pack header."
    }
    $Count = [BitConverter]::ToUInt32($Bytes, 16)
    $Directory = [BitConverter]::ToUInt64($Bytes, 20)
    for ($Index = 0; $Index -lt $Count; ++$Index) {
        $EntryOffset = [int]($Directory + [uint64]$Index * 128)
        $Name = [Text.Encoding]::ASCII.GetString(
            $Bytes, $EntryOffset, 64).Split([char]0)[0]
        if ($Name -eq $Id) {
            return [pscustomobject]@{
                offset = [BitConverter]::ToUInt64($Bytes, $EntryOffset + 84)
                size = [BitConverter]::ToUInt64($Bytes, $EntryOffset + 92)
            }
        }
    }
    throw "Asset-pack entry '$Id' was not found."
}

function Get-Fnv32 {
    param([byte[]]$Bytes)
    [uint32]$Hash = 2166136261
    foreach ($Byte in $Bytes) {
        [uint64]$Product = [uint64]($Hash -bxor [uint32]$Byte) *
            [uint64]16777619
        $Hash = [uint32]($Product % [uint64]4294967296)
    }
    return $Hash.ToString("X8")
}

$PackPath = Join-Path $OutputRoot "tecmo.assetpack"
$PackRun = Invoke-Native -NativeArguments @(
    "--build-assetpack", $RomPath, $PackPath) `
    -LogPath (Join-Path $LogsRoot "asset-pack.log")
if ($PackRun.code -ne 0 -or !(Test-Path -LiteralPath $PackPath -PathType Leaf)) {
    throw "Asset-pack build failed.`n$($PackRun.text)"
}
$PackBytes = [IO.File]::ReadAllBytes($PackPath)
$TptiEntry = Get-AssetPackEntry -Bytes $PackBytes -Id "gameplay/pre-tip"
if ($TptiEntry.size -ne $ExpectedTptiBytes) {
    throw "TPTI payload length changed: $($TptiEntry.size)."
}
$TptiPayload = New-Object byte[] ([int]$TptiEntry.size)
[Array]::Copy($PackBytes, [int]$TptiEntry.offset,
    $TptiPayload, 0, $TptiPayload.Length)
$TptiFnv32 = Get-Fnv32 -Bytes $TptiPayload
if ($TptiFnv32 -ne $ExpectedTptiFnv32) {
    throw "TPTI payload fingerprint changed: $TptiFnv32."
}

Add-Type -AssemblyName System.Drawing

function Get-FrameInspection {
    param([string]$Path)
    $Source = [Drawing.Bitmap]::FromFile($Path)
    $Bitmap = $null
    $Data = $null
    try {
        if ($Source.Width -ne $OutputWidth -or
            $Source.Height -ne $OutputHeight) {
            throw "Unexpected proof frame dimensions at ${Path}: $($Source.Width)x$($Source.Height)."
        }
        $Bitmap = New-Object Drawing.Bitmap(
            $OutputWidth, $OutputHeight,
            [Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $Graphics = [Drawing.Graphics]::FromImage($Bitmap)
        try {
            $Graphics.DrawImageUnscaled($Source, 0, 0)
        } finally {
            $Graphics.Dispose()
        }
        $Rectangle = [Drawing.Rectangle]::FromLTRB(
            0, 0, $OutputWidth, $OutputHeight)
        $Data = $Bitmap.LockBits(
            $Rectangle, [Drawing.Imaging.ImageLockMode]::ReadOnly,
            [Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $Stride = [Math]::Abs($Data.Stride)
        $Raw = New-Object byte[] ($Stride * $OutputHeight)
        [Runtime.InteropServices.Marshal]::Copy(
            $Data.Scan0, $Raw, 0, $Raw.Length)
        $LeftNonBlack = 0
        $RightNonBlack = 0
        for ($Y = 0; $Y -lt $OutputHeight; ++$Y) {
            $Row = $Y * $Stride
            for ($X = 0; $X -lt $ActiveLeft; ++$X) {
                $Pixel = $Row + $X * 4
                if ($Raw[$Pixel] -ne 0 -or $Raw[$Pixel + 1] -ne 0 -or
                    $Raw[$Pixel + 2] -ne 0) {
                    ++$LeftNonBlack
                }
            }
            for ($X = $ActiveRight + 1; $X -lt $OutputWidth; ++$X) {
                $Pixel = $Row + $X * 4
                if ($Raw[$Pixel] -ne 0 -or $Raw[$Pixel + 1] -ne 0 -or
                    $Raw[$Pixel + 2] -ne 0) {
                    ++$RightNonBlack
                }
            }
        }
        return [pscustomobject]@{
            width = $Source.Width
            height = $Source.Height
            left_margin_nonblack_pixels = $LeftNonBlack
            right_margin_nonblack_pixels = $RightNonBlack
        }
    } finally {
        if ($Data -ne $null -and $Bitmap -ne $null) {
            $Bitmap.UnlockBits($Data)
        }
        if ($Bitmap -ne $null) { $Bitmap.Dispose() }
        $Source.Dispose()
    }
}

function Convert-TipoffDiagnostic {
    param([string]$Line)
    if (!$Line.StartsWith("tipoff-proof ")) {
        throw "Malformed tip-off diagnostic: $Line"
    }
    $Fields = [ordered]@{}
    foreach ($Token in ($Line.Substring(13) -split " ")) {
        if ($Token -match "^([^=]+)=(.*)$") {
            $Fields[$Matches[1]] = $Matches[2]
        }
    }
    return [pscustomobject]$Fields
}

function Get-StageLabel {
    param([int]$Frame)
    $Labels = @{
        661 = "staging-crouch"
        665 = "takeoff"
        669 = "rising"
        673 = "apex-contact"
        676 = "apex-hold"
        680 = "falling"
        686 = "landing"
        690 = "settled"
        691 = "live-handoff"
        695 = "live-continuity"
    }
    if ($Labels.ContainsKey($Frame)) { return $Labels[$Frame] }
    if ($Frame -le 690) { return "jump-contest-$($Frame - 661)" }
    return "live-$($Frame - 691)"
}

function Assert-TipoffDiagnostic {
    param([int]$Frame, $Diagnostic)
    if ([int]$Diagnostic.frame -ne $Frame -or
        [int]$Diagnostic.'away-actor' -ne 4 -or
        [int]$Diagnostic.'home-actor' -ne 9 -or
        [int]$Diagnostic.'home-sampled' -ne 0) {
        throw "Tip-off actor/input identity contract failed at frame $Frame."
    }
    if ($Frame -le 690) {
        if ($Diagnostic.pretip -ne "jump-contest" -or
            [int]$Diagnostic.'pretip-frame' -ne ($Frame - 661) -or
            [int]$Diagnostic.'away-visible' -ne 1 -or
            [int]$Diagnostic.'home-visible' -ne 1 -or
            [int]$Diagnostic.'away-world-y' -ne 144 -or
            [int]$Diagnostic.'home-world-y' -ne 144 -or
            [int]$Diagnostic.'away-screen-y' -ne
                [int]$Diagnostic.'home-screen-y' -or
            [int]$Diagnostic.'away-altitude-q8' -ne
                [int]$Diagnostic.'home-altitude-q8' -or
            [int]$Diagnostic.'away-facing-right' -ne 0 -or
            [int]$Diagnostic.'home-facing-right' -ne 1 -or
            [int]$Diagnostic.'camera-x' -ne 256) {
            throw "Visible center-camera tip presentation contract failed at frame $Frame."
        }
    } elseif ($Diagnostic.pretip -ne "live" -or
        [int]$Diagnostic.possession -ne 0 -or
        [int]$Diagnostic.direction -ne 0 -or
        [int]$Diagnostic.'hoop-x' -ne 160) {
        throw "Away-left live handoff contract failed at frame $Frame."
    }
    if ($Frame -eq 661) {
        if ([int]$Diagnostic.'away-sampled' -ne 0) {
            throw "Away input was sampled before the first contest update."
        }
    } elseif ([int]$Diagnostic.'away-sampled' -ne 1 -or
        [int]$Diagnostic.'away-sample-frame' -ne 0 -or
        [int]$Diagnostic.'away-error' -ne 0) {
        throw "Production held-B tip input contract failed at frame $Frame."
    }
}

$PreviousPack = $env:TECMO_ASSETPACK
$env:TECMO_ASSETPACK = $PackPath
$ProofFrames = @()
try {
    for ($Frame = $ProofFirstFrame; $Frame -le $ProofLastFrame; ++$Frame) {
        $Name = "tipoff-{0:D4}.png" -f $Frame
        $Path = Join-Path $FramesRoot $Name
        $Mode = "gameplay-tipoff-proof-frame$Frame"
        $Run = Invoke-Native -NativeArguments @(
            "--root", $ProjectRoot, "--render-test-mode", $Mode, $Path) `
            -LogPath (Join-Path $LogsRoot ("tipoff-{0:D4}-pass1.log" -f $Frame))
        $DiagnosticLine = @($Run.lines | Where-Object {
            $_ -match "^tipoff-proof "
        })
        if ($Run.code -ne 0 -or $DiagnosticLine.Count -ne 1 -or
            !(Test-Path -LiteralPath $Path -PathType Leaf)) {
            throw "Native proof render failed at frame $Frame.`n$($Run.text)"
        }
        $Diagnostic = Convert-TipoffDiagnostic -Line $DiagnosticLine[0]
        Assert-TipoffDiagnostic -Frame $Frame -Diagnostic $Diagnostic
        $Inspection = Get-FrameInspection -Path $Path
        if ($Inspection.left_margin_nonblack_pixels -ne 0 -or
            $Inspection.right_margin_nonblack_pixels -ne 0) {
            throw "Host edge margin corruption detected at frame $Frame."
        }
        $ProofFrames += [pscustomobject][ordered]@{
            frame = $Frame
            stage = Get-StageLabel -Frame $Frame
            mode = $Mode
            path = $Path
            sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
            bytes = (Get-Item -LiteralPath $Path).Length
            width = $Inspection.width
            height = $Inspection.height
            left_margin_nonblack_pixels =
                $Inspection.left_margin_nonblack_pixels
            right_margin_nonblack_pixels =
                $Inspection.right_margin_nonblack_pixels
            runtime = $Diagnostic
        }
    }

    foreach ($Proof in $ProofFrames) {
        $SecondPath = Join-Path $VerifyRoot (
            "tipoff-{0:D4}.png" -f $Proof.frame)
        $SecondRun = Invoke-Native -NativeArguments @(
            "--root", $ProjectRoot, "--render-test-mode", $Proof.mode,
            $SecondPath) -LogPath (Join-Path $LogsRoot (
                "tipoff-{0:D4}-pass2.log" -f $Proof.frame))
        $SecondDiagnostic = @($SecondRun.lines | Where-Object {
            $_ -match "^tipoff-proof "
        })
        if ($SecondRun.code -ne 0 -or $SecondDiagnostic.Count -ne 1 -or
            (Get-FileHash -LiteralPath $SecondPath -Algorithm SHA256).Hash -ne
                $Proof.sha256) {
            throw "Tip-off frame $($Proof.frame) was nondeterministic."
        }
    }

    $StageExpectations = @(
        [pscustomobject]@{ frame=661; y=144; altitude=0; pose=325 },
        [pscustomobject]@{ frame=665; y=141; altitude=768; pose=1060 },
        [pscustomobject]@{ frame=669; y=129; altitude=3840; pose=1061 },
        [pscustomobject]@{ frame=673; y=120; altitude=6144; pose=213 },
        [pscustomobject]@{ frame=676; y=120; altitude=6144; pose=213 },
        [pscustomobject]@{ frame=680; y=131; altitude=3414; pose=213 },
        [pscustomobject]@{ frame=686; y=144; altitude=0; pose=469 },
        [pscustomobject]@{ frame=690; y=144; altitude=0; pose=469 }
    )
    foreach ($Expected in $StageExpectations) {
        $Actual = @($ProofFrames | Where-Object frame -eq $Expected.frame)[0]
        if ([int]$Actual.runtime.'away-screen-y' -ne $Expected.y -or
            [int]$Actual.runtime.'home-screen-y' -ne $Expected.y -or
            [int]$Actual.runtime.'away-altitude-q8' -ne $Expected.altitude -or
            [int]$Actual.runtime.'away-pose' -ne $Expected.pose -or
            [int]$Actual.runtime.'home-pose' -ne $Expected.pose) {
            throw "Visible jump stage '$($Actual.stage)' changed."
        }
    }

    $FacingPath = Join-Path $OutputRoot "away-left-facing.png"
    $FacingVerifyPath = Join-Path $VerifyRoot "away-left-facing.png"
    $FacingMode = "gameplay-facing-away-left"
    $FacingRun = Invoke-Native -NativeArguments @(
        "--root", $ProjectRoot, "--render-test-mode", $FacingMode,
        $FacingPath) -LogPath (Join-Path $LogsRoot "away-left-facing-pass1.log")
    $FacingVerifyRun = Invoke-Native -NativeArguments @(
        "--root", $ProjectRoot, "--render-test-mode", $FacingMode,
        $FacingVerifyPath) -LogPath (Join-Path $LogsRoot "away-left-facing-pass2.log")
    if ($FacingRun.code -ne 0 -or $FacingVerifyRun.code -ne 0 -or
        $FacingRun.text -notmatch
            "gameplay-state frame=691 shot=none phase=live" -or
        (Get-FileHash -LiteralPath $FacingPath -Algorithm SHA256).Hash -ne
            (Get-FileHash -LiteralPath $FacingVerifyPath -Algorithm SHA256).Hash) {
        throw "Away-left facing proof checkpoint failed or was nondeterministic."
    }
    $FacingInspection = Get-FrameInspection -Path $FacingPath
    if ($FacingInspection.left_margin_nonblack_pixels -ne 0 -or
        $FacingInspection.right_margin_nonblack_pixels -ne 0) {
        throw "Away-left facing proof has non-black host margins."
    }
} finally {
    $env:TECMO_ASSETPACK = $PreviousPack
}

function New-StageContactSheet {
    param([object[]]$Frames, [string]$Path)
    $Selected = @(661,665,669,673,676,680,686,690,691,695)
    $Columns = 5
    $CellWidth = 320
    $ImageHeight = 240
    $LabelHeight = 28
    $HeaderHeight = 38
    $Rows = [int][Math]::Ceiling($Selected.Count / [double]$Columns)
    $Sheet = New-Object Drawing.Bitmap(
        ($Columns * $CellWidth),
        ($HeaderHeight + $Rows * ($ImageHeight + $LabelHeight)),
        [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $Graphics = [Drawing.Graphics]::FromImage($Sheet)
    $Font = New-Object Drawing.Font(
        "Consolas", 14, [Drawing.FontStyle]::Bold,
        [Drawing.GraphicsUnit]::Pixel)
    $TitleFont = New-Object Drawing.Font(
        "Consolas", 18, [Drawing.FontStyle]::Bold,
        [Drawing.GraphicsUnit]::Pixel)
    try {
        $Graphics.Clear([Drawing.Color]::FromArgb(255, 16, 16, 20))
        $Graphics.InterpolationMode =
            [Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
        $Graphics.PixelOffsetMode =
            [Drawing.Drawing2D.PixelOffsetMode]::Half
        $Graphics.DrawString(
            "Native tip-off sequence: held B on every jump-contest update",
            $TitleFont, [Drawing.Brushes]::White, 8, 8)
        for ($Index = 0; $Index -lt $Selected.Count; ++$Index) {
            $Frame = @($Frames | Where-Object frame -eq $Selected[$Index])[0]
            $Column = $Index % $Columns
            $Row = [int][Math]::Floor($Index / $Columns)
            $X = $Column * $CellWidth
            $Y = $HeaderHeight + $Row * ($ImageHeight + $LabelHeight)
            $Source = [Drawing.Bitmap]::FromFile($Frame.path)
            try {
                $Destination = [Drawing.Rectangle]::FromLTRB(
                    $X, $Y, $X + $CellWidth, $Y + $ImageHeight)
                $Graphics.DrawImage($Source, $Destination)
                $Graphics.DrawString(
                    ("{0}  {1}" -f $Frame.frame, $Frame.stage),
                    $Font, [Drawing.Brushes]::White,
                    $X + 5, $Y + $ImageHeight + 5)
            } finally {
                $Source.Dispose()
            }
        }
        $Sheet.Save($Path, [Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $TitleFont.Dispose()
        $Font.Dispose()
        $Graphics.Dispose()
        $Sheet.Dispose()
    }
}

function New-ActiveEdgeSheet {
    param([object[]]$Frames, [string]$Side, [string]$Path)
    $Columns = 7
    $CropWidth = 64
    $DrawWidth = 128
    $DrawHeight = 480
    $LabelHeight = 24
    $HeaderHeight = 38
    $Rows = [int][Math]::Ceiling($Frames.Count / [double]$Columns)
    $Sheet = New-Object Drawing.Bitmap(
        ($Columns * $DrawWidth),
        ($HeaderHeight + $Rows * ($DrawHeight + $LabelHeight)),
        [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $Graphics = [Drawing.Graphics]::FromImage($Sheet)
    $Font = New-Object Drawing.Font(
        "Consolas", 13, [Drawing.FontStyle]::Bold,
        [Drawing.GraphicsUnit]::Pixel)
    $TitleFont = New-Object Drawing.Font(
        "Consolas", 18, [Drawing.FontStyle]::Bold,
        [Drawing.GraphicsUnit]::Pixel)
    try {
        $Graphics.Clear([Drawing.Color]::FromArgb(255, 16, 16, 20))
        $Graphics.InterpolationMode =
            [Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
        $Graphics.PixelOffsetMode =
            [Drawing.Drawing2D.PixelOffsetMode]::Half
        $Graphics.DrawString(
            "$Side active-view edge, every native frame 661-695",
            $TitleFont, [Drawing.Brushes]::White, 8, 8)
        $CropX = $ActiveLeft
        if ($Side -eq "right") { $CropX = $ActiveRight - $CropWidth + 1 }
        for ($Index = 0; $Index -lt $Frames.Count; ++$Index) {
            $Frame = $Frames[$Index]
            $Column = $Index % $Columns
            $Row = [int][Math]::Floor($Index / $Columns)
            $X = $Column * $DrawWidth
            $Y = $HeaderHeight + $Row * ($DrawHeight + $LabelHeight)
            $Source = [Drawing.Bitmap]::FromFile($Frame.path)
            try {
                $Destination = [Drawing.Rectangle]::FromLTRB(
                    $X, $Y, $X + $DrawWidth, $Y + $DrawHeight)
                $Crop = [Drawing.Rectangle]::FromLTRB(
                    $CropX, 0, $CropX + $CropWidth, $OutputHeight)
                $Graphics.DrawImage(
                    $Source, $Destination, $Crop,
                    [Drawing.GraphicsUnit]::Pixel)
                $Graphics.DrawString(
                    [string]$Frame.frame, $Font, [Drawing.Brushes]::White,
                    $X + 4, $Y + $DrawHeight + 4)
            } finally {
                $Source.Dispose()
            }
        }
        $Sheet.Save($Path, [Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $TitleFont.Dispose()
        $Font.Dispose()
        $Graphics.Dispose()
        $Sheet.Dispose()
    }
}

$ContactSheetPath = Join-Path $OutputRoot "tipoff-stage-contact-sheet.png"
$LeftEdgeSheetPath = Join-Path $OutputRoot "tipoff-left-edge-all-frames.png"
$RightEdgeSheetPath = Join-Path $OutputRoot "tipoff-right-edge-all-frames.png"
New-StageContactSheet -Frames $ProofFrames -Path $ContactSheetPath
New-ActiveEdgeSheet -Frames $ProofFrames -Side "left" -Path $LeftEdgeSheetPath
New-ActiveEdgeSheet -Frames $ProofFrames -Side "right" -Path $RightEdgeSheetPath

$VideoPath = $null
$VideoStatus = "ffmpeg unavailable"
$FfmpegLog = Join-Path $LogsRoot "ffmpeg.log"
if (!$FfmpegPath) {
    $FfmpegCommand = Get-Command ffmpeg -ErrorAction SilentlyContinue
    if ($FfmpegCommand -ne $null) { $FfmpegPath = $FfmpegCommand.Source }
}
if ($FfmpegPath) {
    $FfmpegPath = (Resolve-Path -LiteralPath $FfmpegPath).Path
    $VideoPath = Join-Path $OutputRoot "tipoff-sequence-661-695.mp4"
    $InputPattern = Join-Path $FramesRoot "tipoff-%04d.png"
    $FfmpegArguments =
        "-hide_banner -loglevel info -y -framerate 10 " +
        "-start_number $ProofFirstFrame -i `"$InputPattern`" " +
        "-frames:v $($ProofLastFrame - $ProofFirstFrame + 1) " +
        "-c:v libx264 -preset medium -crf 18 -pix_fmt yuv420p " +
        "`"$VideoPath`""
    $StartInfo = New-Object Diagnostics.ProcessStartInfo
    $StartInfo.FileName = $FfmpegPath
    $StartInfo.Arguments = $FfmpegArguments
    $StartInfo.UseShellExecute = $false
    $StartInfo.CreateNoWindow = $true
    $StartInfo.RedirectStandardOutput = $true
    $StartInfo.RedirectStandardError = $true
    $FfmpegProcess = New-Object Diagnostics.Process
    $FfmpegProcess.StartInfo = $StartInfo
    [void]$FfmpegProcess.Start()
    $FfmpegOutput = $FfmpegProcess.StandardOutput.ReadToEnd()
    $FfmpegError = $FfmpegProcess.StandardError.ReadToEnd()
    $FfmpegProcess.WaitForExit()
    $FfmpegExitCode = $FfmpegProcess.ExitCode
    $FfmpegProcess.Dispose()
    [IO.File]::WriteAllText(
        $FfmpegLog, $FfmpegOutput + $FfmpegError, $Utf8NoBom)
    if ($FfmpegExitCode -ne 0 -or
        !(Test-Path -LiteralPath $VideoPath -PathType Leaf)) {
        throw "ffmpeg proof encoding failed.`n$((Get-Content -LiteralPath $FfmpegLog -Tail 50) -join [Environment]::NewLine)"
    }
    $VideoStatus = "encoded 35 contiguous native frames at 10 fps"
} else {
    [IO.File]::WriteAllText(
        $FfmpegLog, "ffmpeg was unavailable; PNG/contact-sheet proof remains complete.`r`n",
        $Utf8NoBom)
}

$ScriptHash = (Get-FileHash -LiteralPath $ScriptPath -Algorithm SHA256).Hash
$GenerationCommand =
    "powershell.exe -NoProfile -ExecutionPolicy Bypass -File `"$ScriptPath`" " +
    "-ProjectRoot `"$ProjectRoot`" -RomPath `"$RomPath`" " +
    "-OutputRoot `"$OutputRoot`""
if ($FfmpegPath) {
    $GenerationCommand += " -FfmpegPath `"$FfmpegPath`""
}
if ($SkipBuild) { $GenerationCommand += " -SkipBuild" }

$ArtifactPaths = @(
    $ContactSheetPath,
    $LeftEdgeSheetPath,
    $RightEdgeSheetPath,
    $FacingPath,
    $BuildLog,
    $FfmpegLog
)
if ($VideoPath) { $ArtifactPaths += $VideoPath }
$Artifacts = @($ArtifactPaths | ForEach-Object {
    [pscustomobject][ordered]@{
        path = $_
        sha256 = (Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash
        bytes = (Get-Item -LiteralPath $_).Length
    }
})

$SummaryPath = Join-Path $OutputRoot "proof-summary.txt"
$SummaryLines = @(
    "Tecmo Basketball native tip-off visual proof",
    "commit: $Commit",
    "branch: $Branch",
    "asset pack SHA256: $((Get-FileHash -LiteralPath $PackPath -Algorithm SHA256).Hash)",
    "frames: $ProofFirstFrame-$ProofLastFrame (35 contiguous frames, deterministic double render)",
    "input: P1 controls Away; held B on every update while phase is jump-contest",
    "output: 640x480; active view x=$ActiveLeft..$ActiveRight; both host margins verified black",
    "contact sheet: $ContactSheetPath",
    "left edge sheet: $LeftEdgeSheetPath",
    "right edge sheet: $RightEdgeSheetPath",
    "away-left facing: $FacingPath",
    "video: $VideoPath",
    "generation command: $GenerationCommand"
)
[IO.File]::WriteAllLines($SummaryPath, [string[]]$SummaryLines, $Utf8NoBom)
$Artifacts += [pscustomobject][ordered]@{
    path = $SummaryPath
    sha256 = (Get-FileHash -LiteralPath $SummaryPath -Algorithm SHA256).Hash
    bytes = (Get-Item -LiteralPath $SummaryPath).Length
}

$Manifest = [pscustomobject][ordered]@{
    schema = "tecmo.tipoff-visual-proof/1"
    generated_utc = [DateTime]::UtcNow.ToString("o")
    repository = [pscustomobject][ordered]@{
        worktree = $ProjectRoot
        branch = $Branch
        commit = $Commit
        clean_worktree_required = $true
    }
    source = [pscustomobject][ordered]@{
        rom_revision = "Tecmo NBA Basketball (USA) Rev 1"
        rom_path = $RomPath
        rom_sha256 = $ExpectedRomSha256
        rom_or_payload_committed = $false
        tpti_payload_bytes = $TptiPayload.Length
        tpti_payload_fnv1a32 = $TptiFnv32
    }
    asset_pack = [pscustomobject][ordered]@{
        path = $PackPath
        sha256 = (Get-FileHash -LiteralPath $PackPath -Algorithm SHA256).Hash
        bytes = (Get-Item -LiteralPath $PackPath).Length
    }
    render = [pscustomobject][ordered]@{
        width = $OutputWidth
        height = $OutputHeight
        active_view_x = "$ActiveLeft..$ActiveRight"
        active_view_y = "0..479"
        native_scale = 2
        first_frame = $ProofFirstFrame
        last_frame = $ProofLastFrame
        contiguous_frame_count = $ProofFrames.Count
        deterministic_passes = 2
        left_host_margin_nonblack_pixels = 0
        right_host_margin_nonblack_pixels = 0
    }
    input_script = [pscustomobject][ordered]@{
        generator_path = $ScriptPath
        generator_sha256 = $ScriptHash
        controller_1_team = "Away"
        schedule = "neutral before contest; held B for every production update while phase is jump-contest; neutral after live handoff"
        observed_away_sample_frame = 0
        observed_away_tip_error = 0
        home_tip_sampled = $false
    }
    assertions = @(
        "both TPTI jumper actors 4 and 9 are visible in every contest frame",
        "both jumper screen Y and pose follow crouch/takeoff/rise/apex/fall/landing stages",
        "pre-tip camera remains at source-backed center x=0x0100",
        "live handoff awards Away possession and preserves its left goal orientation",
        "all 35 first-pass PNGs exactly match independently rendered second-pass PNGs",
        "all output pixels outside active view x=64..575 are black on both edges",
        "gameplay-facing-away-left checkpoint validates active actors against team goal mapping"
    )
    frames = $ProofFrames
    facing_checkpoint = [pscustomobject][ordered]@{
        mode = $FacingMode
        path = $FacingPath
        sha256 = (Get-FileHash -LiteralPath $FacingPath -Algorithm SHA256).Hash
        deterministic_passes = 2
        validated_contract = "fresh TGOR: Away attacks/faces left; Home attacks/faces right; explicit action overrides remain authoritative"
    }
    video = [pscustomobject][ordered]@{
        status = $VideoStatus
        path = $VideoPath
        frame_rate = 10
        frame_count = 35
    }
    artifacts = $Artifacts
    generation_command = $GenerationCommand
}
$ManifestPath = Join-Path $OutputRoot "proof-manifest.json"
$ManifestJson = $Manifest | ConvertTo-Json -Depth 12
[IO.File]::WriteAllText($ManifestPath, $ManifestJson + "`r`n", $Utf8NoBom)

Write-Output "TIPOFF VISUAL PROOF PASS"
Write-Output "manifest=$ManifestPath"
Write-Output "contact-sheet=$ContactSheetPath"
Write-Output "left-edge-sheet=$LeftEdgeSheetPath"
Write-Output "right-edge-sheet=$RightEdgeSheetPath"
Write-Output "facing=$FacingPath"
Write-Output "video=$VideoPath"
Write-Output "frames=$FramesRoot"
