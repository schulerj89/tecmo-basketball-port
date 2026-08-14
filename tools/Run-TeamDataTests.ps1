param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [string]$DecompRoot,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path $ProjectRoot).Path
$BuildDir = Join-Path $ProjectRoot "build"
$Executable = Join-Path $BuildDir "tecmo_port.exe"

if (!$RomPath) {
    $RomPath = $env:TECMO_ROM_PATH
}
if (!$RomPath) {
    $RomPath = Join-Path (Split-Path -Parent $ProjectRoot) `
        "disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes"
}
if (!(Test-Path $RomPath)) {
    throw "Rev1 ROM not found. Pass -RomPath or set TECMO_ROM_PATH."
}
$RomPath = (Resolve-Path $RomPath).Path

if (!$DecompRoot) {
    $DecompRoot = $env:TECMO_DECOMP_ROOT
}
if (!$DecompRoot) {
    $DecompRoot = Join-Path (Split-Path -Parent $ProjectRoot) `
        "disassem\tecmo-basketball-decompilation"
}
if (!(Test-Path $DecompRoot)) {
    throw "Decompilation root not found. Pass -DecompRoot or set TECMO_DECOMP_ROOT."
}
$DecompRoot = (Resolve-Path $DecompRoot).Path

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
$Scratch = Join-Path $BuildDir ("team_data_test_" + [Guid]::NewGuid().ToString("N"))
$Scratch = [System.IO.Path]::GetFullPath($Scratch)
$BuildPrefix = [System.IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/') + `
    [System.IO.Path]::DirectorySeparatorChar
if (!$Scratch.StartsWith($BuildPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "TEAM DATA scratch path escaped build\."
}
New-Item -ItemType Directory -Path $Scratch | Out-Null

$PreviousSkipShortcut = $env:TECMO_SKIP_SHORTCUT
$PreviousAssetPack = $env:TECMO_ASSETPACK

function Invoke-Tecmo {
    param(
        [string[]]$Arguments,
        [int]$ExpectedExit = 0
    )
    $Output = @(& $Executable @Arguments 2>&1)
    $ExitCode = $LASTEXITCODE
    if ($ExitCode -ne $ExpectedExit) {
        $Tail = @($Output | Select-Object -Last 12) -join [Environment]::NewLine
        throw "tecmo_port exit $ExitCode (expected $ExpectedExit):$([Environment]::NewLine)$Tail"
    }
    return ($Output -join [Environment]::NewLine)
}

function Get-PackEntry {
    param([byte[]]$Bytes, [string]$Id)
    if ($Bytes.Length -lt 40 -or
        [Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -ne "TAP1") {
        throw "Private asset pack header was malformed."
    }
    $EntryCount = [BitConverter]::ToUInt32($Bytes, 16)
    $DirectoryOffset = [BitConverter]::ToUInt64($Bytes, 20)
    for ($Index = 0; $Index -lt $EntryCount; ++$Index) {
        $EntryOffset = [uint64]$DirectoryOffset + [uint64]$Index * 128
        if ($EntryOffset + 128 -gt $Bytes.Length) {
            throw "Private asset pack directory was truncated."
        }
        $EntryId = [Text.Encoding]::ASCII.GetString(
            $Bytes, [int]$EntryOffset, 64).Trim([char]0)
        if ($EntryId -eq $Id) {
            $PayloadOffset = [BitConverter]::ToUInt64(
                $Bytes, [int]$EntryOffset + 84)
            $PayloadSize = [BitConverter]::ToUInt64(
                $Bytes, [int]$EntryOffset + 92)
            if ($PayloadOffset -lt 40 -or
                $PayloadOffset -gt $DirectoryOffset -or
                $PayloadSize -gt $DirectoryOffset - $PayloadOffset) {
                throw "$Id payload bounds were malformed."
            }
            return @{
                Offset = $PayloadOffset
                Size = $PayloadSize
            }
        }
    }
    throw "$Id was missing from the private asset pack."
}

function Assert-StaticShootingPixelLayout {
    param([string]$FirstPath, [string]$SecondPath)

    Add-Type -AssemblyName System.Drawing
    $First = [Drawing.Bitmap]::FromFile($FirstPath)
    $Second = [Drawing.Bitmap]::FromFile($SecondPath)
    try {
        if ($First.Width -ne 640 -or $First.Height -ne 480 -or
            $Second.Width -ne $First.Width -or
            $Second.Height -ne $First.Height) {
            throw "Static-shooting layout fixtures had unexpected dimensions."
        }
        # The 256x240 viewport is 2x-scaled with host x origin 64. Bank02
        # $21E1/$21E5/$21E9 therefore permit differences only in these three
        # 3-character digit rectangles at logical x=8/40/72,y=120.
        $Allowed = @(
            @{ Left = 80; Right = 127; Top = 240; Bottom = 255 },
            @{ Left = 144; Right = 191; Top = 240; Bottom = 255 },
            @{ Left = 208; Right = 255; Top = 240; Bottom = 255 }
        )
        $Changed = @(0, 0, 0)
        for ($Y = 0; $Y -lt $First.Height; ++$Y) {
            for ($X = 0; $X -lt $First.Width; ++$X) {
                if ($First.GetPixel($X, $Y).ToArgb() -eq
                    $Second.GetPixel($X, $Y).ToArgb()) {
                    continue
                }
                $Matched = $false
                for ($Index = 0; $Index -lt $Allowed.Count; ++$Index) {
                    $Rect = $Allowed[$Index]
                    if ($X -ge $Rect.Left -and $X -le $Rect.Right -and
                        $Y -ge $Rect.Top -and $Y -le $Rect.Bottom) {
                        $Changed[$Index]++
                        $Matched = $true
                    }
                }
                if (!$Matched) {
                    throw "Static-shooting fixtures changed outside Bank02 digit destinations at host pixel $X,$Y."
                }
            }
        }
        if ($Changed[0] -eq 0 -or $Changed[1] -eq 0 -or $Changed[2] -eq 0) {
            throw "Static-shooting fixture did not exercise all three digit destinations."
        }
        # The former composite-dot cells at logical x=8/48/88,y=112 must be
        # untouched black background in both fixtures; authored dots remain at
        # $21E0/$21E4/$21E8 on y=120.
        $Black = [Drawing.Color]::Black.ToArgb()
        foreach ($HostX in @(80, 160, 240)) {
            for ($Y = 224; $Y -le 239; ++$Y) {
                for ($X = $HostX; $X -le $HostX + 15; ++$X) {
                    if ($First.GetPixel($X, $Y).ToArgb() -ne $Black -or
                        $Second.GetPixel($X, $Y).ToArgb() -ne $Black) {
                        throw "Duplicate shooting decimal-point pixels remain on the old y=112 row."
                    }
                }
            }
        }
    } finally {
        $First.Dispose()
        $Second.Dispose()
    }
}

function Assert-MutableStatisticPixelLayout {
    param([string]$FreshPath, [string]$PopulatedPath)

    Add-Type -AssemblyName System.Drawing
    $Fresh = [Drawing.Bitmap]::FromFile($FreshPath)
    $Populated = [Drawing.Bitmap]::FromFile($PopulatedPath)
    try {
        $Black = [Drawing.Color]::Black.ToArgb()
        function Get-InkColors {
            param([Drawing.Bitmap]$Image, [int]$Left, [int]$Top,
                  [int]$Right, [int]$Bottom)
            $Colors = @{}
            for ($Y = $Top; $Y -le $Bottom; ++$Y) {
                for ($X = $Left; $X -le $Right; ++$X) {
                    $Argb = $Image.GetPixel($X, $Y).ToArgb()
                    if ($Argb -ne $Black) { $Colors[$Argb.ToString("X8")] = 1 }
                }
            }
            return @($Colors.Keys)
        }
        # Logical x112..247,y112 was the incorrect native row.
        for ($Y = 224; $Y -le 239; ++$Y) {
            for ($X = 288; $X -le 559; ++$X) {
                if ($Fresh.GetPixel($X, $Y).ToArgb() -ne $Black -or
                    $Populated.GetPixel($X, $Y).ToArgb() -ne $Black) {
                    throw "Mutable statistic pixels remain on old logical y=112."
                }
            }
        }
        function Assert-TileInk {
            param([Drawing.Bitmap]$Image, [int]$LogicalX, [string]$Field)
            $Ink = 0
            $HostX = 64 + $LogicalX * 2
            for ($Y = 240; $Y -le 255; ++$Y) {
                for ($X = $HostX; $X -le $HostX + 15; ++$X) {
                    if ($Image.GetPixel($X, $Y).ToArgb() -ne $Black) { ++$Ink }
                }
            }
            if ($Ink -eq 0) { throw "$Field did not occupy logical x=$LogicalX,y=120." }
        }
        # Fresh 0/0/0/.0 locks the final cells and PTS punctuation/fraction.
        Assert-TileInk $Fresh 136 "fresh STL final cell"
        Assert-TileInk $Fresh 168 "fresh BLK final cell"
        Assert-TileInk $Fresh 200 "fresh REB final cell"
        Assert-TileInk $Fresh 232 "fresh PTS decimal cell"
        Assert-TileInk $Fresh 240 "fresh PTS fraction cell"
        # Populated ---/---/---/21.0 locks blank-elided starts and right edges.
        Assert-TileInk $Populated 120 "populated STL start"
        Assert-TileInk $Populated 136 "populated STL final cell"
        Assert-TileInk $Populated 152 "populated BLK start"
        Assert-TileInk $Populated 168 "populated BLK final cell"
        Assert-TileInk $Populated 184 "populated REB start"
        Assert-TileInk $Populated 200 "populated REB final cell"
        Assert-TileInk $Populated 216 "populated PTS start"
        Assert-TileInk $Populated 240 "populated PTS final cell"

        # Bank02 writes nametable values only. Screen 2's authored attribute
        # cells own their color, so every dynamic glyph must exactly match the
        # authored dot while the label row stays a distinct white palette.
        $AuthoredDotColors = @(Get-InkColors $Fresh 64 240 79 255)
        $FreshValueColors = @(Get-InkColors $Fresh 64 240 559 255)
        $PopulatedValueColors = @(Get-InkColors $Populated 64 240 559 255)
        $FreshLabelColors = @(Get-InkColors $Fresh 64 208 559 223)
        $PopulatedLabelColors = @(Get-InkColors $Populated 64 208 559 223)
        if ($AuthoredDotColors.Count -ne 1 -or
            ($FreshValueColors -join ",") -ne $AuthoredDotColors[0] -or
            ($PopulatedValueColors -join ",") -ne $AuthoredDotColors[0] -or
            $FreshLabelColors.Count -ne 1 -or
            $PopulatedLabelColors.Count -ne 1 -or
            $FreshLabelColors[0] -eq $AuthoredDotColors[0] -or
            $PopulatedLabelColors[0] -ne $FreshLabelColors[0]) {
            throw "Player-detail statistic value/label palette ownership mismatch."
        }
        # $AC32 writes raw tile $81 at the PTS decimal cell. The imported live
        # font '.' is that exact tile; lock its 2x-rendered pixel mask.
        for ($Y = 0; $Y -lt 16; ++$Y) {
            for ($X = 0; $X -lt 16; ++$X) {
                $ExpectedInk = $X -ge 2 -and $X -le 5 -and
                    $Y -ge 10 -and $Y -le 13
                $FreshInk = $Fresh.GetPixel(528 + $X, 240 + $Y).ToArgb() -ne
                    $Black
                $PopulatedInk =
                    $Populated.GetPixel(528 + $X, 240 + $Y).ToArgb() -ne $Black
                if ($FreshInk -ne $ExpectedInk -or
                    $PopulatedInk -ne $ExpectedInk) {
                    throw "PTS `$21FD decimal tile does not match exact authored `$81 dot pixels."
                }
            }
        }
        foreach ($Region in @(
            @{ Name = "number/first name"; Left = 304; Top = 48; Right = 559; Bottom = 63 },
            @{ Name = "surname"; Left = 352; Top = 64; Right = 559; Bottom = 79 },
            @{ Name = "height"; Left = 416; Top = 96; Right = 479; Bottom = 111 },
            @{ Name = "weight"; Left = 416; Top = 112; Right = 463; Bottom = 127 },
            @{ Name = "position"; Left = 416; Top = 128; Right = 559; Bottom = 143 },
            @{ Name = "condition"; Left = 416; Top = 144; Right = 559; Bottom = 159 }
        )) {
            $FreshColors = @(Get-InkColors -Image $Fresh `
                -Left $Region.Left -Top $Region.Top `
                -Right $Region.Right -Bottom $Region.Bottom)
            $PopulatedColors = @(Get-InkColors -Image $Populated `
                -Left $Region.Left -Top $Region.Top `
                -Right $Region.Right -Bottom $Region.Bottom)
            if ($FreshColors.Count -ne 1 -or $PopulatedColors.Count -ne 1 -or
                $FreshColors[0] -ne $AuthoredDotColors[0] -or
                $PopulatedColors[0] -ne $AuthoredDotColors[0]) {
                throw "$($Region.Name) does not use the screen-owned orange palette."
            }
        }
        foreach ($Image in @($Fresh, $Populated)) {
            $PositionColors = @(Get-InkColors -Image $Image `
                -Left 416 -Top 128 -Right 431 -Bottom 143)
            if ($PositionColors.Count -eq 0) {
                throw "position missing source start cell x=176,y=64."
            }
            foreach ($Field in @(
                @{ Name = "height"; Y = 48 },
                @{ Name = "weight"; Y = 56 }
            )) {
                foreach ($LogicalX in @(176, 192)) {
                    $Colors = @(Get-InkColors -Image $Image `
                        -Left (64 + $LogicalX * 2) -Top ($Field.Y * 2) `
                        -Right (79 + $LogicalX * 2) -Bottom ($Field.Y * 2 + 15))
                    if ($Colors.Count -eq 0) {
                        throw "$($Field.Name) missing source cell x=$LogicalX."
                    }
                }
                foreach ($LogicalX in @(200, 208)) {
                    $Colors = @(Get-InkColors -Image $Image `
                        -Left (64 + $LogicalX * 2) -Top ($Field.Y * 2) `
                        -Right (79 + $LogicalX * 2) -Bottom ($Field.Y * 2 + 15))
                    if ($Colors.Count -ne 0) {
                        throw "$($Field.Name) retained shifted ink at x=$LogicalX."
                    }
                }
            }
        }
    } finally {
        $Fresh.Dispose()
        $Populated.Dispose()
    }
}

try {
    if (!$SkipBuild) {
        $env:TECMO_SKIP_SHORTCUT = "1"
        $BuildLog = Join-Path $Scratch "build.log"
        @(& (Join-Path $ProjectRoot "build.ps1") 2>&1) | Set-Content $BuildLog
        if ($LASTEXITCODE -ne 0) {
            throw "Warning-clean build failed:`n$(@(Get-Content $BuildLog -Tail 30) -join "`n")"
        }
        if (Select-String -Path $BuildLog -Pattern 'warning C\d+' -Quiet) {
            throw "Warning-clean build emitted a compiler warning."
        }
    }
    if (!(Test-Path $Executable)) {
        throw "Executable not found at $Executable."
    }

    $Pack = Join-Path $Scratch "team-data.assetpack"
    [void](Invoke-Tecmo @("--build-assetpack", $RomPath, $Pack))
    $PackBytes = [IO.File]::ReadAllBytes($Pack)
    $SourceMap = Get-PackEntry $PackBytes "system/source-map"
    $Chr = Get-PackEntry $PackBytes "chr/all"
    $TeamData = Get-PackEntry $PackBytes "menu/team-data"
    if ($SourceMap.Size -eq 0 -or $Chr.Size -ne 262144 -or
        $TeamData.Size -ne 96372) {
        throw "Required source-map/CHR/TTDT-1 directory contracts were rejected."
    }
    $env:TECMO_ASSETPACK = $Pack

    $FlowOutput = Invoke-Tecmo @("--root", $DecompRoot, "--flow-test")
    if ($FlowOutput -notmatch 'FLOW TEST PASS') {
        throw "TEAM DATA state/input/all-star/transition flow did not pass."
    }

    $Checkpoints = @(
        @{ mode = "team-data-select"; hash = "C04A940E9BD78DC9D330AC9E41C2B6F03906A040CE52D442EB08BCE7FE4C7EB8"; status = "cursor-drawn=1 cursor-oam=15,32 cursor-visible=15,33" },
        @{ mode = "team-data-profile"; hash = "68282C16A45F58477598FC650D40650A0D2F1FFBC37F899A0C62EA54F895E406"; status = "cursor-drawn=1 cursor-oam=135,80 cursor-visible=135,81" },
        @{ mode = "team-data-profile-row2"; hash = "D39688787B5DD89354A293CFCD04983FEF774A47316DEDE759C76249B825E2C3"; status = "cursor-drawn=1 cursor-oam=135,96 cursor-visible=135,97" },
        @{ mode = "team-data-roster-page1"; hash = "E760133A6C88C9A1B7C25F75AB935F5EB77F88459BA20C71A0BF5445BED53B25"; status = "cursor-drawn=1 cursor-oam=40,143 cursor-visible=40,144" },
        @{ mode = "team-data-roster-row5"; hash = "424F3342D6044F8856B58415D9936876FD927BC00BCAD6F652AE45B45F6D4D99"; status = "cursor-drawn=1 cursor-oam=40,183 cursor-visible=40,184" },
        @{ mode = "team-data-player-detail"; hash = "07A55D6ED89C1C0308100396FD82F86CD27DB63A8D634F60976CF6C8360E4CA8"; status = "team-data-detail team=0 player=0 city=`"ATLANTA`" nickname=`"HAWKS`" name=`"RUMEAL ROBINSON`" fg=.456 ft=.636 three=.324 stl=0 blk=0 reb=0 pts=.0" },
        @{ mode = "team-data-player-detail-populated"; hash = "8E4648F0CD7ADF29327A0370B709F8FAFD627D3276CF970D511E0B7EC6807201"; status = "team-data-detail team=0 player=0 city=`"ATLANTA`" nickname=`"HAWKS`" name=`"RUMEAL ROBINSON`" fg=.456 ft=.636 three=.324 stl=--- blk=--- reb=--- pts=21.0" },
        @{ mode = "team-data-player-detail-chicago"; hash = "DE090A0E2F11F4D7856ED053A77EBF61B405B03A9DC4B6E750884882FD459CB6"; status = "team-data-detail team=3 player=1 city=`"CHICAGO`" nickname=`"BULLS`" name=`"MICHAEL JORDAN`" fg=.516 ft=.832 three=.268 stl=0 blk=0 reb=0 pts=.0" },
        @{ mode = "team-data-player-detail-chicago-populated"; hash = "EE2B9B0D02AA66DCB19340BC8A8127A355103677143BA6B6164E7106C7CF6C8E"; status = "team-data-detail team=3 player=1 city=`"CHICAGO`" nickname=`"BULLS`" name=`"MICHAEL JORDAN`" fg=.516 ft=.832 three=.268 stl=--- blk=--- reb=--- pts=21.0" },
        @{ mode = "team-data-entry-transition-frame0"; hash = "2377B0FF24274E21F5963CC35E43D0F666B7626E890A23C01A7621B842055F9A"; status = "transition-frame=0 palette=4 render=0" },
        @{ mode = "team-data-entry-transition-frame4"; hash = "2377B0FF24274E21F5963CC35E43D0F666B7626E890A23C01A7621B842055F9A"; status = "transition-frame=4 palette=4 render=1" },
        @{ mode = "team-data-entry-transition-frame7"; hash = "1C94880CC9919AFC5C7AB1C482B24C586556F3479421F1B5BBD29DC8808AB34A"; status = "transition-frame=7 palette=0 render=1" },
        @{ mode = "team-data-entry-transition-frame19"; hash = "C0F7882DC8C7D23A97B0864172B88446A886F9CC0C21005AEE9DE2DFA373DD07"; status = "transition-frame=19 palette=3 render=1" },
        @{ mode = "team-data-selector-profile-transition-frame10"; hash = "2377B0FF24274E21F5963CC35E43D0F666B7626E890A23C01A7621B842055F9A"; status = "transition-frame=10 palette=4 render=0" },
        @{ mode = "team-data-selector-profile-transition-frame16"; hash = "2377B0FF24274E21F5963CC35E43D0F666B7626E890A23C01A7621B842055F9A"; status = "transition-frame=16 palette=4 render=1" },
        @{ mode = "team-data-selector-profile-transition-frame19"; hash = "CAE05F093ED3AA29F647D6A87DC7662286B3B16DD9273EF0C100309E116DC2F7"; status = "transition-frame=19 palette=0 render=1" },
        @{ mode = "team-data-selector-profile-transition-frame31"; hash = "FD51903BA36151C55CDD0DF1A0B16C7526402F4533F5C19D5AC5BAFE04C0BAD9"; status = "transition-frame=31 palette=3 render=1" },
        @{ mode = "team-data-roster-detail-transition-frame15"; hash = "2377B0FF24274E21F5963CC35E43D0F666B7626E890A23C01A7621B842055F9A"; status = "transition-frame=15 palette=4 render=1" },
        @{ mode = "team-data-roster-detail-transition-frame18"; hash = "3853A7B55ABDA461367BD53DA5656E19C920CF1FAD4818E5FACF33EDE1448EE3"; status = "transition-frame=18 palette=0 render=1" },
        @{ mode = "team-data-roster-detail-transition-frame30"; hash = "07A55D6ED89C1C0308100396FD82F86CD27DB63A8D634F60976CF6C8360E4CA8"; status = "transition-frame=30 palette=3 render=1" },
        @{ mode = "team-data-detail-roster-transition-frame31"; hash = "FD51903BA36151C55CDD0DF1A0B16C7526402F4533F5C19D5AC5BAFE04C0BAD9"; status = "transition-frame=31 palette=3 render=1" }
    )
    foreach ($Checkpoint in $Checkpoints) {
        $Png = Join-Path $Scratch ($Checkpoint.mode + ".png")
        $Output = Invoke-Tecmo @("--render-test-mode", $Checkpoint.mode, $Png)
        if ($Output -notlike ("*" + $Checkpoint.status + "*")) {
            throw "Render state mismatch for $($Checkpoint.mode)."
        }
        $ActualHash = (Get-FileHash $Png -Algorithm SHA256).Hash
        if ($ActualHash -ne $Checkpoint.hash) {
            throw "Pixel checkpoint mismatch for $($Checkpoint.mode): $ActualHash"
        }
    }

    $LayoutA = Join-Path $Scratch "team-data-static-layout-a.png"
    $LayoutB = Join-Path $Scratch "team-data-static-layout-b.png"
    $LayoutAStatus = Invoke-Tecmo @(
        "--render-test-mode", "team-data-player-detail-static-layout-a", $LayoutA)
    $LayoutBStatus = Invoke-Tecmo @(
        "--render-test-mode", "team-data-player-detail-static-layout-b", $LayoutB)
    if ($LayoutAStatus -notlike "*fg=.004 ft=.008 three=.012*" -or
        $LayoutBStatus -notlike "*fg=.044 ft=.088 three=.132*") {
        throw "Static-shooting pixel fixtures did not resolve expected display values."
    }
    Assert-StaticShootingPixelLayout -FirstPath $LayoutA -SecondPath $LayoutB
    Assert-MutableStatisticPixelLayout `
        -FreshPath (Join-Path $Scratch "team-data-player-detail.png") `
        -PopulatedPath (Join-Path $Scratch "team-data-player-detail-populated.png")

    $Malformed = Join-Path $Scratch "team-data-malformed.assetpack"
    $MalformedBytes = [IO.File]::ReadAllBytes($Pack)
    $PayloadOffset = (Get-PackEntry $MalformedBytes "menu/team-data").Offset
    $MutationOffset = [uint64]$PayloadOffset + 128
    if ($MutationOffset -ge $MalformedBytes.Length) {
        throw "Malformed-pack mutation escaped the TTDT payload."
    }
    $MalformedBytes[[int]$MutationOffset] = $MalformedBytes[[int]$MutationOffset] -bxor 1
    [IO.File]::WriteAllBytes($Malformed, $MalformedBytes)
    $env:TECMO_ASSETPACK = $Malformed
    $RejectedPng = Join-Path $Scratch "rejected.png"
    [void](Invoke-Tecmo @("--render-test-mode", "team-data-profile", $RejectedPng) 1)
    if (Test-Path $RejectedPng) {
        throw "Malformed TTDT-1 unexpectedly produced a screenshot."
    }

    $global:LASTEXITCODE = 0
    Write-Host "TEAM DATA TEST PASS: ROM-only TTDT parser, static-shooting/mutable-stat ownership, all-star mapping, input/state transitions, malformed rejection, 21 pixel checkpoints, exact Bank02 statistic/height/weight/position layout, and authored palette ownership"
} finally {
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    $env:TECMO_ASSETPACK = $PreviousAssetPack
    if (Test-Path $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
}
