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

function Test-RangePresent {
    param(
        [hashtable]$Map,
        [int]$Start,
        [int]$EndExclusive
    )

    for ($Address = $Start; $Address -lt $EndExclusive; ++$Address) {
        if (!$Map.ContainsKey($Address)) {
            return $false
        }
    }
    return $true
}

function New-StreamTableEntry {
    param(
        [hashtable]$Map,
        [int]$Index
    )

    $Lo = Get-MapByte -Map $Map -Address (0xB33F + $Index)
    $Hi = Get-MapByte -Map $Map -Address (0xB34E + $Index)
    $Pointer = ($Hi -shl 8) -bor $Lo
    $RecordCount = Get-MapByte -Map $Map -Address $Pointer
    $BaseParameterBytes = 2
    $SourceFieldsPerRecord = 4
    $StagedFieldsPerRecord = 4
    $BytesConsumed = 1 + $BaseParameterBytes + ($RecordCount * $SourceFieldsPerRecord)
    $EmittedBytes = $RecordCount * $StagedFieldsPerRecord

    return [pscustomobject]@{
        index = $Index
        pointer = Format-HexWord $Pointer
        record_count = $RecordCount
        base_parameter_bytes = $BaseParameterBytes
        source_fields_per_record = $SourceFieldsPerRecord
        staged_fields_per_record = $StagedFieldsPerRecord
        source_bytes_consumed = $BytesConsumed
        staged_bytes_emitted = $EmittedBytes
        record_shape = "one count byte, two private base-offset bytes, then four private source fields per record; helper emits four staged bytes per record"
        semantic_effect = "field 0 is offset by the stream X base, field 1 is offset by the fixed tile stride, field 2 is copied, and field 3 is offset by the stream Y base"
        payload_bytes_included = $false
        verified_range_present = Test-RangePresent -Map $Map -Start $Pointer -EndExclusive ($Pointer + $BytesConsumed)
    }
}

function New-SelectorRow {
    param(
        [hashtable]$Map,
        [int]$Row,
        [bool[]]$SelectedStreams
    )

    $Lo = Get-MapByte -Map $Map -Address (0xB317 + $Row)
    $Hi = Get-MapByte -Map $Map -Address (0xB31C + $Row)
    $Pointer = ($Hi -shl 8) -bor $Lo
    $EntryCount = 0
    $TerminatorFound = $false
    $MaxStreamIndex = -1

    for ($Offset = 0; $Offset -lt 16; ++$Offset) {
        $Value = Get-MapByte -Map $Map -Address ($Pointer + $Offset)
        if (($Value -band 0x80) -ne 0) {
            $TerminatorFound = $true
            break
        }
        if ($SelectedStreams -and $Value -ge 0 -and $Value -lt $SelectedStreams.Length) {
            $SelectedStreams[$Value] = $true
        }
        ++$EntryCount
        if ($Value -gt $MaxStreamIndex) {
            $MaxStreamIndex = $Value
        }
    }

    return [pscustomobject]@{
        row = $Row
        pointer = Format-HexWord $Pointer
        entry_count = $EntryCount
        terminator_found = $TerminatorFound
        max_stream_index = if ($MaxStreamIndex -ge 0) { $MaxStreamIndex } else { $null }
        values_included = $false
    }
}

function New-StreamStagingSummary {
    param(
        [array]$Entries,
        [bool[]]$SelectedStreams
    )

    $SelectedCount = 0
    $RecordCount = 0
    $BytesWritten = 0
    $MaxStagedBytes = 0

    for ($Index = 0; $Index -lt $SelectedStreams.Length; ++$Index) {
        if (!$SelectedStreams[$Index]) {
            continue
        }
        $Entry = $Entries[$Index]
        ++$SelectedCount
        $RecordCount += [int]$Entry.record_count
        $BytesWritten += [int]$Entry.staged_bytes_emitted
        if ([int]$Entry.staged_bytes_emitted -gt $MaxStagedBytes) {
            $MaxStagedBytes = [int]$Entry.staged_bytes_emitted
        }
    }

    $DestinationBase = 0x01FD
    $FirstWrite = if ($MaxStagedBytes -gt 0) { $DestinationBase + 3 } else { $null }
    $LastWrite = if ($MaxStagedBytes -gt 0) { $DestinationBase + 2 + $MaxStagedBytes } else { $null }

    return [pscustomobject]@{
        status = "selected streams applied to aggregate native staging summary without retaining payload bytes"
        destination_base = Format-HexWord $DestinationBase
        first_write = if ($null -ne $FirstWrite) { Format-HexWord $FirstWrite } else { $null }
        last_write = if ($null -ne $LastWrite) { Format-HexWord $LastWrite } else { $null }
        selected_stream_count = $SelectedCount
        record_count = $RecordCount
        staged_bytes_written = $BytesWritten
        payload_bytes_retained = $false
    }
}

