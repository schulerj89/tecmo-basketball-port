param(
    [string]$ProjectRoot,
    [string]$DecompRoot,
    [string]$ReportPath,
    [string]$ProbePath,
    [switch]$SkipProbe
)

$ErrorActionPreference = "Stop"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path $ProjectRoot).Path

if (!$ReportPath) {
    $ReportPath = Join-Path $ProjectRoot "build\title_chr_mapping.json"
}

if (!$ProbePath) {
    $ProbePath = Join-Path $ProjectRoot "build\title_mapped_chr_probe.png"
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

function Read-AsmByteValues {
    param([string]$Path)

    $Bytes = New-Object System.Collections.Generic.List[byte]
    foreach ($Line in Get-Content $Path) {
        $Data = $Line.Split(';')[0]
        foreach ($Match in [regex]::Matches($Data, '\$([0-9a-fA-F]{2})(?![0-9a-fA-F])')) {
            $Bytes.Add([byte][Convert]::ToInt32($Match.Groups[1].Value, 16))
        }
    }
    return $Bytes.ToArray()
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

        $CommentMatch = [regex]::Match($Parts[1], '\$([0-9a-fA-F]{4,6})')
        if (!$CommentMatch.Success) {
            continue
        }

        $Data = $Parts[0]
        $Values = @(
            foreach ($Match in [regex]::Matches($Data, '\$([0-9a-fA-F]{2})(?![0-9a-fA-F])')) {
                [int][Convert]::ToInt32($Match.Groups[1].Value, 16)
            }
        )
        if ($Values.Count -eq 0) {
            continue
        }

        $RomAddress = [Convert]::ToInt32($CommentMatch.Groups[1].Value, 16)
        for ($Index = 0; $Index -lt $Values.Count; ++$Index) {
            $CpuAddress = $CpuBase + (($RomAddress + $Index) - $RomBase)
            $Map[$CpuAddress] = $Values[$Index]
        }
    }

    return $Map
}

