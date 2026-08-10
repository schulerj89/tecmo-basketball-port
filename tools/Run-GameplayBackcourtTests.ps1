param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [switch]$Build
)

$ErrorActionPreference = "Stop"
$ExpectedRomSha256 =
    "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4"
if (!$ProjectRoot) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if (!$RomPath) { $RomPath = $env:TECMO_ROM_PATH }
if (!$RomPath -or !(Test-Path -LiteralPath $RomPath -PathType Leaf)) {
    throw "Pass -RomPath or set TECMO_ROM_PATH to the local Rev1 iNES ROM."
}
$RomPath = (Resolve-Path -LiteralPath $RomPath).Path
if ((Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash -ne
    $ExpectedRomSha256) {
    throw "TGBC-1 tests require the exact Tecmo NBA Basketball Rev1 ROM."
}

$BuildDir = Join-Path $ProjectRoot "build"
$Executable = Join-Path $BuildDir "tecmo_port.exe"
$Scratch = [IO.Path]::GetFullPath(
    (Join-Path $BuildDir "gameplay_backcourt_test"))
$BuildPrefix = [IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/') +
    [IO.Path]::DirectorySeparatorChar
if (!$Scratch.StartsWith($BuildPrefix,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw "Gameplay-backcourt scratch path escaped build\."
}
$PackPath = Join-Path $Scratch "backcourt.assetpack"
$PreviousPack = $env:TECMO_ASSETPACK
$PreviousSkipShortcut = $env:TECMO_SKIP_SHORTCUT

function Get-Tail([object[]]$Lines) {
    return (@($Lines | Select-Object -Last 12) -join [Environment]::NewLine)
}

function Get-Fnv1a32([byte[]]$Bytes) {
    [uint32]$Hash = 2166136261
    foreach ($Byte in $Bytes) {
        [uint64]$Product = [uint64]($Hash -bxor [uint32]$Byte) *
            [uint64]16777619
        $Hash = [uint32]($Product % [uint64]4294967296)
    }
    return ("{0:X8}" -f $Hash)
}

function Get-Entry([byte[]]$Bytes, [string]$Id) {
    if ($Bytes.Length -lt 40 -or
        [Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -ne "TAP1" -or
        [BitConverter]::ToUInt32($Bytes, 12) -ne 128) {
        throw "Asset pack header is not TAP1 v1."
    }
    $Count = [BitConverter]::ToUInt32($Bytes, 16)
    $Directory = [BitConverter]::ToUInt64($Bytes, 20)
    if ($Directory -gt [uint64]$Bytes.Length -or
        [uint64]$Count * 128 -gt [uint64]$Bytes.Length - $Directory) {
        throw "Asset pack directory is out of bounds."
    }
    for ($Index = 0; $Index -lt $Count; ++$Index) {
        $Offset = [int]$Directory + $Index * 128
        $End = [Array]::IndexOf($Bytes, [byte]0, $Offset, 64)
        if ($End -lt 0) { $End = $Offset + 64 }
        if ([Text.Encoding]::ASCII.GetString(
                $Bytes, $Offset, $End - $Offset) -ne $Id) { continue }
        return [pscustomobject]@{
            directory_offset = $Offset
            pack_offset = [BitConverter]::ToUInt64($Bytes, $Offset + 84)
            byte_count = [BitConverter]::ToUInt64($Bytes, $Offset + 92)
        }
    }
    throw "Asset pack entry '$Id' was not found."
}

function Get-EntryBytes([byte[]]$Bytes, [object]$Entry) {
    $Result = New-Object byte[] ([int]$Entry.byte_count)
    [Array]::Copy($Bytes, [int64]$Entry.pack_offset,
        $Result, 0, [int64]$Entry.byte_count)
    return $Result
}

function Invoke-Backcourt([string]$AssetPack, [bool]$Success,
                          [string]$Failure = "") {
    $Output = @(& $Executable --gameplay-backcourt-test $AssetPack 2>&1)
    $Text = $Output -join [Environment]::NewLine
    if ($Success) {
        if ($LASTEXITCODE -ne 0 -or
            $Text -notmatch 'TGBC-1 parser/detector self-test passed') {
            throw "Valid TGBC-1 test failed.`n$(Get-Tail $Output)"
        }
    } elseif ($LASTEXITCODE -eq 0 -or
              $Text -notmatch 'Backcourt asset test failed:' -or
              ($Failure -and $Text -notmatch [regex]::Escape($Failure))) {
        throw "Invalid TGBC-1 pack was accepted.`n$(Get-Tail $Output)"
    }
}

function Write-Mutation([byte[]]$Original, [object]$Entry,
                        [string]$Name, [int]$PayloadOffset,
                        [string]$Failure = "TGBC-1") {
    $Bytes = [byte[]]$Original.Clone()
    $Absolute = [int]$Entry.pack_offset + $PayloadOffset
    $Bytes[$Absolute] = $Bytes[$Absolute] -bxor 1
    $Path = Join-Path $Scratch ($Name + ".assetpack")
    [IO.File]::WriteAllBytes($Path, $Bytes)
    Invoke-Backcourt $Path $false $Failure
}

function Invoke-Render([string]$Mode, [string]$Expected, [int]$Frame) {
    $Hashes = @()
    for ($Pass = 0; $Pass -lt 2; ++$Pass) {
        $Png = Join-Path $Scratch ("$Mode-$Frame-$Pass.png")
        $Output = @(& $Executable --root $ProjectRoot `
            --render-test-mode ("gameplay-$Mode-frame$Frame") $Png 2>&1)
        $Text = $Output -join [Environment]::NewLine
        if ($LASTEXITCODE -ne 0 -or
            !(Test-Path -LiteralPath $Png -PathType Leaf) -or
            $Text -notmatch 'phase=violation-presentation' -or
            $Text -notmatch
                ("phase-frame=$Frame violation=" + [regex]::Escape($Expected))) {
            throw "TGBC-1 render $Mode frame $Frame failed.`n$(Get-Tail $Output)"
        }
        $Hashes += (Get-FileHash -LiteralPath $Png -Algorithm SHA256).Hash
    }
    if ($Hashes[0] -ne $Hashes[1]) {
        throw "TGBC-1 render $Mode frame $Frame was nondeterministic."
    }
    return $Hashes[0]
}

try {
    $env:TECMO_SKIP_SHORTCUT = "1"
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
    if ($Build) {
        $BuildOutput = @(& (Join-Path $ProjectRoot "build.ps1") 2>&1)
        if ($LASTEXITCODE -ne 0 -or @($BuildOutput | Where-Object {
                $_ -match 'warning [A-Z]+[0-9]+:'
            }).Count -ne 0) {
            throw "Warning-free TGBC-1 build failed.`n$(Get-Tail $BuildOutput)"
        }
    }
    if (!(Test-Path -LiteralPath $Executable -PathType Leaf)) {
        throw "Build output is missing; rerun with -Build."
    }
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
    New-Item -ItemType Directory -Path $Scratch | Out-Null
    $PackOutput = @(& $Executable --build-assetpack $RomPath $PackPath 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "TGBC-1 asset-pack build failed.`n$(Get-Tail $PackOutput)"
    }
    $SourceOutput = @(& $Executable --gameplay-backcourt-test `
        $PackPath $RomPath 2>&1)
    if ($LASTEXITCODE -ne 0 -or
        ($SourceOutput -join [Environment]::NewLine) -notmatch
            'TGBC-1 parser/detector self-test passed') {
        throw "TGBC-1 source/parser vectors failed.`n$(Get-Tail $SourceOutput)"
    }

    $PackBytes = [IO.File]::ReadAllBytes($PackPath)
    $Backcourt = Get-Entry $PackBytes "gameplay/backcourt"
    $Orientation = Get-Entry $PackBytes "gameplay/court-orientation"
    $Penalties = Get-Entry $PackBytes "gameplay/penalties"
    $SourceMap = Get-Entry $PackBytes "system/source-map"
    $Payload = Get-EntryBytes $PackBytes $Backcourt
    if ($Backcourt.byte_count -ne 512 -or
        (Get-Fnv1a32 $Payload) -ne "810886EF" -or
        [Text.Encoding]::ASCII.GetString($Payload, 0, 4) -ne "TGBC" -or
        [BitConverter]::ToUInt16($Payload, 80) -ne 376 -or
        [BitConverter]::ToUInt16($Payload, 84) -ne 392) {
        throw "TGBC-1 canonical payload changed."
    }
    $Map = [Text.Encoding]::UTF8.GetString(
        (Get-EntryBytes $PackBytes $SourceMap)) | ConvertFrom-Json
    $Mapped = @($Map.logical_entries | Where-Object {
        $_.id -eq "gameplay/backcourt"
    })
    if ($Mapped.Count -ne 1 -or
        $Mapped[0].schema -ne "tecmo.gameplay-backcourt/TGBC-1" -or
        $Mapped[0].fingerprint_fnv1a32 -ne "810886EF" -or
        @($Mapped[0].source_spans).Count -ne 1 -or
        $Mapped[0].source_spans[0].fingerprint_fnv1a32 -ne "C137674F" -or
        $Mapped[0].native_contract.violation_selector -ne 2 -or
        $Mapped[0].excluded -notmatch 'ten-second') {
        throw "TGBC-1 source-map provenance is incomplete."
    }
    Invoke-Backcourt $PackPath $true

    foreach ($Mutation in @(
        @{ name="header"; offset=0 },
        @{ name="header-reserved"; offset=112 },
        @{ name="source-record"; offset=192 },
        @{ name="detector"; offset=224 },
        @{ name="source-padding"; offset=348 },
        @{ name="rules"; offset=352 },
        @{ name="tail-padding"; offset=384 }
    )) {
        Write-Mutation $PackBytes $Backcourt $Mutation.name $Mutation.offset
    }
    Write-Mutation $PackBytes $Orientation "orientation-dependency" 0 `
        "dependencies"
    Write-Mutation $PackBytes $Penalties "penalty-dependency" 0 `
        "dependencies"

    $Missing = [byte[]]$PackBytes.Clone()
    $Missing[[int]$Backcourt.directory_offset] = [byte][char]'x'
    $MissingPath = Join-Path $Scratch "missing.assetpack"
    [IO.File]::WriteAllBytes($MissingPath, $Missing)
    Invoke-Backcourt $MissingPath $false "missing or wrong-sized"
    $Oversized = [byte[]]$PackBytes.Clone()
    [BitConverter]::GetBytes([uint64]513).CopyTo(
        $Oversized, [int]$Backcourt.directory_offset + 92)
    $OversizedPath = Join-Path $Scratch "oversized.assetpack"
    [IO.File]::WriteAllBytes($OversizedPath, $Oversized)
    Invoke-Backcourt $OversizedPath $false "missing or wrong-sized"

    $MutatedRomPath = Join-Path $Scratch "mutated-backcourt-source.nes"
    $MutatedRom = [IO.File]::ReadAllBytes($RomPath)
    $SourceOffset = 16 + 5 * 0x4000 + (0x970B - 0x8000)
    $MutatedRom[$SourceOffset] = $MutatedRom[$SourceOffset] -bxor 1
    [IO.File]::WriteAllBytes($MutatedRomPath, $MutatedRom)
    $MutationOutput = @(& $Executable --gameplay-backcourt-test `
        $PackPath $MutatedRomPath 2>&1)
    if ($LASTEXITCODE -eq 0 -or
        ($MutationOutput -join [Environment]::NewLine) -notmatch
            'TGBC-1 import requires the exact Rev1 ROM fingerprint') {
        throw "TGBC-1 direct importer accepted a mutated source ROM."
    }

    $env:TECMO_ASSETPACK = $PackPath
    $BackcourtHashes = @{}
    foreach ($Frame in @(23, 27, 31)) {
        $BackcourtHashes[$Frame] = Invoke-Render "backcourt" "BACKCOURT" $Frame
    }
    $OutOfBoundsHash = Invoke-Render "out-of-bounds" "OUT OF BOUNDS" 23
    if ($BackcourtHashes[23] -eq $BackcourtHashes[27] -or
        $BackcourtHashes[27] -eq $BackcourtHashes[31] -or
        $BackcourtHashes[23] -eq $OutOfBoundsHash) {
        throw "TGBC-1 message or referee groups 3->4->5 collapsed together."
    }

    Write-Host (
        "TGBC-1 backcourt tests passed: exact Bank05 detector, both " +
        "orientations, hysteresis, transactional rejection, possession-owned " +
        "live trigger, BACKCOURT text, and referee groups 3->4->5.")
    $global:LASTEXITCODE = 0
} finally {
    $env:TECMO_ASSETPACK = $PreviousPack
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
}
