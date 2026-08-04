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
        @{ Mode="season-team-control"; Hash="ED752BB5D977D72AA05B136D6800AB5F7A94EB3A40EB8614D03A41F0D7557324"; Status="phase=team-control .*save=0" },
        @{ Mode="season-schedule"; Hash="C237B9EF2318108D821DABABF46D2B891ACAFE7468705AA32BAE3390770C2D5E"; Status="phase=schedule .*popup-rows=0" },
        @{ Mode="season-schedule-popup"; Hash="449A17BB6E83875BB7AECC2D796CCD4F2A11183B28DE6A54F21BAEFC6AC7E03A"; Status="phase=schedule-popup .*popup-rows=6" },
        @{ Mode="season-playoff"; Hash="FB074365D44606A973CFAB124FBF1870ADB2F253A3540083B429CCF003BD529A"; Status="phase=playoff .*playoff-scroll=0" },
        @{ Mode="season-playoff-mid"; Hash="665BFBBAC1FADDE02391AB880F84D8289D291D9A021DFB80583E04D36575D502"; Status="phase=playoff .*playoff-scroll=128" },
        @{ Mode="season-playoff-east"; Hash="1F63907C61631329250454D3317E0929D9384A7DC4E30A53680B63F4A48ED0AA"; Status="phase=playoff .*playoff-scroll=252" },
        @{ Mode="season-standings-east"; Hash="3CC04A3C668C9EA7265D7758AA08CADB33BA5E416C1D717B37B9D595050229AB"; Status="phase=standings .*page=0" },
        @{ Mode="season-standings-west"; Hash="96C6B839321B82393D706C325610A800B8FA2B8368B662719DF8EB45DFC9387B"; Status="phase=standings .*page=1" },
        @{ Mode="season-standings-programmed"; Hash="972415E9F5C8E7AA4305E386AD156F49C8637EB4AA8AD0C7E3E104232A4E0FB9"; Status="phase=programmed-editor .*type=PROGRAMMED" },
        @{ Mode="season-leaders"; Hash="BF506207114DC4E2823852147DFCD05E455496CE08DA502E3238A439A4877823"; Status="phase=leaders .*leader=0 .*leader-result=0" },
        @{ Mode="season-leaders4"; Hash="1F735E60C69435EDFB2099C5A68CE630A0BC84E73E5092D8A107ED1F5BC55DEF"; Status="phase=leaders .*leader=4 .*leader-result=0" },
        @{ Mode="season-leaders-results"; Hash="1B65D684B43BC1EB31B205CD463F4E3093E0AA19D6C7A45EB7C0162E4B5A61CA"; Status="phase=leaders .*leader=0 .*leader-result=1" },
        @{ Mode="season-leaders-results-populated"; Hash="54D1F1F208AB4BFF2C55CDC3762A1AD3A50F436CFFFB577CEDBA3EAF02EA5E26"; Status="phase=leaders .*leader=0 .*leader-result=1" },
        @{ Mode="season-leaders-results-populated-page6"; Hash="DD23B090928A1222AFB916324992CB628F5CF97CD30EE3E1FA495F71DD2B15F2"; Status="phase=leaders .*leader=0 .*leader-result=1" },
        @{ Mode="season-leaders-results-populated-page12"; Hash="F7FFB61D8B3EDF9838B6BF8804D84197F90D6FCEE9461D03AA07ED93731AC11E"; Status="phase=leaders .*leader=0 .*leader-result=1" },
        @{ Mode="season-leaders-results-populated3"; Hash="794DA4AE2CC6FB0B75B1F30A4F682B565F5B16A3DBC26BD0E594ABC9763A182E"; Status="phase=leaders .*leader=3 .*leader-result=1" },
        @{ Mode="season-leaders-results-populated5"; Hash="9A876C4A1048AE37C13D1F975E8E00F6C4DD4477F3EBF9DD5ECF2F80FE550DA3"; Status="phase=leaders .*leader=5 .*leader-result=1" },
        @{ Mode="season-leaders-results-populated6"; Hash="7D1BCC97947C768E3217A79CDFC5D6B3A3D094111BDC2A86885BF2B82ADA25B1"; Status="phase=leaders .*leader=6 .*leader-result=1" },
        @{ Mode="season-game-start"; Hash="66458313C7243A8EB3C464495B5B31D1EEAD31BA5D5B8669AD1F9009B0D65649"; Status="phase=game-start-prelaunch .*game-pending=1 launch-blocked=1" }
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

