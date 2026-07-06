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
    $ReportPath = Join-Path $ProjectRoot "build\original_screen_sources.json"
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

function Read-Labels {
    param(
        [string]$Path,
        [int]$Limit = 12
    )

    $Labels = New-Object System.Collections.Generic.List[string]
    foreach ($Match in Select-String -Path $Path -Pattern '^\s*([A-Za-z_.$][A-Za-z0-9_.$]*):') {
        $Label = $Match.Matches[0].Groups[1].Value
        if ($Labels.Count -lt $Limit) {
            $Labels.Add($Label)
        }
    }
    return @($Labels)
}

function Count-Pattern {
    param(
        [string]$Path,
        [string]$Pattern
    )

    return @((Select-String -Path $Path -Pattern $Pattern -SimpleMatch)).Count
}

function New-FileCandidate {
    param(
        [string]$RelativePath,
        [string]$Chunk,
        [string]$Range,
        [string]$Role,
        [string]$Need
    )

    return [pscustomobject]@{
        relative_path = $RelativePath
        chunk = $Chunk
        range = $Range
        role = $Role
        implementation_need = $Need
    }
}

$LocalDecompRoot = Find-LocalDecompRoot
if (!$LocalDecompRoot) {
    throw "Could not find a local decomp root. Pass -DecompRoot or set TECMO_DECOMP_ROOT."
}

$Groups = @(
    [pscustomobject]@{
        id = "bank04_title_splash_core"
        confidence = "high"
        summary = "Closest first-screen target: title splash control loop, title text table, and helper wrapper."
        files = @(
            (New-FileCandidate "decomp\lifted\bank04\C-0114_bank04_title_render_loop_pretext_8303_836A.asm" "C-0114" '04:$8303-$836A' "title render loop" "Port control flow and C711-style text emission."),
            (New-FileCandidate "decomp\lifted\bank04\C-0004_bank04_menu_title_text_836B_8384.asm" "C-0004" '04:$836B-$8384' "title text table" "Decode through the original character/tile output path; do not commit table bytes."),
            (New-FileCandidate "decomp\lifted\bank04\C-0115_bank04_title_render_helpers_8385_83AA.asm" "C-0115" '04:$8385-$83AA' "title render helpers" "Port timing/position helper state and setup wrapper.")
        )
    },
    [pscustomobject]@{
        id = "bank04_intro_staging"
        confidence = "high"
        summary = "Intro/title splash setup, per-frame loop, pattern records, transition tables, and fade/scroll helpers."
        files = @(
            (New-FileCandidate "decomp\lifted\bank04\C-0116_bank04_intro_sequence_setup_83AB_8426.asm" "C-0116" '04:$83AB-$8426' "intro sequence setup" "Port setup state before the frame loop."),
            (New-FileCandidate "decomp\lifted\bank04\C-0117_bank04_intro_loop_step_8427_8462.asm" "C-0117" '04:$8427-$8462' "intro loop step" "Port per-frame counters and optional tile emission."),
            (New-FileCandidate "decomp\lifted\bank04\C-0118_bank04_intro_loop_lookup_tables_8463_8482.asm" "C-0118" '04:$8463-$8482' "loop lookup tables" "Map small parameter tables into native read-only local data."),
            (New-FileCandidate "decomp\lifted\bank04\C-0119_bank04_intro_pattern_loader_8483_84DB.asm" "C-0119" '04:$8483-$84DB' "pattern loader" "Port record-copy staging into native screen command buffers."),
            (New-FileCandidate "decomp\lifted\bank04\C-0120_bank04_intro_pattern_records_84DC_850B.asm" "C-0120" '04:$84DC-$850B' "pattern records" "Decode record fields locally; do not commit raw record bytes."),
            (New-FileCandidate "decomp\lifted\bank04\C-0122_bank04_intro_transition_sequences_851C_863B.asm" "C-0122" '04:$851C-$863B' "transition sequences" "Port timed movement/transition loops."),
            (New-FileCandidate "decomp\lifted\bank04\C-0123_bank04_intro_transition_tables_863C_8644.asm" "C-0123" '04:$863C-$8644' "transition parameter tables" "Map compact parameter bytes locally."),
            (New-FileCandidate "decomp\lifted\bank04\C-0127_bank04_intro_tile_fade_and_scroll_88A9_8983.asm" "C-0127" '04:$88A9-$8983' "tile fade and scroll" "Port fade/scroll state and BA16/C000 helper dependencies.")
        )
    },
    [pscustomobject]@{
        id = "bank04_large_intro_tables"
        confidence = "medium"
        summary = "Coarse structured script/presentation regions likely needed after the first static title splash."
        files = @(
            (New-FileCandidate "decomp\lifted\bank04\C-0132_bank04_intro_script_pattern_table_89DD_8A2C.asm" "C-0132" '04:$89DD-$8A2C' "intro script/pattern table" "Split into named tables once cross-references are confirmed."),
            (New-FileCandidate "decomp\lifted\bank04\C-0134_bank04_intro_script_tables_8AA4_8FFF.asm" "C-0134" '04:$8AA4-$8FFF' "intro script tables" "Local-only table mapping before renderer consumes it."),
            (New-FileCandidate "decomp\lifted\bank04\C-0135_bank04_presentation_data_tables_9000_BFFF.asm" "C-0135" '04:$9000-$BFFF' "presentation data tables" "Resolve sub-table boundaries before using this in native render code.")
        )
    },
    [pscustomobject]@{
        id = "bank03_menu_presentation_flow"
        confidence = "medium"
        summary = "Follow-on menu/presentation logic after the title/intro path; useful for the first real menu after title."
        files = @(
            (New-FileCandidate "decomp\lifted\bank03\C-0142_bank03_initial_intro_and_dispatch_8000_8302.asm" "C-0142" '03:$8000-$8302' "initial intro dispatch" "Map handoff into presentation/menu flow."),
            (New-FileCandidate "decomp\lifted\bank03\C-0143_bank03_intro_selection_loop_8303_8373.asm" "C-0143" '03:$8303-$8373' "intro selection loop" "Port confirm/cancel loop when menu state is selected."),
            (New-FileCandidate "decomp\lifted\bank03\C-0149_bank03_presentation_setup_and_input_loop_848A_85D7.asm" "C-0149" '03:$848A-$85D7' "presentation setup/input loop" "Map input and setup transitions."),
            (New-FileCandidate "decomp\lifted\bank03\C-0152_bank03_presentation_update_loop_and_banner_dispatch_8670_87D8.asm" "C-0152" '03:$8670-$87D8' "presentation update loop" "Map banner dispatch and PPU-style write staging."),
            (New-FileCandidate "decomp\lifted\bank03\C-0156_bank03_state_gate_and_menu_cycle_loop_88F8_89BC.asm" "C-0156" '03:$88F8-$89BC' "menu cycle loop" "Port state gate and menu cycle behavior."),
            (New-FileCandidate "decomp\lifted\bank03\C-0171_bank03_large_lookup_tables_9AC8_A09D.asm" "C-0171" '03:$9AC8-$A09D' "menu text/script records" "Decode script records locally without committing generated text/data.")
        )
    },
    [pscustomobject]@{
        id = "ppu_chr_and_fixed_helpers"
        confidence = "medium"
        summary = "Renderer dependencies that must be replaced by native C equivalents."
        files = @(
            (New-FileCandidate "build\baseline\Tiles.asm" "CHR" "local CHR source" "CHR/tile patterns" "Determine exact CHR bank and tile IDs for the title/menu screen locally."),
            (New-FileCandidate "decomp\lifted\C-0002_bank07_nmi.asm" "C-0002" '07:$CD2F-$CD86' "NMI/OAM DMA" "Map NMI-era presentation side effects into explicit native frame update."),
            (New-FileCandidate "decomp\lifted\C-0003_bank07_irq_dispatch.asm" "C-0003" '07:$FC98-$FF77' "IRQ/bank/scroll dispatch" "Map scroll/bank side effects that title/menu rendering may rely on.")
        )
    }
)

