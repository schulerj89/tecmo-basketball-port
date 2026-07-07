param(
    [string]$ProjectRoot,
    [string]$DecompRoot,
    [string]$ReportPath,
    [string]$PreviewPath,
    [int]$ChrBank = 31
)

$ErrorActionPreference = "Stop"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path $ProjectRoot).Path

if (!$ReportPath) {
    $ReportPath = Join-Path $ProjectRoot "build\intro_composite_trace.json"
}
if (!$PreviewPath) {
    $PreviewPath = Join-Path $ProjectRoot "build\intro_composite_trace_preview.png"
}
if (![System.IO.Path]::IsPathRooted($ReportPath)) {
    $ReportPath = Join-Path $ProjectRoot $ReportPath
}
if (![System.IO.Path]::IsPathRooted($PreviewPath)) {
    $PreviewPath = Join-Path $ProjectRoot $PreviewPath
}
$ReportPath = [System.IO.Path]::GetFullPath($ReportPath)
$PreviewPath = [System.IO.Path]::GetFullPath($PreviewPath)

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
    (Join-Path $LocalDecompRoot "build\out\tecmo_basketball_split_bank01.nes"),
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
    throw "Local image is not an iNES file."
}

$PrgBankCount = [int]$Rom[4]
$ChrBankCount = [int]$Rom[5]
$TrainerBytes = if (($Rom[6] -band 0x04) -ne 0) { 512 } else { 0 }
$PrgStart = 16 + $TrainerBytes
$PrgBankSize = 0x4000
$ChrBankSize = 0x2000
$ChrStart = $PrgStart + $PrgBankCount * $PrgBankSize

if ($PrgBankCount -lt 8) {
    throw "Expected at least 8 PRG banks; found $PrgBankCount."
}
if ($ChrBank -lt 0 -or $ChrBank -ge $ChrBankCount) {
    throw "CHR bank $ChrBank is outside this ROM's declared bank range 0-$($ChrBankCount - 1)."
}
if ($Rom.Length -lt ($ChrStart + $ChrBankCount * $ChrBankSize)) {
    throw "ROM is too short for its declared CHR bank count."
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

    $Low = Get-PrgByte $Bank $CpuAddress
    $High = Get-PrgByte $Bank ($CpuAddress + 1)
    return $Low -bor ($High -shl 8)
}

function Get-FixedPrgByte {
    param([int]$CpuAddress)

    if ($CpuAddress -lt 0xC000 -or $CpuAddress -gt 0xFFFF) {
        throw ("Fixed CPU address must be in C000-FFFF: {0}" -f (Format-HexWord $CpuAddress))
    }

    return Get-PrgByte ($PrgBankCount - 1) (0x8000 + ($CpuAddress - 0xC000))
}

function Get-IntroGlyphCode {
    param([char]$Char)

    if ($Char -ge [char]'0' -and $Char -le [char]'9') {
        return 0x82 + ([int]$Char - [int][char]'0')
    }
    if ($Char -ge [char]'A' -and $Char -le [char]'D') {
        return 0x8C + ([int]$Char - [int][char]'A')
    }
    if ($Char -ge [char]'E' -and $Char -le [char]'T') {
        return 0x90 + ([int]$Char - [int][char]'E')
    }
    if ($Char -ge [char]'U' -and $Char -le [char]'Z') {
        return 0xA0 + ([int]$Char - [int][char]'U')
    }
    return $null
}

function Convert-IntroTextToGlyphCodes {
    param([string]$Text)

    $Codes = New-Object System.Collections.Generic.List[int]
    foreach ($Char in $Text.ToCharArray()) {
        $Code = Get-IntroGlyphCode $Char
        if ($null -eq $Code) {
            return @()
        }
        $Codes.Add($Code)
    }
    return @($Codes.ToArray())
}

