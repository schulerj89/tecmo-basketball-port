param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [switch]$Build
)

$ErrorActionPreference = "Stop"
$ExpectedRomSha256 =
    "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if (!$RomPath) { $RomPath = $env:TECMO_ROM_PATH }
if (!$RomPath -or !(Test-Path -LiteralPath $RomPath -PathType Leaf)) {
    throw "Pass -RomPath or set TECMO_ROM_PATH to the local Rev1 iNES ROM."
}
$RomPath = (Resolve-Path -LiteralPath $RomPath).Path
if ((Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash -ne
    $ExpectedRomSha256) {
    throw "TGMO-1 tests require the exact Tecmo NBA Basketball Rev1 ROM."
}

$BuildDir = Join-Path $ProjectRoot "build"
$Executable = Join-Path $BuildDir "tecmo_port.exe"
$Scratch = [IO.Path]::GetFullPath(
    (Join-Path $BuildDir "gameplay_movement_test"))
$BuildPrefix = [IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/') +
    [IO.Path]::DirectorySeparatorChar
if (!$Scratch.StartsWith($BuildPrefix,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw "Gameplay-movement scratch path escaped build\."
}
$PackPath = Join-Path $Scratch "gameplay-movement.assetpack"
$PreviousSkipShortcut = $env:TECMO_SKIP_SHORTCUT

function Get-ShortTail {
    param([object[]]$Lines)
    return (@($Lines | Select-Object -Last 12) -join
        [Environment]::NewLine)
}

function Get-Fnv1a32 {
    param([byte[]]$Bytes)
    [uint32]$Hash = 2166136261
    foreach ($Byte in $Bytes) {
        [uint64]$Product = [uint64]($Hash -bxor [uint32]$Byte) *
            [uint64]16777619
        $Hash = [uint32]($Product % [uint64]4294967296)
    }
    return ("{0:X8}" -f $Hash)
}

function Get-AssetPackEntry {
    param([byte[]]$Bytes, [string]$Id)
    if ($Bytes.Length -lt 40 -or
        [Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -ne "TAP1" -or
        [BitConverter]::ToUInt32($Bytes, 4) -ne 1 -or
        [BitConverter]::ToUInt32($Bytes, 8) -ne 40 -or
        [BitConverter]::ToUInt32($Bytes, 12) -ne 128) {
        throw "Asset pack header is not TAP1 v1."
    }
    $EntryCount = [BitConverter]::ToUInt32($Bytes, 16)
    $DirectoryOffset = [BitConverter]::ToUInt64($Bytes, 20)
    if ($DirectoryOffset -gt [uint64]$Bytes.Length -or
        [uint64]$EntryCount * 128 -gt
            [uint64]$Bytes.Length - $DirectoryOffset) {
        throw "Asset pack directory is out of bounds."
    }
    for ($Index = 0; $Index -lt $EntryCount; ++$Index) {
        $Offset = [int]$DirectoryOffset + $Index * 128
        $Terminator = [Array]::IndexOf($Bytes, [byte]0, $Offset, 64)
        if ($Terminator -lt 0) { $Terminator = $Offset + 64 }
        $EntryId = [Text.Encoding]::ASCII.GetString(
            $Bytes, $Offset, $Terminator - $Offset)
        if ($EntryId -ne $Id) { continue }
        $PackOffset = [BitConverter]::ToUInt64($Bytes, $Offset + 84)
        $ByteCount = [BitConverter]::ToUInt64($Bytes, $Offset + 92)
        if ($PackOffset -gt [uint64]$Bytes.Length -or
            $ByteCount -gt [uint64]$Bytes.Length - $PackOffset -or
            $ByteCount -gt [int]::MaxValue) {
            throw "Asset pack entry '$Id' is out of bounds."
        }
        return [pscustomobject]@{
            directory_offset = $Offset
            pack_offset = $PackOffset
            byte_count = $ByteCount
        }
    }
    throw "Asset pack entry '$Id' was not found."
}

function Get-EntryBytes {
    param([byte[]]$PackBytes, [object]$Entry)
    $Result = New-Object byte[] ([int]$Entry.byte_count)
    [Array]::Copy($PackBytes, [int64]$Entry.pack_offset,
        $Result, 0, [int64]$Entry.byte_count)
    return $Result
}

function Invoke-MovementTest {
    param(
        [string]$AssetPack,
        [bool]$ExpectSuccess,
        [string]$ExpectedFailure = ""
    )
    $Output = @(& $Executable --gameplay-movement-test $AssetPack 2>&1)
    $ExitCode = $LASTEXITCODE
    $Text = ($Output -join [Environment]::NewLine).Trim()
    if ($ExpectSuccess) {
        if ($ExitCode -ne 0 -or
            $Text -notmatch '^TGMO-1 movement passed:') {
            throw "TGMO-1 loader/state goldens failed.`n$(Get-ShortTail $Output)"
        }
    } elseif ($ExitCode -eq 0 -or
              $Text -notmatch 'Gameplay movement test failed:' -or
              ($ExpectedFailure -and
               $Text -notmatch [regex]::Escape($ExpectedFailure))) {
        throw "Malformed TGMO-1 pack was accepted or failure changed.`n$(Get-ShortTail $Output)"
    }
}

function Write-PayloadMutationAndReject {
    param([byte[]]$Original, [object]$Entry,
          [string]$Id, [int]$PayloadOffset)
    $Path = Join-Path $Scratch ("payload-" + $Id + ".assetpack")
    $Bytes = [byte[]]$Original.Clone()
    $Absolute = [int]$Entry.pack_offset + $PayloadOffset
    $Bytes[$Absolute] = $Bytes[$Absolute] -bxor 1
    [IO.File]::WriteAllBytes($Path, $Bytes)
    Invoke-MovementTest $Path $false "TGMO-1"
}

function Invoke-Harness {
    param([string[]]$Arguments)
    $Output = @(& $Executable --gameplay-movement-harness @Arguments 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "TGMO-1 harness rejected a valid vector.`n$(Get-ShortTail $Output)"
    }
    return ($Output -join [Environment]::NewLine)
}

try {
    $env:TECMO_SKIP_SHORTCUT = "1"
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
    if ($Build) {
        $BuildOutput = @(& (Join-Path $ProjectRoot "build.ps1") 2>&1)
        if ($LASTEXITCODE -ne 0 -or
            @($BuildOutput | Where-Object {
                $_ -match 'warning [A-Z]+[0-9]+:'
            }).Count -ne 0) {
            throw "Warning-free movement build failed.`n$(Get-ShortTail $BuildOutput)"
        }
    }
    if (!(Test-Path -LiteralPath $Executable -PathType Leaf)) {
        throw "Build output is missing; rerun with -Build."
    }
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
    New-Item -ItemType Directory -Path $Scratch | Out-Null

    $SourceOutput = @(& $Executable --gameplay-movement-source-test `
        $RomPath 2>&1)
    if ($LASTEXITCODE -ne 0 -or
        ($SourceOutput -join [Environment]::NewLine) -notmatch
            '^Built strict ROM-derived TGMO-1 movement asset') {
        throw "Direct Rev1 TGMO-1 source gate failed.`n$(Get-ShortTail $SourceOutput)"
    }
    $PackOutput = @(& $Executable --build-assetpack `
        $RomPath $PackPath 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "Rev1 TGMO-1 asset-pack build failed.`n$(Get-ShortTail $PackOutput)"
    }

    $PackBytes = [IO.File]::ReadAllBytes($PackPath)
    $MovementEntry = Get-AssetPackEntry $PackBytes "gameplay/movement"
    $CoreEntry = Get-AssetPackEntry $PackBytes "gameplay/core"
    $CameraEntry = Get-AssetPackEntry $PackBytes "gameplay/camera-projection"
    $TeamDataEntry = Get-AssetPackEntry $PackBytes "menu/team-data"
    $SourceMapEntry = Get-AssetPackEntry $PackBytes "system/source-map"
    $Payload = Get-EntryBytes $PackBytes $MovementEntry
    if ($MovementEntry.byte_count -ne 1664 -or
        (Get-Fnv1a32 $Payload) -ne "6C82A137") {
        throw "gameplay/movement size or canonical fingerprint changed."
    }
    Invoke-MovementTest $PackPath $true
    $SceneOutput = @(& $Executable --root $ProjectRoot `
        --gameplay-scene-test $PackPath 2>&1)
    if ($LASTEXITCODE -ne 0 -or
        ($SceneOutput -join [Environment]::NewLine) -notmatch
            '^GAMEPLAY SCENE SELF TEST PASS$') {
        throw "TGMO-1 live scene integration failed.`n$(Get-ShortTail $SceneOutput)"
    }

    $ListOutput = @(& $Executable --assetpack-list $PackPath 2>&1)
    if ($LASTEXITCODE -ne 0 -or
        @($ListOutput | Where-Object {
            $_ -match '^gameplay/movement\s' -and
            $_ -match 'bank=5' -and $_ -match 'cpu=0x879B' -and
            $_ -match 'bytes=1664'
        }).Count -ne 1) {
        throw "Asset-pack listing omitted the exact TGMO-1 entry.`n$(Get-ShortTail $ListOutput)"
    }

    $ExpectedSpans = @(
        @{ bank=2; fixed=$false; start=0xA89E; size=112; hash="0BD2CB61"; payload=480 },
        @{ bank=4; fixed=$false; start=0xACE4; size=66;  hash="36A1B92C"; payload=592 },
        @{ bank=5; fixed=$false; start=0x879B; size=204; hash="E05FE645"; payload=672 },
        @{ bank=5; fixed=$false; start=0x88F9; size=196; hash="613D0B4C"; payload=880 },
        @{ bank=5; fixed=$false; start=0x8E58; size=319; hash="A32D3C92"; payload=1088 },
        @{ bank=5; fixed=$false; start=0xBF6C; size=60;  hash="71812CB0"; payload=1408 },
        @{ bank=7; fixed=$true;  start=0xF106; size=171; hash="CB1D4EAF"; payload=1472 }
    )
    $SourceMap = ([Text.Encoding]::UTF8.GetString(
        (Get-EntryBytes $PackBytes $SourceMapEntry))) | ConvertFrom-Json
    $Maps = @($SourceMap.logical_entries | Where-Object {
        $_.id -eq "gameplay/movement"
    })
    $MapOk = $Maps.Count -eq 1
    if ($MapOk) {
        $Map = $Maps[0]
        $MapOk =
            $Map.schema -eq "tecmo.gameplay-movement/TGMO-1" -and
            $Map.size -eq 1664 -and
            $Map.fingerprint_fnv1a32 -eq "6C82A137" -and
            $Map.revision_sha256_identity -eq $ExpectedRomSha256 -and
            $Map.revision_full_rom_fnv1a32 -eq "0650F5B0" -and
            [bool]$Map.revision_full_rom_sha256_verified -and
            [bool]$Map.revision_full_rom_fnv1a32_verified -and
            [bool]$Map.revision_source_fingerprints_verified -and
            @($Map.dependencies).Count -eq 3 -and
            $Map.dependencies[0].entry -eq "gameplay/core" -and
            $Map.dependencies[0].fingerprint_fnv1a32 -eq "2047CCE0" -and
            $Map.dependencies[1].entry -eq "gameplay/camera-projection" -and
            $Map.dependencies[1].fingerprint_fnv1a32 -eq "53247856" -and
            $Map.dependencies[2].entry -eq "menu/team-data" -and
            $Map.dependencies[2].fingerprint_fnv1a32 -eq "812628F0" -and
            @($Map.dependencies | Where-Object {
                ![bool]$_.same_pack_required
            }).Count -eq 0 -and
            @($Map.source_spans).Count -eq 7 -and
            $Map.native_contract.direction_change_latency_updates -eq 1 -and
            $Map.native_contract.movement_fractional_bits -eq 4 -and
            (@($Map.native_contract.game_speed_adjustments) -join ',') -eq
                '5,-1,-6' -and
            $Map.native_contract.diagonal_formula -eq
                'amount-floor(amount/4)' -and
            $Map.native_contract.vertical_compare_before_move.up -eq 74 -and
            $Map.native_contract.vertical_compare_before_move.down -eq 236 -and
            [bool]$Map.native_contract.transactional -and
            [bool]$Map.native_contract.overflow_rejected -and
            $Map.live_adapter.scope -match "TGAI-directed CPU" -and
            $Map.live_adapter.condition -match "TGFT-1 evolves" -and
            $Map.live_adapter.starting_layout -match
                "Bank04 AC76.*exact source evidence.*native post-tip stable layout.*native-faithful/inferred" -and
            $Map.live_adapter.roster_binding -match
                "production binds selected TTDT starters" -and
            $Map.live_adapter.boundary_latch_reset_and_settlement -match
                "TPNL selector 1" -and
            $Map.live_adapter.boundary_latch_reset_and_settlement -match
                "other violation detection remains unported" -and
            $Map.live_adapter.pose_half_selection -match '\$8F02' -and
            $Map.live_adapter.matchup_link -match
                "fixed-link seed values.*dynamic matchup.*inferred" -and
            $Map.live_adapter.cpu_target_and_shot_policy -match
                "live-wired" -and
            $Map.live_adapter.cpu_target_and_shot_policy -match
                "deferred/non-launch" -and
            [bool]$Map.developer_harness.deterministic -and
            ![bool]$Map.developer_harness.normal_game_flow_exposed
        if ($MapOk) {
            for ($Index = 0; $Index -lt $ExpectedSpans.Count; ++$Index) {
                $Expected = $ExpectedSpans[$Index]
                $Actual = $Map.source_spans[$Index]
                $CpuBase = if ($Expected.fixed) { 0xC000 } else { 0x8000 }
                $ExpectedOffset = [uint64]$SourceMap.source.prg_offset +
                    [uint64]$Expected.bank * 0x4000 +
                    [uint64]($Expected.start - $CpuBase)
                if ($Actual.bank -ne $Expected.bank -or
                    [bool]$Actual.fixed_bank -ne [bool]$Expected.fixed -or
                    [uint64]$Actual.source_offset -ne $ExpectedOffset -or
                    $Actual.cpu_start -ne $Expected.start -or
                    $Actual.cpu_end -ne
                        ($Expected.start + $Expected.size - 1) -or
                    $Actual.size -ne $Expected.size -or
                    $Actual.fingerprint_fnv1a32 -ne $Expected.hash -or
                    $Actual.payload_offset -ne $Expected.payload) {
                    $MapOk = $false
                    break
                }
            }
        }
    }
    if (!$MapOk) {
        throw "TGMO-1 source-map provenance is incomplete or malformed."
    }

    $Normal = Invoke-Harness @($PackPath, '0', '0', '384', '148',
        '1', '0', '0', 'right', '4')
    if ($Normal -notmatch
            'frame=1 x=384 y=148 action=1 direction=0 fraction=0 animation=50 boundary=0' -or
        $Normal -notmatch
            'frame=4 x=388 y=148 action=1 direction=0 fraction=8 animation=20 boundary=0') {
        throw "Normal-speed cardinal harness vector changed.`n$Normal"
    }
    $Fast = Invoke-Harness @($PackPath, '0', '0', '384', '148',
        '0', '0', '0', 'right', '3')
    if ($Fast -notmatch
            'frame=2 x=385 y=148 action=1 direction=0 fraction=14' -or
        $Fast -notmatch
            'frame=3 x=387 y=148 action=1 direction=0 fraction=12') {
        throw "Fast-speed cardinal harness vector changed.`n$Fast"
    }
    $Slow = Invoke-Harness @($PackPath, '0', '0', '384', '148',
        '2', '0', '0', 'right', '3')
    if ($Slow -notmatch
            'frame=2 x=385 y=148 action=1 direction=0 fraction=3' -or
        $Slow -notmatch
            'frame=3 x=386 y=148 action=1 direction=0 fraction=6') {
        throw "Slow-speed cardinal harness vector changed.`n$Slow"
    }
    $Diagonal = Invoke-Harness @($PackPath, '0', '0', '384', '148',
        '1', '1', '1', 'down-right', '3')
    if ($Diagonal -notmatch
            'frame=2 x=385 y=149 action=5 direction=3 fraction=2' -or
        $Diagonal -notmatch
            'frame=3 x=386 y=150 action=5 direction=3 fraction=4') {
        throw "Diagonal reduction harness vector changed.`n$Diagonal"
    }
    $Boundary = Invoke-Harness @($PackPath, '0', '0', '149', '148',
        '1', '0', '0', 'left', '2')
    if ($Boundary -notmatch
            'frame=2 x=149 y=148 action=2 direction=1 fraction=8 animation=40 boundary=1') {
        throw "Fixed-bank boundary/latch harness vector changed.`n$Boundary"
    }
    $UpperGate = Invoke-Harness @($PackPath, '0', '0', '384', '74',
        '1', '0', '0', 'up', '3')
    if ($UpperGate -notmatch
            'frame=2 x=384 y=73 action=8 direction=5 fraction=8' -or
        $UpperGate -notmatch
            'frame=3 x=384 y=73 action=8 direction=5 fraction=0') {
        throw "Upper compare-before-move gate vector changed.`n$UpperGate"
    }
    $LowerGate = Invoke-Harness @($PackPath, '0', '0', '384', '236',
        '1', '0', '0', 'down', '3')
    if ($LowerGate -notmatch
            'frame=2 x=384 y=236 action=4 direction=2 fraction=8' -or
        $LowerGate -notmatch
            'frame=3 x=384 y=236 action=4 direction=2 fraction=0') {
        throw "Lower compare-before-move gate vector changed.`n$LowerGate"
    }

    foreach ($Mutation in @(
        @{ id="magic"; offset=0 },
        @{ id="version"; offset=4 },
        @{ id="dependency"; offset=24 },
        @{ id="revision-sha"; offset=52 },
        @{ id="reserved-header"; offset=84 },
        @{ id="descriptor"; offset=88 },
        @{ id="speed-table"; offset=176 },
        @{ id="direction-map"; offset=200 },
        @{ id="source-record"; offset=256 },
        @{ id="profile-source"; offset=480 },
        @{ id="config-source"; offset=592 },
        @{ id="config-padding"; offset=658 },
        @{ id="delta-source"; offset=672 },
        @{ id="handler-source"; offset=880 },
        @{ id="input-source"; offset=1088 },
        @{ id="map-source"; offset=1408 },
        @{ id="clamp-source"; offset=1472 },
        @{ id="trailing-padding"; offset=1663 }
    )) {
        Write-PayloadMutationAndReject $PackBytes $MovementEntry `
            $Mutation.id $Mutation.offset
    }

    foreach ($Case in @(
        @{ id="undersized-movement"; entry=$MovementEntry; size=1663;
           status="gameplay/movement entry missing or wrong-sized" },
        @{ id="oversized-movement"; entry=$MovementEntry; size=1665;
           status="gameplay/movement entry missing or wrong-sized" },
        @{ id="undersized-core"; entry=$CoreEntry; size=23415;
           status="same-pack dependency missing or wrong-sized" },
        @{ id="oversized-camera"; entry=$CameraEntry; size=1537;
           status="same-pack dependency missing or wrong-sized" },
        @{ id="undersized-team-data"; entry=$TeamDataEntry; size=96371;
           status="same-pack dependency missing or wrong-sized" }
    )) {
        $Path = Join-Path $Scratch ($Case.id + ".assetpack")
        $Bytes = [byte[]]$PackBytes.Clone()
        [BitConverter]::GetBytes([uint64]$Case.size).CopyTo(
            $Bytes, [int]$Case.entry.directory_offset + 92)
        [IO.File]::WriteAllBytes($Path, $Bytes)
        Invoke-MovementTest $Path $false $Case.status
    }
    foreach ($Case in @(
        @{ id="missing-movement"; entry=$MovementEntry;
           status="gameplay/movement entry missing or wrong-sized" },
        @{ id="missing-core"; entry=$CoreEntry;
           status="same-pack dependency missing or wrong-sized" },
        @{ id="missing-camera"; entry=$CameraEntry;
           status="same-pack dependency missing or wrong-sized" },
        @{ id="missing-team-data"; entry=$TeamDataEntry;
           status="same-pack dependency missing or wrong-sized" }
    )) {
        $Path = Join-Path $Scratch ($Case.id + ".assetpack")
        $Bytes = [byte[]]$PackBytes.Clone()
        $Bytes[[int]$Case.entry.directory_offset] = [byte][char]'x'
        [IO.File]::WriteAllBytes($Path, $Bytes)
        Invoke-MovementTest $Path $false $Case.status
    }
    foreach ($Case in @(
        @{ id="malformed-core"; entry=$CoreEntry; offset=184 },
        @{ id="malformed-camera"; entry=$CameraEntry; offset=480 },
        @{ id="malformed-team-data"; entry=$TeamDataEntry; offset=1000 }
    )) {
        $Path = Join-Path $Scratch ($Case.id + ".assetpack")
        $Bytes = [byte[]]$PackBytes.Clone()
        $Absolute = [int]$Case.entry.pack_offset + $Case.offset
        $Bytes[$Absolute] = $Bytes[$Absolute] -bxor 1
        [IO.File]::WriteAllBytes($Path, $Bytes)
        Invoke-MovementTest $Path $false "dependency contract rejected"
    }

    $InvalidHarness = @(& $Executable --gameplay-movement-harness `
        $PackPath 0 0 768 148 1 0 0 right 2 2>&1)
    if ($LASTEXITCODE -eq 0 -or
        ($InvalidHarness -join [Environment]::NewLine) -notmatch
            'Movement harness argument rejected') {
        throw "Out-of-range harness input was accepted."
    }

    $RomBytes = [IO.File]::ReadAllBytes($RomPath)
    $Trainer = if (($RomBytes[6] -band 4) -ne 0) { 512 } else { 0 }
    $Prg = 16 + $Trainer
    $RomMutationCount = 0
    foreach ($Span in $ExpectedSpans) {
        $CpuBase = if ($Span.fixed) { 0xC000 } else { 0x8000 }
        $Offset = $Prg + $Span.bank * 0x4000 + ($Span.start - $CpuBase)
        $MutatedRom = Join-Path $Scratch `
            ("rom-{0:X4}.nes" -f $Span.start)
        $Bytes = [byte[]]$RomBytes.Clone()
        $Bytes[$Offset] = $Bytes[$Offset] -bxor 1
        [IO.File]::WriteAllBytes($MutatedRom, $Bytes)
        $Output = @(& $Executable --gameplay-movement-source-test `
            $MutatedRom 2>&1)
        if ($LASTEXITCODE -eq 0 -or
            ($Output -join [Environment]::NewLine) -notmatch
                'TGMO-1 import requires the exact Rev1 ROM fingerprint') {
            $CpuLabel = "{0:X4}" -f $Span.start
            throw "Rev1 source mutation at 0x$CpuLabel was accepted.`n$(Get-ShortTail $Output)"
        }
        ++$RomMutationCount
    }

    Write-Host ("TGMO-1 focused tests passed: exact Rev1 importer and seven " +
        "source spans, strict payload/provenance/dependencies, transactional " +
        "state rejection, normal/fast/slow/diagonal/boundary/Y-gate harness " +
        "vectors, live scene integration, $RomMutationCount ROM mutations")
    $global:LASTEXITCODE = 0
} finally {
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
}