function New-StreamDecodeSummary {
    param([hashtable]$Map)

    $SelectedStreams = New-Object bool[] 15
    $SelectedStreams[0] = $true
    $Entries = @(
        for ($Index = 0; $Index -lt 15; ++$Index) {
            New-StreamTableEntry -Map $Map -Index $Index
        }
    )
    $Rows = @(
        for ($Row = 0; $Row -lt 5; ++$Row) {
            New-SelectorRow -Map $Map -Row $Row -SelectedStreams $SelectedStreams
        }
    )
    $VerifiedEntries = @($Entries | Where-Object { $_.verified_range_present }).Count
    $TerminatedRows = @($Rows | Where-Object { $_.terminator_found }).Count
    $MaxRecordCount = ($Entries | Measure-Object -Property record_count -Maximum).Maximum
    $MaxBytesConsumed = ($Entries | Measure-Object -Property source_bytes_consumed -Maximum).Maximum
    $MaxEmittedBytes = ($Entries | Measure-Object -Property staged_bytes_emitted -Maximum).Maximum

    return [pscustomobject]@{
        status = "stream format and record effects summarized without payload bytes; fixed helper effects are summarized separately as aggregate native metadata"
        helper_range = '04:$BAA4-$BAF0'
        direct_bootstrap_stream_index = 0
        dynamic_selector_path = '$0742 -> selector remap at 04:$BA88 -> pointer rows at 04:$B317/04:$B31C -> BAA4 stream index'
        record_effect_model = [pscustomobject]@{
            count_bytes = 1
            base_parameter_bytes = 2
            source_fields_per_record = 4
            staged_fields_per_record = 4
            payload_bytes_included = $false
        }
        native_staging_summary = New-StreamStagingSummary -Entries $Entries -SelectedStreams $SelectedStreams
        stream_table_entries = $Entries
        selector_rows = $Rows
        aggregate = [pscustomobject]@{
            stream_table_entry_count = @($Entries).Count
            verified_stream_table_entry_count = $VerifiedEntries
            selector_row_count = @($Rows).Count
            terminated_selector_row_count = $TerminatedRows
            max_stream_record_count = $MaxRecordCount
            max_source_bytes_consumed = $MaxBytesConsumed
            max_staged_bytes_emitted = $MaxEmittedBytes
        }
        safety = [pscustomobject]@{
            source_payload_bytes_included = $false
            selector_values_included = $false
            report_is_ignored = $true
        }
    }
}

