param(
    [string]$ProjectRoot,
    [string]$DecompRoot,
    [string]$ReportPath
)

$ErrorActionPreference = "Stop"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path $ProjectRoot).Path

if (!$ReportPath) {
    $ReportPath = Join-Path $ProjectRoot "build\title_setup_mapping.json"
}

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

function Read-BankedByteMap {
    param(
        [string]$Path,
        [int]$BankIndex,
        [int]$CpuBase = 0x8000
    )

    $Map = @{}
    $RomBase = $BankIndex * 0x4000

    foreach ($Line in Get-Content $Path) {
        $Parts = $Line.Split(';', 2)
        if ($Parts.Count -lt 2) {
            continue
        }

        $AddressMatch = [regex]::Match($Parts[1], '\$([0-9a-fA-F]{4,6})')
        if (!$AddressMatch.Success) {
            continue
        }

        $Values = @(
            foreach ($Match in [regex]::Matches($Parts[0], '\$([0-9a-fA-F]{2})(?![0-9a-fA-F])')) {
                [int][Convert]::ToInt32($Match.Groups[1].Value, 16)
            }
        )
        if ($Values.Count -eq 0) {
            continue
        }

        $RomAddress = [Convert]::ToInt32($AddressMatch.Groups[1].Value, 16)
        for ($Index = 0; $Index -lt $Values.Count; ++$Index) {
            $CpuAddress = $CpuBase + (($RomAddress + $Index) - $RomBase)
            if ($CpuAddress -ge $CpuBase -and $CpuAddress -lt ($CpuBase + 0x4000)) {
                $Map[$CpuAddress] = $Values[$Index]
            }
        }
    }

    return $Map
}

function Get-MapByte {
    param(
        [hashtable]$Map,
        [int]$Address
    )

    if (!$Map.ContainsKey($Address)) {
        throw ("Missing Bank 04 byte at {0}" -f (Format-HexWord $Address))
    }
    return [int]$Map[$Address]
}

function Format-HexByte {
    param([int]$Value)
    return ("0x{0:X2}" -f ($Value -band 0xFF))
}

function Format-HexWord {
    param([int]$Value)
    return ("0x{0:X4}" -f ($Value -band 0xFFFF))
}

function Count-CallTargets {
    param(
        [hashtable]$Map,
        [int]$Start,
        [int]$End
    )

    $Calls = @{}
    for ($Address = $Start; $Address -le $End - 2; ++$Address) {
        if ((Get-MapByte -Map $Map -Address $Address) -ne 0x20) {
            continue
        }
        $Lo = Get-MapByte -Map $Map -Address ($Address + 1)
        $Hi = Get-MapByte -Map $Map -Address ($Address + 2)
        $Target = ($Hi -shl 8) -bor $Lo
        $Key = Format-HexWord $Target
        if (!$Calls.ContainsKey($Key)) {
            $Calls[$Key] = 0
        }
        ++$Calls[$Key]
    }

    return @(
        foreach ($Key in ($Calls.Keys | Sort-Object)) {
            [pscustomobject]@{
                target = $Key
                count = $Calls[$Key]
                role = Get-KnownHelperRole -Target $Key
            }
        }
    )
}

function Count-WriteTargets {
    param(
        [hashtable]$Map,
        [int]$Start,
        [int]$End
    )

    $Writes = @{}
    for ($Address = $Start; $Address -le $End - 2; ++$Address) {
        $Opcode = Get-MapByte -Map $Map -Address $Address
        $Target = $null
        if ($Opcode -eq 0x85) {
            $Target = Get-MapByte -Map $Map -Address ($Address + 1)
        } elseif ($Opcode -eq 0x8D -and $Address -le $End - 2) {
            $Lo = Get-MapByte -Map $Map -Address ($Address + 1)
            $Hi = Get-MapByte -Map $Map -Address ($Address + 2)
            $Target = ($Hi -shl 8) -bor $Lo
        }
        if ($null -ne $Target) {
            $Key = Format-HexWord $Target
            if (!$Writes.ContainsKey($Key)) {
                $Writes[$Key] = 0
            }
            ++$Writes[$Key]
        }
    }

    return @(
        foreach ($Key in ($Writes.Keys | Sort-Object)) {
            [pscustomobject]@{
                target = $Key
                count = $Writes[$Key]
                note = Get-KnownWriteRole -Target $Key
            }
        }
    )
}

function Get-KnownHelperRole {
    param([string]$Target)

    switch ($Target) {
        "0xBA95" { return "local pointer seed helper" }
        "0xBAA4" { return "local stream copy/staging helper" }
        "0xC000" { return "fixed frame wait/helper" }
        "0xC009" { return "fixed setup/finalization helper" }
        "0xC054" { return "fixed return/finalize helper" }
        "0xC05A" { return "fixed UI pointer/staging seed helper" }
        "0xC06F" { return "fixed staging/finalization helper" }
        default { return "unclassified call target" }
    }
}

function Get-KnownWriteRole {
    param([string]$Target)

    switch ($Target) {
        "0x0002" { return "PPU/control-style scratch byte used before fixed helpers" }
        "0x0004" { return "copy/staging completion gate" }
        "0x0057" { return "local pointer seed byte" }
        "0x0058" { return "local pointer seed byte" }
        "0x0059" { return "local pointer seed byte" }
        "0x005A" { return "local pointer seed byte" }
        "0x005B" { return "source pointer low byte" }
        "0x005C" { return "source pointer high byte" }
        "0x0084" { return "destination pointer low byte" }
        "0x0085" { return "destination pointer high byte" }
        "0x0086" { return "stream pointer low byte" }
        "0x0087" { return "stream pointer high byte" }
        "0x0088" { return "stream x/offset scratch" }
        "0x0089" { return "stream y/offset scratch" }
        "0x00CC" { return "PPU/staging pointer low byte" }
        "0x00CD" { return "PPU/staging pointer high byte" }
        "0x00CE" { return "copy dimensions/stride scratch" }
        "0x00CF" { return "copy dimensions/stride scratch" }
        "0x058D" { return "stream length/control byte mirror" }
        "0x06E2" { return "bank/config selector for staged copy" }
        default { return "unclassified write target" }
    }
}

