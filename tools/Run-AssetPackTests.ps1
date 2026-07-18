param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [switch]$Build,
    [string]$AssetPackPath,
    [string]$ReportPath
)

$ErrorActionPreference = "Stop"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path $ProjectRoot).Path

$BuildDir = Join-Path $ProjectRoot "build"
$CanonicalAssetPackPath = [System.IO.Path]::GetFullPath((Join-Path $BuildDir "tecmo.assetpack"))

if (!$AssetPackPath) {
    $AssetPackPath = Join-Path $BuildDir "asset_pack_test\tecmo_test.assetpack"
}
if (![System.IO.Path]::IsPathRooted($AssetPackPath)) {
    $AssetPackPath = Join-Path $ProjectRoot $AssetPackPath
}
$AssetPackPath = [System.IO.Path]::GetFullPath($AssetPackPath)

if (!$ReportPath) {
    $ReportPath = Join-Path $BuildDir "asset_pack_test_report.json"
}
if (![System.IO.Path]::IsPathRooted($ReportPath)) {
    $ReportPath = Join-Path $ProjectRoot $ReportPath
}
$ReportPath = [System.IO.Path]::GetFullPath($ReportPath)

function Find-ReferenceRom {
    if ($RomPath) {
        if (!(Test-Path $RomPath)) {
            throw "RomPath does not exist."
        }
        return (Resolve-Path $RomPath).Path
    }
    if ($env:TECMO_ROM_PATH -and (Test-Path $env:TECMO_ROM_PATH)) {
        return (Resolve-Path $env:TECMO_ROM_PATH).Path
    }

    return $null
}

function Get-InesHeaderInfo {
    param([string]$Path)

    $File = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
    try {
        $Header = New-Object byte[] 16
        $Read = $File.Read($Header, 0, $Header.Length)
        if ($Read -ne 16 -or
            $Header[0] -ne [byte][char]'N' -or
            $Header[1] -ne [byte][char]'E' -or
            $Header[2] -ne [byte][char]'S' -or
            $Header[3] -ne 0x1A) {
            throw "Reference ROM is not an iNES file."
        }

        return [pscustomobject]@{
            prg_banks = [int]$Header[4]
            chr_banks = [int]$Header[5]
            mapper = (($Header[6] -shr 4) -bor ($Header[7] -band 0xF0))
            trainer_bytes = if (($Header[6] -band 0x04) -ne 0) { 512 } else { 0 }
        }
    } finally {
        $File.Dispose()
    }
}

