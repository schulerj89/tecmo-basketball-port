param(
    [string]$ProjectRoot,
    [string]$DecompRoot,
    [string]$CompositeTracePath,
    [string]$ReportPath,
    [int]$ChrBank = 31
)

$ErrorActionPreference = "Stop"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path $ProjectRoot).Path

if (!$CompositeTracePath) {
    $CompositeTracePath = Join-Path $ProjectRoot "build\intro_composite_trace.json"
}
if (!$ReportPath) {
    $ReportPath = Join-Path $ProjectRoot "build\intro_first_sprite_trace.json"
}
if (![System.IO.Path]::IsPathRooted($CompositeTracePath)) {
    $CompositeTracePath = Join-Path $ProjectRoot $CompositeTracePath
}
if (![System.IO.Path]::IsPathRooted($ReportPath)) {
    $ReportPath = Join-Path $ProjectRoot $ReportPath
}
$CompositeTracePath = [System.IO.Path]::GetFullPath($CompositeTracePath)
$ReportPath = [System.IO.Path]::GetFullPath($ReportPath)

function Convert-HexByteToInt {
    param([string]$Text)
    return [Convert]::ToInt32(($Text -replace '^\$', ''), 16) -band 0xFF
}

function Format-HexByte {
    param([int]$Value)
    return ("{0:X2}" -f ($Value -band 0xFF))
}

function New-SpriteSummary {
    param(
        [object]$Record,
        [string]$Kind
    )

    if (!$Record) {
        return $null
    }

    $Tile = Convert-HexByteToInt $Record.oam_tile_low_after_offset_hex
    $Attributes = Convert-HexByteToInt $Record.attributes_hex
    $X = [int]$Record.screen_x_from_current_base
    $Y = [int]$Record.screen_y_from_current_base

    return [ordered]@{
        kind = $Kind
        component = $Record.component
        role = $Record.role
        record_index = [int]$Record.record_index
        stream_record_address = $Record.stream_record_address
        oam = [ordered]@{
            y_hex = Format-HexByte $Y
            tile_hex = Format-HexByte $Tile
            attributes_hex = Format-HexByte $Attributes
            x_hex = Format-HexByte $X
        }
        screen_position = [ordered]@{
            x = $X
            y = $Y
            visible_y = ($Y -ge 0 -and $Y -lt 240)
        }
        chr_pairs = [ordered]@{
            table1_top_hex = $Record.table1_pair_top_hex
            table1_bottom_hex = $Record.table1_pair_bottom_hex
        }
    }
}

if (!(Test-Path $CompositeTracePath)) {
    $CompositeTool = Join-Path $PSScriptRoot "Find-IntroCompositeTrace.ps1"
    if (!(Test-Path $CompositeTool)) {
        throw "Composite trace is missing and Find-IntroCompositeTrace.ps1 was not found."
    }

    $Args = @(
        "-ProjectRoot", $ProjectRoot,
        "-ReportPath", $CompositeTracePath,
        "-ChrBank", $ChrBank
    )
    if ($DecompRoot) {
        $Args += @("-DecompRoot", $DecompRoot)
    }
    & $CompositeTool @Args | Out-Host
}

if (!(Test-Path $CompositeTracePath)) {
    throw "Composite trace is still missing: $CompositeTracePath"
}

$Composite = Get-Content -Raw $CompositeTracePath | ConvertFrom-Json
$RabbitStream = @($Composite.primary_streams | Where-Object { $_.component -eq "rabbit_full_stream" } | Select-Object -First 1)
if (!$RabbitStream) {
    throw "Composite trace does not contain rabbit_full_stream."
}

$Records = @($RabbitStream.records)
if ($Records.Count -eq 0) {
    throw "rabbit_full_stream contains no records."
}

$FirstStaged = $Records[0]
$FirstVisible = @($Records | Where-Object {
    [int]$_.screen_y_from_current_base -ge 0 -and [int]$_.screen_y_from_current_base -lt 240
} | Select-Object -First 1)
$StreamRows = @(
    $Records |
        Select-Object -First ([int]$RabbitStream.record_count) |
        ForEach-Object { New-SpriteSummary -Record $_ -Kind "early D861 staged record" }
)