function Find-PrgSequence {
    param(
        [int]$Bank,
        [int[]]$Sequence
    )

    if ($Sequence.Count -eq 0) {
        return $null
    }

    for ($CpuAddress = 0x8000; $CpuAddress -le (0xBFFF - $Sequence.Count + 1); ++$CpuAddress) {
        $Matches = $true
        for ($Index = 0; $Index -lt $Sequence.Count; ++$Index) {
            if ((Get-PrgByte $Bank ($CpuAddress + $Index)) -ne $Sequence[$Index]) {
                $Matches = $false
                break
            }
        }
        if ($Matches) {
            return $CpuAddress
        }
    }

    return $null
}

function Get-PointerTableEntryCount {
    param([int]$PointerTable)

    $FirstStreamPointer = Get-PrgWord 0 $PointerTable
    $Span = $FirstStreamPointer - $PointerTable
    if ($Span -le 0 -or ($Span % 2) -ne 0) {
        return 0
    }
    return [int]($Span / 2)
}

function Read-StreamRecords {
    param(
        [string]$Component,
        [int]$PointerTable,
        [int]$Selector,
        [string]$Driver,
        [int]$BaseX,
        [int]$BaseY,
        [string]$BaseXExpression,
        [string]$BaseYExpression
    )

    $StreamPointer = Get-PrgWord 0 ($PointerTable + $Selector * 2)
    $RecordCount = Get-PrgByte 0 $StreamPointer
    $Records = New-Object System.Collections.Generic.List[object]

    for ($RecordIndex = 0; $RecordIndex -lt $RecordCount; ++$RecordIndex) {
        $RecordAddress = $StreamPointer + 1 + $RecordIndex * 4
        $RawY = Get-PrgByte 0 $RecordAddress
        $RawTile = Get-PrgByte 0 ($RecordAddress + 1)
        $RawAttributes = Get-PrgByte 0 ($RecordAddress + 2)
        $RawX = Get-PrgByte 0 ($RecordAddress + 3)
        $OamTileLow = ($RawTile + 1) -band 0xFF
        $PairTop = $OamTileLow -band 0xFE
        $PairBottom = $PairTop -bor 0x01
        $RelativeX = Convert-SignedByte $RawX
        $RelativeY = Convert-SignedByte $RawY
        $ScreenX = $BaseX + $RelativeX
        $ScreenY = $BaseY + $RelativeY
        $Role = "stream_record"
        if ($Component -eq "rabbit_full_stream" -or $Component.StartsWith("a7db_")) {
            $Role = "rabbit_full_stream"
            if ($OamTileLow -ge 0x25 -and $OamTileLow -le 0x2B) {
                $Role = "rabbit_head_or_upper_candidate"
            }
        } elseif ($Component -eq "tecmo_selector0" -or $Component.StartsWith("a90f_")) {
            if ($OamTileLow -ge 0x80 -and $OamTileLow -le 0x93) {
                $Role = "tecmo_logo_candidate"
            } else {
                $Role = "same_a90f_stream_other"
            }
        }

        $Records.Add([pscustomobject][ordered]@{
            component = $Component
            role = $Role
            record_index = $RecordIndex
            stream_record_address = ("bank00:{0}" -f (Format-HexWord $RecordAddress))
            raw_tile_before_offset_hex = (Format-HexByte $RawTile)
            oam_tile_low_after_offset_hex = (Format-HexByte $OamTileLow)
            table1_pair_top_hex = (Format-HexWord (0x100 + $PairTop))
            table1_pair_bottom_hex = (Format-HexWord (0x100 + $PairBottom))
            attributes_hex = (Format-HexByte $RawAttributes)
            relative_x = $RelativeX
            relative_y = $RelativeY
            screen_x_from_current_base = $ScreenX
            screen_y_from_current_base = $ScreenY
        })
    }

    return [pscustomobject][ordered]@{
        component = $Component
        driver = $Driver
        pointer_table = ("bank00:{0}" -f (Format-HexWord $PointerTable))
        selector = $Selector
        pointer_table_entry = ("bank00:{0}" -f (Format-HexWord ($PointerTable + $Selector * 2)))
        stream_pointer = ("bank00:{0}" -f (Format-HexWord $StreamPointer))
        record_count = $RecordCount
        base_x_expression = $BaseXExpression
        base_y_expression = $BaseYExpression
        preview_base_x = $BaseX
        preview_base_y = $BaseY
        records = @($Records.ToArray())
    }
}