function Read-FileBytesAtOffset {
    param(
        [string]$Path,
        [uint64]$Offset,
        [int]$Count
    )

    $File = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
    try {
        if ($Count -lt 0 -or $Offset -gt [uint64]$File.Length -or [uint64]$Count -gt ([uint64]$File.Length - $Offset)) {
            throw "Requested byte range is outside the file."
        }
        [void]$File.Seek([int64]$Offset, [System.IO.SeekOrigin]::Begin)
        $Bytes = New-Object byte[] $Count
        if ($File.Read($Bytes, 0, $Count) -ne $Count) {
            throw "Requested byte range is truncated."
        }
        return $Bytes
    } finally {
        $File.Dispose()
    }
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

function ConvertTo-RepoRelativePath {
    param([string]$Path)

    $FullPath = [System.IO.Path]::GetFullPath($Path)
    $FullRoot = [System.IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    if ($FullPath.StartsWith($FullRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $FullPath.Substring($FullRoot.Length).Replace('\', '/')
    }
    return "<outside-repo>"
}

function Get-SafeExceptionName {
    param($ErrorRecord)

    if ($ErrorRecord -and $ErrorRecord.Exception) {
        return $ErrorRecord.Exception.GetType().Name
    }
    return "Error"
}

function Read-AssetPackDirectory {
    param([string]$Path)

    $File = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
    $Reader = [System.IO.BinaryReader]::new($File)
    try {
        $Magic = [System.Text.Encoding]::ASCII.GetString($Reader.ReadBytes(4))
        $Version = $Reader.ReadUInt32()
        $HeaderSize = $Reader.ReadUInt32()
        $EntrySize = $Reader.ReadUInt32()
        $EntryCount = $Reader.ReadUInt32()
        $DirectoryOffset = $Reader.ReadUInt64()
        $DataOffset = $Reader.ReadUInt64()
        [void]$Reader.ReadUInt32()

        if ($Magic -ne "TAP1" -or $Version -ne 1 -or $HeaderSize -ne 40 -or $EntrySize -ne 128) {
            throw "Asset pack header is not the expected TAP1 v1 format."
        }
        if ($DataOffset -ne 40) {
            throw "Asset pack data offset is not the expected header size."
        }
        if ($DirectoryOffset -gt [uint64]$File.Length) {
            throw "Asset pack directory offset is outside the file."
        }
        if (($DirectoryOffset + ([uint64]$EntryCount * [uint64]$EntrySize)) -gt [uint64]$File.Length) {
            throw "Asset pack directory extends outside the file."
        }

        [void]$File.Seek([int64]$DirectoryOffset, [System.IO.SeekOrigin]::Begin)
        $Ids = [System.Collections.Generic.List[string]]::new()
        $Entries = [System.Collections.Generic.List[object]]::new()
        for ($Index = 0; $Index -lt $EntryCount; ++$Index) {
            $EntryBytes = $Reader.ReadBytes($EntrySize)
            if ($EntryBytes.Length -ne $EntrySize) {
                throw "Asset pack directory entry is truncated."
            }
            $IdLength = [Array]::IndexOf($EntryBytes, [byte]0, 0, 64)
            if ($IdLength -lt 0) {
                $IdLength = 64
            }
            $Id = [System.Text.Encoding]::ASCII.GetString($EntryBytes, 0, $IdLength)
            $Type = [System.BitConverter]::ToUInt32($EntryBytes, 64)
            $Bank = [System.BitConverter]::ToUInt32($EntryBytes, 68)
            $CpuAddress = [System.BitConverter]::ToUInt32($EntryBytes, 72)
            $SourceOffset = [System.BitConverter]::ToUInt64($EntryBytes, 76)
            $PackOffset = [System.BitConverter]::ToUInt64($EntryBytes, 84)
            $ByteCount = [System.BitConverter]::ToUInt64($EntryBytes, 92)
            $Flags = [System.BitConverter]::ToUInt32($EntryBytes, 100)
            if ($PackOffset -lt $DataOffset -or $PackOffset -gt $DirectoryOffset -or $ByteCount -gt ($DirectoryOffset - $PackOffset)) {
                throw "Asset pack directory entry payload bounds are invalid."
            }
            [void]$Ids.Add($Id)
            [void]$Entries.Add([pscustomobject]@{
                id = $Id
                type = $Type
                bank = $Bank
                cpu_address = $CpuAddress
                source_offset = $SourceOffset
                pack_offset = $PackOffset
                byte_count = $ByteCount
                flags = $Flags
            })
        }

        return [pscustomobject]@{
            version = $Version
            entry_count = $EntryCount
            ids = $Ids
            entries = $Entries
        }
    } finally {
        $Reader.Dispose()
        $File.Dispose()
    }
}

function Read-AssetPackEntryBytes {
    param(
        [string]$Path,
        [object]$Directory,
        [string]$EntryId
    )

    $Entry = @($Directory.entries | Where-Object { $_.id -eq $EntryId } | Select-Object -First 1)
    if (!$Entry) {
        throw "Asset pack entry '$EntryId' was not found."
    }
    if ([uint64]$Entry.byte_count -gt [uint64][int]::MaxValue) {
        throw "Asset pack entry '$EntryId' is too large for test inspection."
    }

    $File = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
    try {
        [void]$File.Seek([int64]$Entry.pack_offset, [System.IO.SeekOrigin]::Begin)
        $Bytes = New-Object byte[] ([int]$Entry.byte_count)
        $Read = $File.Read($Bytes, 0, $Bytes.Length)
        if ($Read -ne $Bytes.Length) {
            throw "Asset pack entry '$EntryId' payload is truncated."
        }
        return $Bytes
    } finally {
        $File.Dispose()
    }
}

function Read-AssetPackEntryText {
    param(
        [string]$Path,
        [object]$Directory,
        [string]$EntryId
    )

    $Bytes = Read-AssetPackEntryBytes -Path $Path -Directory $Directory -EntryId $EntryId
    return [System.Text.Encoding]::UTF8.GetString($Bytes)
}

function Get-ExpectedAssetPackEntries {
    param(
        [int]$PrgBanks,
        [int]$ChrBanks
    )

    $Entries = [System.Collections.Generic.List[string]]::new()
    [void]$Entries.Add("system/manifest")
    [void]$Entries.Add("system/source-map")
    for ($Index = 0; $Index -lt $PrgBanks; ++$Index) {
        [void]$Entries.Add(("prg/bank{0:D2}" -f $Index))
    }
    [void]$Entries.Add("prg/fixed")
    if ($ChrBanks -gt 0) {
        [void]$Entries.Add("chr/all")
        for ($Index = 0; $Index -lt $ChrBanks; ++$Index) {
            [void]$Entries.Add(("chr/bank{0:D2}" -f $Index))
        }
    }
    return $Entries.ToArray()
}

function New-StringSet {
    param([string[]]$Values)

    $Set = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($Value in $Values) {
        [void]$Set.Add($Value)
    }
    return $Set
}

function Get-KnownLogicalAssetPackEntries {
    return @(
        "arena/intro/script",
        "arena/intro/background-layer",
        "arena/intro/palette-cycle",
        "arena/intro/sprite-groups",
        "arena/intro/ready-screen",
        "arena/intro/warriors-transition",
        "arena/intro/clippers-transition",
        "arena/intro/bucks-transition",
        "arena/intro/pass-transition",
        "intro/tecmo-presents-screen",
        "intro/nba-license-screen",
        "intro/finale-sequence",
        "title/attract-continuation",
        "title/start-screen",
        "menu/start-game",
        "roster/table.tsv",
        "title/original-text.txt",
        "title/glyph-map.tsv",
        "intro/arena/capture.ndjson",
        "intro/arena/emu_intro_memory_watch.ndjson",
        "intro/arena/emu_intro_arena_irq_watch.ndjson",
        "intro/post-arena/emu_intro_memory_watch.ndjson",
        "intro/post-arena/capture.ndjson",
        "intro/captures/source-map"
    )
}

function Get-KnownLogicalAssetPackEntryTypes {
    $DataType = [System.BitConverter]::ToUInt32([System.Text.Encoding]::ASCII.GetBytes("DATA"), 0)
    $MetaType = [System.BitConverter]::ToUInt32([System.Text.Encoding]::ASCII.GetBytes("META"), 0)
    $Types = [System.Collections.Generic.Dictionary[string, uint32]]::new([System.StringComparer]::Ordinal)

    foreach ($EntryId in Get-KnownLogicalAssetPackEntries) {
        $Types[$EntryId] = $DataType
    }
    $Types["intro/captures/source-map"] = $MetaType
    return $Types
}

function Get-ExpectedLogicalAssetPackEntries {
    return [pscustomobject]@{
        entries = @(
            "arena/intro/script",
            "arena/intro/background-layer",
            "arena/intro/palette-cycle",
            "arena/intro/sprite-groups",
            "arena/intro/ready-screen",
            "arena/intro/warriors-transition",
            "arena/intro/clippers-transition",
            "arena/intro/bucks-transition",
            "arena/intro/pass-transition",
            "intro/tecmo-presents-screen",
            "intro/nba-license-screen",
            "intro/finale-sequence"
            "title/attract-continuation"
            "title/start-screen"
            "menu/start-game"
        )
        capture_sources_present = @()
    }
}

function Get-DuplicateValues {
    param([string[]]$Values)

    $Seen = @{}
    $Duplicates = [System.Collections.Generic.List[string]]::new()
    foreach ($Value in $Values) {
        if ($Seen.ContainsKey($Value)) {
            if (!$Duplicates.Contains($Value)) {
                [void]$Duplicates.Add($Value)
            }
        } else {
            $Seen[$Value] = $true
        }
    }
    return $Duplicates.ToArray()
}

function Get-AssetPackSizeMismatches {
    param(
        [object]$Directory,
        [int]$PrgBanks,
        [int]$ChrBanks
    )

    $EntryById = @{}
    foreach ($Entry in $Directory.entries) {
        if (!$EntryById.ContainsKey($Entry.id)) {
            $EntryById[$Entry.id] = $Entry
        }
    }

    $ExpectedSizes = @{}
    for ($Index = 0; $Index -lt $PrgBanks; ++$Index) {
        $ExpectedSizes[("prg/bank{0:D2}" -f $Index)] = 16384
    }
    $ExpectedSizes["prg/fixed"] = 16384
    if ($ChrBanks -gt 0) {
        $ExpectedSizes["chr/all"] = 8192 * $ChrBanks
        for ($Index = 0; $Index -lt $ChrBanks; ++$Index) {
            $ExpectedSizes[("chr/bank{0:D2}" -f $Index)] = 8192
        }
    }

    $Mismatches = [System.Collections.Generic.List[object]]::new()
    foreach ($Id in $ExpectedSizes.Keys) {
        if (!$EntryById.ContainsKey($Id)) {
            continue
        }
        $ExpectedBytes = [uint64]$ExpectedSizes[$Id]
        $ActualBytes = [uint64]$EntryById[$Id].byte_count
        if ($ActualBytes -ne $ExpectedBytes) {
            [void]$Mismatches.Add([pscustomobject]@{
                id = $Id
                expected_bytes = $ExpectedBytes
                actual_bytes = $ActualBytes
            })
        }
    }

    return $Mismatches.ToArray()
}

function Test-OpeningScreenPayloadContract {
    param(
        [byte[]]$Bytes,
        [uint64]$ChrByteCount,
        [ValidateSet("presents", "license")]
        [string]$Kind
    )

    $Issues = [System.Collections.Generic.List[string]]::new()
    $HeaderSize = 64
    $CellCount = 960
    $CellStride = 6
    $CellsOffset = 64
    $PalettesOffset = $CellsOffset + $CellCount * $CellStride
    $SpriteStride = 12
    $IsPresents = $Kind -eq "presents"
    $ExpectedKind = if ($IsPresents) { 0 } else { 1 }
    $StageCount = if ($IsPresents) { 9 } else { 6 }
    $PaletteStride = if ($IsPresents) { 32 } else { 16 }
    $Duration = if ($IsPresents) { 133 } else { 277 }
    $SpriteCount = if ($IsPresents) { 20 } else { 0 }
    $SpriteFirstFrame = 0
    $SpriteHideFrame = if ($IsPresents) { 131 } else { 0 }
    $ExpectedFrames = if ($IsPresents) {
        @(0, 4, 8, 12, 16, 123, 125, 127, 129)
    } else {
        @(0, 36, 40, 44, 48, 275)
    }
    $FramesOffset = $PalettesOffset + $StageCount * $PaletteStride
    $SpritesOffset = $FramesOffset + $StageCount * 2
    $PayloadSize = $SpritesOffset + $SpriteCount * $SpriteStride

    if ($null -eq $Bytes -or $Bytes.Length -ne $PayloadSize) {
        [void]$Issues.Add("payload-size")
        return [pscustomobject]@{ passed = $false; issues = $Issues.ToArray() }
    }

    if ([System.Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -ne "TISC") {
        [void]$Issues.Add("magic")
    }
    $ExpectedU16 = @{
        4 = 1
        6 = $HeaderSize
        8 = 32
        10 = 30
        12 = $CellStride
        14 = $ExpectedKind
        24 = $StageCount
        26 = $PaletteStride
        36 = $Duration
        38 = $SpriteCount
        48 = $SpriteStride
        50 = $SpriteFirstFrame
        52 = $SpriteHideFrame
    }
    $ExpectedU32 = @{
        16 = $CellCount
        20 = $CellsOffset
        28 = $PalettesOffset
        32 = $FramesOffset
        40 = $PayloadSize
        44 = $SpritesOffset
    }
    foreach ($Offset in $ExpectedU16.Keys) {
        if ([System.BitConverter]::ToUInt16($Bytes, [int]$Offset) -ne
            [uint16]$ExpectedU16[$Offset]) {
            [void]$Issues.Add("u16-$Offset")
        }
    }
    foreach ($Offset in $ExpectedU32.Keys) {
        if ([System.BitConverter]::ToUInt32($Bytes, [int]$Offset) -ne
            [uint32]$ExpectedU32[$Offset]) {
            [void]$Issues.Add("u32-$Offset")
        }
    }
    for ($Index = 54; $Index -lt $HeaderSize; ++$Index) {
        if ($Bytes[$Index] -ne 0) {
            [void]$Issues.Add("header-reserved")
            break
        }
    }

    $NonblankCells = 0
    for ($Index = 0; $Index -lt $CellCount; ++$Index) {
        $Offset = $CellsOffset + $Index * $CellStride
        $ChrOffset = [System.BitConverter]::ToUInt32($Bytes, $Offset + 2)
        if ($Bytes[$Offset] -ne 0xFF) { ++$NonblankCells }
        if ($Bytes[$Offset + 1] -gt 3 -or ($ChrOffset % 16) -ne 0 -or
            [uint64]$ChrOffset + 16 -gt $ChrByteCount) {
            [void]$Issues.Add("cell-$Index")
        }
    }
    $ExpectedNonblankCells = if ($IsPresents) { 58 } else { 104 }
    if ($NonblankCells -ne $ExpectedNonblankCells) {
        [void]$Issues.Add("nonblank-cell-count")
    }
    for ($Index = 0; $Index -lt $StageCount * $PaletteStride; ++$Index) {
        if ($Bytes[$PalettesOffset + $Index] -gt 0x3F) {
            [void]$Issues.Add("palette-range")
            break
        }
    }
    for ($Index = 0; $Index -lt $StageCount; ++$Index) {
        if ([System.BitConverter]::ToUInt16($Bytes, $FramesOffset + $Index * 2) -ne
            [uint16]$ExpectedFrames[$Index]) {
            [void]$Issues.Add("palette-frames")
            break
        }
    }
    for ($Color = 0; $Color -lt $PaletteStride; ++$Color) {
        $Target = $Bytes[$PalettesOffset + 4 * $PaletteStride + $Color]
        if ($Bytes[$PalettesOffset + $Color] -ne 0x0F) {
            [void]$Issues.Add("palette-black-stage")
            break
        }
        for ($Stage = 1; $Stage -le 3; ++$Stage) {
            if ($Target -eq 0x0F) {
                $ExpectedColor = 0x0F
            } else {
                $High = $Target -band 0x30
                $Maximum = ($Stage - 1) * 0x10
                if ($High -gt $Maximum) { $High = $Maximum }
                $ExpectedColor = $High -bor ($Target -band 0x0F)
            }
            if ($Bytes[$PalettesOffset + $Stage * $PaletteStride + $Color] -ne
                [byte]$ExpectedColor) {
                [void]$Issues.Add("palette-fade-in")
                break
            }
        }
        if ($IsPresents) {
            $Current = [int]$Target
            for ($Stage = 5; $Stage -lt $StageCount; ++$Stage) {
                if ($Current -eq 0x0F) {
                    $Current = 0x0F
                } elseif (($Current -band 0x30) -ge 0x10) {
                    $Current -= 0x10
                } else {
                    $Current = 0x0F
                }
                if ($Bytes[$PalettesOffset + $Stage * $PaletteStride + $Color] -ne
                    [byte]$Current) {
                    [void]$Issues.Add("palette-fade-out")
                    break
                }
            }
        } elseif ($Bytes[$PalettesOffset + 5 * $PaletteStride + $Color] -ne 0x0F) {
            [void]$Issues.Add("palette-final-black")
            break
        }
    }
    for ($Index = 0; $Index -lt $SpriteCount; ++$Index) {
        $Offset = $SpritesOffset + $Index * $SpriteStride
        $ChrOffset = [System.BitConverter]::ToUInt32($Bytes, $Offset + 4)
        $SpriteX = [System.BitConverter]::ToInt16($Bytes, $Offset)
        $SpriteY = [System.BitConverter]::ToInt16($Bytes, $Offset + 2)
        if ($Bytes[$Offset + 8] -gt 3 -or
            ($Bytes[$Offset + 9] -band 0xFC) -ne 0 -or
            [System.BitConverter]::ToUInt16($Bytes, $Offset + 10) -ne 0 -or
            $SpriteX -lt 80 -or $SpriteX -gt 104 -or
            $SpriteY -lt 64 -or $SpriteY -gt 120 -or
            ($ChrOffset % 16) -ne 0 -or
            [uint64]$ChrOffset + 16 -gt $ChrByteCount) {
            [void]$Issues.Add("sprite-$Index")
        }
    }

    return [pscustomobject]@{
        passed = $Issues.Count -eq 0
        issues = $Issues.ToArray()
    }
}

function Test-FinalePayloadContract {
    param(
        [byte[]]$Bytes,
        [uint64]$ChrByteCount
    )

    $Issues = [System.Collections.Generic.List[string]]::new()
    $HeaderSize = 192
    $ScreenCount = 5
    $CellsPerScreen = 1920
    $CellStride = 6
    $ScreensOffset = 192
    $BackgroundPalettesOffset = 57792
    $ReversePalettesOffset = 57872
    $ReverseFramesOffset = 57952
    $GroupsOffset = 57964
    $SpritePalettesOffset = 57996
    $PiecesOffset = 58028
    $RoutesOffset = 58188
    $AnchorsOffset = 58228
    $ReverseMetadataOffset = 58248
    $TitleMetadataOffset = 58264
    $BandsOffset = 58296
    $TitleSlotsOffset = 58344
    $PayloadSize = 59752

    if ($null -eq $Bytes -or $Bytes.Length -ne $PayloadSize) {
        [void]$Issues.Add("payload-size")
        return [pscustomobject]@{ passed = $false; issues = $Issues.ToArray() }
    }

    $Magic = [System.Text.Encoding]::ASCII.GetString($Bytes, 0, 4)
    $ExpectedU16 = @{
        4 = 1; 6 = $HeaderSize; 8 = $ScreenCount; 10 = 32; 12 = 30; 14 = 2; 16 = $CellStride
        18 = 5; 28 = 5; 30 = 2; 40 = 2; 42 = 16; 48 = 16; 50 = 16; 52 = 10
        54 = 0; 60 = 5; 62 = 8; 68 = 9; 70 = 2; 76 = 16; 78 = 32
        88 = 44; 90 = 26; 92 = 32; 94 = 4; 100 = 3; 102 = 16; 108 = 1; 110 = 1
    }
    $ExpectedU32 = @{
        20 = $ScreensOffset; 24 = $BackgroundPalettesOffset; 32 = $ReversePalettesOffset
        36 = $ReverseFramesOffset; 44 = $GroupsOffset; 56 = $PiecesOffset; 64 = $RoutesOffset
        72 = $AnchorsOffset; 80 = $ReverseMetadataOffset; 84 = $TitleMetadataOffset
        96 = $TitleSlotsOffset; 104 = $BandsOffset; 112 = $PayloadSize
    }
    if ($Magic -ne "TFIN") { [void]$Issues.Add("magic") }
    foreach ($Offset in $ExpectedU16.Keys) {
        if ([System.BitConverter]::ToUInt16($Bytes, [int]$Offset) -ne [uint16]$ExpectedU16[$Offset]) {
            [void]$Issues.Add("u16-$Offset")
        }
    }
    foreach ($Offset in $ExpectedU32.Keys) {
        if ([System.BitConverter]::ToUInt32($Bytes, [int]$Offset) -ne [uint32]$ExpectedU32[$Offset]) {
            [void]$Issues.Add("u32-$Offset")
        }
    }
    for ($Index = 106; $Index -le 107; ++$Index) {
        if ($Bytes[$Index] -ne 0) { [void]$Issues.Add("header-offset-high-reserved"); break }
    }
    for ($Index = 116; $Index -lt $HeaderSize; ++$Index) {
        if ($Bytes[$Index] -ne 0) { [void]$Issues.Add("header-reserved"); break }
    }
    for ($Index = $ReverseFramesOffset + 10; $Index -lt $GroupsOffset; ++$Index) {
        if ($Bytes[$Index] -ne 0) { [void]$Issues.Add("alignment-reserved"); break }
    }
    for ($Index = $AnchorsOffset + 9 * 2; $Index -lt $ReverseMetadataOffset; ++$Index) {
        if ($Bytes[$Index] -ne 0) { [void]$Issues.Add("post-anchor-reserved"); break }
    }

    $InvalidScreenCells = 0
    for ($Screen = 0; $Screen -lt $ScreenCount; ++$Screen) {
        for ($CellIndex = 0; $CellIndex -lt $CellsPerScreen; ++$CellIndex) {
            $Offset = $ScreensOffset + ($Screen * $CellsPerScreen + $CellIndex) * $CellStride
            $Palette = $Bytes[$Offset + 1]
            $ChrOffset = [System.BitConverter]::ToUInt32($Bytes, $Offset + 2)
            if ($Palette -gt 3 -or ($ChrOffset % 16) -ne 0 -or
                [uint64]$ChrOffset + 16 -gt $ChrByteCount) {
                ++$InvalidScreenCells
            }
        }
    }
    if ($InvalidScreenCells -ne 0) { [void]$Issues.Add("screen-cells") }
    foreach ($Screen in @(0, 3)) {
        $Page0 = New-Object byte[] (960 * $CellStride)
        $Page1 = New-Object byte[] (960 * $CellStride)
        [Array]::Copy($Bytes, $ScreensOffset + $Screen * $CellsPerScreen * $CellStride,
                      $Page0, 0, $Page0.Length)
        [Array]::Copy($Bytes, $ScreensOffset + ($Screen * $CellsPerScreen + 960) * $CellStride,
                      $Page1, 0, $Page1.Length)
        if ([Convert]::ToBase64String($Page0) -cne [Convert]::ToBase64String($Page1)) {
            [void]$Issues.Add("one-page-mirror-$Screen")
        }
    }

    foreach ($PaletteOffset in @($BackgroundPalettesOffset, $ReversePalettesOffset, $SpritePalettesOffset)) {
        $PaletteCount = if ($PaletteOffset -eq $SpritePalettesOffset) { 2 } else { 5 }
        for ($Index = 0; $Index -lt $PaletteCount * 16; ++$Index) {
            if ($Bytes[$PaletteOffset + $Index] -gt 0x3F) {
                [void]$Issues.Add("palette-range")
                break
            }
        }
    }
    $ExpectedFrames = @(10, 14, 18, 22, 27)
    for ($Index = 0; $Index -lt $ExpectedFrames.Count; ++$Index) {
        if ([System.BitConverter]::ToUInt16($Bytes, $ReverseFramesOffset + $Index * 2) -ne $ExpectedFrames[$Index]) {
            [void]$Issues.Add("reverse-palette-frames")
            break
        }
    }

    for ($Group = 0; $Group -lt 2; ++$Group) {
        $Offset = $GroupsOffset + $Group * 16
        $ExpectedUsage = if ($Group -eq 0) { 5 } else { 2 }
        if ($Bytes[$Offset] -ne $Group -or $Bytes[$Offset + 1] -ne $Group -or
            [System.BitConverter]::ToUInt16($Bytes, $Offset + 2) -ne 10 -or
            [System.BitConverter]::ToUInt32($Bytes, $Offset + 4) -ne $PiecesOffset -or
            [System.BitConverter]::ToUInt32($Bytes, $Offset + 8) -ne ($SpritePalettesOffset + $Group * 16) -or
            [System.BitConverter]::ToUInt16($Bytes, $Offset + 12) -ne $ExpectedUsage -or
            [System.BitConverter]::ToUInt16($Bytes, $Offset + 14) -ne 0) {
            [void]$Issues.Add("group-$Group")
        }
    }
    for ($Piece = 0; $Piece -lt 10; ++$Piece) {
        $Offset = $PiecesOffset + $Piece * 16
        $Top = [System.BitConverter]::ToUInt32($Bytes, $Offset + 4)
        $Bottom = [System.BitConverter]::ToUInt32($Bytes, $Offset + 8)
        if ($Bytes[$Offset + 12] -gt 3 -or ($Bytes[$Offset + 13] -band 0xFC) -ne 0 -or
            $Bytes[$Offset + 14] -ne 0 -or $Bytes[$Offset + 15] -ne 0 -or
            ($Top % 16) -ne 0 -or $Bottom -ne ($Top + 16) -or
            [uint64]$Bottom + 16 -gt $ChrByteCount) {
            [void]$Issues.Add("piece-$Piece")
        }
    }

    $ExpectedRouteGroups = @(0xFF, 0, 1, 0, 0xFF)
    $ExpectedInternal = @(0, 16, 45, 80, 601)
    $ExpectedWaits = @(50, 30, 0, 75, 1)
    for ($Route = 0; $Route -lt 5; ++$Route) {
        $Offset = $RoutesOffset + $Route * 8
        $ExpectedFlags = if ($Route -eq 4) { 1 } else { 0 }
        if ($Bytes[$Offset] -ne $Route -or $Bytes[$Offset + 1] -ne $ExpectedRouteGroups[$Route] -or
            $Bytes[$Offset + 2] -ne $Route -or $Bytes[$Offset + 3] -ne $ExpectedFlags -or
            [System.BitConverter]::ToUInt16($Bytes, $Offset + 4) -ne $ExpectedInternal[$Route] -or
            [System.BitConverter]::ToUInt16($Bytes, $Offset + 6) -ne $ExpectedWaits[$Route]) {
            [void]$Issues.Add("route-$Route")
        }
    }
    $ExpectedReverseBytes = @(0x78, 0xD8, 0xF8, 0x54)
    for ($Index = 0; $Index -lt 4; ++$Index) {
        if ($Bytes[$ReverseMetadataOffset + $Index] -ne $ExpectedReverseBytes[$Index]) {
            [void]$Issues.Add("reverse-operands"); break
        }
    }
    if ([System.BitConverter]::ToUInt16($Bytes, $ReverseMetadataOffset + 4) -ne 18 -or
        [System.BitConverter]::ToUInt16($Bytes, $ReverseMetadataOffset + 6) -ne 1 -or
        [System.BitConverter]::ToUInt16($Bytes, $ReverseMetadataOffset + 8) -ne 26 -or
        [System.BitConverter]::ToUInt16($Bytes, $ReverseMetadataOffset + 10) -ne 5 -or
        [System.BitConverter]::ToUInt32($Bytes, $ReverseMetadataOffset + 12) -ne 0) {
        [void]$Issues.Add("reverse-metadata")
    }
    $ExpectedTitleMetadata = @(128, 44, 1, 7, 301, 345, 128, 1, 2, 2, 8, 1, 16, 2, 2, 0)
    for ($Index = 0; $Index -lt 16; ++$Index) {
        if ([System.BitConverter]::ToUInt16($Bytes, $TitleMetadataOffset + $Index * 2) -ne $ExpectedTitleMetadata[$Index]) {
            [void]$Issues.Add("title-metadata"); break
        }
    }
    $ExpectedBandStarts = @(0, 200, 223)
    $ExpectedBandEnds = @(200, 223, 240)
    for ($Band = 0; $Band -lt 3; ++$Band) {
        $Offset = $BandsOffset + $Band * 16
        $Low = [System.BitConverter]::ToUInt32($Bytes, $Offset + 8)
        $High = [System.BitConverter]::ToUInt32($Bytes, $Offset + 12)
        if ([System.BitConverter]::ToUInt16($Bytes, $Offset) -ne $ExpectedBandStarts[$Band] -or
            [System.BitConverter]::ToUInt16($Bytes, $Offset + 2) -ne $ExpectedBandEnds[$Band] -or
            $Bytes[$Offset + 4] -ne $Band -or $Bytes[$Offset + 5] -ne $Band -or
            [System.BitConverter]::ToUInt16($Bytes, $Offset + 6) -ne 0 -or
            ($Low % 1024) -ne 0 -or ($High % 1024) -ne 0 -or
            [uint64]$Low + 2048 -gt $ChrByteCount -or [uint64]$High + 2048 -gt $ChrByteCount) {
            [void]$Issues.Add("band-$Band")
        }
    }

    $BlankTileSignatures = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    for ($SlotIndex = 0; $SlotIndex -lt 44; ++$SlotIndex) {
        $Offset = $TitleSlotsOffset + $SlotIndex * 32
        $VirtualSlot = ($SlotIndex + 16) -band 31
        $ExpectedPage = if ($VirtualSlot -ge 16) { 1 } else { 0 }
        $ExpectedColumn = ($VirtualSlot -band 15) * 2
        if ($Bytes[$Offset] -ne $ExpectedPage -or $Bytes[$Offset + 1] -ne $ExpectedColumn -or
            $Bytes[$Offset + 2] -ne 16 -or $Bytes[$Offset + 3] -ne 0) {
            [void]$Issues.Add("title-order-$SlotIndex")
        }
        for ($Pad = 28; $Pad -lt 32; ++$Pad) {
            if ($Bytes[$Offset + $Pad] -ne 0) { [void]$Issues.Add("title-reserved-$SlotIndex"); break }
        }
        $SignatureParts = [System.Collections.Generic.List[string]]::new()
        for ($Tile = 0; $Tile -lt 4; ++$Tile) {
            $CellOffset = $Offset + 4 + $Tile * 6
            $ChrOffset = [System.BitConverter]::ToUInt32($Bytes, $CellOffset + 2)
            if ($Bytes[$CellOffset + 1] -gt 3 -or ($ChrOffset % 16) -ne 0 -or
                [uint64]$ChrOffset + 16 -gt $ChrByteCount) {
                [void]$Issues.Add("title-cell-$SlotIndex-$Tile")
            }
            [void]$SignatureParts.Add(("{0:X2}:{1}" -f $Bytes[$CellOffset], $ChrOffset))
        }
        if ($SlotIndex -ge 26) { [void]$BlankTileSignatures.Add(($SignatureParts -join ",")) }
        if ($SlotIndex -ge 32) {
            $PriorOffset = $TitleSlotsOffset + ($SlotIndex - 32) * 32
            if ($Bytes[$Offset] -ne $Bytes[$PriorOffset] -or
                $Bytes[$Offset + 1] -ne $Bytes[$PriorOffset + 1] -or
                $Bytes[$Offset + 2] -ne $Bytes[$PriorOffset + 2]) {
                [void]$Issues.Add("title-overwrite-$SlotIndex")
            }
        }
    }
    if ($BlankTileSignatures.Count -ne 1) { [void]$Issues.Add("blank-glyph-consistency") }

    return [pscustomobject]@{
        passed = $Issues.Count -eq 0
        issues = $Issues.ToArray()
    }
}

function Test-StartMenuPayloadContract {
    param(
        [byte[]]$Bytes,
        [uint64]$ChrByteCount
    )

    $Issues = [System.Collections.Generic.List[string]]::new()
    $CellsOffset = 160
    $PalettesOffset = 11680
    $EmblemOffset = 11968
    $CursorOffset = 12752
    $OverlayDescsOffset = 12768
    $OverlayCellsOffset = 12816
    $DigitsOffset = 14052
    if ($Bytes.Length -ne 14112 -or
        [System.Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -ne "TSGM" -or
        [System.BitConverter]::ToUInt16($Bytes, 4) -ne 1 -or
        [System.BitConverter]::ToUInt16($Bytes, 6) -ne 160 -or
        [System.BitConverter]::ToUInt16($Bytes, 8) -ne 32 -or
        [System.BitConverter]::ToUInt16($Bytes, 10) -ne 30 -or
        [System.BitConverter]::ToUInt16($Bytes, 12) -ne 2 -or
        [System.BitConverter]::ToUInt16($Bytes, 14) -ne 6 -or
        [System.BitConverter]::ToUInt32($Bytes, 16) -ne 1920 -or
        [System.BitConverter]::ToUInt32($Bytes, 20) -ne $CellsOffset -or
        [System.BitConverter]::ToUInt32($Bytes, 24) -ne $PalettesOffset -or
        [System.BitConverter]::ToUInt16($Bytes, 28) -ne 9 -or
        [System.BitConverter]::ToUInt16($Bytes, 30) -ne 32 -or
        [System.BitConverter]::ToUInt32($Bytes, 32) -ne $EmblemOffset -or
        [System.BitConverter]::ToUInt16($Bytes, 36) -ne 49 -or
        [System.BitConverter]::ToUInt16($Bytes, 38) -ne 16 -or
        [System.BitConverter]::ToUInt32($Bytes, 40) -ne $CursorOffset -or
        [System.BitConverter]::ToUInt32($Bytes, 44) -ne $Bytes.Length) {
        [void]$Issues.Add("header")
        return [pscustomobject]@{ passed = $false; issues = $Issues.ToArray() }
    }
    $ExpectedMetadata = @(32, 7, 6, 8, 32, 8, 5, 31, 88, 16, 184, 60)
    for ($Index = 0; $Index -lt $ExpectedMetadata.Count; ++$Index) {
        if ([System.BitConverter]::ToUInt16($Bytes, 48 + $Index * 2) -ne $ExpectedMetadata[$Index]) {
            [void]$Issues.Add("metadata"); break
        }
    }
    if ([System.BitConverter]::ToUInt32($Bytes, 72) -ne $OverlayDescsOffset -or
        [System.BitConverter]::ToUInt16($Bytes, 76) -ne 3 -or
        [System.BitConverter]::ToUInt16($Bytes, 78) -ne 16 -or
        [System.BitConverter]::ToUInt32($Bytes, 80) -ne $OverlayCellsOffset -or
        [System.BitConverter]::ToUInt32($Bytes, 84) -ne 206 -or
        [System.BitConverter]::ToUInt32($Bytes, 88) -ne $DigitsOffset -or
        [System.BitConverter]::ToUInt16($Bytes, 92) -ne 10 -or
        [System.BitConverter]::ToUInt16($Bytes, 94) -ne 6) {
        [void]$Issues.Add("aux-layout")
    }
    $ExpectedStageFrames = @(0, 2, 4, 6, 8, 20, 24, 28, 32)
    for ($Index = 0; $Index -lt 9; ++$Index) {
        if ([System.BitConverter]::ToUInt16($Bytes, 96 + $Index * 2) -ne $ExpectedStageFrames[$Index]) {
            [void]$Issues.Add("stage-frames"); break
        }
    }
    for ($Index = 0; $Index -lt 7; ++$Index) {
        if ($Bytes[114 + $Index] -ne ($Index + 1)) { [void]$Issues.Add("routes"); break }
    }
    $ExpectedPeriodValues = @(2, 3, 4, 8, 12)
    for ($Index = 0; $Index -lt 5; ++$Index) {
        if ($Bytes[121 + $Index] -ne $ExpectedPeriodValues[$Index]) {
            [void]$Issues.Add("period-values"); break
        }
    }
    for ($Index = 126; $Index -lt 160; ++$Index) {
        if ($Bytes[$Index] -ne 0) { [void]$Issues.Add("reserved"); break }
    }
    foreach ($Range in @(
        [pscustomobject]@{ start = $CellsOffset; count = 1920 },
        [pscustomobject]@{ start = $OverlayCellsOffset; count = 206 },
        [pscustomobject]@{ start = $DigitsOffset; count = 10 }
    )) {
        for ($Cell = 0; $Cell -lt $Range.count; ++$Cell) {
            $Offset = $Range.start + $Cell * 6
            $ChrOffset = [System.BitConverter]::ToUInt32($Bytes, $Offset + 2)
            if ($Bytes[$Offset + 1] -gt 3 -or ($ChrOffset % 16) -ne 0 -or
                [uint64]$ChrOffset + 16 -gt $ChrByteCount) {
                [void]$Issues.Add("cell-range"); break
            }
        }
    }
    for ($Index = 0; $Index -lt 9 * 32; ++$Index) {
        if ($Bytes[$PalettesOffset + $Index] -gt 0x3F) { [void]$Issues.Add("palette-range"); break }
    }
    for ($Index = 0; $Index -lt 32; ++$Index) {
        if ($Bytes[$PalettesOffset + 4 * 32 + $Index] -ne 0x0F) {
            [void]$Issues.Add("black-stage"); break
        }
    }
    for ($Piece = 0; $Piece -lt 49; ++$Piece) {
        $Offset = $EmblemOffset + $Piece * 16
        $Top = [System.BitConverter]::ToUInt32($Bytes, $Offset + 4)
        $Bottom = [System.BitConverter]::ToUInt32($Bytes, $Offset + 8)
        if (($Top % 16) -ne 0 -or $Bottom -ne $Top + 16 -or
            [uint64]$Bottom + 16 -gt $ChrByteCount -or $Bytes[$Offset + 12] -gt 3 -or
            ($Bytes[$Offset + 13] -band 0xF8) -ne 0 -or $Bytes[$Offset + 14] -ne 0 -or
            $Bytes[$Offset + 15] -ne 0) {
            [void]$Issues.Add("emblem-piece"); break
        }
    }
    if ([System.BitConverter]::ToInt16($Bytes, $CursorOffset) -ne 31 -or
        [System.BitConverter]::ToInt16($Bytes, $CursorOffset + 2) -ne 88 -or
        [System.BitConverter]::ToUInt32($Bytes, $CursorOffset + 8) -ne
            ([System.BitConverter]::ToUInt32($Bytes, $CursorOffset + 4) + 16) -or
        $Bytes[$CursorOffset + 12] -ne 0 -or $Bytes[$CursorOffset + 13] -ne 0 -or
        $Bytes[$CursorOffset + 14] -ne 0 -or $Bytes[$CursorOffset + 15] -ne 0) {
        [void]$Issues.Add("cursor")
    }
    $ExpectedOverlays = @(
        @(0, 42, 7, 6, 5, 23, 0),
        @(42, 80, 10, 8, 5, 19, 1),
        @(122, 84, 14, 6, 5, 21, 2)
    )
    for ($Index = 0; $Index -lt 3; ++$Index) {
        $Offset = $OverlayDescsOffset + $Index * 16
        $Actual = @(
            [System.BitConverter]::ToUInt32($Bytes, $Offset),
            [System.BitConverter]::ToUInt16($Bytes, $Offset + 4),
            [System.BitConverter]::ToUInt16($Bytes, $Offset + 6),
            [System.BitConverter]::ToUInt16($Bytes, $Offset + 8),
            [System.BitConverter]::ToUInt16($Bytes, $Offset + 10),
            [System.BitConverter]::ToUInt16($Bytes, $Offset + 12),
            $Bytes[$Offset + 14]
        )
        if (($Actual -join ",") -ne ($ExpectedOverlays[$Index] -join ",") -or $Bytes[$Offset + 15] -ne 0) {
            [void]$Issues.Add("overlay-desc"); break
        }
    }
    return [pscustomobject]@{ passed = $Issues.Count -eq 0; issues = $Issues.ToArray() }
}

function Add-TestResult {
    param([pscustomobject]$Result)

    $script:Results.Add($Result) | Out-Null
    if (($Result.PSObject.Properties.Name -contains "passed") -and !$Result.passed) {
        ++$script:Failures
    }
}

function Write-AssetPackReport {
    $Report = [pscustomobject]@{
        schema_version = 1
        generated_by = "tools/Run-AssetPackTests.ps1"
        generated_at_utc = [DateTime]::UtcNow.ToString("o")
        data_policy = "Sanitized asset-pack smoke report only; raw stdout/stderr, local paths, ROM bytes, extracted assets, ASM, CHR bytes, and screenshots outside ignored build output are not persisted."
        passed = $Failures -eq 0
        reference_rom_used = "<local>"
        build_requested = [bool]$Build
        private_paths_included = $false
        raw_output_persisted = $false
        asset_pack = [pscustomobject]@{
            output = $AssetPackRelative
            canonical_fallback = $CanonicalAssetPackRelative
            prg_banks_expected_from_rom = $ExpectedPrgBanks
            chr_banks_expected_from_rom = $ExpectedChrBanks
            prg_banks_reported = $ReportedPrgBanks
            chr_banks_reported = $ReportedChrBanks
            entries_reported = $ReportedEntries
            expected_entry_count = if ($null -ne $ExpectedEntries) { $ExpectedEntries.Count } else { $null }
            expected_raw_entry_count = if ($null -ne $ExpectedRawEntries) { $ExpectedRawEntries.Count } else { $null }
            expected_logical_entry_count = if ($null -ne $ExpectedLogicalEntries) { $ExpectedLogicalEntries.Count } else { $null }
            directory_entry_count = if ($Directory) { $Directory.entry_count } else { $null }
            logical_capture_sources_present = $LogicalCaptureSources
            canonical_fallback_cleared = $CanonicalFallbackCleared
            rom_only_contract = $true
        }
        tests = $Results
    }

    $ReportDir = Split-Path -Parent $ReportPath
    if ($ReportDir) {
        New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null
    }
    $Report | ConvertTo-Json -Depth 8 | Set-Content -Path $ReportPath -Encoding UTF8
}

if (!(Test-PathUnder $AssetPackPath $BuildDir)) {
    throw "Asset pack output must stay under build\."
}
if (!(Test-PathUnder $ReportPath $BuildDir)) {
    throw "Asset pack test report must stay under build\."
}
if (!$AssetPackPath.EndsWith(".assetpack", [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Asset pack output must use the .assetpack extension."
}

$ExePath = Join-Path $ProjectRoot "build\tecmo_port.exe"

$ReferenceRom = Find-ReferenceRom
if (!$ReferenceRom) {
    throw "Could not find a local iNES ROM. Pass -RomPath or set TECMO_ROM_PATH."
}
$ReferenceHeader = Get-InesHeaderInfo $ReferenceRom
if ($ReferenceHeader.prg_banks -le 0) {
    throw "Reference ROM reports zero PRG banks."
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
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $AssetPackPath) | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $ReportPath) | Out-Null

$Results = [System.Collections.Generic.List[object]]::new()
$Failures = 0
$AssetPackRelative = ConvertTo-RepoRelativePath $AssetPackPath
$CanonicalAssetPackRelative = ConvertTo-RepoRelativePath $CanonicalAssetPackPath
$ReportedPrgBanks = $null
$ReportedChrBanks = $null
$ReportedEntries = $null
$ExpectedPrgBanks = [int]$ReferenceHeader.prg_banks
$ExpectedChrBanks = [int]$ReferenceHeader.chr_banks
$Directory = $null
$ExpectedEntries = $null
$ExpectedRawEntries = $null
$ExpectedLogicalEntries = $null
$LogicalCaptureSources = @()
$DirectoryCountPassed = $false
$BankCompletenessPassed = $false
$LogicalEntriesPassed = $false
$ChrAllValid = $false
$CanonicalFallbackInitiallyPresent = Test-Path -LiteralPath $CanonicalAssetPackPath

try {
    $ClearTargets = [System.Collections.Generic.List[string]]::new()
    [void]$ClearTargets.Add($AssetPackPath)

    $ClearedTargets = [System.Collections.Generic.List[string]]::new()
    $ClearErrors = 0
    foreach ($Target in $ClearTargets) {
        try {
            if (Test-Path -LiteralPath $Target) {
                Remove-Item -LiteralPath $Target -Force
                [void]$ClearedTargets.Add((ConvertTo-RepoRelativePath $Target))
            }
        } catch {
            ++$ClearErrors
        }
        if (Test-Path -LiteralPath $Target) {
            ++$ClearErrors
        }
    }
    $TargetOutputCleared = !(Test-Path -LiteralPath $AssetPackPath)
    $CanonicalFallbackPreserved = $AssetPackPath.Equals(
        $CanonicalAssetPackPath,
        [System.StringComparison]::OrdinalIgnoreCase) -or
        ((Test-Path -LiteralPath $CanonicalAssetPackPath) -eq $CanonicalFallbackInitiallyPresent)
    Add-TestResult ([pscustomobject]@{
        id = "target-assetpack-cleared"
        passed = $ClearErrors -eq 0 -and $TargetOutputCleared -and $CanonicalFallbackPreserved
        target_output = $AssetPackRelative
        canonical_fallback = $CanonicalAssetPackRelative
        removed_outputs = $ClearedTargets.ToArray()
        canonical_fallback_preserved = $CanonicalFallbackPreserved
        raw_output_persisted = $false
    })

    Write-Host "Building asset pack -> $AssetPackRelative"
    Push-Location $ProjectRoot
    try {
        $BuildOutput = & $ExePath --build-assetpack $ReferenceRom $AssetPackPath 2>&1
        $BuildExitCode = $LASTEXITCODE
    } finally {
        Pop-Location
    }
    $BuildOutputText = (@($BuildOutput) | ForEach-Object { [string]$_ }) -join "`n"
    if ($BuildOutputText -match "Wrote\s+([0-9]+)\s+PRG banks,\s+([0-9]+)\s+CHR banks,\s+([0-9]+)\s+entries to .+") {
        $ReportedPrgBanks = [int]$Matches[1]
        $ReportedChrBanks = [int]$Matches[2]
        $ReportedEntries = [int]$Matches[3]
    }
    $ReportedBanksMatchRom = $null -ne $ReportedPrgBanks -and
        $null -ne $ReportedChrBanks -and
        [int]$ReportedPrgBanks -eq $ExpectedPrgBanks -and
        [int]$ReportedChrBanks -eq $ExpectedChrBanks
    $BuildPassed = $BuildExitCode -eq 0 -and $ReportedBanksMatchRom -and $null -ne $ReportedEntries
    Add-TestResult ([pscustomobject]@{
        id = "build-assetpack"
        passed = $BuildPassed
        exit_code = $BuildExitCode
        expected_prg_banks_from_rom = $ExpectedPrgBanks
        expected_chr_banks_from_rom = $ExpectedChrBanks
        reported_banks_match_rom = $ReportedBanksMatchRom
        output_reported_prg_banks = $null -ne $ReportedPrgBanks
        output_reported_chr_banks = $null -ne $ReportedChrBanks
        output_reported_entry_count = $null -ne $ReportedEntries
        raw_output_persisted = $false
    })

    if ($BuildPassed) {
        Write-Host ("Asset pack build reported {0} PRG banks, {1} CHR banks, {2} entries." -f $ReportedPrgBanks, $ReportedChrBanks, $ReportedEntries)
    } else {
        Write-Host "Asset pack build failed or did not report PRG/CHR entry counts."
    }

    $MalformedSourceRom = Join-Path (Split-Path -Parent $AssetPackPath) "malformed-finale-source.nes"
    $MalformedSourcePack = Join-Path (Split-Path -Parent $AssetPackPath) "malformed-finale-source.assetpack"
    $SourceMutationCases = [System.Collections.Generic.List[object]]::new()
    $PrgStart = 16 + [int]$ReferenceHeader.trainer_bytes
    $SourceMutationSpecs = @(
        [pscustomobject]@{ id = "selector-two-immediate"; bank = 4; cpu = 0x852F },
        [pscustomobject]@{ id = "selector-two-indexed-operand"; bank = 4; cpu = 0x863E },
        [pscustomobject]@{ id = "sprite-chr-selector"; bank = 4; cpu = 0x856A },
        [pscustomobject]@{ id = "geometry-pointer"; bank = 0; cpu = 0xA911 },
        [pscustomobject]@{ id = "geometry-count"; bank = 0; cpu = 0xA9D2 },
        [pscustomobject]@{ id = "title-uppercase-map"; bank = 6; cpu = 0xA2B4 },
        [pscustomobject]@{ id = "one-page-screen-source"; bank = 1; cpu = 0xB7D5 }
    )
    foreach ($Spec in $SourceMutationSpecs) {
        $Rejected = $false
        try {
            [System.IO.File]::Copy($ReferenceRom, $MalformedSourceRom, $true)
            $PatchOffset = [uint64]($PrgStart + [int]$Spec.bank * 0x4000 + ([int]$Spec.cpu - 0x8000))
            $PatchFile = [System.IO.File]::Open($MalformedSourceRom,
                                               [System.IO.FileMode]::Open,
                                               [System.IO.FileAccess]::ReadWrite,
                                               [System.IO.FileShare]::None)
            try {
                [void]$PatchFile.Seek([int64]$PatchOffset, [System.IO.SeekOrigin]::Begin)
                $Original = $PatchFile.ReadByte()
                if ($Original -lt 0) { throw "Could not read finale mutation source byte." }
                [void]$PatchFile.Seek(-1, [System.IO.SeekOrigin]::Current)
                $PatchFile.WriteByte([byte]($Original -bxor 1))
            } finally {
                $PatchFile.Dispose()
            }
            $null = & $ExePath --build-assetpack $MalformedSourceRom $MalformedSourcePack 2>&1
            $Rejected = $LASTEXITCODE -ne 0
        } finally {
            if (Test-Path -LiteralPath $MalformedSourcePack) { Remove-Item -LiteralPath $MalformedSourcePack -Force }
            if (Test-Path -LiteralPath $MalformedSourceRom) { Remove-Item -LiteralPath $MalformedSourceRom -Force }
        }
        [void]$SourceMutationCases.Add([pscustomobject]@{ id = $Spec.id; rejected = $Rejected })
    }
    Add-TestResult ([pscustomobject]@{
        id = "assetpack-finale-source-contract"
        passed = @($SourceMutationCases | Where-Object { !$_.rejected }).Count -eq 0
        cases = $SourceMutationCases.ToArray()
        malformed_rom_persisted = $false
        malformed_assetpack_persisted = $false
        raw_output_persisted = $false
    })

    $StartMenuSourceCases = [System.Collections.Generic.List[object]]::new()
    $StartMenuSourceSpecs = @(
        [pscustomobject]@{ id = "descriptor"; bank = $ExpectedPrgBanks - 1; cpu = 0xDCA1; fixed = $true },
        [pscustomobject]@{ id = "compressed-screen"; bank = 0; cpu = 0x8898; fixed = $false },
        [pscustomobject]@{ id = "root-menu-record"; bank = 3; cpu = 0x9B55; fixed = $false },
        [pscustomobject]@{ id = "fade-out"; bank = $ExpectedPrgBanks - 1; cpu = 0xDB25; fixed = $true },
        [pscustomobject]@{ id = "input-parameters"; bank = 3; cpu = 0x9F13; fixed = $false }
    )
    foreach ($Spec in $StartMenuSourceSpecs) {
        $Rejected = $false
        try {
            [System.IO.File]::Copy($ReferenceRom, $MalformedSourceRom, $true)
            $CpuBase = if ($Spec.fixed) { 0xC000 } else { 0x8000 }
            $PatchOffset = [uint64]($PrgStart + [int]$Spec.bank * 0x4000 + ([int]$Spec.cpu - $CpuBase))
            $PatchFile = [System.IO.File]::Open($MalformedSourceRom,
                                               [System.IO.FileMode]::Open,
                                               [System.IO.FileAccess]::ReadWrite,
                                               [System.IO.FileShare]::None)
            try {
                [void]$PatchFile.Seek([int64]$PatchOffset, [System.IO.SeekOrigin]::Begin)
                $Original = $PatchFile.ReadByte()
                if ($Original -lt 0) { throw "Could not read start-menu mutation source byte." }
                [void]$PatchFile.Seek(-1, [System.IO.SeekOrigin]::Current)
                $PatchFile.WriteByte([byte]($Original -bxor 1))
            } finally {
                $PatchFile.Dispose()
            }
            $null = & $ExePath --build-assetpack $MalformedSourceRom $MalformedSourcePack 2>&1
            $Rejected = $LASTEXITCODE -ne 0
        } finally {
            if (Test-Path -LiteralPath $MalformedSourcePack) { Remove-Item -LiteralPath $MalformedSourcePack -Force }
            if (Test-Path -LiteralPath $MalformedSourceRom) { Remove-Item -LiteralPath $MalformedSourceRom -Force }
        }
        [void]$StartMenuSourceCases.Add([pscustomobject]@{ id = $Spec.id; rejected = $Rejected })
    }
    Add-TestResult ([pscustomobject]@{
        id = "assetpack-start-game-menu-source-contract"
        passed = @($StartMenuSourceCases | Where-Object { !$_.rejected }).Count -eq 0
        cases = $StartMenuSourceCases.ToArray()
        malformed_rom_persisted = $false
        malformed_assetpack_persisted = $false
        raw_output_persisted = $false
    })

    $PackCreated = Test-Path -LiteralPath $AssetPackPath
    Add-TestResult ([pscustomobject]@{
        id = "assetpack-file-created"
        passed = $PackCreated
        output = $AssetPackRelative
        bytes = if ($PackCreated) { (Get-Item -LiteralPath $AssetPackPath).Length } else { $null }
    })

    if ($PackCreated) {
        try {
            $Directory = Read-AssetPackDirectory $AssetPackPath
            Add-TestResult ([pscustomobject]@{
                id = "assetpack-directory-readable"
                passed = $true
                directory_entry_count = $Directory.entry_count
                raw_asset_bytes_persisted = $false
            })
        } catch {
            Add-TestResult ([pscustomobject]@{
                id = "assetpack-directory-readable"
                passed = $false
                error = "asset pack directory inspection failed"
                error_type = Get-SafeExceptionName $_
                raw_asset_bytes_persisted = $false
            })
        }
    } else {
        Add-TestResult ([pscustomobject]@{
            id = "assetpack-directory-readable"
            passed = $false
            error = "asset pack file was not created"
            raw_asset_bytes_persisted = $false
        })
    }

    if ($Directory) {
        $DirectoryCountPassed = $null -ne $ReportedEntries -and
            [uint32]$Directory.entry_count -eq [uint32]$ReportedEntries -and
            $Directory.ids.Count -eq [int]$Directory.entry_count
        Add-TestResult ([pscustomobject]@{
            id = "assetpack-directory-count"
            passed = $DirectoryCountPassed
            cli_reported_entry_count = $ReportedEntries
            parsed_directory_entry_count = $Directory.entry_count
            parsed_id_count = $Directory.ids.Count
            raw_asset_bytes_persisted = $false
        })

        if ($ExpectedPrgBanks -gt 0 -and $ExpectedChrBanks -ge 0) {
            $ExpectedRawEntries = @(Get-ExpectedAssetPackEntries -PrgBanks $ExpectedPrgBanks -ChrBanks $ExpectedChrBanks)
            $LogicalExpectation = Get-ExpectedLogicalAssetPackEntries
            $ExpectedLogicalEntries = @($LogicalExpectation.entries)
            $LogicalCaptureSources = @($LogicalExpectation.capture_sources_present)
            $ExpectedEntries = @($ExpectedRawEntries + $ExpectedLogicalEntries)
            $ExpectedRawEntrySet = New-StringSet -Values ([string[]]$ExpectedRawEntries)
            $KnownLogicalEntries = @(Get-KnownLogicalAssetPackEntries)
            $KnownLogicalEntrySet = New-StringSet -Values ([string[]]$KnownLogicalEntries)
            $ExpectedLogicalEntrySet = New-StringSet -Values ([string[]]$ExpectedLogicalEntries)
            $ExpectedLogicalEntryTypes = Get-KnownLogicalAssetPackEntryTypes

            $MissingEntries = @($ExpectedRawEntries | Where-Object { !$Directory.ids.Contains($_) })
            $DuplicateEntries = @(Get-DuplicateValues -Values ([string[]]$Directory.ids.ToArray()))
            $RawEntriesPresent = @($Directory.ids | Where-Object { $ExpectedRawEntrySet.Contains($_) })
            $UnexpectedRawEntries = @($Directory.ids | Where-Object {
                (($_.StartsWith("prg/", [System.StringComparison]::Ordinal) -or
                  $_.StartsWith("chr/", [System.StringComparison]::Ordinal) -or
                  $_.StartsWith("system/", [System.StringComparison]::Ordinal)) -and
                 !$ExpectedRawEntrySet.Contains($_))
            })
            $SizeMismatches = @(Get-AssetPackSizeMismatches -Directory $Directory -PrgBanks $ExpectedPrgBanks -ChrBanks $ExpectedChrBanks)
            $RawCountMatchesExpected = $RawEntriesPresent.Count -eq $ExpectedRawEntries.Count
            $ChrAllMismatches = @($SizeMismatches | Where-Object { $_.id -eq "chr/all" })
            $ChrAllValid = $Directory.ids.Contains("chr/all") -and $ChrAllMismatches.Count -eq 0
            $BankCompletenessPassed = $MissingEntries.Count -eq 0 -and
                $DuplicateEntries.Count -eq 0 -and
                $UnexpectedRawEntries.Count -eq 0 -and
                $RawCountMatchesExpected -and
                $SizeMismatches.Count -eq 0
            Add-TestResult ([pscustomobject]@{
                id = "assetpack-bank-completeness"
                passed = $BankCompletenessPassed
                expected_entry_count = $ExpectedEntries.Count
                expected_raw_entry_count = $ExpectedRawEntries.Count
                expected_prg_banks_from_rom = $ExpectedPrgBanks
                expected_chr_banks_from_rom = $ExpectedChrBanks
                parsed_raw_entry_count = $RawEntriesPresent.Count
                cli_reported_entry_count = $ReportedEntries
                parsed_directory_entry_count = $Directory.entry_count
                raw_count_matches_expected = $RawCountMatchesExpected
                missing_entries = $MissingEntries
                duplicate_entries = $DuplicateEntries
                unexpected_raw_entries = $UnexpectedRawEntries
                size_mismatches = $SizeMismatches
                chr_all_valid = $ChrAllValid
                logical_entries_allowed = $ExpectedLogicalEntries.Count -gt 0
                raw_asset_bytes_persisted = $false
            })

            $MissingLogicalEntries = @($ExpectedLogicalEntries | Where-Object { !$Directory.ids.Contains($_) })
            $LogicalEntriesPresent = @($Directory.ids | Where-Object { $KnownLogicalEntrySet.Contains($_) })
            $UnknownEntries = @($Directory.ids | Where-Object {
                !$ExpectedRawEntrySet.Contains($_) -and !$KnownLogicalEntrySet.Contains($_)
            })
            $UnexpectedLogicalEntries = @($Directory.ids | Where-Object {
                $KnownLogicalEntrySet.Contains($_) -and !$ExpectedLogicalEntrySet.Contains($_)
            })
            $EmptyLogicalEntries = @($Directory.entries | Where-Object {
                $KnownLogicalEntrySet.Contains($_.id) -and [uint64]$_.byte_count -eq 0
            } | ForEach-Object { $_.id })
            $LogicalTypeMismatches = @($Directory.entries | Where-Object {
                $KnownLogicalEntrySet.Contains($_.id) -and
                    $ExpectedLogicalEntryTypes.ContainsKey($_.id) -and
                    [uint32]$_.type -ne [uint32]$ExpectedLogicalEntryTypes[$_.id]
            } | ForEach-Object {
                [pscustomobject]@{
                    id = $_.id
                    expected_type = $ExpectedLogicalEntryTypes[$_.id]
                    actual_type = $_.type
                }
            })
            $LogicalEntriesPassed = $MissingLogicalEntries.Count -eq 0 -and
                $UnexpectedLogicalEntries.Count -eq 0 -and
                $UnknownEntries.Count -eq 0 -and
                $EmptyLogicalEntries.Count -eq 0 -and
                $LogicalTypeMismatches.Count -eq 0
            Add-TestResult ([pscustomobject]@{
                id = "assetpack-logical-entries"
                passed = $LogicalEntriesPassed
                expected_logical_entries = $ExpectedLogicalEntries
                logical_entries_present = $LogicalEntriesPresent
                missing_logical_entries = $MissingLogicalEntries
                unexpected_logical_entries = $UnexpectedLogicalEntries
                unknown_entries = $UnknownEntries
                empty_logical_entries = $EmptyLogicalEntries
                logical_type_mismatches = $LogicalTypeMismatches
                capture_sources_present = $LogicalCaptureSources
                raw_asset_bytes_persisted = $false
            })

            try {
                $LayerBytes = Read-AssetPackEntryBytes -Path $AssetPackPath -Directory $Directory -EntryId "arena/intro/background-layer"
                $LayerMagic = if ($LayerBytes.Length -ge 4) {
                    [System.Text.Encoding]::ASCII.GetString($LayerBytes, 0, 4)
                } else {
                    ""
                }
                $LayerVersion = if ($LayerBytes.Length -ge 6) { [System.BitConverter]::ToUInt16($LayerBytes, 4) } else { 0 }
                $LayerHeaderSize = if ($LayerBytes.Length -ge 8) { [System.BitConverter]::ToUInt16($LayerBytes, 6) } else { 0 }
                $LayerWidth = if ($LayerBytes.Length -ge 10) { [System.BitConverter]::ToUInt16($LayerBytes, 8) } else { 0 }
                $LayerHeight = if ($LayerBytes.Length -ge 12) { [System.BitConverter]::ToUInt16($LayerBytes, 10) } else { 0 }
                $LayerCellStride = if ($LayerBytes.Length -ge 18) { [System.BitConverter]::ToUInt16($LayerBytes, 16) } else { 0 }
                $LayerCellCount = if ($LayerBytes.Length -ge 24) { [System.BitConverter]::ToUInt32($LayerBytes, 20) } else { 0 }
                $LayerCellsOffset = if ($LayerBytes.Length -ge 28) { [System.BitConverter]::ToUInt32($LayerBytes, 24) } else { 0 }
                $ExpectedLayerSize = 48 + 1632 * 6
                $KnownReferenceRomSha256 = "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4"
                $KnownReferenceLayerSha256 = "5DE04C010CC08F47A510A25D3A48012C58C1DE58529EED4D5BF3F553C1107EFC"
                $RomSha256 = (Get-FileHash -LiteralPath $ReferenceRom -Algorithm SHA256).Hash
                $LayerHasher = [System.Security.Cryptography.SHA256]::Create()
                try {
                    $LayerSha256 = [System.BitConverter]::ToString($LayerHasher.ComputeHash($LayerBytes)).Replace("-", "")
                } finally {
                    $LayerHasher.Dispose()
                }
                $KnownReferenceContentMatches = $RomSha256 -ne $KnownReferenceRomSha256 -or
                    $LayerSha256 -eq $KnownReferenceLayerSha256
                $InvalidPaletteIndexes = 0
                $InvalidChrOffsets = 0
                $DistinctChrOffsets = [System.Collections.Generic.HashSet[uint32]]::new()
                $ChrByteCount = [uint64]$ExpectedChrBanks * 8192

                if ($LayerBytes.Length -eq $ExpectedLayerSize -and
                    $LayerCellCount -eq 1632 -and $LayerCellStride -eq 6 -and
                    $LayerCellsOffset -eq 48) {
                    for ($Index = 0; $Index -lt [int]$LayerCellCount; ++$Index) {
                        $CellOffset = [int]$LayerCellsOffset + $Index * [int]$LayerCellStride
                        $PaletteIndex = $LayerBytes[$CellOffset + 1]
                        $ChrOffset = [System.BitConverter]::ToUInt32($LayerBytes, $CellOffset + 2)
                        if ($PaletteIndex -gt 3) {
                            ++$InvalidPaletteIndexes
                        }
                        if (($ChrOffset -band 0x0F) -ne 0 -or ([uint64]$ChrOffset + 16) -gt $ChrByteCount) {
                            ++$InvalidChrOffsets
                        }
                        [void]$DistinctChrOffsets.Add($ChrOffset)
                    }
                }

                $LayerContractPassed = $LayerMagic -eq "TATL" -and
                    $LayerVersion -eq 1 -and $LayerHeaderSize -eq 48 -and
                    $LayerWidth -eq 32 -and $LayerHeight -eq 51 -and
                    $LayerCellStride -eq 6 -and $LayerCellCount -eq 1632 -and
                    $LayerCellsOffset -eq 48 -and $LayerBytes.Length -eq $ExpectedLayerSize -and
                    $InvalidPaletteIndexes -eq 0 -and $InvalidChrOffsets -eq 0 -and
                    $DistinctChrOffsets.Count -gt 16 -and $KnownReferenceContentMatches
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-arena-background-layer"
                    passed = $LayerContractPassed
                    format = $LayerMagic
                    version = $LayerVersion
                    width = $LayerWidth
                    height = $LayerHeight
                    cell_count = $LayerCellCount
                    cell_stride = $LayerCellStride
                    byte_count = $LayerBytes.Length
                    invalid_palette_indexes = $InvalidPaletteIndexes
                    invalid_chr_offsets = $InvalidChrOffsets
                    distinct_chr_offset_count = $DistinctChrOffsets.Count
                    known_reference_revision = $RomSha256 -eq $KnownReferenceRomSha256
                    known_reference_content_match = $KnownReferenceContentMatches
                    raw_asset_bytes_persisted = $false
                })
            } catch {
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-arena-background-layer"
                    passed = $false
                    error = "arena background layer inspection failed"
                    error_type = Get-SafeExceptionName $_
                    raw_asset_bytes_persisted = $false
                })
            }

            try {
                $SpriteBytes = Read-AssetPackEntryBytes -Path $AssetPackPath -Directory $Directory -EntryId "arena/intro/sprite-groups"
                $KnownReferenceSpriteSha256 = "8479FF1F5AB502D68169F74D88DBCB4B582D5E1F6009D18D509E020499A79679"
                $SpriteHasher = [System.Security.Cryptography.SHA256]::Create()
                try {
                    $SpriteSha256 = [System.BitConverter]::ToString($SpriteHasher.ComputeHash($SpriteBytes)).Replace("-", "")
                } finally {
                    $SpriteHasher.Dispose()
                }
                $KnownReferenceSpriteContentMatches = $RomSha256 -ne $KnownReferenceRomSha256 -or
                    $SpriteSha256 -eq $KnownReferenceSpriteSha256
                $SpriteMagic = if ($SpriteBytes.Length -ge 4) { [System.Text.Encoding]::ASCII.GetString($SpriteBytes, 0, 4) } else { "" }
                $SpriteVersion = if ($SpriteBytes.Length -ge 6) { [System.BitConverter]::ToUInt16($SpriteBytes, 4) } else { 0 }
                $SpriteHeaderSize = if ($SpriteBytes.Length -ge 8) { [System.BitConverter]::ToUInt16($SpriteBytes, 6) } else { 0 }
                $SpriteGroupCount = if ($SpriteBytes.Length -ge 10) { [System.BitConverter]::ToUInt16($SpriteBytes, 8) } else { 0 }
                $SpriteGroupStride = if ($SpriteBytes.Length -ge 12) { [System.BitConverter]::ToUInt16($SpriteBytes, 10) } else { 0 }
                $SpritePieceCount = if ($SpriteBytes.Length -ge 16) { [System.BitConverter]::ToUInt32($SpriteBytes, 12) } else { 0 }
                $SpritePieceStride = if ($SpriteBytes.Length -ge 18) { [System.BitConverter]::ToUInt16($SpriteBytes, 16) } else { 0 }
                $SpriteFlags = if ($SpriteBytes.Length -ge 20) { [System.BitConverter]::ToUInt16($SpriteBytes, 18) } else { 0 }
                $SpritePaletteOffset = if ($SpriteBytes.Length -ge 24) { [System.BitConverter]::ToUInt32($SpriteBytes, 20) } else { 0 }
                $SpriteGroupsOffset = if ($SpriteBytes.Length -ge 28) { [System.BitConverter]::ToUInt32($SpriteBytes, 24) } else { 0 }
                $SpritePiecesOffset = if ($SpriteBytes.Length -ge 32) { [System.BitConverter]::ToUInt32($SpriteBytes, 28) } else { 0 }
                $ReservedHeaderNonzero = @($SpriteBytes[32..47] | Where-Object { $_ -ne 0 }).Count
                $InvalidUniversalColors = 0
                foreach ($Offset in @(48, 52, 56, 60)) {
                    if ($SpriteBytes[$Offset] -ne 0x0F) { ++$InvalidUniversalColors }
                }
                $InvalidSpritePaletteColors = @($SpriteBytes[48..63] | Where-Object { $_ -gt 0x3F }).Count

                $Jumbotron = 64
                $Goal = 84
                $GroupsValid =
                    [System.BitConverter]::ToUInt16($SpriteBytes, $Jumbotron + 0) -eq 1 -and
                    [System.BitConverter]::ToUInt16($SpriteBytes, $Jumbotron + 2) -eq 1 -and
                    [System.BitConverter]::ToUInt32($SpriteBytes, $Jumbotron + 4) -eq 0 -and
                    [System.BitConverter]::ToUInt32($SpriteBytes, $Jumbotron + 8) -eq 55 -and
                    [System.BitConverter]::ToInt16($SpriteBytes, $Jumbotron + 12) -eq 0 -and
                    [System.BitConverter]::ToInt16($SpriteBytes, $Jumbotron + 14) -eq 0 -and
                    [System.BitConverter]::ToInt16($SpriteBytes, $Jumbotron + 16) -eq 0 -and
                    [System.BitConverter]::ToInt16($SpriteBytes, $Jumbotron + 18) -eq 2 -and
                    [System.BitConverter]::ToUInt16($SpriteBytes, $Goal + 0) -eq 2 -and
                    [System.BitConverter]::ToUInt16($SpriteBytes, $Goal + 2) -eq 0 -and
                    [System.BitConverter]::ToUInt32($SpriteBytes, $Goal + 4) -eq 55 -and
                    [System.BitConverter]::ToUInt32($SpriteBytes, $Goal + 8) -eq 16 -and
                    [System.BitConverter]::ToInt16($SpriteBytes, $Goal + 12) -eq 165 -and
                    [System.BitConverter]::ToInt16($SpriteBytes, $Goal + 14) -eq 350 -and
                    [System.BitConverter]::ToInt16($SpriteBytes, $Goal + 16) -eq 0 -and
                    [System.BitConverter]::ToInt16($SpriteBytes, $Goal + 18) -eq 2

                $InvalidSpritePalettes = 0
                $InvalidSpriteFlags = 0
                $InvalidConnectorOverlays = 0
                $InvalidSpriteChrOffsets = 0
                $InvalidGoalY = 0
                $ConnectorOverlayCount = 0
                $ZeroConnectorOverlayCount = 0
                $GoalConnectorOverlayContractCount = 0
                $SpriteChrByteCount = [uint64]$ExpectedChrBanks * 8192
                $SpriteChrPagesAvailable = $SpriteChrByteCount -ge 10240
                if ($SpriteBytes.Length -eq 956 -and $SpritePieceCount -eq 71 -and $SpritePieceStride -eq 12) {
                    for ($Index = 0; $Index -lt 71; ++$Index) {
                        $PieceOffset = 104 + $Index * 12
                        $Dx = [System.BitConverter]::ToInt16($SpriteBytes, $PieceOffset + 0)
                        $Dy = [System.BitConverter]::ToInt16($SpriteBytes, $PieceOffset + 2)
                        $ChrOffset = [System.BitConverter]::ToUInt32($SpriteBytes, $PieceOffset + 4)
                        $ConnectorOverlayYAdjust = [System.BitConverter]::ToInt16($SpriteBytes, $PieceOffset + 10)
                        if ($SpriteBytes[$PieceOffset + 8] -gt 3) { ++$InvalidSpritePalettes }
                        if (($SpriteBytes[$PieceOffset + 9] -band 0xFC) -ne 0) { ++$InvalidSpriteFlags }
                        if ($ConnectorOverlayYAdjust -eq -1) {
                            ++$ConnectorOverlayCount
                            if ($Index -ge 55 -and $Dx -eq 16 -and $Dy -eq 32 -and $ChrOffset -eq 9056) {
                                ++$GoalConnectorOverlayContractCount
                            } else {
                                ++$InvalidConnectorOverlays
                            }
                        } elseif ($ConnectorOverlayYAdjust -eq 0) {
                            ++$ZeroConnectorOverlayCount
                        } else {
                            ++$InvalidConnectorOverlays
                        }
                        if (($ChrOffset -band 0x0F) -ne 0 -or $ChrOffset -lt 8192 -or ([uint64]$ChrOffset + 32) -gt 10240) {
                            ++$InvalidSpriteChrOffsets
                        }
                        if ($Index -ge 55 -and !(@(0, 16, 32, 48).Contains([int]$Dy))) {
                            ++$InvalidGoalY
                        }
                    }
                }
                $SpriteContractPassed = $SpriteMagic -eq "TASG" -and $SpriteVersion -eq 2 -and
                    $SpriteHeaderSize -eq 48 -and $SpriteGroupCount -eq 2 -and $SpriteGroupStride -eq 20 -and
                    $SpritePieceCount -eq 71 -and $SpritePieceStride -eq 12 -and $SpriteFlags -eq 1 -and
                    $SpritePaletteOffset -eq 48 -and $SpriteGroupsOffset -eq 64 -and $SpritePiecesOffset -eq 104 -and
                    $SpriteBytes.Length -eq 956 -and $ReservedHeaderNonzero -eq 0 -and
                    $InvalidUniversalColors -eq 0 -and $InvalidSpritePaletteColors -eq 0 -and $GroupsValid -and
                    $SpriteChrPagesAvailable -and $KnownReferenceSpriteContentMatches -and
                    $InvalidSpritePalettes -eq 0 -and $InvalidSpriteFlags -eq 0 -and
                    $InvalidConnectorOverlays -eq 0 -and $InvalidSpriteChrOffsets -eq 0 -and $InvalidGoalY -eq 0 -and
                    $ConnectorOverlayCount -eq 1 -and $ZeroConnectorOverlayCount -eq 70 -and
                    $GoalConnectorOverlayContractCount -eq 1
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-arena-sprite-groups"
                    passed = $SpriteContractPassed
                    format = $SpriteMagic
                    version = $SpriteVersion
                    group_count = $SpriteGroupCount
                    piece_count = $SpritePieceCount
                    byte_count = $SpriteBytes.Length
                    groups_valid = $GroupsValid
                    normalized_palette_count = 4 - $InvalidUniversalColors
                    palette_colors_valid = $InvalidSpritePaletteColors -eq 0
                    chr_page_pair_bounds_valid = $SpriteChrPagesAvailable -and $InvalidSpriteChrOffsets -eq 0
                    connector_overlay_count = $ConnectorOverlayCount
                    zero_connector_overlay_count = $ZeroConnectorOverlayCount
                    goal_connector_overlay_contract_count = $GoalConnectorOverlayContractCount
                    known_reference_revision = $RomSha256 -eq $KnownReferenceRomSha256
                    known_reference_content_match = $KnownReferenceSpriteContentMatches
                    invalid_piece_count = $InvalidSpritePalettes + $InvalidSpriteFlags + $InvalidConnectorOverlays + $InvalidSpriteChrOffsets + $InvalidGoalY
                    raw_asset_bytes_persisted = $false
                })
            } catch {
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-arena-sprite-groups"
                    passed = $false
                    error = "arena sprite groups inspection failed"
                    error_type = Get-SafeExceptionName $_
                    raw_asset_bytes_persisted = $false
                })
            }

            try {
                $ReadyBytes = Read-AssetPackEntryBytes -Path $AssetPackPath -Directory $Directory -EntryId "arena/intro/ready-screen"
                $ReadyMagic = if ($ReadyBytes.Length -ge 4) { [System.Text.Encoding]::ASCII.GetString($ReadyBytes, 0, 4) } else { "" }
                $ReadyPassed = $ReadyBytes.Length -eq 6005 -and
                    $ReadyMagic -eq "TRDY" -and
                    [System.BitConverter]::ToUInt16($ReadyBytes, 4) -eq 1 -and
                    [System.BitConverter]::ToUInt16($ReadyBytes, 6) -eq 64 -and
                    [System.BitConverter]::ToUInt16($ReadyBytes, 8) -eq 32 -and
                    [System.BitConverter]::ToUInt16($ReadyBytes, 10) -eq 30 -and
                    [System.BitConverter]::ToUInt16($ReadyBytes, 14) -eq 5 -and
                    [System.BitConverter]::ToUInt16($ReadyBytes, 16) -eq 12 -and
                    [System.BitConverter]::ToUInt16($ReadyBytes, 18) -eq 8 -and
                    [System.BitConverter]::ToUInt32($ReadyBytes, 40) -eq 58
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-ready-native"
                    passed = $ReadyPassed
                    format = $ReadyMagic
                    byte_count = $ReadyBytes.Length
                    palette_stage_count = [System.BitConverter]::ToUInt16($ReadyBytes, 14)
                    reveal_record_count = [System.BitConverter]::ToUInt16($ReadyBytes, 16)
                    handoff_frame = [System.BitConverter]::ToUInt32($ReadyBytes, 40)
                    raw_asset_bytes_persisted = $false
                })

                $WarriorsBytes = Read-AssetPackEntryBytes -Path $AssetPackPath -Directory $Directory -EntryId "arena/intro/warriors-transition"
                $WarriorsMagic = if ($WarriorsBytes.Length -ge 4) { [System.Text.Encoding]::ASCII.GetString($WarriorsBytes, 0, 4) } else { "" }
                $WordmarkOffset = if ($WarriorsBytes.Length -ge 68) { [System.BitConverter]::ToUInt32($WarriorsBytes, 64) } else { 0 }
                $InvalidWordmarkTiles = 0
                if ($WordmarkOffset -eq 20832 -and $WarriorsBytes.Length -eq 21024) {
                    for ($Index = 0; $Index -lt 32; ++$Index) {
                        $CellOffset = [int]$WordmarkOffset + $Index * 6
                        $Tile = $WarriorsBytes[$CellOffset]
                        $PaletteIndex = $WarriorsBytes[$CellOffset + 1]
                        $ChrOffset = [System.BitConverter]::ToUInt32($WarriorsBytes, $CellOffset + 2)
                        if ($Tile -lt 0x80 -or $PaletteIndex -gt 3 -or
                            ($ChrOffset -band 0x0F) -ne 0 -or
                            ([uint64]$ChrOffset + 16) -gt ([uint64]$ExpectedChrBanks * 8192)) {
                            ++$InvalidWordmarkTiles
                        }
                    }
                } else {
                    $InvalidWordmarkTiles = 32
                }
                $WarriorsPassed = $WarriorsBytes.Length -eq 21024 -and
                    $WarriorsMagic -eq "TWAR" -and
                    [System.BitConverter]::ToUInt16($WarriorsBytes, 4) -eq 1 -and
                    [System.BitConverter]::ToUInt16($WarriorsBytes, 6) -eq 96 -and
                    [System.BitConverter]::ToUInt16($WarriorsBytes, 12) -eq 2 -and
                    [System.BitConverter]::ToUInt16($WarriorsBytes, 16) -eq 46 -and
                    [System.BitConverter]::ToUInt16($WarriorsBytes, 20) -eq 2 -and
                    [System.BitConverter]::ToUInt16($WarriorsBytes, 22) -eq 64 -and
                    [System.BitConverter]::ToUInt16($WarriorsBytes, 24) -eq 168 -and
                    [System.BitConverter]::ToUInt32($WarriorsBytes, 48) -eq 214 -and
                    $WarriorsBytes[52] -eq 0x1B -and
                    $WordmarkOffset -eq 20832 -and
                    [System.BitConverter]::ToUInt16($WarriorsBytes, 68) -eq 8 -and
                    [System.BitConverter]::ToUInt16($WarriorsBytes, 70) -eq 4 -and
                    [System.BitConverter]::ToUInt16($WarriorsBytes, 72) -eq 8 -and
                    [System.BitConverter]::ToUInt16($WarriorsBytes, 74) -eq 26 -and
                    $InvalidWordmarkTiles -eq 0
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-warriors-native"
                    passed = $WarriorsPassed
                    format = $WarriorsMagic
                    byte_count = $WarriorsBytes.Length
                    sprite_piece_count = [System.BitConverter]::ToUInt16($WarriorsBytes, 16)
                    split_scanline = [System.BitConverter]::ToUInt16($WarriorsBytes, 24)
                    wordmark_glyph_count = [System.BitConverter]::ToUInt16($WarriorsBytes, 68)
                    invalid_wordmark_tiles = $InvalidWordmarkTiles
                    handoff_frame = [System.BitConverter]::ToUInt32($WarriorsBytes, 48)
                    raw_asset_bytes_persisted = $false
                })

                $ClippersBytes = Read-AssetPackEntryBytes -Path $AssetPackPath -Directory $Directory -EntryId "arena/intro/clippers-transition"
                $ClippersMagic = if ($ClippersBytes.Length -ge 4) { [System.Text.Encoding]::ASCII.GetString($ClippersBytes, 0, 4) } else { "" }
                [byte[]]$ExpectedClippersWordmark = @(
                    0xBA,0xBB,0xBC,0xBD, 0xD0,0xFF,0xC0,0xD1,
                    0xC7,0xC8,0xC9,0xCA, 0xB6,0xB7,0xB4,0xD8,
                    0xB6,0xB7,0xB4,0xD8, 0xB6,0xC2,0xB8,0xC3,
                    0xB6,0xB7,0xB4,0xCF, 0xD9,0xDA,0xDB,0xB9
                )
                $InvalidClippersWordmarkTiles = 0
                if ($ClippersBytes.Length -eq 19680) {
                    for ($Index = 0; $Index -lt 32; ++$Index) {
                        $CellOffset = 19360 + $Index * 10
                        $BaseChrOffset = [System.BitConverter]::ToUInt32($ClippersBytes, $CellOffset + 2)
                        $LowerChrOffset = [System.BitConverter]::ToUInt32($ClippersBytes, $CellOffset + 6)
                        if ($ClippersBytes[$CellOffset] -ne $ExpectedClippersWordmark[$Index] -or
                            $ClippersBytes[$CellOffset + 1] -gt 3 -or
                            ($BaseChrOffset -band 0x0F) -ne 0 -or
                            ($LowerChrOffset -band 0x0F) -ne 0 -or
                            ([uint64]$BaseChrOffset + 16) -gt ([uint64]$ExpectedChrBanks * 8192) -or
                            ([uint64]$LowerChrOffset + 16) -gt ([uint64]$ExpectedChrBanks * 8192)) {
                            ++$InvalidClippersWordmarkTiles
                        }
                    }
                } else {
                    $InvalidClippersWordmarkTiles = 32
                }
                $ClippersPassed = $ClippersBytes.Length -eq 19680 -and
                    $ClippersMagic -eq "TCLP" -and
                    [System.BitConverter]::ToUInt16($ClippersBytes, 4) -eq 1 -and
                    [System.BitConverter]::ToUInt16($ClippersBytes, 6) -eq 96 -and
                    [System.BitConverter]::ToUInt16($ClippersBytes, 12) -eq 2 -and
                    [System.BitConverter]::ToUInt16($ClippersBytes, 14) -eq 10 -and
                    [System.BitConverter]::ToUInt16($ClippersBytes, 16) -eq 4 -and
                    [System.BitConverter]::ToUInt32($ClippersBytes, 28) -eq 1920 -and
                    [System.BitConverter]::ToUInt32($ClippersBytes, 32) -eq 151 -and
                    [System.BitConverter]::ToUInt16($ClippersBytes, 36) -eq 0x883D -and
                    [System.BitConverter]::ToUInt16($ClippersBytes, 38) -eq 200 -and
                    $ClippersBytes[48] -eq 0x2C -and $ClippersBytes[49] -eq 0x2E -and
                    $ClippersBytes[50] -eq 0x2C -and $ClippersBytes[51] -eq 0xFA -and
                    [System.BitConverter]::ToUInt32($ClippersBytes, 56) -eq 19360 -and
                    [System.BitConverter]::ToUInt16($ClippersBytes, 60) -eq 32 -and
                    [System.BitConverter]::ToUInt16($ClippersBytes, 62) -eq 32 -and
                    $InvalidClippersWordmarkTiles -eq 0
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-clippers-native"
                    passed = $ClippersPassed
                    format = $ClippersMagic
                    byte_count = $ClippersBytes.Length
                    handoff_frame = [System.BitConverter]::ToUInt32($ClippersBytes, 32)
                    next_route = ('{0:X4}' -f [System.BitConverter]::ToUInt16($ClippersBytes, 36))
                    invalid_wordmark_tiles = $InvalidClippersWordmarkTiles
                    raw_asset_bytes_persisted = $false
                })

                $BucksBytes = Read-AssetPackEntryBytes -Path $AssetPackPath -Directory $Directory -EntryId "arena/intro/bucks-transition"
                $BucksMagic = if ($BucksBytes.Length -ge 4) { [System.Text.Encoding]::ASCII.GetString($BucksBytes, 0, 4) } else { "" }
                $BucksPaletteHex = @()
                if ($BucksBytes.Length -ge 160) {
                    for ($Stage = 0; $Stage -lt 4; ++$Stage) {
                        $BucksPaletteHex += [System.BitConverter]::ToString($BucksBytes, 96 + $Stage * 16, 16).Replace("-", "")
                    }
                }
                $ExpectedBucksPalettes = @(
                    "0F1727010F1716260F17272A0F172711",
                    "0F1727010F0706160F07171A0F071701",
                    "0F1727010F0F0F060F0F070A0F0F070F",
                    "0F1727010F0F0F0F0F0F0F0F0F0F0F0F"
                )
                $BucksThresholds = if ($BucksBytes.Length -ge 70) { [System.BitConverter]::ToString($BucksBytes, 64, 6).Replace("-", "") } else { "" }
                $BucksPassed = $BucksBytes.Length -eq 19560 -and $BucksMagic -eq "TBUC" -and
                    [System.BitConverter]::ToUInt16($BucksBytes, 6) -eq 96 -and
                    [System.BitConverter]::ToUInt16($BucksBytes, 12) -eq 2 -and
                    [System.BitConverter]::ToUInt16($BucksBytes, 14) -eq 10 -and
                    [System.BitConverter]::ToUInt16($BucksBytes, 16) -eq 4 -and
                    [System.BitConverter]::ToUInt16($BucksBytes, 18) -eq 5 -and
                    [System.BitConverter]::ToUInt32($BucksBytes, 36) -eq 83 -and
                    [System.BitConverter]::ToUInt16($BucksBytes, 40) -eq 0x854F -and
                    [System.BitConverter]::ToUInt16($BucksBytes, 42) -eq 31 -and
                    [System.BitConverter]::ToUInt16($BucksBytes, 44) -eq 168 -and
                    $BucksBytes[52] -eq 0x5E -and $BucksBytes[53] -eq 0x60 -and
                    $BucksBytes[54] -eq 0x5E -and $BucksBytes[55] -eq 0xFA -and
                    $BucksPaletteHex.Count -eq 4 -and (@(Compare-Object $ExpectedBucksPalettes $BucksPaletteHex)).Count -eq 0 -and
                    $BucksThresholds -eq "EFC090603000"
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-bucks-native"
                    passed = $BucksPassed
                    format = $BucksMagic
                    byte_count = $BucksBytes.Length
                    palettes = $BucksPaletteHex
                    thresholds = $BucksThresholds
                    handoff_frame = [System.BitConverter]::ToUInt32($BucksBytes, 36)
                    raw_asset_bytes_persisted = $false
                })

                $PassBytes = Read-AssetPackEntryBytes -Path $AssetPackPath -Directory $Directory -EntryId "arena/intro/pass-transition"
                $PassMagic = if ($PassBytes.Length -ge 4) { [System.Text.Encoding]::ASCII.GetString($PassBytes, 0, 4) } else { "" }
                $PassPaletteHex = @()
                if ($PassBytes.Length -ge 208) {
                    for ($Stage = 0; $Stage -lt 5; ++$Stage) {
                        $PassPaletteHex += [System.BitConverter]::ToString($PassBytes, 128 + $Stage * 16, 16).Replace("-", "")
                    }
                }
                $ExpectedPassPalettes = @(
                    "0F070707020F0607020C010002010707",
                    "0F171717020F1617020C011002111717",
                    "0F172727020F1627020C012002211727",
                    "0F172737020F1627020C013002211727",
                    "0F011121020F1627020C0130020C1727"
                )
                $PassPassed = $PassBytes.Length -eq 11904 -and $PassMagic -eq "TPAS" -and
                    [System.BitConverter]::ToUInt16($PassBytes, 6) -eq 128 -and
                    [System.BitConverter]::ToUInt16($PassBytes, 16) -eq 10 -and
                    [System.BitConverter]::ToUInt16($PassBytes, 20) -eq 5 -and
                    [System.BitConverter]::ToUInt32($PassBytes, 40) -eq 52 -and
                    [System.BitConverter]::ToUInt16($PassBytes, 44) -eq 0x851C -and
                    [System.BitConverter]::ToUInt16($PassBytes, 46) -eq 18 -and
                    [System.BitConverter]::ToUInt16($PassBytes, 48) -eq 30 -and
                    $PassBytes[50] -eq 0x68 -and $PassBytes[51] -eq 8 -and
                    $PassBytes[52] -eq 0xF0 -and $PassBytes[53] -eq 0xF2 -and
                    $PassBytes[54] -eq 0x91 -and $PassBytes[55] -eq 0x93 -and $PassBytes[56] -eq 0x95 -and
                    $PassBytes[69] -eq 28 -and
                    $PassPaletteHex.Count -eq 5 -and (@(Compare-Object $ExpectedPassPalettes $PassPaletteHex)).Count -eq 0
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-pass-native"
                    passed = $PassPassed
                    format = $PassMagic
                    byte_count = $PassBytes.Length
                    palettes = $PassPaletteHex
                    piece_count = [System.BitConverter]::ToUInt16($PassBytes, 16)
                    handoff_frame = [System.BitConverter]::ToUInt32($PassBytes, 40)
                    raw_asset_bytes_persisted = $false
                })

                $ChrByteCount = [uint64]$ExpectedChrBanks * 8192
                $PresentsBytes = Read-AssetPackEntryBytes `
                    -Path $AssetPackPath -Directory $Directory `
                    -EntryId "intro/tecmo-presents-screen"
                $LicenseBytes = Read-AssetPackEntryBytes `
                    -Path $AssetPackPath -Directory $Directory `
                    -EntryId "intro/nba-license-screen"
                $PresentsContract = Test-OpeningScreenPayloadContract `
                    -Bytes $PresentsBytes -ChrByteCount $ChrByteCount -Kind "presents"
                $LicenseContract = Test-OpeningScreenPayloadContract `
                    -Bytes $LicenseBytes -ChrByteCount $ChrByteCount -Kind "license"
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-opening-presents-native"
                    passed = $PresentsContract.passed
                    format = if ($PresentsBytes.Length -ge 4) {
                        [System.Text.Encoding]::ASCII.GetString($PresentsBytes, 0, 4)
                    } else { "" }
                    byte_count = $PresentsBytes.Length
                    palette_stage_count = if ($PresentsBytes.Length -ge 26) {
                        [System.BitConverter]::ToUInt16($PresentsBytes, 24)
                    } else { 0 }
                    sprite_count = if ($PresentsBytes.Length -ge 40) {
                        [System.BitConverter]::ToUInt16($PresentsBytes, 38)
                    } else { 0 }
                    contract_issues = $PresentsContract.issues
                    raw_asset_bytes_persisted = $false
                })
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-opening-license-native"
                    passed = $LicenseContract.passed
                    format = if ($LicenseBytes.Length -ge 4) {
                        [System.Text.Encoding]::ASCII.GetString($LicenseBytes, 0, 4)
                    } else { "" }
                    byte_count = $LicenseBytes.Length
                    palette_stage_count = if ($LicenseBytes.Length -ge 26) {
                        [System.BitConverter]::ToUInt16($LicenseBytes, 24)
                    } else { 0 }
                    sprite_count = if ($LicenseBytes.Length -ge 40) {
                        [System.BitConverter]::ToUInt16($LicenseBytes, 38)
                    } else { 0 }
                    contract_issues = $LicenseContract.issues
                    raw_asset_bytes_persisted = $false
                })

                $OpeningMalformedCases = [System.Collections.Generic.List[object]]::new()
                $OpeningMalformedSpecs = @(
                    [pscustomobject]@{ id = "presents-magic"; kind = "presents"; mutate = {
                        param([byte[]]$Data) $Data[0] = [byte][char]'X'
                    } },
                    [pscustomobject]@{ id = "presents-packed-layout"; kind = "presents"; mutate = {
                        param([byte[]]$Data)
                        [System.BitConverter]::GetBytes([uint32]6113).CopyTo($Data, 32)
                    } },
                    [pscustomobject]@{ id = "presents-declared-size"; kind = "presents"; mutate = {
                        param([byte[]]$Data)
                        [System.BitConverter]::GetBytes([uint32]($Data.Length - 1)).CopyTo($Data, 40)
                    } },
                    [pscustomobject]@{ id = "presents-header-reserved"; kind = "presents"; mutate = {
                        param([byte[]]$Data) $Data[54] = 1
                    } },
                    [pscustomobject]@{ id = "presents-cell-palette"; kind = "presents"; mutate = {
                        param([byte[]]$Data) $Data[65] = 4
                    } },
                    [pscustomobject]@{ id = "presents-cell-chr-alignment"; kind = "presents"; mutate = {
                        param([byte[]]$Data) $Data[66] = $Data[66] -bxor 1
                    } },
                    [pscustomobject]@{ id = "presents-cell-chr-range"; kind = "presents"; mutate = {
                        param([byte[]]$Data)
                        [System.BitConverter]::GetBytes([uint32]$ChrByteCount).CopyTo($Data, 66)
                    } },
                    [pscustomobject]@{ id = "presents-nonblank-count"; kind = "presents"; mutate = {
                        param([byte[]]$Data)
                        for ($Cell = 0; $Cell -lt 960; ++$Cell) {
                            $Offset = 64 + $Cell * 6
                            if ($Data[$Offset] -eq 0xFF) { $Data[$Offset] = 0; break }
                        }
                    } },
                    [pscustomobject]@{ id = "presents-palette-range"; kind = "presents"; mutate = {
                        param([byte[]]$Data) $Data[5824] = 0x40
                    } },
                    [pscustomobject]@{ id = "presents-palette-fade"; kind = "presents"; mutate = {
                        param([byte[]]$Data) $Data[5824 + 32 + 1] =
                            [byte](($Data[5824 + 32 + 1] + 1) -band 0x3F)
                    } },
                    [pscustomobject]@{ id = "presents-palette-frame"; kind = "presents"; mutate = {
                        param([byte[]]$Data) $Data[6112 + 2] = 5
                    } },
                    [pscustomobject]@{ id = "presents-sprite-palette"; kind = "presents"; mutate = {
                        param([byte[]]$Data) $Data[6130 + 8] = 4
                    } },
                    [pscustomobject]@{ id = "presents-sprite-flags"; kind = "presents"; mutate = {
                        param([byte[]]$Data) $Data[6130 + 9] = 4
                    } },
                    [pscustomobject]@{ id = "presents-sprite-reserved"; kind = "presents"; mutate = {
                        param([byte[]]$Data) $Data[6130 + 10] = 1
                    } },
                    [pscustomobject]@{ id = "presents-sprite-coordinate"; kind = "presents"; mutate = {
                        param([byte[]]$Data) $Data[6130] = 0
                    } },
                    [pscustomobject]@{ id = "presents-sprite-chr-alignment"; kind = "presents"; mutate = {
                        param([byte[]]$Data) $Data[6130 + 4] = $Data[6130 + 4] -bxor 1
                    } },
                    [pscustomobject]@{ id = "presents-sprite-chr-range"; kind = "presents"; mutate = {
                        param([byte[]]$Data)
                        [System.BitConverter]::GetBytes([uint32]$ChrByteCount).CopyTo($Data, 6130 + 4)
                    } },
                    [pscustomobject]@{ id = "license-stage-count"; kind = "license"; mutate = {
                        param([byte[]]$Data) $Data[24] = 5
                    } },
                    [pscustomobject]@{ id = "license-palette-stride"; kind = "license"; mutate = {
                        param([byte[]]$Data) $Data[26] = 32
                    } },
                    [pscustomobject]@{ id = "license-duration"; kind = "license"; mutate = {
                        param([byte[]]$Data) $Data[36] = 0
                    } },
                    [pscustomobject]@{ id = "license-palette-frame"; kind = "license"; mutate = {
                        param([byte[]]$Data) $Data[5920 + 2] = 35
                    } },
                    [pscustomobject]@{ id = "license-final-palette"; kind = "license"; mutate = {
                        param([byte[]]$Data) $Data[5824 + 5 * 16] = 0x0E
                    } },
                    [pscustomobject]@{ id = "license-sprite-count"; kind = "license"; mutate = {
                        param([byte[]]$Data) $Data[38] = 1
                    } }
                )
                foreach ($Spec in $OpeningMalformedSpecs) {
                    $SourceBytes = if ($Spec.kind -eq "presents") {
                        $PresentsBytes
                    } else {
                        $LicenseBytes
                    }
                    $Malformed = [byte[]]$SourceBytes.Clone()
                    & $Spec.mutate $Malformed
                    $Rejected = !(Test-OpeningScreenPayloadContract `
                        -Bytes $Malformed -ChrByteCount $ChrByteCount -Kind $Spec.kind).passed
                    [void]$OpeningMalformedCases.Add([pscustomobject]@{
                        id = $Spec.id
                        rejected = $Rejected
                    })
                }
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-opening-malformed-contracts"
                    passed = @($OpeningMalformedCases | Where-Object { !$_.rejected }).Count -eq 0
                    cases = $OpeningMalformedCases.ToArray()
                    mutated_payloads_persisted = $false
                    raw_asset_bytes_persisted = $false
                })

                $StartMenuBytes = Read-AssetPackEntryBytes -Path $AssetPackPath -Directory $Directory -EntryId "menu/start-game"
                $StartMenuContract = Test-StartMenuPayloadContract -Bytes $StartMenuBytes -ChrByteCount $ChrByteCount
                $TitleBgPalette = Read-FileBytesAtOffset -Path $ReferenceRom -Offset ([uint64]($PrgStart + (0x9366 - 0x8000))) -Count 16
                $TitleSpritePalette = Read-FileBytesAtOffset -Path $ReferenceRom -Offset ([uint64]($PrgStart + (0xBE27 - 0x8000))) -Count 16
                $MenuBgPalette = Read-FileBytesAtOffset -Path $ReferenceRom -Offset ([uint64]($PrgStart + (0x9356 - 0x8000))) -Count 16
                $MenuSpritePalette = Read-FileBytesAtOffset -Path $ReferenceRom -Offset ([uint64]($PrgStart + (0xBE37 - 0x8000))) -Count 16
                $ExpectedMenuPalettes = [System.Collections.Generic.List[byte]]::new()
                $TitlePaletteSource = @($TitleBgPalette + $TitleSpritePalette)
                for ($Stage = 0; $Stage -lt 4; ++$Stage) {
                    $Reduction = $Stage * 0x10
                    foreach ($Color in $TitlePaletteSource) {
                        $Value = if ($Color -eq 0x0F) { 0x0F } elseif (($Color -band 0x30) -ge $Reduction) {
                            $Color - $Reduction
                        } else { 0x0F }
                        [void]$ExpectedMenuPalettes.Add([byte]$Value)
                    }
                }
                for ($Index = 0; $Index -lt 32; ++$Index) { [void]$ExpectedMenuPalettes.Add([byte]0x0F) }
                $MenuPaletteSource = @($MenuBgPalette + $MenuSpritePalette)
                for ($Cap = 0; $Cap -lt 4; ++$Cap) {
                    foreach ($Color in $MenuPaletteSource) {
                        $Value = $Color
                        if ($Color -ne 0x0F -and ($Color -band 0x30) -gt ($Cap * 0x10)) {
                            $Value = ($Color -band 0x0F) -bor ($Cap * 0x10)
                        }
                        [void]$ExpectedMenuPalettes.Add([byte]$Value)
                    }
                }
                $ActualMenuPalettes = [byte[]]$StartMenuBytes[11680..11967]
                $MenuPalettesExact = (@(Compare-Object $ExpectedMenuPalettes.ToArray() $ActualMenuPalettes -SyncWindow 0)).Count -eq 0
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-start-game-menu-native"
                    passed = $StartMenuContract.passed -and $MenuPalettesExact
                    format = if ($StartMenuBytes.Length -ge 4) { [System.Text.Encoding]::ASCII.GetString($StartMenuBytes, 0, 4) } else { "" }
                    byte_count = $StartMenuBytes.Length
                    page_count = if ($StartMenuBytes.Length -ge 14) { [System.BitConverter]::ToUInt16($StartMenuBytes, 12) } else { 0 }
                    palette_stage_count = if ($StartMenuBytes.Length -ge 30) { [System.BitConverter]::ToUInt16($StartMenuBytes, 28) } else { 0 }
                    root_item_count = if ($StartMenuBytes.Length -ge 52) { [System.BitConverter]::ToUInt16($StartMenuBytes, 50) } else { 0 }
                    season_transition_frames = if ($StartMenuBytes.Length -ge 58) { [System.BitConverter]::ToUInt16($StartMenuBytes, 56) } else { 0 }
                    palette_stages_exact = $MenuPalettesExact
                    contract_issues = $StartMenuContract.issues
                    raw_asset_bytes_persisted = $false
                })
                $StartMenuMalformedCases = [System.Collections.Generic.List[object]]::new()
                $StartMenuMalformedSpecs = @(
                    [pscustomobject]@{ id = "magic"; mutate = { param([byte[]]$Data) $Data[0] = [byte][char]'X' } },
                    [pscustomobject]@{ id = "declared-size"; mutate = { param([byte[]]$Data) [System.BitConverter]::GetBytes([uint32]($Data.Length - 1)).CopyTo($Data, 44) } },
                    [pscustomobject]@{ id = "period-value"; mutate = { param([byte[]]$Data) $Data[121] = 1 } },
                    [pscustomobject]@{ id = "reserved"; mutate = { param([byte[]]$Data) $Data[126] = 1 } },
                    [pscustomobject]@{ id = "stage-frame"; mutate = { param([byte[]]$Data) $Data[96 + 8 * 2] = 31 } },
                    [pscustomobject]@{ id = "route"; mutate = { param([byte[]]$Data) $Data[114] = 0 } },
                    [pscustomobject]@{ id = "cell-palette"; mutate = { param([byte[]]$Data) $Data[161] = 4 } },
                    [pscustomobject]@{ id = "cell-chr-alignment"; mutate = { param([byte[]]$Data) $Data[162] = $Data[162] -bxor 1 } },
                    [pscustomobject]@{ id = "cell-chr-range"; mutate = { param([byte[]]$Data) [System.BitConverter]::GetBytes([uint32]$ChrByteCount).CopyTo($Data, 162) } },
                    [pscustomobject]@{ id = "palette-range"; mutate = { param([byte[]]$Data) $Data[11680] = 0x40 } },
                    [pscustomobject]@{ id = "black-stage"; mutate = { param([byte[]]$Data) $Data[11680 + 4 * 32] = 0 } },
                    [pscustomobject]@{ id = "emblem-reserved"; mutate = { param([byte[]]$Data) $Data[11968 + 14] = 1 } },
                    [pscustomobject]@{ id = "cursor-coordinate"; mutate = { param([byte[]]$Data) $Data[12752] = 30 } },
                    [pscustomobject]@{ id = "overlay-desc"; mutate = { param([byte[]]$Data) $Data[12768 + 14] = 7 } }
                )
                foreach ($Spec in $StartMenuMalformedSpecs) {
                    $Malformed = [byte[]]$StartMenuBytes.Clone()
                    & $Spec.mutate $Malformed
                    $Rejected = !(Test-StartMenuPayloadContract -Bytes $Malformed -ChrByteCount $ChrByteCount).passed
                    [void]$StartMenuMalformedCases.Add([pscustomobject]@{ id = $Spec.id; rejected = $Rejected })
                }
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-start-game-menu-malformed-contracts"
                    passed = @($StartMenuMalformedCases | Where-Object { !$_.rejected }).Count -eq 0
                    cases = $StartMenuMalformedCases.ToArray()
                    mutated_payloads_persisted = $false
                    raw_asset_bytes_persisted = $false
                })

                $FinaleBytes = Read-AssetPackEntryBytes -Path $AssetPackPath -Directory $Directory -EntryId "intro/finale-sequence"
                $FinaleContract = Test-FinalePayloadContract -Bytes $FinaleBytes -ChrByteCount ([uint64]$ExpectedChrBanks * 8192)
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-finale-native"
                    passed = $FinaleContract.passed
                    format = if ($FinaleBytes.Length -ge 4) { [System.Text.Encoding]::ASCII.GetString($FinaleBytes, 0, 4) } else { "" }
                    byte_count = $FinaleBytes.Length
                    screen_count = if ($FinaleBytes.Length -ge 10) { [System.BitConverter]::ToUInt16($FinaleBytes, 8) } else { 0 }
                    sprite_group_count = if ($FinaleBytes.Length -ge 42) { [System.BitConverter]::ToUInt16($FinaleBytes, 40) } else { 0 }
                    title_slot_count = if ($FinaleBytes.Length -ge 90) { [System.BitConverter]::ToUInt16($FinaleBytes, 88) } else { 0 }
                    one_page_layers_mirrored = $FinaleContract.issues -notcontains "one-page-mirror-0" -and
                        $FinaleContract.issues -notcontains "one-page-mirror-3"
                    contract_issues = $FinaleContract.issues
                    raw_asset_bytes_persisted = $false
                })

                $MalformedCases = [System.Collections.Generic.List[object]]::new()
                $MalformedSpecs = @(
                    [pscustomobject]@{ id = "reserved"; mutate = { param([byte[]]$Data) $Data[116] = 1 } },
                    [pscustomobject]@{ id = "header-offset-high-reserved"; mutate = { param([byte[]]$Data) $Data[106] = 1 } },
                    [pscustomobject]@{ id = "post-anchor-reserved"; mutate = { param([byte[]]$Data) $Data[58246] = 1 } },
                    [pscustomobject]@{ id = "offset"; mutate = {
                        param([byte[]]$Data)
                        [Array]::Copy([System.BitConverter]::GetBytes([uint32]193), 0, $Data, 20, 4)
                    } },
                    [pscustomobject]@{ id = "count"; mutate = {
                        param([byte[]]$Data)
                        [Array]::Copy([System.BitConverter]::GetBytes([uint16]1), 0, $Data, 40, 2)
                    } },
                    [pscustomobject]@{ id = "group"; mutate = { param([byte[]]$Data) $Data[57964] = 7 } },
                    [pscustomobject]@{ id = "palette"; mutate = { param([byte[]]$Data) $Data[57792] = 0x40 } },
                    [pscustomobject]@{ id = "route"; mutate = { param([byte[]]$Data) $Data[58188 + 2] = 7 } },
                    [pscustomobject]@{ id = "band"; mutate = { param([byte[]]$Data) $Data[58296 + 4] = 7 } },
                    [pscustomobject]@{ id = "title-order"; mutate = { param([byte[]]$Data) $Data[58344] = 0 } },
                    [pscustomobject]@{ id = "title-reserved"; mutate = { param([byte[]]$Data) $Data[58344 + 28] = 1 } },
                    [pscustomobject]@{ id = "chr-range"; mutate = {
                        param([byte[]]$Data)
                        [Array]::Copy([System.BitConverter]::GetBytes([uint32]([uint64]$ExpectedChrBanks * 8192)), 0, $Data, 194, 4)
                    } },
                    [pscustomobject]@{ id = "chr-alignment"; mutate = {
                        param([byte[]]$Data)
                        $Value = [System.BitConverter]::ToUInt32($Data, 194) + 1
                        [Array]::Copy([System.BitConverter]::GetBytes([uint32]$Value), 0, $Data, 194, 4)
                    } },
                    [pscustomobject]@{ id = "one-page-mirror"; mutate = { param([byte[]]$Data) $Data[192 + 960 * 6] = $Data[192 + 960 * 6] -bxor 1 } }
                )
                foreach ($Spec in $MalformedSpecs) {
                    $Malformed = [byte[]]$FinaleBytes.Clone()
                    & $Spec.mutate $Malformed
                    $Rejected = !(Test-FinalePayloadContract -Bytes $Malformed -ChrByteCount ([uint64]$ExpectedChrBanks * 8192)).passed
                    [void]$MalformedCases.Add([pscustomobject]@{ id = $Spec.id; rejected = $Rejected })
                }
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-finale-malformed-contracts"
                    passed = @($MalformedCases | Where-Object { !$_.rejected }).Count -eq 0
                    cases = $MalformedCases.ToArray()
                    mutated_payloads_persisted = $false
                    raw_asset_bytes_persisted = $false
                })
            } catch {
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-post-arena-native"
                    passed = $false
                    error = "post-arena native asset inspection failed"
                    error_type = Get-SafeExceptionName $_
                    raw_asset_bytes_persisted = $false
                })
            }

            try {
                $SourceMapText = Read-AssetPackEntryText -Path $AssetPackPath -Directory $Directory -EntryId "system/source-map"
                $SourceMap = $SourceMapText | ConvertFrom-Json
                $SourceMapLogicalIds = @($SourceMap.logical_entries | ForEach-Object { [string]$_.id })
                $MissingSourceMapLogicalEntries = @($ExpectedLogicalEntries | Where-Object { !$SourceMapLogicalIds.Contains($_) })
                $UnexpectedSourceMapLogicalEntries = @($SourceMapLogicalIds | Where-Object { !$ExpectedLogicalEntrySet.Contains($_) })
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-source-map-logical-entries"
                    passed = $MissingSourceMapLogicalEntries.Count -eq 0 -and $UnexpectedSourceMapLogicalEntries.Count -eq 0
                    expected_logical_entries = $ExpectedLogicalEntries
                    source_map_logical_entries = $SourceMapLogicalIds
                    missing_logical_entries = $MissingSourceMapLogicalEntries
                    unexpected_logical_entries = $UnexpectedSourceMapLogicalEntries
                    raw_asset_bytes_persisted = $false
                })

                $PresentsSource = @($SourceMap.logical_entries | Where-Object {
                    $_.id -eq "intro/tecmo-presents-screen"
                } | Select-Object -First 1)
                $LicenseSource = @($SourceMap.logical_entries | Where-Object {
                    $_.id -eq "intro/nba-license-screen"
                } | Select-Object -First 1)
                $PresentsRoles = @($PresentsSource.sources | ForEach-Object { [string]$_.role })
                $LicenseRoles = @($LicenseSource.sources | ForEach-Object { [string]$_.role })
                $PresentsDescriptor = @($PresentsSource.sources | Where-Object { $_.role -eq "descriptor" } | Select-Object -First 1)
                $PresentsStream = @($PresentsSource.sources | Where-Object { $_.role -eq "compressed-screen" } | Select-Object -First 1)
                $PresentsSelectors = @($PresentsSource.sources | Where-Object { $_.role -eq "sprite-chr-selectors" } | Select-Object -First 1)
                $PresentsRecords = @($PresentsSource.sources | Where-Object { $_.role -eq "sprite-records" } | Select-Object -First 1)
                $PresentsSpritePalette = @($PresentsSource.sources | Where-Object { $_.role -eq "sprite-palette" } | Select-Object -First 1)
                $PresentsLayout = @($PresentsSource.sources | Where-Object { $_.role -eq "sprite-layout-operands" } | Select-Object -First 1)
                $LicenseDescriptor = @($LicenseSource.sources | Where-Object { $_.role -eq "descriptor" } | Select-Object -First 1)
                $LicenseStream = @($LicenseSource.sources | Where-Object { $_.role -eq "compressed-screen" } | Select-Object -First 1)
                $OpeningPrgStart = 16 + [int]$ReferenceHeader.trainer_bytes
                $OpeningFixedStart = $OpeningPrgStart + ($ExpectedPrgBanks - 1) * 0x4000
                $ExpectedPresentsDescriptor = [uint64]($OpeningFixedStart + (0xDC85 - 0xC000))
                $ExpectedLicenseDescriptor = [uint64]($OpeningFixedStart + (0xDC93 - 0xC000))
                $ExpectedPresentsStream = [uint64]($OpeningPrgStart + (0x84FB - 0x8000))
                $ExpectedLicenseStream = [uint64]($OpeningPrgStart + (0x856E - 0x8000))
                $ExpectedSelectorSource = [uint64]($OpeningPrgStart + (0xBC90 - 0x8000))
                $ExpectedRecordSource = [uint64]($OpeningPrgStart + (0xBDD6 - 0x8000))
                $ExpectedSpritePaletteSource = [uint64]($OpeningPrgStart + (0xBE27 - 0x8000))
                $ExpectedLayoutSources = @(
                    [uint64]($OpeningPrgStart + (0xBDA8 - 0x8000)),
                    [uint64]($OpeningPrgStart + (0xBDB2 - 0x8000)),
                    [uint64]($OpeningPrgStart + (0xBDC3 - 0x8000))
                )
                $OpeningSourcePassed = $PresentsSource.Count -eq 1 -and
                    $LicenseSource.Count -eq 1 -and
                    $PresentsSource.schema -eq "tecmo.intro.screen/TISC-1" -and
                    $LicenseSource.schema -eq "tecmo.intro.screen/TISC-1" -and
                    [int]$PresentsSource.duration_frames -eq 133 -and
                    [int]$LicenseSource.duration_frames -eq 277 -and
                    $PresentsRoles -contains "descriptor" -and
                    $PresentsRoles -contains "compressed-screen" -and
                    $PresentsRoles -contains "background-palette" -and
                    $PresentsRoles -contains "sprite-chr-selectors" -and
                    $PresentsRoles -contains "sprite-records" -and
                    $PresentsRoles -contains "sprite-palette" -and
                    $PresentsRoles -contains "sprite-layout-operands" -and
                    $LicenseRoles -contains "descriptor" -and
                    $LicenseRoles -contains "compressed-screen" -and
                    $LicenseRoles -contains "background-palette" -and
                    @($LicenseRoles | Where-Object { $_ -like "sprite-*" }).Count -eq 0 -and
                    @($PresentsRoles + $LicenseRoles | Where-Object {
                        $_ -match "trace|capture|log"
                    }).Count -eq 0 -and
                    [uint64]$PresentsDescriptor.source_offset -eq $ExpectedPresentsDescriptor -and
                    [int]$PresentsDescriptor.cpu_address -eq 0xDC85 -and
                    [uint64]$LicenseDescriptor.source_offset -eq $ExpectedLicenseDescriptor -and
                    [int]$LicenseDescriptor.cpu_address -eq 0xDC93 -and
                    [uint64]$PresentsStream.source_offset -eq $ExpectedPresentsStream -and
                    [int]$PresentsStream.cpu_address -eq 0x84FB -and
                    [int]$PresentsStream.encoded_size -eq 116 -and
                    [int]$PresentsStream.decoded_size -eq 1024 -and
                    [uint64]$LicenseStream.source_offset -eq $ExpectedLicenseStream -and
                    [int]$LicenseStream.cpu_address -eq 0x856E -and
                    [int]$LicenseStream.encoded_size -eq 177 -and
                    [int]$LicenseStream.decoded_size -eq 1024 -and
                    [uint64]$PresentsSelectors.source_offset -eq $ExpectedSelectorSource -and
                    [int]$PresentsSelectors.cpu_address -eq 0xBC90 -and
                    (@(Compare-Object @(244, 245) @($PresentsSelectors.selectors))).Count -eq 0 -and
                    [uint64]$PresentsRecords.source_offset -eq $ExpectedRecordSource -and
                    [int]$PresentsRecords.cpu_address -eq 0xBDD6 -and
                    [int]$PresentsRecords.record_count -eq 20 -and
                    [int]$PresentsRecords.record_stride -eq 4 -and
                    [uint64]$PresentsSpritePalette.source_offset -eq $ExpectedSpritePaletteSource -and
                    [int]$PresentsSpritePalette.cpu_address -eq 0xBE27 -and
                    (@(Compare-Object $ExpectedLayoutSources @($PresentsLayout.source_offsets))).Count -eq 0
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-opening-source-provenance"
                    passed = $OpeningSourcePassed
                    presents_roles = $PresentsRoles
                    license_roles = $LicenseRoles
                    rabbit_record_count = if ($PresentsRecords.Count -eq 1) {
                        [int]$PresentsRecords.record_count
                    } else { 0 }
                    loose_trace_sources = @($PresentsRoles + $LicenseRoles | Where-Object {
                        $_ -match "trace|capture|log"
                    })
                    raw_asset_bytes_persisted = $false
                })

                $TitleAttractSource = @($SourceMap.logical_entries | Where-Object { $_.id -eq "title/attract-continuation" } | Select-Object -First 1)
                $TitleStartSource = @($SourceMap.logical_entries | Where-Object { $_.id -eq "title/start-screen" } | Select-Object -First 1)
                $TitleAttractRoles = @($TitleAttractSource.sources | ForEach-Object { [string]$_.role })
                $TitleStartRoles = @($TitleStartSource.sources | ForEach-Object { [string]$_.role })
                $ExpectedAttractRoles = @("descriptor", "compressed-screen", "initial-palettes", "final-sprite-palette", "sprite-records", "attribute-state-a", "attribute-state-b")
                $ExpectedStartRoles = @("descriptor", "compressed-screen", "palette", "prompt-blank", "prompt-visible")
                $TitleSourcePassed = $TitleAttractSource.Count -eq 1 -and
                    $TitleStartSource.Count -eq 1 -and
                    $TitleAttractSource.schema -eq "tecmo.title-attract/TATR-2" -and
                    $TitleStartSource.schema -eq "tecmo.title-start/TTLE-1" -and
                    (@(Compare-Object $ExpectedAttractRoles $TitleAttractRoles)).Count -eq 0 -and
                    (@(Compare-Object $ExpectedStartRoles $TitleStartRoles)).Count -eq 0 -and
                    @($TitleAttractRoles + $TitleStartRoles | Where-Object { $_ -match "trace|capture|log" }).Count -eq 0
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-title-source-provenance"
                    passed = $TitleSourcePassed
                    attract_roles = $TitleAttractRoles
                    start_roles = $TitleStartRoles
                    raw_asset_bytes_persisted = $false
                })

                $StartMenuSource = @($SourceMap.logical_entries | Where-Object { $_.id -eq "menu/start-game" } | Select-Object -First 1)
                $StartMenuRoles = @($StartMenuSource.sources | ForEach-Object { [string]$_.role })
                $ExpectedStartMenuRoles = @(
                    "descriptor", "compressed-screen", "menu-background-palette",
                    "title-background-palette", "title-sprite-palette", "menu-sprite-selectors",
                    "menu-sprite-palette", "nba-emblem", "root-cursor", "character-map",
                    "root-menu-record", "season-menu-record", "music-popup-record",
                    "speed-popup-record", "period-popup-record", "period-values", "screen-loader",
                    "fade-out", "fade-in", "season-transition", "root-input-parameters",
                    "pointer-coordinate-tables"
                )
                $StartMenuFingerprintedRoles = @($StartMenuSource.sources | Where-Object {
                    (($_.PSObject.Properties.Name -contains "fingerprint_fnv1a32") -or
                     ($_.PSObject.Properties.Name -contains "encoded_fingerprint_fnv1a32"))
                })
                $ExpectedStartMenuDescriptorOffset = [uint64]($OpeningFixedStart + (0xDCA1 - 0xC000))
                $ExpectedStartMenuStreamOffset = [uint64]($OpeningPrgStart + (0x8898 - 0x8000))
                $StartMenuDescriptor = @($StartMenuSource.sources | Where-Object { $_.role -eq "descriptor" } | Select-Object -First 1)
                $StartMenuStream = @($StartMenuSource.sources | Where-Object { $_.role -eq "compressed-screen" } | Select-Object -First 1)
                $StartMenuSourcePassed = $StartMenuSource.Count -eq 1 -and
                    $StartMenuSource.schema -eq "tecmo.start-game-menu/TSGM-1" -and
                    $StartMenuSource.input_contract -eq "ines-only" -and
                    (@(Compare-Object $ExpectedStartMenuRoles $StartMenuRoles)).Count -eq 0 -and
                    $StartMenuFingerprintedRoles.Count -eq 22 -and
                    [uint64]$StartMenuDescriptor.source_offset -eq $ExpectedStartMenuDescriptorOffset -and
                    [int]$StartMenuDescriptor.cpu_address -eq 0xDCA1 -and
                    [uint64]$StartMenuStream.source_offset -eq $ExpectedStartMenuStreamOffset -and
                    [int]$StartMenuStream.cpu_address -eq 0x8898 -and
                    [int]$StartMenuStream.encoded_size -eq 220 -and
                    [int]$StartMenuStream.decoded_size -eq 2048 -and
                    [int]$StartMenuSource.native_contract.pages -eq 2 -and
                    [int]$StartMenuSource.native_contract.cells -eq 1920 -and
                    (@($StartMenuSource.native_contract.palette_stage_frames) -join ",") -eq "0,2,4,6,8,20,24,28,32" -and
                    [int]$StartMenuSource.native_contract.root_items -eq 7 -and
                    [int]$StartMenuSource.native_contract.season_items -eq 6 -and
                    [int]$StartMenuSource.native_contract.direction_repeat_frames -eq 8 -and
                    [int]$StartMenuSource.native_contract.season_transition_frames -eq 32 -and
                    [int]$StartMenuSource.native_contract.period_value_count -eq 5 -and
                    @($StartMenuRoles | Where-Object { $_ -match "trace|capture|log|screenshot" }).Count -eq 0
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-start-game-menu-source-provenance"
                    passed = $StartMenuSourcePassed
                    roles = $StartMenuRoles
                    fingerprinted_role_count = $StartMenuFingerprintedRoles.Count
                    input_contract = if ($StartMenuSource.Count -eq 1) { $StartMenuSource.input_contract } else { $null }
                    raw_asset_bytes_persisted = $false
                })

                $BucksSource = @($SourceMap.logical_entries | Where-Object { $_.id -eq "arena/intro/bucks-transition" } | Select-Object -First 1)
                $PassSource = @($SourceMap.logical_entries | Where-Object { $_.id -eq "arena/intro/pass-transition" } | Select-Object -First 1)
                $BucksRoles = @($BucksSource.sources | ForEach-Object { [string]$_.role })
                $PassRoles = @($PassSource.sources | ForEach-Object { [string]$_.role })
                $PostArenaProvenancePassed = $BucksSource.Count -eq 1 -and $PassSource.Count -eq 1 -and
                    $BucksSource.schema -eq "tecmo.intro.bucks/TBUC-1" -and
                    $PassSource.schema -eq "tecmo.intro.pass/TPAS-1" -and
                    $BucksRoles -contains "route" -and $BucksRoles -contains "descriptor" -and
                    $BucksRoles -contains "compressed-screen" -and $BucksRoles -contains "flash-thresholds" -and
                    $BucksRoles -contains "team-name" -and
                    $PassRoles -contains "route" -and $PassRoles -contains "descriptor" -and
                    $PassRoles -contains "compressed-screen" -and $PassRoles -contains "helper-palette" -and
                    $PassRoles -contains "special-palette" -and $PassRoles -contains "player-ball-stream"
                $PassPointerSource = @($PassSource.sources | Where-Object { $_.role -eq "player-sprite-pointer" } | Select-Object -First 1)
                $PassStreamSource = @($PassSource.sources | Where-Object { $_.role -eq "player-ball-stream" } | Select-Object -First 1)
                $PassPrgStart = 16 + [int]$ReferenceHeader.trainer_bytes
                $ExpectedPassPointerOffset = [uint64]($PassPrgStart + (0xA911 - 0x8000))
                $ExpectedPassStreamOffset = [uint64]($PassPrgStart + (0xA9D2 - 0x8000))
                $PostArenaProvenancePassed = $PostArenaProvenancePassed -and
                    $PassPointerSource.Count -eq 1 -and [int]$PassPointerSource.cpu_address -eq 0xA911 -and
                    [int]$PassPointerSource.size -eq 2 -and [uint64]$PassPointerSource.source_offset -eq $ExpectedPassPointerOffset -and
                    $PassStreamSource.Count -eq 1 -and [int]$PassStreamSource.cpu_address -eq 0xA9D2 -and
                    [int]$PassStreamSource.size -eq 41 -and [uint64]$PassStreamSource.source_offset -eq $ExpectedPassStreamOffset
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-bucks-pass-source-provenance"
                    passed = $PostArenaProvenancePassed
                    bucks_schema = if ($BucksSource.Count -eq 1) { $BucksSource.schema } else { $null }
                    pass_schema = if ($PassSource.Count -eq 1) { $PassSource.schema } else { $null }
                    bucks_roles = $BucksRoles
                    pass_roles = $PassRoles
                    raw_asset_bytes_persisted = $false
                })

                $FinaleSource = @($SourceMap.logical_entries | Where-Object { $_.id -eq "intro/finale-sequence" } | Select-Object -First 1)
                $FinaleRoles = @($FinaleSource.sources | ForEach-Object { [string]$_.role })
                $FinaleDescriptors = @($FinaleSource.sources | Where-Object { $_.role -like "screen-descriptor-*" })
                $FinaleStreams = @($FinaleSource.sources | Where-Object { $_.role -like "compressed-screen-*" })
                $FinaleRouteTitle = @($FinaleSource.sources | Where-Object { $_.role -eq "route-title" } | Select-Object -First 1)
                $FinaleSelectorImmediate = @($FinaleSource.sources | Where-Object { $_.role -eq "selector-two-immediate" } | Select-Object -First 1)
                $FinaleSelectorIndexed = @($FinaleSource.sources | Where-Object { $_.role -eq "selector-two-indexed-operands" } | Select-Object -First 1)
                $FinaleSpriteSelectors = @($FinaleSource.sources | Where-Object { $_.role -eq "sprite-chr-selector-writes" } | Select-Object -First 1)
                $FinalePointer = @($FinaleSource.sources | Where-Object { $_.role -eq "sprite-pointer" } | Select-Object -First 1)
                $FinaleStream = @($FinaleSource.sources | Where-Object { $_.role -eq "sprite-stream" } | Select-Object -First 1)
                $FinaleTitleRecord = @($FinaleSource.sources | Where-Object { $_.role -eq "title-character-record" } | Select-Object -First 1)
                $FinaleSplit = @($FinaleSource.sources | Where-Object { $_.role -eq "title-split" } | Select-Object -First 1)
                $ExpectedFinaleDescriptorCpu = @(0xDD49, 0xDD65, 0xDD5E, 0xDD73, 0xDDC0)
                $ActualFinaleDescriptorCpu = @($FinaleDescriptors | ForEach-Object { [int]$_.cpu_address })
                $ExpectedFinaleDispatchOffset = [uint64]($PassPrgStart + 4 * 0x4000 + (0x82CF - 0x8000))
                $ExpectedFinaleTitleOffset = [uint64]($PassPrgStart + 4 * 0x4000 + (0x836B - 0x8000))
                $ExpectedFinaleSplitOffset = [uint64]($PassPrgStart + ($ExpectedPrgBanks - 1) * 0x4000 + (0xFE14 - 0xC000))
                $ExpectedSelectorImmediateOffset = [uint64]($PassPrgStart + 4 * 0x4000 + (0x852F - 0x8000))
                $ExpectedSelectorIndexedCpu = @(0x863E, 0x8640, 0x8642, 0x8644)
                $ExpectedSelectorIndexedOffsets = @($ExpectedSelectorIndexedCpu | ForEach-Object {
                    [uint64]($PassPrgStart + 4 * 0x4000 + ($_ - 0x8000))
                })
                $ExpectedSpriteSelectorCpu = @(0x856A, 0x856E, 0x8572)
                $ExpectedSpriteSelectorOffsets = @($ExpectedSpriteSelectorCpu | ForEach-Object {
                    [uint64]($PassPrgStart + 4 * 0x4000 + ($_ - 0x8000))
                })
                $ActualSelectorIndexedCpu = @($FinaleSelectorIndexed.cpu_addresses | ForEach-Object { [int]$_ })
                $ActualSelectorIndexedOffsets = @($FinaleSelectorIndexed.source_offsets | ForEach-Object { [uint64]$_ })
                $ActualSpriteSelectorCpu = @($FinaleSpriteSelectors.cpu_addresses | ForEach-Object { [int]$_ })
                $ActualSpriteSelectorOffsets = @($FinaleSpriteSelectors.source_offsets | ForEach-Object { [uint64]$_ })
                $ActualSpriteSelectors = @($FinaleSpriteSelectors.selectors | ForEach-Object { [int]$_ })
                $FinaleSourcePassed = $FinaleSource.Count -eq 1 -and
                    $FinaleSource.schema -eq "tecmo.intro.finale/TFIN-1" -and
                    $FinaleRoles -contains "dispatch" -and $FinaleRoles -contains "route-opening" -and
                    $FinaleRoles -contains "route-short-loop" -and $FinaleRoles -contains "route-selector-two" -and
                    $FinaleRoles -contains "route-staged" -and $FinaleRoles -contains "route-title" -and
                    $FinaleRoles -contains "short-anchor-tables" -and
                    $FinaleRoles -contains "selector-two-immediate" -and
                    $FinaleRoles -contains "selector-two-indexed-operands" -and
                    $FinaleRoles -contains "sprite-chr-selector-writes" -and
                    $FinaleRoles -contains "short-staged-sprite-palette" -and
                    $FinaleRoles -contains "selector-two-sprite-palette" -and
                    $FinaleRoles -contains "title-character-record" -and $FinaleRoles -contains "glyph-map" -and
                    $FinaleRoles -contains "glyph-quads" -and $FinaleRoles -contains "title-split" -and
                    $FinaleDescriptors.Count -eq 5 -and $FinaleStreams.Count -eq 5 -and
                    (@(Compare-Object $ExpectedFinaleDescriptorCpu $ActualFinaleDescriptorCpu)).Count -eq 0 -and
                    [uint64]$FinaleSource.sources[0].source_offset -eq $ExpectedFinaleDispatchOffset -and
                    $FinaleRouteTitle.Count -eq 1 -and [int]$FinaleRouteTitle.cpu_address -eq 0x8310 -and
                    $FinaleSelectorImmediate.Count -eq 1 -and
                    [int]$FinaleSelectorImmediate.cpu_address -eq 0x852F -and
                    [int]$FinaleSelectorImmediate.size -eq 1 -and
                    [int]$FinaleSelectorImmediate.selector -eq 2 -and
                    [uint64]$FinaleSelectorImmediate.source_offset -eq $ExpectedSelectorImmediateOffset -and
                    $FinaleSelectorIndexed.Count -eq 1 -and
                    [int]$FinaleSelectorIndexed.selector -eq 2 -and
                    ($ActualSelectorIndexedCpu -join ",") -eq ($ExpectedSelectorIndexedCpu -join ",") -and
                    ($ActualSelectorIndexedOffsets -join ",") -eq ($ExpectedSelectorIndexedOffsets -join ",") -and
                    $FinaleSpriteSelectors.Count -eq 1 -and
                    ($ActualSpriteSelectorCpu -join ",") -eq ($ExpectedSpriteSelectorCpu -join ",") -and
                    ($ActualSpriteSelectorOffsets -join ",") -eq ($ExpectedSpriteSelectorOffsets -join ",") -and
                    ($ActualSpriteSelectors -join ",") -eq (@(0x91, 0x93, 0x95) -join ",") -and
                    $FinalePointer.Count -eq 1 -and [int]$FinalePointer.cpu_address -eq 0xA911 -and
                    [int]$FinalePointer.size -eq 2 -and
                    $FinaleStream.Count -eq 1 -and [int]$FinaleStream.cpu_address -eq 0xA9D2 -and
                    [int]$FinaleStream.size -eq 41 -and
                    $FinaleTitleRecord.Count -eq 1 -and [int]$FinaleTitleRecord.cpu_address -eq 0x836B -and
                    [int]$FinaleTitleRecord.size -eq 26 -and [uint64]$FinaleTitleRecord.source_offset -eq $ExpectedFinaleTitleOffset -and
                    $FinaleSplit.Count -eq 1 -and [int]$FinaleSplit.cpu_address -eq 0xFE14 -and
                    [int]$FinaleSplit.end_cpu_address -eq 0xFE91 -and
                    [int]$FinaleSplit.size -eq 126 -and [uint64]$FinaleSplit.source_offset -eq $ExpectedFinaleSplitOffset -and
                    [int]$FinaleSource.native_contract.screen_layers -eq 5 -and
                    [int]$FinaleSource.native_contract.pages_per_layer -eq 2 -and
                    @($FinaleSource.native_contract.one_page_mirror_layers).Count -eq 2 -and
                    [int]$FinaleSource.native_contract.one_page_source_decoded_bytes -eq 1024 -and
                    [int]$FinaleSource.native_contract.sprite_groups -eq 2 -and
                    [int]$FinaleSource.native_contract.shared_piece_count -eq 10 -and
                    [int]$FinaleSource.native_contract.title_slots -eq 44 -and
                    [int]$FinaleSource.native_contract.title_source_slots -eq 26 -and
                    [int]$FinaleSource.native_contract.blank_slots -eq 18
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-finale-source-provenance"
                    passed = $FinaleSourcePassed
                    schema = if ($FinaleSource.Count -eq 1) { $FinaleSource.schema } else { $null }
                    source_role_count = $FinaleRoles.Count
                    descriptor_count = $FinaleDescriptors.Count
                    stream_count = $FinaleStreams.Count
                    title_route_exact = $FinaleRouteTitle.Count -eq 1 -and [int]$FinaleRouteTitle.cpu_address -eq 0x8310
                    one_page_mirror_contract = $FinaleSource.Count -eq 1 -and
                        @($FinaleSource.native_contract.one_page_mirror_layers).Count -eq 2
                    raw_asset_bytes_persisted = $false
                })

                $ArenaSource = @($SourceMap.logical_entries | Where-Object { $_.id -eq "arena/intro/background-layer" } | Select-Object -First 1)
                $PrgStart = 16 + [int]$ReferenceHeader.trainer_bytes
                $ExpectedRouteOffset = [uint64]($PrgStart + 4 * 0x4000 + (0x88E8 - 0x8000))
                $ExpectedDescriptorOffset = [uint64]($PrgStart + ($ExpectedPrgBanks - 1) * 0x4000 + (0xDD2D - 0xC000))
                $ExpectedLowerR0Offset = [uint64]($PrgStart + ($ExpectedPrgBanks - 1) * 0x4000 + (0xFD7C - 0xC000))
                $ExpectedLowerR1Offset = [uint64]($PrgStart + ($ExpectedPrgBanks - 1) * 0x4000 + (0xFD80 - 0xC000))
                $ExpectedStreamOffset = if ($ArenaSource.Count -eq 1) {
                    [uint64]($PrgStart + [int]$ArenaSource.stream.bank * 0x4000 + ([int]$ArenaSource.stream.cpu_address - 0x8000))
                } else { 0 }
                $ExpectedPaletteOffset = if ($ArenaSource.Count -eq 1) {
                    [uint64]($PrgStart + [int]$ArenaSource.palette.bank * 0x4000 + ([int]$ArenaSource.palette.cpu_address - 0x8000))
                } else { 0 }
                $ArenaBasicProvenancePassed = $ArenaSource.Count -eq 1 -and
                    $ArenaSource.schema -eq "tecmo.arena-intro.background-layer/TATL-1" -and
                    [int]$ArenaSource.screen_id -eq 24 -and
                    [int]$ArenaSource.decoder_cpu_address -eq 0xD9F6 -and
                    [int]$ArenaSource.route.bank -eq 4 -and [int]$ArenaSource.route.cpu_address -eq 0x88E8 -and
                    [uint64]$ArenaSource.route.source_offset -eq $ExpectedRouteOffset -and
                    [int]$ArenaSource.descriptor.bank -eq ($ExpectedPrgBanks - 1) -and
                    [int]$ArenaSource.descriptor.cpu_address -eq 0xDD2D -and
                    [uint64]$ArenaSource.descriptor.source_offset -eq $ExpectedDescriptorOffset -and
                    [uint64]$ArenaSource.stream.source_offset -eq $ExpectedStreamOffset -and
                    [int]$ArenaSource.stream.encoded_size -gt 0 -and [int]$ArenaSource.stream.decoded_size -eq 2048 -and
                    [int]$ArenaSource.palette.size -eq 16 -and
                    [uint64]$ArenaSource.palette.source_offset -eq $ExpectedPaletteOffset -and
                    @($ArenaSource.lower_chr_tables).Count -eq 2 -and
                    [int]$ArenaSource.lower_chr_tables[0].selector_cpu_address -eq 0xFD7D -and
                    [uint64]$ArenaSource.lower_chr_tables[0].source_offset -eq $ExpectedLowerR0Offset -and
                    [int]$ArenaSource.lower_chr_tables[1].selector_cpu_address -eq 0xFD81 -and
                    [uint64]$ArenaSource.lower_chr_tables[1].source_offset -eq $ExpectedLowerR1Offset
                $KnownReferenceProvenanceMatches = $RomSha256 -ne $KnownReferenceRomSha256 -or
                    ([int]$ArenaSource.stream.bank -eq 0 -and
                     [int]$ArenaSource.stream.cpu_address -eq 0xA2ED -and
                     [int]$ArenaSource.stream.encoded_size -eq 1247 -and
                     [int]$ArenaSource.palette.bank -eq 0 -and
                     [int]$ArenaSource.palette.cpu_address -eq 0xA7CB)
                $ArenaProvenancePassed = $ArenaBasicProvenancePassed -and $KnownReferenceProvenanceMatches
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-arena-source-provenance"
                    passed = $ArenaProvenancePassed
                    schema_valid = $ArenaSource.Count -eq 1 -and $ArenaSource.schema -eq "tecmo.arena-intro.background-layer/TATL-1"
                    descriptor_valid = $ArenaSource.Count -eq 1 -and [int]$ArenaSource.descriptor.cpu_address -eq 0xDD2D
                    stream_valid = $ArenaSource.Count -eq 1 -and [int]$ArenaSource.stream.encoded_size -gt 0 -and [int]$ArenaSource.stream.decoded_size -eq 2048
                    irq_selector_tables_valid = $ArenaSource.Count -eq 1 -and @($ArenaSource.lower_chr_tables).Count -eq 2
                    known_reference_provenance_match = $KnownReferenceProvenanceMatches
                    raw_asset_bytes_persisted = $false
                })

                $SpriteSource = @($SourceMap.logical_entries | Where-Object { $_.id -eq "arena/intro/sprite-groups" } | Select-Object -First 1)
                $ChrStart = [uint64]($PrgStart + $ExpectedPrgBanks * 0x4000)
                $ExpectedSpritePaletteOffset = [uint64]($PrgStart + 4 * 0x4000 + (0x89DD - 0x8000))
                $ExpectedPointerTableOffset = [uint64]($PrgStart + (0xA7DB - 0x8000))
                $ExpectedSeedsOffset = [uint64]($PrgStart + 4 * 0x4000 + (0x8984 - 0x8000))
                $ExpectedEmitterOffset = [uint64]($PrgStart + 4 * 0x4000 + (0x8988 - 0x8000))
                $ExpectedParamsOffset = [uint64]($PrgStart + 4 * 0x4000 + (0x89BD - 0x8000))
                $SeedBytes = Read-FileBytesAtOffset -Path $ReferenceRom -Offset $ExpectedSeedsOffset -Count 4
                $ParamBytes = Read-FileBytesAtOffset -Path $ReferenceRom -Offset $ExpectedParamsOffset -Count 4
                $Bank04AnchorContractsValid =
                    $SeedBytes[0] -eq 0x00 -and $SeedBytes[1] -eq 0x1E -and
                    $SeedBytes[2] -eq 0x00 -and $SeedBytes[3] -eq 0x01 -and
                    $ParamBytes[0] -eq 0x00 -and $ParamBytes[1] -eq 0xB8 -and
                    $ParamBytes[2] -eq 0x00 -and $ParamBytes[3] -eq 0x01
                $SpriteChrPagesValid = $SpriteSource.Count -eq 1 -and
                    @($SpriteSource.chr_pages).Count -eq 2 -and
                    [int]$SpriteSource.chr_pages[0].size -eq 1024 -and
                    [int]$SpriteSource.chr_pages[0].mapper_register -eq 2 -and
                    [int]$SpriteSource.chr_pages[0].selector -eq 8 -and
                    [int]$SpriteSource.chr_pages[0].chr_offset -eq 8192 -and
                    [uint64]$SpriteSource.chr_pages[0].source_offset -eq ($ChrStart + 8192) -and
                    [int]$SpriteSource.chr_pages[1].size -eq 1024 -and
                    [int]$SpriteSource.chr_pages[1].mapper_register -eq 3 -and
                    [int]$SpriteSource.chr_pages[1].selector -eq 9 -and
                    [int]$SpriteSource.chr_pages[1].chr_offset -eq 9216 -and
                    [uint64]$SpriteSource.chr_pages[1].source_offset -eq ($ChrStart + 9216)
                $SpriteProvenancePassed = $SpriteSource.Count -eq 1 -and
                    $SpriteSource.schema -eq "tecmo.arena-intro.sprite-groups/TASG-2" -and
                    [int]$SpriteSource.palette.bank -eq 4 -and [int]$SpriteSource.palette.cpu_address -eq 0x89DD -and
                    [int]$SpriteSource.palette.size -eq 16 -and [uint64]$SpriteSource.palette.source_offset -eq $ExpectedSpritePaletteOffset -and
                    [int]$SpriteSource.pointer_table.bank -eq 0 -and [int]$SpriteSource.pointer_table.cpu_address -eq 0xA7DB -and
                    [int]$SpriteSource.pointer_table.size -eq 4 -and [uint64]$SpriteSource.pointer_table.source_offset -eq $ExpectedPointerTableOffset -and
                    @($SpriteSource.streams).Count -eq 2 -and
                    [int]$SpriteSource.streams[0].selector -eq 0 -and $SpriteSource.streams[0].kind -eq "jumbotron" -and
                    [int]$SpriteSource.streams[0].record_count -eq 55 -and [int]$SpriteSource.streams[0].size -eq 221 -and
                    [uint64]$SpriteSource.streams[0].source_offset -eq [uint64]($PrgStart + [int]$SpriteSource.streams[0].cpu_address - 0x8000) -and
                    [int]$SpriteSource.streams[1].selector -eq 1 -and $SpriteSource.streams[1].kind -eq "goal" -and
                    [int]$SpriteSource.streams[1].record_count -eq 16 -and [int]$SpriteSource.streams[1].size -eq 65 -and
                    [uint64]$SpriteSource.streams[1].source_offset -eq [uint64]($PrgStart + [int]$SpriteSource.streams[1].cpu_address - 0x8000) -and
                    [int]$SpriteSource.bank04.seeds.cpu_address -eq 0x8984 -and [int]$SpriteSource.bank04.seeds.size -eq 4 -and
                    [uint64]$SpriteSource.bank04.seeds.source_offset -eq $ExpectedSeedsOffset -and
                    [int]$SpriteSource.bank04.emitter.cpu_address -eq 0x8988 -and [int]$SpriteSource.bank04.emitter.size -eq 53 -and
                    [uint64]$SpriteSource.bank04.emitter.source_offset -eq $ExpectedEmitterOffset -and
                    [int]$SpriteSource.bank04.params.cpu_address -eq 0x89BD -and [int]$SpriteSource.bank04.params.size -eq 4 -and
                    [uint64]$SpriteSource.bank04.params.source_offset -eq $ExpectedParamsOffset -and
                    $Bank04AnchorContractsValid -and
                    [int]$SpriteSource.mapper.r2 -eq 8 -and [int]$SpriteSource.mapper.r3 -eq 9 -and
                    $SpriteChrPagesValid
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-arena-sprite-provenance"
                    passed = $SpriteProvenancePassed
                    schema_valid = $SpriteSource.Count -eq 1 -and $SpriteSource.schema -eq "tecmo.arena-intro.sprite-groups/TASG-2"
                    streams_valid = $SpriteSource.Count -eq 1 -and @($SpriteSource.streams).Count -eq 2
                    bank04_regions_valid = $SpriteSource.Count -eq 1 -and [int]$SpriteSource.bank04.emitter.cpu_address -eq 0x8988
                    bank04_anchor_contracts_valid = $Bank04AnchorContractsValid
                    chr_pages_valid = $SpriteChrPagesValid
                    raw_asset_bytes_persisted = $false
                })
            } catch {
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-source-map-logical-entries"
                    passed = $false
                    error = "system/source-map logical entry inspection failed"
                    error_type = Get-SafeExceptionName $_
                    raw_asset_bytes_persisted = $false
                })
            }

            try {
                $ListOutput = & $ExePath --assetpack-list $AssetPackPath 2>&1
                $ListExitCode = $LASTEXITCODE
                $ListText = (@($ListOutput) | ForEach-Object { [string]$_ }) -join "`n"
                $MissingFromList = @($ExpectedEntries | Where-Object { $ListText -notmatch [regex]::Escape($_) })
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-list-verification"
                    passed = $ListExitCode -eq 0 -and $MissingFromList.Count -eq 0
                    exit_code = $ListExitCode
                    expected_entry_count = $ExpectedEntries.Count
                    missing_entries = $MissingFromList
                    raw_output_persisted = $false
                })
            } catch {
                Add-TestResult ([pscustomobject]@{
                    id = "assetpack-list-verification"
                    passed = $false
                    error = "--assetpack-list verification failed"
                    error_type = Get-SafeExceptionName $_
                    raw_output_persisted = $false
                })
            }
        } else {
            Add-TestResult ([pscustomobject]@{
                id = "assetpack-bank-completeness"
                passed = $false
                error = "asset pack builder did not report PRG/CHR bank counts"
                raw_asset_bytes_persisted = $false
            })
            Add-TestResult ([pscustomobject]@{
                id = "assetpack-logical-entries"
                passed = $false
                error = "asset pack builder did not report PRG/CHR bank counts"
                raw_asset_bytes_persisted = $false
            })
        }
    } else {
        Add-TestResult ([pscustomobject]@{
            id = "assetpack-directory-count"
            passed = $false
            error = "asset pack directory was not parsed"
            raw_asset_bytes_persisted = $false
        })
        Add-TestResult ([pscustomobject]@{
            id = "assetpack-bank-completeness"
            passed = $false
            error = "asset pack directory was not parsed"
            raw_asset_bytes_persisted = $false
        })
        Add-TestResult ([pscustomobject]@{
            id = "assetpack-logical-entries"
            passed = $false
            error = "asset pack directory was not parsed"
            raw_asset_bytes_persisted = $false
        })
    }

    $ChrAllReadable = $PackCreated -and
        $DirectoryCountPassed -and
        $BankCompletenessPassed -and
        $LogicalEntriesPassed -and
        $Directory -and
        $ChrAllValid

    Add-TestResult ([pscustomobject]@{
        id = "chr-all-entry-readable"
        passed = $ChrAllReadable
        asset_pack_entry_validated = $ChrAllValid
        canonical_fallback_preserved = $CanonicalFallbackPreserved
        rom_only_contract = $true
        raw_output_persisted = $false
        error = if ($ChrAllReadable) { $null } else { "chr/all entry was missing or failed size/bounds validation" }
    })
} catch {
    Add-TestResult ([pscustomobject]@{
        id = "assetpack-runner-error"
        passed = $false
        raw_output_persisted = $false
        private_paths_included = $false
        error = "asset-pack test runner failed before completing all checks"
        error_type = Get-SafeExceptionName $_
    })
} finally {
    Write-AssetPackReport
}

$Results | Format-Table -AutoSize
Write-Host "Wrote asset-pack test report: $(ConvertTo-RepoRelativePath $ReportPath)"

if ($Failures -ne 0) {
    throw "$Failures asset-pack test(s) failed."
}

Write-Host "All asset-pack tests passed."