$Report = [ordered]@{
    schema_version = 1
    generated_at_utc = [DateTime]::UtcNow.ToString("o")
    data_policy = "Local-only decoded sprite trace. Do not commit this generated report; it contains private decoded sprite-row metadata."
    private_paths_included = $false
    sequence = @(
        "Bank07 reset at 0xFFBC jumps to fixed bootstrap 0xCC30.",
        "Fixed bootstrap schedules the Bank07 task that seeds Bank04 0x825D.",
        "Bank04 intro driver 0x825D uses dispatch index 0 from table 0x82CF.",
        "Dispatch index 0 targets Bank04 0x88E8, the first intro script routine.",
        "0x88E8 runs setup command 0x18 through 0x82F2, stages setup bytes through fixed 0xC05A, then calls the local 0x8988 dual-stream emitter.",
        "0x8988 calls fixed 0xC051, which resolves to 0xD861 and stages sprite/OAM-shaped records."
    )
    first_c051_call = [ordered]@{
        bank04_entry = "0x8988"
        fixed_vector = "0xC051"
        fixed_target = "0xD861"
        pointer_table = $RabbitStream.pointer_table
        selector = [int]$RabbitStream.selector
        pointer_table_entry = $RabbitStream.pointer_table_entry
        stream_pointer = $RabbitStream.stream_pointer
        record_count = [int]$RabbitStream.record_count
        base_x = [int]$RabbitStream.preview_base_x
        base_y = [int]$RabbitStream.preview_base_y
    }
    stream_records = $StreamRows
    stream_bytes_shown = ([int]$StreamRows.Count * 4)
    first_staged_sprite = New-SpriteSummary -Record $FirstStaged -Kind "first D861 staged record"
    first_visible_sprite = New-SpriteSummary -Record $FirstVisible -Kind "first rabbit record with onscreen Y"
    native_probe = [ordered]@{
        menu_path = "Title Screen -> Enter -> Play Game"
        render_test = ".\\build\\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode first-sprite build\\first_sprite_test.png"
    }
    emulator_probe = [ordered]@{
        lua_script = "tools\\emu_intro_first_sprite_watch.lua"
        compare_first_staged_oam = $null
        compare_first_visible_oam = $null
        output = "build\\emu_intro_first_sprite_watch.ndjson"
    }
}

$Report.emulator_probe.compare_first_staged_oam = ("Y {0}, tile {1}, attr {2}, X {3}" -f `
    $Report.first_staged_sprite.oam.y_hex,
    $Report.first_staged_sprite.oam.tile_hex,
    $Report.first_staged_sprite.oam.attributes_hex,
    $Report.first_staged_sprite.oam.x_hex)
$Report.emulator_probe.compare_first_visible_oam = ("Y {0}, tile {1}, attr {2}, X {3}" -f `
    $Report.first_visible_sprite.oam.y_hex,
    $Report.first_visible_sprite.oam.tile_hex,
    $Report.first_visible_sprite.oam.attributes_hex,
    $Report.first_visible_sprite.oam.x_hex)

$ReportDirectory = Split-Path -Parent $ReportPath
if ($ReportDirectory -and !(Test-Path $ReportDirectory)) {
    New-Item -ItemType Directory -Force -Path $ReportDirectory | Out-Null
}

$Report | ConvertTo-Json -Depth 8 | Set-Content -Path $ReportPath -Encoding UTF8

Write-Host ("FIRST SPRITE TRACE PASS: staged OAM {0}/{1}/{2}/{3}; first visible record {4}" -f `
    $Report.first_staged_sprite.oam.y_hex,
    $Report.first_staged_sprite.oam.tile_hex,
    $Report.first_staged_sprite.oam.attributes_hex,
    $Report.first_staged_sprite.oam.x_hex,
    $Report.first_visible_sprite.record_index)
Write-Host ("Report: {0}" -f $ReportPath)