function Get-AttributeHistogram {
    param([object[]]$Records)

    $Histogram = [ordered]@{}
    foreach ($Record in $Records) {
        $Key = $Record.attributes_hex
        if ($Histogram.Contains($Key)) {
            $Histogram[$Key] = $Histogram[$Key] + 1
        } else {
            $Histogram[$Key] = 1
        }
    }
    return $Histogram
}

function Get-StreamSummary {
    param([object]$Stream)

    $Records = @($Stream.records)
    $Pairs = @(
        $Records |
            ForEach-Object { $_.table1_pair_top_hex; $_.table1_pair_bottom_hex } |
            Sort-Object -Unique
    )
    $LogoHits = @($Records | Where-Object { $_.role -eq "tecmo_logo_candidate" })
    $RabbitHits = @($Records | Where-Object { $_.role -eq "rabbit_head_or_upper_candidate" })
    $Bounds = Get-Bounds $Records

    return [ordered]@{
        selector = $Stream.selector
        pointer_table_entry = $Stream.pointer_table_entry
        stream_pointer = $Stream.stream_pointer
        record_count = $Stream.record_count
        tecmo_logo_hit_count = $LogoHits.Count
        rabbit_head_candidate_count = $RabbitHits.Count
        bounds_from_current_base = $Bounds
        attribute_histogram = Get-AttributeHistogram $Records
        unique_table1_pairs = $Pairs
    }
}

function Read-PointerTableStreams {
    param(
        [string]$ComponentPrefix,
        [int]$PointerTable,
        [int]$EntryCount,
        [string]$Driver
    )

    $Streams = New-Object System.Collections.Generic.List[object]
    for ($Selector = 0; $Selector -lt $EntryCount; ++$Selector) {
        $Streams.Add((Read-StreamRecords `
            -Component ("{0}_selector{1}" -f $ComponentPrefix, $Selector) `
            -PointerTable $PointerTable `
            -Selector $Selector `
            -Driver $Driver `
            -BaseX 0 `
            -BaseY 0 `
            -BaseXExpression "summary origin" `
            -BaseYExpression "summary origin"))
    }
    return @($Streams.ToArray())
}

$RabbitSeedX = Get-PrgByte 4 0x89BE
$RabbitSelector = Get-PrgByte 4 0x89C0
$RabbitBaseY = Get-PrgByte 4 0x8985
$A7DbEntryCount = Get-PointerTableEntryCount 0xA7DB
$A90FEntryCount = Get-PointerTableEntryCount 0xA90F
$A7DbStreams = Read-PointerTableStreams `
    -ComponentPrefix "a7db" `
    -PointerTable 0xA7DB `
    -EntryCount $A7DbEntryCount `
    -Driver "bank04:L8988 selector sweep"
$A90FStreams = Read-PointerTableStreams `
    -ComponentPrefix "a90f" `
    -PointerTable 0xA90F `
    -EntryCount $A90FEntryCount `
    -Driver "bank04:L8818/L8645 selector sweep"
$RabbitStream = Read-StreamRecords `
    -Component "rabbit_full_stream" `
    -PointerTable 0xA7DB `
    -Selector $RabbitSelector `
    -Driver 'bank04:L88E7 seeds $07EC/$21, then bank04:L8988 calls C051/D861 with A7DB' `
    -BaseX $RabbitSeedX `
    -BaseY $RabbitBaseY `
    -BaseXExpression 'bank04:89BE, copied to $09 during L8988 pass X=1' `
    -BaseYExpression 'bank04:8985, copied to $07EC then $0B during L8988 pass X=1'

