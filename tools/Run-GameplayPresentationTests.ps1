param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [switch]$Build,
    [string]$ProofRootPath
)

$ErrorActionPreference = "Stop"
if (!$ProjectRoot) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if (!$RomPath) { $RomPath = $env:TECMO_ROM_PATH }
if (!$RomPath -or !(Test-Path -LiteralPath $RomPath -PathType Leaf)) {
    throw "Pass -RomPath or set TECMO_ROM_PATH to the local Rev1 iNES ROM."
}
$RomPath = (Resolve-Path -LiteralPath $RomPath).Path

$OutputWidth = 640
$OutputHeight = 480
$ActiveFirstFrame = 1
$ActiveLastFrame = 16
$TerminalFrame = 17
$UpperBoundPlusOneFrame = 18
$ExpectedRomSha256 = "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4"
$ExpectedCloseShotBytes = 3144
$ExpectedCloseShotFnv1a32 = "DACDC976"
$ExpectedVariant2Phases = @(0, 1, 2, 3, 3, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5)

$BuildDir = Join-Path $ProjectRoot "build"
$Executable = Join-Path $BuildDir "tecmo_port.exe"
$BuildPrefix = [IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
$Scratch = [IO.Path]::GetFullPath((Join-Path $BuildDir "gameplay_presentation_test"))
if (!$Scratch.StartsWith($BuildPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Gameplay presentation scratch path escaped build\."
}
if (!$ProofRootPath) {
    $ProofRootPath = Join-Path $BuildDir ("gameplay-layup-proof-" + [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss-fff"))
} elseif (![IO.Path]::IsPathRooted($ProofRootPath)) {
    $ProofRootPath = Join-Path $BuildDir $ProofRootPath
}
$ProofRoot = [IO.Path]::GetFullPath($ProofRootPath)
if (!$ProofRoot.StartsWith($BuildPrefix, [StringComparison]::OrdinalIgnoreCase) -or
    $ProofRoot -eq [IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/')) {
    throw "Gameplay presentation proof path must be a child of build\."
}
if (Test-Path -LiteralPath $ProofRoot) {
    if (@(Get-ChildItem -LiteralPath $ProofRoot -Force).Count -ne 0) {
        throw "Proof path already contains artifacts: $ProofRoot"
    }
} else {
    New-Item -ItemType Directory -Force -Path $ProofRoot | Out-Null
}
$PassOneRoot = Join-Path $ProofRoot "pass-one"
$PassTwoRoot = Join-Path $ProofRoot "pass-two"
$NegativeRoot = Join-Path $ProofRoot "negative"
$LogsRoot = Join-Path $ProofRoot "logs"
New-Item -ItemType Directory -Force -Path $PassOneRoot, $PassTwoRoot, $NegativeRoot, $LogsRoot | Out-Null

Add-Type -AssemblyName System.Drawing

function Get-ShortTail {
    param([object[]]$Lines)
    return (@($Lines | Select-Object -Last 12) -join [Environment]::NewLine)
}

function Invoke-Native {
    param([string[]]$Arguments, [string]$LogPath)
    $Lines = @(& $Executable @Arguments 2>&1)
    $ExitCode = $LASTEXITCODE
    $Text = $Lines -join [Environment]::NewLine
    if ($LogPath) {
        [IO.File]::WriteAllText($LogPath, $Text + [Environment]::NewLine, [Text.Encoding]::UTF8)
    }
    return [pscustomobject][ordered]@{
        code = $ExitCode
        lines = $Lines
        text = $Text
        log = $LogPath
        command = $Executable + " " + ($Arguments -join " ")
    }
}

function Get-GitState {
    $Branch = @(& git -C $ProjectRoot symbolic-ref --short HEAD 2>&1)
    $Head = @(& git -C $ProjectRoot rev-parse HEAD 2>&1)
    $Status = @(& git -C $ProjectRoot status --short 2>&1)
    if ($LASTEXITCODE -ne 0 -or $Branch.Count -eq 0 -or $Head.Count -eq 0) {
        throw "Could not read git branch, HEAD, or status."
    }
    $Status = @($Status | Where-Object { ![String]::IsNullOrWhiteSpace([string]$_) } | ForEach-Object { [string]$_ })
    $Clean = $Status.Count -eq 0
    return [pscustomobject][ordered]@{
        branch = ($Branch -join "").Trim()
        head = ($Head -join "").Trim()
        clean = $Clean
        clean_status = if ($Clean) { "clean" } else { "dirty" }
        status_lines = $Status
    }
}

function Get-Fnv1a32 {
    param([byte[]]$Bytes)
    [uint32]$Hash = 2166136261
    foreach ($Byte in $Bytes) {
        [uint64]$Product = [uint64]($Hash -bxor [uint32]$Byte) * [uint64]16777619
        $Hash = [uint32]($Product % [uint64]4294967296)
    }
    return ("{0:X8}" -f $Hash)
}

function Get-AssetPackEntry {
    param([byte[]]$Bytes, [string]$Id)
    if ($Bytes.Length -lt 40 -or [Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -ne "TAP1" -or
        [BitConverter]::ToUInt32($Bytes, 4) -ne 1 -or [BitConverter]::ToUInt32($Bytes, 8) -ne 40 -or
        [BitConverter]::ToUInt32($Bytes, 12) -ne 128) {
        throw "Asset pack header is not TAP1 v1."
    }
    $Count = [BitConverter]::ToUInt32($Bytes, 16)
    $Directory = [BitConverter]::ToUInt64($Bytes, 20)
    if ($Directory -gt [uint64]$Bytes.Length -or [uint64]$Count * 128 -gt [uint64]$Bytes.Length - $Directory) {
        throw "Asset pack directory is out of bounds."
    }
    for ($Index = 0; $Index -lt $Count; ++$Index) {
        $Offset = [int]$Directory + $Index * 128
        $Terminator = [Array]::IndexOf($Bytes, [byte]0, $Offset, 64)
        if ($Terminator -lt 0) { $Terminator = $Offset + 64 }
        $EntryId = [Text.Encoding]::ASCII.GetString($Bytes, $Offset, $Terminator - $Offset)
        if ($EntryId -ne $Id) { continue }
        $PackOffset = [BitConverter]::ToUInt64($Bytes, $Offset + 84)
        $ByteCount = [BitConverter]::ToUInt64($Bytes, $Offset + 92)
        if ($PackOffset -gt [uint64]$Bytes.Length -or $ByteCount -gt [uint64]$Bytes.Length - $PackOffset) {
            throw "Asset pack entry '$Id' is out of bounds."
        }
        return [pscustomobject][ordered]@{
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
    [Array]::Copy($PackBytes, [int]$Entry.pack_offset, $Result, 0, $Result.Length)
    return $Result
}

function Get-TgcsIdentity {
    param([string]$AssetPackPath)
    $PackBytes = [IO.File]::ReadAllBytes($AssetPackPath)
    $Entry = Get-AssetPackEntry $PackBytes "gameplay/close-shots"
    if ($Entry.byte_count -ne $ExpectedCloseShotBytes) { throw "TGCS-1 byte count changed." }
    $Payload = Get-EntryBytes $PackBytes $Entry
    if ([Text.Encoding]::ASCII.GetString($Payload, 0, 4) -ne "TGCS" -or
        [BitConverter]::ToUInt16($Payload, 4) -ne 1 -or [BitConverter]::ToUInt16($Payload, 6) -ne 256 -or
        (Get-Fnv1a32 $Payload) -ne $ExpectedCloseShotFnv1a32) {
        throw "TGCS-1 payload identity changed."
    }
    $Variant2Offset = [BitConverter]::ToUInt32($Payload, 36)
    $Variant2StepCount = [BitConverter]::ToUInt32($Payload, 40)
    if ($Variant2StepCount -ne $ExpectedVariant2Phases.Count -or
        $Variant2Offset -gt $Payload.Length -or $Variant2StepCount -gt $Payload.Length - $Variant2Offset) {
        throw "TGCS-1 variant-2 phase span is malformed."
    }
    $Phases = @()
    for ($Index = 0; $Index -lt $Variant2StepCount; ++$Index) {
        $Phases += [int]$Payload[[int]$Variant2Offset + $Index]
    }
    if (($Phases -join ",") -ne ($ExpectedVariant2Phases -join ",")) {
        throw "TGCS-1 variant-2 phase schedule changed."
    }
    return [pscustomobject][ordered]@{
        path = $AssetPackPath
        sha256 = (Get-FileHash -LiteralPath $AssetPackPath -Algorithm SHA256).Hash.ToUpperInvariant()
        entry_bytes = [int]$Entry.byte_count
        entry_fnv1a32 = $ExpectedCloseShotFnv1a32
        variant2_numeric_id = 2
        variant2_offset = [int]$Variant2Offset
        variant2_step_count = [int]$Variant2StepCount
        variant2_phases = $Phases
    }
}

function Get-FrameInspection {
    param([string]$Path)
    $Bitmap = $null
    try {
        $Bitmap = [Drawing.Bitmap]::FromFile($Path)
        if ($Bitmap.Width -ne $OutputWidth -or $Bitmap.Height -ne $OutputHeight) {
            throw "Expected 640x480 PNG at $Path."
        }
        $Colors = New-Object 'System.Collections.Generic.HashSet[int]'
        $NonBlack = 0
        $Samples = 0
        for ($Y = 0; $Y -lt $OutputHeight; $Y += 4) {
            for ($X = 0; $X -lt $OutputWidth; $X += 4) {
                $Argb = $Bitmap.GetPixel($X, $Y).ToArgb()
                [void]$Colors.Add($Argb)
                if ($Argb -ne -16777216) { ++$NonBlack }
                ++$Samples
            }
        }
        if ($Colors.Count -lt 4 -or $NonBlack -lt 16) {
            throw "Blank or collapsed visual sentinel at $Path."
        }
        return [pscustomobject][ordered]@{
            width = $Bitmap.Width
            height = $Bitmap.Height
            sampled_pixels = $Samples
            unique_sample_colors = $Colors.Count
            nonblack_sample_pixels = $NonBlack
        }
    } finally {
        if ($Bitmap -ne $null) { $Bitmap.Dispose() }
    }
}

function Get-ImageDimensions {
    param([string]$Path)
    $Image = $null
    try {
        $Image = [Drawing.Image]::FromFile($Path)
        return [pscustomobject][ordered]@{ width = $Image.Width; height = $Image.Height }
    } finally {
        if ($Image -ne $null) { $Image.Dispose() }
    }
}

function Invoke-LayupRender {
    param([int]$Frame, [string]$PassName, [string]$PassRoot, [int[]]$TgcsPhases)
    $Mode = "gameplay-layup-frame$Frame"
    $Path = Join-Path $PassRoot ("gameplay-layup-frame{0:D2}.png" -f $Frame)
    $LogPath = Join-Path $LogsRoot ("$PassName-gameplay-layup-frame{0:D2}.log" -f $Frame)
    if (Test-Path -LiteralPath $Path) { throw "Positive output already exists: $Path" }
    $Run = Invoke-Native @("--root", $ProjectRoot, "--render-test-mode", $Mode, $Path) $LogPath
    $StateLines = @($Run.lines | Where-Object { [string]$_ -match "^gameplay-state " })
    if ($Run.code -ne 0 -or $StateLines.Count -ne 1 -or !(Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw ("Layup render failed at frame " + $Frame + ": " + (Get-ShortTail $Run.lines))
    }
    $State = [string]$StateLines[0]
    if ($Frame -le $ActiveLastFrame) {
        $StateClass = "active"
        $ExpectedState = "gameplay-state frame=$Frame shot=layup phase=live"
        if ($State -notmatch ("^" + [regex]::Escape($ExpectedState) + "(\s|$)")) {
            throw ("Unexpected active layup state at frame " + $Frame + ": " + $State)
        }
        $PhaseIndex = $Frame - $ActiveFirstFrame
        $TgcsStep = $PhaseIndex
        $TgcsPhase = [int]$TgcsPhases[$PhaseIndex]
    } else {
        $StateClass = "terminal"
        $ExpectedState = "gameplay-state frame=17 shot=none phase=live"
        if ($State -notmatch ("^" + [regex]::Escape($ExpectedState) + "(\s|$)")) {
            throw ("Unexpected terminal layup state: " + $State)
        }
        $TgcsStep = "terminal-boundary"
        $TgcsPhase = $null
    }
    $Inspection = Get-FrameInspection $Path
    return [pscustomobject][ordered]@{
        pass = $PassName
        branch = $GitState.branch
        head = $GitState.head
        clean = $GitState.clean
        clean_status = $GitState.clean_status
        frame = $Frame
        mode = $Mode
        state_class = $StateClass
        expected_state = $ExpectedState
        state = $State
        tgcs_numeric_variant = 2
        tgcs_step = $TgcsStep
        tgcs_phase = $TgcsPhase
        output = $Path
        log = $LogPath
        command = $Run.command
        width = $Inspection.width
        height = $Inspection.height
        sampled_pixels = $Inspection.sampled_pixels
        unique_sample_colors = $Inspection.unique_sample_colors
        nonblack_sample_pixels = $Inspection.nonblack_sample_pixels
        sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
        pass_two_sha256 = $null
    }
}

function Invoke-RejectedRender {
    param([string]$Label, [string]$Mode, [bool]$PreserveOutput)
    $Path = Join-Path $NegativeRoot "$Label.png"
    $LogPath = Join-Path $LogsRoot "negative-$Label.log"
    if (Test-Path -LiteralPath $Path) { throw "Negative output already exists: $Path" }
    $BeforeExists = $false
    $BeforeHash = $null
    $BeforeBytes = 0
    if ($PreserveOutput) {
        $Sentinel = [byte[]](0x54, 0x52, 0x41, 0x4E, 0x53, 0x41, 0x43, 0x54, 0x49, 0x4F, 0x4E, 0x41, 0x4C)
        [IO.File]::WriteAllBytes($Path, $Sentinel)
        $BeforeExists = $true
        $BeforeHash = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
        $BeforeBytes = (Get-Item -LiteralPath $Path).Length
    }
    $Run = Invoke-Native @("--root", $ProjectRoot, "--render-test-mode", $Mode, $Path) $LogPath
    $AfterExists = Test-Path -LiteralPath $Path -PathType Leaf
    $AfterHash = $null
    $AfterBytes = 0
    if ($AfterExists) {
        $AfterHash = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
        $AfterBytes = (Get-Item -LiteralPath $Path).Length
    }
    if ($Run.code -eq 0) { throw "Rejected mode was accepted: $Mode" }
    if ($PreserveOutput -and (!$AfterExists -or $AfterHash -ne $BeforeHash -or $AfterBytes -ne $BeforeBytes)) {
        throw "Rejected mode altered its pre-existing output: $Mode"
    }
    if (!$PreserveOutput -and $AfterExists) { throw "Rejected mode created an output: $Mode" }
    return [pscustomobject][ordered]@{
        label = $Label
        mode = $Mode
        output = $Path
        log = $LogPath
        exit_code = $Run.code
        output_existed_before = $BeforeExists
        output_exists_after = $AfterExists
        sha256_before = $BeforeHash
        sha256_after = $AfterHash
        bytes_before = $BeforeBytes
        bytes_after = $AfterBytes
        transactional = $true
        output_tail = Get-ShortTail $Run.lines
    }
}

function New-ContactSheet {
    param([object[]]$Frames, [string]$Path)
    $Columns = 4
    $Rows = 5
    $Sheet = New-Object Drawing.Bitmap(($Columns * $OutputWidth), ($Rows * $OutputHeight), [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $Graphics = [Drawing.Graphics]::FromImage($Sheet)
    try {
        $Graphics.Clear([Drawing.Color]::Black)
        $Graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
        $Graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::Half
        for ($Index = 0; $Index -lt $Frames.Count; ++$Index) {
            $Image = [Drawing.Image]::FromFile($Frames[$Index].output)
            try {
                $X = ($Index % $Columns) * $OutputWidth
                $Y = [int][Math]::Floor($Index / [double]$Columns) * $OutputHeight
                $Graphics.DrawImageUnscaled($Image, $X, $Y)
            } finally {
                $Image.Dispose()
            }
        }
        $Sheet.Save($Path, [Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $Graphics.Dispose()
        $Sheet.Dispose()
    }
    $Dimensions = Get-ImageDimensions $Path
    if ($Dimensions.width -ne 2560 -or $Dimensions.height -ne 2400) {
        throw "Contact sheet is not 4x5 full-resolution 640x480 cells."
    }
    return [pscustomobject][ordered]@{
        path = $Path
        width = $Dimensions.width
        height = $Dimensions.height
        columns = $Columns
        rows = $Rows
        cell_width = $OutputWidth
        cell_height = $OutputHeight
        frame_order = @($Frames | ForEach-Object { [int]$_.frame })
        sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
    }
}

$RomSha256 = (Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash.ToUpperInvariant()
if ($RomSha256 -ne $ExpectedRomSha256) { throw "The focused runner requires the exact Rev1 ROM SHA-256." }
$PreviousAssetPack = $env:TECMO_ASSETPACK
$PreviousSkipShortcut = $env:TECMO_SKIP_SHORTCUT
$GitState = Get-GitState
$BuildRecord = $null
$PackIdentity = $null
$PassOne = @()
$PassTwo = @()
$NegativeTests = @()
$AllowedEqualities = @()
$ManifestPath = Join-Path $ProofRoot "manifest.json"
$SummaryPath = Join-Path $ProofRoot "summary.txt"

try {
    $env:TECMO_SKIP_SHORTCUT = "1"
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
    if ($Build) {
        $BuildScript = Join-Path $ProjectRoot "build.ps1"
        $BuildLogPath = Join-Path $LogsRoot "build.log"
        $BuildLines = @(& $BuildScript 2>&1)
        $BuildExitCode = $LASTEXITCODE
        [IO.File]::WriteAllText($BuildLogPath, ($BuildLines -join [Environment]::NewLine) + [Environment]::NewLine, [Text.Encoding]::UTF8)
        $BuildWarnings = @($BuildLines | Where-Object { [string]$_ -match "(?i)\bwarning\s+[A-Z]+\d+" })
        if ($BuildExitCode -ne 0 -or $BuildWarnings.Count -ne 0) {
            throw ("Warning-free CMake/build.ps1 build failed: " + (Get-ShortTail $BuildLines))
        }
        $BuildRecord = [pscustomobject][ordered]@{ requested = $true; exit_code = $BuildExitCode; warning_lines = $BuildWarnings; log = $BuildLogPath }
    } else {
        $BuildRecord = [pscustomobject][ordered]@{ requested = $false; exit_code = $null; warning_lines = @(); log = $null }
    }
    if (!(Test-Path -LiteralPath $Executable -PathType Leaf)) { throw "Build output is missing; rerun with -Build." }
    if (Test-Path -LiteralPath $Scratch) { Remove-Item -LiteralPath $Scratch -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $Scratch | Out-Null
    $PackPath = Join-Path $Scratch "gameplay-presentation.assetpack"
    $PackRun = Invoke-Native @("--build-assetpack", $RomPath, $PackPath) (Join-Path $LogsRoot "build-assetpack.log")
    if ($PackRun.code -ne 0 -or !(Test-Path -LiteralPath $PackPath -PathType Leaf)) {
        throw ("Asset-pack build failed: " + (Get-ShortTail $PackRun.lines))
    }
    $PackIdentity = Get-TgcsIdentity $PackPath
    $TgcsRun = Invoke-Native @("--gameplay-close-shots-test", $PackPath) (Join-Path $LogsRoot "tgcs-loader.log")
    if ($TgcsRun.code -ne 0 -or $TgcsRun.text -notmatch "TGCS-1 close-shot assets passed") {
        throw ("TGCS-1 loader proof failed: " + (Get-ShortTail $TgcsRun.lines))
    }
    $env:TECMO_ASSETPACK = $PackPath
    for ($Frame = $ActiveFirstFrame; $Frame -le $TerminalFrame; ++$Frame) {
        $PassOne += Invoke-LayupRender $Frame "pass-one" $PassOneRoot $ExpectedVariant2Phases
    }
    for ($Frame = $ActiveFirstFrame; $Frame -le $TerminalFrame; ++$Frame) {
        $PassTwo += Invoke-LayupRender $Frame "pass-two" $PassTwoRoot $ExpectedVariant2Phases
    }
    if ($PassOne.Count -ne $TerminalFrame -or $PassTwo.Count -ne $TerminalFrame) {
        throw "Two-pass proof did not render exactly $TerminalFrame frames."
    }
    for ($Index = 0; $Index -lt $TerminalFrame; ++$Index) {
        $First = $PassOne[$Index]
        $Second = $PassTwo[$Index]
        if ([int]$First.frame -ne ($Index + 1) -or [int]$Second.frame -ne ($Index + 1) -or
            $First.mode -ne $Second.mode -or $First.state -ne $Second.state -or $First.sha256 -ne $Second.sha256) {
            throw ("Pass-one/pass-two determinism failed at frame " + ($Index + 1) + ".")
        }
        $First.pass_two_sha256 = $Second.sha256
    }
    if (@($PassOne | Where-Object { $_.state_class -eq "active" }).Count -ne 16 -or
        @($PassOne | Where-Object { $_.state_class -eq "terminal" }).Count -ne 1) {
        throw "Active/terminal state classification is incomplete."
    }
    for ($Index = 0; $Index -lt $ExpectedVariant2Phases.Count; ++$Index) {
        $Frame = $PassOne[$Index]
        if ([int]$Frame.frame -ne ($Index + 1) -or [int]$Frame.tgcs_step -ne $Index -or
            [int]$Frame.tgcs_phase -ne $ExpectedVariant2Phases[$Index] -or
            [int]$Frame.width -ne $OutputWidth -or [int]$Frame.height -ne $OutputHeight) {
            throw ("TGCS variant-2 step coverage failed at frame " + ($Index + 1) + ".")
        }
    }
    $Terminal = $PassOne[$TerminalFrame - 1]
    if ($Terminal.state_class -ne "terminal" -or $Terminal.tgcs_step -ne "terminal-boundary" -or
        $Terminal.width -ne $OutputWidth -or $Terminal.height -ne $OutputHeight) {
        throw "TGCS terminal/tail boundary proof failed."
    }
    $RepresentativeByPhase = @{}
    foreach ($Frame in $PassOne | Where-Object { $_.state_class -eq "active" }) {
        $Key = [string]$Frame.tgcs_phase
        if (!$RepresentativeByPhase.ContainsKey($Key)) { $RepresentativeByPhase[$Key] = $Frame }
    }
    if ($RepresentativeByPhase.Count -ne 6) { throw "Variant-2 visual proof did not expose all six TGCS phases." }
    $Representatives = @($RepresentativeByPhase.Values)
    for ($Left = 0; $Left -lt $Representatives.Count; ++$Left) {
        for ($Right = $Left + 1; $Right -lt $Representatives.Count; ++$Right) {
            if ($Representatives[$Left].sha256 -eq $Representatives[$Right].sha256) {
                throw ("Visual phases collapsed at TGCS phases " + $Representatives[$Left].tgcs_phase + " and " + $Representatives[$Right].tgcs_phase + ".")
            }
        }
    }
    for ($Left = 0; $Left -lt $PassOne.Count; ++$Left) {
        for ($Right = $Left + 1; $Right -lt $PassOne.Count; ++$Right) {
            $LeftFrame = $PassOne[$Left]
            $RightFrame = $PassOne[$Right]
            if ($LeftFrame.sha256 -ne $RightFrame.sha256) { continue }
            $SameActivePhase = $LeftFrame.state_class -eq "active" -and $RightFrame.state_class -eq "active" -and $LeftFrame.tgcs_phase -eq $RightFrame.tgcs_phase
            if (!$SameActivePhase) {
                throw ("Unexpected duplicate/collapsed output at frames " + $LeftFrame.frame + " and " + $RightFrame.frame + ".")
            }
            $AllowedEqualities += [pscustomobject][ordered]@{
                frame_a = [int]$LeftFrame.frame
                frame_b = [int]$RightFrame.frame
                tgcs_phase = [int]$LeftFrame.tgcs_phase
                reason = "same TGCS variant-2 pose phase"
            }
        }
    }
    if (@($PassOne | Select-Object -ExpandProperty sha256 -Unique).Count -lt 6 -or $PassOne[0].sha256 -eq $Terminal.sha256) {
        throw "Two-pass visual hashes are too collapsed."
    }
    $NegativeTests += Invoke-RejectedRender "frame-zero" "gameplay-layup-frame0" $false
    $NegativeTests += Invoke-RejectedRender "frame-upper-bound-plus-one" "gameplay-layup-frame18" $true
    $NegativeTests += Invoke-RejectedRender "missing-suffix" "gameplay-layup-frame" $false
    $NegativeTests += Invoke-RejectedRender "plus-suffix" "gameplay-layup-frame+1" $false
    $NegativeTests += Invoke-RejectedRender "trailing-character" "gameplay-layup-frame1x" $false
    $NegativeTests += Invoke-RejectedRender "leading-zero" "gameplay-layup-frame01" $false
    if ($NegativeTests.Count -ne 6 -or @($NegativeTests | Where-Object { !$_.transactional }).Count -ne 0) {
        throw "Strict negative-input transaction proof is incomplete."
    }
    $ContactSheetPath = Join-Path $ProofRoot "gameplay-layup-contact-sheet.png"
    $ContactSheet = New-ContactSheet $PassOne $ContactSheetPath
    $ContactKeyPath = Join-Path $ProofRoot "contact-sheet-key.txt"
    $ContactKeyLines = @("full-resolution cell size: 640x480", "columns: 4", "rows: 5", "cell order: frames 1 through 17, row-major", "unused cells: 18 through 20 are black padding")
    [IO.File]::WriteAllText($ContactKeyPath, ($ContactKeyLines -join [Environment]::NewLine) + [Environment]::NewLine, [Text.Encoding]::UTF8)
    $Manifest = [ordered]@{
        schema = "tecmo.gameplay-presentation/TGPR-1"
        generated_utc = [DateTime]::UtcNow.ToString("o")
        project_root = $ProjectRoot
        branch = $GitState.branch
        head = $GitState.head
        clean = $GitState.clean
        clean_status = $GitState.clean_status
        git_status_lines = @($GitState.status_lines)
        rom_sha256 = $RomSha256
        build = $BuildRecord
        executable = $Executable
        asset_pack = $PackIdentity
        mode_prefix = "gameplay-layup-frameN"
        semantic_kind = "layup"
        numeric_variant = 2
        active_frame_range = "1..16"
        terminal_frame = $TerminalFrame
        upper_bound_plus_one_frame = $UpperBoundPlusOneFrame
        state_expectations = [ordered]@{ active = "gameplay-state frame=N shot=layup phase=live"; terminal = "gameplay-state frame=17 shot=none phase=live" }
        tgcs_variant2_step_count = $PackIdentity.variant2_step_count
        tgcs_variant2_phases = @($PackIdentity.variant2_phases)
        pass_one = @($PassOne)
        pass_two = @($PassTwo)
        pass_one_pass_two_hashes_equal = $true
        allowed_equalities = @($AllowedEqualities)
        negative_tests = @($NegativeTests)
        contact_sheet = $ContactSheet
        contact_sheet_key = $ContactKeyPath
        proof_artifacts_under_build = $true
        capture_surface = "native tecmo_port.exe --render-test-mode"
        evidence_classifications = @(
            "exact-source-pinned: TGCS-1 variant-2 phase schedule and payload identity",
            "native-faithful: production scene selection yields shot=layup and variant=2",
            "native-approximate: coherent CLI live-court input fixture and host-native PNG rendering",
            "incomplete-unproven: no emulator-perfect parity, capture trace, or unsupported semantics claimed"
        )
    }
    [IO.File]::WriteAllText($ManifestPath, ($Manifest | ConvertTo-Json -Depth 18) + [Environment]::NewLine, [Text.Encoding]::UTF8)
    $SummaryLines = @("Gameplay presentation layup proof passed.", "mode-prefix=gameplay-layup-frameN", "active-frames=1..16", "terminal-frame=17", "numeric-variant=2", "tgcs-variant2-phases=$($PackIdentity.variant2_phases -join ',')", "two-pass-hashes-equal=true", "frame-size=640x480", "negative-tests=frame-zero,upper-bound-plus-one,malformed-and-canonical-suffixes", "manifest=$ManifestPath", "contact-sheet=$($ContactSheet.path)")
    [IO.File]::WriteAllText($SummaryPath, ($SummaryLines -join [Environment]::NewLine) + [Environment]::NewLine, [Text.Encoding]::UTF8)
    Write-Output ($SummaryLines -join [Environment]::NewLine)
} finally {
    if ($null -eq $PreviousAssetPack) { Remove-Item Env:\TECMO_ASSETPACK -ErrorAction SilentlyContinue } else { $env:TECMO_ASSETPACK = $PreviousAssetPack }
    if ($null -eq $PreviousSkipShortcut) { Remove-Item Env:\TECMO_SKIP_SHORTCUT -ErrorAction SilentlyContinue } else { $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut }
    if (Test-Path -LiteralPath $Scratch) { Remove-Item -LiteralPath $Scratch -Recurse -Force }
}