function Get-MapByte {
    param(
        [hashtable]$Map,
        [int]$Address,
        [string]$SourceName
    )

    if (!$Map.ContainsKey($Address)) {
        throw ("Missing byte for {0}:{1}" -f $SourceName, (Format-HexWord $Address))
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

function Get-TitleCharacterTile {
    param(
        [int]$Code,
        [hashtable]$Bank06
    )

    if ($Code -eq 0x2E -or $Code -eq 0x20) {
        return 0x18
    }
    if ($Code -eq 0x2D) {
        return 0x25
    }
    if ($Code -lt 0x3A) {
        $Value = ($Code - 0x17) -band 0xFF
        if ($Value -lt 0x80) {
            return $Value
        }
    }

    return (Get-MapByte -Map $Bank06 -Address (0xA273 + $Code) -SourceName "bank06")
}

function Get-GlyphTiles {
    param(
        [int]$TileIndex,
        [hashtable]$Bank06
    )

    $Base = 0xAF05 + ($TileIndex * 4)
    return @(
        (Get-MapByte -Map $Bank06 -Address $Base -SourceName "bank06"),
        (Get-MapByte -Map $Bank06 -Address ($Base + 1) -SourceName "bank06"),
        (Get-MapByte -Map $Bank06 -Address ($Base + 2) -SourceName "bank06"),
        (Get-MapByte -Map $Bank06 -Address ($Base + 3) -SourceName "bank06")
    )
}

function Get-TitlePpuAddress {
    param([int]$SourceIndex)

    $RenderX = ($SourceIndex + 0x10) -band 0x1F
    $High = 0x22
    $Column = $RenderX
    if ($RenderX -ge 0x10) {
        $High = $High -bxor 0x04
        $Column = $RenderX - 0x10
    }

    return (($High -shl 8) -bor ($Column * 2))
}

function Write-ProbeImage {
    param(
        [string]$Path,
        [string]$Title,
        [array]$Characters,
        [byte[]]$ChrBytes
    )

    Add-Type -AssemblyName System.Drawing

    $Scale = 2
    $BankCount = [Math]::Floor($ChrBytes.Length / 8192)
    $Width = [Math]::Max(920, ($Title.Length * 16 * $Scale) + 160)
    $RowHeight = 40
    $Height = ($BankCount * $RowHeight) + 26

    $Bitmap = New-Object Drawing.Bitmap $Width, $Height
    $Graphics = [Drawing.Graphics]::FromImage($Bitmap)
    $Font = New-Object Drawing.Font "Consolas", 8
    $Palette = @(
        (New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(0, 0, 0))),
        (New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(80, 88, 104))),
        (New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(176, 184, 196))),
        (New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(250, 252, 255)))
    )

    try {
        $Graphics.Clear([Drawing.Color]::FromArgb(18, 18, 20))
        $Graphics.DrawString(("mapped title: ""{0}""" -f $Title), $Font, [Drawing.Brushes]::White, 4, 2)

        for ($Bank = 0; $Bank -lt $BankCount; ++$Bank) {
            $Y = 18 + ($Bank * $RowHeight)
            $Graphics.DrawString(("bank {0:D2}" -f $Bank), $Font, [Drawing.Brushes]::Gray, 4, $Y + 8)

            for ($CharIndex = 0; $CharIndex -lt $Characters.Count; ++$CharIndex) {
                $Glyph = $Characters[$CharIndex].glyph_tiles
                $X = 72 + ($CharIndex * 16 * $Scale)
                Draw-ChrTile -Graphics $Graphics -Brushes $Palette -ChrBytes $ChrBytes -ChrBank $Bank -Tile $Glyph[0] -X $X -Y $Y -Scale $Scale
                Draw-ChrTile -Graphics $Graphics -Brushes $Palette -ChrBytes $ChrBytes -ChrBank $Bank -Tile $Glyph[1] -X ($X + (8 * $Scale)) -Y $Y -Scale $Scale
                Draw-ChrTile -Graphics $Graphics -Brushes $Palette -ChrBytes $ChrBytes -ChrBank $Bank -Tile $Glyph[2] -X $X -Y ($Y + (8 * $Scale)) -Scale $Scale
                Draw-ChrTile -Graphics $Graphics -Brushes $Palette -ChrBytes $ChrBytes -ChrBank $Bank -Tile $Glyph[3] -X ($X + (8 * $Scale)) -Y ($Y + (8 * $Scale)) -Scale $Scale
            }
        }

        $Dir = Split-Path -Parent $Path
        if ($Dir) {
            New-Item -ItemType Directory -Force -Path $Dir | Out-Null
        }
        $Bitmap.Save($Path, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        foreach ($Brush in $Palette) {
            $Brush.Dispose()
        }
        $Font.Dispose()
        $Graphics.Dispose()
        $Bitmap.Dispose()
    }
}

function Draw-ChrTile {
    param(
        [Drawing.Graphics]$Graphics,
        [array]$Brushes,
        [byte[]]$ChrBytes,
        [int]$ChrBank,
        [int]$Tile,
        [int]$X,
        [int]$Y,
        [int]$Scale
    )

    $TileOffset = ($ChrBank * 8192) + ($Tile * 16)
    if ($TileOffset + 15 -ge $ChrBytes.Length) {
        return
    }

    for ($Row = 0; $Row -lt 8; ++$Row) {
        $Plane0 = $ChrBytes[$TileOffset + $Row]
        $Plane1 = $ChrBytes[$TileOffset + $Row + 8]
        for ($Col = 0; $Col -lt 8; ++$Col) {
            $Bit = 7 - $Col
            $Value = (($Plane0 -shr $Bit) -band 1) -bor (((($Plane1 -shr $Bit) -band 1)) -shl 1)
            $Graphics.FillRectangle($Brushes[$Value], $X + ($Col * $Scale), $Y + ($Row * $Scale), $Scale, $Scale)
        }
    }
}

$LocalDecompRoot = Find-LocalDecompRoot
if (!$LocalDecompRoot) {
    throw "Could not find a local decomp root. Pass -DecompRoot or set TECMO_DECOMP_ROOT."
}

