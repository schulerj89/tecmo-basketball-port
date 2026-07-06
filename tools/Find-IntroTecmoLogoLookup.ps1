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
    $OutputPath = Join-Path $ProjectRoot "build\intro_tecmo_logo_lookup.json"
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

function Read-StreamSummary {
    param(
        [string]$Name,
        [string]$CallSite,
        [int]$PointerTable,
        [int[]]$Selectors
    )

    $Streams = New-Object System.Collections.Generic.List[object]
    foreach ($Selector in $Selectors) {
        $StreamPointer = Get-PrgWord 0 ($PointerTable + $Selector * 2)
        $RecordCount = Get-PrgByte 0 $StreamPointer
        $Hits = New-Object System.Collections.Generic.List[object]

        for ($RecordIndex = 0; $RecordIndex -lt $RecordCount; ++$RecordIndex) {
            $RecordAddress = $StreamPointer + 1 + $RecordIndex * 4
            $RawY = Get-PrgByte 0 $RecordAddress
            $RawTile = Get-PrgByte 0 ($RecordAddress + 1)
            $RawAttributes = Get-PrgByte 0 ($RecordAddress + 2)
            $RawX = Get-PrgByte 0 ($RecordAddress + 3)
            $OamTileLow = ($RawTile + 1) -band 0xFF
            if ($OamTileLow -ge 0x80 -and $OamTileLow -le 0x93) {
                $PairTop = $OamTileLow -band 0xFE
                $PairBottom = $PairTop -bor 0x01
                $Hits.Add([pscustomobject][ordered]@{
                    record_index = $RecordIndex
                    stream_record_address = ("bank00:{0}" -f (Format-HexWord $RecordAddress))
                    oam_tile_low_hex = (Format-HexByte $OamTileLow)
                    table1_pair_top_hex = (Format-HexWord (0x100 + $PairTop))
                    table1_pair_bottom_hex = (Format-HexWord (0x100 + $PairBottom))
                    attributes_hex = (Format-HexByte $RawAttributes)
                    relative_x = (Convert-SignedByte $RawX)
                    relative_y = (Convert-SignedByte $RawY)
                })
            }
        }

        $Streams.Add([pscustomobject][ordered]@{
            selector = $Selector
            pointer_table_entry = ("bank00:{0}" -f (Format-HexWord ($PointerTable + $Selector * 2)))
            stream_pointer = ("bank00:{0}" -f (Format-HexWord $StreamPointer))
            record_count = $RecordCount
            tecmo_logo_hit_count = $Hits.Count
            tecmo_logo_hits = $Hits
        })
    }

    return [pscustomobject][ordered]@{
        name = $Name
        call_site = $CallSite
        pointer_table = ("bank00:{0}" -f (Format-HexWord $PointerTable))
        selectors = $Streams
    }
}

$A7DbProbe = Read-StreamSummary "rabbit_reference_table" "bank04:L8988" 0xA7DB @(0, 1, 2, 3)
$A90fProbe = Read-StreamSummary "tecmo_logo_candidate_table" "bank04:L8818/L8645" 0xA90F @(0, 1, 2, 3, 4)
$A90fSelector0 = @($A90fProbe.selectors | Where-Object { $_.selector -eq 0 })[0]
$AllPairs = @(
    $A90fSelector0.tecmo_logo_hits |
        ForEach-Object { $_.table1_pair_top_hex; $_.table1_pair_bottom_hex } |
        Sort-Object -Unique
)

$Report = [ordered]@{
    schema_version = 1
    generated_at_utc = [DateTime]::UtcNow.ToString("o")
    data_policy = "Local-only intro TECMO logo lookup report. Do not commit this generated file; it may contain decoded tile IDs and positions from private data."
    source_image = "<local private decomp build>"
    model = [ordered]@{
        helper = "C051 -> D861"
        record_shape = 'count byte, then y, tile+$0D, attributes, x'
        target_oam_tile_low_range = "80-93"
        interpreted_chr_bank = 31
        interpreted_chr_table = 1
    }
    regression_summary = [ordered]@{
        a7db_selectors_0_to_3_hit_count = @($A7DbProbe.selectors | ForEach-Object { $_.tecmo_logo_hit_count } | Measure-Object -Sum).Sum
        a90f_selector_00_hit_count = $A90fSelector0.tecmo_logo_hit_count
        a90f_selector_00_expected_hit_count = 10
        a90f_selector_00_covers_table1_tiles = $AllPairs
    }
    probes = @($A7DbProbe, $A90fProbe)
}

$OutputDir = Split-Path -Parent $OutputPath
if ($OutputDir) {
    New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
}
$Report | ConvertTo-Json -Depth 12 | Set-Content -Path $OutputPath -Encoding ASCII
Write-Host "Wrote intro TECMO logo lookup report: $OutputPath"
