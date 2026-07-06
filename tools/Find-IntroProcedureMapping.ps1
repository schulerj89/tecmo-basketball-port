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
    $ReportPath = Join-Path $ProjectRoot "build\intro_procedure_mapping.json"
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

function Find-PatternLines {
    param(
        [string]$Path,
        [string]$Pattern
    )

    if (!(Test-Path $Path)) {
        return @()
    }

    return @(
        Select-String -Path $Path -Pattern $Pattern -SimpleMatch |
            ForEach-Object { [int]$_.LineNumber }
    )
}

function Count-Pattern {
    param(
        [string]$Path,
        [string]$Pattern
    )

    return @(Find-PatternLines -Path $Path -Pattern $Pattern).Count
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

function Resolve-JmpTarget {
    param(
        [hashtable]$Map,
        [int]$Address
    )

    if (!$Map.ContainsKey($Address) -or !$Map.ContainsKey($Address + 1) -or !$Map.ContainsKey($Address + 2)) {
        return [pscustomobject]@{
            address = ("0x{0:X4}" -f $Address)
            resolved = $false
            target = $null
            reason = "vector bytes missing from local baseline map"
        }
    }

    if ([int]$Map[$Address] -ne 0x4C) {
        return [pscustomobject]@{
            address = ("0x{0:X4}" -f $Address)
            resolved = $false
            target = $null
            reason = "entry is not a direct JMP vector in the local baseline map"
        }
    }

    $Target = ([int]$Map[$Address + 2] -shl 8) -bor [int]$Map[$Address + 1]
    return [pscustomobject]@{
        address = ("0x{0:X4}" -f $Address)
        resolved = $true
        target = ("0x{0:X4}" -f $Target)
        reason = "direct fixed-bank JMP vector"
    }
}

function New-ProcedureLead {
    param(
        [string]$Root,
        [string]$RelativePath,
        [string]$Chunk,
        [string]$Role
    )

    $Path = Join-Path $Root $RelativePath
    $Exists = Test-Path $Path
    $C051Lines = Find-PatternLines -Path $Path -Pattern '$C051'
    $C711Lines = Find-PatternLines -Path $Path -Pattern '$C711'
    $StateLines = Find-PatternLines -Path $Path -Pattern '$058D'
    $OamLines = Find-PatternLines -Path $Path -Pattern '$0200'

    return [pscustomobject]@{
        relative_path = $RelativePath
        chunk = $Chunk
        role = $Role
        exists = $Exists
        c051_reference_count = @($C051Lines).Count
        c051_line_numbers = $C051Lines
        c711_reference_count = @($C711Lines).Count
        c711_line_numbers = $C711Lines
        state_058d_write_or_reference_count = @($StateLines).Count
        state_058d_line_numbers = $StateLines
        oam_0200_reference_count = @($OamLines).Count
        oam_0200_line_numbers = $OamLines
    }
}

$LocalDecompRoot = Find-LocalDecompRoot
if (!$LocalDecompRoot) {
    throw "Could not find a local decomp root. Pass -DecompRoot or set TECMO_DECOMP_ROOT."
}

$IntroChunks = @(
    @("decomp\lifted\bank04\C-0116_bank04_intro_sequence_setup_83AB_8426.asm", "C-0116", "intro sequence setup"),
    @("decomp\lifted\bank04\C-0117_bank04_intro_loop_step_8427_8462.asm", "C-0117", "intro loop step"),
    @("decomp\lifted\bank04\C-0119_bank04_intro_pattern_loader_8483_84DB.asm", "C-0119", "intro pattern loader"),
    @("decomp\lifted\bank04\C-0122_bank04_intro_transition_sequences_851C_863B.asm", "C-0122", "intro transition sequences"),
    @("decomp\lifted\bank04\C-0124_bank04_intro_render_helpers_and_followon_8645_86E0.asm", "C-0124", "intro render helper handoff"),
    @("decomp\lifted\bank04\C-0125_bank04_intro_capture_sequence_86E1_88A2.asm", "C-0125", "intro capture sequence"),
    @("decomp\lifted\bank04\C-0127_bank04_intro_tile_fade_and_scroll_88A9_8983.asm", "C-0127", "intro tile fade and scroll"),
    @("decomp\lifted\bank04\C-0129_bank04_intro_dual_stream_emit_8988_89BC.asm", "C-0129", "intro dual stream emit"),
    @("decomp\lifted\bank04\C-0131_bank04_intro_loop_and_copy_helper_89C1_89DC.asm", "C-0131", "intro loop copy helper"),
    @("decomp\lifted\bank04\C-0133_bank04_intro_mode_update_helpers_8A2D_8AA3.asm", "C-0133", "intro mode update helpers"),
    @("decomp\lifted\bank04\C-0134_bank04_intro_script_tables_8AA4_8FFF.asm", "C-0134", "intro script tables"),
    @("decomp\lifted\bank04\C-0135_bank04_presentation_data_tables_9000_BFFF.asm", "C-0135", "presentation data tables"),
    @("decomp\lifted\bank04\C-0136_bank04_early_intro_flow_8000_81C2.asm", "C-0136", "early intro flow"),
    @("decomp\lifted\bank04\C-0137_bank04_wait_and_intro_dispatch_81C3_8259.asm", "C-0137", "wait and intro dispatch"),
    @("decomp\lifted\bank04\C-0139_bank04_intro_sequence_driver_825D_82CE.asm", "C-0139", "intro sequence driver"),
    @("decomp\lifted\bank04\C-0140_bank04_intro_dispatch_tables_82CF_82F9.asm", "C-0140", "intro dispatch tables"),
    @("decomp\lifted\bank00\C-0191_bank00_menu_text_and_layout_stream_c_0400_06FF.asm", "C-0191", "PRESENTS text and layout lead"),
    @("decomp\lifted\bank00\C-0192_bank00_ui_banner_and_glyph_stream_0700_0AFF.asm", "C-0192", "TECMO text and tile-run lead"),
    @("decomp\lifted\bank00\C-0195_bank00_tile_atlas_blocks_19C0_22FF.asm", "C-0195", "mascot or composite tile-block lead")
)

$Leads = @(
    foreach ($Chunk in $IntroChunks) {
        New-ProcedureLead -Root $LocalDecompRoot -RelativePath $Chunk[0] -Chunk $Chunk[1] -Role $Chunk[2]
    }
)

$FixedPath = Join-Path $LocalDecompRoot "build\baseline\Tecmo_07.asm"
$FixedVector = [pscustomobject]@{
    address = "0xC051"
    resolved = $false
    target = $null
    reason = "fixed baseline not found"
}
$FixedOamMentions = 0
if (Test-Path $FixedPath) {
    $FixedMap = Read-BankedByteMap -Path $FixedPath -BankIndex 7 -CpuBase 0xC000
    $FixedVector = Resolve-JmpTarget -Map $FixedMap -Address 0xC051
    $FixedOamMentions = Count-Pattern -Path $FixedPath -Pattern '$0200'
}

$Report = [ordered]@{
    schema_version = 1
    generated_at_utc = [DateTime]::UtcNow.ToString("o")
    decomp_root_used = "<local>"
    source_policy = "Derived counts, paths, line numbers, and model boundaries only. No ASM text, ROM bytes, CHR bytes, extracted graphics, or private absolute paths."
    intro_driver_leads = $Leads
    fixed_helper_model = [ordered]@{
        c051_vector = $FixedVector
        fixed_bank_oam_0200_reference_count = $FixedOamMentions
        watched_state = [ordered]@{
            sprite_staging_range = "0x0200-0x02FF"
            sprite_count_candidate = "0x058D"
            source_pointer_candidate = "0x002C/0x002D"
            param_register_candidates = @("0x0009", "0x000B", "0x000D", "0x002E")
        }
    }
    recommended_native_boundaries = @(
        [ordered]@{
            name = "TecmoIntroProcedureReport"
            owner = "local-only tooling"
            purpose = "Hold safe counts, chunk references, and verified helper targets for docs and debug UI."
        },
        [ordered]@{
            name = "TecmoIntroSpriteStream"
            owner = "future portable C runtime"
            purpose = "Represent decoded local-only stream records as neutral sprite commands without retaining source payload bytes."
        },
        [ordered]@{
            name = "tecmo_intro_emit_sprite_stream"
            owner = "future portable C runtime"
            purpose = "Native first-pass model of fixed helper 0xC051/0xD861 that consumes decoded stream commands and writes OAM-shaped staging records."
        },
        [ordered]@{
            name = "TecmoIntroFrameState"
            owner = "future portable C runtime"
            purpose = "Track bank, palette hint, frame counter, sprite count, scroll/fade state, and OAM staging for the intro sequence."
        }
    )
    next_decode_steps = @(
        "Use the Intro Lab bank/table/tile picks to identify the visible CHR source and candidate tile IDs.",
        "Decode Bank 04 C-0124/C-0129 stream parameters into local-only neutral records.",
        "Model 0xC051/0xD861 as a native sprite staging helper against OAM-shaped memory.",
        "Only then replace the diagnostic TECMO PRESENTS placement with original script-driven placement."
    )
}

$ReportDir = Split-Path -Parent $ReportPath
if ($ReportDir) {
    New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null
}

$Report | ConvertTo-Json -Depth 8 | Set-Content -Path $ReportPath -Encoding ASCII
Write-Host "Wrote intro procedure mapping report: $ReportPath"
