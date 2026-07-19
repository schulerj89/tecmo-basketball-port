param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [switch]$Build
)

$ErrorActionPreference = "Stop"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path $ProjectRoot).Path
$BuildDir = Join-Path $ProjectRoot "build"
$TestDir = [System.IO.Path]::GetFullPath((Join-Path $BuildDir "music_test"))
$BuildRoot = [System.IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/') +
    [System.IO.Path]::DirectorySeparatorChar
if (!$TestDir.StartsWith($BuildRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Music test directory escaped the build directory."
}

if (!$RomPath) {
    $RomPath = $env:TECMO_ROM_PATH
}
if (!$RomPath -or !(Test-Path -LiteralPath $RomPath)) {
    throw "Pass -RomPath or set TECMO_ROM_PATH to the local Rev1 iNES ROM."
}
$RomPath = (Resolve-Path -LiteralPath $RomPath).Path
$ExePath = Join-Path $BuildDir "tecmo_port.exe"
$PackPath = Join-Path $TestDir "tecmo_music.assetpack"
$ExpectedOutput = "TMUS-1 parser/state/synth: payload=05C00ECB instructions=2251 voices=37 pcm=105B1338 state=1C74513C pulse=D52B0696 tri=1C9A3181 noise=56252AAE env=6515A87A opening_ticks=2614 stinger_ticks=396 cadence=pass gate=pass startup=pass anchors=pass voice=pass pitch=pass long=pass null=pass malformed=pass output=frozen-fallback ring=8x1024"
$PreviousPack = $env:TECMO_ASSETPACK
$PreviousSkipShortcut = $env:TECMO_SKIP_SHORTCUT

function Get-ShortTail {
    param([object[]]$Lines)
    return (@($Lines | Select-Object -Last 8) -join [Environment]::NewLine)
}

function Get-AssetPackEntry {
    param(
        [byte[]]$Bytes,
        [string]$Id
    )

    if ($Bytes.Length -lt 40 -or
        [System.Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -ne "TAP1" -or
        [System.BitConverter]::ToUInt32($Bytes, 4) -ne 1 -or
        [System.BitConverter]::ToUInt32($Bytes, 8) -ne 40 -or
        [System.BitConverter]::ToUInt32($Bytes, 12) -ne 128) {
        throw "Asset pack header is not TAP1 v1."
    }
    $EntryCount = [System.BitConverter]::ToUInt32($Bytes, 16)
    $DirectoryOffset = [System.BitConverter]::ToUInt64($Bytes, 20)
    if ($DirectoryOffset -gt [uint64]$Bytes.Length -or
        [uint64]$EntryCount * 128 -gt [uint64]$Bytes.Length - $DirectoryOffset) {
        throw "Asset pack directory is out of bounds."
    }
    for ($Index = 0; $Index -lt $EntryCount; ++$Index) {
        $EntryOffset = [int]$DirectoryOffset + $Index * 128
        $IdLength = [Array]::IndexOf($Bytes, [byte]0, $EntryOffset, 64)
        if ($IdLength -lt 0) { $IdLength = $EntryOffset + 64 }
        $EntryId = [System.Text.Encoding]::ASCII.GetString(
            $Bytes, $EntryOffset, $IdLength - $EntryOffset)
        if ($EntryId -ne $Id) { continue }
        $PackOffset = [System.BitConverter]::ToUInt64($Bytes, $EntryOffset + 84)
        $ByteCount = [System.BitConverter]::ToUInt64($Bytes, $EntryOffset + 92)
        if ($PackOffset -gt [uint64]$Bytes.Length -or
            $ByteCount -gt [uint64]$Bytes.Length - $PackOffset) {
            throw "Asset pack entry is out of bounds."
        }
        return [pscustomobject]@{
            directory_offset = $EntryOffset
            pack_offset = $PackOffset
            byte_count = $ByteCount
        }
    }
    throw "Asset pack entry '$Id' was not found."
}

function Invoke-MusicTest {
    param(
        [string]$AssetPack,
        [bool]$ExpectSuccess
    )

    $env:TECMO_ASSETPACK = $AssetPack
    $Output = @(& $ExePath --music-test 2>&1)
    $ExitCode = $LASTEXITCODE
    if ($ExpectSuccess) {
        if ($ExitCode -ne 0 -or ($Output -join [Environment]::NewLine).Trim() -ne $ExpectedOutput) {
            throw "Native music golden failed.`n$(Get-ShortTail $Output)"
        }
    } elseif ($ExitCode -eq 0 -or
              (@($Output | Where-Object { $_ -match "TMUS-1" }).Count -eq 0)) {
        throw "Malformed or missing music pack was not rejected.`n$(Get-ShortTail $Output)"
    }
}

try {
    $env:TECMO_SKIP_SHORTCUT = "1"
    if ($Build) {
        $BuildOutput = @(& (Join-Path $ProjectRoot "build.ps1") 2>&1)
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed.`n$(Get-ShortTail $BuildOutput)"
        }
    }
    if (!(Test-Path -LiteralPath $ExePath)) {
        throw "Build output is missing; rerun with -Build."
    }
    if (Test-Path -LiteralPath $TestDir) {
        Remove-Item -LiteralPath $TestDir -Recurse -Force
    }
    [void](New-Item -ItemType Directory -Path $TestDir)

    $PackOutput = @(& $ExePath --build-assetpack $RomPath $PackPath 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "Rev1 music asset-pack build failed.`n$(Get-ShortTail $PackOutput)"
    }
    $PackBytes = [System.IO.File]::ReadAllBytes($PackPath)
    $MusicEntry = Get-AssetPackEntry -Bytes $PackBytes -Id "audio/music"
    if ($MusicEntry.byte_count -ne 36784) {
        throw "audio/music does not have the exact TMUS-1 size."
    }
    Invoke-MusicTest -AssetPack $PackPath -ExpectSuccess $true

    $PayloadMutationPath = Join-Path $TestDir "payload-mutated.assetpack"
    $PayloadMutation = [byte[]]$PackBytes.Clone()
    $PayloadMutation[[int]$MusicEntry.pack_offset + 128] =
        $PayloadMutation[[int]$MusicEntry.pack_offset + 128] -bxor 1
    [System.IO.File]::WriteAllBytes($PayloadMutationPath, $PayloadMutation)
    Invoke-MusicTest -AssetPack $PayloadMutationPath -ExpectSuccess $false

    $OversizedPath = Join-Path $TestDir "oversized.assetpack"
    $Oversized = [byte[]]$PackBytes.Clone()
    [System.BitConverter]::GetBytes([uint64]36785).CopyTo(
        $Oversized, [int]$MusicEntry.directory_offset + 92)
    [System.IO.File]::WriteAllBytes($OversizedPath, $Oversized)
    Invoke-MusicTest -AssetPack $OversizedPath -ExpectSuccess $false

    $MissingPath = Join-Path $TestDir "missing.assetpack"
    $Missing = [byte[]]$PackBytes.Clone()
    $Missing[[int]$MusicEntry.directory_offset] = [byte][char]'x'
    [System.IO.File]::WriteAllBytes($MissingPath, $Missing)
    Invoke-MusicTest -AssetPack $MissingPath -ExpectSuccess $false

    $Header = ([System.IO.File]::ReadAllBytes($RomPath))[0..15]
    $TrainerBytes = if (($Header[6] -band 0x04) -ne 0) { 512 } else { 0 }
    $PrgStart = 16 + $TrainerBytes
    $PrgBanks = [int]$Header[4]
    $FixedStart = $PrgStart + ($PrgBanks - 1) * 0x4000
    $MusicSourceMutations = @(
        [pscustomobject]@{ id = "audio-bank"; offset = $PrgStart + 4 * 0x4000 + (0x8AA4 - 0x8000) },
        [pscustomobject]@{ id = "directory"; offset = $PrgStart + 4 * 0x4000 + (0x8CD0 - 0x8000) },
        [pscustomobject]@{ id = "opening-track"; offset = $PrgStart + 4 * 0x4000 + (0x8CE2 - 0x8000) },
        [pscustomobject]@{ id = "gameplay-track"; offset = $PrgStart + 4 * 0x4000 + (0x92F4 - 0x8000) },
        [pscustomobject]@{ id = "presentation-track"; offset = $PrgStart + 4 * 0x4000 + (0x96C3 - 0x8000) },
        [pscustomobject]@{ id = "stinger-track"; offset = $PrgStart + 4 * 0x4000 + (0x9E13 - 0x8000) },
        [pscustomobject]@{ id = "audio-engine"; offset = $FixedStart + (0xF2F2 - 0xC000) },
        [pscustomobject]@{ id = "period-table"; offset = $FixedStart + (0xF93B - 0xC000) }
    )
    foreach ($Spec in $MusicSourceMutations) {
        $MutatedRomPath = Join-Path $TestDir ("source-{0}.nes" -f $Spec.id)
        $RejectedPackPath = Join-Path $TestDir ("source-{0}.assetpack" -f $Spec.id)
        [System.IO.File]::Copy($RomPath, $MutatedRomPath, $true)
        $File = [System.IO.File]::Open(
            $MutatedRomPath, [System.IO.FileMode]::Open,
            [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
        try {
            [void]$File.Seek([int64]$Spec.offset, [System.IO.SeekOrigin]::Begin)
            $Original = $File.ReadByte()
            if ($Original -lt 0) { throw "Music source mutation is outside the ROM." }
            [void]$File.Seek(-1, [System.IO.SeekOrigin]::Current)
            $File.WriteByte([byte]($Original -bxor 1))
        } finally {
            $File.Dispose()
        }
        $RejectOutput = @(& $ExePath --build-assetpack $MutatedRomPath $RejectedPackPath 2>&1)
        if ($LASTEXITCODE -eq 0 -or
            @($RejectOutput | Where-Object { $_ -match "music.*fingerprint" }).Count -eq 0) {
            throw "Music source mutation '$($Spec.id)' was not rejected.`n$(Get-ShortTail $RejectOutput)"
        }
        Remove-Item -LiteralPath $MutatedRomPath -Force
        if (Test-Path -LiteralPath $RejectedPackPath) {
            Remove-Item -LiteralPath $RejectedPackPath -Force
        }
    }

    $global:LASTEXITCODE = 0
    Write-Output "MUSIC TEST PASS: TMUS-1 provenance parser sequencer synth cadence startup envelope null-sink frozen-fallback malformed missing oversized source-mutations"
} finally {
    $env:TECMO_ASSETPACK = $PreviousPack
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    if (Test-Path -LiteralPath $TestDir) {
        Remove-Item -LiteralPath $TestDir -Recurse -Force
    }
}