$ResolvedGroups = New-Object System.Collections.Generic.List[object]
foreach ($Group in $Groups) {
    $ResolvedFiles = New-Object System.Collections.Generic.List[object]
    $Present = 0

    foreach ($File in $Group.files) {
        $FullPath = Join-Path $LocalDecompRoot $File.relative_path
        $Exists = Test-Path $FullPath
        if ($Exists) {
            ++$Present
        }

        $ResolvedFiles.Add([pscustomobject]@{
            relative_path = $File.relative_path
            chunk = $File.chunk
            range = $File.range
            role = $File.role
            implementation_need = $File.implementation_need
            exists = $Exists
            label_count = if ($Exists) { @((Select-String -Path $FullPath -Pattern '^\s*([A-Za-z_.$][A-Za-z0-9_.$]*):')).Count } else { 0 }
            sample_labels = if ($Exists) { Read-Labels -Path $FullPath -Limit 8 } else { @() }
            c000_references = if ($Exists) { Count-Pattern -Path $FullPath -Pattern '$C000' } else { 0 }
            c711_references = if ($Exists) { Count-Pattern -Path $FullPath -Pattern '$C711' } else { 0 }
            ba16_references = if ($Exists) { Count-Pattern -Path $FullPath -Pattern '$BA16' } else { 0 }
            c018_references = if ($Exists) { Count-Pattern -Path $FullPath -Pattern '$C018' } else { 0 }
        })
    }

    $ResolvedGroups.Add([pscustomobject]@{
        id = $Group.id
        confidence = $Group.confidence
        summary = $Group.summary
        present_files = $Present
        total_files = @($Group.files).Count
        files = $ResolvedFiles
    })
}

$Report = [pscustomobject]@{
    generated_at_utc = [DateTime]::UtcNow.ToString("o")
    target = "first original title/menu screen"
    decomp_root_used = "<local>"
    safety = [pscustomobject]@{
        generated_report_contains = "paths, labels, ranges, counts, and implementation notes only"
        forbidden = "ASM text, ROM bytes, CHR bytes, extracted graphics, generated roster data, and absolute private paths"
    }
    source_groups = $ResolvedGroups
    unresolved_gates = @(
        "Identify the exact fixed-bank behavior behind C000 frame waits and C711/C018/BA16 render helpers.",
        "Resolve title/menu palette initialization and any palette animation path.",
        "Map title/menu tile IDs to local CHR bank(s) without committing extracted tile bytes.",
        "Turn Bank 04 title/pattern/script records into native structs generated or loaded only from local data.",
        "Add a native renderer quick launch after the above dependencies are mapped."
    )
}

$ReportDir = Split-Path -Parent $ReportPath
if ($ReportDir) {
    New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null
}
$Report | ConvertTo-Json -Depth 8 | Set-Content -Path $ReportPath -Encoding ASCII

$ResolvedGroups | Select-Object id, confidence, present_files, total_files, summary | Format-Table -AutoSize
Write-Host "Wrote original screen source map: $ReportPath"
