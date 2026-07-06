param(
    [string]$ProjectRoot,
    [string]$OutputPath,
    [int]$Bank = 31,
    [int]$Table = 0
)

$ErrorActionPreference = "Stop"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path $ProjectRoot).Path

if ($Bank -lt 0 -or $Bank -gt 31) {
    throw "Bank must be in the 0..31 CHR bank range."
}
if ($Table -lt 0 -or $Table -gt 1) {
    throw "Table must be 0 or 1."
}

if (!$OutputPath) {
    $OutputPath = Join-Path $ProjectRoot "build\intro_layout_picks.json"
}

if (![System.IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath = Join-Path $ProjectRoot $OutputPath
}
$OutputPath = [System.IO.Path]::GetFullPath($OutputPath)

$Draft = [ordered]@{
    schema_version = 1
    capture_mode = "intro_lab"
    screen_id = "intro_tecmopresents"
    generated_at_utc = [DateTime]::UtcNow.ToString("o")
    generated_by = "tools/New-IntroLayoutDraft.ps1"
    data_policy = "Local-only user notes and tile IDs. Do not add ROM bytes, CHR bytes, ASM, extracted images, or private absolute paths."
    intro_lab_controls = [ordered]@{
        open_app = ".\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --play"
        open_render_test = ".\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode intro-presents build\intro_presents_test.png"
        switch_bank = "Intro Lab: Q/E. CHR Playground: Left/Right or Tab."
        switch_table = "Intro Lab: T. CHR Playground: Up/Down."
        rabbit_candidate = "Intro Lab: R records Bank 31 table 1 lookup-derived 8x16 sprite pairs 124-12B."
        tecmo_candidate = "Intro Lab: M records the visual Bank 31 table 1 TECMO logo candidate tiles 180-193."
        composite_candidate = "Intro Lab: C records the current rabbit plus TECMO composite candidate."
        save_records = "Intro Lab: S writes build/intro_layout_picks.json."
    }
    selected_chr_bank = $Bank
    selected_chr_table = $Table
    canvas = [ordered]@{
        screen_width = 640
        screen_height = 480
        origin_x = 330
        origin_y = 82
        width = 284
        height = 230
        grid_cell_pixels = 16
    }
    tile_picks = @(
        [ordered]@{
            id = "pick_001"
            chr_bank = $Bank
            chr_table = $Table
            tile_id_hex = ""
            row_hex = ""
            column_hex = ""
            role = ""
            confidence = "candidate"
            notes = "Example: bank 12 table 1 tile 1B6, mascot head highlight, or TECMO letter edge."
        }
    )
    placements = @(
        [ordered]@{
            id = "placement_001"
            label = ""
            chr_bank = $Bank
            chr_table = $Table
            tile_ids_hex = @()
            canvas_cell_x = $null
            canvas_cell_y = $null
            pixel_x = $null
            pixel_y = $null
            scale = 2
            flip_x = $false
            flip_y = $false
            palette_hint = ""
            notes = "Use 16px grid offsets from the Intro Lab canvas top-left."
        }
    )
    known_text_groups = @(
        [ordered]@{
            id = "bank06_text_map_tecmopresents"
            source = "native Bank 06 character map against selected CHR bank"
            chr_bank = $Bank
            chr_table = $Table
            text = "TECMO PRESENTS"
            status = "diagnostic only until original intro script path is decoded"
        }
    )
    procedure_leads = @(
        [ordered]@{
            id = "bank04_intro_driver"
            chunks = @("C-0116..C-0140")
            role = "intro sequence, dispatch, transition, and stream setup"
        },
        [ordered]@{
            id = "fixed_c051_d861"
            helper = "0xC051 -> 0xD861"
            role = "first native stream-to-sprite/OAM-style staging model candidate"
        },
        [ordered]@{
            id = "bank00_text_layout"
            chunks = @("C-0191", "C-0192", "C-0195")
            role = "TECMO/PRESENTS text-layout and mascot/composite tile-block candidates"
        }
    )
    verification = [ordered]@{
        screenshot_suite = ".\tools\Run-ScreenshotTests.ps1 -Build"
        procedure_scan = ".\tools\Find-IntroProcedureMapping.ps1"
        draft_is_commit_safe = $false
        expected_location = "build/intro_layout_picks.json"
    }
}

$OutputDir = Split-Path -Parent $OutputPath
if ($OutputDir) {
    New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
}

$Draft | ConvertTo-Json -Depth 8 | Set-Content -Path $OutputPath -Encoding ASCII
Write-Host "Wrote local-only intro layout draft: $OutputPath"