function New-FixedHelperEffectSummary {
    param([hashtable]$Map)

    $KnownFixedTargets = @("0xC000", "0xC009", "0xC054", "0xC05A", "0xC06F")
    $Seen = @{}
    $InvocationCount = 0
    $WaitCallCount = 0
    $WaitRequestTotal = 0
    $SetupFinalizeCallCount = 0
    $StagingSeedCallCount = 0
    $StreamFinalizeCallCount = 0
    $StreamFinalizeInsideHelper = $false
    $Ranges = @(
        [pscustomobject]@{ name = "adjacent_setup_driver"; start = 0xBA25; end = 0xBA84; display = '04:$BA25-$BA84' },
        [pscustomobject]@{ name = "local_stream_copy_helper"; start = 0xBAA4; end = 0xBAF0; display = '04:$BAA4-$BAF0' }
    )

    foreach ($Range in $Ranges) {
        for ($Address = $Range.start; $Address -le $Range.end - 2; ++$Address) {
            if ((Get-MapByte -Map $Map -Address $Address) -ne 0x20) {
                continue
            }

            $Lo = Get-MapByte -Map $Map -Address ($Address + 1)
            $Hi = Get-MapByte -Map $Map -Address ($Address + 2)
            $Target = Format-HexWord (($Hi -shl 8) -bor $Lo)
            if ($KnownFixedTargets -notcontains $Target) {
                continue
            }

            if (!$Seen.ContainsKey($Target)) {
                $Seen[$Target] = $true
            }
            ++$InvocationCount

            switch ($Target) {
                "0xC000" {
                    ++$WaitCallCount
                    if ($Address -ge $Range.start + 2 -and
                        (Get-MapByte -Map $Map -Address ($Address - 2)) -eq 0xA9) {
                        $WaitRequestTotal += Get-MapByte -Map $Map -Address ($Address - 1)
                    }
                }
                "0xC009" {
                    ++$SetupFinalizeCallCount
                }
                "0xC054" {
                    ++$StreamFinalizeCallCount
                    if ($Range.name -eq "local_stream_copy_helper") {
                        $StreamFinalizeInsideHelper = $true
                    }
                }
                { $_ -eq "0xC05A" -or $_ -eq "0xC06F" } {
                    ++$StagingSeedCallCount
                }
            }
        }
    }

    return [pscustomobject]@{
        status = "fixed helper calls summarized as explicit native side-effect categories without copying helper code"
        helper_ranges = @($Ranges | ForEach-Object { $_.display })
        known_fixed_targets = $KnownFixedTargets
        unique_helper_count = $Seen.Count
        fixed_call_invocations = $InvocationCount
        wait_call_count = $WaitCallCount
        wait_request_total = $WaitRequestTotal
        setup_finalize_call_count = $SetupFinalizeCallCount
        staging_seed_call_count = $StagingSeedCallCount
        stream_finalize_call_count = $StreamFinalizeCallCount
        stream_finalize_inside_baa4_helper = $StreamFinalizeInsideHelper
        payload_bytes_included = $false
        source_code_included = $false
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
$FixedHelperSummary = New-FixedHelperEffectSummary -Map $Bank04
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
        status = "mapped as control flow, state targets, and aggregate fixed-helper side-effect categories"
        helper_calls = @(Count-CallTargets -Map $Bank04 -Start 0xBA25 -End 0xBA84)
        write_targets = @(Count-WriteTargets -Map $Bank04 -Start 0xBA25 -End 0xBA84)
    }
    local_pointer_seed_helper = [pscustomobject]@{
        range = '04:$BA95-$BAA3'
        helper_calls = @(Count-CallTargets -Map $Bank04 -Start 0xBA95 -End 0xBAA3)
        write_targets = @(Count-WriteTargets -Map $Bank04 -Start 0xBA95 -End 0xBAA3)
    }
    local_stream_copy_helper = [pscustomobject]@{
        range = '04:$BAA4-$BAF0'
        helper_calls = @(Count-CallTargets -Map $Bank04 -Start 0xBAA4 -End 0xBAF0)
        write_targets = @(Count-WriteTargets -Map $Bank04 -Start 0xBAA4 -End 0xBAF0)
    }
    stream_decode_summary = New-StreamDecodeSummary -Map $Bank04
    fixed_helper_effect_summary = $FixedHelperSummary
    table_references = $TableRefs
    unresolved_gates = @(
        'Expand fixed helper aggregate categories into explicit native pattern-table/VRAM staging operations where needed.',
        "Identify which adjacent stream/table path supplies title pattern-table or nametable data for the first visible title screen.",
        "Identify palette RAM initialization and palette animation for the title path."
    )
    next_steps = @(
        "Add native structs for fixed helper side effects.",
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
    stream_entries = $Report.stream_decode_summary.aggregate.stream_table_entry_count
    stream_entries_verified = $Report.stream_decode_summary.aggregate.verified_stream_table_entry_count
    staging_records = $Report.stream_decode_summary.native_staging_summary.record_count
    staging_bytes = $Report.stream_decode_summary.native_staging_summary.staged_bytes_written
    fixed_helper_calls = $Report.fixed_helper_effect_summary.fixed_call_invocations
    wait_request_total = $Report.fixed_helper_effect_summary.wait_request_total
    table_refs = @($Report.table_references).Count
    next_gate = "Expand native helper side effects and palette setup"
} | Format-List