function Update-TsavChecksum {
    param([byte[]]$Bytes)
    if ($Bytes.Length -lt 24 -or
        [Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -ne "TSAV") {
        throw "TSAV checksum update received a malformed buffer."
    }
    [uint32]$TotalSize = [BitConverter]::ToUInt32($Bytes, 8)
    if ($TotalSize -lt 24 -or $TotalSize -gt $Bytes.Length) {
        throw "TSAV checksum update received an invalid size."
    }
    [int]$PayloadSize = [int]$TotalSize - 24
    [byte[]]$Payload = New-Object byte[] $PayloadSize
    [Array]::Copy($Bytes, 24, $Payload, 0, $PayloadSize)
    [BitConverter]::GetBytes((Get-Fnv1a32Value $Payload)).CopyTo($Bytes, 12)
}

function New-TsavBytes {
    param(
        [byte]$SeasonType,
        [uint16]$ScheduleIndex,
        [byte]$Marker,
        [ValidateSet(1, 2)][int]$Version = 1
    )
    [int]$PayloadSize = if ($Version -eq 1) { 84 } else { 5918 }
    [int]$TotalSize = 24 + $PayloadSize
    [byte[]]$Bytes = New-Object byte[] $TotalSize
    [byte[]]$Payload = New-Object byte[] $PayloadSize
    $Payload[0] = $SeasonType
    $Payload[1 + 4] = $Marker -band 3
    $Payload[28 + 4] = 12
    $Payload[55 + 4] = 8
    [BitConverter]::GetBytes($ScheduleIndex).CopyTo($Payload, 82)
    if ($Version -eq 2) {
        [uint16]$Coverage = if ($ScheduleIndex -eq 0) { 0x003F } else { 0 }
        [BitConverter]::GetBytes($Coverage).CopyTo($Payload, 84)
    }
    [Text.Encoding]::ASCII.GetBytes("TSAV").CopyTo($Bytes, 0)
    [BitConverter]::GetBytes([uint16]$Version).CopyTo($Bytes, 4)
    [BitConverter]::GetBytes([uint16]24).CopyTo($Bytes, 6)
    [BitConverter]::GetBytes([uint32]$TotalSize).CopyTo($Bytes, 8)
    $Payload.CopyTo($Bytes, 24)
    Update-TsavChecksum $Bytes
    return ,$Bytes
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

function Set-TsavV2Total {
    param(
        [byte[]]$Bytes,
        [int]$Team,
        [int]$Roster,
        [int]$Counter,
        [uint16]$Value
    )
    if ($Bytes.Length -ne 5942 -or $Team -lt 0 -or $Team -ge 27 -or
        $Roster -lt 0 -or $Roster -ge 12 -or $Counter -lt 0 -or
        $Counter -ge 9) {
        throw "TSAV-2 total coordinate was out of bounds."
    }
    [int]$Word = (($Team * 12 + $Roster) * 9) + $Counter
    [int]$Offset = 24 + 86 + ($Word * 2)
    [BitConverter]::GetBytes($Value).CopyTo($Bytes, $Offset)
}

function Assert-TsavV2Bytes {
    param(
        [byte[]]$Bytes,
        [byte[]]$ExpectedPrefix,
        [uint16]$ExpectedCoverage
    )
    if ($Bytes.Length -ne 5942 -or
        [Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -ne "TSAV" -or
        [BitConverter]::ToUInt16($Bytes, 4) -ne 2 -or
        [BitConverter]::ToUInt16($Bytes, 6) -ne 24 -or
        [BitConverter]::ToUInt32($Bytes, 8) -ne 5942 -or
        (@($Bytes[16..23] | Where-Object { $_ -ne 0 }).Count -ne 0 -or
        [BitConverter]::ToUInt16($Bytes, 24 + 84) -ne $ExpectedCoverage)) {
        throw "TSAV-2 header/coverage contract was rejected."
    }
    [byte[]]$Payload = New-Object byte[] 5918
    [Array]::Copy($Bytes, 24, $Payload, 0, 5918)
    if ([BitConverter]::ToUInt32($Bytes, 12) -ne
            (Get-Fnv1a32Value $Payload)) {
        throw "TSAV-2 checksum contract was rejected."
    }
    if ($null -ne $ExpectedPrefix) {
        if ($ExpectedPrefix.Length -ne 84) {
            throw "TSAV-2 expected prefix has the wrong size."
        }
        for ($Index = 0; $Index -lt 84; ++$Index) {
            if ($Bytes[24 + $Index] -ne $ExpectedPrefix[$Index]) {
                throw "TSAV-2 v1 prefix was not preserved at byte $Index."
            }
        }
    }
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
        $SeasonSource.native_contract.native_game_simulation -ne $false -or
        $SeasonSource.native_contract.game_result_boundary -ne
            "pending-completed-result-required" -or
        @($SeasonSource.native_contract.PSObject.Properties.Name) -contains
            "skip_simulation" -or
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
    $UnexpectedRenderSave = Join-Path $RenderRoot "saves\tecmo-season.sav"
    if (Test-Path -LiteralPath $UnexpectedRenderSave) {
        throw "GAME START boundary created or mutated TSAV before a result commit."
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

    $FreshV1Save = New-TsavBytes 1 0 3
    $ValidSave = New-TsavBytes 1 5 3
    $AlternateSave = New-TsavBytes 2 3 2
    $ValidV2Save = New-TsavBytes 1 5 3 2
    [byte[]]$V1Prefix = New-Object byte[] 84
    [Array]::Copy($ValidSave, 24, $V1Prefix, 0, 84)

    $NonzeroV2Save = New-TsavBytes 1 5 3 2
    for ($Team = 0; $Team -lt 27; ++$Team) {
        for ($Roster = 0; $Roster -lt 12; ++$Roster) {
            for ($Counter = 0; $Counter -lt 6; ++$Counter) {
                [uint16]$Value = [uint16](1000 + $Team * 17 +
                    $Roster * 5 + $Counter)
                Set-TsavV2Total $NonzeroV2Save $Team $Roster $Counter $Value
            }
        }
    }
    Set-TsavV2Total $NonzeroV2Save 26 11 5 0x1234
    Update-TsavChecksum $NonzeroV2Save
    Assert-TsavV2Bytes $NonzeroV2Save $V1Prefix 0
    if ([BitConverter]::ToUInt16($NonzeroV2Save, 24 + 86) -ne 1000 -or
        [BitConverter]::ToUInt16($NonzeroV2Save, 24 + 86 +
            ((26 * 12 + 11) * 9 + 5) * 2) -ne 0x1234 -or
        [BitConverter]::ToUInt16($NonzeroV2Save, 24 + 86 +
            ((0 * 12 + 0) * 9 + 6) * 2) -ne 0) {
        throw "TSAV-2 team-major totals order or unsupported-counter boundary failed."
    }

    $V2Root = Join-Path $Scratch "save-v2-roundtrip"
    New-Item -ItemType Directory -Force -Path `
        (Join-Path $V2Root "saves"), (Join-Path $V2Root "build") |
        Out-Null
    $V2Path = Join-Path $V2Root "saves\tecmo-season.sav"
    [IO.File]::WriteAllBytes($V2Path, $NonzeroV2Save)
    $V2Before = Get-FileSnapshot $V2Path
    [void](Invoke-SaveStatusRender $V2Root 1 "v2-roundtrip")
    Assert-FileSnapshot $V2Path $V2Before
    Assert-TsavV2Bytes ([IO.File]::ReadAllBytes($V2Path)) $V1Prefix 0

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
        (Test-Path -LiteralPath ($MigrationNew + ".tmp"))) {
        throw "Legacy TSAV migration was not atomic."
    }
    [byte[]]$MigrationV2 = [IO.File]::ReadAllBytes($MigrationNew)
    Assert-TsavV2Bytes $MigrationV2 $V1Prefix 0

    $FreshMigrationRoot = Join-Path $Scratch "save-v1-fresh-migration"
    New-Item -ItemType Directory -Force -Path `
        (Join-Path $FreshMigrationRoot "build") | Out-Null
    $FreshMigrationLegacy = Join-Path $FreshMigrationRoot "build\tecmo-season.sav"
    $FreshMigrationNew = Join-Path $FreshMigrationRoot "saves\tecmo-season.sav"
    [IO.File]::WriteAllBytes($FreshMigrationLegacy, $FreshV1Save)
    [void](Invoke-SaveStatusRender $FreshMigrationRoot 1 "v1-fresh-migration")
    if (!(Test-Path -LiteralPath $FreshMigrationNew)) {
        throw "Fresh v1 TSAV migration did not install a v2 save."
    }
    [byte[]]$FreshMigrationV2 = [IO.File]::ReadAllBytes($FreshMigrationNew)
    [byte[]]$FreshV1Prefix = New-Object byte[] 84
    [Array]::Copy($FreshV1Save, 24, $FreshV1Prefix, 0, 84)
    Assert-TsavV2Bytes $FreshMigrationV2 $FreshV1Prefix 0x003F

    $SaveRejectCases = @(
        @{ Name="v1-bad-version"; Data=$ValidSave; Mutate={ param([byte[]]$Data)
            $Data[4] = 9 } },
        @{ Name="v1-bad-header"; Data=$ValidSave; Mutate={ param([byte[]]$Data)
            [BitConverter]::GetBytes([uint16]23).CopyTo($Data, 6) } },
        @{ Name="v1-bad-reserved"; Data=$ValidSave; Mutate={ param([byte[]]$Data)
            $Data[16] = 1 } },
        @{ Name="v1-bad-checksum"; Data=$ValidSave; Mutate={ param([byte[]]$Data)
            $Data[12] = $Data[12] -bxor 1 } },
        @{ Name="v1-truncated"; Data=(New-Object byte[] 107); Mutate={ param([byte[]]$Data)
            [Array]::Copy($ValidSave, 0, $Data, 0, $Data.Length) } },
        @{ Name="v1-wrong-size"; Data=$ValidSave; Mutate={ param([byte[]]$Data)
            [BitConverter]::GetBytes([uint32]107).CopyTo($Data, 8) } },
        @{ Name="v1-marked-v2-physical108"; Data=$ValidSave; Mutate={ param([byte[]]$Data)
            $Data[4] = 2 } },
        @{ Name="v2-marked-v1-physical5942"; Data=$ValidV2Save; Mutate={ param([byte[]]$Data)
            $Data[4] = 1 } },
        @{ Name="v2-bad-version"; Data=$ValidV2Save; Mutate={ param([byte[]]$Data)
            $Data[4] = 9 } },
        @{ Name="v2-bad-header"; Data=$ValidV2Save; Mutate={ param([byte[]]$Data)
            [BitConverter]::GetBytes([uint16]23).CopyTo($Data, 6) } },
        @{ Name="v2-bad-reserved"; Data=$ValidV2Save; Mutate={ param([byte[]]$Data)
            $Data[16] = 1 } },
        @{ Name="v2-bad-checksum"; Data=$ValidV2Save; Mutate={ param([byte[]]$Data)
            $Data[12] = $Data[12] -bxor 1 } },
        @{ Name="v2-unknown-coverage"; Data=$ValidV2Save; Mutate={ param([byte[]]$Data)
            [BitConverter]::GetBytes([uint16]0x0040).CopyTo($Data, 108)
            Update-TsavChecksum $Data } },
        @{ Name="v2-unsupported-counter"; Data=$ValidV2Save; Mutate={ param([byte[]]$Data)
            Set-TsavV2Total $Data 0 0 6 1
            Update-TsavChecksum $Data } },
        @{ Name="v2-wrong-size"; Data=$ValidV2Save; Mutate={ param([byte[]]$Data)
            [BitConverter]::GetBytes([uint32]5941).CopyTo($Data, 8) } },
        @{ Name="v2-truncated"; Data=(New-Object byte[] 5941); Mutate={ param([byte[]]$Data)
            [Array]::Copy($ValidV2Save, 0, $Data, 0, $Data.Length) } },
        @{ Name="v2-trailing"; Data=(New-Object byte[] 5943); Mutate={ param([byte[]]$Data)
            $ValidV2Save.CopyTo($Data, 0)
            $Data[5942] = 0xEE } }
    )
    foreach ($Case in $SaveRejectCases) {
        [byte[]]$RejectedBytes = [byte[]]$Case.Data.Clone()
        & $Case.Mutate $RejectedBytes
        $RejectRoot = Join-Path $Scratch ("save-reject-" + $Case.Name)
        New-Item -ItemType Directory -Force -Path `
            (Join-Path $RejectRoot "saves"),
            (Join-Path $RejectRoot "build") | Out-Null
        $RejectNew = Join-Path $RejectRoot "saves\tecmo-season.sav"
        $RejectLegacy = Join-Path $RejectRoot "build\tecmo-season.sav"
        [IO.File]::WriteAllBytes($RejectNew, $RejectedBytes)
        [IO.File]::WriteAllBytes($RejectLegacy, $AlternateSave)
        $RejectNewBefore = Get-FileSnapshot $RejectNew
        $RejectLegacyBefore = Get-FileSnapshot $RejectLegacy
        [void](Invoke-SaveStatusRender $RejectRoot 3 $Case.Name)
        Assert-FileSnapshot $RejectNew $RejectNewBefore
        Assert-FileSnapshot $RejectLegacy $RejectLegacyBefore
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
        throw "Native season gameplay handoff/result flow regression did not pass."
    }

    foreach ($Path in $RealSavePaths) {
        Assert-FileSnapshot $Path $RealSaveSnapshots[$Path]
    }
    $global:LASTEXITCODE = 0
    Write-Host "SEASON TEST PASS: strict ROM-only TSNS provenance/dependencies, TSAV isolation/migration/rejection, native gameplay handoff/result flow, malformed-pack guards, and 19 pixel checkpoints"
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
