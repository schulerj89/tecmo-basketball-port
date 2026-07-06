param(
    [string]$ProjectRoot,
    [string]$DecompRoot,
    [string]$OutputPath
)

$ErrorActionPreference = "Stop"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path $ProjectRoot).Path

if (!$OutputPath) {
    $OutputPath = Join-Path $ProjectRoot "build\intro_rabbit_lookup.json"
}
if (![System.IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath = Join-Path $ProjectRoot $OutputPath
}
$OutputPath = [System.IO.Path]::GetFullPath($OutputPath)

function Find-LocalDecompRoot {
    if ($DecompRoot -and (Test-Path $DecompRoot)) {
        return (Resolve-Path $DecompRoot).Path
    }
    if ($env:TECMO_DECOMP_ROOT -and (Test-Path $env:TECMO_DECOMP_ROOT)) {
        return (Resolve-Path $env:TECMO_DECOMP_ROOT).Path
    }

    $ProjectsRoot = Split-Path -Parent $ProjectRoot
    $Candidates = @(
        (Join-Path $ProjectsRoot "disassem\tecmo-basketball-decompilation"),
        (Join-Path $ProjectsRoot "tecmo-basketball-decompilation")
    )
    foreach ($Candidate in $Candidates) {
        if (Test-Path $Candidate) {
            return (Resolve-Path $Candidate).Path
        }
    }

    return $null
}

function Format-HexByte {
    param([int]$Value)
    return ("{0:X2}" -f ($Value -band 0xFF))
}

function Format-HexWord {
    param([int]$Value)
    return ("{0:X4}" -f ($Value -band 0xFFFF))
}

function Convert-SignedByte {
    param([int]$Value)
    $Byte = $Value -band 0xFF
    if ($Byte -ge 0x80) {
        return $Byte - 0x100
    }
    return $Byte
}

$LocalDecompRoot = Find-LocalDecompRoot
if (!$LocalDecompRoot) {
    throw "Could not find a local decomp root. Pass -DecompRoot or set TECMO_DECOMP_ROOT."
}

$RomCandidates = @(
    (Join-Path $LocalDecompRoot "build\out\tecmo_basketball_disassem.nes"),
    (Join-Path $LocalDecompRoot "build\out\tecmo_basketball_split_compile.nes"),
    (Join-Path $LocalDecompRoot "build\split_compile\build\out\tecmo_basketball_split_compile.nes")
)
$RomPath = $null
foreach ($Candidate in $RomCandidates) {
    if (Test-Path $Candidate) {
        $RomPath = (Resolve-Path $Candidate).Path
        break
    }
}
if (!$RomPath) {
    throw "Could not find a locally built NES image under the private decomp root."
}

$Rom = [System.IO.File]::ReadAllBytes($RomPath)
if ($Rom.Length -lt 16 -or $Rom[0] -ne 0x4E -or $Rom[1] -ne 0x45 -or $Rom[2] -ne 0x53 -or $Rom[3] -ne 0x1A) {
    throw "Local image is not an iNES file: $RomPath"
}

$PrgBankCount = [int]$Rom[4]
$TrainerBytes = if (($Rom[6] -band 0x04) -ne 0) { 512 } else { 0 }
$PrgStart = 16 + $TrainerBytes
$PrgBankSize = 0x4000
if ($PrgBankCount -lt 8) {
    throw "Expected at least 8 PRG banks; found $PrgBankCount."
}
if ($Rom.Length -lt ($PrgStart + $PrgBankCount * $PrgBankSize)) {
    throw "ROM is too short for its declared PRG bank count."
}

function Get-PrgByte {
    param(
        [int]$Bank,
        [int]$CpuAddress
    )

    if ($Bank -lt 0 -or $Bank -ge $PrgBankCount) {
        throw "PRG bank out of range: $Bank"
    }
    if ($CpuAddress -lt 0x8000 -or $CpuAddress -gt 0xBFFF) {
        throw ("CPU address must be in switchable PRG range 8000-BFFF: {0}" -f (Format-HexWord $CpuAddress))
    }

    return [int]$Rom[$PrgStart + $Bank * $PrgBankSize + ($CpuAddress - 0x8000)]
}

function Get-PrgWord {
    param(
        [int]$Bank,
        [int]$CpuAddress
    )

    return (Get-PrgByte $Bank $CpuAddress) -bor ((Get-PrgByte $Bank ($CpuAddress + 1)) -shl 8)
}

function Get-FixedByte {
    param([int]$CpuAddress)

    if ($CpuAddress -lt 0xC000 -or $CpuAddress -gt 0xFFFF) {
        throw ("CPU address must be in fixed PRG range C000-FFFF: {0}" -f (Format-HexWord $CpuAddress))
    }
    return [int]$Rom[$PrgStart + ($PrgBankCount - 1) * $PrgBankSize + ($CpuAddress - 0xC000)]
}

function Get-FixedWord {
    param([int]$CpuAddress)
    return (Get-FixedByte $CpuAddress) -bor ((Get-FixedByte ($CpuAddress + 1)) -shl 8)
}

function Read-FixedJmpTarget {
    param([int]$CpuAddress)

    $Opcode = Get-FixedByte $CpuAddress
    if ($Opcode -ne 0x4C) {
        return $null
    }
    return Get-FixedWord ($CpuAddress + 1)
}

$Bank04 = 4
$PointerBank = 0
$PointerTableAddress = 0xA7DB
$TileOffset = 1
$AssumedChrTable = 1

$SelectorBytes = @()
foreach ($Address in 0x89BD..0x89C0) {
    $SelectorBytes += Get-PrgByte $Bank04 $Address
}
$SeedBytes = @()
foreach ($Address in 0x8984..0x8987) {
    $SeedBytes += Get-PrgByte $Bank04 $Address
}

$Passes = New-Object System.Collections.Generic.List[object]
foreach ($PassX in @(1, 0)) {
    $PointerIndex = $SelectorBytes[2 + $PassX]
    $BaseX = $SelectorBytes[$PassX]
    $BaseY = $SeedBytes[$PassX]
    $BaseYPage = $SeedBytes[2 + $PassX]
    $StreamPointer = Get-PrgWord $PointerBank ($PointerTableAddress + $PointerIndex * 2)
    $RecordCount = Get-PrgByte $PointerBank $StreamPointer
    $Records = New-Object System.Collections.Generic.List[object]
    $CandidateRecords = New-Object System.Collections.Generic.List[object]

    for ($RecordIndex = 0; $RecordIndex -lt $RecordCount; ++$RecordIndex) {
        $RecordAddress = $StreamPointer + 1 + $RecordIndex * 4
        $RawY = Get-PrgByte $PointerBank $RecordAddress
        $RawTile = Get-PrgByte $PointerBank ($RecordAddress + 1)
        $RawAttributes = Get-PrgByte $PointerBank ($RecordAddress + 2)
        $RawX = Get-PrgByte $PointerBank ($RecordAddress + 3)
        $TileLow = ($RawTile + $TileOffset) -band 0xFF
        $FullTile = $AssumedChrTable * 0x100 + $TileLow
        $RelativeX = Convert-SignedByte $RawX
        $RelativeY = Convert-SignedByte $RawY
        $ScreenX = $BaseX + $RelativeX
        $ScreenY = $BaseY + $RelativeY
        $Record = [ordered]@{
            record_index = $RecordIndex
            stream_record_address = ("bank00:{0}" -f (Format-HexWord $RecordAddress))
            tile_low_after_offset_hex = (Format-HexByte $TileLow)
            full_tile_if_table1_hex = (Format-HexWord $FullTile)
            attributes_hex = (Format-HexByte $RawAttributes)
            relative_x = $RelativeX
            relative_y = $RelativeY
            screen_x_from_initial_seed = $ScreenX
            screen_y_from_initial_seed = $ScreenY
        }
        $Records.Add([pscustomobject]$Record)
        if ($TileLow -ge 0x25 -and $TileLow -le 0x2B) {
            $CandidateRecords.Add([pscustomobject]$Record)
        }
    }

    $Passes.Add([pscustomobject][ordered]@{
        pass_x = $PassX
        pointer_index = $PointerIndex
        pointer_table_entry = ("bank00:{0}" -f (Format-HexWord ($PointerTableAddress + $PointerIndex * 2)))
        stream_pointer = ("bank00:{0}" -f (Format-HexWord $StreamPointer))
        record_count = $RecordCount
        base_x = $BaseX
        base_y = $BaseY
        base_y_page = $BaseYPage
        records = $Records
        candidate_records_25_to_2b = $CandidateRecords
    })
}

$AllCandidateTiles = @(
    $Passes |
        ForEach-Object { $_.candidate_records_25_to_2b } |
        ForEach-Object { $_.full_tile_if_table1_hex } |
        Sort-Object -Unique
)

$Report = [ordered]@{
    schema_version = 1
    generated_at_utc = [DateTime]::UtcNow.ToString("o")
    data_policy = "Local-only intro lookup report. Do not commit this generated file; it may contain decoded tile IDs from private data."
    source_image = "<local private decomp build>"
    fixed_helpers = [ordered]@{
        c051_target = if ($null -ne (Read-FixedJmpTarget 0xC051)) { Format-HexWord (Read-FixedJmpTarget 0xC051) } else { $null }
        c05a_target = if ($null -ne (Read-FixedJmpTarget 0xC05A)) { Format-HexWord (Read-FixedJmpTarget 0xC05A) } else { $null }
        c051_record_model = 'D861 reads a pointer table entry selected by $2E, then stages 4-byte sprite records as y, tile+$0D, attributes, x.'
    }
    bank04_selector = [ordered]@{
        driver = "bank04:L88E7 -> L8988 -> fixed C051"
        stream_selector_bytes = "bank04:89BD-89C0"
        seed_bytes = "bank04:8984-8987"
        pointer_table = "bank00:A7DB"
        pass_order = "X=1 then X=0"
    }
    assumed_render_context = [ordered]@{
        chr_bank = 31
        chr_table = $AssumedChrTable
        tile_offset_from_d861 = $TileOffset
        note = "Full tile IDs are interpreted for Intro Lab table 1. Final NES sprite-size/palette behavior still needs validation."
    }
    candidate_tiles_table1 = $AllCandidateTiles
    passes = $Passes
}

$OutputDir = Split-Path -Parent $OutputPath
if ($OutputDir) {
    New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
}
$Report | ConvertTo-Json -Depth 12 | Set-Content -Path $OutputPath -Encoding ASCII
Write-Host "Wrote intro rabbit lookup report: $OutputPath"
