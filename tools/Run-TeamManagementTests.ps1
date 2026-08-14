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
    $RomPath = Join-Path (Split-Path -Parent $ProjectRoot) `
        "disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes"
}
if (!(Test-Path $RomPath)) {
    throw "Rev1 ROM not found. Pass -RomPath."
}
$RomPath = (Resolve-Path $RomPath).Path
if (!$DecompRoot) {
    $DecompRoot = Join-Path (Split-Path -Parent $ProjectRoot) `
        "disassem\tecmo-basketball-decompilation"
}
if (!(Test-Path $DecompRoot)) {
    throw "Decompilation root not found. Pass -DecompRoot."
}
$DecompRoot = (Resolve-Path $DecompRoot).Path

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
$Scratch = Join-Path $BuildDir `
    ("team_management_test_" + [Guid]::NewGuid().ToString("N"))
$Scratch = [IO.Path]::GetFullPath($Scratch)
$BuildPrefix = [IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/') + `
    [IO.Path]::DirectorySeparatorChar
if (!$Scratch.StartsWith($BuildPrefix,
                         [StringComparison]::OrdinalIgnoreCase)) {
    throw "TEAM management scratch path escaped build\."
}
New-Item -ItemType Directory -Path $Scratch | Out-Null

$PreviousSkipShortcut = $env:TECMO_SKIP_SHORTCUT
$PreviousAssetPack = $env:TECMO_ASSETPACK

