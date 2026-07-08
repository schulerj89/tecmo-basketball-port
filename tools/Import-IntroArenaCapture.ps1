param(
    [string]$ProjectRoot,
    [string]$MemoryLog,
    [string]$ArenaLog,
    [string]$OutputPath
)

$ErrorActionPreference = "Stop"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path $ProjectRoot).Path

if (!$MemoryLog) {
    $MemoryLog = Join-Path $ProjectRoot "build\emu_intro_memory_watch.ndjson"
}
if (!$ArenaLog) {
    $ArenaLog = Join-Path $ProjectRoot "build\emu_intro_arena_irq_watch.ndjson"
}
if (!$OutputPath) {
    $OutputPath = Join-Path $ProjectRoot "build\intro_arena_capture.ndjson"
}

if (![System.IO.Path]::IsPathRooted($MemoryLog)) {
    $MemoryLog = Join-Path $ProjectRoot $MemoryLog
}
if (![System.IO.Path]::IsPathRooted($ArenaLog)) {
    $ArenaLog = Join-Path $ProjectRoot $ArenaLog
}
if (![System.IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath = Join-Path $ProjectRoot $OutputPath
}

$MemoryLog = [System.IO.Path]::GetFullPath($MemoryLog)
$ArenaLog = [System.IO.Path]::GetFullPath($ArenaLog)
$OutputPath = [System.IO.Path]::GetFullPath($OutputPath)

function Get-JsonLineField {
    param(
        [string]$Line,
        [string]$Name
    )

    $Pattern = '"' + [regex]::Escape($Name) + '"\s*:\s*("[^"]*"|-?\d+)'
    $Match = [regex]::Match($Line, $Pattern)
    if (!$Match.Success) {
        return $null
    }
    $Value = $Match.Groups[1].Value
    if ($Value.StartsWith('"') -and $Value.EndsWith('"')) {
        return $Value.Substring(1, $Value.Length - 2)
    }
    return $Value
}

function Add-SelectedLines {
    param(
        [System.Collections.Generic.List[object]]$Rows,
        [string]$Path,
        [string]$Source,
        [ref]$Order
    )

    if (!(Test-Path $Path)) {
        return [pscustomobject]@{
            source = $Source
            path = $Path
            found = $false
            selected = 0
        }
    }

    $Selected = 0
    Get-Content -LiteralPath $Path | ForEach-Object {
        $Line = $_
        $Kind = Get-JsonLineField -Line $Line -Name "kind"
        if (!$Kind) {
            return
        }

        $FrameText = Get-JsonLineField -Line $Line -Name "frame"
        $Frame = 0
        if ($FrameText -and [int]::TryParse($FrameText, [ref]$Frame)) {
            $Include = $false
            if ($Kind -eq "ppu_arena_rows_snapshot") {
                $Include = $true
            } elseif ($Kind -eq "ppu_nametable_write_batch" -and $Frame -ge 428 -and $Frame -le 430) {
                $Include = $true
            } elseif ($Kind -eq "ppu_attribute_write_batch" -and $Frame -ge 427 -and $Frame -le 430) {
                $Include = $true
            } elseif ($Kind -eq "ppu_palette_write_batch" -and $Frame -ge 425 -and $Frame -le 476) {
                $Include = $true
            } elseif ($Kind -eq "oam_frame_diff" -and $Frame -ge 461 -and $Frame -le 811) {
                $Include = $true
            }

            if ($Include) {
                $Rows.Add([pscustomobject]@{
                    frame = $Frame
                    order = $Order.Value
                    line = $Line
                    source = $Source
                    kind = $Kind
                })
                ++$Order.Value
                ++$Selected
            }
        }
    }

    return [pscustomobject]@{
        source = $Source
        path = $Path
        found = $true
        selected = $Selected
    }
}

$Rows = [System.Collections.Generic.List[object]]::new()
$Order = 0
$Reports = @(
    (Add-SelectedLines -Rows $Rows -Path $MemoryLog -Source "memory" -Order ([ref]$Order)),
    (Add-SelectedLines -Rows $Rows -Path $ArenaLog -Source "arena" -Order ([ref]$Order))
)

if ($Rows.Count -eq 0) {
    $Missing = $Reports | Where-Object { !$_.found } | ForEach-Object { $_.path }
    if ($Missing.Count -gt 0) {
        throw "No arena capture rows were imported. Missing input(s): $($Missing -join ', ')"
    }
    throw "No arena capture rows matched the import filters."
}

$OutputDir = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$Rows |
    Sort-Object order |
    ForEach-Object { $_.line } |
    Set-Content -LiteralPath $OutputPath -Encoding ascii

$Report = [pscustomobject]@{
    output = $OutputPath
    rows = $Rows.Count
    sources = $Reports
}
$Report | ConvertTo-Json -Depth 4
