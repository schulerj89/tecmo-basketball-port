param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [string]$DecompRoot,
    [string]$AssetPackPath,
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
if (!(Test-Path -LiteralPath $RomPath)) {
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
if (!(Test-Path -LiteralPath $DecompRoot)) {
    throw "Decompilation root not found. Pass -DecompRoot."
}
$DecompRoot = (Resolve-Path $DecompRoot).Path

if ($AssetPackPath) {
    if (!(Test-Path -LiteralPath $AssetPackPath -PathType Leaf)) {
        throw "AssetPackPath does not exist."
    }
    $AssetPackPath = (Resolve-Path $AssetPackPath).Path
}

# Final TSNS size/fingerprint and render baselines live here so an intentional
# importer or renderer revision requires one explicit reviewable update.
$SeasonContract = [ordered]@{
    EntryId = "menu/season"
    PayloadSize = 104732
    PayloadFnv1a32 = "29C64F84"
    TeamDataSize = 96372
    TeamDataFnv1a32 = "812628F0"
    ChrSize = 262144
    ChrFnv1a32 = "F6F6E854"
    ChrFnv1a64 = "96A64F53B240ABB4"
    RenderCheckpoints = @(
        @{ Mode="season-team-control"; Hash="D396B7449C9524FC601185A6AE9F97DC28A55A29A85C65C4465D3612539F17D6"; Status="phase=team-control .*save=0" },
        @{ Mode="season-schedule"; Hash="36872EB4BC3A6AD51F2F250361D16DB8906E4613F6FE80E7B2833A94B54171A5"; Status="phase=schedule .*popup-rows=0" },
        @{ Mode="season-schedule-popup"; Hash="DB7EBA70DB18721DD3CC11AF717B3C3B335E1ABF061319F200861CE4BB385044"; Status="phase=schedule-popup .*popup-rows=6" },
        @{ Mode="season-playoff"; Hash="9FC6FE525A53D47014EDD4F271CD17D93666150482D9E00EC0CD556B7D9B3912"; Status="phase=playoff .*playoff-scroll=0" },
        @{ Mode="season-playoff-mid"; Hash="2F7596748543EAAD907350E238525853A87AEABFD00525E837DFBFE2F5B65DC0"; Status="phase=playoff .*playoff-scroll=128" },
        @{ Mode="season-playoff-east"; Hash="F918020EB3E95C9F427B8B3D3028D9823470DD8A773CA5A10BE62564FC75CFAF"; Status="phase=playoff .*playoff-scroll=252" },
        @{ Mode="season-standings-east"; Hash="E8922D9674FDD8489A24843B5E3E66BAD5F48ABEAA8CCA33262602F0E2B57912"; Status="phase=standings .*page=0" },
        @{ Mode="season-standings-west"; Hash="E994E8876285E26E8DB1A6F094DC2C2ED27B4CC531D9D72AB6B5904DE522B90C"; Status="phase=standings .*page=1" },
        @{ Mode="season-standings-programmed"; Hash="0E5683A17727792E69AF46D5ADADC8B36EDAF1B674FC8D110C46B96AE987E12B"; Status="phase=programmed-editor .*type=PROGRAMMED" },
        @{ Mode="season-leaders"; Hash="0FE1144CB12AA0A9D078ECF72EA227F795A7FA8A41C98D1F3F5420E07546671D"; Status="phase=leaders .*leader=0 .*leader-result=0" },
        @{ Mode="season-leaders4"; Hash="B2087496761DA16132129EEEBD6609879BF0F45C759FB5E091743A8E7200D2FE"; Status="phase=leaders .*leader=4 .*leader-result=0" },
        @{ Mode="season-game-start"; Hash="7D36349A9E9B1D110A6445DA7D7F589F132E3111ED6A41BDFE3DC7D714FDBA84"; Status="phase=game-start-prelaunch .*launch-blocked=0" }
    )
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
$Scratch = Join-Path $BuildDir `
    ("season_test_" + [Guid]::NewGuid().ToString("N"))
$Scratch = [IO.Path]::GetFullPath($Scratch)
$BuildPrefix = [IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/') + `
    [IO.Path]::DirectorySeparatorChar
if (!$Scratch.StartsWith($BuildPrefix,
                         [StringComparison]::OrdinalIgnoreCase)) {
    throw "SEASON scratch path escaped build\."
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

function Get-Fnv1a32Hex {
    param([byte[]]$Bytes)
    [uint64]$Hash = 2166136261
    foreach ($Byte in $Bytes) {
        $Hash = (($Hash -bxor [uint64]$Byte) * [uint64]16777619) %
            [uint64]4294967296
    }
    return ("{0:X8}" -f $Hash)
}

function Get-Fnv1a32Value {
    param([byte[]]$Bytes)
    return [Convert]::ToUInt32((Get-Fnv1a32Hex $Bytes), 16)
}

function Read-PackDirectory {
    param([byte[]]$Bytes)
    if ($Bytes.Length -lt 40 -or
        [Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -ne "TAP1" -or
        [BitConverter]::ToUInt32($Bytes, 4) -ne 1 -or
        [BitConverter]::ToUInt32($Bytes, 8) -ne 40 -or
        [BitConverter]::ToUInt32($Bytes, 12) -ne 128) {
        throw "Private asset pack header was malformed."
    }
    $Count = [BitConverter]::ToUInt32($Bytes, 16)
    $DirectoryOffset = [BitConverter]::ToUInt64($Bytes, 20)
    if ($DirectoryOffset + [uint64]$Count * 128 -gt $Bytes.Length) {
        throw "Private asset pack directory was truncated."
    }
    $Entries = @{}
    for ($Index = 0; $Index -lt $Count; ++$Index) {
        $Record = [uint64]$DirectoryOffset + [uint64]$Index * 128
        $Id = [Text.Encoding]::ASCII.GetString(
            $Bytes, [int]$Record, 64).Trim([char]0)
        $Offset = [BitConverter]::ToUInt64($Bytes, [int]$Record + 84)
        $Size = [BitConverter]::ToUInt64($Bytes, [int]$Record + 92)
        if ($Entries.ContainsKey($Id) -or $Offset -gt $DirectoryOffset -or
            $Size -gt $DirectoryOffset - $Offset) {
            throw "Private asset pack directory contract was malformed."
        }
        $Entries[$Id] = [pscustomobject]@{
            Id = $Id
            RecordOffset = $Record
            Type = [BitConverter]::ToUInt32($Bytes, [int]$Record + 64)
            Bank = [BitConverter]::ToUInt32($Bytes, [int]$Record + 68)
            Cpu = [BitConverter]::ToUInt32($Bytes, [int]$Record + 72)
            Offset = $Offset
            Size = $Size
            Flags = [BitConverter]::ToUInt32($Bytes, [int]$Record + 100)
        }
    }
    return $Entries
}

function Get-EntryBytes {
    param([byte[]]$PackBytes, [object]$Entry)
    if ($Entry.Size -gt [int]::MaxValue) {
        throw "Asset entry is too large for focused inspection."
    }
    [byte[]]$Result = New-Object byte[] ([int]$Entry.Size)
    [Array]::Copy($PackBytes, [int64]$Entry.Offset, $Result, 0,
                  [int64]$Entry.Size)
    return $Result
}

function Get-FileSnapshot {
    param([string]$Path)
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) {
        return [pscustomobject]@{ Exists=$false; Length=0; Hash="" }
    }
    $Item = Get-Item -LiteralPath $Path
    return [pscustomobject]@{
        Exists = $true
        Length = $Item.Length
        Hash = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
    }
}

function Assert-FileSnapshot {
    param([string]$Path, [object]$Before)
    $After = Get-FileSnapshot $Path
    if ($After.Exists -ne $Before.Exists -or
        $After.Length -ne $Before.Length -or $After.Hash -ne $Before.Hash) {
        throw "A user-owned season save changed outside test scratch."
    }
}

function New-TsavBytes {
    param([byte]$SeasonType, [uint16]$ScheduleIndex, [byte]$Marker)
    [byte[]]$Bytes = New-Object byte[] 108
    [byte[]]$Payload = New-Object byte[] 84
    $Payload[0] = $SeasonType
    $Payload[1 + 4] = $Marker -band 3
    $Payload[28 + 4] = 12
    $Payload[55 + 4] = 8
    [BitConverter]::GetBytes($ScheduleIndex).CopyTo($Payload, 82)
    [Text.Encoding]::ASCII.GetBytes("TSAV").CopyTo($Bytes, 0)
    [BitConverter]::GetBytes([uint16]1).CopyTo($Bytes, 4)
    [BitConverter]::GetBytes([uint16]24).CopyTo($Bytes, 6)
    [BitConverter]::GetBytes([uint32]108).CopyTo($Bytes, 8)
    [BitConverter]::GetBytes(
        (Get-Fnv1a32Value $Payload)).CopyTo($Bytes, 12)
    $Payload.CopyTo($Bytes, 24)
    return $Bytes
}

function Invoke-SaveStatusRender {
    param([string]$Root, [int]$ExpectedStatus, [string]$Name)
    New-Item -ItemType Directory -Force -Path $Root | Out-Null
    $Png = Join-Path $Root ($Name + ".png")
    $Output = Invoke-Tecmo @(
        "--root", $Root, "--render-test-mode", "season-team-control", $Png)
    if ($Output -notmatch ("season-state .*save=" + $ExpectedStatus + "(\s|$)")) {
        throw "TSAV status mismatch for $Name."
    }
    if (!(Test-Path -LiteralPath $Png)) {
        throw "TSAV fixture render was not produced for $Name."
    }
    return $Output
}

function Write-MutatedPack {
    param([string]$Path, [byte[]]$Source, [scriptblock]$Mutate)
    [byte[]]$Bytes = [byte[]]$Source.Clone()
    & $Mutate $Bytes
    [IO.File]::WriteAllBytes($Path, $Bytes)
}

function Assert-RejectedSeasonPack {
    param([string]$PackPath, [string]$Name)
    $env:TECMO_ASSETPACK = $PackPath
    $Root = Join-Path $Scratch ("reject-root-" + $Name)
    New-Item -ItemType Directory -Path $Root | Out-Null
    $Png = Join-Path $Root "rejected.png"
    [void](Invoke-Tecmo @(
        "--root", $Root, "--render-test-mode", "season-team-control", $Png) 1)
    if (Test-Path -LiteralPath $Png) {
        throw "Rejected season pack '$Name' unexpectedly rendered."
    }
}

$RealSavePaths = @(
    (Join-Path $ProjectRoot "saves\tecmo-season.sav"),
    (Join-Path $ProjectRoot "saves\tecmo-season.sav.tmp"),
    (Join-Path $ProjectRoot "build\tecmo-season.sav"),
    (Join-Path $ProjectRoot "build\tecmo-season.sav.tmp")
)
$RealSaveSnapshots = @{}
foreach ($Path in $RealSavePaths) {
    $RealSaveSnapshots[$Path] = Get-FileSnapshot $Path
}

try {
    if (!$SkipBuild) {
        $env:TECMO_SKIP_SHORTCUT = "1"
        $BuildLog = Join-Path $Scratch "build.log"
        @(& (Join-Path $ProjectRoot "build.ps1") 2>&1) |
            Set-Content -LiteralPath $BuildLog
        if ($LASTEXITCODE -ne 0) {
            throw "Warning-clean build failed:`n$(@(
                Get-Content -LiteralPath $BuildLog -Tail 30) -join "`n")"
        }
        if (Select-String -LiteralPath $BuildLog `
                -Pattern 'warning (C|LNK)\d+' -Quiet) {
            throw "Warning-clean build emitted a compiler or linker warning."
        }
    }
    if (!(Test-Path -LiteralPath $Executable)) {
        throw "Executable not found at $Executable."
    }

    $Pack = Join-Path $Scratch "season.assetpack"
    if ($AssetPackPath) {
        [IO.File]::Copy($AssetPackPath, $Pack, $true)
    } else {
        [void](Invoke-Tecmo @("--build-assetpack", $RomPath, $Pack))
    }
    $PackBytes = [IO.File]::ReadAllBytes($Pack)
    $Entries = Read-PackDirectory $PackBytes
    foreach ($Required in @(
        "system/source-map", "chr/all", "menu/team-data", "menu/season")) {
        if (!$Entries.ContainsKey($Required)) {
            throw "Required asset-pack entry '$Required' was missing."
        }
    }
    $Season = $Entries["menu/season"]
    $TeamData = $Entries["menu/team-data"]
    $Chr = $Entries["chr/all"]
    $DataType = [BitConverter]::ToUInt32(
        [Text.Encoding]::ASCII.GetBytes("DATA"), 0)
    if ($Season.Type -ne $DataType -or $Season.Bank -ne 3 -or
        $Season.Cpu -ne 0x83B3 -or $Season.Flags -ne 6 -or
        $Season.Size -ne [uint64]$SeasonContract.PayloadSize -or
        $TeamData.Size -ne [uint64]$SeasonContract.TeamDataSize -or
        $Chr.Size -ne [uint64]$SeasonContract.ChrSize) {
        throw "TSNS-1 directory/dependency contract was rejected."
    }

    $SeasonBytes = Get-EntryBytes $PackBytes $Season
    $TeamDataBytes = Get-EntryBytes $PackBytes $TeamData
    $ChrBytes = Get-EntryBytes $PackBytes $Chr
    if ((Get-Fnv1a32Hex $SeasonBytes) -ne
            $SeasonContract.PayloadFnv1a32 -or
        (Get-Fnv1a32Hex $TeamDataBytes) -ne
            $SeasonContract.TeamDataFnv1a32 -or
        (Get-Fnv1a32Hex $ChrBytes) -ne $SeasonContract.ChrFnv1a32 -or
        [Text.Encoding]::ASCII.GetString($SeasonBytes, 0, 4) -ne "TSNS" -or
        [BitConverter]::ToUInt16($SeasonBytes, 4) -ne 1 -or
        [BitConverter]::ToUInt16($SeasonBytes, 6) -ne 256 -or
        [BitConverter]::ToUInt32($SeasonBytes, 52) -ne
            [uint32]$SeasonContract.PayloadSize -or
        [BitConverter]::ToUInt32($SeasonBytes, 56) -ne
            [uint32]$SeasonContract.ChrSize -or
        [BitConverter]::ToUInt32($SeasonBytes, 60) -ne
            [Convert]::ToUInt32($SeasonContract.ChrFnv1a32, 16) -or
        (@(
            [BitConverter]::ToUInt16($SeasonBytes,92),
            [BitConverter]::ToUInt16($SeasonBytes,94),
            [BitConverter]::ToUInt16($SeasonBytes,96),
            [BitConverter]::ToUInt16($SeasonBytes,98)) -join ",") -ne
            "1107,567,351,1107" -or
        [BitConverter]::ToUInt16($SeasonBytes,100) -ne 0x8599 -or
        [BitConverter]::ToUInt16($SeasonBytes,102) -ne 0xB27F -or
        (@($SeasonBytes[197..212]) -join ",") -ne
            "15,83,15,99,135,163,135,179,31,115,31,131,31,147,31,163" -or
        (@($SeasonBytes[213..230]) -join ",") -ne
            "255,2,240,239,3,0,0,2,0,232,3,0,248,2,128,239,3,0") {
        throw "TSNS-1 header/fingerprint contract was rejected."
    }

    $RawDivision = @(
        17,19,13,18,1,16,26,10,3,14,4,2,7,0,
        23,15,6,5,25,9,8,21,22,11,20,24,12)
    $ResolvedDivision = @()
    $Starts = @(0,7,14,20)
    $Ends = @(7,14,20,27)
    if ((@($SeasonBytes[61210..61213]) -join ",") -ne
            ($Starts -join ",") -or
        (@($SeasonBytes[61214..61240]) -join ",") -ne
            ($RawDivision -join ",")) {
        throw "TSNS-1 raw division order was rejected."
    }
    for ($Division = 0; $Division -lt 4; ++$Division) {
        for ($Index = $Ends[$Division] - 1;
             $Index -ge $Starts[$Division]; --$Index) {
            $ResolvedDivision += $RawDivision[$Index]
        }
    }
    if (($ResolvedDivision -join ",") -ne
        "26,16,1,18,13,19,17,0,7,2,4,14,3,10,9,25,5,6,15,23,12,24,20,11,22,21,8") {
        throw "TSNS-1 resolved standings order was rejected."
    }

    $SourceMapBytes = Get-EntryBytes $PackBytes $Entries["system/source-map"]
    $SourceMap = [Text.Encoding]::UTF8.GetString($SourceMapBytes) |
        ConvertFrom-Json
    $SeasonSource = @($SourceMap.logical_entries |
        Where-Object { $_.id -eq "menu/season" } | Select-Object -First 1)
    $Roles = @($SeasonSource.sources | ForEach-Object { [string]$_.role })
    $RequiredRoles = @(
        "game-launch-terminal-unexecuted", "game-launch-target-unexecuted",
        "semantic-menu-records-and-box-descriptors",
        "popup-cursor-coordinate-tables", "regular-season-schedule",
        "standings-games-behind-renderer",
        "division-starts", "division-team-order", "leader-navigation",
        "leader-template-map", "leader-screen-descriptors",
        "leader-screen-streams", "leader-screen-palettes", "full-chr")
    $Dependencies = @($SeasonSource.runtime_dependencies)
    if ($SeasonSource.Count -ne 1 -or
        $SeasonSource.schema -ne "tecmo.season/TSNS-1" -or
        $SeasonSource.input_contract -ne "ines-only" -or
        @($RequiredRoles | Where-Object { $Roles -notcontains $_ }).Count -ne 0 -or
        @($Roles | Where-Object {
            $_ -match "trace|capture|log|screenshot|dump|state|video"
        }).Count -ne 0 -or
        (@($Dependencies | ForEach-Object { [string]$_.entry }) -join ",") -ne
            "chr/all,menu/team-data" -or
        [int]$SeasonSource.native_contract.payload_size -ne
            [int]$SeasonContract.PayloadSize -or
        $SeasonSource.native_contract.payload_fingerprint_fnv1a32 -ne
            $SeasonContract.PayloadFnv1a32 -or
        (@($SeasonSource.native_contract.filtered_schedule_counts) -join ",") -ne
            "1107,567,351,1107" -or
        $SeasonSource.native_contract.controlled_match_terminal -ne
            "gameplay-launch-blocked") {
        throw "TSNS-1 source-map/dependency provenance was rejected."
    }

    $env:TECMO_ASSETPACK = $Pack
    $AssetSelfTest = Invoke-Tecmo @("--assetpack-test")
    if ($AssetSelfTest -notmatch "self-test passed") {
        throw "Asset-pack self-test did not pass."
    }
    $SeasonSelfTest = Invoke-Tecmo @("--season-test")
    if ($SeasonSelfTest -notmatch "Season management self-test passed") {
        throw "Season input/route/popup/schedule/save self-test did not pass."
    }

    $RenderRoot = Join-Path $Scratch "render-root"
    New-Item -ItemType Directory -Path $RenderRoot | Out-Null
    foreach ($Checkpoint in $SeasonContract.RenderCheckpoints) {
        $Png = Join-Path $RenderRoot ($Checkpoint.Mode + ".png")
        $Output = Invoke-Tecmo @(
            "--root", $RenderRoot, "--render-test-mode",
            $Checkpoint.Mode, $Png)
        if ($Output -notmatch $Checkpoint.Status) {
            throw "Season render state mismatch for $($Checkpoint.Mode)."
        }
        $Actual = (Get-FileHash -LiteralPath $Png -Algorithm SHA256).Hash
        if ($Actual -ne $Checkpoint.Hash) {
            throw "Season pixel checkpoint mismatch for $($Checkpoint.Mode): $Actual"
        }
    }
    $MutationCases = @(
        @{ Name="missing"; Mutate={ param([byte[]]$Data)
            $Data[[int]$Season.RecordOffset] = [byte][char]'X' } },
        @{ Name="malformed"; Mutate={ param([byte[]]$Data)
            $Data[[int]$Season.Offset + 213] = 1 } },
        @{ Name="oversized"; Mutate={ param([byte[]]$Data)
            [BitConverter]::GetBytes([uint64]($Season.Size + 1)).CopyTo(
                $Data, [int]$Season.RecordOffset + 92) } },
        @{ Name="cross-pack-team-data"; Mutate={ param([byte[]]$Data)
            $Data[[int]$TeamData.Offset + 128] =
                $Data[[int]$TeamData.Offset + 128] -bxor 1 } },
        @{ Name="missing-team-data"; Mutate={ param([byte[]]$Data)
            $Data[[int]$TeamData.RecordOffset] = [byte][char]'X' } },
        @{ Name="cross-pack-chr"; Mutate={ param([byte[]]$Data)
            $Data[[int]$Chr.Offset] = $Data[[int]$Chr.Offset] -bxor 1 } }
    )
    foreach ($Case in $MutationCases) {
        $Mutated = Join-Path $Scratch ($Case.Name + ".assetpack")
        Write-MutatedPack $Mutated $PackBytes $Case.Mutate
        Assert-RejectedSeasonPack $Mutated $Case.Name
    }
    $env:TECMO_ASSETPACK = $Pack

    $ValidSave = New-TsavBytes 1 5 3
    $AlternateSave = New-TsavBytes 2 3 2

    $MissingRoot = Join-Path $Scratch "save-missing"
    [void](Invoke-SaveStatusRender $MissingRoot 0 "missing")
    $UnexpectedMissingSave = Join-Path $MissingRoot "saves\tecmo-season.sav"
    if (Test-Path -LiteralPath $UnexpectedMissingSave) {
        throw "Missing TSAV unexpectedly created a save."
    }

    $NewRoot = Join-Path $Scratch "save-new-preferred"
    New-Item -ItemType Directory -Force -Path `
        (Join-Path $NewRoot "saves"), (Join-Path $NewRoot "build") |
        Out-Null
    $NewPath = Join-Path $NewRoot "saves\tecmo-season.sav"
    $LegacyPath = Join-Path $NewRoot "build\tecmo-season.sav"
    [IO.File]::WriteAllBytes($NewPath, $ValidSave)
    [IO.File]::WriteAllBytes($LegacyPath, $AlternateSave)
    $NewBefore = Get-FileSnapshot $NewPath
    $LegacyBefore = Get-FileSnapshot $LegacyPath
    [void](Invoke-SaveStatusRender $NewRoot 1 "new-preferred")
    Assert-FileSnapshot $NewPath $NewBefore
    Assert-FileSnapshot $LegacyPath $LegacyBefore

    $MalformedRoot = Join-Path $Scratch "save-malformed"
    New-Item -ItemType Directory -Force -Path `
        (Join-Path $MalformedRoot "saves"),
        (Join-Path $MalformedRoot "build") | Out-Null
    $MalformedPath = Join-Path $MalformedRoot "saves\tecmo-season.sav"
    $MalformedLegacy = Join-Path $MalformedRoot "build\tecmo-season.sav"
    [IO.File]::WriteAllBytes($MalformedPath, (New-Object byte[] 108))
    [IO.File]::WriteAllBytes($MalformedLegacy, $ValidSave)
    $MalformedBefore = Get-FileSnapshot $MalformedPath
    $MalformedLegacyBefore = Get-FileSnapshot $MalformedLegacy
    [void](Invoke-SaveStatusRender $MalformedRoot 3 "malformed")
    Assert-FileSnapshot $MalformedPath $MalformedBefore
    Assert-FileSnapshot $MalformedLegacy $MalformedLegacyBefore

    $TrailingRoot = Join-Path $Scratch "save-trailing"
    New-Item -ItemType Directory -Force -Path `
        (Join-Path $TrailingRoot "saves") | Out-Null
    $TrailingPath = Join-Path $TrailingRoot "saves\tecmo-season.sav"
    [byte[]]$TrailingBytes = New-Object byte[] 109
    $ValidSave.CopyTo($TrailingBytes, 0)
    $TrailingBytes[108] = 0xEE
    [IO.File]::WriteAllBytes($TrailingPath, $TrailingBytes)
    $TrailingBefore = Get-FileSnapshot $TrailingPath
    [void](Invoke-SaveStatusRender $TrailingRoot 3 "trailing")
    Assert-FileSnapshot $TrailingPath $TrailingBefore

    $MigrationRoot = Join-Path $Scratch "save-migration"
    New-Item -ItemType Directory -Force -Path `
        (Join-Path $MigrationRoot "build") | Out-Null
    $MigrationLegacy = Join-Path $MigrationRoot "build\tecmo-season.sav"
    $MigrationNew = Join-Path $MigrationRoot "saves\tecmo-season.sav"
    [IO.File]::WriteAllBytes($MigrationLegacy, $ValidSave)
    $MigrationLegacyBefore = Get-FileSnapshot $MigrationLegacy
    [void](Invoke-SaveStatusRender $MigrationRoot 1 "migration")
    Assert-FileSnapshot $MigrationLegacy $MigrationLegacyBefore
    if (!(Test-Path -LiteralPath $MigrationNew) -or
        ((Get-FileHash -LiteralPath $MigrationNew -Algorithm SHA256).Hash -ne
         (Get-FileHash -LiteralPath $MigrationLegacy -Algorithm SHA256).Hash) -or
        (Test-Path -LiteralPath ($MigrationNew + ".tmp"))) {
        throw "Legacy TSAV migration was not atomic and exact."
    }

    $FailureRoot = Join-Path $Scratch "save-install-failure"
    New-Item -ItemType Directory -Force -Path `
        (Join-Path $FailureRoot "build") | Out-Null
    [IO.File]::WriteAllBytes((Join-Path $FailureRoot "saves"),
                             [byte[]]@(0x5A))
    $FailureLegacy = Join-Path $FailureRoot "build\tecmo-season.sav"
    [IO.File]::WriteAllBytes($FailureLegacy, $ValidSave)
    $FailureDirectoryBlock = Get-FileSnapshot (Join-Path $FailureRoot "saves")
    $FailureLegacyBefore = Get-FileSnapshot $FailureLegacy
    [void](Invoke-SaveStatusRender $FailureRoot 4 "install-failure")
    Assert-FileSnapshot (Join-Path $FailureRoot "saves") `
        $FailureDirectoryBlock
    Assert-FileSnapshot $FailureLegacy $FailureLegacyBefore

    $InvalidPng = Join-Path $RenderRoot "invalid.png"
    [void](Invoke-Tecmo @(
        "--root", $RenderRoot, "--render-test-mode",
        "season-invalid-state", $InvalidPng) 1)
    if (Test-Path -LiteralPath $InvalidPng) {
        throw "Invalid season state unexpectedly rendered."
    }

    $FlowRoot = Join-Path $Scratch "flow-root"
    $RosterRelativePaths = @(
        "decomp\lifted\bank02\C-0176_bank02_team_roster_and_player_data_8000_8FFF.asm",
        "decomp\lifted\bank02\C-0177_bank02_roster_team_player_data_9000_BFFF.asm")
    foreach ($Relative in $RosterRelativePaths) {
        $Source = Join-Path $DecompRoot $Relative
        if (!(Test-Path -LiteralPath $Source)) {
            throw "Native-flow roster fixture source was missing."
        }
        $Destination = Join-Path $FlowRoot $Relative
        New-Item -ItemType Directory -Force -Path `
            (Split-Path -Parent $Destination) | Out-Null
        [IO.File]::Copy($Source, $Destination, $true)
    }
    $Flow = Invoke-Tecmo @("--root", $FlowRoot, "--flow-test")
    if ($Flow -notmatch
        "FLOW TEST PASS: menu play-intro title start-game-menu preseason season quit") {
        throw "Native season no-gameplay flow regression did not pass."
    }

    foreach ($Path in $RealSavePaths) {
        Assert-FileSnapshot $Path $RealSaveSnapshots[$Path]
    }
    $global:LASTEXITCODE = 0
    Write-Host "SEASON TEST PASS: strict ROM-only TSNS provenance/dependencies, TSAV isolation/migration/rejection, native no-launch flow, malformed-pack guards, and 12 pixel checkpoints"
} finally {
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    $env:TECMO_ASSETPACK = $PreviousAssetPack
    foreach ($Path in $RealSavePaths) {
        Assert-FileSnapshot $Path $RealSaveSnapshots[$Path]
    }
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
}