function New-TableRef {
    param(
        [string]$Name,
        [int]$Start,
        [int]$Count,
        [string]$Role
    )

    return [pscustomobject]@{
        name = $Name
        start = Format-HexWord $Start
        count = $Count
        end_exclusive = Format-HexWord ($Start + $Count)
        role = $Role
        verified_addresses_present = $true
    }
}

$LocalDecompRoot = Find-LocalDecompRoot
if (!$LocalDecompRoot) {
    throw "Could not find a local decomp root. Pass -DecompRoot or set TECMO_DECOMP_ROOT."
}

$Bank04Path = Join-Path $LocalDecompRoot "build\baseline\Tecmo_04.asm"
if (!(Test-Path $Bank04Path)) {
    throw "Required local file not found: $Bank04Path"
}

$Bank04 = Read-BankedByteMap -Path $Bank04Path -BankIndex 4 -CpuBase 0x8000
$TableRefs = @(
    (New-TableRef -Name "selector_remap_ba88" -Start 0xBA88 -Count 13 -Role "selector remap used by adjacent setup driver"),
    (New-TableRef -Name "target_ptr_lo_b317" -Start 0xB317 -Count 5 -Role "low-byte pointer table selected through selector_remap_ba88"),
    (New-TableRef -Name "target_ptr_hi_b31c" -Start 0xB31C -Count 5 -Role "high-byte pointer table selected through selector_remap_ba88"),
    (New-TableRef -Name "stream_ptr_lo_b33f" -Start 0xB33F -Count 15 -Role "low-byte stream pointer table used by local stream copy helper"),
    (New-TableRef -Name "stream_ptr_hi_b34e" -Start 0xB34E -Count 15 -Role "high-byte stream pointer table used by local stream copy helper")
)

foreach ($Ref in $TableRefs) {
    for ($Address = [Convert]::ToInt32($Ref.start.Substring(2), 16); $Address -lt [Convert]::ToInt32($Ref.end_exclusive.Substring(2), 16); ++$Address) {
        if (!$Bank04.ContainsKey($Address)) {
            $Ref.verified_addresses_present = $false
            break
        }
    }
}

$Report = [pscustomobject]@{
    generated_at_utc = [DateTime]::UtcNow.ToString("o")
    target = "Bank 04 title setup and adjacent pattern/palette setup mapping"
    decomp_root_used = "<local>"
    safety = [pscustomobject]@{
        generated_report_contains = "ranges, helper targets, write targets, table counts, and implementation notes only"
        forbidden_from_commit = "this JSON report, ASM text, ROM bytes, table bytes, extracted CHR/palette data, generated proprietary data, and absolute private paths"
    }
    exact_ba16_entry = [pscustomobject]@{
        range = '04:$BA16-$BA24'
        behavior = 'sets update flag bit: $05B6 |= 0x01, then returns'
        native_status = "modeled in TecmoOriginalTitleGlyphs metadata and original-title-chr diagnostic"
    }
    adjacent_setup_driver = [pscustomobject]@{
        range = '04:$BA25-$BA84'
        status = "mapped as control flow and state targets; stream/table formats remain local-only and unresolved"
        helper_calls = @(Count-CallTargets -Map $Bank04 -Start 0xBA25 -End 0xBA84)
        write_targets = @(Count-WriteTargets -Map $Bank04 -Start 0xBA25 -End 0xBA84)
    }
    local_pointer_seed_helper = [pscustomobject]@{
        range = '04:$BA95-$BAA3'
        helper_calls = @(Count-CallTargets -Map $Bank04 -Start 0xBA95 -End 0xBAA3)
        write_targets = @(Count-WriteTargets -Map $Bank04 -Start 0xBA95 -End 0xBAA3)
    }
    local_stream_copy_helper = [pscustomobject]@{
        range = '04:$BAA4-$BAEF'
        helper_calls = @(Count-CallTargets -Map $Bank04 -Start 0xBAA4 -End 0xBAEF)
        write_targets = @(Count-WriteTargets -Map $Bank04 -Start 0xBAA4 -End 0xBAEF)
    }
    table_references = $TableRefs
    unresolved_gates = @(
        "Decode the adjacent setup stream/table formats without committing table bytes.",
        'Model fixed helper effects for $C05A, $C06F, $C009, $C054, and $C000 as explicit native staging operations.',
        "Identify which adjacent stream/table path supplies title pattern-table or nametable data for the first visible title screen.",
        "Identify palette RAM initialization and palette animation for the title path."
    )
    next_steps = @(
        "Add native structs for title setup staging state and fixed helper side effects.",
        "Add a local-only stream decoder report for selected B33F/B34E stream entries.",
        "Promote original-title-chr from raw CHR diagnostic once pattern/palette state is modeled."
    )
}

$ReportDir = Split-Path -Parent $ReportPath
if ($ReportDir) {
    New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null
}
$Report | ConvertTo-Json -Depth 8 | Set-Content -Path $ReportPath -Encoding ASCII

[pscustomobject]@{
    report = $ReportPath
    exact_ba16 = $Report.exact_ba16_entry.behavior
    adjacent_driver_calls = @($Report.adjacent_setup_driver.helper_calls).Count
    table_refs = @($Report.table_references).Count
    next_gate = "Decode adjacent setup streams and fixed helper effects"
} | Format-List
