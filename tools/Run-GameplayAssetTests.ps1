param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [switch]$Build
)

$ErrorActionPreference = "Stop"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if (!$RomPath) {
    $RomPath = $env:TECMO_ROM_PATH
}
if (!$RomPath -or !(Test-Path -LiteralPath $RomPath -PathType Leaf)) {
    throw "Pass -RomPath or set TECMO_ROM_PATH to the local Rev1 iNES ROM."
}
$RomPath = (Resolve-Path -LiteralPath $RomPath).Path
$BuildDir = Join-Path $ProjectRoot "build"
$Executable = Join-Path $BuildDir "tecmo_port.exe"
$Scratch = [IO.Path]::GetFullPath((Join-Path $BuildDir "gameplay_asset_test"))
$BuildPrefix = [IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/') +
    [IO.Path]::DirectorySeparatorChar
if (!$Scratch.StartsWith($BuildPrefix,
                         [StringComparison]::OrdinalIgnoreCase)) {
    throw "Gameplay asset scratch path escaped build\."
}
$PackPath = Join-Path $Scratch "gameplay.assetpack"
$ExpectedOutput =
    "TGPL-1 gameplay assets passed: screens=2 sources=21 pointers=1179 chr=F6F6E854"
$PreviousSkipShortcut = $env:TECMO_SKIP_SHORTCUT

function Get-ShortTail {
    param([object[]]$Lines)
    return (@($Lines | Select-Object -Last 10) -join [Environment]::NewLine)
}

function Get-Fnv1a32 {
    param([byte[]]$Bytes)
    [uint32]$Hash = 2166136261
    foreach ($Byte in $Bytes) {
        [uint64]$Product = [uint64]($Hash -bxor [uint32]$Byte) *
            [uint64]16777619
        $Hash = [uint32]($Product % [uint64]4294967296)
    }
    return ("{0:X8}" -f $Hash)
}

function Get-AssetPackEntry {
    param([byte[]]$Bytes, [string]$Id)
    if ($Bytes.Length -lt 40 -or
        [Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -ne "TAP1" -or
        [BitConverter]::ToUInt32($Bytes, 4) -ne 1 -or
        [BitConverter]::ToUInt32($Bytes, 8) -ne 40 -or
        [BitConverter]::ToUInt32($Bytes, 12) -ne 128) {
        throw "Asset pack header is not TAP1 v1."
    }
    $EntryCount = [BitConverter]::ToUInt32($Bytes, 16)
    $DirectoryOffset = [BitConverter]::ToUInt64($Bytes, 20)
    if ($DirectoryOffset -gt [uint64]$Bytes.Length -or
        [uint64]$EntryCount * 128 -gt
            [uint64]$Bytes.Length - $DirectoryOffset) {
        throw "Asset pack directory is out of bounds."
    }
    for ($Index = 0; $Index -lt $EntryCount; ++$Index) {
        $Offset = [int]$DirectoryOffset + $Index * 128
        $Terminator = [Array]::IndexOf($Bytes, [byte]0, $Offset, 64)
        if ($Terminator -lt 0) { $Terminator = $Offset + 64 }
        $EntryId = [Text.Encoding]::ASCII.GetString(
            $Bytes, $Offset, $Terminator - $Offset)
        if ($EntryId -ne $Id) { continue }
        $PackOffset = [BitConverter]::ToUInt64($Bytes, $Offset + 84)
        $ByteCount = [BitConverter]::ToUInt64($Bytes, $Offset + 92)
        if ($PackOffset -gt [uint64]$Bytes.Length -or
            $ByteCount -gt [uint64]$Bytes.Length - $PackOffset) {
            throw "Asset pack entry '$Id' is out of bounds."
        }
        return [pscustomobject]@{
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
    [Array]::Copy($PackBytes, [int]$Entry.pack_offset,
                  $Result, 0, $Result.Length)
    return $Result
}

function Invoke-GameplayAssetTest {
    param([string]$AssetPack, [bool]$ExpectSuccess)
    $Output = @(& $Executable --gameplay-assets-test $AssetPack 2>&1)
    $ExitCode = $LASTEXITCODE
    if ($ExpectSuccess) {
        if ($ExitCode -ne 0 -or
            ($Output -join [Environment]::NewLine).Trim() -ne $ExpectedOutput) {
            throw "TGPL-1 loader golden failed.`n$(Get-ShortTail $Output)"
        }
    } elseif ($ExitCode -eq 0 -or
              @($Output | Where-Object { $_ -match "TGPL-1|Gameplay asset" }).Count -eq 0) {
        throw "Malformed TGPL-1 pack was accepted.`n$(Get-ShortTail $Output)"
    }
}

function Invoke-RejectedRomMutation {
    param(
        [byte[]]$Original,
        [string]$Id,
        [int]$Offset
    )
    $MutatedRom = Join-Path $Scratch ("rom-" + $Id + ".nes")
    $MutatedPack = Join-Path $Scratch ("rom-" + $Id + ".assetpack")
    $Bytes = [byte[]]$Original.Clone()
    $Bytes[$Offset] = $Bytes[$Offset] -bxor 1
    [IO.File]::WriteAllBytes($MutatedRom, $Bytes)
    $Output = @(& $Executable --build-assetpack $MutatedRom $MutatedPack 2>&1)
    $ExitCode = $LASTEXITCODE
    if (Test-Path -LiteralPath $MutatedPack) {
        Remove-Item -LiteralPath $MutatedPack -Force
    }
    if ($ExitCode -eq 0) {
        throw "Rev1 source mutation '$Id' was not rejected.`n$(Get-ShortTail $Output)"
    }
}

try {
    $env:TECMO_SKIP_SHORTCUT = "1"
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
    if ($Build) {
        $BuildOutput = @(& (Join-Path $ProjectRoot "build.ps1") 2>&1)
        if ($LASTEXITCODE -ne 0 -or
            @($BuildOutput | Where-Object { $_ -match "warning [A-Z]+[0-9]+" }).Count -ne 0) {
            throw "Warning-free build failed.`n$(Get-ShortTail $BuildOutput)"
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
        throw "Rev1 TGPL-1 asset-pack build failed.`n$(Get-ShortTail $PackOutput)"
    }
    $PackBytes = [IO.File]::ReadAllBytes($PackPath)
    $GameplayEntry = Get-AssetPackEntry $PackBytes "gameplay/core"
    $ChrEntry = Get-AssetPackEntry $PackBytes "chr/all"
    $SourceMapEntry = Get-AssetPackEntry $PackBytes "system/source-map"
    $Payload = Get-EntryBytes $PackBytes $GameplayEntry
    if ($GameplayEntry.byte_count -ne 35794 -or
        (Get-Fnv1a32 $Payload) -ne "1D7897A1") {
        throw "gameplay/core size or canonical fingerprint changed."
    }
    Invoke-GameplayAssetTest $PackPath $true

    $SourceMapText = [Text.Encoding]::UTF8.GetString(
        (Get-EntryBytes $PackBytes $SourceMapEntry))
    $SourceMap = $SourceMapText | ConvertFrom-Json
    $GameplayMap = @($SourceMap.logical_entries | Where-Object {
        $_.id -eq "gameplay/core"
    })
    if ($GameplayMap.Count -ne 1 -or
        $GameplayMap[0].schema -ne "tecmo.gameplay/TGPL-1" -or
        $GameplayMap[0].size -ne 35794 -or
        $GameplayMap[0].fingerprint_fnv1a32 -ne "1D7897A1" -or
        @($GameplayMap[0].screens).Count -ne 2 -or
        @($GameplayMap[0].source_spans).Count -ne 21 -or
        @($GameplayMap[0].source_spans | Where-Object {
            $_.role -eq "scoreboard-violation-dispatch-and-text" -and
            $_.cpu_start -eq 0xBE87 -and $_.cpu_end -eq 0xBFA8
        }).Count -ne 1 -or
        @($GameplayMap[0].source_spans | Where-Object {
            $_.role -eq "foul-overlay-and-text" -and
            $_.cpu_start -eq 0xB0F8 -and $_.cpu_end -eq 0xB2B0
        }).Count -ne 1 -or
        @($GameplayMap[0].source_spans | Where-Object {
            $_.role -eq "halftime-final-banner-loop-and-data" -and
            $_.cpu_start -eq 0xBC3C -and $_.cpu_end -eq 0xBD10
        }).Count -ne 1) {
        throw "TGPL-1 source-map provenance is incomplete or malformed."
    }

    $Mutations = @(
        @{ id="magic"; offset=0 },
        @{ id="declared-size"; offset=8 },
        @{ id="header-reserved"; offset=228 },
        @{ id="screen-record"; offset=256 + 24 },
        @{ id="source-record"; offset=384 + 16 },
        @{ id="actor-pointer"; offset=15797 },
        @{ id="period-data"; offset=21384 },
        @{ id="event-data"; offset=21538 }
    )
    foreach ($Mutation in $Mutations) {
        $Path = Join-Path $Scratch ("payload-" + $Mutation.id + ".assetpack")
        $Bytes = [byte[]]$PackBytes.Clone()
        $Absolute = [int]$GameplayEntry.pack_offset + $Mutation.offset
        $Bytes[$Absolute] = $Bytes[$Absolute] -bxor 1
        [IO.File]::WriteAllBytes($Path, $Bytes)
        Invoke-GameplayAssetTest $Path $false
    }

    $OversizedPath = Join-Path $Scratch "oversized.assetpack"
    $Oversized = [byte[]]$PackBytes.Clone()
    [BitConverter]::GetBytes([uint64]35795).CopyTo(
        $Oversized, [int]$GameplayEntry.directory_offset + 92)
    [IO.File]::WriteAllBytes($OversizedPath, $Oversized)
    Invoke-GameplayAssetTest $OversizedPath $false

    $MissingPath = Join-Path $Scratch "missing.assetpack"
    $Missing = [byte[]]$PackBytes.Clone()
    $Missing[[int]$GameplayEntry.directory_offset] = [byte][char]'x'
    [IO.File]::WriteAllBytes($MissingPath, $Missing)
    Invoke-GameplayAssetTest $MissingPath $false

    $CrossPackPath = Join-Path $Scratch "cross-pack-chr.assetpack"
    $CrossPack = [byte[]]$PackBytes.Clone()
    $CrossPack[[int]$ChrEntry.pack_offset] =
        $CrossPack[[int]$ChrEntry.pack_offset] -bxor 1
    [IO.File]::WriteAllBytes($CrossPackPath, $CrossPack)
    Invoke-GameplayAssetTest $CrossPackPath $false

    $RomBytes = [IO.File]::ReadAllBytes($RomPath)
    $Trainer = if (($RomBytes[6] -band 4) -ne 0) { 512 } else { 0 }
    $Prg = 16 + $Trainer
    $PrgBanks = [int]$RomBytes[4]
    $Fixed = $Prg + ($PrgBanks - 1) * 0x4000
    $Chr = $Prg + $PrgBanks * 0x4000
    $RomMutations = @(
        @{ id="descriptor"; offset=$Fixed + (0xDD42 - 0xC000) },
        @{ id="screen-stream"; offset=$Prg + 1 * 0x4000 + (0xB5EB - 0x8000) },
        @{ id="actor-record"; offset=$Prg + 1 * 0x4000 },
        @{ id="actor-pointer"; offset=$Prg + 1 * 0x4000 + (0xA5B9 - 0x8000) },
        @{ id="shot-rule"; offset=$Prg + 5 * 0x4000 + (0x91BC - 0x8000) },
        @{ id="scoreboard"; offset=$Prg + 3 * 0x4000 + (0xBE87 - 0x8000) },
        @{ id="foul"; offset=$Prg + 2 * 0x4000 + (0xB0F8 - 0x8000) },
        @{ id="halftime"; offset=$Prg + 6 * 0x4000 + (0xBC3C - 0x8000) },
        @{ id="sprite-selector"; offset=$Fixed + (0xF24D - 0xC000) },
        @{ id="chr"; offset=$Chr + 0x10000 }
    )
    foreach ($Mutation in $RomMutations) {
        Invoke-RejectedRomMutation $RomBytes $Mutation.id $Mutation.offset
    }

    Write-Host "TGPL-1 focused tests passed: canonical, provenance, poses, malformed, cross-pack, Rev1 mutations"
} finally {
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
}
