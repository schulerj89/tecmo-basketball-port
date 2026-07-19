param(
    [string]$ExePath,
    [string]$ProjectRoot,
    [string]$ShortcutName = "Tecmo Basketball Native Port.lnk",
    [string]$ShortcutPath
)

$ErrorActionPreference = "Stop"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
if (!$ExePath) {
    $ExePath = Join-Path $ProjectRoot "build\tecmo_port_game.exe"
}

$ProjectRoot = (Resolve-Path $ProjectRoot).Path
$ExePath = (Resolve-Path $ExePath).Path
$BuildDir = Split-Path -Parent $ExePath
$IconPath = Join-Path $BuildDir "tecmo_port.ico"

function New-TecmoPortIcon {
    param([string]$Path)

    Add-Type -AssemblyName System.Drawing

    $bitmap = New-Object System.Drawing.Bitmap 256, 256
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit

    $bg = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        (New-Object System.Drawing.Rectangle 0, 0, 256, 256),
        [System.Drawing.Color]::FromArgb(18, 22, 28),
        [System.Drawing.Color]::FromArgb(52, 34, 38),
        [System.Drawing.Drawing2D.LinearGradientMode]::ForwardDiagonal
    )
    $orange = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(223, 104, 35))
    $orangeLight = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(248, 158, 78))
    $cream = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(250, 246, 220))
    $shadow = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(80, 0, 0, 0))
    $linePen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(62, 35, 24), 10)
    $ringPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(250, 244, 212), 8)
    $font = New-Object System.Drawing.Font([System.Drawing.FontFamily]::GenericSansSerif, 58, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
    $smallFont = New-Object System.Drawing.Font([System.Drawing.FontFamily]::GenericSansSerif, 25, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
    $format = New-Object System.Drawing.StringFormat
    $format.Alignment = [System.Drawing.StringAlignment]::Center
    $format.LineAlignment = [System.Drawing.StringAlignment]::Center

    $graphics.FillRectangle($bg, 0, 0, 256, 256)
    $graphics.FillEllipse($shadow, 38, 44, 188, 190)
    $graphics.FillEllipse($orange, 28, 24, 196, 196)
    $graphics.FillEllipse($orangeLight, 58, 42, 82, 58)
    $graphics.DrawEllipse($ringPen, 28, 24, 196, 196)
    $graphics.DrawLine($linePen, 126, 26, 126, 220)
    $graphics.DrawLine($linePen, 32, 122, 222, 122)
    $graphics.DrawArc($linePen, -24, 26, 168, 194, 292, 136)
    $graphics.DrawArc($linePen, 112, 26, 168, 194, 112, 136)

    $textRect = New-Object System.Drawing.RectangleF 0, 72, 256, 86
    $graphics.DrawString("TNB", $font, $shadow, $textRect, $format)
    $graphics.DrawString("TNB", $font, $cream, $textRect, $format)
    $smallRect = New-Object System.Drawing.RectangleF 0, 150, 256, 38
    $graphics.DrawString("PORT", $smallFont, $shadow, $smallRect, $format)
    $graphics.DrawString("PORT", $smallFont, $cream, $smallRect, $format)

    $width = $bitmap.Width
    $height = $bitmap.Height
    $xorBytes = New-Object byte[] ($width * $height * 4)
    $offset = 0
    for ($y = $height - 1; $y -ge 0; --$y) {
        for ($x = 0; $x -lt $width; ++$x) {
            $pixel = $bitmap.GetPixel($x, $y)
            $xorBytes[$offset++] = $pixel.B
            $xorBytes[$offset++] = $pixel.G
            $xorBytes[$offset++] = $pixel.R
            $xorBytes[$offset++] = $pixel.A
        }
    }

    $maskStride = [int][Math]::Floor(($width + 31) / 32) * 4
    $maskBytes = New-Object byte[] ($maskStride * $height)
    $dibBytesLength = 40 + $xorBytes.Length + $maskBytes.Length

    $fileStream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
    $writer = New-Object System.IO.BinaryWriter($fileStream)
    try {
        $writer.Write([UInt16]0)
        $writer.Write([UInt16]1)
        $writer.Write([UInt16]1)
        $writer.Write([byte]0)
        $writer.Write([byte]0)
        $writer.Write([byte]0)
        $writer.Write([byte]0)
        $writer.Write([UInt16]1)
        $writer.Write([UInt16]32)
        $writer.Write([UInt32]$dibBytesLength)
        $writer.Write([UInt32]22)
        $writer.Write([UInt32]40)
        $writer.Write([Int32]$width)
        $writer.Write([Int32]($height * 2))
        $writer.Write([UInt16]1)
        $writer.Write([UInt16]32)
        $writer.Write([UInt32]0)
        $writer.Write([UInt32]$xorBytes.Length)
        $writer.Write([Int32]0)
        $writer.Write([Int32]0)
        $writer.Write([UInt32]0)
        $writer.Write([UInt32]0)
        $writer.Write($xorBytes)
        $writer.Write($maskBytes)
    } finally {
        $writer.Dispose()
        $fileStream.Dispose()
        $graphics.Dispose()
        $bitmap.Dispose()
        $bg.Dispose()
        $orange.Dispose()
        $orangeLight.Dispose()
        $cream.Dispose()
        $shadow.Dispose()
        $linePen.Dispose()
        $ringPen.Dispose()
        $font.Dispose()
        $smallFont.Dispose()
        $format.Dispose()
    }
}

New-TecmoPortIcon -Path $IconPath

if (!$ShortcutPath) {
    $Desktop = [Environment]::GetFolderPath("Desktop")
    $ShortcutPath = Join-Path $Desktop $ShortcutName
}
$ShortcutPath = [System.IO.Path]::GetFullPath($ShortcutPath)
$ShortcutDirectory = Split-Path -Parent $ShortcutPath
if (!(Test-Path -LiteralPath $ShortcutDirectory -PathType Container)) {
    throw "Shortcut directory was not found: $ShortcutDirectory"
}
$Arguments = "--root `"$ProjectRoot`" --play"

$Shell = New-Object -ComObject WScript.Shell
$Shortcut = $Shell.CreateShortcut($ShortcutPath)
$Shortcut.TargetPath = $ExePath
$Shortcut.Arguments = $Arguments
$Shortcut.WorkingDirectory = $ProjectRoot
$Shortcut.IconLocation = "$IconPath,0"
$Shortcut.Description = "Launch the local-only Tecmo Basketball native port prototype."
$Shortcut.Save()

Write-Host "Updated shortcut: $ShortcutPath"