$TecmoBaseX = 0x62
$TecmoBaseY = 0x5C
$TecmoStream = Read-StreamRecords `
    -Component "tecmo_selector0" `
    -PointerTable 0xA90F `
    -Selector 0 `
    -Driver 'bank04:L8818 computes $09 = #$62 - $88 and calls C051/D861 with A90F selector 0' `
    -BaseX $TecmoBaseX `
    -BaseY $TecmoBaseY `
    -BaseXExpression 'bank04:L8818 #$62 - runtime $88; preview uses $88=0 until runtime capture pins this frame' `
    -BaseYExpression 'bank04:L8818 literal #$5C'

$A7DbSelector0Stream = @($A7DbStreams | Where-Object { $_.selector -eq 0 })[0]
$A90FSelector1Stream = @($A90FStreams | Where-Object { $_.selector -eq 1 })[0]
$A7DbSelector0Records = @($A7DbSelector0Stream.records)
$A90FSelector1Records = @($A90FSelector1Stream.records)
$RabbitRecords = @($RabbitStream.records)
$TecmoLogoRecords = @($TecmoStream.records | Where-Object { $_.role -eq "tecmo_logo_candidate" })
$TecmoAllRecords = @($TecmoStream.records)

function Get-TileIndexFromHex {
    param([string]$Hex)
    return [Convert]::ToInt32($Hex, 16)
}

function Get-Bounds {
    param([object[]]$Records)

    $MinX = 0
    $MinY = 0
    $MaxX = 0
    $MaxY = 0
    if ($Records.Count -gt 0) {
        $MinX = ($Records | Measure-Object -Property screen_x_from_current_base -Minimum).Minimum
        $MinY = ($Records | Measure-Object -Property screen_y_from_current_base -Minimum).Minimum
        $MaxX = ($Records | ForEach-Object { $_.screen_x_from_current_base + 8 } | Measure-Object -Maximum).Maximum
        $MaxY = ($Records | ForEach-Object { $_.screen_y_from_current_base + 16 } | Measure-Object -Maximum).Maximum
    }
    return [pscustomobject]@{
        min_x = [int]$MinX
        min_y = [int]$MinY
        max_x = [int]$MaxX
        max_y = [int]$MaxY
        width = [int]($MaxX - $MinX)
        height = [int]($MaxY - $MinY)
    }
}

Add-Type -AssemblyName System.Drawing

$PreviewWidth = 1180
$PreviewHeight = 900
$Bitmap = New-Object System.Drawing.Bitmap $PreviewWidth, $PreviewHeight
$Graphics = [System.Drawing.Graphics]::FromImage($Bitmap)
$Graphics.Clear([System.Drawing.Color]::FromArgb(255, 8, 8, 12))
$Font = New-Object System.Drawing.Font "Consolas", 10
$BrushText = [System.Drawing.Brushes]::White
$BrushMuted = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 160, 174, 184))
$PenPanel = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 86, 102, 118))

$PaletteSets = @(
    @(
        [System.Drawing.Color]::Transparent,
        [System.Drawing.Color]::FromArgb(255, 104, 78, 82),
        [System.Drawing.Color]::FromArgb(255, 236, 92, 126),
        [System.Drawing.Color]::FromArgb(255, 255, 244, 214)
    ),
    @(
        [System.Drawing.Color]::Transparent,
        [System.Drawing.Color]::FromArgb(255, 88, 96, 112),
        [System.Drawing.Color]::FromArgb(255, 178, 194, 210),
        [System.Drawing.Color]::FromArgb(255, 252, 252, 244)
    ),
    @(
        [System.Drawing.Color]::Transparent,
        [System.Drawing.Color]::FromArgb(255, 92, 54, 42),
        [System.Drawing.Color]::FromArgb(255, 222, 116, 64),
        [System.Drawing.Color]::FromArgb(255, 255, 232, 184)
    ),
    @(
        [System.Drawing.Color]::Transparent,
        [System.Drawing.Color]::FromArgb(255, 58, 70, 92),
        [System.Drawing.Color]::FromArgb(255, 132, 154, 190),
        [System.Drawing.Color]::FromArgb(255, 230, 238, 255)
    )
)