$Bank04TitlePath = Join-Path $LocalDecompRoot "decomp\lifted\bank04\C-0004_bank04_menu_title_text_836B_8384.asm"
$Bank06Path = Join-Path $LocalDecompRoot "build\baseline\Tecmo_06.asm"
$Bank07Path = Join-Path $LocalDecompRoot "build\baseline\Tecmo_07.asm"
$TilesPath = Join-Path $LocalDecompRoot "build\baseline\Tiles.asm"

foreach ($RequiredPath in @($Bank04TitlePath, $Bank06Path, $Bank07Path, $TilesPath)) {
    if (!(Test-Path $RequiredPath)) {
        throw "Required local file not found: $RequiredPath"
    }
}

$Bank06 = Read-BankedByteMap -Path $Bank06Path -BankIndex 6
$Bank07 = Read-BankedByteMap -Path $Bank07Path -BankIndex 7 -CpuBase 0xC000

$TitleBytes = Read-AsmByteValues -Path $Bank04TitlePath
$TitleChars = New-Object System.Collections.Generic.List[string]
foreach ($Byte in $TitleBytes) {
    if ($Byte -lt 0x20 -or $Byte -gt 0x7E) {
        throw ("Title byte is not printable ASCII: {0}" -f (Format-HexByte $Byte))
    }
    $TitleChars.Add([string][char]$Byte)
}
$TitleText = ($TitleChars -join "")

$DispatcherIndex = 0x38
$DispatchBank = Get-MapByte -Map $Bank07 -Address (0xCAF5 + $DispatcherIndex) -SourceName "bank07"
$DispatchLo = Get-MapByte -Map $Bank07 -Address (0xCB33 + $DispatcherIndex) -SourceName "bank07"
$DispatchHi = Get-MapByte -Map $Bank07 -Address (0xCB71 + $DispatcherIndex) -SourceName "bank07"
$DispatchTarget = ($DispatchHi -shl 8) -bor $DispatchLo

$Characters = New-Object System.Collections.Generic.List[object]
for ($Index = 0; $Index -lt $TitleText.Length; ++$Index) {
    $Code = [int][char]$TitleText[$Index]
    $TileIndex = Get-TitleCharacterTile -Code $Code -Bank06 $Bank06
    $GlyphTiles = Get-GlyphTiles -TileIndex $TileIndex -Bank06 $Bank06
    $PpuAddress = Get-TitlePpuAddress -SourceIndex $Index

    $Characters.Add([pscustomobject]@{
        index = $Index
        character = [string][char]$Code
        char_code = Format-HexByte $Code
        render_x = (($Index + 0x10) -band 0x1F)
        ppu_address = Format-HexWord $PpuAddress
        tile_index = Format-HexByte $TileIndex
        glyph_tiles = @($GlyphTiles | ForEach-Object { Format-HexByte $_ })
    })
}

$ProbeWritten = $false
if (!$SkipProbe) {
    $ChrRaw = Read-AsmByteValues -Path $TilesPath
    $ChrBytes = New-Object byte[] (0x40000)
    [Array]::Copy($ChrRaw, $ChrBytes, [Math]::Min($ChrRaw.Length, $ChrBytes.Length))

    $ProbeDir = Split-Path -Parent $ProbePath
    if ($ProbeDir) {
        New-Item -ItemType Directory -Force -Path $ProbeDir | Out-Null
    }
    Write-ProbeImage -Path $ProbePath -Title $TitleText -Characters $Characters -ChrBytes $ChrBytes
    $ProbeWritten = $true
}

