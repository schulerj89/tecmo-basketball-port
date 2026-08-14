param(
    [string]$ProjectRoot,
    [Parameter(Mandatory = $true)]
    [string]$RomPath,
    [string]$OutputRoot,
    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$RomPath = (Resolve-Path -LiteralPath $RomPath).Path
if (!$OutputRoot) {
    $OutputRoot = Join-Path $ProjectRoot "build\proof\team-data-player-data"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
$Executable = Join-Path $ProjectRoot "build\tecmo_port.exe"
$BuildScript = Join-Path $ProjectRoot "build.ps1"
$ProvenancePath = Join-Path $ProjectRoot "docs\team-data-player-detail-provenance.json"
$PreviousAssetPack = $env:TECMO_ASSETPACK
$PreviousSkipShortcut = $env:TECMO_SKIP_SHORTCUT
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)

function Invoke-Tecmo {
    param([string[]]$Arguments)
    $Output = @(& $Executable @Arguments 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw ("tecmo_port failed ({0}): {1}" -f $LASTEXITCODE,
               ($Output -join [Environment]::NewLine))
    }
    return ($Output -join [Environment]::NewLine)
}

function Assert-PlayerDetailScreenshotLayout {
    param([string]$FreshPath, [string]$PopulatedPath)

    Add-Type -AssemblyName System.Drawing
    $Fresh = [Drawing.Bitmap]::FromFile($FreshPath)
    $Populated = [Drawing.Bitmap]::FromFile($PopulatedPath)
    try {
        if ($Fresh.Width -ne 640 -or $Fresh.Height -ne 480 -or
            $Populated.Width -ne 640 -or $Populated.Height -ne 480) {
            throw "TEAM DATA proof screenshot dimensions were unexpected."
        }
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
        foreach ($HostX in @(80, 160, 240)) {
            for ($Y = 224; $Y -le 239; ++$Y) {
                for ($X = $HostX; $X -le $HostX + 15; ++$X) {
                    if ($Fresh.GetPixel($X, $Y).ToArgb() -ne $Black -or
                        $Populated.GetPixel($X, $Y).ToArgb() -ne $Black) {
                        throw "TEAM DATA proof found a duplicate shooting dot on old logical y=112."
                    }
                }
            }
        }
        foreach ($HostX in @(80, 144, 208)) {
            $Ink = 0
            for ($Y = 240; $Y -le 255; ++$Y) {
                for ($X = $HostX; $X -le $HostX + 47; ++$X) {
                    if ($Fresh.GetPixel($X, $Y).ToArgb() -ne $Black) {
                        ++$Ink
                    }
                }
            }
            if ($Ink -eq 0) {
                throw "TEAM DATA proof found a missing Bank02 shooting digit run."
            }
        }
        # Bank02 $AB91 starts one continuous y=120 mutable stream at $21EE;
        # the prior native y=112 value span must be empty in both states.
        for ($Y = 224; $Y -le 239; ++$Y) {
            for ($X = 288; $X -le 559; ++$X) {
                if ($Fresh.GetPixel($X, $Y).ToArgb() -ne $Black -or
                    $Populated.GetPixel($X, $Y).ToArgb() -ne $Black) {
                    throw "TEAM DATA proof found a mutable statistic on old logical y=112."
                }
            }
        }
        function Assert-TileHasInk {
            param([Drawing.Bitmap]$Image, [int]$LogicalX, [string]$Description)
            $Ink = 0
            $HostX = 64 + $LogicalX * 2
            for ($Y = 240; $Y -le 255; ++$Y) {
                for ($X = $HostX; $X -le $HostX + 15; ++$X) {
                    if ($Image.GetPixel($X, $Y).ToArgb() -ne $Black) { ++$Ink }
                }
            }
            if ($Ink -eq 0) { throw "TEAM DATA proof missing $Description at logical x=$LogicalX,y=120." }
        }
        Assert-TileHasInk $Fresh 136 "fresh STL final cell"
        Assert-TileHasInk $Fresh 168 "fresh BLK final cell"
        Assert-TileHasInk $Fresh 200 "fresh REB final cell"
        Assert-TileHasInk $Fresh 232 "fresh PTS decimal cell"
        Assert-TileHasInk $Fresh 240 "fresh PTS fraction cell"
        Assert-TileHasInk $Populated 120 "populated STL start"
        Assert-TileHasInk $Populated 136 "populated STL final cell"
        Assert-TileHasInk $Populated 152 "populated BLK start"
        Assert-TileHasInk $Populated 168 "populated BLK final cell"
        Assert-TileHasInk $Populated 184 "populated REB start"
        Assert-TileHasInk $Populated 200 "populated REB final cell"
        Assert-TileHasInk $Populated 216 "populated PTS start"
        Assert-TileHasInk $Populated 240 "populated PTS final cell"
        $DotColors = @(Get-InkColors $Fresh 64 240 79 255)
        $FreshValues = @(Get-InkColors $Fresh 64 240 559 255)
        $PopulatedValues = @(Get-InkColors $Populated 64 240 559 255)
        $FreshLabels = @(Get-InkColors $Fresh 64 208 559 223)
        $PopulatedLabels = @(Get-InkColors $Populated 64 208 559 223)
        if ($DotColors.Count -ne 1 -or
            ($FreshValues -join ",") -ne $DotColors[0] -or
            ($PopulatedValues -join ",") -ne $DotColors[0] -or
            $FreshLabels.Count -ne 1 -or $PopulatedLabels.Count -ne 1 -or
            $FreshLabels[0] -eq $DotColors[0] -or
            $PopulatedLabels[0] -ne $FreshLabels[0]) {
            throw "TEAM DATA proof found incorrect value/label palette ownership."
        }
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
                    throw "TEAM DATA proof found incorrect PTS `$21FD tile `$81 pixels."
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
                $FreshColors[0] -ne $DotColors[0] -or
                $PopulatedColors[0] -ne $DotColors[0]) {
                throw "TEAM DATA proof found wrong $($Region.Name) palette."
            }
        }
        foreach ($Image in @($Fresh, $Populated)) {
            $PositionColors = @(Get-InkColors -Image $Image `
                -Left 416 -Top 128 -Right 431 -Bottom 143)
            if ($PositionColors.Count -eq 0) {
                throw "TEAM DATA proof missing position x=176,y=64."
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
                        throw "TEAM DATA proof missing $($Field.Name) cell x=$LogicalX."
                    }
                }
                foreach ($LogicalX in @(200, 208)) {
                    $Colors = @(Get-InkColors -Image $Image `
                        -Left (64 + $LogicalX * 2) -Top ($Field.Y * 2) `
                        -Right (79 + $LogicalX * 2) -Bottom ($Field.Y * 2 + 15))
                    if ($Colors.Count -ne 0) {
                        throw "TEAM DATA proof found shifted $($Field.Name) ink x=$LogicalX."
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
    if (!(Test-Path -LiteralPath $ProvenancePath -PathType Leaf)) {
        throw "TEAM DATA player-detail provenance was not found at $ProvenancePath."
    }
    $Provenance = Get-Content -LiteralPath $ProvenancePath -Raw |
        ConvertFrom-Json
    if ($Provenance.schema -ne
            "tecmo.team-data.player-detail-provenance/1" -or
        $Provenance.cursor.profile.generic_input_config -ne "0C" -or
        $Provenance.cursor.profile.emitter_base[0] -ne 135 -or
        $Provenance.cursor.generic_record_delta[1] -ne -4 -or
        $Provenance.cursor.profile.emitter_base[1] -ne 84 -or
        $Provenance.cursor.profile.resolved_oam_anchor[0] -ne 135 -or
        $Provenance.cursor.profile.resolved_oam_anchor[1] -ne 80 -or
        $Provenance.cursor.profile.visible_anchor[0] -ne 135 -or
        $Provenance.cursor.profile.visible_anchor[1] -ne 81 -or
        $Provenance.cursor.roster.generic_input_config -ne "10" -or
        $Provenance.cursor.roster.emitter_base[0] -ne 40 -or
        $Provenance.cursor.roster.emitter_base[1] -ne 147 -or
        $Provenance.cursor.roster.resolved_oam_anchor[0] -ne 40 -or
        $Provenance.cursor.roster.resolved_oam_anchor[1] -ne 143 -or
        $Provenance.cursor.roster.visible_anchor[0] -ne 40 -or
        $Provenance.cursor.roster.visible_anchor[1] -ne 144 -or
        $Provenance.cursor.row_stride -ne 8 -or
        $Provenance.cursor.generic_record_delta[0] -ne 0 -or
        $Provenance.cursor.coordinate_contract -ne
            "emitter base + generic record delta = resolved OAM; visible Y = OAM Y + 1" -or
        $Provenance.statistics_ownership.static_shooting.native_source -ne
            "selected TTDT TecmoTeamDataPlayer.attributes[4..6]" -or
        (@($Provenance.statistics_ownership.static_shooting.fields) -join ",") -ne
            "FG%,FT%,3PT%" -or
        (@($Provenance.native_layout.static_shooting.digit_runs | ForEach-Object {
            $_.ppu_cell
        }) -join ",") -ne "21E1,21E5,21E9" -or
        (@($Provenance.native_layout.static_shooting.digit_runs | ForEach-Object {
            @($_.logical_origin) -join ","
        }) -join ";") -ne "8,120;40,120;72,120" -or
        (@($Provenance.native_layout.static_shooting.authored_decimal_points |
            ForEach-Object { $_.ppu_cell }) -join ",") -ne "21E0,21E4,21E8" -or
        (@($Provenance.native_layout.static_shooting.authored_decimal_points |
            ForEach-Object { @($_.tile_origin) -join "," }) -join ";") -ne
            "0,120;32,120;64,120" -or
        $Provenance.native_layout.mutable_statistics.value_y -ne 120 -or
        (@($Provenance.native_layout.mutable_statistics.integer_fields |
            ForEach-Object { $_.string_right_edge }) -join ",") -ne
            "144,176,208" -or
        $Provenance.native_layout.mutable_statistics.points.string_right_edge -ne
            248 -or
        $Provenance.native_layout.mutable_statistics.points.decimal_point_tile -ne
            "81" -or
        $Provenance.native_layout.value_palette.ownership -ne
            'Bank00 $877D player-detail screen attribute table' -or
        $Provenance.native_layout.value_palette.decoded_indices.value_row_15 -ne
            3 -or
        $Provenance.native_layout.value_palette.decoded_indices.label_row_13 -ne
            2 -or
        (@($Provenance.native_layout.other_dynamic_profile_text.height.logical_origin) `
            -join ",") -ne "176,48" -or
        $Provenance.native_layout.other_dynamic_profile_text.weight.string_right_edge -ne
            200 -or
        (@($Provenance.native_layout.other_dynamic_profile_text.position.logical_origin) `
            -join ",") -ne "176,64" -or
        $Provenance.statistics_ownership.mutable_statistics.native_source -ne
            "TecmoSeasonSession.player_stats_totals") {
        throw "TEAM DATA player-detail provenance contract was malformed."
    }
    if (!$SkipBuild) {
        $env:TECMO_SKIP_SHORTCUT = "1"
        & $BuildScript
        if ($LASTEXITCODE -ne 0) {
            throw "Native build failed before TEAM DATA proof rendering."
        }
    }
    if (!(Test-Path -LiteralPath $Executable -PathType Leaf)) {
        throw "Native executable was not found at $Executable."
    }

    New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
    $AssetPack = Join-Path $OutputRoot "team-data-player-data-proof.assetpack"
    [void](Invoke-Tecmo @("--build-assetpack", $RomPath, $AssetPack))
    $env:TECMO_ASSETPACK = $AssetPack

    $Modes = @(
        [ordered]@{
            mode = "team-data-profile"
            file = "team-data-profile-cursor.png"
            purpose = "Bank01 `$8031 generic cursor at resolved profile OAM (135,80), visible (135,81)"
            expected_status = "cursor-drawn=1 cursor-oam=135,80 cursor-visible=135,81"
        },
        [ordered]@{
            mode = "team-data-profile-row2"
            file = "team-data-profile-row2-cursor.png"
            purpose = "profile row 2 resolves to OAM (135,96), visible (135,97)"
            expected_status = "cursor-drawn=1 cursor-oam=135,96 cursor-visible=135,97"
        },
        [ordered]@{
            mode = "team-data-roster-page1"
            file = "team-data-roster-cursor.png"
            purpose = "Bank01 `$8031 generic cursor at resolved roster OAM (40,143), visible (40,144)"
            expected_status = "cursor-drawn=1 cursor-oam=40,143 cursor-visible=40,144"
        },
        [ordered]@{
            mode = "team-data-roster-row5"
            file = "team-data-roster-row5-cursor.png"
            purpose = "roster row 5 resolves to OAM (40,183), visible (40,184)"
            expected_status = "cursor-drawn=1 cursor-oam=40,183 cursor-visible=40,184"
        },
        [ordered]@{
            mode = "team-data-player-detail"
            file = "team-data-player-detail-fresh.png"
            purpose = "fresh season keeps parsed Atlanta player 0 static shooting while mutable fields are zero"
            expected_status = "team-data-detail team=0 player=0 city=`"ATLANTA`" nickname=`"HAWKS`" name=`"RUMEAL ROBINSON`""
        },
        [ordered]@{
            mode = "team-data-player-detail-populated"
            file = "team-data-player-detail-populated.png"
            purpose = "conflicting .600/.875/.500 ledger cannot replace Atlanta static shooting; mutable PTS becomes 21.0"
            expected_status = "team-data-detail team=0 player=0 city=`"ATLANTA`" nickname=`"HAWKS`" name=`"RUMEAL ROBINSON`""
        },
        [ordered]@{
            mode = "team-data-player-detail-chicago"
            file = "team-data-player-detail-chicago-fresh.png"
            purpose = "fresh Chicago team 3 zero-based roster index 1 resolves parsed Michael Jordan identity and static shooting"
            expected_status = "team-data-detail team=3 player=1 city=`"CHICAGO`" nickname=`"BULLS`" name=`"MICHAEL JORDAN`""
        },
        [ordered]@{
            mode = "team-data-player-detail-chicago-populated"
            file = "team-data-player-detail-chicago-populated.png"
            purpose = "conflicting ledger changes mutable PTS only for the same parsed Michael Jordan record"
            expected_status = "team-data-detail team=3 player=1 city=`"CHICAGO`" nickname=`"BULLS`" name=`"MICHAEL JORDAN`""
        }
    )
    $Frames = @()
    foreach ($Mode in $Modes) {
        $Png = Join-Path $OutputRoot $Mode.file
        $Status = Invoke-Tecmo @("--render-test-mode", $Mode.mode, $Png)
        if ($Status -notlike ("*" + $Mode.expected_status + "*")) {
            throw "TEAM DATA proof renderer status mismatch for $($Mode.mode)."
        }
        if (!(Test-Path -LiteralPath $Png -PathType Leaf)) {
            throw "TEAM DATA proof renderer did not create $($Mode.file)."
        }
        $Frames += [ordered]@{
            mode = $Mode.mode
            file = $Mode.file
            purpose = $Mode.purpose
            sha256 = (Get-FileHash -LiteralPath $Png -Algorithm SHA256).Hash
            renderer_status = $Status
        }
    }
    $AtlantaFresh = @($Frames | Where-Object {
        $_.mode -eq "team-data-player-detail"
    } | Select-Object -First 1)
    $AtlantaPopulated = @($Frames | Where-Object {
        $_.mode -eq "team-data-player-detail-populated"
    } | Select-Object -First 1)
    $ChicagoFresh = @($Frames | Where-Object {
        $_.mode -eq "team-data-player-detail-chicago"
    } | Select-Object -First 1)
    $ChicagoPopulated = @($Frames | Where-Object {
        $_.mode -eq "team-data-player-detail-chicago-populated"
    } | Select-Object -First 1)
    if ($AtlantaFresh.Count -ne 1 -or $AtlantaPopulated.Count -ne 1 -or
        $ChicagoFresh.Count -ne 1 -or $ChicagoPopulated.Count -ne 1 -or
        $AtlantaFresh.renderer_status -notmatch
            'fg=\.[0-9]{3} ft=\.[0-9]{3} three=\.[0-9]{3}.*pts=\.0' -or
        $AtlantaPopulated.renderer_status -notmatch 'pts=21\.0' -or
        $ChicagoFresh.renderer_status -notmatch
            'fg=\.[0-9]{3} ft=\.[0-9]{3} three=\.[0-9]{3}.*pts=\.0' -or
        $ChicagoPopulated.renderer_status -notmatch
            'fg=\.[0-9]{3} ft=\.[0-9]{3} three=\.[0-9]{3}.*pts=21\.0') {
        throw "TEAM DATA static-shooting/mutable-stat ownership proof failed."
    }
    $AtlantaShooting = [regex]::Match(
        $AtlantaFresh.renderer_status,
        'fg=(\.[0-9]{3}) ft=(\.[0-9]{3}) three=(\.[0-9]{3})')
    if (!$AtlantaShooting.Success -or
        $AtlantaPopulated.renderer_status -notlike
            ("*fg=" + $AtlantaShooting.Groups[1].Value +
             " ft=" + $AtlantaShooting.Groups[2].Value +
             " three=" + $AtlantaShooting.Groups[3].Value + "*")) {
        throw "Atlanta static shooting changed under conflicting ledger data."
    }
    $ChicagoShooting = [regex]::Match(
        $ChicagoFresh.renderer_status,
        'fg=(\.[0-9]{3}) ft=(\.[0-9]{3}) three=(\.[0-9]{3})')
    if (!$ChicagoShooting.Success -or
        $ChicagoShooting.Groups[1].Value -eq ".000" -or
        $ChicagoShooting.Groups[2].Value -eq ".000" -or
        $ChicagoShooting.Groups[3].Value -eq ".000" -or
        $ChicagoPopulated.renderer_status -notlike
            ("*fg=" + $ChicagoShooting.Groups[1].Value +
             " ft=" + $ChicagoShooting.Groups[2].Value +
             " three=" + $ChicagoShooting.Groups[3].Value + "*")) {
        throw "Chicago static shooting changed under conflicting ledger data."
    }
    Assert-PlayerDetailScreenshotLayout `
        -FreshPath (Join-Path $OutputRoot $ChicagoFresh.file) `
        -PopulatedPath (Join-Path $OutputRoot $ChicagoPopulated.file)
    $Manifest = [ordered]@{
        schema = "tecmo.team-data.player-data-proof/1"
        provenance = "docs/team-data-player-detail-provenance.json"
        source = "ROM-derived native asset pack; no decompilation or capture input is read by the renderer"
        cursor_coordinate_contract = $Provenance.cursor
        statistics_ownership = $Provenance.statistics_ownership
        native_layout = $Provenance.native_layout
        pixel_layout_verified =
            'all dynamic detail fields use screen-owned orange; height x176 and weight edge200, seven values y120, PTS $21FD tile $81, empty old y112, and white labels verified'
        chicago_static_display = [ordered]@{
            team = 3
            zero_based_roster_index = 1
            parsed_identity = "MICHAEL JORDAN"
            fg = $ChicagoShooting.Groups[1].Value
            ft = $ChicagoShooting.Groups[2].Value
            three_point = $ChicagoShooting.Groups[3].Value
            invariant_under_conflicting_ledger = $true
        }
        frames = $Frames
    }
    $ManifestPath = Join-Path $OutputRoot "team-data-player-data-proof.json"
    [IO.File]::WriteAllText(
        $ManifestPath,
        ($Manifest | ConvertTo-Json -Depth 5),
        $Utf8NoBom)
    Write-Output "TEAM DATA PLAYER-DATA PROOF PASS: $ManifestPath"
} finally {
    $env:TECMO_ASSETPACK = $PreviousAssetPack
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
}