function Draw-ChrTile {
    param(
        [System.Drawing.Bitmap]$Target,
        [int]$Tile,
        [int]$X,
        [int]$Y,
        [int]$Scale,
        [int]$PaletteIndex,
        [bool]$FlipHorizontal,
        [bool]$FlipVertical
    )

    $TileOffset = $ChrStart + $ChrBank * $ChrBankSize + $Tile * 16
    if ($Tile -lt 0 -or $Tile -gt 0x1FF -or $TileOffset + 15 -ge $Rom.Length) {
        return
    }
    $Palette = $PaletteSets[$PaletteIndex -band 3]
    for ($Row = 0; $Row -lt 8; ++$Row) {
        $SourceRow = if ($FlipVertical) { 7 - $Row } else { $Row }
        $Plane0 = [int]$Rom[$TileOffset + $SourceRow]
        $Plane1 = [int]$Rom[$TileOffset + $SourceRow + 8]
        for ($Col = 0; $Col -lt 8; ++$Col) {
            $SourceCol = if ($FlipHorizontal) { $Col } else { 7 - $Col }
            $Value = (($Plane0 -shr $SourceCol) -band 1) -bor (((($Plane1 -shr $SourceCol) -band 1) -shl 1))
            if ($Value -eq 0) {
                continue
            }
            $Color = $Palette[$Value]
            for ($Dy = 0; $Dy -lt $Scale; ++$Dy) {
                for ($Dx = 0; $Dx -lt $Scale; ++$Dx) {
                    $Px = $X + $Col * $Scale + $Dx
                    $Py = $Y + $Row * $Scale + $Dy
                    if ($Px -ge 0 -and $Px -lt $Target.Width -and $Py -ge 0 -and $Py -lt $Target.Height) {
                        $Target.SetPixel($Px, $Py, $Color)
                    }
                }
            }
        }
    }
}

function Draw-SpriteRecord {
    param(
        [System.Drawing.Bitmap]$Target,
        [object]$Record,
        [int]$X,
        [int]$Y,
        [int]$Scale
    )

    $Attributes = [Convert]::ToInt32($Record.attributes_hex, 16)
    $PaletteIndex = $Attributes -band 3
    $FlipHorizontal = ($Attributes -band 0x40) -ne 0
    $FlipVertical = ($Attributes -band 0x80) -ne 0
    $TopTile = Get-TileIndexFromHex $Record.table1_pair_top_hex
    $BottomTile = Get-TileIndexFromHex $Record.table1_pair_bottom_hex
    $Tiles = if ($FlipVertical) { @($BottomTile, $TopTile) } else { @($TopTile, $BottomTile) }
    Draw-ChrTile $Target $Tiles[0] $X $Y $Scale $PaletteIndex $FlipHorizontal $FlipVertical
    Draw-ChrTile $Target $Tiles[1] $X ($Y + 8 * $Scale) $Scale $PaletteIndex $FlipHorizontal $FlipVertical
}

function Draw-ComponentPanel {
    param(
        [string]$Title,
        [object[]]$Records,
        [int]$PanelX,
        [int]$PanelY,
        [int]$PanelW,
        [int]$PanelH,
        [int]$Scale
    )

    $Graphics.DrawRectangle($PenPanel, $PanelX, $PanelY, $PanelW, $PanelH)
    $Graphics.DrawString($Title, $Font, $BrushText, $PanelX + 12, $PanelY + 10)
    if ($Records.Count -eq 0) {
        $Graphics.DrawString("NO RECORDS", $Font, $BrushMuted, $PanelX + 12, $PanelY + 32)
        return
    }

    $Bounds = Get-Bounds $Records
    $DrawX = $PanelX + 22
    $DrawY = $PanelY + 48
    $Graphics.DrawString(("records {0}  bounds {1}x{2}" -f $Records.Count, $Bounds.width, $Bounds.height), $Font, $BrushMuted, $PanelX + 12, $PanelY + 30)

    foreach ($Record in $Records) {
        $X = $DrawX + (($Record.screen_x_from_current_base - $Bounds.min_x) * $Scale)
        $Y = $DrawY + (($Record.screen_y_from_current_base - $Bounds.min_y) * $Scale)
        Draw-SpriteRecord $Bitmap $Record $X $Y $Scale
    }
}

