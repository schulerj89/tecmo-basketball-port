param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [switch]$Build,
    [string]$AssetPackPath,
    [string]$ReportPath
)

$ErrorActionPreference = "Stop"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path $ProjectRoot).Path

$BuildDir = Join-Path $ProjectRoot "build"
$OutputDir = Join-Path $BuildDir "intro_sequence"
$CheckpointOutputDir = Join-Path $BuildDir "arena_rom_exact"
if (!$AssetPackPath) {
    $AssetPackPath = Join-Path $OutputDir "tecmo_intro_sequence_test.assetpack"
}
if (![System.IO.Path]::IsPathRooted($AssetPackPath)) {
    $AssetPackPath = Join-Path $ProjectRoot $AssetPackPath
}
$AssetPackPath = [System.IO.Path]::GetFullPath($AssetPackPath)
if (!$ReportPath) {
    $ReportPath = Join-Path $BuildDir "intro_sequence_test_report.json"
}
if (![System.IO.Path]::IsPathRooted($ReportPath)) {
    $ReportPath = Join-Path $ProjectRoot $ReportPath
}
$ReportPath = [System.IO.Path]::GetFullPath($ReportPath)

function Find-ReferenceRom {
    if ($RomPath) {
        if (!(Test-Path $RomPath)) {
            throw "RomPath does not exist."
        }
        return (Resolve-Path $RomPath).Path
    }
    if ($env:TECMO_ROM_PATH -and (Test-Path $env:TECMO_ROM_PATH)) {
        return (Resolve-Path $env:TECMO_ROM_PATH).Path
    }

    return $null
}

function Test-PathUnder {
    param(
        [string]$Path,
        [string]$Parent
    )

    $FullPath = [System.IO.Path]::GetFullPath($Path)
    $FullParent = [System.IO.Path]::GetFullPath($Parent).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    return $FullPath.StartsWith($FullParent, [System.StringComparison]::OrdinalIgnoreCase)
}

