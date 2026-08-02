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
$ProofFrameCount = $ProofLastFrame - $ProofFirstFrame + 1
$NativeFrameRateNumerator = [int64]39375000
$NativeFrameRateDenominator = [int64]655171
$NativeFrameRateHz = [double]$NativeFrameRateNumerator /
    [double]$NativeFrameRateDenominator
$NativeFrameDurationSeconds = 1.0 / $NativeFrameRateHz
$NativeFrameRateText = "$NativeFrameRateNumerator/$NativeFrameRateDenominator"
$ProofSchema = "tecmo.tipoff-realtime-proof/2"
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
$ScriptPath = (Resolve-Path -LiteralPath $MyInvocation.MyCommand.Path).Path
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

function Format-CommandArguments {
    param([string[]]$Arguments)
    $Parts = @()
    foreach ($Argument in $Arguments) {
        $Escaped = $Argument.Replace('"', '\"')
        $Parts += ('"' + $Escaped + '"')
    }
    return ($Parts -join ' ')
}

function Format-CommandLine {
    param([string]$File, [string[]]$Arguments)
    return ('"' + $File + '" ' + (Format-CommandArguments -Arguments $Arguments)).Trim()
}

function Get-ByteArraySha256 {
    param([byte[]]$Bytes)
    $Hasher = [Security.Cryptography.SHA256]::Create()
    try {
        return ([BitConverter]::ToString($Hasher.ComputeHash($Bytes)) -replace '-', '')
    } finally {
        $Hasher.Dispose()
    }
}

function Resolve-ToolPath {
    param([string]$Candidate, [string]$Name)
    if ($Candidate) {
        if (Test-Path -LiteralPath $Candidate -PathType Leaf) {
            return (Resolve-Path -LiteralPath $Candidate).Path
        }
        $CandidateCommand = Get-Command $Candidate -ErrorAction SilentlyContinue
        if ($null -eq $CandidateCommand -or !$CandidateCommand.Path) {
            throw "$Name executable was not found: $Candidate"
        }
        return (Resolve-Path -LiteralPath $CandidateCommand.Path).Path
    }
    $Command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -eq $Command -or !$Command.Path) {
        throw "$Name executable is required but was not found on PATH."
    }
    return (Resolve-Path -LiteralPath $Command.Path).Path
}