Draw-ComponentPanel "A7DB SEL1 FULL RABBIT STREAM" $RabbitRecords 24 28 360 260 3
Draw-ComponentPanel "A7DB SEL0 PARTNER STREAM" $A7DbSelector0Records 410 28 330 260 3
Draw-ComponentPanel "A90F SEL0 TECMO LOGO HITS" $TecmoLogoRecords 766 28 390 260 3
Draw-ComponentPanel "A90F SEL1 L8645 STREAM" $A90FSelector1Records 24 322 360 240 3
Draw-ComponentPanel "A90F SEL0 FULL STREAM CONTEXT" $TecmoAllRecords 410 322 746 360 2
$Graphics.DrawString(("LOCAL PRIVATE CHR PREVIEW - BANK {0} - DO NOT COMMIT GENERATED PNG" -f $ChrBank), $Font, $BrushMuted, 24, 862)

$PreviewDir = Split-Path -Parent $PreviewPath
if ($PreviewDir) {
    New-Item -ItemType Directory -Force -Path $PreviewDir | Out-Null
}
$Bitmap.Save($PreviewPath, [System.Drawing.Imaging.ImageFormat]::Png)
$Graphics.Dispose()
$Bitmap.Dispose()
$Font.Dispose()
$BrushMuted.Dispose()
$PenPanel.Dispose()

$RabbitPairs = @($RabbitRecords | ForEach-Object { $_.table1_pair_top_hex; $_.table1_pair_bottom_hex } | Sort-Object -Unique)
$TecmoPairs = @($TecmoLogoRecords | ForEach-Object { $_.table1_pair_top_hex; $_.table1_pair_bottom_hex } | Sort-Object -Unique)
$L88E7PaletteBytes = @()
for ($PaletteIndex = 0; $PaletteIndex -lt 16; ++$PaletteIndex) {
    $L88E7PaletteBytes += (Format-HexByte (Get-PrgByte 4 (0x89DD + $PaletteIndex)))
}
$L88E7IrqVectorIndex = 0x05
$L88E7IrqVectorLow = Get-FixedPrgByte (0xCDF2 + $L88E7IrqVectorIndex)
$L88E7IrqVectorHigh = Get-FixedPrgByte (0xCDFA + $L88E7IrqVectorIndex)
$L88E7IrqVectorTarget = $L88E7IrqVectorLow -bor ($L88E7IrqVectorHigh -shl 8)
$PresentsText = "PRESENTS"
$PresentsGlyphCodes = Convert-IntroTextToGlyphCodes $PresentsText
$PresentsCpu = Find-PrgSequence -Bank 0 -Sequence $PresentsGlyphCodes

