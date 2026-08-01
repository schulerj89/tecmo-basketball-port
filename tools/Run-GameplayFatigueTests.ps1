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
    throw "TGFT-1 tests require the exact Tecmo NBA Basketball Rev1 ROM."
}

$BuildDir = Join-Path $ProjectRoot "build"
$Executable = Join-Path $BuildDir "tecmo_port.exe"
$Scratch = [IO.Path]::GetFullPath(
    (Join-Path $BuildDir "gameplay_fatigue_test"))
$BuildPrefix = [IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/') +
    [IO.Path]::DirectorySeparatorChar
if (!$Scratch.StartsWith($BuildPrefix,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw "Gameplay-fatigue scratch path escaped build\."
}
$PackPath = Join-Path $Scratch "gameplay-fatigue.assetpack"
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

function Invoke-Fatigue([string]$AssetPack, [bool]$Success,
                        [string]$Failure = "") {
    $Output = @(& $Executable --gameplay-fatigue-test $AssetPack 2>&1)
    $Text = ($Output -join [Environment]::NewLine)
    if ($Success) {
        if ($LASTEXITCODE -ne 0 -or
            $Text -notmatch 'TGFT-1 fatigue parser/evolution self-test passed') {
            throw "Valid TGFT-1 test failed.`n$(Get-Tail $Output)"
        }
    } elseif ($LASTEXITCODE -eq 0 -or
              $Text -notmatch 'Gameplay fatigue test failed:' -or
              ($Failure -and $Text -notmatch [regex]::Escape($Failure))) {
        throw "Invalid TGFT-1 pack was accepted.`n$(Get-Tail $Output)"
    }
}

function Write-Mutation([byte[]]$Original, [string]$Name,
                        [int]$AbsoluteOffset, [string]$Failure) {
    $Bytes = [byte[]]$Original.Clone()
    $Bytes[$AbsoluteOffset] = $Bytes[$AbsoluteOffset] -bxor 1
    $Path = Join-Path $Scratch ($Name + ".assetpack")
    [IO.File]::WriteAllBytes($Path, $Bytes)
    Invoke-Fatigue $Path $false $Failure
}

try {
    $env:TECMO_SKIP_SHORTCUT = "1"
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
    if ($Build) {
        $BuildOutput = @(& (Join-Path $ProjectRoot "build.ps1") 2>&1)
        if ($LASTEXITCODE -ne 0 -or @($BuildOutput | Where-Object {
                $_ -match 'warning [A-Z]+[0-9]+:'
            }).Count -ne 0) {
            throw "Warning-free fatigue build failed.`n$(Get-Tail $BuildOutput)"
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
        throw "TGFT-1 asset-pack build failed.`n$(Get-Tail $PackOutput)"
    }
    $SourceOutput = @(& $Executable --gameplay-fatigue-test `
        $PackPath $RomPath 2>&1)
    if ($LASTEXITCODE -ne 0 -or
        ($SourceOutput -join [Environment]::NewLine) -notmatch
            'TGFT-1 fatigue parser/evolution self-test passed') {
        throw "TGFT-1 source/parser vectors failed.`n$(Get-Tail $SourceOutput)"
    }

    $PackBytes = [IO.File]::ReadAllBytes($PackPath)
    $Fatigue = Get-Entry $PackBytes "gameplay/fatigue"
    $TeamData = Get-Entry $PackBytes "menu/team-data"
    $SourceMap = Get-Entry $PackBytes "system/source-map"
    $Payload = Get-EntryBytes $PackBytes $Fatigue
    if ($Fatigue.byte_count -ne 512 -or
        (Get-Fnv1a32 $Payload) -ne "F80F170D") {
        throw "TGFT-1 canonical payload changed."
    }
    $Map = [Text.Encoding]::UTF8.GetString(
        (Get-EntryBytes $PackBytes $SourceMap)) | ConvertFrom-Json
    $Mapped = @($Map.logical_entries | Where-Object {
        $_.id -eq "gameplay/fatigue"
    })
    if ($Mapped.Count -ne 1 -or
        $Mapped[0].schema -ne "tecmo.gameplay-fatigue/TGFT-1" -or
        @($Mapped[0].source_spans).Count -ne 2 -or
        $Mapped[0].source_spans[0].fingerprint_fnv1a32 -ne "F61DFFF7" -or
        $Mapped[0].source_spans[1].fingerprint_fnv1a32 -ne "09342B88" -or
        ![bool]$Mapped[0].source_spans[1].fixed_bank -or
        ![bool]$Mapped[0].dependency.same_pack_required) {
        throw "TGFT-1 source-map provenance is incomplete."
    }
    Invoke-Fatigue $PackPath $true
    Write-Mutation $PackBytes "header" ([int]$Fatigue.pack_offset) "TGFT-1"
    Write-Mutation $PackBytes "evolution" `
        ([int]$Fatigue.pack_offset + 256) "TGFT-1"
    Write-Mutation $PackBytes "caller" `
        ([int]$Fatigue.pack_offset + 496) "TGFT-1"
    Write-Mutation $PackBytes "team-data-dependency" `
        ([int]$TeamData.pack_offset) "TTDT-1 dependency"

    $Missing = [byte[]]$PackBytes.Clone()
    $Missing[[int]$Fatigue.directory_offset] = [byte][char]'x'
    $MissingPath = Join-Path $Scratch "missing.assetpack"
    [IO.File]::WriteAllBytes($MissingPath, $Missing)
    Invoke-Fatigue $MissingPath $false "missing or wrong-sized"
    $Oversized = [byte[]]$PackBytes.Clone()
    [BitConverter]::GetBytes([uint64]513).CopyTo(
        $Oversized, [int]$Fatigue.directory_offset + 92)
    $OversizedPath = Join-Path $Scratch "oversized.assetpack"
    [IO.File]::WriteAllBytes($OversizedPath, $Oversized)
    Invoke-Fatigue $OversizedPath $false "missing or wrong-sized"
    Write-Host "TGFT-1 fatigue tests passed."
} finally {
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
}
