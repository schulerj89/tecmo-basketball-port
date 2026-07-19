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
$Scratch = [IO.Path]::GetFullPath((Join-Path $BuildDir "gameplay_court_test"))
$BuildPrefix = [IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/') +
    [IO.Path]::DirectorySeparatorChar
if (!$Scratch.StartsWith($BuildPrefix,
                         [StringComparison]::OrdinalIgnoreCase)) {
    throw "Gameplay court scratch path escaped build\."
}
$PackPath = Join-Path $Scratch "court.assetpack"
$ExpectedOutput =
    "TGCT-1 gameplay court passed: size=1024 palette=16 min=0 max=360 unique=130 nametable=0CF54A0E palette-fnv=B20C1E11"
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

function Invoke-GameplayCourtTest {
    param([string]$AssetPack, [bool]$ExpectSuccess)
    $Output = @(& $Executable --gameplay-court-test $AssetPack 2>&1)
    $ExitCode = $LASTEXITCODE
    if ($ExpectSuccess) {
        if ($ExitCode -ne 0 -or
            ($Output -join [Environment]::NewLine).Trim() -ne $ExpectedOutput) {
            throw "TGCT-1 loader golden failed.`n$(Get-ShortTail $Output)"
        }
    } elseif ($ExitCode -eq 0 -or
              @($Output | Where-Object {
                  $_ -match "TGCT-1|Gameplay court"
              }).Count -eq 0) {
        throw "Malformed TGCT-1 pack was accepted.`n$(Get-ShortTail $Output)"
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
    $Output = @(& $Executable --build-assetpack `
        $MutatedRom $MutatedPack 2>&1)
    $ExitCode = $LASTEXITCODE
    if (Test-Path -LiteralPath $MutatedPack) {
        Remove-Item -LiteralPath $MutatedPack -Force
    }
    Remove-Item -LiteralPath $MutatedRom -Force
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
            @($BuildOutput | Where-Object {
                $_ -match "warning [A-Z]+[0-9]+"
            }).Count -ne 0) {
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

    $PackOutput = @(& $Executable --build-assetpack `
        $RomPath $PackPath 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "Rev1 TGCT-1 asset-pack build failed.`n$(Get-ShortTail $PackOutput)"
    }
    $PackBytes = [IO.File]::ReadAllBytes($PackPath)
    $CourtEntry = Get-AssetPackEntry $PackBytes "gameplay/court"
    $ChrEntry = Get-AssetPackEntry $PackBytes "chr/all"
    $SourceMapEntry = Get-AssetPackEntry $PackBytes "system/source-map"
    $Payload = Get-EntryBytes $PackBytes $CourtEntry
    if ($CourtEntry.byte_count -ne 6559 -or
        (Get-Fnv1a32 $Payload) -ne "ECAB7A93" -or
        [Text.Encoding]::ASCII.GetString($Payload, 0, 4) -ne "TGCT" -or
        [BitConverter]::ToUInt16($Payload, 4) -ne 1 -or
        [BitConverter]::ToUInt16($Payload, 6) -ne 256 -or
        [BitConverter]::ToUInt32($Payload, 8) -ne 6559 -or
        [BitConverter]::ToUInt16($Payload, 12) -ne 10 -or
        [BitConverter]::ToUInt32($Payload, 36) -ne 5519 -or
        [BitConverter]::ToUInt32($Payload, 44) -ne 6543 -or
        [BitConverter]::ToUInt16($Payload, 64) -ne 360 -or
        [BitConverter]::ToUInt16($Payload, 66) -ne 130) {
        throw "gameplay/court canonical header or payload changed."
    }
    Invoke-GameplayCourtTest $PackPath $true

    $SourceMapText = [Text.Encoding]::UTF8.GetString(
        (Get-EntryBytes $PackBytes $SourceMapEntry))
    $SourceMap = $SourceMapText | ConvertFrom-Json
    $CourtMap = @($SourceMap.logical_entries | Where-Object {
        $_.id -eq "gameplay/court"
    })
    $ExpectedSources = @(
        [pscustomobject]@{ role="screen-0f-descriptor"; bank=7; fixed=$true; start=0xDCEE; size=7; hash="EEC27A7D" },
        [pscustomobject]@{ role="screen-0f-encoded-stream"; bank=0; fixed=$false; start=0xB518; size=201; hash="C869A670" },
        [pscustomobject]@{ role="screen-0f-descriptor-palette"; bank=0; fixed=$false; start=0xB5E0; size=16; hash="98634D94" },
        [pscustomobject]@{ role="court-pointer-tuple"; bank=0; fixed=$false; start=0xABEC; size=8; hash="CD524DB3" },
        [pscustomobject]@{ role="court-layout"; bank=0; fixed=$false; start=0x93C6; size=1440; hash="578BFD90" },
        [pscustomobject]@{ role="court-macro-tiles"; bank=0; fixed=$false; start=0x99C6; size=1444; hash="A6CBF6F7" },
        [pscustomobject]@{ role="court-macro-attributes"; bank=0; fixed=$false; start=0x9F6A; size=361; hash="2BE5CD2F" },
        [pscustomobject]@{ role="fixed-macro-builder-and-tables"; bank=7; fixed=$true; start=0xD5C5; size=315; hash="D9379154" },
        [pscustomobject]@{ role="fixed-court-layout-loop"; bank=7; fixed=$true; start=0xDE2D; size=111; hash="2779098E" },
        [pscustomobject]@{ role="fixed-live-background-palette"; bank=7; fixed=$true; start=0xF2E2; size=16; hash="B20C1E11" }
    )
    $SourceContractsValid = $CourtMap.Count -eq 1 -and
        @($CourtMap[0].source_spans).Count -eq $ExpectedSources.Count
    if ($SourceContractsValid) {
        foreach ($Expected in $ExpectedSources) {
            [uint64]$ExpectedSourceOffset = if ($Expected.fixed) {
                [uint64]$SourceMap.source.prg_offset +
                    ([uint64]$SourceMap.source.prg_banks - 1) * 0x4000 +
                    ($Expected.start - 0xC000)
            } else {
                [uint64]$SourceMap.source.prg_offset +
                    $Expected.bank * 0x4000 + ($Expected.start - 0x8000)
            }
            $Matches = @($CourtMap[0].source_spans | Where-Object {
                $_.role -eq $Expected.role -and
                [uint64]$_.source_offset -eq $ExpectedSourceOffset -and
                [int]$_.bank -eq $Expected.bank -and
                [bool]$_.fixed_bank -eq $Expected.fixed -and
                [int]$_.cpu_start -eq $Expected.start -and
                [int]$_.cpu_end -eq $Expected.start + $Expected.size - 1 -and
                [int]$_.size -eq $Expected.size -and
                $_.fingerprint_fnv1a32 -eq $Expected.hash
            })
            if ($Matches.Count -ne 1) {
                $SourceContractsValid = $false
                break
            }
        }
    }
    if ($CourtMap.Count -ne 1 -or
        $CourtMap[0].schema -ne "tecmo.gameplay-court/TGCT-1" -or
        [int]$CourtMap[0].size -ne 6559 -or
        $CourtMap[0].fingerprint_fnv1a32 -ne "ECAB7A93" -or
        @($CourtMap[0].dependencies).Count -ne 1 -or
        $CourtMap[0].dependencies[0].entry -ne "chr/all" -or
        [uint64]$CourtMap[0].dependencies[0].source_offset -ne
            [uint64]$SourceMap.source.chr_offset -or
        [int]$CourtMap[0].dependencies[0].size -ne 262144 -or
        $CourtMap[0].dependencies[0].fingerprint_fnv1a32 -ne "F6F6E854" -or
        $CourtMap[0].dependencies[0].fingerprint_fnv1a64 -ne "96A64F53B240ABB4" -or
        !$SourceContractsValid -or
        [int]$CourtMap[0].decoded_screen_contract.screen_id -ne 15 -or
        [int]$CourtMap[0].decoded_screen_contract.encoded_size -ne 201 -or
        $CourtMap[0].decoded_screen_contract.decoded_fingerprint_fnv1a32 -ne "483171E7" -or
        [int]$CourtMap[0].native_contract.macro_index_min -ne 0 -or
        [int]$CourtMap[0].native_contract.macro_index_max -ne 360 -or
        [int]$CourtMap[0].native_contract.unique_macro_indexes -ne 130 -or
        $CourtMap[0].native_contract.initial_nametable_fill -notmatch
            'unused lower final-row' -or
        $CourtMap[0].native_contract.nametable_fingerprint_fnv1a32 -ne "0CF54A0E" -or
        $CourtMap[0].native_contract.tile_fingerprint_fnv1a32 -ne "D2F8364A" -or
        $CourtMap[0].native_contract.attribute_fingerprint_fnv1a32 -ne "B54833D1" -or
        $CourtMap[0].native_contract.live_palette_fingerprint_fnv1a32 -ne "B20C1E11" -or
        $CourtMap[0].native_contract.boundary -notmatch "static court base only" -or
        $CourtMap[0].native_contract.runtime_inputs -notmatch "no decompilation") {
        throw "TGCT-1 source-map provenance is incomplete or malformed."
    }

    $PayloadSpans = @(
        [pscustomobject]@{ id="descriptor"; offset=576; size=7 },
        [pscustomobject]@{ id="encoded"; offset=583; size=201 },
        [pscustomobject]@{ id="descriptor-palette"; offset=784; size=16 },
        [pscustomobject]@{ id="pointer-tuple"; offset=800; size=8 },
        [pscustomobject]@{ id="layout"; offset=808; size=1440 },
        [pscustomobject]@{ id="macro-tiles"; offset=2248; size=1444 },
        [pscustomobject]@{ id="macro-attributes"; offset=3692; size=361 },
        [pscustomobject]@{ id="macro-builder"; offset=4053; size=315 },
        [pscustomobject]@{ id="layout-loop"; offset=4368; size=111 },
        [pscustomobject]@{ id="live-palette"; offset=4479; size=16 }
    )
    $PayloadMutations = @(
        [pscustomobject]@{ id="magic"; offset=0 },
        [pscustomobject]@{ id="declared-size"; offset=8 },
        [pscustomobject]@{ id="reserved"; offset=160 },
        [pscustomobject]@{ id="decoded-start"; offset=4495 },
        [pscustomobject]@{ id="decoded-middle"; offset=4495 + 512 },
        [pscustomobject]@{ id="decoded-end"; offset=5518 },
        [pscustomobject]@{ id="nametable-start"; offset=5519 },
        [pscustomobject]@{ id="nametable-middle"; offset=5519 + 512 },
        [pscustomobject]@{ id="nametable-end"; offset=6542 },
        [pscustomobject]@{ id="palette-start"; offset=6543 },
        [pscustomobject]@{ id="palette-middle"; offset=6551 },
        [pscustomobject]@{ id="palette-end"; offset=6558 }
    )
    foreach ($Span in $PayloadSpans) {
        $Offsets = @(0, [int]($Span.size / 2),
                     ([int]$Span.size - 1))
        $Labels = @("start", "middle", "end")
        for ($Point = 0; $Point -lt 3; ++$Point) {
            $PayloadMutations += [pscustomobject]@{
                id = "$($Span.id)-$($Labels[$Point])"
                offset = $Span.offset + $Offsets[$Point]
            }
        }
    }
    for ($Index = 0; $Index -lt 10; ++$Index) {
        $PayloadMutations += [pscustomobject]@{
            id = "source-record-$Index"
            offset = 256 + $Index * 32 + 16
        }
    }
    foreach ($Mutation in $PayloadMutations) {
        $Path = Join-Path $Scratch ("payload-" + $Mutation.id + ".assetpack")
        $Bytes = [byte[]]$PackBytes.Clone()
        $Absolute = [int]$CourtEntry.pack_offset + $Mutation.offset
        $Bytes[$Absolute] = $Bytes[$Absolute] -bxor 1
        [IO.File]::WriteAllBytes($Path, $Bytes)
        Invoke-GameplayCourtTest $Path $false
        Remove-Item -LiteralPath $Path -Force
    }

    $OversizedPath = Join-Path $Scratch "oversized.assetpack"
    $Oversized = [byte[]]$PackBytes.Clone()
    [BitConverter]::GetBytes([uint64]6560).CopyTo(
        $Oversized, [int]$CourtEntry.directory_offset + 92)
    [IO.File]::WriteAllBytes($OversizedPath, $Oversized)
    Invoke-GameplayCourtTest $OversizedPath $false

    $MissingPath = Join-Path $Scratch "missing-court.assetpack"
    $Missing = [byte[]]$PackBytes.Clone()
    $Missing[[int]$CourtEntry.directory_offset] = [byte][char]'x'
    [IO.File]::WriteAllBytes($MissingPath, $Missing)
    Invoke-GameplayCourtTest $MissingPath $false

    $MissingChrPath = Join-Path $Scratch "missing-chr.assetpack"
    $MissingChr = [byte[]]$PackBytes.Clone()
    $MissingChr[[int]$ChrEntry.directory_offset] = [byte][char]'x'
    [IO.File]::WriteAllBytes($MissingChrPath, $MissingChr)
    Invoke-GameplayCourtTest $MissingChrPath $false

    $CrossPackPath = Join-Path $Scratch "cross-pack-chr.assetpack"
    $CrossPack = [byte[]]$PackBytes.Clone()
    $CrossPack[[int]$ChrEntry.pack_offset] =
        $CrossPack[[int]$ChrEntry.pack_offset] -bxor 1
    [IO.File]::WriteAllBytes($CrossPackPath, $CrossPack)
    Invoke-GameplayCourtTest $CrossPackPath $false

    # This replacement preserves the full Rev1 CHR FNV32 but changes FNV64,
    # proving the same-pack dependency does not silently accept a collision.
    $Fnv64CollisionPath = Join-Path $Scratch "chr-fnv64-collision.assetpack"
    $Fnv64Collision = [byte[]]$PackBytes.Clone()
    $OriginalChr = Get-EntryBytes $PackBytes $ChrEntry
    [byte[]]$CollisionPatch = 0x02,0xD6,0xF7,0x4E,0x6E
    [Array]::Copy($CollisionPatch, 0, $Fnv64Collision,
                  [int]$ChrEntry.pack_offset, $CollisionPatch.Length)
    $CollisionChr = Get-EntryBytes $Fnv64Collision $ChrEntry
    if ((Get-Fnv1a32 $OriginalChr) -ne (Get-Fnv1a32 $CollisionChr)) {
        throw "Focused CHR FNV32 collision fixture is invalid."
    }
    [IO.File]::WriteAllBytes($Fnv64CollisionPath, $Fnv64Collision)
    Invoke-GameplayCourtTest $Fnv64CollisionPath $false

    $RomBytes = [IO.File]::ReadAllBytes($RomPath)
    $Trainer = if (($RomBytes[6] -band 4) -ne 0) { 512 } else { 0 }
    $Prg = 16 + $Trainer
    $PrgBanks = [int]$RomBytes[4]
    $Fixed = $Prg + ($PrgBanks - 1) * 0x4000
    foreach ($Source in $ExpectedSources) {
        $Base = if ($Source.fixed) {
            $Fixed + ($Source.start - 0xC000)
        } else {
            $Prg + $Source.bank * 0x4000 + ($Source.start - 0x8000)
        }
        $Offsets = @(0, [int]($Source.size / 2),
                     ([int]$Source.size - 1))
        $Labels = @("start", "middle", "end")
        for ($Point = 0; $Point -lt 3; ++$Point) {
            Invoke-RejectedRomMutation $RomBytes `
                "$($Source.role)-$($Labels[$Point])" `
                ($Base + $Offsets[$Point])
        }
    }

    $global:LASTEXITCODE = 0
    Write-Host "TGCT-1 focused tests passed: canonical/rebuild, provenance, reload, exact-size, malformed, missing, cross-pack/FNV64, payload endpoints/interiors, Rev1 source endpoints/interiors"
} finally {
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
}
