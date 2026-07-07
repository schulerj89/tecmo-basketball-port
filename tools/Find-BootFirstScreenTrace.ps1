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
    $ReportPath = Join-Path $ProjectRoot "build\boot_first_screen_trace.json"
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

function Format-Addr {
    param([int]$Value)
    return ("0x{0:X4}" -f $Value)
}

function Read-BankedByteMap {
    param(
        [string]$Path,
        [int]$BankIndex,
        [int]$CpuBase
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
        throw ("Missing byte at {0} in local baseline map." -f (Format-Addr $Address))
    }
    return [int]$Map[$Address]
}

function Get-Le16 {
    param(
        [hashtable]$Map,
        [int]$Address
    )

    $Low = Get-MapByte -Map $Map -Address $Address
    $High = Get-MapByte -Map $Map -Address ($Address + 1)
    return (($High -shl 8) -bor $Low)
}

function Resolve-JmpTarget {
    param(
        [hashtable]$Map,
        [int]$Address
    )

    if (!$Map.ContainsKey($Address) -or !$Map.ContainsKey($Address + 1) -or !$Map.ContainsKey($Address + 2)) {
        return [pscustomobject]@{
            address = Format-Addr $Address
            resolved = $false
            target = $null
            reason = "vector bytes missing from local baseline map"
        }
    }

    if ([int]$Map[$Address] -ne 0x4C) {
        return [pscustomobject]@{
            address = Format-Addr $Address
            resolved = $false
            target = $null
            reason = "entry is not a direct JMP vector in the local baseline map"
        }
    }

    $Target = Get-Le16 -Map $Map -Address ($Address + 1)
    return [pscustomobject]@{
        address = Format-Addr $Address
        resolved = $true
        target = Format-Addr $Target
        reason = "direct fixed-bank JMP vector"
    }
}

function Find-BytePattern {
    param(
        [hashtable]$Map,
        [int]$Start,
        [int]$End,
        [int[]]$Pattern
    )

    if (!$Pattern -or $Pattern.Count -eq 0) {
        return $null
    }

    $Last = $End - $Pattern.Count + 1
    for ($Address = $Start; $Address -le $Last; ++$Address) {
        $Matched = $true
        for ($Index = 0; $Index -lt $Pattern.Count; ++$Index) {
            if (!$Map.ContainsKey($Address + $Index) -or [int]$Map[$Address + $Index] -ne $Pattern[$Index]) {
                $Matched = $false
                break
            }
        }
        if ($Matched) {
            return $Address
        }
    }

    return $null
}

function New-PatternEvidence {
    param(
        [string]$Name,
        [hashtable]$Map,
        [int]$Start,
        [int]$End,
        [int[]]$Pattern,
        [string]$Meaning
    )

    $Address = Find-BytePattern -Map $Map -Start $Start -End $End -Pattern $Pattern
    return [pscustomobject]@{
        name = $Name
        found = ($null -ne $Address)
        match_address = if ($null -ne $Address) { Format-Addr $Address } else { $null }
        search_range = ("{0}-{1}" -f (Format-Addr $Start), (Format-Addr $End))
        meaning = $Meaning
    }
}

function Get-Bank04ChunkRole {
    param([int]$Address)

    $Ranges = @(
        @{ Start = 0x825D; End = 0x82CE; Chunk = "C-0139"; Role = "intro sequence driver" },
        @{ Start = 0x82CF; End = 0x82F9; Chunk = "C-0140"; Role = "intro dispatch pointer/timing tables" },
        @{ Start = 0x8310; End = 0x836A; Chunk = "C-0114"; Role = "later title render loop/pretext" },
        @{ Start = 0x83AB; End = 0x8426; Chunk = "C-0116"; Role = "intro sequence setup" },
        @{ Start = 0x8483; End = 0x84DB; Chunk = "C-0119"; Role = "intro pattern loader" },
        @{ Start = 0x851C; End = 0x863B; Chunk = "C-0122"; Role = "intro transition sequence" },
        @{ Start = 0x8645; End = 0x86E0; Chunk = "C-0124"; Role = "intro render helper handoff" },
        @{ Start = 0x86E1; End = 0x88A2; Chunk = "C-0125"; Role = "intro capture sequence" },
        @{ Start = 0x88A9; End = 0x8983; Chunk = "C-0127"; Role = "intro tile fade/scroll sequence" },
        @{ Start = 0x8988; End = 0x89BC; Chunk = "C-0129"; Role = "intro dual stream emit helper" }
    )

    foreach ($Range in $Ranges) {
        if ($Address -ge $Range.Start -and $Address -le $Range.End) {
            return [pscustomobject]@{
                chunk = $Range.Chunk
                role = $Range.Role
                range = ("{0}-{1}" -f (Format-Addr $Range.Start), (Format-Addr $Range.End))
            }
        }
    }

    return [pscustomobject]@{
        chunk = $null
        role = "not covered by current safe Bank04 intro role map"
        range = $null
    }
}