function Get-RepoRelativePath {
    param(
        [string]$Path
    )

    $FullPath = [System.IO.Path]::GetFullPath($Path)
    $FullRoot = [System.IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\', '/')
    $RootWithSlash = $FullRoot + [System.IO.Path]::DirectorySeparatorChar
    if ($FullPath.StartsWith($RootWithSlash, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $FullPath.Substring($RootWithSlash.Length).Replace('\', '/')
    }
    if ($FullPath.Equals($FullRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        return "."
    }
    return "<outside-project>"
}

function Get-SafeExceptionName {
    param($ErrorRecord)

    if ($ErrorRecord -and $ErrorRecord.Exception) {
        return $ErrorRecord.Exception.GetType().Name
    }
    return "Error"
}

function Test-PixelRectColor {
    param(
        [System.Drawing.Bitmap]$Bitmap,
        [int]$Left,
        [int]$Top,
        [int]$Right,
        [int]$Bottom,
        [int]$Red,
        [int]$Green,
        [int]$Blue
    )

    for ($Y = $Top; $Y -le $Bottom; ++$Y) {
        for ($X = $Left; $X -le $Right; ++$X) {
            $Pixel = $Bitmap.GetPixel($X, $Y)
            if ($Pixel.R -ne $Red -or $Pixel.G -ne $Green -or $Pixel.B -ne $Blue) {
                return $false
            }
        }
    }
    return $true
}

function Test-PixelRectHasNonBlack {
    param(
        [System.Drawing.Bitmap]$Bitmap,
        [int]$Left,
        [int]$Top,
        [int]$Right,
        [int]$Bottom
    )

    for ($Y = $Top; $Y -le $Bottom; ++$Y) {
        for ($X = $Left; $X -le $Right; ++$X) {
            $Pixel = $Bitmap.GetPixel($X, $Y)
            if ($Pixel.R -ne 0 -or $Pixel.G -ne 0 -or $Pixel.B -ne 0) {
                return $true
            }
        }
    }
    return $false
}

function Compare-NativeViewportMask {
    param(
        [System.Drawing.Bitmap]$HostBitmap,
        [System.Drawing.Bitmap]$ReferenceBitmap
    )

    $HostCount = 0
    $ReferenceCount = 0
    $Intersection = 0
    $Union = 0
    $HostBounds = @([int]::MaxValue, [int]::MaxValue, -1, -1)
    $ReferenceBounds = @([int]::MaxValue, [int]::MaxValue, -1, -1)
    for ($Y = 0; $Y -lt 224; ++$Y) {
        for ($X = 0; $X -lt 256; ++$X) {
            $HostPixel = $HostBitmap.GetPixel(64 + $X * 2, 16 + $Y * 2)
            $ReferencePixel = $ReferenceBitmap.GetPixel($X, $Y)
            $HostSet = $HostPixel.R -ne 0 -or $HostPixel.G -ne 0 -or $HostPixel.B -ne 0
            $ReferenceSet = $ReferencePixel.R -ne 0 -or $ReferencePixel.G -ne 0 -or $ReferencePixel.B -ne 0
            if ($HostSet) {
                ++$HostCount
                if ($X -lt $HostBounds[0]) { $HostBounds[0] = $X }
                if ($Y -lt $HostBounds[1]) { $HostBounds[1] = $Y }
                if ($X -gt $HostBounds[2]) { $HostBounds[2] = $X }
                if ($Y -gt $HostBounds[3]) { $HostBounds[3] = $Y }
            }
            if ($ReferenceSet) {
                ++$ReferenceCount
                if ($X -lt $ReferenceBounds[0]) { $ReferenceBounds[0] = $X }
                if ($Y -lt $ReferenceBounds[1]) { $ReferenceBounds[1] = $Y }
                if ($X -gt $ReferenceBounds[2]) { $ReferenceBounds[2] = $X }
                if ($Y -gt $ReferenceBounds[3]) { $ReferenceBounds[3] = $Y }
            }
            if ($HostSet -and $ReferenceSet) { ++$Intersection }
            if ($HostSet -or $ReferenceSet) { ++$Union }
        }
    }
    $HostBoundsText = $HostBounds -join ","
    $ReferenceBoundsText = $ReferenceBounds -join ","
    return [pscustomobject]@{
        passed = $HostCount -eq $ReferenceCount -and $Intersection -eq $Union -and
            $HostBoundsText -eq $ReferenceBoundsText
        host_nonblack = $HostCount
        reference_nonblack = $ReferenceCount
        intersection = $Intersection
        union = $Union
        mask_iou = if ($Union -eq 0) { 1.0 } else { [double]$Intersection / [double]$Union }
        host_bounds = $HostBoundsText
        reference_bounds = $ReferenceBoundsText
    }
}

function Get-AssetPackEntryPayloadOffset {
    param(
        [byte[]]$Bytes,
        [string]$EntryId
    )

    if ($Bytes.Length -lt 40 -or
        [System.Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -ne "TAP1") {
        throw "Malformed asset-pack header."
    }

    $EntryCount = [System.BitConverter]::ToUInt32($Bytes, 16)
    $DirectoryOffset = [System.BitConverter]::ToUInt64($Bytes, 20)
    for ($EntryIndex = 0; $EntryIndex -lt $EntryCount; ++$EntryIndex) {
        $EntryOffset64 = $DirectoryOffset + [uint64]($EntryIndex * 128)
        if ($EntryOffset64 + 128 -gt [uint64]$Bytes.Length -or
            $EntryOffset64 -gt [int]::MaxValue) {
            throw "Malformed asset-pack directory."
        }

        $EntryOffset = [int]$EntryOffset64
        $EntryNameLength = 0
        while ($EntryNameLength -lt 64 -and $Bytes[$EntryOffset + $EntryNameLength] -ne 0) {
            ++$EntryNameLength
        }
        $EntryName = [System.Text.Encoding]::ASCII.GetString(
            $Bytes,
            $EntryOffset,
            $EntryNameLength)
        if ($EntryName -eq $EntryId) {
            $PayloadOffset = [System.BitConverter]::ToUInt64($Bytes, $EntryOffset + 84)
            $PayloadSize = [System.BitConverter]::ToUInt64($Bytes, $EntryOffset + 92)
            if ($PayloadOffset + $PayloadSize -gt [uint64]$Bytes.Length -or
                $PayloadOffset -gt [int]::MaxValue) {
                throw "Malformed asset-pack entry payload."
            }
            return [int]$PayloadOffset
        }
    }

    throw "Asset-pack entry was not found."
}

function Get-AssetPackEntryDirectoryOffset {
    param(
        [byte[]]$Bytes,
        [string]$EntryId
    )

    if ($Bytes.Length -lt 40 -or
        [System.Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -ne "TAP1") {
        throw "Malformed asset-pack header."
    }
    $EntryCount = [System.BitConverter]::ToUInt32($Bytes, 16)
    $DirectoryOffset = [System.BitConverter]::ToUInt64($Bytes, 20)
    for ($EntryIndex = 0; $EntryIndex -lt $EntryCount; ++$EntryIndex) {
        $EntryOffset64 = $DirectoryOffset + [uint64]($EntryIndex * 128)
        if ($EntryOffset64 + 128 -gt [uint64]$Bytes.Length -or
            $EntryOffset64 -gt [int]::MaxValue) {
            throw "Malformed asset-pack directory."
        }
        $EntryOffset = [int]$EntryOffset64
        $EntryNameLength = 0
        while ($EntryNameLength -lt 64 -and $Bytes[$EntryOffset + $EntryNameLength] -ne 0) {
            ++$EntryNameLength
        }
        $EntryName = [System.Text.Encoding]::ASCII.GetString(
            $Bytes, $EntryOffset, $EntryNameLength)
        if ($EntryName -eq $EntryId) { return $EntryOffset }
    }
    throw "Asset-pack entry was not found."
}

$ExePath = Join-Path $BuildDir "tecmo_port.exe"

$ReferenceRom = Find-ReferenceRom
if (!$ReferenceRom) {
    throw "Could not find a local iNES ROM. Pass -RomPath or set TECMO_ROM_PATH."
}

if (!(Test-PathUnder $AssetPackPath $BuildDir)) {
    throw "Intro sequence asset pack output must stay under build\."
}
if (!(Test-PathUnder $ReportPath $BuildDir)) {
    throw "Intro sequence report must stay under build\."
}
if (!$AssetPackPath.EndsWith(".assetpack", [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Intro sequence asset pack output must use the .assetpack extension."
}

if ($Build) {
    $PreviousSkipShortcut = $env:TECMO_SKIP_SHORTCUT
    $env:TECMO_SKIP_SHORTCUT = "1"
    try {
        & (Join-Path $ProjectRoot "build.ps1")
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed with exit code $LASTEXITCODE."
        }
    } finally {
        $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    }
}
if (!(Test-Path $ExePath)) {
    throw "Executable not found at $ExePath. Run .\build.ps1 or pass -Build."
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
New-Item -ItemType Directory -Force -Path $CheckpointOutputDir | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $AssetPackPath) | Out-Null
Add-Type -AssemblyName System.Drawing
$RomOnlyArenaRenderCases = @(
    [pscustomobject]@{ mode = "intro-arena-frame0"; expected_goal = 0; expected_jumbotron = 55; checkpoint = $false },
    [pscustomobject]@{ mode = "intro-arena-frame240"; expected_goal = 0; expected_jumbotron = $null; checkpoint = $true },
    [pscustomobject]@{ mode = "intro-arena-frame260"; expected_goal = 5; expected_jumbotron = $null; checkpoint = $true },
    [pscustomobject]@{ mode = "intro-arena-frame276"; expected_goal = 10; expected_jumbotron = $null; checkpoint = $true },
    [pscustomobject]@{ mode = "intro-arena-frame280"; expected_goal = 10; expected_jumbotron = $null; checkpoint = $true },
    [pscustomobject]@{ mode = "intro-arena-frame292"; expected_goal = 15; expected_jumbotron = $null; checkpoint = $true },
    [pscustomobject]@{ mode = "intro-arena-frame300"; expected_goal = 15; expected_jumbotron = $null; checkpoint = $true },
    [pscustomobject]@{ mode = "intro-arena-frame308"; expected_goal = 16; expected_jumbotron = $null; checkpoint = $true },
    [pscustomobject]@{ mode = "intro-arena-frame539"; expected_goal = 16; expected_jumbotron = $null; checkpoint = $true }
)
$RomOnlyArenaRenderModes = @($RomOnlyArenaRenderCases | ForEach-Object { $_.mode })
$LowerBandGeometryCases = @(
    [pscustomobject]@{ frame = 308; scroll = "50"; motion = "6A"; origin_y = 458; clip_y = 460 },
    [pscustomobject]@{ frame = 324; scroll = "58"; motion = "5A"; origin_y = 426; clip_y = 428 },
    [pscustomobject]@{ frame = 340; scroll = "60"; motion = "4A"; origin_y = 394; clip_y = 396 },
    [pscustomobject]@{ frame = 348; scroll = "64"; motion = "42"; origin_y = 378; clip_y = 380 }
)
$MalformedCleanArenaModes = @(
    "intro-arena-clean-frame",
    "intro-arena-clean-frame-1",
    "intro-arena-clean-frame1junk",
    "intro-arena-clean-frame4294967296"
)
$PostArenaRenderCases = @(
    [pscustomobject]@{ mode = "intro-ready-clean-frame0"; state = "intro-ready-state frame=0 palette=0 mask=0 black=0 handoff=0"; visual = "black" },
    [pscustomobject]@{ mode = "intro-ready-clean-frame28"; state = "intro-ready-state frame=28 palette=4 mask=2 black=0 handoff=0"; visual = "ready" },
    [pscustomobject]@{ mode = "intro-ready-clean-frame32"; state = "intro-ready-state frame=32 palette=4 mask=4 black=0 handoff=0"; visual = "ready" },
    [pscustomobject]@{ mode = "intro-ready-clean-frame35"; state = "intro-ready-state frame=35 palette=4 mask=5 black=0 handoff=0"; visual = "ready" },
    [pscustomobject]@{ mode = "intro-ready-clean-frame40"; state = "intro-ready-state frame=40 palette=4 mask=8 black=0 handoff=0"; visual = "ready" },
    [pscustomobject]@{ mode = "intro-ready-clean-frame56"; state = "intro-ready-state frame=56 palette=4 mask=11 black=1 handoff=0"; visual = "black" },
    [pscustomobject]@{ mode = "intro-warriors-clean-frame0"; state = "intro-warriors-state frame=0 phase=load palette=0 pan=0 wordmark=0 patches=0 black=0 handoff=0 next_screen=1B"; visual = "black" },
    [pscustomobject]@{ mode = "intro-warriors-clean-frame17"; state = "intro-warriors-state frame=17 phase=wordmark palette=3 pan=0 wordmark=1 patches=0 black=0 handoff=0 next_screen=1B"; visual = "warriors" },
    [pscustomobject]@{ mode = "intro-warriors-clean-frame24"; state = "intro-warriors-state frame=24 phase=wordmark palette=3 pan=0 wordmark=8 patches=0 black=0 handoff=0 next_screen=1B"; visual = "warriors" },
    [pscustomobject]@{ mode = "intro-warriors-clean-frame64"; state = "intro-warriors-state frame=64 phase=pan palette=3 pan=20 wordmark=8 patches=0 black=0 handoff=0 next_screen=1B"; visual = "warriors" },
    [pscustomobject]@{ mode = "intro-warriors-clean-frame160"; state = "intro-warriors-state frame=160 phase=hold palette=3 pan=25 wordmark=8 patches=0 black=0 handoff=0 next_screen=1B"; visual = "warriors" },
    [pscustomobject]@{ mode = "intro-warriors-clean-frame193"; state = "intro-warriors-state frame=193 phase=patch-one palette=3 pan=25 wordmark=8 patches=1 black=0 handoff=0 next_screen=1B"; visual = "warriors" },
    [pscustomobject]@{ mode = "intro-warriors-clean-frame200"; state = "intro-warriors-state frame=200 phase=patch-two palette=3 pan=25 wordmark=8 patches=2 black=0 handoff=0 next_screen=1B"; visual = "warriors" },
    [pscustomobject]@{ mode = "intro-warriors-clean-frame213"; state = "intro-warriors-state frame=213 phase=black palette=0 pan=0 wordmark=0 patches=0 black=1 handoff=0 next_screen=1B"; visual = "black" },
    [pscustomobject]@{ mode = "intro-warriors-clean-frame214"; state = "intro-warriors-state frame=214 phase=handoff palette=0 pan=0 wordmark=0 patches=0 black=1 handoff=1 next_screen=1B"; visual = "black" },
    [pscustomobject]@{ mode = "intro-clippers-clean-frame0"; state = "intro-clippers-state frame=0 palette=0 motion=0 scroll=0 page=0 wordmark=0 handoff=0 next_route=883D"; visual = "black" },
    [pscustomobject]@{ mode = "intro-clippers-clean-frame10"; state = "intro-clippers-state frame=10 palette=1 motion=0 scroll=0 page=0 wordmark=0 handoff=0 next_route=883D"; visual = "clippers-pose" },
    [pscustomobject]@{ mode = "intro-clippers-clean-frame14"; state = "intro-clippers-state frame=14 palette=2 motion=0 scroll=0 page=0 wordmark=0 handoff=0 next_route=883D"; visual = "clippers-pose" },
    [pscustomobject]@{ mode = "intro-clippers-clean-frame18"; state = "intro-clippers-state frame=18 palette=3 motion=0 scroll=0 page=0 wordmark=0 handoff=0 next_route=883D"; visual = "clippers-pose" },
    [pscustomobject]@{ mode = "intro-clippers-clean-frame31"; state = "intro-clippers-state frame=31 palette=3 motion=0 scroll=0 page=0 wordmark=0 handoff=0 next_route=883D"; visual = "clippers-pose" },
    [pscustomobject]@{ mode = "intro-clippers-clean-frame32"; state = "intro-clippers-state frame=32 palette=3 motion=0 scroll=0 page=0 wordmark=1 handoff=0 next_route=883D"; visual = "clippers" },
    [pscustomobject]@{ mode = "intro-clippers-clean-frame39"; state = "intro-clippers-state frame=39 palette=3 motion=0 scroll=0 page=0 wordmark=1 handoff=0 next_route=883D"; visual = "clippers" },
    [pscustomobject]@{ mode = "intro-clippers-clean-frame40"; state = "intro-clippers-state frame=40 palette=3 motion=0 scroll=0 page=0 wordmark=1 handoff=0 next_route=883D"; visual = "clippers" },
    [pscustomobject]@{ mode = "intro-clippers-clean-frame79"; state = "intro-clippers-state frame=79 palette=3 motion=19 scroll=0 page=0 wordmark=1 handoff=0 next_route=883D"; visual = "clippers" },
    [pscustomobject]@{ mode = "intro-clippers-clean-frame80"; state = "intro-clippers-state frame=80 palette=3 motion=20 scroll=255 page=1 wordmark=1 handoff=0 next_route=883D"; visual = "clippers" },
    [pscustomobject]@{ mode = "intro-clippers-clean-frame98"; state = "intro-clippers-state frame=98 palette=3 motion=29 scroll=255 page=1 wordmark=1 handoff=0 next_route=883D"; visual = "clippers" },
    [pscustomobject]@{ mode = "intro-clippers-clean-frame150"; state = "intro-clippers-state frame=150 palette=3 motion=41 scroll=255 page=1 wordmark=1 handoff=0 next_route=883D"; visual = "clippers" },
    [pscustomobject]@{ mode = "intro-clippers-clean-frame151"; state = "intro-clippers-state frame=151 palette=3 motion=41 scroll=255 page=1 wordmark=1 handoff=1 next_route=883D"; visual = "clippers" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame0"; state = "intro-bucks-state frame=0 palette=0 flash=0 scroll=0 wordmark=0 prior=1 black=1 handoff=0 next_route=854F"; visual = "black" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame1"; state = "intro-bucks-state frame=1 palette=0 flash=0 scroll=0 wordmark=0 prior=0 black=1 handoff=0 next_route=854F"; visual = "black" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame14"; state = "intro-bucks-state frame=14 palette=0 flash=0 scroll=0 wordmark=5 prior=0 black=0 handoff=0 next_route=854F"; visual = "bucks" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame16"; state = "intro-bucks-state frame=16 palette=1 flash=1 scroll=40 wordmark=5 prior=0 black=0 handoff=0 next_route=854F"; visual = "bucks" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame17"; state = "intro-bucks-state frame=17 palette=0 flash=2 scroll=48 wordmark=5 prior=0 black=0 handoff=0 next_route=854F"; visual = "bucks" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame19"; state = "intro-bucks-state frame=19 palette=1 flash=3 scroll=16 wordmark=5 prior=0 black=0 handoff=0 next_route=854F"; visual = "bucks" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame21"; state = "intro-bucks-state frame=21 palette=2 flash=4 scroll=88 wordmark=5 prior=0 black=0 handoff=0 next_route=854F"; visual = "bucks" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame22"; state = "intro-bucks-state frame=22 palette=0 flash=5 scroll=96 wordmark=5 prior=0 black=0 handoff=0 next_route=854F"; visual = "bucks" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame24"; state = "intro-bucks-state frame=24 palette=1 flash=6 scroll=64 wordmark=5 prior=0 black=0 handoff=0 next_route=854F"; visual = "bucks" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame26"; state = "intro-bucks-state frame=26 palette=2 flash=7 scroll=32 wordmark=5 prior=0 black=0 handoff=0 next_route=854F"; visual = "bucks" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame28"; state = "intro-bucks-state frame=28 palette=3 flash=8 scroll=0 wordmark=5 prior=0 black=0 handoff=0 next_route=854F"; visual = "bucks" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame30"; state = "intro-bucks-state frame=30 palette=0 flash=9 scroll=144 wordmark=5 prior=0 black=0 handoff=0 next_route=854F"; visual = "bucks" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame32"; state = "intro-bucks-state frame=32 palette=1 flash=10 scroll=112 wordmark=5 prior=0 black=0 handoff=0 next_route=854F"; visual = "bucks" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame34"; state = "intro-bucks-state frame=34 palette=2 flash=11 scroll=80 wordmark=5 prior=0 black=0 handoff=0 next_route=854F"; visual = "bucks" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame36"; state = "intro-bucks-state frame=36 palette=3 flash=12 scroll=48 wordmark=5 prior=0 black=0 handoff=0 next_route=854F"; visual = "bucks" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame38"; state = "intro-bucks-state frame=38 palette=0 flash=13 scroll=192 wordmark=5 prior=0 black=0 handoff=0 next_route=854F"; visual = "bucks" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame40"; state = "intro-bucks-state frame=40 palette=1 flash=14 scroll=160 wordmark=5 prior=0 black=0 handoff=0 next_route=854F"; visual = "bucks" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame42"; state = "intro-bucks-state frame=42 palette=2 flash=15 scroll=128 wordmark=5 prior=0 black=0 handoff=0 next_route=854F"; visual = "bucks" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame44"; state = "intro-bucks-state frame=44 palette=3 flash=16 scroll=96 wordmark=5 prior=0 black=0 handoff=0 next_route=854F"; visual = "bucks" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame46"; state = "intro-bucks-state frame=46 palette=0 flash=17 scroll=239 wordmark=5 prior=0 black=0 handoff=0 next_route=854F"; visual = "bucks" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame48"; state = "intro-bucks-state frame=48 palette=1 flash=18 scroll=207 wordmark=5 prior=0 black=0 handoff=0 next_route=854F"; visual = "bucks" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame50"; state = "intro-bucks-state frame=50 palette=2 flash=19 scroll=175 wordmark=5 prior=0 black=0 handoff=0 next_route=854F"; visual = "bucks" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame52"; state = "intro-bucks-state frame=52 palette=3 flash=20 scroll=143 wordmark=5 prior=0 black=0 handoff=0 next_route=854F"; visual = "bucks" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame53"; state = "intro-bucks-state frame=53 palette=3 flash=20 scroll=240 wordmark=5 prior=0 black=0 handoff=0 next_route=854F"; visual = "bucks" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame83"; state = "intro-bucks-state frame=83 palette=3 flash=20 scroll=240 wordmark=5 prior=0 black=0 handoff=1 next_route=854F"; visual = "bucks" },
    [pscustomobject]@{ mode = "intro-pass-clean-frame1"; state = "intro-pass-state frame=1 phase=black palette=0 x=104 scroll=0 first=0 second=0 sprites=0 black=1 handoff=0 next_route=851C"; visual = "black" },
    [pscustomobject]@{ mode = "intro-pass-clean-frame10"; state = "intro-pass-state frame=10 phase=first-move palette=0 x=128 scroll=0 first=3 second=0 sprites=1 black=0 handoff=0 next_route=851C"; visual = "pass" },
    [pscustomobject]@{ mode = "intro-pass-clean-frame14"; state = "intro-pass-state frame=14 phase=first-move palette=1 x=160 scroll=0 first=7 second=0 sprites=1 black=0 handoff=0 next_route=851C"; visual = "pass" },
    [pscustomobject]@{ mode = "intro-pass-clean-frame18"; state = "intro-pass-state frame=18 phase=first-move palette=2 x=192 scroll=0 first=11 second=0 sprites=1 black=0 handoff=0 next_route=851C"; visual = "pass" },
    [pscustomobject]@{ mode = "intro-pass-clean-frame22"; state = "intro-pass-state frame=22 phase=first-move palette=3 x=224 scroll=0 first=15 second=0 sprites=1 black=0 handoff=0 next_route=851C"; visual = "pass" },
    [pscustomobject]@{ mode = "intro-pass-clean-frame25"; state = "intro-pass-state frame=25 phase=first-move palette=3 x=248 scroll=0 first=18 second=0 sprites=1 black=0 handoff=0 next_route=851C"; visual = "pass" },
    [pscustomobject]@{ mode = "intro-pass-clean-frame26"; state = "intro-pass-state frame=26 phase=hold palette=3 x=248 scroll=0 first=18 second=0 sprites=1 black=0 handoff=0 next_route=851C"; visual = "pass" },
    [pscustomobject]@{ mode = "intro-pass-clean-frame27"; state = "intro-pass-state frame=27 phase=second-move palette=4 x=8 scroll=8 first=18 second=1 sprites=1 black=0 handoff=0 next_route=851C"; visual = "pass" },
    [pscustomobject]@{ mode = "intro-pass-clean-frame28"; state = "intro-pass-state frame=28 phase=second-move palette=4 x=16 scroll=16 first=18 second=2 sprites=1 black=0 handoff=0 next_route=851C"; visual = "pass" },
    [pscustomobject]@{ mode = "intro-pass-clean-frame49"; state = "intro-pass-state frame=49 phase=second-move palette=4 x=184 scroll=184 first=18 second=23 sprites=1 black=0 handoff=0 next_route=851C"; visual = "pass" },
    [pscustomobject]@{ mode = "intro-pass-clean-frame51"; state = "intro-pass-state frame=51 phase=second-move palette=4 x=200 scroll=200 first=18 second=25 sprites=1 black=0 handoff=0 next_route=851C"; visual = "pass-late" },
    [pscustomobject]@{ mode = "intro-pass-clean-frame52"; state = "intro-pass-state frame=52 phase=handoff palette=4 x=200 scroll=200 first=18 second=25 sprites=0 black=1 handoff=1 next_route=851C"; visual = "black" }
)
$FinaleRenderCases = @(
    [pscustomobject]@{ mode = "intro-finale-opening-clean-frame0"; state = "intro-finale-state frame=0 scene=opening-screen phase=load local=0 palette=0 variant=0 loop=0 anchor=0,0 title=0 primary=0:0 secondary=0:0 sprites=0 black=0 hold=0"; visual = "scene" },
    [pscustomobject]@{ mode = "intro-finale-opening-clean-frame50"; state = "intro-finale-state frame=50 scene=opening-screen phase=dispatch-wait local=50 palette=0 variant=0 loop=0 anchor=0,0 title=0 primary=0:0 secondary=0:0 sprites=0 black=0 hold=0"; visual = "scene" },
    [pscustomobject]@{ mode = "intro-finale-short-clean-frame0"; state = "intro-finale-state frame=51 scene=short-sprite-loop phase=load local=0 palette=0 variant=0 loop=0 anchor=0,0 title=0 primary=0:0 secondary=0:0 sprites=0 black=0 hold=0"; visual = "scene" },
    [pscustomobject]@{ mode = "intro-finale-short-clean-frame1"; state = "intro-finale-state frame=52 scene=short-sprite-loop phase=short-loop local=1 palette=0 variant=0 loop=0 anchor=142,70 title=0 primary=0:0 secondary=0:0 sprites=1 black=0 hold=0"; visual = "scene" },
    [pscustomobject]@{ mode = "intro-finale-short-clean-frame16"; state = "intro-finale-state frame=67 scene=short-sprite-loop phase=short-loop local=16 palette=0 variant=0 loop=7 anchor=142,240 title=0 primary=0:0 secondary=0:0 sprites=1 black=0 hold=0"; visual = "scene" },
    [pscustomobject]@{ mode = "intro-finale-short-clean-frame17"; state = "intro-finale-state frame=68 scene=short-sprite-loop phase=dispatch-wait local=17 palette=0 variant=0 loop=7 anchor=142,240 title=0 primary=0:0 secondary=0:0 sprites=1 black=0 hold=0"; visual = "scene" },
    [pscustomobject]@{ mode = "intro-finale-short-clean-frame46"; state = "intro-finale-state frame=97 scene=short-sprite-loop phase=dispatch-wait local=46 palette=0 variant=0 loop=7 anchor=142,240 title=0 primary=0:0 secondary=0:0 sprites=1 black=0 hold=0"; visual = "scene" },
    [pscustomobject]@{ mode = "intro-finale-reverse-clean-frame0"; state = "intro-finale-state frame=98 scene=selector-transition phase=load local=0 palette=0 variant=1 loop=0 anchor=0,84 title=0 primary=0:0 secondary=0:0 sprites=0 black=1 hold=0"; visual = "black" },
    [pscustomobject]@{ mode = "intro-finale-reverse-clean-frame10"; state = "intro-finale-state frame=108 scene=selector-transition phase=first-move local=10 palette=0 variant=1 loop=0 anchor=96,84 title=0 primary=0:0 secondary=0:0 sprites=1 black=0 hold=0"; visual = "scene" },
    [pscustomobject]@{ mode = "intro-finale-reverse-clean-frame26"; state = "intro-finale-state frame=124 scene=selector-transition phase=hold local=26 palette=3 variant=1 loop=0 anchor=232,84 title=0 primary=0:0 secondary=0:0 sprites=1 black=0 hold=0"; visual = "scene" },
    [pscustomobject]@{ mode = "intro-finale-reverse-clean-frame27"; state = "intro-finale-state frame=125 scene=selector-transition phase=second-move local=27 palette=4 variant=1 loop=0 anchor=208,84 title=0 primary=0:248 secondary=0:0 sprites=1 black=0 hold=0"; visual = "scene" },
    [pscustomobject]@{ mode = "intro-finale-reverse-clean-frame51"; state = "intro-finale-state frame=149 scene=selector-transition phase=second-move local=51 palette=4 variant=1 loop=0 anchor=16,84 title=0 primary=0:56 secondary=0:0 sprites=1 black=0 hold=0"; visual = "scene" },
    [pscustomobject]@{ mode = "intro-finale-staged-clean-frame0"; state = "intro-finale-state frame=150 scene=staged-group phase=load local=0 palette=0 variant=0 loop=0 anchor=48,44 title=0 primary=0:0 secondary=0:0 sprites=0 black=0 hold=0"; visual = "scene" },
    [pscustomobject]@{ mode = "intro-finale-staged-clean-frame1"; state = "intro-finale-state frame=151 scene=staged-group phase=staged-wait local=1 palette=0 variant=0 loop=0 anchor=48,44 title=0 primary=0:0 secondary=0:0 sprites=1 black=0 hold=0"; visual = "scene" },
    [pscustomobject]@{ mode = "intro-finale-staged-clean-frame81"; state = "intro-finale-state frame=231 scene=staged-group phase=dispatch-wait local=81 palette=0 variant=0 loop=0 anchor=48,44 title=0 primary=0:0 secondary=0:0 sprites=1 black=0 hold=0"; visual = "scene" },
    [pscustomobject]@{ mode = "intro-finale-staged-clean-frame155"; state = "intro-finale-state frame=305 scene=staged-group phase=dispatch-wait local=155 palette=0 variant=0 loop=0 anchor=48,44 title=0 primary=0:0 secondary=0:0 sprites=1 black=0 hold=0"; visual = "scene" },
    [pscustomobject]@{ mode = "intro-finale-title-clean-frame0"; state = "intro-finale-state frame=306 scene=title phase=load local=0 palette=0 variant=0 loop=0 anchor=0,0 title=0 primary=0:0 secondary=0:0 sprites=0 black=0 hold=0"; visual = "black" },
    [pscustomobject]@{ mode = "intro-finale-title-clean-frame128"; state = "intro-finale-state frame=434 scene=title phase=title-preroll local=128 palette=0 variant=0 loop=0 anchor=0,0 title=0 primary=0:0 secondary=0:254 sprites=0 black=0 hold=0"; visual = "black" },
    [pscustomobject]@{ mode = "intro-finale-title-clean-frame129"; state = "intro-finale-state frame=435 scene=title phase=title-write local=129 palette=0 variant=0 loop=0 anchor=0,0 title=1 primary=0:2 secondary=1:0 sprites=0 black=0 hold=0"; visual = "scene" },
    [pscustomobject]@{ mode = "intro-finale-title-clean-frame473"; state = "intro-finale-state frame=779 scene=title phase=title-write local=473 palette=0 variant=0 loop=0 anchor=0,0 title=44 primary=0:178 secondary=1:0 sprites=0 black=0 hold=0"; visual = "scene" },
    [pscustomobject]@{ mode = "intro-finale-title-clean-frame474"; state = "intro-finale-state frame=780 scene=title phase=title-tail local=474 palette=0 variant=0 loop=0 anchor=0,0 title=44 primary=0:178 secondary=1:0 sprites=0 black=0 hold=0"; visual = "scene" },
    [pscustomobject]@{ mode = "intro-finale-title-clean-frame602"; state = "intro-finale-state frame=908 scene=title phase=dispatch-wait local=602 palette=0 variant=0 loop=0 anchor=0,0 title=44 primary=0:178 secondary=0:0 sprites=0 black=0 hold=0"; visual = "scene" },
    [pscustomobject]@{ mode = "intro-finale-hold-clean-frame0"; state = "intro-finale-state frame=909 scene=terminator-hold phase=terminator-hold local=0 palette=0 variant=0 loop=0 anchor=0,0 title=44 primary=0:178 secondary=0:0 sprites=0 black=0 hold=1"; visual = "scene" },
    [pscustomobject]@{ mode = "intro-finale-hold-clean-frame1024"; state = "intro-finale-state frame=1933 scene=terminator-hold phase=terminator-hold local=1024 palette=0 variant=0 loop=0 anchor=0,0 title=44 primary=0:178 secondary=0:0 sprites=0 black=0 hold=1"; visual = "scene" },
    [pscustomobject]@{ mode = "intro-finale-opening-frame1"; state = "intro-finale-state frame=1 scene=opening-screen phase=dispatch-wait local=1 palette=0 variant=0 loop=0 anchor=0,0 title=0 primary=0:0 secondary=0:0 sprites=0 black=0 hold=0"; visual = "scene" },
    [pscustomobject]@{ mode = "intro-finale-short-frame1"; state = "intro-finale-state frame=52 scene=short-sprite-loop phase=short-loop local=1 palette=0 variant=0 loop=0 anchor=142,70 title=0 primary=0:0 secondary=0:0 sprites=1 black=0 hold=0"; visual = "scene" },
    [pscustomobject]@{ mode = "intro-finale-reverse-frame27"; state = "intro-finale-state frame=125 scene=selector-transition phase=second-move local=27 palette=4 variant=1 loop=0 anchor=208,84 title=0 primary=0:248 secondary=0:0 sprites=1 black=0 hold=0"; visual = "scene" },
    [pscustomobject]@{ mode = "intro-finale-staged-frame1"; state = "intro-finale-state frame=151 scene=staged-group phase=staged-wait local=1 palette=0 variant=0 loop=0 anchor=48,44 title=0 primary=0:0 secondary=0:0 sprites=1 black=0 hold=0"; visual = "scene" },
    [pscustomobject]@{ mode = "intro-finale-title-frame473"; state = "intro-finale-state frame=779 scene=title phase=title-write local=473 palette=0 variant=0 loop=0 anchor=0,0 title=44 primary=0:178 secondary=1:0 sprites=0 black=0 hold=0"; visual = "scene" },
    [pscustomobject]@{ mode = "intro-finale-hold-frame0"; state = "intro-finale-state frame=909 scene=terminator-hold phase=terminator-hold local=0 palette=0 variant=0 loop=0 anchor=0,0 title=44 primary=0:178 secondary=0:0 sprites=0 black=0 hold=1"; visual = "scene" }
)
$BoundedReferenceCases = @(
    [pscustomobject]@{ mode = "intro-bucks-clean-frame14"; reference = "bucks-pass-02.png" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame16"; reference = "bucks-pass-03.png" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame22"; reference = "bucks-pass-07.png" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame30"; reference = "bucks-pass-11.png" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame38"; reference = "bucks-pass-15.png" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame46"; reference = "bucks-pass-19.png" },
    [pscustomobject]@{ mode = "intro-bucks-clean-frame52"; reference = "bucks-pass-22.png" },
    [pscustomobject]@{ mode = "intro-pass-clean-frame10"; reference = "bucks-pass-25.png" },
    [pscustomobject]@{ mode = "intro-pass-clean-frame14"; reference = "bucks-pass-26.png" },
    [pscustomobject]@{ mode = "intro-pass-clean-frame18"; reference = "bucks-pass-27.png" },
    [pscustomobject]@{ mode = "intro-pass-clean-frame22"; reference = "bucks-pass-28.png" },
    [pscustomobject]@{ mode = "intro-pass-clean-frame27"; reference = "bucks-pass-29.png" }
)

$Results = New-Object System.Collections.Generic.List[object]
$Failures = 0
$Skipped = 0
$AssetPackRelative = Get-RepoRelativePath $AssetPackPath

try {
    if (Test-Path -LiteralPath $AssetPackPath) {
        Remove-Item -LiteralPath $AssetPackPath -Force
    }

    Write-Host "Building intro sequence test asset pack -> $AssetPackRelative"
    $PackBuildOutput = & $ExePath --build-assetpack $ReferenceRom $AssetPackPath 2>&1
    $PackBuildExitCode = $LASTEXITCODE
    $PackCreated = Test-Path -LiteralPath $AssetPackPath
    $PackBuildOutputText = (@($PackBuildOutput) | ForEach-Object { [string]$_ }) -join "`n"
    $RomOnlyMessageSeen = $PackBuildOutputText -match "from iNES ROM"
    $PackBuildPassed = $PackBuildExitCode -eq 0 -and $PackCreated -and $RomOnlyMessageSeen
    if (!$PackBuildPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "intro-test-assetpack-build"
        command = ".\build\tecmo_port.exe --build-assetpack <LOCAL_ROM.nes> $($AssetPackRelative.Replace('/', '\'))"
        output = $AssetPackRelative
        passed = $PackBuildPassed
        skipped = $false
        exit_code = $PackBuildExitCode
        created = $PackCreated
        rom_only_message_seen = $RomOnlyMessageSeen
        raw_output_persisted = $false
        error = if ($PackBuildPassed) { $null } else { "fresh test asset pack was not built" }
    })

    $ListOutput = & $ExePath --assetpack-list $AssetPackPath 2>&1
    $ListExitCode = $LASTEXITCODE
    $ListText = (@($ListOutput) | ForEach-Object { [string]$_ }) -join "`n"
    $RequiredNativeEntries = @(
        "arena/intro/script",
        "arena/intro/background-layer",
        "arena/intro/palette-cycle",
        "arena/intro/sprite-groups",
        "arena/intro/ready-screen",
        "arena/intro/warriors-transition",
        "arena/intro/clippers-transition",
        "arena/intro/bucks-transition",
        "arena/intro/pass-transition",
        "intro/tecmo-presents-screen",
        "intro/nba-license-screen",
        "intro/finale-sequence",
        "title/attract-continuation",
        "title/start-screen",
        "menu/start-game"
    )
    $ForbiddenCaptureEntries = @("intro/arena/capture.ndjson", "intro/post-arena/capture.ndjson", "intro/captures/source-map")
    $MissingNativeEntries = @($RequiredNativeEntries | Where-Object { $ListText -notmatch [regex]::Escape($_) })
    $PresentForbiddenCaptureEntries = @($ForbiddenCaptureEntries | Where-Object { $ListText -match [regex]::Escape($_) })
    $ListPassed = $ListExitCode -eq 0 -and
        $MissingNativeEntries.Count -eq 0 -and
        $PresentForbiddenCaptureEntries.Count -eq 0
    if (!$ListPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "intro-test-assetpack-rom-only"
        output = $AssetPackRelative
        passed = $ListPassed
        skipped = $false
        exit_code = $ListExitCode
        required_native_entries = $RequiredNativeEntries
        missing_native_entries = $MissingNativeEntries
        forbidden_capture_entries = $ForbiddenCaptureEntries
        present_forbidden_capture_entries = $PresentForbiddenCaptureEntries
        raw_output_persisted = $false
        error = if ($ListPassed) { $null } else { "ROM-only test asset pack is missing native arena entries or contains capture entries" }
    })

    $PreviousAssetPack = $env:TECMO_ASSETPACK
    $TitleRenderPassed = $ListPassed
    $TitleRenderOutputs = New-Object System.Collections.Generic.List[object]
    try {
        $env:TECMO_ASSETPACK = $AssetPackPath
        foreach ($Mode in @("title-attract-frame6", "title-attract-frame621",
                             "title-screen", "title-confirm-frame1",
                             "title-confirm-frame10", "title-confirm-frame30",
                             "title-confirm-frame60", "title-confirm-frame120",
                             "title-confirm-frame126")) {
            $RenderPath = Join-Path $OutputDir ($Mode + ".png")
            $Output = & $ExePath --root $ProjectRoot --render-test-mode $Mode $RenderPath 2>&1
            $ExitCode = $LASTEXITCODE
            $Passed = $ExitCode -eq 0 -and (Test-Path -LiteralPath $RenderPath)
            if (!$Passed) { $TitleRenderPassed = $false }
            $TitleRenderOutputs.Add([pscustomobject]@{
                mode = $Mode
                passed = $Passed
                exit_code = $ExitCode
                output_created = Test-Path -LiteralPath $RenderPath
            })
            Remove-Item -LiteralPath $RenderPath -Force -ErrorAction SilentlyContinue
        }
    } finally {
        $env:TECMO_ASSETPACK = $PreviousAssetPack
    }
    if (!$TitleRenderPassed) { ++$Failures }
    $Results.Add([pscustomobject]@{
        id = "intro-title-attract-start-render"
        passed = $TitleRenderPassed
        skipped = $false
        cases = $TitleRenderOutputs
        timing = "TATR natural=621 reset=642; TTLE prompt halves=7 frames handoff=127"
        raw_output_persisted = $false
        error = if ($TitleRenderPassed) { $null } else { "native title attract/start render mode failed" }
    })

    $MalformedTitleCases = @(
        [pscustomobject]@{ id = "attract-reserved"; entry = "title/attract-continuation"; offset = 48; mode = "title-attract-frame6" },
        [pscustomobject]@{ id = "start-prompt"; entry = "title/start-screen"; offset = 5850; mode = "title-confirm-frame10" }
    )
    $MalformedTitleResults = New-Object System.Collections.Generic.List[object]
    $MalformedTitlePassed = $ListPassed
    $PreviousAssetPack = $env:TECMO_ASSETPACK
    try {
        foreach ($Case in $MalformedTitleCases) {
            $CasePack = Join-Path $OutputDir "malformed-title-$($Case.id).assetpack"
            $CaseRender = Join-Path $OutputDir "malformed-title-$($Case.id).png"
            [byte[]]$CaseBytes = [System.IO.File]::ReadAllBytes($AssetPackPath)
            $PayloadOffset = Get-AssetPackEntryPayloadOffset -Bytes $CaseBytes -EntryId $Case.entry
            $CaseBytes[$PayloadOffset + $Case.offset] = $CaseBytes[$PayloadOffset + $Case.offset] -bxor 1
            [System.IO.File]::WriteAllBytes($CasePack, $CaseBytes)
            $env:TECMO_ASSETPACK = $CasePack
            $CaseOutput = & $ExePath --root $ProjectRoot --render-test-mode $Case.mode $CaseRender 2>&1
            $CaseExitCode = $LASTEXITCODE
            $Rejected = $CaseExitCode -eq 1 -and !(Test-Path -LiteralPath $CaseRender)
            if (!$Rejected) { $MalformedTitlePassed = $false }
            $MalformedTitleResults.Add([pscustomobject]@{
                id = $Case.id
                passed = $Rejected
                exit_code = $CaseExitCode
                rejected_by_runtime = $Rejected
            })
            Remove-Item -LiteralPath $CasePack -Force -ErrorAction SilentlyContinue
            Remove-Item -LiteralPath $CaseRender -Force -ErrorAction SilentlyContinue
        }
    } finally {
        $env:TECMO_ASSETPACK = $PreviousAssetPack
    }
    if (!$MalformedTitlePassed) { ++$Failures }
    $Results.Add([pscustomobject]@{
        id = "intro-title-malformed-contract"
        passed = $MalformedTitlePassed
        skipped = $false
        cases = $MalformedTitleResults
        raw_output_persisted = $false
        coverage_status = "covered"
        error = if ($MalformedTitlePassed) { $null } else { "runtime accepted malformed native title data" }
    })

    $StartMenuRenderModes = @(
        "start-game-menu-frame0", "start-game-menu-frame2",
        "start-game-menu-frame4", "start-game-menu-frame6",
        "start-game-menu-frame8", "start-game-menu-frame19",
        "start-game-menu-frame20", "start-game-menu-frame24",
        "start-game-menu-frame28", "start-game-menu-frame32",
        "start-game-menu-cursor6", "start-game-menu-season-frame16",
        "start-game-menu-season-frame32", "start-game-menu-music",
        "start-game-menu-speed", "start-game-menu-period"
    )
    $StartMenuRenderResults = New-Object System.Collections.Generic.List[object]
    $StartMenuRenderPassed = $ListPassed
    $StartMenuIsolatedRoot = Join-Path $OutputDir `
        ("start_menu_rom_only_root-" + [Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force -Path $StartMenuIsolatedRoot | Out-Null
    $PreviousAssetPack = $env:TECMO_ASSETPACK
    try {
        $env:TECMO_ASSETPACK = $AssetPackPath
        foreach ($Mode in $StartMenuRenderModes) {
            $RenderPath = Join-Path $OutputDir ($Mode + ".png")
            Remove-Item -LiteralPath $RenderPath -Force -ErrorAction SilentlyContinue
            $RenderOutput = & $ExePath --root $StartMenuIsolatedRoot `
                --render-test-mode $Mode $RenderPath 2>&1
            $RenderExitCode = $LASTEXITCODE
            $RenderText = (@($RenderOutput) | ForEach-Object { [string]$_ }) -join "`n"
            $RenderPassed = $RenderExitCode -eq 0 -and
                (Test-Path -LiteralPath $RenderPath) -and
                $RenderText -match "start-game-menu-state"
            if (!$RenderPassed) { $StartMenuRenderPassed = $false }
            $StartMenuRenderResults.Add([pscustomobject]@{
                mode = $Mode
                passed = $RenderPassed
                exit_code = $RenderExitCode
                output_created = Test-Path -LiteralPath $RenderPath
            })
            Remove-Item -LiteralPath $RenderPath -Force -ErrorAction SilentlyContinue
        }
    } finally {
        $env:TECMO_ASSETPACK = $PreviousAssetPack
        Remove-Item -LiteralPath $StartMenuIsolatedRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
    if (!$StartMenuRenderPassed) { ++$Failures }
    $Results.Add([pscustomobject]@{
        id = "start-game-menu-rom-only-render"
        passed = $StartMenuRenderPassed
        skipped = $false
        cases = $StartMenuRenderResults
        timing = "title fade 0/2/4/6; black 8-19; menu fade 20/24/28; stable 32; season slide 32"
        isolated_root = $true
        raw_output_persisted = $false
        error = if ($StartMenuRenderPassed) { $null } else { "TSGM-1 render mode failed from an isolated root" }
    })

    $StartMenuNegativeCases = @(
        [pscustomobject]@{ id = "reserved"; offset = 126; value = 1 },
        [pscustomobject]@{ id = "period-values"; offset = 121; value = 5 },
        [pscustomobject]@{ id = "black-stage"; offset = 11808; value = 0 },
        [pscustomobject]@{ id = "chr-range"; offset = 162; value = $null }
    )
    $StartMenuNegativeResults = New-Object System.Collections.Generic.List[object]
    $StartMenuNegativePassed = $ListPassed
    $StartMenuNegativeRoot = Join-Path $OutputDir `
        ("start_menu_negative_root-" + [Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force -Path $StartMenuNegativeRoot | Out-Null
    $PreviousAssetPack = $env:TECMO_ASSETPACK
    try {
        [byte[]]$MissingBytes = [System.IO.File]::ReadAllBytes($AssetPackPath)
        $MissingDirectoryOffset = Get-AssetPackEntryDirectoryOffset `
            -Bytes $MissingBytes -EntryId "menu/start-game"
        $MissingBytes[$MissingDirectoryOffset] = [byte][char]'x'
        $MissingPack = Join-Path $OutputDir "missing-start-game-menu.assetpack"
        $MissingRender = Join-Path $OutputDir "missing-start-game-menu.png"
        [System.IO.File]::WriteAllBytes($MissingPack, $MissingBytes)
        $env:TECMO_ASSETPACK = $MissingPack
        $MissingOutput = & $ExePath --root $StartMenuNegativeRoot `
            --render-test-mode start-game-menu $MissingRender 2>&1
        $MissingExitCode = $LASTEXITCODE
        $MissingRejected = $MissingExitCode -eq 1 -and !(Test-Path -LiteralPath $MissingRender)
        if (!$MissingRejected) { $StartMenuNegativePassed = $false }
        $StartMenuNegativeResults.Add([pscustomobject]@{
            id = "missing-entry"
            passed = $MissingRejected
            exit_code = $MissingExitCode
            rejected_by_runtime = $MissingRejected
        })
        Remove-Item -LiteralPath $MissingPack -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $MissingRender -Force -ErrorAction SilentlyContinue

        foreach ($Case in $StartMenuNegativeCases) {
            [byte[]]$CaseBytes = [System.IO.File]::ReadAllBytes($AssetPackPath)
            $PayloadOffset = Get-AssetPackEntryPayloadOffset `
                -Bytes $CaseBytes -EntryId "menu/start-game"
            if ($Case.id -eq "chr-range") {
                [System.BitConverter]::GetBytes([uint32]([uint32]::MaxValue - 15)).CopyTo(
                    $CaseBytes, $PayloadOffset + $Case.offset)
            } else {
                $CaseBytes[$PayloadOffset + $Case.offset] = [byte]$Case.value
            }
            $CasePack = Join-Path $OutputDir "malformed-start-menu-$($Case.id).assetpack"
            $CaseRender = Join-Path $OutputDir "malformed-start-menu-$($Case.id).png"
            [System.IO.File]::WriteAllBytes($CasePack, $CaseBytes)
            $env:TECMO_ASSETPACK = $CasePack
            $CaseOutput = & $ExePath --root $StartMenuNegativeRoot `
                --render-test-mode start-game-menu $CaseRender 2>&1
            $CaseExitCode = $LASTEXITCODE
            $Rejected = $CaseExitCode -eq 1 -and !(Test-Path -LiteralPath $CaseRender)
            if (!$Rejected) { $StartMenuNegativePassed = $false }
            $StartMenuNegativeResults.Add([pscustomobject]@{
                id = $Case.id
                passed = $Rejected
                exit_code = $CaseExitCode
                rejected_by_runtime = $Rejected
            })
            Remove-Item -LiteralPath $CasePack -Force -ErrorAction SilentlyContinue
            Remove-Item -LiteralPath $CaseRender -Force -ErrorAction SilentlyContinue
        }
    } finally {
        $env:TECMO_ASSETPACK = $PreviousAssetPack
        Remove-Item -LiteralPath $StartMenuNegativeRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
    if (!$StartMenuNegativePassed) { ++$Failures }
    $Results.Add([pscustomobject]@{
        id = "start-game-menu-missing-malformed-contract"
        passed = $StartMenuNegativePassed
        skipped = $false
        cases = $StartMenuNegativeResults
        isolated_root = $true
        raw_output_persisted = $false
        coverage_status = "missing-reserved-period-palette-chr"
        error = if ($StartMenuNegativePassed) { $null } else { "runtime accepted missing or malformed TSGM-1 data" }
    })

    $OpeningRenderCases = @(
        [pscustomobject]@{
            mode = "play-step6"
            state = "intro-opening-state kind=tecmo-presents frame=16 palette=4 duration=133"
        },
        [pscustomobject]@{
            mode = "intro-license"
            state = "intro-opening-state kind=nba-license frame=48 palette=4 duration=277"
        },
        [pscustomobject]@{
            mode = "play-step7"
            state = "intro-opening-state kind=nba-license frame=48 palette=4 duration=277"
        }
    )
    $OpeningRenderOutputs = New-Object System.Collections.Generic.List[object]
    $OpeningRenderPassed = $ListPassed
    $PreviousAssetPack = $env:TECMO_ASSETPACK
    $PreviousLooseIntroTrace = $env:TECMO_ALLOW_LOOSE_INTRO_TRACE
    $OpeningIsolatedRoot = Join-Path $OutputDir `
        ("opening_rom_only_root-" + [Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force -Path $OpeningIsolatedRoot | Out-Null
    try {
        $env:TECMO_ASSETPACK = $AssetPackPath
        Remove-Item Env:TECMO_ALLOW_LOOSE_INTRO_TRACE -ErrorAction SilentlyContinue
        foreach ($OpeningCase in $OpeningRenderCases) {
            $RenderPath = Join-Path $OutputDir "$($OpeningCase.mode)-rom-only.png"
            Remove-Item -LiteralPath $RenderPath -Force -ErrorAction SilentlyContinue
            $RenderOutput = & $ExePath --root $OpeningIsolatedRoot `
                --render-test-mode $OpeningCase.mode $RenderPath 2>&1
            $RenderExitCode = $LASTEXITCODE
            $RenderText = (@($RenderOutput) | ForEach-Object { [string]$_ }) -join "`n"
            $RenderCreated = Test-Path -LiteralPath $RenderPath
            $NativeSourceSeen = $RenderText -match
                "intro-opening-render-source presents=1 license=1 chr=1 schema=TISC-1 loose_trace=0"
            $StateSeen = $RenderText.Contains($OpeningCase.state)
            $ModePassed = $RenderExitCode -eq 0 -and $RenderCreated -and
                $NativeSourceSeen -and $StateSeen
            if (!$ModePassed) { $OpeningRenderPassed = $false }
            $OpeningRenderOutputs.Add([pscustomobject]@{
                mode = $OpeningCase.mode
                output = Get-RepoRelativePath $RenderPath
                passed = $ModePassed
                exit_code = $RenderExitCode
                native_source_seen = $NativeSourceSeen
                state_seen = $StateSeen
                loose_trace_disabled = $NativeSourceSeen
                isolated_root = $true
            })
        }
    } finally {
        $env:TECMO_ASSETPACK = $PreviousAssetPack
        if ($null -eq $PreviousLooseIntroTrace) {
            Remove-Item Env:TECMO_ALLOW_LOOSE_INTRO_TRACE -ErrorAction SilentlyContinue
        } else {
            $env:TECMO_ALLOW_LOOSE_INTRO_TRACE = $PreviousLooseIntroTrace
        }
        Remove-Item -LiteralPath $OpeningIsolatedRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
    if (!$OpeningRenderPassed) { ++$Failures }
    $Results.Add([pscustomobject]@{
        id = "intro-opening-rom-only-render"
        passed = $OpeningRenderPassed
        skipped = $false
        outputs = $OpeningRenderOutputs
        rom_only_asset_pack = $AssetPackRelative
        loose_trace_disabled = $true
        isolated_from_trace_and_decomp = $true
        raw_output_persisted = $false
        error = if ($OpeningRenderPassed) { $null } else { "ROM-only TECMO/NBA opening render failed or used loose trace data" }
    })

    $OpeningNegativeSpecs = @(
        [pscustomobject]@{ id = "presents-missing"; entry = "intro/tecmo-presents-screen"; mode = "play-step6"; mutation = "missing"; marker = "presents=0 license=1" },
        [pscustomobject]@{ id = "presents-bad-magic"; entry = "intro/tecmo-presents-screen"; mode = "play-step6"; mutation = "bad-magic"; marker = "presents=0 license=1" },
        [pscustomobject]@{ id = "presents-bad-layout"; entry = "intro/tecmo-presents-screen"; mode = "play-step6"; mutation = "bad-layout"; marker = "presents=0 license=1" },
        [pscustomobject]@{ id = "presents-bad-cell-chr"; entry = "intro/tecmo-presents-screen"; mode = "play-step6"; mutation = "bad-cell-chr"; marker = "presents=0 license=1" },
        [pscustomobject]@{ id = "presents-bad-sprite-reserved"; entry = "intro/tecmo-presents-screen"; mode = "play-step6"; mutation = "bad-sprite-reserved"; marker = "presents=0 license=1" },
        [pscustomobject]@{ id = "license-missing"; entry = "intro/nba-license-screen"; mode = "intro-license"; mutation = "missing"; marker = "presents=1 license=0" },
        [pscustomobject]@{ id = "license-bad-magic"; entry = "intro/nba-license-screen"; mode = "intro-license"; mutation = "bad-magic"; marker = "presents=1 license=0" },
        [pscustomobject]@{ id = "license-bad-layout"; entry = "intro/nba-license-screen"; mode = "play-step7"; mutation = "bad-layout"; marker = "presents=1 license=0" },
        [pscustomobject]@{ id = "license-bad-palette"; entry = "intro/nba-license-screen"; mode = "intro-license"; mutation = "bad-palette"; marker = "presents=1 license=0" },
        [pscustomobject]@{ id = "license-bad-frame"; entry = "intro/nba-license-screen"; mode = "play-step7"; mutation = "bad-frame"; marker = "presents=1 license=0" }
    )
    $OpeningNegativeResults = New-Object System.Collections.Generic.List[object]
    $OpeningNegativePassed = $PackBuildPassed
    $PreviousAssetPack = $env:TECMO_ASSETPACK
    $PreviousLooseIntroTrace = $env:TECMO_ALLOW_LOOSE_INTRO_TRACE
    $OpeningNegativeRoot = Join-Path $OutputDir `
        ("opening_negative_root-" + [Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force -Path $OpeningNegativeRoot | Out-Null
    try {
        Remove-Item Env:TECMO_ALLOW_LOOSE_INTRO_TRACE -ErrorAction SilentlyContinue
        foreach ($NegativeCase in $OpeningNegativeSpecs) {
            [byte[]]$CaseBytes = [System.IO.File]::ReadAllBytes($AssetPackPath)
            $PayloadOffset = Get-AssetPackEntryPayloadOffset `
                -Bytes $CaseBytes -EntryId $NegativeCase.entry
            if ($NegativeCase.mutation -eq "missing") {
                $DirectoryOffset = Get-AssetPackEntryDirectoryOffset `
                    -Bytes $CaseBytes -EntryId $NegativeCase.entry
                $CaseBytes[$DirectoryOffset] = [byte][char]'x'
            } elseif ($NegativeCase.mutation -eq "bad-magic") {
                $CaseBytes[$PayloadOffset] = [byte][char]'X'
            } elseif ($NegativeCase.mutation -eq "bad-layout") {
                $FramesOffset = [System.BitConverter]::ToUInt32($CaseBytes, $PayloadOffset + 32)
                [System.BitConverter]::GetBytes([uint32]($FramesOffset + 1)).CopyTo(
                    $CaseBytes, $PayloadOffset + 32)
            } elseif ($NegativeCase.mutation -eq "bad-cell-chr") {
                $CellsOffset = [System.BitConverter]::ToUInt32($CaseBytes, $PayloadOffset + 20)
                [System.BitConverter]::GetBytes([uint32]::MaxValue).CopyTo(
                    $CaseBytes, $PayloadOffset + $CellsOffset + 2)
            } elseif ($NegativeCase.mutation -eq "bad-sprite-reserved") {
                $SpritesOffset = [System.BitConverter]::ToUInt32($CaseBytes, $PayloadOffset + 44)
                $CaseBytes[$PayloadOffset + $SpritesOffset + 10] = 1
            } elseif ($NegativeCase.mutation -eq "bad-palette") {
                $PalettesOffset = [System.BitConverter]::ToUInt32($CaseBytes, $PayloadOffset + 28)
                $CaseBytes[$PayloadOffset + $PalettesOffset] = 0x40
            } else {
                $FramesOffset = [System.BitConverter]::ToUInt32($CaseBytes, $PayloadOffset + 32)
                $CaseBytes[$PayloadOffset + $FramesOffset + 2] = 35
            }

            $CasePack = Join-Path $OutputDir "$($NegativeCase.id).assetpack"
            $CaseRender = Join-Path $OutputDir "$($NegativeCase.id).png"
            [System.IO.File]::WriteAllBytes($CasePack, $CaseBytes)
            Remove-Item -LiteralPath $CaseRender -Force -ErrorAction SilentlyContinue
            $env:TECMO_ASSETPACK = $CasePack
            $CaseOutput = & $ExePath --root $OpeningNegativeRoot `
                --render-test-mode $NegativeCase.mode $CaseRender 2>&1
            $CaseExitCode = $LASTEXITCODE
            $CaseText = (@($CaseOutput) | ForEach-Object { [string]$_ }) -join "`n"
            $ExpectedSource = "intro-opening-render-source $($NegativeCase.marker) chr=1 schema=TISC-1 loose_trace=0"
            $Rejected = $CaseExitCode -eq 1 -and
                !(Test-Path -LiteralPath $CaseRender) -and
                $CaseText.Contains($ExpectedSource)
            if (!$Rejected) { $OpeningNegativePassed = $false }
            $OpeningNegativeResults.Add([pscustomobject]@{
                id = $NegativeCase.id
                mode = $NegativeCase.mode
                passed = $Rejected
                exit_code = $CaseExitCode
                rejected_by_runtime = $Rejected
            })
            Remove-Item -LiteralPath $CasePack -Force -ErrorAction SilentlyContinue
            Remove-Item -LiteralPath $CaseRender -Force -ErrorAction SilentlyContinue
        }
    } finally {
        $env:TECMO_ASSETPACK = $PreviousAssetPack
        if ($null -eq $PreviousLooseIntroTrace) {
            Remove-Item Env:TECMO_ALLOW_LOOSE_INTRO_TRACE -ErrorAction SilentlyContinue
        } else {
            $env:TECMO_ALLOW_LOOSE_INTRO_TRACE = $PreviousLooseIntroTrace
        }
        Remove-Item -LiteralPath $OpeningNegativeRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
    if (!$OpeningNegativePassed) { ++$Failures }
    $Results.Add([pscustomobject]@{
        id = "intro-opening-missing-malformed-contract"
        passed = $OpeningNegativePassed
        skipped = $false
        cases = $OpeningNegativeResults
        loose_trace_disabled = $true
        isolated_from_trace_and_decomp = $true
        raw_output_persisted = $false
        error = if ($OpeningNegativePassed) { $null } else { "runtime accepted a missing or malformed TISC entry" }
    })

    $PreviousAssetPack = $env:TECMO_ASSETPACK
    $PreviousLooseArenaCapture = $env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE
    $RenderOutputs = New-Object System.Collections.Generic.List[object]
    $RenderPassed = $ListPassed
    try {
        $env:TECMO_ASSETPACK = $AssetPackPath
        Remove-Item Env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE -ErrorAction SilentlyContinue

        foreach ($RenderCase in $RomOnlyArenaRenderCases) {
            $Mode = $RenderCase.mode
            $RenderDirectory = if ($RenderCase.checkpoint) { $CheckpointOutputDir } else { $OutputDir }
            $RenderPath = Join-Path $RenderDirectory "$Mode.png"
            if (Test-Path -LiteralPath $RenderPath) {
                Remove-Item -LiteralPath $RenderPath -Force
            }
            $RenderOutput = & $ExePath --root $ProjectRoot --render-test-mode $Mode $RenderPath 2>&1
            $RenderExitCode = $LASTEXITCODE
            $RenderText = (@($RenderOutput) | ForEach-Object { [string]$_ }) -join "`n"
            $RenderCreated = Test-Path -LiteralPath $RenderPath
            $CaptureUnavailableSeen = $RenderText -match "intro-capture-status kind=arena available=0"
            $NoCaptureSourceSeen = $RenderText -match "intro-capture-source kind=arena assetpack=0 entry=none"
            $ExactLayerSeen = $RenderText -match "intro-arena-render-source kind=arena exact_layer=1 rendered=1 cells=1632 palette=16"
            $TasgContractSeen = $RenderText -match "sprite_groups=2 jumbotron_pieces=55 goal_pieces=16"
            $VisibilityMatch = [regex]::Match(
                $RenderText,
                "visible_jumbotron=([0-9]+) visible_goal=([0-9]+)")
            $ActualJumbotron = if ($VisibilityMatch.Success) {
                [int]$VisibilityMatch.Groups[1].Value
            } else {
                -1
            }
            $ActualGoal = if ($VisibilityMatch.Success) {
                [int]$VisibilityMatch.Groups[2].Value
            } else {
                -1
            }
            $ExactGoalSeen = $ActualGoal -eq [int]$RenderCase.expected_goal
            $ExactJumbotronSeen = $null -eq $RenderCase.expected_jumbotron -or
                $ActualJumbotron -eq [int]$RenderCase.expected_jumbotron
            $FrameVisibilitySeen = $VisibilityMatch.Success -and
                $ExactGoalSeen -and $ExactJumbotronSeen
            $ModePassed = $RenderExitCode -eq 0 -and
                $RenderCreated -and
                $CaptureUnavailableSeen -and
                $NoCaptureSourceSeen -and
                $ExactLayerSeen -and
                $TasgContractSeen -and
                $FrameVisibilitySeen
            if (!$ModePassed) {
                $RenderPassed = $false
            }
            $RenderOutputs.Add([pscustomobject]@{
                mode = $Mode
                output = Get-RepoRelativePath $RenderPath
                passed = $ModePassed
                exit_code = $RenderExitCode
                capture_unavailable_seen = $CaptureUnavailableSeen
                no_capture_source_seen = $NoCaptureSourceSeen
                exact_layer_seen = $ExactLayerSeen
                tasg_contract_seen = $TasgContractSeen
                visible_jumbotron = $ActualJumbotron
                expected_jumbotron = $RenderCase.expected_jumbotron
                visible_goal = $ActualGoal
                expected_goal = $RenderCase.expected_goal
                frame_visibility_seen = $FrameVisibilitySeen
                checkpoint = [bool]$RenderCase.checkpoint
            })
        }
    } finally {
        $env:TECMO_ASSETPACK = $PreviousAssetPack
        $env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE = $PreviousLooseArenaCapture
    }
    if (!$RenderPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "intro-render-rom-only-arena"
        passed = $RenderPassed
        skipped = $false
        render_modes = $RomOnlyArenaRenderModes
        outputs = $RenderOutputs
        rom_only_asset_pack = $AssetPackRelative
        raw_output_persisted = $false
        coverage_status = "covered"
        error = if ($RenderPassed) { $null } else { "ROM-only arena render did not complete without capture data" }
    })

    $PostArenaOutputs = New-Object System.Collections.Generic.List[object]
    $PostArenaPassed = $ListPassed
    $PreviousAssetPack = $env:TECMO_ASSETPACK
    $PreviousLooseArenaCapture = $env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE
    try {
        $env:TECMO_ASSETPACK = $AssetPackPath
        Remove-Item Env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE -ErrorAction SilentlyContinue
        foreach ($RenderCase in $PostArenaRenderCases) {
            $RenderPath = Join-Path $OutputDir "$($RenderCase.mode).png"
            if (Test-Path -LiteralPath $RenderPath) {
                Remove-Item -LiteralPath $RenderPath -Force
            }
            $RenderOutput = & $ExePath --root $ProjectRoot --render-test-mode $RenderCase.mode $RenderPath 2>&1
            $RenderExitCode = $LASTEXITCODE
            $RenderText = (@($RenderOutput) | ForEach-Object { [string]$_ }) -join "`n"
            $RenderCreated = Test-Path -LiteralPath $RenderPath
            $NativeSourceSeen = $RenderText -match "intro-post-render-source ready=1 warriors=1 clippers=1 bucks=1 pass=1 chr=1 ready_schema=TRDY-1 warriors_schema=TWAR-1 clippers_schema=TCLP-1 bucks_schema=TBUC-1 pass_schema=TPAS-1"
            $StateSeen = $RenderText.Contains([string]$RenderCase.state)
            $VisualSeen = $false
            if ($RenderCreated) {
                $Bitmap = [System.Drawing.Bitmap]::FromFile($RenderPath)
                try {
                    if ($RenderCase.visual -eq "black") {
                        $VisualSeen = Test-PixelRectColor $Bitmap 64 0 575 479 0 0 0
                    } elseif ($RenderCase.visual -eq "ready") {
                        $VisualSeen = Test-PixelRectHasNonBlack $Bitmap 192 240 447 319
                    } elseif ($RenderCase.visual -eq "clippers-pose") {
                        $VisualSeen = Test-PixelRectHasNonBlack $Bitmap 64 32 575 383
                    } elseif ($RenderCase.visual -eq "pass-late") {
                        $PasserSeen = Test-PixelRectHasNonBlack $Bitmap 160 160 431 351
                        $BallEdgeSeen = Test-PixelRectHasNonBlack $Bitmap 472 144 551 255
                        $VisualSeen = $PasserSeen -and $BallEdgeSeen
                    } elseif ($RenderCase.visual -eq "pass") {
                        $VisualSeen = Test-PixelRectHasNonBlack $Bitmap 64 32 575 447
                    } elseif ($RenderCase.visual -eq "bucks") {
                        $SceneSeen = Test-PixelRectHasNonBlack $Bitmap 64 32 575 383
                        $WordmarkSeen = Test-PixelRectHasNonBlack $Bitmap 192 416 447 447
                        $VisualSeen = $SceneSeen -and $WordmarkSeen
                    } else {
                        $PlayersSeen = Test-PixelRectHasNonBlack $Bitmap 64 80 575 327
                        $WordmarkSeen = Test-PixelRectHasNonBlack $Bitmap 192 416 447 447
                        $VisualSeen = $PlayersSeen -and $WordmarkSeen
                    }
                } finally {
                    $Bitmap.Dispose()
                }
            }
            $ModePassed = $RenderExitCode -eq 0 -and $RenderCreated -and
                $NativeSourceSeen -and $StateSeen -and $VisualSeen
            if (!$ModePassed) {
                $PostArenaPassed = $false
            }
            $PostArenaOutputs.Add([pscustomobject]@{
                mode = $RenderCase.mode
                output = Get-RepoRelativePath $RenderPath
                passed = $ModePassed
                exit_code = $RenderExitCode
                native_source_seen = $NativeSourceSeen
                state_seen = $StateSeen
                visual_signature_seen = $VisualSeen
            })
        }
    } finally {
        $env:TECMO_ASSETPACK = $PreviousAssetPack
        $env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE = $PreviousLooseArenaCapture
    }
    if (!$PostArenaPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "intro-render-rom-only-post-arena"
        passed = $PostArenaPassed
        skipped = $false
        outputs = $PostArenaOutputs
        rom_only_asset_pack = $AssetPackRelative
        raw_output_persisted = $false
        coverage_status = "ready-warriors-clippers-bucks-pass-native-checkpoints"
        error = if ($PostArenaPassed) { $null } else { "ROM-only post-arena render or timing checkpoint failed" }
    })

    $FinaleOutputs = New-Object System.Collections.Generic.List[object]
    $FinalePassed = $ListPassed
    $PreviousAssetPack = $env:TECMO_ASSETPACK
    try {
        $env:TECMO_ASSETPACK = $AssetPackPath
        foreach ($RenderCase in $FinaleRenderCases) {
            $RenderPath = Join-Path $OutputDir "$($RenderCase.mode).png"
            if (Test-Path -LiteralPath $RenderPath) {
                Remove-Item -LiteralPath $RenderPath -Force
            }
            $RenderOutput = & $ExePath --root $ProjectRoot `
                --render-test-mode $RenderCase.mode $RenderPath 2>&1
            $RenderExitCode = $LASTEXITCODE
            $RenderText = (@($RenderOutput) | ForEach-Object { [string]$_ }) -join "`n"
            $RenderCreated = Test-Path -LiteralPath $RenderPath
            $NativeSourceSeen = $RenderText -match `
                "intro-finale-render-source finale=1 chr=1 schema=TFIN-1"
            $StateLines = @($RenderOutput | ForEach-Object { [string]$_ } |
                Where-Object { $_ -like "intro-finale-state *" })
            $StateSeen = $StateLines.Count -eq 1 -and
                $StateLines[0] -ceq [string]$RenderCase.state
            $VisualSeen = $false
            if ($RenderCreated) {
                $Bitmap = [System.Drawing.Bitmap]::FromFile($RenderPath)
                try {
                    if ($RenderCase.visual -eq "black") {
                        $VisualSeen = Test-PixelRectColor $Bitmap 64 0 575 479 0 0 0
                    } else {
                        $VisualSeen = Test-PixelRectHasNonBlack $Bitmap 64 0 575 479
                    }
                } finally {
                    $Bitmap.Dispose()
                }
            }
            $ModePassed = $RenderExitCode -eq 0 -and $RenderCreated -and
                $NativeSourceSeen -and $StateSeen -and $VisualSeen
            if (!$ModePassed) { $FinalePassed = $false }
            $FinaleOutputs.Add([pscustomobject]@{
                mode = $RenderCase.mode
                output = Get-RepoRelativePath $RenderPath
                passed = $ModePassed
                exit_code = $RenderExitCode
                native_source_seen = $NativeSourceSeen
                state_seen = $StateSeen
                visual_signature_seen = $VisualSeen
            })
        }
    } finally {
        $env:TECMO_ASSETPACK = $PreviousAssetPack
    }
    if (!$FinalePassed) { ++$Failures }
    $Results.Add([pscustomobject]@{
        id = "intro-render-rom-only-finale-title"
        passed = $FinalePassed
        skipped = $false
        outputs = $FinaleOutputs
        rom_only_asset_pack = $AssetPackRelative
        raw_output_persisted = $false
        coverage_status = "five-finale-scenes-title-scroll-and-terminator-hold"
        error = if ($FinalePassed) { $null } else { "ROM-only finale/title render or timing checkpoint failed" }
    })

    $FinaleNegativeResults = New-Object System.Collections.Generic.List[object]
    $FinaleNegativePassed = $PackBuildPassed
    $PreviousAssetPack = $env:TECMO_ASSETPACK
    try {
        foreach ($NegativeCase in @(
            "missing", "bad-magic", "bad-slot-order", "bad-chr", "bad-route-wait",
            "bad-route-screen-order", "bad-reverse-initial", "bad-reverse-second",
            "bad-reverse-delta", "bad-reverse-y", "bad-band-layout",
            "bad-band-channel", "overlapping-sections", "bad-piece-bottom",
            "bad-one-page-mirror", "bad-alignment-padding")) {
            $CasePack = Join-Path $OutputDir "finale-$NegativeCase.assetpack"
            $CaseRender = Join-Path $OutputDir "finale-$NegativeCase.png"
            [byte[]]$CaseBytes = [System.IO.File]::ReadAllBytes($AssetPackPath)
            $PayloadOffset = Get-AssetPackEntryPayloadOffset `
                -Bytes $CaseBytes -EntryId "intro/finale-sequence"
            if ($NegativeCase -eq "missing") {
                $DirectoryOffset = Get-AssetPackEntryDirectoryOffset `
                    -Bytes $CaseBytes -EntryId "intro/finale-sequence"
                $CaseBytes[$DirectoryOffset] = [byte][char]'x'
            } elseif ($NegativeCase -eq "bad-magic") {
                $CaseBytes[$PayloadOffset] = [byte][char]'X'
            } elseif ($NegativeCase -eq "bad-slot-order") {
                $SlotsOffset = [System.BitConverter]::ToUInt32($CaseBytes, $PayloadOffset + 96)
                $CaseBytes[$PayloadOffset + $SlotsOffset + 1] = 1
            } elseif ($NegativeCase -eq "bad-chr") {
                $ScreensOffset = [System.BitConverter]::ToUInt32($CaseBytes, $PayloadOffset + 20)
                [System.BitConverter]::GetBytes([uint32]262144).CopyTo(
                    $CaseBytes, $PayloadOffset + $ScreensOffset + 2)
            } elseif ($NegativeCase -eq "bad-route-wait") {
                $RoutesOffset = [System.BitConverter]::ToUInt32($CaseBytes, $PayloadOffset + 64)
                $CaseBytes[$PayloadOffset + $RoutesOffset + 6] = 49
            } elseif ($NegativeCase -eq "bad-route-screen-order") {
                $RoutesOffset = [System.BitConverter]::ToUInt32($CaseBytes, $PayloadOffset + 64)
                $CaseBytes[$PayloadOffset + $RoutesOffset] = 1
                $CaseBytes[$PayloadOffset + $RoutesOffset + 8] = 0
            } elseif ($NegativeCase -like "bad-reverse-*") {
                $ReverseOffset = [System.BitConverter]::ToUInt32($CaseBytes, $PayloadOffset + 80)
                $ReverseIndex = switch ($NegativeCase) {
                    "bad-reverse-initial" { 0 }
                    "bad-reverse-second" { 1 }
                    "bad-reverse-delta" { 2 }
                    default { 3 }
                }
                $CaseBytes[$PayloadOffset + $ReverseOffset + $ReverseIndex] =
                    [byte]($CaseBytes[$PayloadOffset + $ReverseOffset + $ReverseIndex] -bxor 1)
            } elseif ($NegativeCase -eq "bad-band-layout") {
                $BandsOffset = [System.BitConverter]::ToUInt32($CaseBytes, $PayloadOffset + 104)
                [System.BitConverter]::GetBytes([uint16]199).CopyTo(
                    $CaseBytes, $PayloadOffset + $BandsOffset + 2)
                [System.BitConverter]::GetBytes([uint16]199).CopyTo(
                    $CaseBytes, $PayloadOffset + $BandsOffset + 16)
            } elseif ($NegativeCase -eq "bad-band-channel") {
                $BandsOffset = [System.BitConverter]::ToUInt32($CaseBytes, $PayloadOffset + 104)
                $CaseBytes[$PayloadOffset + $BandsOffset + 4] = 1
                $CaseBytes[$PayloadOffset + $BandsOffset + 5] = 1
            } elseif ($NegativeCase -eq "overlapping-sections") {
                $BackgroundPalettesOffset = [System.BitConverter]::ToUInt32(
                    $CaseBytes, $PayloadOffset + 24)
                [System.BitConverter]::GetBytes([uint32]$BackgroundPalettesOffset).CopyTo(
                    $CaseBytes, $PayloadOffset + 32)
            } elseif ($NegativeCase -eq "bad-piece-bottom") {
                $PiecesOffset = [System.BitConverter]::ToUInt32($CaseBytes, $PayloadOffset + 56)
                $TopOffset = [System.BitConverter]::ToUInt32(
                    $CaseBytes, $PayloadOffset + $PiecesOffset + 4)
                [System.BitConverter]::GetBytes([uint32]($TopOffset + 32)).CopyTo(
                    $CaseBytes, $PayloadOffset + $PiecesOffset + 8)
            } elseif ($NegativeCase -eq "bad-one-page-mirror") {
                $ScreensOffset = [System.BitConverter]::ToUInt32($CaseBytes, $PayloadOffset + 20)
                $MirrorCellOffset = $PayloadOffset + $ScreensOffset + 960 * 6
                $CaseBytes[$MirrorCellOffset] = [byte]($CaseBytes[$MirrorCellOffset] -bxor 1)
            } else {
                $GroupsOffset = [System.BitConverter]::ToUInt32($CaseBytes, $PayloadOffset + 44)
                $CaseBytes[$PayloadOffset + $GroupsOffset - 1] = 1
            }
            [System.IO.File]::WriteAllBytes($CasePack, $CaseBytes)
            Remove-Item -LiteralPath $CaseRender -Force -ErrorAction SilentlyContinue
            $env:TECMO_ASSETPACK = $CasePack
            $CaseOutput = & $ExePath --root $ProjectRoot `
                --render-test-mode intro-finale-opening-clean-frame0 $CaseRender 2>&1
            $CaseExitCode = $LASTEXITCODE
            $CaseText = (@($CaseOutput) | ForEach-Object { [string]$_ }) -join "`n"
            $Rejected = $CaseExitCode -eq 1 -and
                !(Test-Path -LiteralPath $CaseRender) -and
                $CaseText -match "intro-finale-render-source finale=0 chr=1 schema=TFIN-1"
            if (!$Rejected) { $FinaleNegativePassed = $false }
            $FinaleNegativeResults.Add([pscustomobject]@{
                id = $NegativeCase
                passed = $Rejected
                exit_code = $CaseExitCode
                rejected_by_runtime = $Rejected
            })
            Remove-Item -LiteralPath $CasePack -Force -ErrorAction SilentlyContinue
            Remove-Item -LiteralPath $CaseRender -Force -ErrorAction SilentlyContinue
        }

        $env:TECMO_ASSETPACK = $AssetPackPath
        foreach ($InvalidMode in @(
            "intro-finale-opening-clean-frame51", "intro-finale-opening-frame51",
            "intro-finale-short-clean-frame47", "intro-finale-short-frame47",
            "intro-finale-reverse-clean-frame52", "intro-finale-reverse-frame52",
            "intro-finale-staged-clean-frame156", "intro-finale-staged-frame156",
            "intro-finale-title-clean-frame603", "intro-finale-title-frame603",
            "intro-finale-opening-frame909", "intro-finale-short-frame47junk",
            "intro-finale-title-frame4294967296")) {
            $CaseRender = Join-Path $OutputDir "invalid-finale-local-mode.png"
            Remove-Item -LiteralPath $CaseRender -Force -ErrorAction SilentlyContinue
            $CaseOutput = & $ExePath --root $ProjectRoot `
                --render-test-mode $InvalidMode $CaseRender 2>&1
            $CaseExitCode = $LASTEXITCODE
            $CaseText = (@($CaseOutput) | ForEach-Object { [string]$_ }) -join "`n"
            $Rejected = $CaseExitCode -eq 1 -and
                !(Test-Path -LiteralPath $CaseRender) -and
                $CaseText.Contains("Unsupported render-test mode: $InvalidMode")
            if (!$Rejected) { $FinaleNegativePassed = $false }
            $FinaleNegativeResults.Add([pscustomobject]@{
                id = "invalid-local-mode-$InvalidMode"
                passed = $Rejected
                exit_code = $CaseExitCode
                rejected_by_runtime = $Rejected
            })
        }
    } finally {
        $env:TECMO_ASSETPACK = $PreviousAssetPack
    }
    if (!$FinaleNegativePassed) { ++$Failures }
    $Results.Add([pscustomobject]@{
        id = "intro-finale-missing-malformed-contract"
        passed = $FinaleNegativePassed
        skipped = $false
        cases = $FinaleNegativeResults
        raw_output_persisted = $false
        coverage_status = "missing-schema-layout-metadata-chr-mirroring-padding-and-local-mode-bounds"
        error = if ($FinaleNegativePassed) { $null } else { "runtime accepted a missing or malformed TFIN entry" }
    })

    $BoundedReferenceOutputs = New-Object System.Collections.Generic.List[object]
    $BoundedReferencePassed = $true
    $BoundedReferenceCompared = 0
    foreach ($Case in $BoundedReferenceCases) {
        $HostPath = Join-Path $OutputDir "$($Case.mode).png"
        $ReferencePath = Join-Path $BuildDir $Case.reference
        if (!(Test-Path -LiteralPath $ReferencePath)) {
            $BoundedReferenceOutputs.Add([pscustomobject]@{
                mode = $Case.mode
                reference = $Case.reference
                passed = $true
                skipped = $true
                reason = "bounded reference PNG unavailable"
            })
            continue
        }
        ++$BoundedReferenceCompared
        $HostBitmap = [System.Drawing.Bitmap]::FromFile($HostPath)
        $ReferenceBitmap = [System.Drawing.Bitmap]::FromFile($ReferencePath)
        try {
            $Comparison = Compare-NativeViewportMask $HostBitmap $ReferenceBitmap
        } finally {
            $HostBitmap.Dispose()
            $ReferenceBitmap.Dispose()
        }
        if (!$Comparison.passed) { $BoundedReferencePassed = $false }
        $BoundedReferenceOutputs.Add([pscustomobject]@{
            mode = $Case.mode
            reference = $Case.reference
            passed = $Comparison.passed
            skipped = $false
            mask_iou = $Comparison.mask_iou
            host_nonblack = $Comparison.host_nonblack
            reference_nonblack = $Comparison.reference_nonblack
            host_bounds = $Comparison.host_bounds
            reference_bounds = $Comparison.reference_bounds
        })
    }
    if (!$BoundedReferencePassed) { ++$Failures }
    if ($BoundedReferenceCompared -eq 0) { ++$Skipped }
    $Results.Add([pscustomobject]@{
        id = "intro-bucks-pass-bounded-pixel-masks"
        passed = $BoundedReferencePassed
        skipped = $BoundedReferenceCompared -eq 0
        compared = $BoundedReferenceCompared
        outputs = $BoundedReferenceOutputs
        raw_output_persisted = $false
        coverage_status = if ($BoundedReferenceCompared -eq 0) { "local-reference-unavailable" } else { "covered" }
        error = if ($BoundedReferencePassed) { $null } else { "native viewport mask differed from bounded BUCKS/PASS reference" }
    })

    $LowerBandGeometryOutputs = New-Object System.Collections.Generic.List[object]
    $LowerBandGeometryPassed = $true
    $PreviousAssetPack = $env:TECMO_ASSETPACK
    $PreviousLooseArenaCapture = $env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE
    try {
        $env:TECMO_ASSETPACK = $AssetPackPath
        Remove-Item Env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE -ErrorAction SilentlyContinue
        foreach ($GeometryCase in $LowerBandGeometryCases) {
            $Mode = "intro-arena-clean-frame$($GeometryCase.frame)"
            $RenderPath = Join-Path $CheckpointOutputDir "$Mode.png"
            if (Test-Path -LiteralPath $RenderPath) {
                Remove-Item -LiteralPath $RenderPath -Force
            }
            $RenderOutput = & $ExePath `
                --root $ProjectRoot `
                --render-test-mode $Mode $RenderPath 2>&1
            $RenderExitCode = $LASTEXITCODE
            $RenderText = (@($RenderOutput) | ForEach-Object { [string]$_ }) -join "`n"
            $RenderCreated = Test-Path -LiteralPath $RenderPath
            $ExactLayerSeen = $RenderText -match
                "intro-arena-render-source kind=arena exact_layer=1 rendered=1 cells=1632 palette=16"
            $OriginDoesNotMatchBoundary = $false
            $BoundaryRed = $false
            $BoundaryBlack = $false

            if ($RenderCreated) {
                $Bitmap = [System.Drawing.Bitmap]::FromFile($RenderPath)
                try {
                    $OriginDoesNotMatchBoundary = !(Test-PixelRectColor `
                        $Bitmap 64 $GeometryCase.origin_y 191 ($GeometryCase.origin_y + 1) `
                        228 0 88)
                    $BoundaryRed = Test-PixelRectColor `
                        $Bitmap 64 $GeometryCase.clip_y 191 ($GeometryCase.clip_y + 1) `
                        228 0 88
                    $BoundaryBlack = Test-PixelRectColor `
                        $Bitmap 64 ($GeometryCase.clip_y + 2) 191 ($GeometryCase.clip_y + 3) `
                        0 0 0
                } finally {
                    $Bitmap.Dispose()
                }
            }
            $ModePassed = $RenderExitCode -eq 0 -and
                $RenderCreated -and
                $ExactLayerSeen -and
                $OriginDoesNotMatchBoundary -and
                $BoundaryRed -and
                $BoundaryBlack
            if (!$ModePassed) {
                $LowerBandGeometryPassed = $false
            }
            $LowerBandGeometryOutputs.Add([pscustomobject]@{
                frame = [int]$GeometryCase.frame
                mode = $Mode
                output = Get-RepoRelativePath $RenderPath
                passed = $ModePassed
                exit_code = $RenderExitCode
                scroll_0301_hex = $GeometryCase.scroll
                motion_counter_88_hex = $GeometryCase.motion
                expected_origin_output_y = [int]$GeometryCase.origin_y
                expected_clip_output_y = [int]$GeometryCase.clip_y
                origin_does_not_match_boundary = $OriginDoesNotMatchBoundary
                boundary_red_signature = $BoundaryRed
                boundary_black_signature = $BoundaryBlack
            })
        }
    } finally {
        $env:TECMO_ASSETPACK = $PreviousAssetPack
        $env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE = $PreviousLooseArenaCapture
    }
    if (!$LowerBandGeometryPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "intro-arena-dynamic-lower-band-geometry"
        passed = $LowerBandGeometryPassed
        skipped = $false
        outputs = $LowerBandGeometryOutputs
        raw_output_persisted = $false
        coverage_status = "clean-frame-irq-boundary-progression"
        error = if ($LowerBandGeometryPassed) { $null } else { "lower-band IRQ origin progression changed" }
    })

    $MalformedCleanModeOutputs = New-Object System.Collections.Generic.List[object]
    $MalformedCleanModesPassed = $true
    $PreviousAssetPack = $env:TECMO_ASSETPACK
    $PreviousLooseArenaCapture = $env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE
    try {
        $env:TECMO_ASSETPACK = $AssetPackPath
        Remove-Item Env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE -ErrorAction SilentlyContinue
        for ($CaseIndex = 0; $CaseIndex -lt $MalformedCleanArenaModes.Count; ++$CaseIndex) {
            $Mode = $MalformedCleanArenaModes[$CaseIndex]
            $RenderPath = Join-Path $OutputDir "malformed-clean-arena-mode-$CaseIndex.png"
            if (Test-Path -LiteralPath $RenderPath) {
                Remove-Item -LiteralPath $RenderPath -Force
            }
            $RenderOutput = & $ExePath `
                --root $ProjectRoot `
                --render-test-mode $Mode $RenderPath 2>&1
            $RenderExitCode = $LASTEXITCODE
            $RenderText = (@($RenderOutput) | ForEach-Object { [string]$_ }) -join "`n"
            $Rejected = $RenderExitCode -eq 1 -and
                !(Test-Path -LiteralPath $RenderPath) -and
                $RenderText -match "Unsupported render-test mode: $([regex]::Escape($Mode))"
            if (!$Rejected) {
                $MalformedCleanModesPassed = $false
            }
            $MalformedCleanModeOutputs.Add([pscustomobject]@{
                mode = $Mode
                passed = $Rejected
                exit_code = $RenderExitCode
                png_created = Test-Path -LiteralPath $RenderPath
            })
        }
    } finally {
        $env:TECMO_ASSETPACK = $PreviousAssetPack
        $env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE = $PreviousLooseArenaCapture
    }
    if (!$MalformedCleanModesPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "intro-arena-clean-frame-mode-validation"
        passed = $MalformedCleanModesPassed
        skipped = $false
        cases = $MalformedCleanModeOutputs
        raw_output_persisted = $false
        coverage_status = "strict-decimal-suffix"
        error = if ($MalformedCleanModesPassed) { $null } else { "malformed clean arena frame mode was accepted" }
    })

    $CleanFinalMode = "intro-arena-clean-frame539"
    $CleanFinalPath = Join-Path $CheckpointOutputDir "$CleanFinalMode.png"
    $CleanFinalPassed = $false
    $CleanFinalExitCode = -1
    $CleanFinalCreated = $false
    $CleanFinalSourceSeen = $false
    $CleanFinalGoalCountSeen = $false
    $PostGrayLeft = $false
    $PostWhiteCenter = $false
    $PostGrayRight = $false
    $OpeningBlack = $false
    $OpeningCreamLeft = $false
    $OpeningCreamRight = $false
    $CreamCap = $false
    $PreviousAssetPack = $env:TECMO_ASSETPACK
    $PreviousLooseArenaCapture = $env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE
    try {
        if (Test-Path -LiteralPath $CleanFinalPath) {
            Remove-Item -LiteralPath $CleanFinalPath -Force
        }
        $env:TECMO_ASSETPACK = $AssetPackPath
        Remove-Item Env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE -ErrorAction SilentlyContinue
        $CleanFinalOutput = & $ExePath `
            --root $ProjectRoot `
            --render-test-mode $CleanFinalMode $CleanFinalPath 2>&1
        $CleanFinalExitCode = $LASTEXITCODE
        $CleanFinalText = (@($CleanFinalOutput) | ForEach-Object { [string]$_ }) -join "`n"
        $CleanFinalCreated = Test-Path -LiteralPath $CleanFinalPath
        $CleanFinalSourceSeen = $CleanFinalText -match
            "intro-arena-render-source kind=arena exact_layer=1 rendered=1 cells=1632 palette=16"
        $CleanFinalGoalCountSeen = $CleanFinalText -match
            "visible_jumbotron=0 visible_goal=16"

        if ($CleanFinalCreated) {
            $Bitmap = [System.Drawing.Bitmap]::FromFile($CleanFinalPath)
            try {
                # Final ROM registration: post ends at 429, the opening starts
                # at 430, and the lower-band cream cap starts at 432.
                $PostGrayLeft = Test-PixelRectColor $Bitmap 430 428 431 429 124 124 124
                $PostWhiteCenter = Test-PixelRectColor $Bitmap 432 428 433 429 252 252 252
                $PostGrayRight = Test-PixelRectColor $Bitmap 434 428 435 429 124 124 124
                $OpeningBlack = Test-PixelRectColor $Bitmap 424 430 441 431 0 0 0
                $OpeningCreamLeft = Test-PixelRectColor $Bitmap 420 430 423 431 252 224 168
                $OpeningCreamRight = Test-PixelRectColor $Bitmap 442 430 445 431 252 224 168
                $CreamCap = Test-PixelRectColor $Bitmap 424 432 441 433 252 224 168
            } finally {
                $Bitmap.Dispose()
            }
        }
        $CleanFinalPassed = $CleanFinalExitCode -eq 0 -and
            $CleanFinalCreated -and
            $CleanFinalSourceSeen -and
            $CleanFinalGoalCountSeen -and
            $PostGrayLeft -and
            $PostWhiteCenter -and
            $PostGrayRight -and
            $OpeningBlack -and
            $OpeningCreamLeft -and
            $OpeningCreamRight -and
            $CreamCap
    } finally {
        $env:TECMO_ASSETPACK = $PreviousAssetPack
        $env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE = $PreviousLooseArenaCapture
    }
    if (!$CleanFinalPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "intro-arena-rom-exact-final-registration"
        passed = $CleanFinalPassed
        skipped = $false
        mode = $CleanFinalMode
        output = Get-RepoRelativePath $CleanFinalPath
        exit_code = $CleanFinalExitCode
        exact_layer_seen = $CleanFinalSourceSeen
        final_goal_count_seen = $CleanFinalGoalCountSeen
        post_gray_left = $PostGrayLeft
        post_white_center = $PostWhiteCenter
        post_gray_right = $PostGrayRight
        opening_black = $OpeningBlack
        opening_cream_left = $OpeningCreamLeft
        opening_cream_right = $OpeningCreamRight
        cream_cap = $CreamCap
        raw_output_persisted = $false
        coverage_status = "clean-frame-local-relational-pixels"
        error = if ($CleanFinalPassed) { $null } else { "frame 539 post/opening/cap registration changed" }
    })

    $MalformedTasgCases = @(
        [pscustomobject]@{
            id = "top-tile-before-page-8"
            mutation = "u32"
            payload_offset = 108
            value = 8176
        },
        [pscustomobject]@{
            id = "bottom-tile-after-page-9"
            mutation = "u32"
            payload_offset = 108
            value = 10224
        },
        [pscustomobject]@{
            id = "priority-flag-unsupported"
            mutation = "or-byte"
            payload_offset = 113
            value = 0x04
        },
        [pscustomobject]@{
            id = "reserved-flag-unsupported"
            mutation = "or-byte"
            payload_offset = 113
            value = 0x08
        },
        [pscustomobject]@{
            id = "jumbotron-connector-overlay-unsupported"
            mutation = "i16"
            payload_offset = 114
            value = -1
        },
        [pscustomobject]@{
            id = "goal-connector-overlay-required"
            mutation = "i16"
            payload_offset = 858
            value = 0
        },
        [pscustomobject]@{
            id = "goal-connector-overlay-value-strict"
            mutation = "i16"
            payload_offset = 858
            value = -2
        }
    )
    $MalformedPackPath = Join-Path $OutputDir "malformed-tasg.assetpack"
    $MalformedRenderPath = Join-Path $OutputDir "malformed-tasg.png"
    $MalformedCaseResults = New-Object System.Collections.Generic.List[object]
    $MalformedContractPassed = $PackBuildPassed
    $PreviousAssetPack = $env:TECMO_ASSETPACK
    try {
        if ($PackBuildPassed) {
            foreach ($Case in $MalformedTasgCases) {
                [byte[]]$MalformedBytes = [System.IO.File]::ReadAllBytes($AssetPackPath)
                $TasgPayloadOffset = Get-AssetPackEntryPayloadOffset `
                    -Bytes $MalformedBytes `
                    -EntryId "arena/intro/sprite-groups"
                $MutationOffset = $TasgPayloadOffset + $Case.payload_offset
                if ($Case.mutation -eq "u32") {
                    [System.BitConverter]::GetBytes([uint32]$Case.value).CopyTo(
                        $MalformedBytes,
                        $MutationOffset)
                } elseif ($Case.mutation -eq "i16") {
                    [System.BitConverter]::GetBytes([int16]$Case.value).CopyTo(
                        $MalformedBytes,
                        $MutationOffset)
                } else {
                    $MalformedBytes[$MutationOffset] = [byte](
                        $MalformedBytes[$MutationOffset] -bor [byte]$Case.value)
                }
                [System.IO.File]::WriteAllBytes($MalformedPackPath, $MalformedBytes)

                if (Test-Path -LiteralPath $MalformedRenderPath) {
                    Remove-Item -LiteralPath $MalformedRenderPath -Force
                }
                $env:TECMO_ASSETPACK = $MalformedPackPath
                $MalformedRenderOutput = & $ExePath `
                    --root $ProjectRoot `
                    --render-test-mode intro-arena-frame0 $MalformedRenderPath 2>&1
                $MalformedRenderExitCode = $LASTEXITCODE
                $MalformedRenderText = (@($MalformedRenderOutput) | ForEach-Object { [string]$_ }) -join "`n"
                $Rejected = $MalformedRenderExitCode -eq 1 -and
                    !(Test-Path -LiteralPath $MalformedRenderPath) -and
                    $MalformedRenderText -match "intro-arena-render-source kind=arena exact_layer=1 rendered=0 cells=1632 palette=16 sprite_groups=0 jumbotron_pieces=0 goal_pieces=0 visible_jumbotron=0 visible_goal=0"
                if (!$Rejected) {
                    $MalformedContractPassed = $false
                }
                $MalformedCaseResults.Add([pscustomobject]@{
                    id = $Case.id
                    passed = $Rejected
                    exit_code = $MalformedRenderExitCode
                    rejected_by_runtime = $Rejected
                })
            }
        }
    } finally {
        $env:TECMO_ASSETPACK = $PreviousAssetPack
        Remove-Item -LiteralPath $MalformedPackPath -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $MalformedRenderPath -Force -ErrorAction SilentlyContinue
    }
    if (!$MalformedContractPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "intro-tasg-malformed-contract"
        passed = $MalformedContractPassed
        skipped = $false
        cases = $MalformedCaseResults
        raw_output_persisted = $false
        coverage_status = "covered"
        error = if ($MalformedContractPassed) { $null } else { "runtime accepted a malformed TASG sprite contract" }
    })

    $MalformedTclpPackPath = Join-Path $OutputDir "malformed-tclp.assetpack"
    $MalformedTclpRenderPath = Join-Path $OutputDir "malformed-tclp.png"
    $MalformedTclpPassed = $PackBuildPassed
    $PreviousAssetPack = $env:TECMO_ASSETPACK
    try {
        if ($PackBuildPassed) {
            [byte[]]$MalformedTclpBytes = [System.IO.File]::ReadAllBytes($AssetPackPath)
            $TclpPayloadOffset = Get-AssetPackEntryPayloadOffset `
                -Bytes $MalformedTclpBytes `
                -EntryId "arena/intro/clippers-transition"
            [System.BitConverter]::GetBytes([uint32]0).CopyTo(
                $MalformedTclpBytes,
                $TclpPayloadOffset + 162)
            [System.IO.File]::WriteAllBytes($MalformedTclpPackPath, $MalformedTclpBytes)
            $env:TECMO_ASSETPACK = $MalformedTclpPackPath
            $MalformedTclpOutput = & $ExePath `
                --root $ProjectRoot `
                --render-test-mode intro-clippers-clean-frame40 $MalformedTclpRenderPath 2>&1
            $MalformedTclpExitCode = $LASTEXITCODE
            $MalformedTclpText = (@($MalformedTclpOutput) | ForEach-Object { [string]$_ }) -join "`n"
            $MalformedTclpPassed = $MalformedTclpExitCode -eq 1 -and
                !(Test-Path -LiteralPath $MalformedTclpRenderPath) -and
                $MalformedTclpText -match "clippers=0"
        }
    } finally {
        $env:TECMO_ASSETPACK = $PreviousAssetPack
        Remove-Item -LiteralPath $MalformedTclpPackPath -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $MalformedTclpRenderPath -Force -ErrorAction SilentlyContinue
    }
    if (!$MalformedTclpPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "intro-tclp-malformed-chr-contract"
        passed = $MalformedTclpPassed
        skipped = $false
        raw_output_persisted = $false
        coverage_status = "covered"
        error = if ($MalformedTclpPassed) { $null } else { "runtime accepted a malformed TCLP CHR offset" }
    })

    $MalformedNativeCases = @(
        [pscustomobject]@{ id = "bucks"; entry = "arena/intro/bucks-transition"; offset = 162; mode = "intro-bucks-clean-frame14"; unavailable = "bucks=0" },
        [pscustomobject]@{ id = "pass"; entry = "arena/intro/pass-transition"; offset = 226; mode = "intro-pass-clean-frame27"; unavailable = "pass=0" }
    )
    $MalformedNativeResults = New-Object System.Collections.Generic.List[object]
    $MalformedNativePassed = $PackBuildPassed
    $PreviousAssetPack = $env:TECMO_ASSETPACK
    try {
        foreach ($Case in $MalformedNativeCases) {
            $CasePack = Join-Path $OutputDir "malformed-$($Case.id).assetpack"
            $CaseRender = Join-Path $OutputDir "malformed-$($Case.id).png"
            [byte[]]$CaseBytes = [System.IO.File]::ReadAllBytes($AssetPackPath)
            $PayloadOffset = Get-AssetPackEntryPayloadOffset -Bytes $CaseBytes -EntryId $Case.entry
            [System.BitConverter]::GetBytes([uint32]0).CopyTo($CaseBytes, $PayloadOffset + $Case.offset)
            [System.IO.File]::WriteAllBytes($CasePack, $CaseBytes)
            $env:TECMO_ASSETPACK = $CasePack
            $CaseOutput = & $ExePath --root $ProjectRoot --render-test-mode $Case.mode $CaseRender 2>&1
            $CaseExitCode = $LASTEXITCODE
            $CaseText = (@($CaseOutput) | ForEach-Object { [string]$_ }) -join "`n"
            $Rejected = $CaseExitCode -eq 1 -and !(Test-Path -LiteralPath $CaseRender) -and
                $CaseText -match [regex]::Escape($Case.unavailable)
            if (!$Rejected) { $MalformedNativePassed = $false }
            $MalformedNativeResults.Add([pscustomobject]@{
                id = $Case.id
                passed = $Rejected
                exit_code = $CaseExitCode
                rejected_by_runtime = $Rejected
            })
            Remove-Item -LiteralPath $CasePack -Force -ErrorAction SilentlyContinue
            Remove-Item -LiteralPath $CaseRender -Force -ErrorAction SilentlyContinue
        }
    } finally {
        $env:TECMO_ASSETPACK = $PreviousAssetPack
    }
    if (!$MalformedNativePassed) { ++$Failures }
    $Results.Add([pscustomobject]@{
        id = "intro-bucks-pass-malformed-chr-contract"
        passed = $MalformedNativePassed
        skipped = $false
        cases = $MalformedNativeResults
        raw_output_persisted = $false
        coverage_status = "covered"
        error = if ($MalformedNativePassed) { $null } else { "runtime accepted malformed TBUC/TPAS CHR offsets" }
    })
} catch {
    ++$Failures
    $Results.Add([pscustomobject]@{
        id = "intro-sequence-runner-error"
        passed = $false
        raw_output_persisted = $false
        private_paths_included = $false
        error = "intro sequence runner failed before completing all checks"
        error_type = Get-SafeExceptionName $_
    })
} finally {
    $Report = [pscustomobject]@{
        schema_version = 1
        generated_by = "tools/Run-IntroSequenceTests.ps1"
        generated_at_utc = [DateTime]::UtcNow.ToString("o")
        data_policy = "Sanitized intro-sequence smoke report only; raw stdout/stderr, local paths, ROM bytes, ASM, CHR bytes, trace payloads, and screenshots are not embedded."
        passed = $Failures -eq 0
        reference_rom_used = "<local>"
        build_requested = [bool]$Build
        output_directory = (Get-RepoRelativePath $OutputDir)
        asset_pack = [pscustomobject]@{
            output = $AssetPackRelative
            rom_only_contract = $true
        }
        render_coverage = [pscustomobject]@{
            status = "arena-rom-exact-d861-and-irq-band-checkpoints"
            render_modes = $RomOnlyArenaRenderModes
            clean_final_mode = $CleanFinalMode
            checkpoint_directory = Get-RepoRelativePath $CheckpointOutputDir
        }
        private_paths_included = $false
        raw_output_persisted = $false
        test_count = $Results.Count
        skipped_count = $Skipped
        failure_count = $Failures
        tests = $Results
    }

    $ReportDir = Split-Path -Parent $ReportPath
    if ($ReportDir) {
        New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null
    }
    $Report | ConvertTo-Json -Depth 8 | Set-Content -Path $ReportPath -Encoding ASCII
}

$Results | Format-Table id, passed, skipped, output -AutoSize
Write-Host "Wrote intro sequence test report: $(Get-RepoRelativePath $ReportPath)"

if ($Failures -ne 0) {
    throw "$Failures intro sequence smoke test(s) failed."
}
Write-Host "All intro sequence smoke tests passed."
$global:LASTEXITCODE = 0
