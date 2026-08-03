[CmdletBinding()]
param(
    [string]$RomPath,
    [string]$FceuxPath,
    [switch]$Build,
    [switch]$Visible,
    [switch]$RequirePass,
    [switch]$RequireVideo,
    [string]$FfmpegPath,
    [string]$FfprobePath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ExpectedRomSha256 = '076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4'
$ExpectedFceuxSha256 = 'F89812F4E9506EF7090D9D0310D368ABD79BACA362B7BFC4A2E7E499754F2A1B'
$ExpectedBaseSha = '6d8f9c7a99a7ce188f1a523247d3a9b9093860fb'
$ExpectedTgaiBytes = 7616
$ExpectedTgaiFnv1a32 = 'D6C4DB35'
$ReferenceFrameCount = 120
$ReferenceScreenshotCount = 12
$ReferenceWidth = 256
$ReferenceHeight = 224
$ReferenceSheetWidth = 768
$ReferenceSheetHeight = 896
$NativeFirstFrame = 25
$NativeFrameCount = 12
$NativeWidth = 640
$NativeHeight = 480
$NativeSheetWidth = 1920
$NativeSheetHeight = 1920
$NativeFrameRate = '39375000/655171'
$NativeVideoTrackTimescale = 39375000
$NativeVideoTimeBase = '1/39375000'

function Get-ShortTail {
    param([object[]]$Lines)
    return (@($Lines | Select-Object -Last 16) -join [Environment]::NewLine)
}
function Get-SanitizedCommand {
    param([string]$Command, [string[]]$Arguments)
    $Text = ((@($Command) + @($Arguments)) -join ' ')
    foreach ($Pair in @(
        @{ value = $ProjectRoot; replacement = '[PROJECT]' },
        @{ value = $RomPath; replacement = '[LOCAL_REV1_ROM]' },
        @{ value = $FceuxPath; replacement = '[LOCAL_FCEUX]' },
        @{ value = $OutputRoot; replacement = '[IGNORED_PROOF_OUTPUT]' },
        @{ value = $PackPath; replacement = '[IGNORED_PROOF_PACK]' }
    )) {
        if ($Pair.value) { $Text = $Text.Replace([string]$Pair.value, $Pair.replacement) }
    }
    return $Text
}
function Add-ProcessLogMetadata {
    param(
        [string]$Path,
        [string]$Label,
        [string]$Command,
        [string]$StartUtc,
        [string]$EndUtc,
        [int]$ExitCode
    )
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) {
        Set-Content -LiteralPath $Path -Value '' -Encoding UTF8
    }
    $ExistingText = [IO.File]::ReadAllText($Path) -replace '^\uFEFF', ''
    $HasToolOutput = -not [string]::IsNullOrWhiteSpace($ExistingText)
    $Rows = [Collections.Generic.List[string]]::new()
    [void]$Rows.Add("[runner] label=$Label start_utc=$StartUtc command=$Command")
    if (-not $HasToolOutput) { [void]$Rows.Add("[runner] no stdout/stderr emitted; exit=$ExitCode") }
    [void]$Rows.Add("[runner] end_utc=$EndUtc exit=$ExitCode")
    if ($ExistingText.Length -gt 0 -and $ExistingText -notmatch '(\r\n|\r|\n)$') {
        [void]$Rows.Insert(0, '')
    }
    $Rows | Add-Content -LiteralPath $Path -Encoding UTF8
}
function Get-ProgressSnapshot {
    param([string]$Path)
    $Stream = $null
    $Reader = $null
    try {
        $Stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read,
            ([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
        $Reader = [IO.StreamReader]::new($Stream, [Text.Encoding]::UTF8, $true)
        $Buffer = New-Object char[] 4096
        $Builder = [Text.StringBuilder]::new()
        [int]$CharCount = 0
        while (($Read = $Reader.Read($Buffer, 0, $Buffer.Length)) -gt 0) {
            $CharCount += $Read
            if ($CharCount -gt 65536) { return $null }
            [void]$Builder.Append($Buffer, 0, $Read)
        }
        $Text = $Builder.ToString()
    } catch {
        return $null
    } finally {
        if ($null -ne $Reader) { $Reader.Dispose() }
        elseif ($null -ne $Stream) { $Stream.Dispose() }
    }
    if ([string]::IsNullOrWhiteSpace($Text)) { return $null }
    $Values = [ordered]@{}
    foreach ($Line in [regex]::Split($Text, '\r?\n')) {
        if ([string]::IsNullOrWhiteSpace($Line)) { continue }
        if ($Line -notmatch '^([a-z0-9_]+)=(.*)$') { return $null }
        if ($Values.Contains($Matches[1])) { return $null }
        $Values[$Matches[1]] = $Matches[2]
    }
    foreach ($Required in @('schema','sequence','stage','captured_frames')) {
        if (-not $Values.Contains($Required)) { return $null }
    }
    return [pscustomobject]@{ values = $Values; marker = $Text }
}
function Get-Fnv1a32 {
    param([byte[]]$Bytes)
    [uint32]$Hash = 2166136261
    foreach ($Byte in $Bytes) {
        [uint64]$Product = [uint64]($Hash -bxor [uint32]$Byte) * [uint64]16777619
        $Hash = [uint32]($Product % [uint64]4294967296)
    }
    return ('{0:X8}' -f $Hash)
}
function Get-AssetPackEntry {
    param([byte[]]$Bytes, [string]$Id)
    if ($Bytes.Length -lt 40 -or
        [Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -ne 'TAP1' -or
        [BitConverter]::ToUInt32($Bytes, 4) -ne 1 -or
        [BitConverter]::ToUInt32($Bytes, 8) -ne 40 -or
        [BitConverter]::ToUInt32($Bytes, 12) -ne 128) {
        throw 'CPU lifecycle proof expected a TAP1/v1 asset pack.'
    }
    $Count = [BitConverter]::ToUInt32($Bytes, 16)
    $Directory = [BitConverter]::ToUInt64($Bytes, 20)
    if ($Directory -gt [uint64]$Bytes.Length -or
        [uint64]$Count * 128 -gt [uint64]$Bytes.Length - $Directory) {
        throw 'CPU lifecycle proof asset-pack directory is out of bounds.'
    }
    for ($Index = 0; $Index -lt $Count; ++$Index) {
        $Offset = [int]$Directory + $Index * 128
        $End = [Array]::IndexOf($Bytes, [byte]0, $Offset, 64)
        if ($End -lt 0) { $End = $Offset + 64 }
        if ([Text.Encoding]::ASCII.GetString($Bytes, $Offset, $End - $Offset) -ne $Id) { continue }
        return [pscustomobject]@{
            directory_offset = $Offset
            pack_offset = [BitConverter]::ToUInt64($Bytes, $Offset + 84)
            byte_count = [BitConverter]::ToUInt64($Bytes, $Offset + 92)
        }
    }
    throw "CPU lifecycle proof asset-pack entry '$Id' was not found."
}
function Get-EntryBytes {
    param([byte[]]$PackBytes, [object]$Entry)
    [uint64]$PackLength = $PackBytes.Length
    [uint64]$PackOffset = [uint64]$Entry.pack_offset
    [uint64]$ByteCount = [uint64]$Entry.byte_count
    if ($PackOffset -gt $PackLength -or $ByteCount -gt $PackLength - $PackOffset) {
        throw 'CPU lifecycle proof selected asset-pack entry offset/count is out of bounds.'
    }
    if ($ByteCount -gt [uint64][int]::MaxValue) {
        throw 'CPU lifecycle proof selected asset-pack entry is too large for validation.'
    }
    $Result = New-Object byte[] ([int]$ByteCount)
    [Array]::Copy($PackBytes, [int64]$PackOffset, $Result, 0, [int64]$ByteCount)
    return $Result
}
function Get-PngDimensions {
    param([string]$Path)
    $Bytes = [IO.File]::ReadAllBytes($Path)
    [byte[]]$Signature = 137,80,78,71,13,10,26,10
    if ($Bytes.Length -lt 24) { throw "PNG '$Path' is truncated." }
    for ($Index = 0; $Index -lt $Signature.Length; ++$Index) {
        if ($Bytes[$Index] -ne $Signature[$Index]) { throw "PNG '$Path' is invalid." }
    }
    if ([Text.Encoding]::ASCII.GetString($Bytes, 12, 4) -ne 'IHDR') {
        throw "PNG '$Path' has no IHDR."
    }
    [uint32]$Width = ([uint32]$Bytes[16] -shl 24) -bor
        ([uint32]$Bytes[17] -shl 16) -bor ([uint32]$Bytes[18] -shl 8) -bor
        [uint32]$Bytes[19]
    [uint32]$Height = ([uint32]$Bytes[20] -shl 24) -bor
        ([uint32]$Bytes[21] -shl 16) -bor ([uint32]$Bytes[22] -shl 8) -bor
        [uint32]$Bytes[23]
    return [pscustomobject]@{ width = $Width; height = $Height }
}
function Get-Status {
    param([string]$Path)
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing proof status: $Path" }
    $Status = [ordered]@{}
    foreach ($Line in Get-Content -LiteralPath $Path) {
        if ($Line -notmatch '^([a-z0-9_]+)=(.*)$') {
            throw "Malformed CPU lifecycle status row: $Line"
        }
        if ($Status.Contains($Matches[1])) { throw "Duplicate CPU lifecycle status key: $($Matches[1])" }
        $Status[$Matches[1]] = $Matches[2]
    }
    return $Status
}
function Get-FileFingerprint {
    param([string]$Directory, [string]$Pattern)
    $Rows = @()
    foreach ($Item in Get-ChildItem -LiteralPath $Directory -File -Filter $Pattern | Sort-Object Name) {
        $Hash = (Get-FileHash -LiteralPath $Item.FullName -Algorithm SHA256).Hash.ToUpperInvariant()
        if ($Item.Length -le 0 -or [string]::IsNullOrWhiteSpace($Hash)) {
            throw "Empty or unhashed artifact '$($Item.Name)'."
        }
        $Rows += '{0}={1}' -f $Item.Name, $Hash
    }
    if ($Rows.Count -eq 0) { throw "No files matched fingerprint pattern '$Pattern'." }
    $Text = $Rows -join "`n"
    $Hash = [Security.Cryptography.SHA256]::Create()
    try {
        return [BitConverter]::ToString($Hash.ComputeHash(
            [Text.Encoding]::UTF8.GetBytes($Text))).Replace('-', '').ToUpperInvariant()
    } finally { $Hash.Dispose() }
}
function Get-ToolInfo {
    param([string]$Path, [string]$Arguments = '-version')
    if (-not $Path -or !(Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
    $Lines = @(& $Path $Arguments 2>&1)
    return [ordered]@{
        path = '<LOCAL_TOOL>'
        sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
        version = (($Lines | Select-Object -First 1) -join '').Trim()
    }
}
function Invoke-Logged {
    param([string]$Command, [string[]]$Arguments, [string]$LogPath, [string]$Label)
    $StartUtc = (Get-Date).ToUniversalTime().ToString('o')
    $Sanitized = Get-SanitizedCommand $Command $Arguments
    $Raw = @()
    $Code = 1
    $InvocationError = $null
    $PreviousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $Raw = @(& $Command @Arguments 2>&1)
        $ExitValue = Get-Variable -Name LASTEXITCODE -ValueOnly -ErrorAction SilentlyContinue
        if ([IO.Path]::GetExtension($Command) -ieq '.ps1') {
            $Code = 0
        } elseif ($null -eq $ExitValue) {
            $Code = 0
        } else {
            $Code = [int]$ExitValue
        }
    } catch {
        $InvocationError = $_
        $Raw += $_.ToString()
        $Code = 1
    } finally {
        $ErrorActionPreference = $PreviousErrorActionPreference
    }
    $EndUtc = (Get-Date).ToUniversalTime().ToString('o')
    $Rows = [Collections.Generic.List[string]]::new()
    [void]$Rows.Add("[runner] label=$Label start_utc=$StartUtc command=$Sanitized")
    foreach ($Line in $Raw) { [void]$Rows.Add($Line.ToString()) }
    if ($null -ne $InvocationError) {
        [void]$Rows.Add("[runner] invocation_failed=true exit=$Code")
    }
    $RawText = $Raw -join [Environment]::NewLine
    if ([string]::IsNullOrWhiteSpace($RawText)) {
        [void]$Rows.Add("[runner] no stdout/stderr emitted; exit=$Code")
    }
    [void]$Rows.Add("[runner] end_utc=$EndUtc exit=$Code")
    $Rows | Set-Content -LiteralPath $LogPath -Encoding UTF8
    return [pscustomobject]@{
        code = $Code
        tail = Get-ShortTail (Get-Content -LiteralPath $LogPath)
        output = ($Raw -join [Environment]::NewLine)
        start_utc = $StartUtc
        end_utc = $EndUtc
        command = $Sanitized
        label = $Label
    }
}
function Get-SessionBytes {
    param([string]$Path)
    $Sum = (Get-ChildItem -LiteralPath $Path -File -Recurse |
        Measure-Object -Property Length -Sum).Sum
    if ($null -eq $Sum) { return [int64]0 }
    return [int64]$Sum
}
function Get-GitState {
    $Top = @(& git -C $ProjectRoot rev-parse --show-toplevel 2>&1)
    if ($LASTEXITCODE -ne 0 -or $Top.Count -eq 0) { throw 'Git worktree detection failed.' }
    $Head = (@(& git -C $ProjectRoot rev-parse HEAD 2>&1) -join '').Trim()
    if ($LASTEXITCODE -ne 0 -or $Head -notmatch '^[0-9a-f]{40}$') { throw 'Git HEAD detection failed.' }
    $Branch = (@(& git -C $ProjectRoot branch --show-current 2>&1) -join '').Trim()
    if ($LASTEXITCODE -ne 0 -or !$Branch) { throw 'Git branch detection failed.' }
    $StatusLines = @(& git -C $ProjectRoot status --porcelain=v1 --untracked-files=all 2>&1)
    if ($LASTEXITCODE -ne 0) { throw 'Git cleanliness detection failed.' }
    $Tracked = @($StatusLines | Where-Object { $_ -notmatch '^\?\? ' })
    $Untracked = @($StatusLines | Where-Object { $_ -match '^\?\? ' })
    return [ordered]@{
        worktree = $Top[0].ToString().Trim()
        branch = $Branch
        head = $Head
        expected_branch = 'codex/r1-cpu-play-lifecycle-luna'
        tracked_clean = ($Tracked.Count -eq 0)
        nonignored_clean = ($StatusLines.Count -eq 0)
        tracked_status = @($Tracked)
        untracked_status = @($Untracked)
    }
}
function Get-FileRecord {
    param([string]$Path, [string]$Name, [int]$ExpectedWidth = 0, [int]$ExpectedHeight = 0)
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing artifact '$Name'." }
    $Item = Get-Item -LiteralPath $Path
    if ($Item.Length -le 0) { throw "Artifact '$Name' is empty." }
    $Hash = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
    if ([string]::IsNullOrWhiteSpace($Hash)) { throw "Artifact '$Name' has no SHA-256." }
    $Record = [ordered]@{
        name = $Name
        bytes = [int64]$Item.Length
        sha256 = $Hash
    }
    if ($ExpectedWidth -gt 0 -or $ExpectedHeight -gt 0) {
        $Dimensions = Get-PngDimensions $Path
        if (($ExpectedWidth -gt 0 -and $Dimensions.width -ne $ExpectedWidth) -or
            ($ExpectedHeight -gt 0 -and $Dimensions.height -ne $ExpectedHeight)) {
            throw "Artifact '$Name' has dimensions $($Dimensions.width)x$($Dimensions.height); expected $($ExpectedWidth)x$($ExpectedHeight)."
        }
        $Record.dimensions = '{0}x{1}' -f $Dimensions.width, $Dimensions.height
    }
    return $Record
}
function Get-ReferenceFrameRecords {
    param([string]$Directory)
    $Files = @(Get-ChildItem -LiteralPath $Directory -File -Filter 'reference-frame-*.png' | Sort-Object Name)
    if ($Files.Count -ne $ReferenceScreenshotCount) {
        throw "Expected exactly $ReferenceScreenshotCount reference frames; found $($Files.Count)."
    }
    $Records = [Collections.Generic.List[object]]::new()
    for ($Index = 1; $Index -le $ReferenceScreenshotCount; ++$Index) {
        $ExpectedName = 'reference-frame-{0:D4}.png' -f $Index
        if ($Files[$Index - 1].Name -cne $ExpectedName) {
            throw "Reference frame sequence is missing or has an extra frame near '$ExpectedName'."
        }
        [void]$Records.Add((Get-FileRecord $Files[$Index - 1].FullName $ExpectedName $ReferenceWidth $ReferenceHeight))
    }
    return @($Records)
}
function New-ContactSheet {
    param(
        [string]$Directory,
        [string]$OutputName,
        [string]$InputNameFormat,
        [int]$FrameCount,
        [int]$CellWidth,
        [int]$CellHeight,
        [int]$ExpectedWidth,
        [int]$ExpectedHeight
    )
    $OutputPath = Join-Path $Directory $OutputName
    $Sheet = [Drawing.Bitmap]::new(
        [int]($CellWidth * 3),
        [int]($CellHeight * 4))
    $Graphics = [Drawing.Graphics]::FromImage($Sheet)
    try {
        for ($Index = 1; $Index -le $FrameCount; ++$Index) {
            $Name = $InputNameFormat -f $Index
            $Source = [Drawing.Bitmap]::FromFile((Join-Path $Directory $Name))
            try {
                $Graphics.DrawImageUnscaled($Source, (($Index - 1) % 3) * $CellWidth,
                    [Math]::Floor(($Index - 1) / 3) * $CellHeight)
            } finally { $Source.Dispose() }
        }
        $Sheet.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
    } finally { $Graphics.Dispose(); $Sheet.Dispose() }
    return Get-FileRecord $OutputPath $OutputName $ExpectedWidth $ExpectedHeight
}
function Get-ArtifactInventory {
    param([string]$Root)
    $Rows = [ordered]@{}
    foreach ($Item in Get-ChildItem -LiteralPath $Root -File -Recurse | Sort-Object FullName) {
        if ($Item.Name -in @('.incomplete', 'proof-manifest.json', 'proof-summary.txt')) { continue }
        if ($Item.Name -eq 'progress.txt.tmp') { throw 'CPU lifecycle proof left progress.txt.tmp behind.' }
        if ($Item.Length -le 0) { throw "Empty proof artifact '$($Item.Name)'." }
        if ($Item.Name -match '\.log$') {
            $LogText = [IO.File]::ReadAllText($Item.FullName) -replace '^\uFEFF', ''
            if ($LogText -notmatch '(?m)^\[runner\] label=\S+ start_utc=\S+ command=[^\r\n]+\r?$' -or
                $LogText -notmatch '(?m)^\[runner\] end_utc=\S+ exit=-?\d+\r?$') {
                throw "Log '$($Item.Name)' lacks complete runner metadata."
            }
            $ToolRows = @([regex]::Split($LogText, '\r?\n') |
                Where-Object { $_ -and $_ -notmatch '^\[runner\] ' })
            if ($ToolRows.Count -eq 0 -and
                $LogText -notmatch '\[runner\] no stdout/stderr emitted; exit=') {
                throw "Silent log '$($Item.Name)' lacks the explicit no-output record."
            }
        }
        $Relative = $Item.FullName.Substring($Root.Length).TrimStart('\', '/') -replace '\\', '/'
        $Rows[$Relative] = [ordered]@{
            bytes = [int64]$Item.Length
            sha256 = (Get-FileHash -LiteralPath $Item.FullName -Algorithm SHA256).Hash.ToUpperInvariant()
        }
    }
    if ($Rows.Count -eq 0) { throw 'Proof artifact inventory is empty.' }
    return $Rows
}

$RomPath = if ($RomPath) { $RomPath } else { $env:TECMO_GAMEPLAY_LAB_ROM_PATH }
$FceuxPath = if ($FceuxPath) { $FceuxPath } else { $env:TECMO_GAMEPLAY_LAB_FCEUX_PATH }
if (-not $RomPath) { throw 'Pass -RomPath or set TECMO_GAMEPLAY_LAB_ROM_PATH.' }
if (-not $FceuxPath) { throw 'Pass -FceuxPath or set TECMO_GAMEPLAY_LAB_FCEUX_PATH.' }
$RomPath = (Resolve-Path -LiteralPath $RomPath).Path
$FceuxPath = (Resolve-Path -LiteralPath $FceuxPath).Path
$ProjectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
$LuaPath = Join-Path $PSScriptRoot 'tecmo_cpu_lifecycle.lua'
$MapPath = Join-Path $PSScriptRoot 'tecmo_cpu_lifecycle_rev1_map.lua'
$Executable = Join-Path $ProjectRoot 'build\tecmo_port.exe'
$GitState = Get-GitState
if ($GitState.branch -ne $GitState.expected_branch) {
    throw "CPU lifecycle proof requires branch '$($GitState.expected_branch)'; found '$($GitState.branch)'."
}
if ($RequirePass -and !$GitState.tracked_clean) {
    throw 'CPU lifecycle -RequirePass refuses tracked worktree dirtiness.'
}
if ($RequirePass -and !$GitState.nonignored_clean) {
    throw 'CPU lifecycle -RequirePass refuses untracked nonignored worktree entries.'
}
if ($RequirePass -and !$RequireVideo) {
    throw 'CPU lifecycle -RequirePass requires -RequireVideo.'
}
$SolInspection = $env:TECMO_CPU_LIFECYCLE_PERSONAL_SOL_INSPECTION
if ($RequirePass -and $SolInspection -ne 'complete') {
    throw 'CPU lifecycle -RequirePass requires TECMO_CPU_LIFECYCLE_PERSONAL_SOL_INSPECTION=complete.'
}
foreach ($Path in @($LuaPath, $MapPath, $RomPath, $FceuxPath)) {
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing CPU proof input: $Path" }
}
if (@(Get-Process -Name fceux -ErrorAction SilentlyContinue).Count -ne 0) {
    throw 'FCEUX is already running; CPU lifecycle proof refuses concurrent emulator state.'
}
$RomHash = (Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash.ToUpperInvariant()
$FceuxHash = (Get-FileHash -LiteralPath $FceuxPath -Algorithm SHA256).Hash.ToUpperInvariant()
if ($RomHash -ne $ExpectedRomSha256) { throw "Wrong ROM revision: $RomHash" }
if ($FceuxHash -ne $ExpectedFceuxSha256) { throw "Wrong FCEUX build: $FceuxHash" }

$OutputBase = [IO.Path]::GetFullPath((Join-Path $ProjectRoot 'temp-videos\gameplay-lab\cpu-lifecycle'))
$ProjectPrefix = $ProjectRoot.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
if (!$OutputBase.StartsWith($ProjectPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'CPU lifecycle proof output escaped the project.'
}
[void](New-Item -ItemType Directory -Force -Path $OutputBase)
$SessionName = Get-Date -Format 'yyyyMMdd-HHmmss'
$OutputRoot = Join-Path $OutputBase $SessionName
$Suffix = 1
while (Test-Path -LiteralPath $OutputRoot) {
    $OutputRoot = Join-Path $OutputBase ('{0}-{1:D2}' -f $SessionName, $Suffix++)
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
$OutputPrefix = $OutputBase.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
if (!$OutputRoot.StartsWith($OutputPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'CPU lifecycle proof session escaped ignored output.'
}
[void](New-Item -ItemType Directory -Force -Path $OutputRoot)
$IncompletePath = Join-Path $OutputRoot '.incomplete'
Set-Content -LiteralPath $IncompletePath -Value 'CPU lifecycle proof incomplete' -NoNewline -Encoding ASCII
$Original1 = Join-Path $OutputRoot 'original-1'
$Original2 = Join-Path $OutputRoot 'original-2'
$NativeRoot = Join-Path $OutputRoot 'native'
[void](New-Item -ItemType Directory -Force -Path $Original1)
[void](New-Item -ItemType Directory -Force -Path $Original2)
[void](New-Item -ItemType Directory -Force -Path $NativeRoot)

$ScriptHash = (Get-FileHash -LiteralPath $LuaPath -Algorithm SHA256).Hash.ToUpperInvariant()
$MapHash = (Get-FileHash -LiteralPath $MapPath -Algorithm SHA256).Hash.ToUpperInvariant()
$Previous = @{}
$EnvironmentNames = @(
    'TECMO_CPU_LIFECYCLE_OUTPUT', 'TECMO_CPU_LIFECYCLE_MAP',
    'TECMO_CPU_LIFECYCLE_ROM_SHA256', 'TECMO_CPU_LIFECYCLE_FCEUX_SHA256',
    'TECMO_CPU_LIFECYCLE_MAX_FRAMES', 'TECMO_ASSETPACK', 'TECMO_SKIP_SHORTCUT'
)
foreach ($Name in $EnvironmentNames) { $Previous[$Name] = [Environment]::GetEnvironmentVariable($Name, 'Process') }
$Commands = [Collections.Generic.List[string]]::new()
$ReferenceRuns = @()
$NativeFrameHashes = [ordered]@{}
$NativeFrameDetails = [ordered]@{}
$NativeRepeatFrameDetails = [ordered]@{}
$PackPath = Join-Path $NativeRoot 'gameplay-cpu-steering.assetpack'
$PackIdentity = $null
$VideoInfo = [ordered]@{
    status = 'unavailable'; files = [ordered]@{}; sha256 = $null; repeat_sha256 = $null
    deterministic_sha256_equal = $false; commands = @(); probe_commands = @()
    probes = [ordered]@{}; ffmpeg = $null; ffprobe = $null; dimensions = $null
    frames = 0; cadence = $NativeFrameRate
}

function Invoke-ReferenceRun {
    param([string]$Label, [string]$Path)
    $Environment = @{
        TECMO_CPU_LIFECYCLE_OUTPUT = $Path
        TECMO_CPU_LIFECYCLE_MAP = $MapPath
        TECMO_CPU_LIFECYCLE_ROM_SHA256 = $RomHash
        TECMO_CPU_LIFECYCLE_FCEUX_SHA256 = $FceuxHash
        TECMO_CPU_LIFECYCLE_MAX_FRAMES = '4320'
    }
    foreach ($Pair in $Environment.GetEnumerator()) { Set-Item -Path "Env:$($Pair.Key)" -Value $Pair.Value }
    $Stdout = Join-Path $Path 'fceux.stdout.log'
    $Stderr = Join-Path $Path 'fceux.stderr.log'
    $ArgumentLine = '-sound 0 -lua "{0}" "{1}"' -f $LuaPath, $RomPath
    $Start = @{
        FilePath = $FceuxPath
        ArgumentList = $ArgumentLine
        WorkingDirectory = Split-Path -Parent $FceuxPath
        PassThru = $true
        RedirectStandardOutput = $Stdout
        RedirectStandardError = $Stderr
    }
    if (-not $Visible) { $Start.WindowStyle = 'Hidden' }
    $ProcessStartUtc = (Get-Date).ToUniversalTime().ToString('o')
    $Process = Start-Process @Start
    $Watch = [Diagnostics.Stopwatch]::StartNew()
    $SentinelSeen = $false
    $ProgressPath = Join-Path $Path 'progress.txt'
    $LastProgressMarker = ''
    $LastProgressSeconds = 0.0
    $ProcessExitCode = $null
    try {
        while (-not $Process.WaitForExit(250)) {
            if ((Get-SessionBytes $Path) -gt 64MB) {
                $Process.Kill(); $Process.WaitForExit()
                throw "CPU lifecycle original run '$Label' exceeded 64 MiB output cap."
            }
            if (-not $SentinelSeen) {
                $Snapshot = Get-ProgressSnapshot $ProgressPath
                if ($null -ne $Snapshot) {
                    $SentinelSeen = $true
                    $LastProgressMarker = $Snapshot.marker
                    $LastProgressSeconds = $Watch.Elapsed.TotalSeconds
                }
                if (-not $SentinelSeen -and $Watch.Elapsed.TotalSeconds -gt 5) {
                    $Process.Kill(); $Process.WaitForExit()
                    throw "CPU lifecycle original run '$Label' missed five-second startup sentinel."
                }
            } else {
                $Snapshot = Get-ProgressSnapshot $ProgressPath
                if ($null -ne $Snapshot) {
                    if ($Snapshot.marker -cne $LastProgressMarker) {
                        $LastProgressMarker = $Snapshot.marker
                        $LastProgressSeconds = $Watch.Elapsed.TotalSeconds
                    } elseif ($Watch.Elapsed.TotalSeconds - $LastProgressSeconds -gt 5) {
                        $Process.Kill(); $Process.WaitForExit()
                        throw "CPU lifecycle original run '$Label' stopped producing progress."
                    }
                } elseif ($Watch.Elapsed.TotalSeconds - $LastProgressSeconds -gt 5) {
                    $Process.Kill(); $Process.WaitForExit()
                    throw "CPU lifecycle original run '$Label' stopped producing progress."
                }
            }
            if ($Watch.Elapsed.TotalSeconds -gt 180) {
                $Process.Kill(); $Process.WaitForExit()
                throw "CPU lifecycle original run '$Label' exceeded 180 seconds."
            }
        }
    } finally {
        if (-not $Process.HasExited) { $Process.Kill() }
        $Process.WaitForExit()
        $Process.Refresh()
        $ProcessEndUtc = (Get-Date).ToUniversalTime().ToString('o')
        $ProcessExitCode = [int]$Process.ExitCode
        $SanitizedFceuxCommand = Get-SanitizedCommand $FceuxPath @('-sound', '0', '-lua', $LuaPath, $RomPath)
        Add-ProcessLogMetadata $Stdout 'fceux-stdout' $SanitizedFceuxCommand $ProcessStartUtc $ProcessEndUtc $ProcessExitCode
        Add-ProcessLogMetadata $Stderr 'fceux-stderr' $SanitizedFceuxCommand $ProcessStartUtc $ProcessEndUtc $ProcessExitCode
        $Process.Dispose()
    }
    if ($ProcessExitCode -ne 0) { throw "FCEUX '$Label' exited $ProcessExitCode." }
    $Status = Get-Status (Join-Path $Path 'status.txt')
    foreach ($Name in @('schema','schema_version','rom_sha256','fceux_sha256',
                        'result','captured_frames','final_pads_neutral',
                        'capture_window_complete','setup_seen','tip_started',
                        'tip_not_running_seen','clock_stopped_seen','clock_running_seen',
                        'clock_running_observed','running_clock_live_seen','live_seen',
                        'lifecycle_evidence_valid','fetch_events',
                        'opcode_observations','dispatch_events','handler_events',
                        'advance_events','aligned_stream_offsets',
                        'fixed_link_observations','fixed_link_mismatches',
                        'invalid_fetches','misaligned_fetches','screenshot_count',
                        'final_progress_written','speedmode_ok',
                        'observed_actor_count','observed_opcode_count',
                        'observed_handler_count','ram_writes','cheats','savestates')) {
        if (-not $Status.Contains($Name)) { throw "CPU lifecycle '$Label' omitted status '$Name'." }
    }
    if ($Status.schema -ne 'TGLCPU-TRACE-1' -or $Status.schema_version -ne '1' -or
        $Status.rom_sha256 -ne $ExpectedRomSha256 -or $Status.fceux_sha256 -ne $ExpectedFceuxSha256 -or
        $Status.result -ne 'pass' -or $Status.captured_frames -ne [string]$ReferenceFrameCount -or
        $Status.final_pads_neutral -ne 'true' -or $Status.capture_window_complete -ne 'true' -or
        $Status.screenshot_count -ne [string]$ReferenceScreenshotCount -or
        $Status.speedmode_ok -ne 'true' -or $Status.final_progress_written -ne 'true' -or
        $Status.ram_writes -ne '0' -or $Status.cheats -ne '0' -or $Status.savestates -ne '0') {
        throw "CPU lifecycle original run '$Label' failed its fail-closed status contract."
    }
    foreach ($Gate in @('setup_seen','tip_started','tip_not_running_seen','clock_stopped_seen',
                        'clock_running_seen','clock_running_observed',
                        'running_clock_live_seen','live_seen','lifecycle_evidence_valid')) {
        if ($Status[$Gate] -ne 'true') {
            throw "CPU lifecycle '$Label' did not prove gate '$Gate'."
        }
    }
    foreach ($Evidence in @('fetch_events','opcode_observations','dispatch_events',
                            'handler_events','advance_events','aligned_stream_offsets',
                            'fixed_link_observations','observed_actor_count',
                            'observed_opcode_count','observed_handler_count')) {
        [int]$EvidenceValue = 0
        if (-not [int]::TryParse($Status[$Evidence], [ref]$EvidenceValue) -or
                $EvidenceValue -le 0) {
            throw "CPU lifecycle '$Label' lacks positive evidence '$Evidence'."
        }
    }
    if ($Status.fixed_link_mismatches -ne '0') {
        throw "CPU lifecycle '$Label' observed a fixed-link mismatch."
    }
    if ($Status.invalid_fetches -ne '0' -or $Status.misaligned_fetches -ne '0') {
        throw "CPU lifecycle '$Label' observed an invalid or misaligned fetch."
    }
    foreach ($Required in @('metadata.txt','progress.txt','trace.csv','actors.csv')) {
        if (!(Test-Path -LiteralPath (Join-Path $Path $Required) -PathType Leaf)) {
            throw "CPU lifecycle '$Label' omitted $Required."
        }
    }
    if (Test-Path -LiteralPath (Join-Path $Path 'progress.txt.tmp') -PathType Leaf) {
        throw "CPU lifecycle '$Label' left progress.txt.tmp behind."
    }
    $Progress = Get-ProgressSnapshot (Join-Path $Path 'progress.txt')
    if ($null -eq $Progress -or $Progress.values.schema -ne 'TGLCPU-PROGRESS-1' -or
        $Progress.values.stage -ne 'finished' -or
        $Progress.values.captured_frames -ne [string]$ReferenceFrameCount -or
        $Progress.values.speedmode_ok -ne 'true') {
        throw "CPU lifecycle '$Label' did not publish a valid finished progress record."
    }
    [int]$ProgressSequence = 0
    if (-not [int]::TryParse($Progress.values.sequence, [ref]$ProgressSequence) -or
        $ProgressSequence -le 0) {
        throw "CPU lifecycle '$Label' published a non-positive final progress sequence."
    }
    $RequiredRecords = [ordered]@{}
    foreach ($Required in @('metadata.txt','status.txt','progress.txt','trace.csv','actors.csv')) {
        $RequiredRecords[$Required] = Get-FileRecord (Join-Path $Path $Required) $Required
    }
    $FrameDetails = Get-ReferenceFrameRecords $Path
    $ContactSheetRecord = New-ContactSheet $Path 'reference-contact-sheet.png' `
        'reference-frame-{0:D4}.png' $ReferenceScreenshotCount $ReferenceWidth $ReferenceHeight `
        $ReferenceSheetWidth $ReferenceSheetHeight
    return [ordered]@{
        label = $Label
        status = $Status
        progress = $Progress.values
        required_files = $RequiredRecords
        trace_sha256 = (Get-FileHash -LiteralPath (Join-Path $Path 'trace.csv') -Algorithm SHA256).Hash.ToUpperInvariant()
        actor_sha256 = (Get-FileHash -LiteralPath (Join-Path $Path 'actors.csv') -Algorithm SHA256).Hash.ToUpperInvariant()
        frame_index_sha256 = Get-FileFingerprint $Path 'reference-frame-*.png'
        reference_frames = $FrameDetails
        contact_sheet = $ContactSheetRecord
    }
}

try {
    $env:TECMO_SKIP_SHORTCUT = '1'
    & (Join-Path $ProjectRoot 'tools\gameplay-lab\Test-GameplayLab.ps1')
    if ($LASTEXITCODE -ne 0) { throw 'Gameplay-lab static suite failed before CPU proof.' }
    if ($Build -or !(Test-Path -LiteralPath $Executable -PathType Leaf)) {
        $BuildRun = Invoke-Logged -Command (Join-Path $ProjectRoot 'build.ps1') -Arguments @() `
            -LogPath (Join-Path $OutputRoot 'native-build.log') -Label 'native-build'
        if ($BuildRun.code -ne 0 -or $BuildRun.tail -match 'warning [A-Z]+[0-9]+:') {
            throw "Warning-clean native build failed.`n$($BuildRun.tail)"
        }
    }
    if (!(Test-Path -LiteralPath $Executable -PathType Leaf)) { throw 'Native executable is missing.' }
    $PowerShellCommand = Get-Command powershell.exe -CommandType Application -ErrorAction SilentlyContinue
    if ($null -eq $PowerShellCommand) {
        throw 'CPU lifecycle focused wrapper requires powershell.exe for named parameter transport.'
    }
    $FocusedScript = Join-Path $ProjectRoot 'tools\Run-GameplayCpuSteeringTests.ps1'
    $FocusedArguments = @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $FocusedScript,
        '-RomPath', $RomPath, '-ProjectRoot', $ProjectRoot, '-Build'
    )
    $Focused = Invoke-Logged -Command 'powershell.exe' -Arguments $FocusedArguments `
        -LogPath (Join-Path $OutputRoot 'cpu-focused.log') -Label 'cpu-focused'
    [void]$Commands.Add('powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\Run-GameplayCpuSteeringTests.ps1 -RomPath [LOCAL_REV1_ROM] -ProjectRoot [PROJECT] -Build')
    if ($Focused.code -ne 0 -or $Focused.tail -notmatch '17 ROM mutations') {
        throw "CPU focused wrapper failed.`n$($Focused.tail)"
    }
    $PackRun = Invoke-Logged -Command $Executable -Arguments @('--build-assetpack', $RomPath, $PackPath) `
        -LogPath (Join-Path $NativeRoot 'asset-pack.log') -Label 'asset-pack-build'
    [void]$Commands.Add('tecmo_port.exe --build-assetpack [LOCAL_REV1_ROM] [IGNORED_PROOF_PACK]')
    if ($PackRun.code -ne 0 -or !(Test-Path -LiteralPath $PackPath -PathType Leaf)) {
        throw "Fresh CPU proof asset-pack build failed.`n$($PackRun.tail)"
    }
    $PackRecord = Get-FileRecord $PackPath 'gameplay-cpu-steering.assetpack'
    $PackBytes = [IO.File]::ReadAllBytes($PackPath)
    $TgaiEntry = Get-AssetPackEntry $PackBytes 'gameplay/cpu-steering'
    $TgaiPayload = Get-EntryBytes $PackBytes $TgaiEntry
    $PackIdentity = [ordered]@{
        pack_sha256 = (Get-FileHash -LiteralPath $PackPath -Algorithm SHA256).Hash.ToUpperInvariant()
        entry = 'gameplay/cpu-steering'
        bytes = [int]$TgaiEntry.byte_count
        fnv1a32 = Get-Fnv1a32 $TgaiPayload
        pack_bytes = $PackRecord.bytes
    }
    if ($PackIdentity.bytes -ne $ExpectedTgaiBytes -or $PackIdentity.fnv1a32 -ne $ExpectedTgaiFnv1a32) {
        throw 'Fresh CPU proof pack failed TGAI-1 identity.'
    }
    $env:TECMO_ASSETPACK = $PackPath
    $TestRun = Invoke-Logged -Command $Executable -Arguments @('--gameplay-cpu-steering-test', $PackPath) `
        -LogPath (Join-Path $NativeRoot 'cpu-lifecycle-self-test.log') -Label 'cpu-lifecycle-self-test'
    [void]$Commands.Add('tecmo_port.exe --gameplay-cpu-steering-test [IGNORED_PROOF_PACK]')
    if ($TestRun.code -ne 0 -or $TestRun.tail -notmatch 'TGAI-1 CPU steering isolated') {
        throw "Fresh CPU lifecycle self-test failed.`n$($TestRun.tail)"
    }

    Add-Type -AssemblyName System.Drawing
    $ReferenceRuns += Invoke-ReferenceRun 'original-1' $Original1
    $ReferenceRuns += Invoke-ReferenceRun 'original-2' $Original2
    if ($ReferenceRuns[0].trace_sha256 -ne $ReferenceRuns[1].trace_sha256 -or
        $ReferenceRuns[0].actor_sha256 -ne $ReferenceRuns[1].actor_sha256 -or
        $ReferenceRuns[0].frame_index_sha256 -ne $ReferenceRuns[1].frame_index_sha256 -or
        $ReferenceRuns[0].contact_sheet.sha256 -ne $ReferenceRuns[1].contact_sheet.sha256) {
        throw 'Original CPU lifecycle live-window runs were not deterministic.'
    }
    [void]$Commands.Add('FCEUX -sound 0 -lua tecmo_cpu_lifecycle.lua [LOCAL_REV1_ROM] (twice)')

    for ($Pass = 1; $Pass -le 2; ++$Pass) {
        for ($Index = 0; $Index -lt $NativeFrameCount; ++$Index) {
            $Frame = $NativeFirstFrame + $Index
            $FrameKey = '{0:D4}' -f $Frame
            $Name = if ($Pass -eq 1) { 'native-frame-{0:D4}.png' } else { 'native-repeat-frame-{0:D4}.png' }
            $Png = Join-Path $NativeRoot ($Name -f $Frame)
            $Mode = "gameplay-cpu-steering-frame$Frame"
            $Run = Invoke-Logged -Command $Executable -Arguments @(
                '--root', $ProjectRoot, '--render-test-mode', $Mode, $Png) `
                -LogPath (Join-Path $NativeRoot ("render-{0}-{1}.log" -f $Frame, $Pass)) `
                -Label ("native-render-{0}-pass-{1}" -f $Frame, $Pass)
            [void]$Commands.Add("tecmo_port.exe --root [PROJECT] --render-test-mode $Mode [IGNORED_PNG]")
            if ($Run.code -ne 0 -or !(Test-Path -LiteralPath $Png -PathType Leaf)) {
                throw "Native CPU render failed at $Mode.`n$($Run.tail)"
            }
            $Dimensions = Get-PngDimensions $Png
            if ($Dimensions.width -ne $NativeWidth -or $Dimensions.height -ne $NativeHeight) {
                throw "Native CPU render $Mode is not 640x480."
            }
            $Hash = (Get-FileHash -LiteralPath $Png -Algorithm SHA256).Hash.ToUpperInvariant()
            $Record = Get-FileRecord $Png ($Name -f $Frame) $NativeWidth $NativeHeight
            if ($Pass -eq 1) {
                $NativeFrameHashes[$FrameKey] = $Hash
                $NativeFrameDetails[$FrameKey] = $Record
            } else {
                $NativeRepeatFrameDetails[$FrameKey] = $Record
                if ($NativeFrameHashes[$FrameKey] -ne $Hash) { throw "Native render $Mode is nondeterministic." }
            }
        }
    }
    $SheetPath = Join-Path $NativeRoot 'native-contact-sheet.png'
    $Sheet = [Drawing.Bitmap]::new(
        [int]($NativeWidth * 3),
        [int]($NativeHeight * 4))
    $Graphics = [Drawing.Graphics]::FromImage($Sheet)
    try {
        for ($Index = 0; $Index -lt $NativeFrameCount; ++$Index) {
            $Frame = $NativeFirstFrame + $Index
            $Source = [Drawing.Bitmap]::FromFile(
                (Join-Path $NativeRoot ('native-frame-{0:D4}.png' -f $Frame)))
            try { $Graphics.DrawImageUnscaled($Source, ($Index % 3) * $NativeWidth, [Math]::Floor($Index / 3) * $NativeHeight) }
            finally { $Source.Dispose() }
        }
        $Sheet.Save($SheetPath, [Drawing.Imaging.ImageFormat]::Png)
    } finally { $Graphics.Dispose(); $Sheet.Dispose() }
    $ContactSheetRecord = Get-FileRecord $SheetPath 'native-contact-sheet.png' $NativeSheetWidth $NativeSheetHeight

    $Ffmpeg = if ($FfmpegPath) { $FfmpegPath } else {
        $Command = Get-Command ffmpeg -ErrorAction SilentlyContinue
        if ($null -eq $Command) { $null } else { $Command.Source }
    }
    $Ffprobe = if ($FfprobePath) { $FfprobePath } else {
        $Command = Get-Command ffprobe -ErrorAction SilentlyContinue
        if ($null -eq $Command) { $null } else { $Command.Source }
    }
    if ($null -ne $Ffmpeg -and $null -ne $Ffprobe) {
        $VideoRecords = [ordered]@{}
        $VideoCommands = [Collections.Generic.List[string]]::new()
        $ProbeCommands = [Collections.Generic.List[string]]::new()
        $VideoProbes = [ordered]@{}
        foreach ($VideoSpec in @(
            [ordered]@{ key = 'primary'; image = 'native-frame-%04d.png'; file = 'native-cpu-lifecycle.mp4'; ffmpeg_log = 'ffmpeg-primary.log'; ffprobe_log = 'ffprobe-primary.log' },
            [ordered]@{ key = 'repeat'; image = 'native-repeat-frame-%04d.png'; file = 'native-cpu-lifecycle-repeat.mp4'; ffmpeg_log = 'ffmpeg-repeat.log'; ffprobe_log = 'ffprobe-repeat.log' }
        )) {
            $VideoPath = Join-Path $NativeRoot $VideoSpec.file
            $EncodeArgs = @(
                '-y', '-hide_banner', '-loglevel', 'error', '-framerate', $NativeFrameRate,
                '-start_number', [string]$NativeFirstFrame, '-i', (Join-Path $NativeRoot $VideoSpec.image),
                '-frames:v', [string]$NativeFrameCount, '-pix_fmt', 'yuv420p',
                '-video_track_timescale', [string]$NativeVideoTrackTimescale, $VideoPath
            )
            $Encode = Invoke-Logged -Command $Ffmpeg -Arguments $EncodeArgs `
                -LogPath (Join-Path $NativeRoot $VideoSpec.ffmpeg_log) -Label ("ffmpeg-{0}" -f $VideoSpec.key)
            $SanitizedEncode = Get-SanitizedCommand $Ffmpeg $EncodeArgs
            [void]$VideoCommands.Add($SanitizedEncode)
            if ($Encode.code -ne 0 -or !(Test-Path -LiteralPath $VideoPath -PathType Leaf)) {
                throw "ffmpeg was available but CPU lifecycle $($VideoSpec.key) MP4 generation failed."
            }
            $ProbeArgs = @(
                '-v', 'error', '-count_frames', '-select_streams', 'v:0',
                '-show_entries', 'stream=width,height,r_frame_rate,avg_frame_rate,time_base,nb_frames,nb_read_frames',
                '-of', 'json', $VideoPath
            )
            $ProbeRun = Invoke-Logged -Command $Ffprobe -Arguments $ProbeArgs `
                -LogPath (Join-Path $NativeRoot $VideoSpec.ffprobe_log) -Label ("ffprobe-{0}" -f $VideoSpec.key)
            if ($ProbeRun.code -ne 0) { throw "ffprobe failed for CPU lifecycle $($VideoSpec.key) video." }
            try { $ProbeJson = $ProbeRun.output | ConvertFrom-Json } catch { throw "ffprobe returned invalid JSON for CPU lifecycle $($VideoSpec.key) video." }
            $Streams = @($ProbeJson.streams)
            if ($Streams.Count -ne 1) {
                throw "ffprobe returned an unexpected stream count for CPU lifecycle $($VideoSpec.key) video."
            }
            $Stream = $Streams[0]
            if ([int]$Stream.width -ne $NativeWidth -or [int]$Stream.height -ne $NativeHeight -or
                [string]$Stream.r_frame_rate -ne $NativeFrameRate -or
                [string]$Stream.avg_frame_rate -ne $NativeFrameRate -or
                [string]$Stream.time_base -ne $NativeVideoTimeBase -or
                [int]$Stream.nb_frames -ne $NativeFrameCount -or
                [int]$Stream.nb_read_frames -ne $NativeFrameCount) {
                throw "ffprobe CPU lifecycle $($VideoSpec.key) cadence/dimensions/frame metadata failed."
            }
            $VideoRecord = Get-FileRecord $VideoPath $VideoSpec.file
            $VideoRecords[$VideoSpec.key] = $VideoRecord
            $VideoProbes[$VideoSpec.key] = [ordered]@{
                width = [int]$Stream.width; height = [int]$Stream.height
                r_frame_rate = [string]$Stream.r_frame_rate
                avg_frame_rate = [string]$Stream.avg_frame_rate
                time_base = [string]$Stream.time_base
                nb_frames = [int]$Stream.nb_frames
                nb_read_frames = [int]$Stream.nb_read_frames
            }
            [void]$Commands.Add($SanitizedEncode)
            $SanitizedProbe = Get-SanitizedCommand $Ffprobe $ProbeArgs
            [void]$ProbeCommands.Add($SanitizedProbe)
            [void]$Commands.Add($SanitizedProbe)
        }
        if ($VideoRecords.primary.sha256 -ne $VideoRecords.repeat.sha256) {
            throw 'Deterministic native video encodes did not produce equal SHA-256 values.'
        }
        $VideoInfo = [ordered]@{
            status = 'validated'; files = $VideoRecords
            sha256 = $VideoRecords.primary.sha256
            repeat_sha256 = $VideoRecords.repeat.sha256
            deterministic_sha256_equal = $true
            commands = @($VideoCommands.ToArray())
            probe_commands = @($ProbeCommands.ToArray())
            probes = $VideoProbes
            ffmpeg = Get-ToolInfo $Ffmpeg; ffprobe = Get-ToolInfo $Ffprobe
            dimensions = '640x480'; frames = $NativeFrameCount; cadence = $NativeFrameRate
            track_timescale = $NativeVideoTrackTimescale; time_base = $NativeVideoTimeBase
        }
    } elseif ($RequireVideo) {
        throw 'RequireVideo was supplied but ffmpeg/ffprobe were not both available.'
    }

    if ($RequirePass -and ($VideoInfo.status -ne 'validated' -or
        $VideoInfo.deterministic_sha256_equal -ne $true)) {
        throw 'CPU lifecycle -RequirePass refuses unavailable or nondeterministic video evidence.'
    }
    if ((Get-SessionBytes $OutputRoot) -gt 64MB) {
        throw 'CPU lifecycle proof exceeded the 64 MiB whole-session output cap.'
    }

    $ArtifactInventory = Get-ArtifactInventory $OutputRoot
    $LogHashes = [ordered]@{}
    foreach ($Key in $ArtifactInventory.Keys) {
        if ($Key -match '\.log$') { $LogHashes[$Key] = $ArtifactInventory[$Key] }
    }
    if ($LogHashes.Count -eq 0) {
        throw 'CPU lifecycle proof did not produce any inventoried logs.'
    }
    $TraceActorHashes = [ordered]@{
        'original-1/trace.csv' = $ArtifactInventory['original-1/trace.csv']
        'original-1/actors.csv' = $ArtifactInventory['original-1/actors.csv']
        'original-2/trace.csv' = $ArtifactInventory['original-2/trace.csv']
        'original-2/actors.csv' = $ArtifactInventory['original-2/actors.csv']
    }
    $FinalSha = if ($RequirePass) { $GitState.head } else { 'PENDING_FINAL_SHA_UNTIL_COMMIT' }
    $ProofStatus = if ($RequirePass) { 'pass' } else { 'draft_pass' }
    $InspectionValue = if ($RequirePass) { 'complete' } else { 'PENDING_PERSONAL_SOL_INSPECTION' }
    $Manifest = [ordered]@{
        schema = 'tecmo.r1-cpu-play-lifecycle-proof/2'
        status = $ProofStatus
        task = 'R1-CPU-PLAY-LIFECYCLE'
        generated_utc = (Get-Date).ToUniversalTime().ToString('o')
        base_sha = $ExpectedBaseSha
        final_sha = $FinalSha
        git = $GitState
        rom = [ordered]@{ sha256 = $RomHash; revision = 'canonical Rev1'; runtime_reads_rom = $false }
        fceux = [ordered]@{ sha256 = $FceuxHash; private_run = $true }
        asset_pack = $PackIdentity
        scripts = [ordered]@{
            lua = [ordered]@{ path = 'tools/gameplay-lab/tecmo_cpu_lifecycle.lua'; sha256 = $ScriptHash }
            map = [ordered]@{ path = 'tools/gameplay-lab/tecmo_cpu_lifecycle_rev1_map.lua'; sha256 = $MapHash }
            runner = [ordered]@{ path = 'tools/gameplay-lab/Run-GameplayCpuLifecycleProof.ps1'; sha256 = (Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash.ToUpperInvariant() }
        }
        input_schedule = [ordered]@{
            kind = 'controller-input-only'
            classification = 'accepted deterministic/authentic menu and tip schedule; row timing is not claimed as ASM/source-pinned input semantics'
            map = 'tools/gameplay-lab/tecmo_cpu_lifecycle_rev1_map.lua'
            pads = 'complete neutral P1/P2 every frame'
            tip = 'P1 A ages 30-34; A+B ages 35-37; B ages 38-55'
        }
        original_reference = [ordered]@{
            schema = 'TGLCPU-TRACE-1'
            live_window_frames = $ReferenceFrameCount
            screenshot_count = $ReferenceScreenshotCount
            resolution = '256x224 FCEUX gui.savescreenshotas PNG raster/crop'
            video_resolution = '256x240 original AVI/video contract (separate from PNG raster)'
            run_count = 2
            deterministic = $true
            contact_sheet_dimensions = '768x896'
            contact_sheet_hashes_equal = ($ReferenceRuns[0].contact_sheet.sha256 -eq $ReferenceRuns[1].contact_sheet.sha256)
            runs = @($ReferenceRuns | ForEach-Object {
                [ordered]@{
                    label = $_.label
                    status = $_.status
                    progress = $_.progress
                    required_files = $_.required_files
                    trace_sha256 = $_.trace_sha256
                    actor_sha256 = $_.actor_sha256
                    frame_index_sha256 = $_.frame_index_sha256
                    reference_frames = $_.reference_frames
                    contact_sheet = $_.contact_sheet
                }
            })
        }
        native = [ordered]@{
            surface = 'legacy gameplay-cpu-steering-frameN continuity/regression'
            integration = 'deferred to R1-LIVE; isolated lifecycle engine is not consumed by normal scene flow'
            frames = '25..36'
            resolution = '640x480'
            contact_sheet_dimensions = '1920x1920'
            frame_hashes = $NativeFrameDetails
            repeat_frame_hashes = $NativeRepeatFrameDetails
            contact_sheet = $ContactSheetRecord
            video = $VideoInfo
        }
        evidence = [ordered]@{
            original_status = @($ReferenceRuns | ForEach-Object { $_.status })
            trace_actor_files = $TraceActorHashes
            log_files = $LogHashes
            artifacts = $ArtifactInventory
        }
        commands = @($Commands)
        tools = [ordered]@{
            powershell = $PSVersionTable.PSVersion.ToString()
            fceux = [ordered]@{ sha256 = $FceuxHash; version = 'locked private binary' }
            ffmpeg = $VideoInfo.ffmpeg
            ffprobe = $VideoInfo.ffprobe
        }
        limitations = @(
            'production scene is legacy native harness/formation continuity evidence only',
            'isolated lifecycle engine is not consumed by normal scene flow; integration is R1-LIVE',
            'original trace proves source-pinned execution evidence, not inferred handler intent',
            'native frames are deterministic regression evidence, not one-to-one lifecycle parity',
            'shot request outcome/release/make/miss and dynamic candidate ownership remain deferred'
        )
        personal_sol_inspection = $InspectionValue
    }
    if ($ProofStatus -eq 'pass' -and $VideoInfo.status -ne 'validated') {
        throw 'CPU lifecycle pass status cannot carry unavailable video evidence.'
    }
    $VideoEncodeSummary = (@($VideoInfo.commands) -join ' || ')
    $VideoProbeSummary = (@($VideoInfo.probe_commands) -join ' || ')
    $ManifestJson = $Manifest | ConvertTo-Json -Depth 20
    if ($RequirePass -and $ManifestJson -match '(?i)PENDING|PLACEHOLDER') {
        throw 'CPU lifecycle -RequirePass refuses pending/placeholder manifest metadata.'
    }
    $ManifestPath = Join-Path $OutputRoot 'proof-manifest.json'
    $ManifestJson | Set-Content -LiteralPath $ManifestPath -Encoding UTF8
    $SummaryPath = Join-Path $OutputRoot 'proof-summary.txt'
    @(
        "R1 CPU lifecycle proof: $($ProofStatus.ToUpperInvariant())",
        "generated_utc=$($Manifest.generated_utc)",
        "worktree_branch=$($GitState.branch)",
        "head=$($GitState.head)",
        "base_sha=$ExpectedBaseSha",
        "final_sha=$FinalSha",
        "rom_sha256=$RomHash",
        "fceux_sha256=$FceuxHash",
        "tgai_bytes=$($PackIdentity.bytes) tgai_fnv1a32=$($PackIdentity.fnv1a32)",
        'original=two deterministic/authentic 120-frame source traces; 256x224 FCEUX PNG raster; separate 256x240 original AVI/video contract; 12 screenshots each',
        'original_contact_sheets=two separate 768x896 3x4 sheets; equal SHA-256 required',
        'native=12 contiguous 640x480 legacy continuity frames rendered twice; 1920x1920 contact sheet',
        "video=$($VideoInfo.status) sha256=$($VideoInfo.sha256) repeat_sha256=$($VideoInfo.repeat_sha256) cadence=$NativeFrameRate",
        "video_encode_commands=$VideoEncodeSummary",
        "video_probe_commands=$VideoProbeSummary",
        'limitations=scene integration remains R1-LIVE; visual output is regression evidence only',
        "personal_sol_inspection=$InspectionValue"
    ) | Set-Content -LiteralPath $SummaryPath -Encoding UTF8
    if (!(Test-Path -LiteralPath $ManifestPath -PathType Leaf) -or
        !(Test-Path -LiteralPath $SummaryPath -PathType Leaf)) {
        throw 'CPU lifecycle proof manifest/summary write did not complete.'
    }
    [void](Get-FileRecord $ManifestPath 'proof-manifest.json')
    [void](Get-FileRecord $SummaryPath 'proof-summary.txt')
    [void]($ManifestJson | ConvertFrom-Json)
    if ((Get-SessionBytes $OutputRoot) -gt 64MB) {
        throw 'CPU lifecycle proof exceeded the 64 MiB whole-session output cap after manifest/summary.'
    }
    Remove-Item -LiteralPath $IncompletePath -Force
    if (!$RequirePass) { Write-Warning 'Proof completed as draft_pass without -RequirePass; output remains draft evidence.' }
    Write-Output "CPU lifecycle proof $($ProofStatus.ToUpperInvariant()): $OutputRoot"
} finally {
    foreach ($Name in $EnvironmentNames) {
        [Environment]::SetEnvironmentVariable($Name, $Previous[$Name], 'Process')
    }
}