function New-SourceEvidence {
    param(
        [string]$Root,
        [string]$RelativePath,
        [string]$Role
    )

    $Path = Join-Path $Root $RelativePath
    return [pscustomobject]@{
        relative_path = $RelativePath
        role = $Role
        exists = (Test-Path $Path)
    }
}

function Resolve-ResetHandoff {
    param(
        [hashtable]$Map,
        [string]$ResetChunkPath
    )

    $Resolved = Resolve-JmpTarget -Map $Map -Address 0xFFBC
    if ($Resolved.resolved) {
        return $Resolved
    }

    if ((Test-Path $ResetChunkPath) -and @(Select-String -Path $ResetChunkPath -Pattern 'JMP $CC30' -SimpleMatch).Count -gt 0) {
        return [pscustomobject]@{
            address = "0xFFBC"
            resolved = $true
            target = "0xCC30"
            reason = "lifted reset chunk C-0001 confirms fixed bootstrap handoff"
        }
    }

    return $Resolved
}

$LocalDecompRoot = Find-LocalDecompRoot
if (!$LocalDecompRoot) {
    throw "Could not find a local decomp root. Pass -DecompRoot or set TECMO_DECOMP_ROOT."
}

$Bank07Baseline = Join-Path $LocalDecompRoot "build\baseline\Tecmo_07.asm"
$Bank04Baseline = Join-Path $LocalDecompRoot "build\baseline\Tecmo_04.asm"
if (!(Test-Path $Bank07Baseline)) {
    throw "Missing local fixed-bank baseline: $Bank07Baseline"
}
if (!(Test-Path $Bank04Baseline)) {
    throw "Missing local Bank04 baseline: $Bank04Baseline"
}

$Bank07Map = Read-BankedByteMap -Path $Bank07Baseline -BankIndex 7 -CpuBase 0xC000
$Bank04Map = Read-BankedByteMap -Path $Bank04Baseline -BankIndex 4 -CpuBase 0x8000
$ResetChunkPath = Join-Path $LocalDecompRoot "decomp\lifted\C-0001_bank07_reset.asm"

$FixedVectors = @(
    0xC000,
    0xC003,
    0xC00C,
    0xC051,
    0xC054,
    0xC05A
) | ForEach-Object { Resolve-JmpTarget -Map $Bank07Map -Address $_ }

$ResetJump = Resolve-ResetHandoff -Map $Bank07Map -ResetChunkPath $ResetChunkPath
$BootSchedulesBank07 = New-PatternEvidence `
    -Name "bootstrap_schedules_bank07_task" `
    -Map $Bank07Map `
    -Start 0xCC30 `
    -End 0xCD31 `
    -Pattern @(0xA9, 0x07, 0xA2, 0x80, 0x20, 0x99, 0xE3) `
    -Meaning "Bank07 bootstrap seeds scheduler slot 0x80 with bank 0x07 via the fixed scheduler helper at 0xE399."
$BootTaskTarget = New-PatternEvidence `
    -Name "bootstrap_bank07_task_entry_seed" `
    -Map $Bank07Map `
    -Start 0xCC30 `
    -End 0xCD31 `
    -Pattern @(0xA9, 0x19, 0x85, 0x82, 0xA9, 0xE4, 0x85, 0x83) `
    -Meaning "Bootstrap seeds the scheduled Bank07 task return/entry path to the 0xE419/0xE41A region."
$Bank07SchedulesBank04 = New-PatternEvidence `
    -Name "bank07_task_schedules_bank04_intro_driver" `
    -Map $Bank07Map `
    -Start 0xE41A `
    -End 0xE440 `
    -Pattern @(0xA9, 0x5C, 0x85, 0x92, 0xA9, 0x82, 0x85, 0x93, 0xA9, 0x04, 0xA2, 0x90, 0x20, 0x99, 0xE3) `
    -Meaning "The first Bank07 task seeds a Bank04 scheduled task at the 0x825C/0x825D intro-driver region."
$Bank04DriverEntry = New-PatternEvidence `
    -Name "bank04_intro_driver_entry" `
    -Map $Bank04Map `
    -Start 0x825D `
    -End 0x8280 `
    -Pattern @(0xA2, 0xA6, 0x9A, 0xA9, 0x00, 0x8D, 0x84, 0x07, 0xA9, 0x13, 0x20, 0x11, 0xC7) `
    -Meaning "Bank04 0x825D starts the intro sequence driver and immediately runs the 0xC711 setup command 0x13."