$Report = [pscustomobject]@{
    generated_at_utc = [DateTime]::UtcNow.ToString("o")
    target = "Bank 04 title text to CHR glyph mapping"
    decomp_root_used = "<local>"
    safety = [pscustomobject]@{
        generated_report_contains = "derived mapper metadata, tile IDs, chunk IDs, and local-only probe path"
        forbidden_from_commit = "this JSON report, probe PNG, ASM text, ROM bytes, extracted CHR bytes, generated proprietary data, and absolute private paths"
    }
    render_path = @(
        [pscustomobject]@{ chunk = "C-0114"; role = "Bank 04 title loop"; range = '04:$8303-$836A'; note = 'Loads title character, computes X position, dispatches A=0x38 through fixed $C711.' },
        [pscustomobject]@{ chunk = "C-0052"; role = "Bank 06 text render setup"; range = '06:$9E50-$A02C'; note = 'Resolved fixed dispatcher entry 0x38 target.' },
        [pscustomobject]@{ chunk = "C-0060/C-0062/C-0063"; role = "character-to-tile mapping"; range = '06:$A250-$A2CE'; note = 'Maps ASCII-like character values through the baseline operand at $A273,X into tile indices.' },
        [pscustomobject]@{ chunk = "C-0066"; role = "2x2 glyph tile table"; range = '06:$A4CA-$B080'; note = 'Uses $AF05 + tile_index*4 for four tile IDs.' },
        [pscustomobject]@{ chunk = "C-0115/C-0135"; role = "title setup and adjacent pattern tables"; range = '04:$8385-$83AA and 04:$9000-$BFFF'; note = 'C-0115 sets $0352=0x1F and $0100=0x06; the exact $BA16 entry sets the $05B6 update flag bit, while adjacent pattern setup and queued palette effects remain unresolved.' }
    )
    dispatcher = [pscustomobject]@{
        fixed_helper = '$C711'
        call_index = Format-HexByte $DispatcherIndex
        resolved_bank = Format-HexByte $DispatchBank
        resolved_cpu_address = Format-HexWord $DispatchTarget
        expected_target = '06:$9E50'
        matches_expected_target = (($DispatchBank -eq 0x06) -and ($DispatchTarget -eq 0x9E50))
    }
    title_setup = [pscustomobject]@{
        source_chunk = "C-0115"
        chr_config_0100 = "0x06"
        setup_selector_0352 = "0x1F"
        setup_helper = '04:$BA16'
        update_flags_or_05b6 = "0x01"
        status = "exact BA16 update flag is modeled; adjacent pattern/VRAM setup and resolved fixed-helper palette queue decode still need mapping before raw CHR bank rendering is pixel-accurate"
    }
    title_text_length = $TitleText.Length
    characters = $Characters
    probe = [pscustomobject]@{
        written = $ProbeWritten
        local_path = if ($ProbeWritten) { "build\title_mapped_chr_probe.png" } else { $null }
        note = "Probe renders mapped 2x2 glyph tile IDs against every raw CHR bank; it is intentionally ignored because it contains derived original graphics."
    }
    conclusions = @(
        "Direct title bytes are not direct CHR tile IDs.",
        'C711 dispatch for A=0x38 resolves to Bank 06 text setup at 06:$9E50.',
        'The title glyph path maps characters through 06:$A290, uses the baseline $A273,X lookup operand, and reads four tile IDs from the 06:$AF05 table.',
        "Raw CHR-bank probing is still not pixel-accurate until adjacent Bank 04 pattern setup and resolved fixed-helper queued palette effects are modeled."
    )
    next_steps = @(
        "Map the adjacent Bank 04 pattern setup tables and resolved fixed helper bodies into explicit native pattern-table/VRAM state.",
        "Decode resolved fixed-helper/queued PPU palette initialization and any palette animation state.",
        "Promote the original-title quick launch from diagnostic CHR rendering after pattern and palette state are mapped."
    )
}

$ReportDir = Split-Path -Parent $ReportPath
if ($ReportDir) {
    New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null
}
$Report | ConvertTo-Json -Depth 8 | Set-Content -Path $ReportPath -Encoding ASCII

[pscustomobject]@{
    title_length = $TitleText.Length
    dispatcher = ("{0} -> bank {1} {2}" -f (Format-HexByte $DispatcherIndex), (Format-HexByte $DispatchBank), (Format-HexWord $DispatchTarget))
    report = $ReportPath
    probe = if ($ProbeWritten) { $ProbePath } else { "<skipped>" }
    next_gate = "Decode adjacent pattern setup and resolved helper palette queue"
} | Format-List