function Get-TrackedBuildInputFiles {
    $Paths = @(& git -C $ProjectRoot ls-files -- src include build.ps1 `
        CMakeLists.txt 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "Could not enumerate tracked native build inputs: $($Paths -join [Environment]::NewLine)"
    }
    $Files = @()
    foreach ($RelativePath in $Paths) {
        $Text = $RelativePath.ToString().Trim()
        if (!$Text) { continue }
        $FullPath = Join-Path $ProjectRoot $Text
        if (Test-Path -LiteralPath $FullPath -PathType Leaf) {
            $Files += Get-Item -LiteralPath $FullPath
        }
    }
    if ($Files.Count -eq 0) {
        throw "No tracked native build inputs were found for executable freshness validation."
    }
    return $Files
}

function Assert-ExecutableFresh {
    param([string]$Path)
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Native executable is missing: $Path"
    }
    $ExecutableInfo = Get-Item -LiteralPath $Path
    $NewestInput = @(
        Get-TrackedBuildInputFiles |
            Sort-Object -Property LastWriteTimeUtc -Descending
    )[0]
    if ($ExecutableInfo.LastWriteTimeUtc -lt $NewestInput.LastWriteTimeUtc) {
        throw "Native executable is stale: $Path is older than $($NewestInput.FullName). Rebuild without -SkipBuild."
    }
    return $ExecutableInfo
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
$BuildOutputRoot = [IO.Path]::GetFullPath((Join-Path $ProjectRoot "build"))
$OutputCandidate = [IO.Path]::GetFullPath($OutputRoot)
$BuildOutputPrefix = $BuildOutputRoot.TrimEnd([char[]]@('\', '/')) +
    [IO.Path]::DirectorySeparatorChar
if ($OutputCandidate -ne $BuildOutputRoot -and
    !$OutputCandidate.StartsWith($BuildOutputPrefix,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw "Proof output must remain under the ignored build directory: $BuildOutputRoot"
}
if ($OutputCandidate -ne $OutputRoot) { $OutputRoot = $OutputCandidate }
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
$IncompleteMarker = Join-Path $OutputRoot "proof-incomplete.marker"
[IO.File]::WriteAllText(
    $IncompleteMarker,
    "This proof is invalid until the generator removes this marker.`r`n",
    $Utf8NoBom)

if ((Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash -ne
    $ExpectedRomSha256) {
    throw "Tip-off proof requires the supported Rev1 ROM fingerprint."
}

$BuildLog = Join-Path $LogsRoot "build.log"
$BuildCommandArguments = @(
    "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $BuildScript)
$BuildCommand = '$env:TECMO_SKIP_SHORTCUT="1"; ' +
    (Format-CommandLine -File "powershell.exe" `
        -Arguments $BuildCommandArguments)
if (!$SkipBuild) {
    $PreviousSkipShortcut =
        [Environment]::GetEnvironmentVariable("TECMO_SKIP_SHORTCUT")
    $BuildExitCode = 0
    try {
        $env:TECMO_SKIP_SHORTCUT = "1"
        & $BuildScript *> $BuildLog
        $BuildExitCode = $LASTEXITCODE
    } finally {
        if ($null -eq $PreviousSkipShortcut) {
            Remove-Item Env:TECMO_SKIP_SHORTCUT -ErrorAction SilentlyContinue
        } else {
            $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
        }
    }
    if ($BuildExitCode -ne 0) {
        throw "Warning-clean build failed.`n$((Get-Content -LiteralPath $BuildLog -Tail 50) -join [Environment]::NewLine)"
    }
} elseif (!(Test-Path -LiteralPath $Executable -PathType Leaf)) {
    throw "Native executable is missing. Omit -SkipBuild."
} else {
    [IO.File]::WriteAllText(
        $BuildLog,
        "Build skipped by explicit request; executable freshness was checked.`r`n$BuildCommand`r`n",
        $Utf8NoBom)
}
if (!(Test-Path -LiteralPath $Executable -PathType Leaf)) {
    throw "Native executable is missing after the requested build step."
}
$ExecutableItem = Assert-ExecutableFresh -Path $Executable
$ExecutableCanonicalPath = $ExecutableItem.FullName
$ExecutableSha256 =
    (Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash

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
        command = Format-CommandLine -File $Executable -Arguments $NativeArguments
    }
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

function Get-AssetPackPayload {
    param([byte[]]$Bytes, $Entry)
    if ($Entry.offset -gt [uint64][int]::MaxValue -or
        $Entry.size -gt [uint64][int]::MaxValue) {
        throw "Asset-pack entry '$($Entry.id)' is too large to inspect."
    }
    $Payload = New-Object byte[] ([int]$Entry.size)
    [Array]::Copy($Bytes, [int]$Entry.offset,
        $Payload, 0, $Payload.Length)
    return [pscustomobject]@{
        bytes = $Payload
        sha256 = Get-ByteArraySha256 -Bytes $Payload
        fnv1a32 = Get-Fnv32 -Bytes $Payload
    }
}

function Get-AssetPackIdentity {
    param([string]$Path)
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Asset pack is missing: $Path"
    }
    $ResolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $Bytes = [IO.File]::ReadAllBytes($ResolvedPath)
    if ($Bytes.Length -lt 40 -or
        [Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -ne "TAP1") {
        throw "Malformed asset-pack header."
    }
    if ([BitConverter]::ToUInt32($Bytes, 4) -ne 1 -or
        [BitConverter]::ToUInt32($Bytes, 8) -ne 40 -or
        [BitConverter]::ToUInt32($Bytes, 12) -ne 128) {
        throw "Unsupported asset-pack format/version/header."
    }
    $Count = [uint64][BitConverter]::ToUInt32($Bytes, 16)
    $Directory = [BitConverter]::ToUInt64($Bytes, 20)
    $DataOffset = [BitConverter]::ToUInt64($Bytes, 28)
    if ($Directory -lt [uint64]40 -or
        $Directory -gt [uint64]$Bytes.Length -or
        $DataOffset -gt [uint64]$Bytes.Length) {
        throw "Asset-pack directory/data offset is outside the pack."
    }
    $DirectoryBytes = $Count * [uint64]128
    if ($DirectoryBytes -gt ([uint64]$Bytes.Length - $Directory)) {
        throw "Asset-pack directory is outside the pack."
    }
    if ($DataOffset -gt $Directory) {
        throw "Asset-pack directory begins before its data region."
    }

    $RequiredIds = @(
        "system/manifest", "system/source-map", "gameplay/pre-tip", "chr/all")
    $Entries = @{}
    for ($Index = [uint64]0; $Index -lt $Count; ++$Index) {
        $EntryOffset64 = $Directory + $Index * [uint64]128
        if ($EntryOffset64 -gt [uint64][int]::MaxValue) {
            throw "Asset-pack directory entry offset is too large."
        }
        $EntryOffset = [int]$EntryOffset64
        $Name = [Text.Encoding]::ASCII.GetString(
            $Bytes, $EntryOffset, 64).Split([char]0)[0]
        if (!$Name -or $Entries.ContainsKey($Name)) {
            throw "Asset-pack directory contains an empty or duplicate entry ID."
        }
        $Offset = [BitConverter]::ToUInt64($Bytes, $EntryOffset + 84)
        $Size = [BitConverter]::ToUInt64($Bytes, $EntryOffset + 92)
        if ($Offset -lt $DataOffset -or $Offset -gt $Directory -or
            $Size -gt ($Directory - $Offset)) {
            throw "Asset-pack entry '$Name' is outside the pack."
        }
        $Entry = [pscustomobject][ordered]@{
            id = $Name
            type = [BitConverter]::ToUInt32($Bytes, $EntryOffset + 64)
            bank = [BitConverter]::ToUInt32($Bytes, $EntryOffset + 68)
            cpu_address = [BitConverter]::ToUInt32($Bytes, $EntryOffset + 72)
            source_offset = [BitConverter]::ToUInt64($Bytes, $EntryOffset + 76)
            offset = $Offset
            size = $Size
            flags = [BitConverter]::ToUInt32($Bytes, $EntryOffset + 100)
            sha256 = $null
            fnv1a32 = $null
        }
        if ($RequiredIds -contains $Name) {
            $Payload = Get-AssetPackPayload -Bytes $Bytes -Entry $Entry
            $Entry.sha256 = $Payload.sha256
            $Entry.fnv1a32 = $Payload.fnv1a32
        }
        $Entries[$Name] = $Entry
    }
    foreach ($RequiredId in $RequiredIds) {
        if (!$Entries.ContainsKey($RequiredId)) {
            throw "Asset pack is missing required canonical entry '$RequiredId'."
        }
    }

    $ManifestPayload = Get-AssetPackPayload -Bytes $Bytes `
        -Entry $Entries["system/manifest"]
    $ManifestText = [Text.Encoding]::UTF8.GetString($ManifestPayload.bytes)
    if ($ManifestText -notmatch '(?m)^format=tecmo\.assetpack/1$' -or
        $ManifestText -notmatch '(?m)^input_contract=ines-only$' -or
        $ManifestText -notmatch '(?m)^source_map=system/source-map$') {
        throw "Asset-pack system/manifest is missing the ROM-only identity contract."
    }
    $SourceMapPayload = Get-AssetPackPayload -Bytes $Bytes `
        -Entry $Entries["system/source-map"]
    $SourceMapText = [Text.Encoding]::UTF8.GetString($SourceMapPayload.bytes).TrimStart([char]0xFEFF)
    try {
        $SourceMap = $SourceMapText | ConvertFrom-Json
    } catch {
        throw "Asset-pack system/source-map is not valid JSON: $($_.Exception.Message)"
    }
    $MappedTip = @($SourceMap.logical_entries | Where-Object {
        $_.id -eq "gameplay/pre-tip"
    })
    if ($SourceMap.format -ne "tecmo.assetpack.source-map/1" -or
        $SourceMap.input_contract -ne "ines-only" -or
        $MappedTip.Count -ne 1 -or
        $MappedTip[0].schema -ne "tecmo.gameplay-pre-tip/TPTI-1") {
        throw "Asset-pack source-map is missing the canonical TPTI-1 provenance."
    }
    $TptiPayload = Get-AssetPackPayload -Bytes $Bytes `
        -Entry $Entries["gameplay/pre-tip"]
    if ($TptiPayload.bytes.Length -lt 6 -or
        [Text.Encoding]::ASCII.GetString($TptiPayload.bytes, 0, 4) -ne "TPTI" -or
        [BitConverter]::ToUInt16($TptiPayload.bytes, 4) -ne 1) {
        throw "Canonical gameplay/pre-tip is not a TPTI-1 payload."
    }
    $RequiredEntries = [ordered]@{}
    foreach ($RequiredId in $RequiredIds) {
        $Entry = $Entries[$RequiredId]
        $RequiredEntries[$RequiredId] = [pscustomobject][ordered]@{
            bytes = $Entry.size
            sha256 = $Entry.sha256
            fnv1a32 = $Entry.fnv1a32
        }
    }
    return [pscustomobject][ordered]@{
        canonical_path = $ResolvedPath
        sha256 = (Get-FileHash -LiteralPath $ResolvedPath -Algorithm SHA256).Hash
        bytes = $Bytes.Length
        format = "TAP1/v1"
        entry_count = $Count
        directory_offset = $Directory
        data_offset = $DataOffset
        required_entries = $RequiredEntries
        tpti_source_map = $MappedTip[0]
    }
}

$PackPath = Join-Path $OutputRoot "tecmo.assetpack"
$PackBuildStartedUtc = [DateTime]::UtcNow
$PackRun = Invoke-Native -NativeArguments @(
    "--build-assetpack", $RomPath, $PackPath) `
    -LogPath (Join-Path $LogsRoot "asset-pack.log")
if ($PackRun.code -ne 0 -or !(Test-Path -LiteralPath $PackPath -PathType Leaf)) {
    throw "Asset-pack build failed.`n$($PackRun.text)"
}
$PackItem = Get-Item -LiteralPath $PackPath
$PackIdentity = Get-AssetPackIdentity -Path $PackPath
if ($PackItem.LastWriteTimeUtc -lt $PackBuildStartedUtc) {
    throw "Asset pack is stale; the native build did not replace the requested output."
}
$TptiIdentity = $PackIdentity.required_entries["gameplay/pre-tip"]
if ([int]$TptiIdentity.bytes -ne $ExpectedTptiBytes) {
    throw "TPTI payload length changed: $($TptiIdentity.bytes)."
}
$TptiPayloadLength = [int]$TptiIdentity.bytes
$TptiFnv32 = $TptiIdentity.fnv1a32
if ($TptiFnv32 -ne $ExpectedTptiFnv32) {
    throw "TPTI payload fingerprint changed: $TptiFnv32."
}
$ChrIdentity = $PackIdentity.required_entries["chr/all"]
if ([int]$ChrIdentity.bytes -ne 262144 -or
    $ChrIdentity.fnv1a32 -ne "F6F6E854") {
    throw "Canonical chr/all asset identity changed."
}
$PackSha256 = $PackIdentity.sha256

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

function Assert-ContiguousFrameFiles {
    param([string]$Root, [string]$Label)
    $Files = @(Get-ChildItem -LiteralPath $Root -Filter "tipoff-*.png" -File)
    if ($Files.Count -ne $ProofFrameCount) {
        throw "$Label contains $($Files.Count) tip-off PNGs; expected $ProofFrameCount."
    }
    for ($Index = 0; $Index -lt $ProofFrameCount; ++$Index) {
        $ExpectedFrame = $ProofFirstFrame + $Index
        $ExpectedName = "tipoff-{0:D4}.png" -f $ExpectedFrame
        $Matches = @($Files | Where-Object Name -eq $ExpectedName)
        if ($Matches.Count -ne 1) {
            throw "$Label is missing exactly one numbered frame: $ExpectedName"
        }
        $Inspection = Get-FrameInspection -Path $Matches[0].FullName
        if ($Inspection.width -ne $OutputWidth -or
            $Inspection.height -ne $OutputHeight) {
            throw "$Label frame $ExpectedFrame has inconsistent PNG dimensions."
        }
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
$FirstPassCommands = @()
$SecondPassCommands = @()
$FacingCommands = @()
try {
    for ($Frame = $ProofFirstFrame; $Frame -le $ProofLastFrame; ++$Frame) {
        $Name = "tipoff-{0:D4}.png" -f $Frame
        $Path = Join-Path $FramesRoot $Name
        $Mode = "gameplay-tipoff-proof-frame$Frame"
        $LogPath = Join-Path $LogsRoot ("tipoff-{0:D4}-pass1.log" -f $Frame)
        $Run = Invoke-Native -NativeArguments @(
            "--root", $ProjectRoot, "--render-test-mode", $Mode, $Path) `
            -LogPath $LogPath
        $FirstPassCommands += [pscustomobject][ordered]@{
            frame = $Frame
            mode = $Mode
            output = $Path
            log = $LogPath
            command = $Run.command
        }
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
            log = $LogPath
            command = $Run.command
            sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
            bytes = (Get-Item -LiteralPath $Path).Length
            width = $Inspection.width
            height = $Inspection.height
            left_margin_nonblack_pixels =
                $Inspection.left_margin_nonblack_pixels
            right_margin_nonblack_pixels =
                $Inspection.right_margin_nonblack_pixels
            determinism_pass_2_sha256 = $null
            runtime = $Diagnostic
        }
    }

    if ($ProofFrames.Count -ne $ProofFrameCount) {
        throw "Tip-off proof did not produce exactly $ProofFrameCount frames."
    }
    for ($Index = 0; $Index -lt $ProofFrameCount; ++$Index) {
        $ExpectedFrame = $ProofFirstFrame + $Index
        if ([int]$ProofFrames[$Index].frame -ne $ExpectedFrame) {
            throw "Tip-off proof frame sequence is not contiguous at index $Index."
        }
    }
    Assert-ContiguousFrameFiles -Root $FramesRoot -Label "First-pass frame directory"

    foreach ($Proof in $ProofFrames) {
        $SecondPath = Join-Path $VerifyRoot (
            "tipoff-{0:D4}.png" -f $Proof.frame)
        $SecondLogPath = Join-Path $LogsRoot (
            "tipoff-{0:D4}-pass2.log" -f $Proof.frame)
        $SecondRun = Invoke-Native -NativeArguments @(
            "--root", $ProjectRoot, "--render-test-mode", $Proof.mode,
            $SecondPath) -LogPath $SecondLogPath
        $SecondPassCommands += [pscustomobject][ordered]@{
            frame = $Proof.frame
            mode = $Proof.mode
            output = $SecondPath
            log = $SecondLogPath
            command = $SecondRun.command
        }
        $SecondDiagnostic = @($SecondRun.lines | Where-Object {
            $_ -match "^tipoff-proof "
        })
        if ($SecondRun.code -ne 0 -or $SecondDiagnostic.Count -ne 1 -or
            !(Test-Path -LiteralPath $SecondPath -PathType Leaf)) {
            throw "Tip-off frame $($Proof.frame) was nondeterministic."
        }
        $SecondDiagnosticObject = Convert-TipoffDiagnostic -Line $SecondDiagnostic[0]
        Assert-TipoffDiagnostic -Frame $Proof.frame `
            -Diagnostic $SecondDiagnosticObject
        $SecondHash =
            (Get-FileHash -LiteralPath $SecondPath -Algorithm SHA256).Hash
        if ($SecondHash -ne $Proof.sha256) {
            throw "Tip-off frame $($Proof.frame) was nondeterministic."
        }
        $Proof.determinism_pass_2_sha256 = $SecondHash
    }
    Assert-ContiguousFrameFiles -Root $VerifyRoot -Label "Second-pass frame directory"

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
    $FacingLogPath = Join-Path $LogsRoot "away-left-facing-pass1.log"
    $FacingVerifyLogPath = Join-Path $LogsRoot "away-left-facing-pass2.log"
    $FacingRun = Invoke-Native -NativeArguments @(
        "--root", $ProjectRoot, "--render-test-mode", $FacingMode,
        $FacingPath) -LogPath $FacingLogPath
    $FacingVerifyRun = Invoke-Native -NativeArguments @(
        "--root", $ProjectRoot, "--render-test-mode", $FacingMode,
        $FacingVerifyPath) -LogPath $FacingVerifyLogPath
    $FacingCommands += [pscustomobject][ordered]@{
        pass = 1
        mode = $FacingMode
        output = $FacingPath
        log = $FacingLogPath
        command = $FacingRun.command
    }
    $FacingCommands += [pscustomobject][ordered]@{
        pass = 2
        mode = $FacingMode
        output = $FacingVerifyPath
        log = $FacingVerifyLogPath
        command = $FacingVerifyRun.command
    }
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
    $FacingVerifyInspection = Get-FrameInspection -Path $FacingVerifyPath
    if ($FacingVerifyInspection.width -ne $OutputWidth -or
        $FacingVerifyInspection.height -ne $OutputHeight) {
        throw "Away-left facing determinism frame dimensions changed."
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
    $HeaderHeight = 68
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
            "TIP-OFF CONTACT SHEET | logical frames 661-695 | 35 contiguous",
            $TitleFont, [Drawing.Brushes]::White, 8, 8)
        $Graphics.DrawString(
            "native cadence $NativeFrameRateText (~$([Math]::Round($NativeFrameRateHz, 6)) fps) | MP4 presentation only; PNG/diagnostics are acceptance evidence",
            $Font, [Drawing.Brushes]::White, 8, 38)
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
    $HeaderHeight = 68
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
            "TIP-OFF $($Side.ToUpperInvariant()) EDGE | every logical frame 661-695",
            $TitleFont, [Drawing.Brushes]::White, 8, 8)
        $Graphics.DrawString(
            "native cadence $NativeFrameRateText (~$([Math]::Round($NativeFrameRateHz, 6)) fps) | contiguous PNG edge evidence",
            $Font, [Drawing.Brushes]::White, 8, 38)
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

function Invoke-External {
    param(
        [string]$FilePath,
        [string]$Arguments,
        [string]$LogPath
    )
    $StartInfo = New-Object Diagnostics.ProcessStartInfo
    $StartInfo.FileName = $FilePath
    $StartInfo.Arguments = $Arguments
    $StartInfo.WorkingDirectory = $ProjectRoot
    $StartInfo.UseShellExecute = $false
    $StartInfo.CreateNoWindow = $true
    $StartInfo.RedirectStandardOutput = $true
    $StartInfo.RedirectStandardError = $true
    $Process = New-Object Diagnostics.Process
    $Process.StartInfo = $StartInfo
    try {
        [void]$Process.Start()
        $Output = $Process.StandardOutput.ReadToEnd()
        $ErrorOutput = $Process.StandardError.ReadToEnd()
        $Process.WaitForExit()
        $ExitCode = $Process.ExitCode
    } finally {
        $Process.Dispose()
    }
    $Text = $Output + $ErrorOutput
    [IO.File]::WriteAllText($LogPath, $Text, $Utf8NoBom)
    return [pscustomobject]@{
        code = $ExitCode
        text = $Text
    }
}

function Get-ValidatedRational {
    param(
        [string]$Actual,
        [string]$FieldName
    )
    if ($Actual -notmatch "^(\d+)/(\d+)$") {
        throw "$FieldName did not report a rational frame rate: '$Actual'."
    }
    [int64]$ActualNumerator = $Matches[1]
    [int64]$ActualDenominator = $Matches[2]
    if ($ActualDenominator -le 0 -or
        ([decimal]$ActualNumerator * [decimal]$NativeFrameRateDenominator) -ne
        ([decimal]$NativeFrameRateNumerator * [decimal]$ActualDenominator)) {
        throw "$FieldName '$Actual' is not the native cadence $NativeFrameRateText."
    }
    return [pscustomobject]@{
        text = $Actual
        numerator = $ActualNumerator
        denominator = $ActualDenominator
    }
}

$FfmpegPath = Resolve-ToolPath -Candidate $FfmpegPath -Name "ffmpeg"
$FfprobeSibling = Join-Path (Split-Path -Parent $FfmpegPath) "ffprobe.exe"
if (!(Test-Path -LiteralPath $FfprobeSibling -PathType Leaf)) {
    $FfprobeSibling = $null
}
$FfprobePath = Resolve-ToolPath -Candidate $FfprobeSibling -Name "ffprobe"
$FfmpegSha256 = (Get-FileHash -LiteralPath $FfmpegPath -Algorithm SHA256).Hash
$FfprobeSha256 = (Get-FileHash -LiteralPath $FfprobePath -Algorithm SHA256).Hash
$FfmpegVersionLog = Join-Path $LogsRoot "ffmpeg-version.log"
$FfprobeVersionLog = Join-Path $LogsRoot "ffprobe-version.log"
$FfmpegVersionLines = @(& $FfmpegPath -hide_banner -version 2>&1 |
    ForEach-Object { $_.ToString() })
$FfmpegVersionExitCode = $LASTEXITCODE
$FfprobeVersionLines = @(& $FfprobePath -hide_banner -version 2>&1 |
    ForEach-Object { $_.ToString() })
$FfprobeVersionExitCode = $LASTEXITCODE
[IO.File]::WriteAllLines($FfmpegVersionLog,
    [string[]]$FfmpegVersionLines, $Utf8NoBom)
[IO.File]::WriteAllLines($FfprobeVersionLog,
    [string[]]$FfprobeVersionLines, $Utf8NoBom)
if ($FfmpegVersionExitCode -ne 0 -or $FfprobeVersionExitCode -ne 0) {
    throw "ffmpeg/ffprobe version query failed; proof tooling is incomplete."
}
$FfmpegVersionMatches = @($FfmpegVersionLines | Where-Object {
    $_ -match '^ffmpeg version '
})
$FfprobeVersionMatches = @($FfprobeVersionLines | Where-Object {
    $_ -match '^ffprobe version '
})
if ($FfmpegVersionMatches.Count -ne 1 -or
    $FfprobeVersionMatches.Count -ne 1) {
    throw "ffmpeg/ffprobe version output was not identifiable."
}
$FfmpegVersion = $FfmpegVersionMatches[0]
$FfprobeVersion = $FfprobeVersionMatches[0]
$VideoPath = Join-Path $OutputRoot "tipoff-sequence-661-695.mp4"
$InputPattern = Join-Path $FramesRoot "tipoff-%04d.png"
$FfmpegLog = Join-Path $LogsRoot "ffmpeg.log"
$FfprobeLog = Join-Path $LogsRoot "ffprobe.log"
$FfmpegArguments =
    "-hide_banner -loglevel info -y " +
    "-framerate $NativeFrameRateText " +
    "-start_number $ProofFirstFrame -i `"$InputPattern`" " +
    "-frames:v $ProofFrameCount -fps_mode passthrough " +
    "-c:v libx264 -preset medium -crf 18 -pix_fmt yuv420p " +
    "-video_track_timescale $NativeFrameRateNumerator " +
    "`"$VideoPath`""
$FfmpegCommand = "`"$FfmpegPath`" $FfmpegArguments"
$FfmpegRun = Invoke-External -FilePath $FfmpegPath `
    -Arguments $FfmpegArguments -LogPath $FfmpegLog
if ($FfmpegRun.code -ne 0 -or
    !(Test-Path -LiteralPath $VideoPath -PathType Leaf)) {
    throw "ffmpeg proof encoding failed.`n$((Get-Content -LiteralPath $FfmpegLog -Tail 50) -join [Environment]::NewLine)"
}

$FfprobeArguments =
    "-v error -select_streams v:0 -count_frames " +
    "-show_entries stream=width,height,nb_read_frames,avg_frame_rate," +
    "r_frame_rate,duration -of json `"$VideoPath`""
$FfprobeCommand = "`"$FfprobePath`" $FfprobeArguments"
$FfprobeRun = Invoke-External -FilePath $FfprobePath `
    -Arguments $FfprobeArguments -LogPath $FfprobeLog
if ($FfprobeRun.code -ne 0) {
    throw "ffprobe proof validation failed.`n$($FfprobeRun.text)"
}
try {
    $FfprobeJson = $FfprobeRun.text | ConvertFrom-Json
} catch {
    throw "ffprobe did not return valid JSON: $($_.Exception.Message)"
}
$FfprobeStreams = @($FfprobeJson.streams)
if ($FfprobeStreams.Count -ne 1) {
    throw "ffprobe did not return exactly one video stream."
}
$VideoStream = $FfprobeStreams[0]
if ([int]$VideoStream.width -ne $OutputWidth -or
    [int]$VideoStream.height -ne $OutputHeight -or
    [int64]$VideoStream.nb_read_frames -ne [int64]$ProofFrameCount) {
    throw "Encoded proof dimensions/frame count are not ${OutputWidth}x${OutputHeight}/$ProofFrameCount."
}
$VideoAverageRate = Get-ValidatedRational `
    -Actual ([string]$VideoStream.avg_frame_rate) `
    -FieldName "ffprobe avg_frame_rate"
$VideoNominalRate = Get-ValidatedRational `
    -Actual ([string]$VideoStream.r_frame_rate) `
    -FieldName "ffprobe r_frame_rate"
if ([string]::IsNullOrWhiteSpace([string]$VideoStream.duration)) {
    throw "ffprobe did not report an encoded video duration."
}
$VideoDurationSeconds = [double]::Parse(
    [string]$VideoStream.duration,
    [Globalization.CultureInfo]::InvariantCulture)
$ExpectedVideoDurationSeconds =
    [double]$ProofFrameCount / $NativeFrameRateHz
if ([Math]::Abs($VideoDurationSeconds - $ExpectedVideoDurationSeconds) -gt
    ($NativeFrameDurationSeconds + 0.001)) {
    throw "Encoded proof duration is inconsistent with the native cadence."
}
$NativeFrameRateDisplay = $NativeFrameRateHz.ToString(
    "0.000000", [Globalization.CultureInfo]::InvariantCulture)
$VideoStatus =
    "encoded $ProofFrameCount contiguous native frames at $NativeFrameRateText fps ($NativeFrameRateDisplay Hz)"

$ScriptHash = (Get-FileHash -LiteralPath $ScriptPath -Algorithm SHA256).Hash
$CheckpointSourcePath = Join-Path $ProjectRoot `
    "src\tecmo_cli_render_gameplay_checkpoint.c"
if (!(Test-Path -LiteralPath $CheckpointSourcePath -PathType Leaf)) {
    throw "Deterministic tip-off input source is missing: $CheckpointSourcePath"
}
$CheckpointSourcePath = (Resolve-Path -LiteralPath $CheckpointSourcePath).Path
$CheckpointSourceSha256 =
    (Get-FileHash -LiteralPath $CheckpointSourcePath -Algorithm SHA256).Hash
$ShortcutScriptPath = Join-Path $ProjectRoot "tools\Update-DesktopShortcut.ps1"
if (!(Test-Path -LiteralPath $ShortcutScriptPath -PathType Leaf)) {
    throw "Shortcut audit input is missing: $ShortcutScriptPath"
}
$ShortcutScriptPath = (Resolve-Path -LiteralPath $ShortcutScriptPath).Path
$ShortcutScriptSha256 =
    (Get-FileHash -LiteralPath $ShortcutScriptPath -Algorithm SHA256).Hash
$AssetPackCommand = $PackRun.command
$GenerationCommand =
    Format-CommandLine -File "powershell.exe" -Arguments @(
        "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $ScriptPath,
        "-ProjectRoot", $ProjectRoot, "-RomPath", $RomPath,
        "-OutputRoot", $OutputRoot)
if ($FfmpegPath) {
    $GenerationCommand += " -FfmpegPath `"$FfmpegPath`""
}
if ($SkipBuild) { $GenerationCommand += " -SkipBuild" }
$GeneratedUtc = [DateTime]::UtcNow.ToString("o")

$ArtifactPaths = @(
    $Executable,
    $PackPath,
    $ContactSheetPath,
    $LeftEdgeSheetPath,
    $RightEdgeSheetPath,
    $FacingPath,
    $BuildLog,
    $FfmpegLog,
    $FfprobeLog,
    $FfmpegVersionLog,
    $FfprobeVersionLog
)
if ($VideoPath) { $ArtifactPaths += $VideoPath }
$Artifacts = @($ArtifactPaths | ForEach-Object {
    if (!(Test-Path -LiteralPath $_ -PathType Leaf)) {
        throw "Expected proof artifact is missing: $_"
    }
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
    "executable: $ExecutableCanonicalPath",
    "executable SHA256: $ExecutableSha256 ($($ExecutableItem.Length) bytes; stale-input check passed)",
    "asset pack: $($PackIdentity.canonical_path)",
    "asset pack SHA256: $PackSha256 ($($PackItem.Length) bytes; $($PackIdentity.entry_count) entries)",
    "asset pack canonical entries: gameplay/pre-tip=$TptiPayloadLength bytes/$TptiFnv32; chr/all=$($ChrIdentity.bytes) bytes/$($ChrIdentity.fnv1a32)",
    "frames: $ProofFirstFrame-$ProofLastFrame ($ProofFrameCount contiguous frames, deterministic double render)",
    "native cadence: $NativeFrameRateText fps ($NativeFrameRateDisplay Hz; frame period $NativeFrameDurationSeconds seconds)",
    "input: P1 controls Away; held B on every production update while phase is jump-contest; source=$CheckpointSourcePath",
    "input source SHA256: $CheckpointSourceSha256",
    "shortcut audit: $ShortcutScriptPath SHA256=$ShortcutScriptSha256; GUI launch is tecmo_port_game.exe --root <project> --play; no native capture option",
    "output: 640x480; active view x=$ActiveLeft..$ActiveRight; both host margins verified black",
    "contact sheet: $ContactSheetPath",
    "left edge sheet: $LeftEdgeSheetPath",
    "right edge sheet: $RightEdgeSheetPath",
    "away-left facing: $FacingPath",
    "video: $VideoPath",
    "video status: $VideoStatus; presentation artifact only; never acceptance proof",
    "approximation/evidence: each PNG is a production-path CLI checkpoint replayed from a clean scene launch to its logical frame; the CLI does not capture wall-clock Win32 frames",
    "approximation/evidence: PNG numbering, runtime diagnostics, both edge sheets, and deterministic pass 2 are acceptance evidence; MP4 only presents the same contiguous frames at the exact rational cadence",
    "approximation/evidence: later pre-tip trajectory/winner policy remains capture-bounded/native-approximate per PORTING.md; this proof does not claim ROM-exact timing",
    "command build: $BuildCommand",
    "command asset-pack: $AssetPackCommand",
    "command ffmpeg: $FfmpegCommand",
    "command ffprobe: $FfprobeCommand",
    "generation command: $GenerationCommand"
)
[IO.File]::WriteAllLines($SummaryPath, [string[]]$SummaryLines, $Utf8NoBom)
$Artifacts += [pscustomobject][ordered]@{
    path = $SummaryPath
    sha256 = (Get-FileHash -LiteralPath $SummaryPath -Algorithm SHA256).Hash
    bytes = (Get-Item -LiteralPath $SummaryPath).Length
}

$Manifest = [pscustomobject][ordered]@{
    schema = $ProofSchema
    generated_utc = $GeneratedUtc
    repository = [pscustomobject][ordered]@{
        worktree = $ProjectRoot
        branch = $Branch
        commit = $Commit
        clean_worktree_required = $true
    }
    executable = [pscustomobject][ordered]@{
        path = $ExecutableCanonicalPath
        sha256 = $ExecutableSha256
        bytes = $ExecutableItem.Length
        freshness_check = "tracked src/include/build.ps1/CMakeLists.txt inputs are not newer than this executable"
        capture_surface = "tecmo_port.exe --render-test-mode"
    }
    source = [pscustomobject][ordered]@{
        rom_revision = "Tecmo NBA Basketball (USA) Rev 1"
        rom_path = $RomPath
        rom_sha256 = $ExpectedRomSha256
        rom_or_payload_committed = $false
        tpti_payload_bytes = $TptiPayloadLength
        tpti_payload_fnv1a32 = $TptiFnv32
    }
    asset_pack = [pscustomobject][ordered]@{
        canonical_path = $PackIdentity.canonical_path
        sha256 = $PackSha256
        bytes = $PackItem.Length
        format = $PackIdentity.format
        entry_count = $PackIdentity.entry_count
        required_entries = $PackIdentity.required_entries
        source_map_tpti = $PackIdentity.tpti_source_map
    }
    render = [pscustomobject][ordered]@{
        width = $OutputWidth
        height = $OutputHeight
        active_view_x = "$ActiveLeft..$ActiveRight"
        active_view_y = "0..479"
        native_scale = 2
        first_frame = $ProofFirstFrame
        last_frame = $ProofLastFrame
        logical_frame_range = [pscustomobject][ordered]@{
            first = $ProofFirstFrame
            last = $ProofLastFrame
            count = $ProofFrameCount
        }
        contiguous_frame_count = $ProofFrameCount
        native_frame_rate = $NativeFrameRateText
        native_frame_rate_hz = $NativeFrameRateHz
        native_frame_duration_seconds = $NativeFrameDurationSeconds
        production_path = "TecmoGameplayScene launch/update/render via gameplay-tipoff-proof-frameN"
        capture_model = "logical-frame CLI checkpoint replay; not wall-clock Win32 capture"
        deterministic_passes = 2
        left_host_margin_nonblack_pixels = 0
        right_host_margin_nonblack_pixels = 0
    }
    input_script = [pscustomobject][ordered]@{
        generator_path = $ScriptPath
        generator_sha256 = $ScriptHash
        native_checkpoint_source_path = $CheckpointSourcePath
        native_checkpoint_source_sha256 = $CheckpointSourceSha256
        controller_1_team = "Away"
        schedule = "neutral before contest; held B via input.cancel on every production update while phase is jump-contest; neutral after live handoff"
        input_semantics = "P1/Away receives the held B/cancel input; Home is not sampled; frame N is reached by replaying updates 0..N-1 from a clean scene launch"
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
        "both frame directories contain exactly tipoff-0661.png through tipoff-0695.png with consistent 640x480 dimensions",
        "all output pixels outside active view x=64..575 are black on both edges",
        "gameplay-facing-away-left checkpoint validates active actors against team goal mapping",
        "MP4 frame count/rate/dimensions are ffprobe-validated and the MP4 is not an acceptance artifact"
    )
    frames = $ProofFrames
    facing_checkpoint = [pscustomobject][ordered]@{
        mode = $FacingMode
        path = $FacingPath
        verify_path = $FacingVerifyPath
        sha256 = (Get-FileHash -LiteralPath $FacingPath -Algorithm SHA256).Hash
        deterministic_passes = 2
        validated_contract = "fresh TGOR: Away attacks/faces left; Home attacks/faces right; explicit action overrides remain authoritative"
    }
    video = [pscustomobject][ordered]@{
        status = $VideoStatus
        path = $VideoPath
        acceptance_proof = $false
        presentation_only = $true
        sha256 = (Get-FileHash -LiteralPath $VideoPath -Algorithm SHA256).Hash
        bytes = (Get-Item -LiteralPath $VideoPath).Length
        width = [int]$VideoStream.width
        height = [int]$VideoStream.height
        frame_rate = $NativeFrameRateText
        frame_rate_hz = $NativeFrameRateHz
        frame_rate_denominator = $NativeFrameRateDenominator
        frame_rate_numerator = $NativeFrameRateNumerator
        ffprobe_avg_frame_rate = $VideoAverageRate.text
        ffprobe_r_frame_rate = $VideoNominalRate.text
        frame_count = $ProofFrameCount
        ffprobe_frame_count = [int64]$VideoStream.nb_read_frames
        duration_seconds = $VideoDurationSeconds
        expected_duration_seconds = $ExpectedVideoDurationSeconds
        ffmpeg_path = $FfmpegPath
        ffmpeg_sha256 = $FfmpegSha256
        ffmpeg_version = $FfmpegVersion
        ffprobe_path = $FfprobePath
        ffprobe_sha256 = $FfprobeSha256
        ffprobe_version = $FfprobeVersion
    }
    evidence = [pscustomobject][ordered]@{
        acceptance_artifacts = @(
            "first-pass contiguous PNG frame set",
            "tipoff-proof runtime diagnostics in per-frame logs",
            "determinism-pass-2 PNG frame set",
            "both active-view edge contact sheets",
            "away-left live-facing checkpoint")
        presentation_artifacts = @("MP4 encoded at the exact native cadence")
        video_acceptance_proof = $false
        approximation_notes = @(
            "The CLI has no one-process or wall-clock capture API; each logical frame is replayed through the production scene path from a clean launch.",
            "The PNG sequence is contiguous by logical frame number and deterministic across two independent process runs; its timestamps are not a claim about capture wall time.",
            "The Win32 shortcut launches tecmo_port_game.exe --root <project> --play and has no proof capture shortcut; it is audited but not used by this proof.",
            "PORTING.md classifies later pre-tip trajectory/winner behavior as capture-bounded/native-approximate; this proof records that limitation.")
        shortcut_capture_audit = [pscustomobject][ordered]@{
            script_path = $ShortcutScriptPath
            script_sha256 = $ShortcutScriptSha256
            launch_arguments = "--root <project> --play"
            capture_option = $false
            proof_capture_surface = "tecmo_port.exe --render-test-mode gameplay-tipoff-proof-frameN"
        }
    }
    artifacts = $Artifacts
    commands = [pscustomobject][ordered]@{
        build = $BuildCommand
        generation = $GenerationCommand
        asset_pack = $AssetPackCommand
        render_pass_1 = $FirstPassCommands
        render_pass_2 = $SecondPassCommands
        facing = $FacingCommands
        ffmpeg = $FfmpegCommand
        ffprobe = $FfprobeCommand
        ffmpeg_version = (Format-CommandLine -File $FfmpegPath -Arguments @("-hide_banner", "-version"))
        ffprobe_version = (Format-CommandLine -File $FfprobePath -Arguments @("-hide_banner", "-version"))
    }
    generation_command = $GenerationCommand
}
$ManifestPath = Join-Path $OutputRoot "proof-manifest.json"
$ManifestJson = $Manifest | ConvertTo-Json -Depth 12
[IO.File]::WriteAllText($ManifestPath, $ManifestJson + "`r`n", $Utf8NoBom)
Remove-Item -LiteralPath $IncompleteMarker -Force

Write-Output "TIPOFF VISUAL PROOF PASS"
Write-Output "manifest=$ManifestPath"
Write-Output "contact-sheet=$ContactSheetPath"
Write-Output "left-edge-sheet=$LeftEdgeSheetPath"
Write-Output "right-edge-sheet=$RightEdgeSheetPath"
Write-Output "facing=$FacingPath"
Write-Output "video=$VideoPath"
Write-Output "frames=$FramesRoot"