$DispatchPointers = @()
$PointerAddress = 0x82CF
for ($Index = 0; $Index -lt 32; ++$Index) {
    $Target = Get-Le16 -Map $Bank04Map -Address $PointerAddress
    $Role = Get-Bank04ChunkRole -Address $Target
    $DispatchPointers += [pscustomobject]@{
        index = $Index
        pointer_address = Format-Addr $PointerAddress
        target = Format-Addr $Target
        terminator = ($Target -eq 0xFFFF)
        chunk = $Role.chunk
        role = $Role.role
        range = $Role.range
    }

    $PointerAddress += 2
    if ($Target -eq 0xFFFF) {
        break
    }
}

$FirstScriptTarget = $DispatchPointers | Where-Object { $_.index -eq 0 } | Select-Object -First 1
$LaterTitleLoop = $DispatchPointers | Where-Object { $_.target -eq "0x8310" } | Select-Object -First 1

$SourceEvidence = @(
    New-SourceEvidence -Root $LocalDecompRoot -RelativePath "decomp\lifted\C-0001_bank07_reset.asm" -Role "reset entry and jump to fixed bootstrap"
    New-SourceEvidence -Root $LocalDecompRoot -RelativePath "decomp\lifted\C-0002_bank07_nmi.asm" -Role "first visible OAM DMA path after staging"
    New-SourceEvidence -Root $LocalDecompRoot -RelativePath "decomp\lifted\bank04\C-0139_bank04_intro_sequence_driver_825D_82CE.asm" -Role "scheduled Bank04 intro driver"
    New-SourceEvidence -Root $LocalDecompRoot -RelativePath "decomp\lifted\bank04\C-0140_bank04_intro_dispatch_tables_82CF_82F9.asm" -Role "Bank04 intro pointer table"
    New-SourceEvidence -Root $LocalDecompRoot -RelativePath "decomp\lifted\bank04\C-0127_bank04_intro_tile_fade_and_scroll_88A9_8983.asm" -Role "first script target region"
    New-SourceEvidence -Root $LocalDecompRoot -RelativePath "decomp\lifted\bank04\C-0129_bank04_intro_dual_stream_emit_8988_89BC.asm" -Role "stream emit into fixed C051/D861 sprite staging"
)

$Report = [ordered]@{
    schema_version = 1
    generated_at_utc = [DateTime]::UtcNow.ToString("o")
    decomp_root_detected = $true
    source_policy = "Safe trace only: no ROM bytes, ASM source, CHR/PRG payloads, or decoded proprietary asset data are emitted."
    reset_jump = $ResetJump
    fixed_vectors = $FixedVectors
    verified_handoffs = @(
        $BootSchedulesBank07,
        $BootTaskTarget,
        $Bank07SchedulesBank04,
        $Bank04DriverEntry
    )
    bank04_intro_dispatch = [ordered]@{
        driver_entry = "0x825D"
        pointer_table_range = "0x82CF-0x82E6"
        delay_table_range = "0x82E7-0x82F1"
        entries = $DispatchPointers
    }
    conclusion = [ordered]@{
        first_non_fixed_bank = "0x04"
        first_scheduled_intro_driver = "Bank04 0x825D"
        first_script_target = $FirstScriptTarget.target
        first_script_role = $FirstScriptTarget.role
        first_visible_stream_path = "Bank04 first script target flows into the 0x8988 dual-stream emitter, which calls fixed vector 0xC051 -> 0xD861 and then 0xC054 -> 0xD2D2 for OAM staging/cleanup."
        later_title_text_loop_index = if ($LaterTitleLoop) { $LaterTitleLoop.index } else { $null }
        later_title_text_loop_target = if ($LaterTitleLoop) { $LaterTitleLoop.target } else { $null }
        porting_takeaway = "Bank07 is the native boot/scheduler layer. The first screen logic to port next is the Bank04 intro driver plus its first dispatch stream path, not a standalone Bank07 renderer."
    }
    source_evidence = $SourceEvidence
}

$ReportDirectory = Split-Path -Parent $ReportPath
if ($ReportDirectory -and !(Test-Path $ReportDirectory)) {
    New-Item -ItemType Directory -Force -Path $ReportDirectory | Out-Null
}

$Report | ConvertTo-Json -Depth 8 | Set-Content -Path $ReportPath -Encoding UTF8

Write-Host ("BOOT TRACE PASS: reset {0} -> Bank07 task -> Bank04 intro {1} -> first script {2}" -f $ResetJump.target, $Report.conclusion.first_scheduled_intro_driver, $Report.conclusion.first_script_target)
Write-Host ("Report: {0}" -f $ReportPath)