function Invoke-Tecmo {
    param([string[]]$Arguments, [int]$ExpectedExit = 0)
    $Output = @(& $Executable @Arguments 2>&1)
    $ExitCode = $LASTEXITCODE
    if ($ExitCode -ne $ExpectedExit) {
        $Tail = @($Output | Select-Object -Last 12) -join `
            [Environment]::NewLine
        throw "tecmo_port exit $ExitCode (expected $ExpectedExit):`n$Tail"
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
        $Record = [uint64]$DirectoryOffset + [uint64]$Index * 128
        if ($Record + 128 -gt $Bytes.Length) {
            throw "Private asset pack directory was truncated."
        }
        $EntryId = [Text.Encoding]::ASCII.GetString(
            $Bytes, [int]$Record, 64).Trim([char]0)
        if ($EntryId -eq $Id) {
            $PayloadOffset = [BitConverter]::ToUInt64(
                $Bytes, [int]$Record + 84)
            $PayloadSize = [BitConverter]::ToUInt64(
                $Bytes, [int]$Record + 92)
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

function Get-PackEntryBytes {
    param([byte[]]$Bytes, $Entry)
    $Result = New-Object byte[] ([int]$Entry.Size)
    [Array]::Copy($Bytes, [int64]$Entry.Offset, $Result, 0,
                  [int64]$Entry.Size)
    return $Result
}

function Get-Fnv1a32 {
    param([byte[]]$Bytes)
    [uint64]$Hash = 2166136261
    foreach ($Byte in $Bytes) {
        $Hash = (($Hash -bxor [uint64]$Byte) * [uint64]16777619) %
            [uint64]4294967296
    }
    return ('{0:X8}' -f [uint32]$Hash)
}

function Assert-StartersPixelLayout {
    param([string]$AtlantaPath, [string]$ChicagoPath,
          [string]$SubstitutedPath)
    Add-Type -AssemblyName System.Drawing
    $Atlanta = [Drawing.Bitmap]::FromFile($AtlantaPath)
    $Chicago = [Drawing.Bitmap]::FromFile($ChicagoPath)
    $Substituted = [Drawing.Bitmap]::FromFile($SubstitutedPath)
    try {
        foreach ($Image in @($Atlanta, $Chicago, $Substituted)) {
            if ($Image.Width -ne 640 -or $Image.Height -ne 480) {
                throw "STARTERS proof frame had unexpected dimensions."
            }
        }
        $Black = [Drawing.Color]::Black.ToArgb()
        function Get-LogicalInkCount {
            param([Drawing.Bitmap]$Image, [int]$Left, [int]$Top,
                  [int]$Right, [int]$Bottom)
            $Ink = 0
            for ($Y = $Top * 2; $Y -lt ($Bottom + 1) * 2; ++$Y) {
                for ($X = 64 + $Left * 2;
                     $X -lt 64 + ($Right + 1) * 2; ++$X) {
                    if ($Image.GetPixel($X, $Y).ToArgb() -ne $Black) { ++$Ink }
                }
            }
            return $Ink
        }
        function Assert-LogicalBlack {
            param([Drawing.Bitmap]$Image, [int]$Left, [int]$Top,
                  [int]$Right, [int]$Bottom, [string]$Label)
            if ((Get-LogicalInkCount $Image $Left $Top $Right $Bottom) -ne 0) {
                throw "$Label was not empty black background."
            }
        }
        # Bank06 $A2E4 + Bank03 $8017: ATL is the x16 origin group while
        # Chicago is x32. The old duplicate top roster occupied x56+,y48+.
        if ((Get-LogicalInkCount $Atlanta 16 48 95 95) -eq 0 -or
            (Get-LogicalInkCount $Chicago 32 48 79 95) -eq 0) {
            throw "TTDT logo composition was missing."
        }
        Assert-LogicalBlack $Chicago 16 48 31 95 "Chicago pre-logo origin"
        Assert-LogicalBlack $Chicago 104 48 247 95 `
            "removed duplicate top roster"

        # The authored divider uses the selected profile palette. Chicago's
        # canonical group is green, and all divider ink is one exact color.
        $DividerColors = @{}
        for ($Y = 192; $Y -le 207; ++$Y) {
            for ($X = 112; $X -le 511; ++$X) {
                $Color = $Chicago.GetPixel($X, $Y)
                if ($Color.ToArgb() -ne $Black) {
                    $DividerColors[$Color.ToArgb().ToString('X8')] = $Color
                }
            }
        }
        if ($DividerColors.Count -ne 1) {
            throw "Chicago divider did not resolve one selected-profile color."
        }
        $Divider = @($DividerColors.Values)[0]
        if ($Divider.G -le $Divider.R -or $Divider.G -le $Divider.B) {
            throw "Chicago divider did not resolve the canonical green profile color."
        }

        foreach ($Image in @($Atlanta, $Chicago, $Substituted)) {
            for ($Row = 0; $Row -lt 5; ++$Row) {
                $Y = 128 + $Row * 8
                if ((Get-LogicalInkCount $Image 16 $Y 23 ($Y + 7)) -eq 0 -or
                    (Get-LogicalInkCount $Image 32 $Y 127 ($Y + 7)) -eq 0) {
                    throw "Lineup row $Row missed exact authored-position/name geometry."
                }
            }
            for ($Row = 0; $Row -lt 7; ++$Row) {
                $Y = 128 + $Row * 8
                if ((Get-LogicalInkCount $Image 136 $Y 143 ($Y + 7)) -eq 0 -or
                    (Get-LogicalInkCount $Image 152 $Y 247 ($Y + 7)) -eq 0) {
                    throw "Bench row $Row missed exact position/name geometry."
                }
            }
            Assert-LogicalBlack $Image 136 200 239 215 `
                "fresh INJURED body"
        }

        # A substitution changes only the replaced lineup and returned-bench
        # name cells. Logo, palette, authored positions, and all other rows stay.
        $Changed = 0
        for ($Y = 0; $Y -lt $Chicago.Height; ++$Y) {
            for ($X = 0; $X -lt $Chicago.Width; ++$X) {
                if ($Chicago.GetPixel($X, $Y).ToArgb() -eq
                    $Substituted.GetPixel($X, $Y).ToArgb()) { continue }
                $LogicalX = [int](($X - 64) / 2)
                $LogicalY = [int]($Y / 2)
                $Allowed = $LogicalY -ge 128 -and $LogicalY -le 135 -and
                    (($LogicalX -ge 32 -and $LogicalX -le 127) -or
                     ($LogicalX -ge 152 -and $LogicalX -le 247))
                if (!$Allowed) {
                    throw "Substitution changed pixels outside the two source-owned row-0 name fields."
                }
                ++$Changed
            }
        }
        if ($Changed -eq 0) { throw "Substitution fixture did not change row 0." }
    } finally {
        $Atlanta.Dispose()
        $Chicago.Dispose()
        $Substituted.Dispose()
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

    $Pack = Join-Path $Scratch "team-management.assetpack"
    [void](Invoke-Tecmo @("--build-assetpack", $RomPath, $Pack))
    $PackBytes = [IO.File]::ReadAllBytes($Pack)
    $SourceMap = Get-PackEntry $PackBytes "system/source-map"
    $Chr = Get-PackEntry $PackBytes "chr/all"
    $Management = Get-PackEntry $PackBytes "menu/team-management"
    $TeamData = Get-PackEntry $PackBytes "menu/team-data"
    if ($SourceMap.Size -eq 0 -or $Chr.Size -ne 262144 -or
        $Management.Size -ne 21061 -or
        (Get-Fnv1a32 (Get-PackEntryBytes $PackBytes $Management)) -ne
            "D192EAC6" -or $TeamData.Size -ne 96372) {
        throw "Required source-map/CHR/TTMG-1/TTDT-1 directory contracts were rejected."
    }
    $env:TECMO_ASSETPACK = $Pack

    $Focused = Invoke-Tecmo @("--team-management-test")
    if ($Focused -notmatch 'parser, persistence, substitution') {
        throw "Native TEAM management self-test did not pass."
    }
    $Flow = Invoke-Tecmo @("--root", $DecompRoot, "--flow-test")
    if ($Flow -notmatch 'FLOW TEST PASS') {
        throw "Native flow regression did not pass."
    }

    $Checkpoints = @(
        @{ mode="team-data-starters"; hash="D0333C619C19BD710ACBEF302162BBB12E073D7DFE30A5877743E6603F937FE4"; matches=@(
            'team-data-starters team=0 palette-group=1 logo=16,48,10,6 view=1 lineup-cursor=1,16,113 bench-cursor=0,0,0',
            'lineup=0:G:"R.ROBINSON"\|1:G:"S.AUGMON"\|2:F:"D.WILKINS"\|3:F:"K.WILLIS"\|4:C:"B.RASMUSSEN"',
            'bench=5:G:"M.WILEY"\|6:G:"R.MONROE"\|7:G:"M.CHEEKS"\|8:F:"D.FERRELL"\|9:F:"P.GRAHAM"\|10:F:"A.VOLKOV"\|11:C:"J.KONCAK"') },
        @{ mode="team-data-starters-chicago"; hash="EE6D62F36CA40A817D469A1F01C39EEC52721CAE74DEE329032A7AA79C055687"; matches=@(
            'team-data-starters team=3 palette-group=1 logo=32,48,6,6 view=1 lineup-cursor=1,16,113 bench-cursor=0,0,0',
            'lineup=0:G:"J.PAXSON"\|1:G:"M.JORDAN"\|2:F:"S.PIPPEN"\|3:F:"H.GRANT"\|4:C:"B.CARTWRIGH"',
            'bench=5:G:"B.ARMSTRONG"\|6:G:"B.HANSEN"\|7:G:"C.HODGES"\|8:F:"S.KING"\|9:F:"C.LEVINGSTO"\|10:C:"S.WILLIAMS"\|11:C:"W.PERDUE"') },
        @{ mode="team-data-starters-chicago-row3"; hash="29923AC9C37003A15AF65AF44E7F044D0D7C5AFA361BA31896F67C60E757543D"; matches=@(
            'lineup-cursor=1,24,145 bench-cursor=0,0,0') },
        @{ mode="team-data-starters-chicago-substituted"; hash="73B66F80E4162040736F03853376AFC8D8AD95C04BD6D920F6936B3F1ECA6C4E"; matches=@(
            'lineup=5:G:"B.ARMSTRONG"\|1:G:"M.JORDAN"',
            'bench=0:G:"J.PAXSON"\|6:G:"B.HANSEN"') },
        @{ mode="team-data-roster-chicago"; hash="F8B527B5EABD65BB5FB7E7EE14106161B2BC97D522066534DAA6A48270DCCDB4" },
        @{ mode="team-data-roster-chicago-substituted"; hash="F8B527B5EABD65BB5FB7E7EE14106161B2BC97D522066534DAA6A48270DCCDB4" },
        @{ mode="team-data-starters-reset"; hash="31EA6E530EB4CF21572460821A18D070B03BA51E4BB13FBF8B96FE22A1C2B68F" },
        @{ mode="team-data-starters-bench"; hash="61CD4960D71E61B4405D0C327980A3265B88A0E4F1AF554937089649F3148F87"; matches=@(
            'view=3 lineup-cursor=1,24,129 bench-cursor=1,144,129') },
        @{ mode="team-data-playbook"; hash="4FE464B77D1C214C4021F70F8A2885D128FAF865B4B5DC6C818C6F823E0FCB34" },
        @{ mode="team-data-playbook-replace-frame0"; hash="D48BE475A0F85032E80FD231DC06EA9E9487AAED2E358FDE374167F5DDDCBF97" },
        @{ mode="team-data-playbook-replace-frame1"; hash="19F2BD6A6922C29AF1B0D148355FB501EDB720E89ADEFBB4116CDBD7CFC4D72F" },
        @{ mode="team-data-playbook-replace-frame7"; hash="0954AF11A25756D22A78A5D2CA3FFEEE13C921290FEA138164376CAB5DBBA078" },
        @{ mode="team-data-playbook-replace-frame8"; hash="1B7CA7E071AD9C6733265F028AA954CC91BC82A7ECC2B5C59FB11F5D2A1B4DFC" },
        @{ mode="team-data-playbook-reset"; hash="DC6A469C9AE9AB6370CBAB61E2C61482C0248F97496746592074E8C5DC0BA9B3" }
    )
    foreach ($Checkpoint in $Checkpoints) {
        $Png = Join-Path $Scratch ($Checkpoint.mode + ".png")
        $RenderOutput = Invoke-Tecmo @("--render-test-mode", $Checkpoint.mode,
                                      $Png)
        $Actual = (Get-FileHash $Png -Algorithm SHA256).Hash
        if ($Actual -ne $Checkpoint.hash) {
            throw "Pixel checkpoint mismatch for $($Checkpoint.mode): $Actual"
        }
        foreach ($Pattern in @($Checkpoint.matches)) {
            if ($Pattern -and $RenderOutput -notmatch $Pattern) {
                throw "Structured checkpoint mismatch for $($Checkpoint.mode): $Pattern"
            }
        }
    }
    Assert-StartersPixelLayout `
        (Join-Path $Scratch "team-data-starters.png") `
        (Join-Path $Scratch "team-data-starters-chicago.png") `
        (Join-Path $Scratch "team-data-starters-chicago-substituted.png")
    if ((Get-FileHash (Join-Path $Scratch "team-data-roster-chicago.png") `
                      -Algorithm SHA256).Hash -ne
        (Get-FileHash (Join-Path $Scratch `
            "team-data-roster-chicago-substituted.png") `
                      -Algorithm SHA256).Hash) {
        throw "Ordinary PLAYERS DATA roster changed with starter session state."
    }

    $Malformed = Join-Path $Scratch "management-malformed.assetpack"
    $MalformedBytes = [IO.File]::ReadAllBytes($Pack)
    $Mutation = [uint64]$Management.Offset + 100
    if ($Mutation -ge $MalformedBytes.Length) {
        throw "TTMG mutation escaped the pack."
    }
    $MalformedBytes[[int]$Mutation] = $MalformedBytes[[int]$Mutation] -bxor 1
    [IO.File]::WriteAllBytes($Malformed, $MalformedBytes)
    $env:TECMO_ASSETPACK = $Malformed
    $RejectedPng = Join-Path $Scratch "rejected-management.png"
    [void](Invoke-Tecmo @("--render-test-mode", "team-data-starters",
                          $RejectedPng) 1)
    if (Test-Path $RejectedPng) {
        throw "Malformed TTMG-1 unexpectedly rendered."
    }

    $CrossPack = Join-Path $Scratch "dependency-malformed.assetpack"
    $CrossBytes = [IO.File]::ReadAllBytes($Pack)
    $DependencyMutation = [uint64]$TeamData.Offset + 128
    if ($DependencyMutation -ge $CrossBytes.Length) {
        throw "TTDT dependency mutation escaped the pack."
    }
    $CrossBytes[[int]$DependencyMutation] =
        $CrossBytes[[int]$DependencyMutation] -bxor 1
    [IO.File]::WriteAllBytes($CrossPack, $CrossBytes)
    $env:TECMO_ASSETPACK = $CrossPack
    $RejectedDependency = Join-Path $Scratch "rejected-dependency.png"
    [void](Invoke-Tecmo @("--render-test-mode", "team-data-playbook",
                          $RejectedDependency) 1)
    if (Test-Path $RejectedDependency) {
        throw "Malformed same-pack TTDT dependency unexpectedly rendered."
    }

    $global:LASTEXITCODE = 0
    Write-Host "TEAM MANAGEMENT TEST PASS: strict ROM-only TTMG parser/payload hash, source-owned fresh real-team STARTERS composition/cursors/substitution, unchanged ordinary roster, transient reset, PLAYBOOK release/carousel/reset, malformed dependency rejection, and 14 pixel checkpoints"
} finally {
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    $env:TECMO_ASSETPACK = $PreviousAssetPack
    if (Test-Path $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
}