$Report = [ordered]@{
    schema_version = 1
    generated_at_utc = [DateTime]::UtcNow.ToString("o")
    data_policy = "Local-only composite intro trace. Do not commit generated report or preview; they contain decoded private sprite rows and rendered CHR-derived graphics."
    source_image = "<local private decomp build>"
    private_paths_included = $false
    generated_preview = "build/intro_composite_trace_preview.png"
    interpreted_render_context = [ordered]@{
        chr_bank = $ChrBank
        chr_table = 1
        tile_offset_from_d861 = 1
        note = "Preview renders local CHR bank $ChrBank/table 1 as 8x16 sprite pairs. Palette and exact runtime base X for L8818 still need emulator/state capture."
    }
    l88e7_setup = [ordered]@{
        driver = "bank04:L88E7 first intro seed path"
        c05a_effect = "fixed $C05A/$D700 copies a 16-byte palette snapshot into $033E-$034D and low nibbles into $031E-$032D"
        palette_snapshot_cpu = "bank04:89DD"
        palette_snapshot_bytes_hex = $L88E7PaletteBytes
        seed_88_hex = "A8"
        seed_8a_hex = "3C"
        seed_0352_hex = "01"
        seed_0100_hex = "05"
        nmi_irq_vector = [ordered]@{
            consumer = 'fixed bank NMI tail $CDAC'
            mapper_bank_select_from_0352_hex = "01"
            vector_index_from_0100_hex = "05"
            vector_table_low_cpu = "fixed:CDF2"
            vector_table_high_cpu = "fixed:CDFA"
            vector_target_cpu = ("fixed:{0}" -f (Format-HexWord $L88E7IrqVectorTarget))
            effect = "selects IRQ/scanline CHR split handler; this is not the PRESENTS text/layout stream"
        }
        presents_data_lead = [ordered]@{
            source = "bank00:C-0191 normal-letter text/layout stream"
            text = $PresentsText
            match_found = ($null -ne $PresentsCpu)
            presents_match_cpu = if ($null -ne $PresentsCpu) { ("bank00:{0}" -f (Format-HexWord $PresentsCpu)) } else { $null }
            note = "This is a data lead for the normal-letter PRESENTS glyph sequence; the stream interpreter and final placement are still the next decode target."
        }
        stream0 = [ordered]@{
            pointer_table = "bank00:A7DB"
            selector = 0
            base_x_hex = "00"
            base_y_hex = "0000"
            record_count = $A7DbSelector0Stream.record_count
        }
        stream1 = [ordered]@{
            pointer_table = "bank00:A7DB"
            selector = $RabbitSelector
            base_x_hex = "B8"
            base_y_hex = "011E"
            record_count = $RabbitStream.record_count
        }
    }
    safe_summary = [ordered]@{
        rabbit_driver = "bank04:L88E7 -> bank04:L8988 -> C051/D861"
        rabbit_pointer_table = "bank00:A7DB"
        rabbit_pointer_table_inferred_entry_count = $A7DbEntryCount
        rabbit_selector = $RabbitSelector
        rabbit_stream_pointer = $RabbitStream.stream_pointer
        rabbit_record_count = $RabbitStream.record_count
        rabbit_unique_table1_pairs = $RabbitPairs
        tecmo_driver = "bank04:L8818 -> C051/D861"
        tecmo_pointer_table = "bank00:A90F"
        tecmo_pointer_table_inferred_entry_count = $A90FEntryCount
        tecmo_selector = 0
        tecmo_stream_pointer = $TecmoStream.stream_pointer
        tecmo_record_count = $TecmoStream.record_count
        tecmo_logo_hit_count = $TecmoLogoRecords.Count
        tecmo_logo_table1_pairs = $TecmoPairs
        a7db_selector_summaries = @($A7DbStreams | ForEach-Object { Get-StreamSummary $_ })
        a90f_selector_summaries = @($A90FStreams | ForEach-Object { Get-StreamSummary $_ })
        unresolved = @(
            'Capture runtime $88 at the exact L8818 call used by the first TECMO PRESENTS frame.',
            "Map D861 output bytes to final OAM Y semantics and palette selection.",
            "Confirm whether non-logo A90F selector 0 records are PRESENTS/registered-mark/supporting intro graphics.",
            "Resolve live base positions for the L8645 A90F selector 1 callers in setup, loop, and transition paths."
        )
    }
    primary_streams = @($RabbitStream, $TecmoStream)
    selector_sweep_streams = @($A7DbStreams + $A90FStreams)
}

$ReportDir = Split-Path -Parent $ReportPath
if ($ReportDir) {
    New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null
}
$Report | ConvertTo-Json -Depth 12 | Set-Content -Path $ReportPath -Encoding ASCII

Write-Host "Wrote intro composite trace report: $ReportPath"
Write-Host "Wrote intro composite CHR preview: $PreviewPath"
