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
    throw "TGAI-1 tests require the exact Tecmo NBA Basketball Rev1 ROM."
}

$BuildDir = Join-Path $ProjectRoot "build"
$Executable = Join-Path $BuildDir "tecmo_port.exe"
$Scratch = [IO.Path]::GetFullPath(
    (Join-Path $BuildDir "gameplay_cpu_steering_test"))
$BuildPrefix = [IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/') +
    [IO.Path]::DirectorySeparatorChar
if (!$Scratch.StartsWith($BuildPrefix,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw "Gameplay CPU-steering scratch path escaped build\."
}
$PackPath = Join-Path $Scratch "gameplay-cpu-steering.assetpack"
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

function Invoke-SteeringTest {
    param(
        [string]$AssetPack,
        [bool]$ExpectSuccess,
        [string]$ExpectedFailure = ""
    )
    $Output = @(& $Executable --gameplay-cpu-steering-test `
        $AssetPack 2>&1)
    $ExitCode = $LASTEXITCODE
    $Text = ($Output -join [Environment]::NewLine).Trim()
    if ($ExpectSuccess) {
        if ($ExitCode -ne 0 -or
            $Text -notmatch
                '^TGAI-1 CPU steering isolated: commands=680 handlers=24 directions=8 tgmo_adapter=1 live=0$') {
            throw "TGAI-1 loader/vector goldens failed.`n$(Get-ShortTail $Output)"
        }
    } elseif ($ExitCode -eq 0 -or
              $Text -notmatch 'Gameplay CPU steering test failed:' -or
              ($ExpectedFailure -and
               $Text -notmatch [regex]::Escape($ExpectedFailure))) {
        throw "Malformed TGAI-1 pack was accepted or failure changed.`n$(Get-ShortTail $Output)"
    }
}

function Invoke-Inspect {
    param(
        [string]$Offset,
        [string]$Horizontal,
        [string]$Depth,
        [string]$Expected
    )
    $Output = @(& $Executable --gameplay-cpu-steering-inspect `
        $PackPath $Offset $Horizontal $Depth 2>&1)
    $Text = $Output -join [Environment]::NewLine
    if ($LASTEXITCODE -ne 0 -or $Text -notmatch $Expected) {
        throw "TGAI-1 inspect vector changed.`n$(Get-ShortTail $Output)"
    }
    return $Text
}

function Invoke-Harness {
    param(
        [string]$Actor,
        [string]$Possession,
        [string]$Orientation,
        [string]$Holder,
        [string]$Matchup,
        [string]$Difficulty,
        [string[]]$Coordinates,
        [bool]$ExpectSuccess = $true,
        [string]$ExpectedFailure = ""
    )
    $Arguments = @(
        "--gameplay-cpu-steering-harness", $PackPath, $Actor,
        $Possession, $Orientation, $Holder, $Matchup, $Difficulty
    ) + @($Coordinates)
    $Output = @(& $Executable @Arguments 2>&1)
    $ExitCode = $LASTEXITCODE
    $Text = ($Output -join [Environment]::NewLine).Trim()
    if ($ExpectSuccess) {
        if ($ExitCode -ne 0 -or
            $Text -notmatch '^TGAI-1 harness ' -or
            $Text -notmatch 'target_policy=native-harness ' -or
            $Text -notmatch 'quantizer=rom-exact live=0$') {
            throw "CPU-steering harness vector failed.`n$(Get-ShortTail $Output)"
        }
    } elseif ($ExitCode -eq 0 -or
              ($ExpectedFailure -and
               $Text -notmatch [regex]::Escape($ExpectedFailure))) {
        throw "Invalid CPU-steering harness input was accepted.`n$(Get-ShortTail $Output)"
    }
    return $Text
}

function Invoke-MovementHarness {
    param(
        [string]$Actor,
        [string]$Possession,
        [string]$Orientation,
        [string]$Holder,
        [string]$Matchup,
        [string]$Difficulty,
        [string]$Rating,
        [string]$Condition,
        [string]$Speed,
        [string]$Frames,
        [string[]]$Coordinates,
        [bool]$ExpectSuccess = $true,
        [string]$ExpectedFailure = ""
    )
    $Arguments = @(
        "--gameplay-cpu-steering-movement-harness", $PackPath,
        $Actor, $Possession, $Orientation, $Holder, $Matchup,
        $Difficulty, $Rating, $Condition, $Speed, $Frames
    ) + @($Coordinates)
    $Output = @(& $Executable @Arguments 2>&1)
    $ExitCode = $LASTEXITCODE
    $Text = ($Output -join [Environment]::NewLine).Trim()
    if ($ExpectSuccess) {
        if ($ExitCode -ne 0 -or
            $Text -notmatch '^TGAI-TGMO harness ' -or
            $Text -notmatch 'target_policy=native-harness ' -or
            $Text -notmatch 'zero_input=native-neutral ' -or
            $Text -notmatch 'quantizer=rom-exact ' -or
            $Text -notmatch 'movement=rom-exact secondary=1 live=0') {
            throw ("CPU-steering movement harness vector failed.`n" +
                "$(Get-ShortTail $Output)")
        }
    } elseif ($ExitCode -eq 0 -or
              ($ExpectedFailure -and
               $Text -notmatch [regex]::Escape($ExpectedFailure))) {
        throw ("Invalid CPU-steering movement harness input was " +
            "accepted.`n$(Get-ShortTail $Output)")
    }
    return $Text
}

function Get-HarnessFingerprint {
    param([string]$Text)
    $Match = [regex]::Match($Text, 'snapshot=([0-9A-F]{8})')
    if (!$Match.Success) {
        throw "CPU-steering harness omitted its snapshot fingerprint."
    }
    return $Match.Groups[1].Value
}

function Write-PayloadMutationAndReject {
    param([byte[]]$Original, [object]$Entry,
          [string]$Id, [int]$PayloadOffset)
    $Path = Join-Path $Scratch ("payload-" + $Id + ".assetpack")
    $Bytes = [byte[]]$Original.Clone()
    $Absolute = [int]$Entry.pack_offset + $PayloadOffset
    $Bytes[$Absolute] = $Bytes[$Absolute] -bxor 1
    [IO.File]::WriteAllBytes($Path, $Bytes)
    Invoke-SteeringTest $Path $false "TGAI-1"
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
            throw "Warning-free CPU-steering build failed.`n$(Get-ShortTail $BuildOutput)"
        }
    }
    if (!(Test-Path -LiteralPath $Executable -PathType Leaf)) {
        throw "Build output is missing; rerun with -Build."
    }
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
    New-Item -ItemType Directory -Path $Scratch | Out-Null

    $SourceOutput = @(& $Executable --gameplay-cpu-steering-source-test `
        $RomPath 2>&1)
    if ($LASTEXITCODE -ne 0 -or
        ($SourceOutput -join [Environment]::NewLine) -notmatch
            '^Built strict ROM-derived TGAI-1 CPU steering evidence asset') {
        throw "Direct Rev1 TGAI-1 source gate failed.`n$(Get-ShortTail $SourceOutput)"
    }
    $PackOutput = @(& $Executable --build-assetpack `
        $RomPath $PackPath 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "Rev1 TGAI-1 asset-pack build failed.`n$(Get-ShortTail $PackOutput)"
    }

    $PackBytes = [IO.File]::ReadAllBytes($PackPath)
    $SteeringEntry = Get-AssetPackEntry $PackBytes "gameplay/cpu-steering"
    $MovementEntry = Get-AssetPackEntry $PackBytes "gameplay/movement"
    $SourceMapEntry = Get-AssetPackEntry $PackBytes "system/source-map"
    $Payload = Get-EntryBytes $PackBytes $SteeringEntry
    if ($SteeringEntry.byte_count -ne 7616 -or
        (Get-Fnv1a32 $Payload) -ne "D6C4DB35") {
        throw "gameplay/cpu-steering size or canonical fingerprint changed."
    }
    Invoke-SteeringTest $PackPath $true

    $ListOutput = @(& $Executable --assetpack-list $PackPath 2>&1)
    if ($LASTEXITCODE -ne 0 -or
        @($ListOutput | Where-Object {
            $_ -match '^gameplay/cpu-steering\s' -and
            $_ -match 'bank=6' -and $_ -match 'cpu=0x8B90' -and
            $_ -match 'bytes=7616'
        }).Count -ne 1) {
        throw "Asset-pack listing omitted the exact TGAI-1 entry.`n$(Get-ShortTail $ListOutput)"
    }

    $ExpectedHandlers = @(
        37088,37707,37504,36958,36858,36754,36653,36626,
        36567,36805,36048,35904,36431,37157,37190,37234,
        36997,35866,35866,35866,36914,35830,35809,36722
    )
    $ExpectedSpans = @(
        @{ bank=6; fixed=$false; start=0x81F7; size=221;  hash="23BB7271"; payload=640 },
        @{ bank=6; fixed=$false; start=0x87AE; size=258;  hash="F866B06C"; payload=864 },
        @{ bank=6; fixed=$false; start=0x88DA; size=444;  hash="9616E586"; payload=1136 },
        @{ bank=6; fixed=$false; start=0x8B90; size=81;   hash="9AD2BA91"; payload=1584 },
        @{ bank=6; fixed=$false; start=0x8BE1; size=1623; hash="344298FE"; payload=1680 },
        @{ bank=6; fixed=$false; start=0x9280; size=170;  hash="C82E6853"; payload=3312 },
        @{ bank=6; fixed=$false; start=0x938B; size=662;  hash="47818A62"; payload=3488 },
        @{ bank=7; fixed=$true;  start=0xC006; size=3;    hash="14B2472E"; payload=4160 },
        @{ bank=7; fixed=$true;  start=0xCBE0; size=23;   hash="41C5B5C8"; payload=4176 },
        @{ bank=4; fixed=$false; start=0x9F2E; size=3400; hash="71331A96"; payload=4208 }
    )
    $SourceMap = ([Text.Encoding]::UTF8.GetString(
        (Get-EntryBytes $PackBytes $SourceMapEntry))) | ConvertFrom-Json
    $Maps = @($SourceMap.logical_entries | Where-Object {
        $_.id -eq "gameplay/cpu-steering"
    })
    $MapOk = $Maps.Count -eq 1
    if ($MapOk) {
        $Map = $Maps[0]
        $MapOk =
            $Map.schema -eq "tecmo.gameplay-cpu-steering/TGAI-1" -and
            $Map.size -eq 7616 -and
            $Map.fingerprint_fnv1a32 -eq "D6C4DB35" -and
            $Map.revision_sha256_identity -eq $ExpectedRomSha256 -and
            $Map.revision_full_rom_fnv1a32 -eq "0650F5B0" -and
            [bool]$Map.revision_full_rom_sha256_verified -and
            [bool]$Map.revision_full_rom_fnv1a32_verified -and
            [bool]$Map.revision_source_fingerprints_verified -and
            $Map.dependency.entry -eq "gameplay/movement" -and
            $Map.dependency.size -eq 1664 -and
            $Map.dependency.fingerprint_fnv1a32 -eq "6C82A137" -and
            [bool]$Map.dependency.same_pack_required -and
            @($Map.source_spans).Count -eq 10 -and
            $Map.native_contract.actor_slots -eq 10 -and
            $Map.native_contract.script_state -eq 4 -and
            $Map.native_contract.command_transport.record_bytes -eq 5 -and
            $Map.native_contract.command_transport.record_count -eq 680 -and
            $Map.native_contract.command_transport.code_resumes_cpu -eq '$AC76' -and
            $Map.native_contract.opcode_dispatch.count -eq 24 -and
            (@($Map.native_contract.opcode_dispatch.handler_cpu) -join ',') -eq
                ($ExpectedHandlers -join ',') -and
            $Map.native_contract.direction_quantizer.cpu -eq
                '$92D4-$92DD; $92FE -> $88DA-$899D' -and
            $Map.native_contract.direction_quantizer.dominant_axis_ratio -match
                '16-bit doubling wrap retained' -and
            (@($Map.native_contract.direction_quantizer.octant_map) -join ',') -eq
                '3,6,4,7,0,1,2,5' -and
            [bool]$Map.native_contract.direction_quantizer.zero_vector_keeps_prior_direction -and
            ![bool]$Map.native_contract.live_wired -and
            [bool]$Map.native_contract.transactional -and
            $Map.evidence_limits.next_integration -match
                'bind proven command inputs' -and
            (@($Map.evidence_limits.not_claimed) -join '|') -match
                'candidate scan \$B081-\$B32E' -and
            [bool]$Map.developer_harness.deterministic -and
            [bool]$Map.developer_harness.cli_only -and
            $Map.developer_harness.coordinate_slots -eq 10 -and
            $Map.developer_harness.coordinate_space -eq
                'TGCT-1 canonical X=0..767 Y=0..239' -and
            @($Map.developer_harness.inputs).Count -eq 7 -and
            [bool]$Map.developer_harness.possession_holder_coherent -and
            [bool]$Map.developer_harness.matchup_assignment_caller_owned -and
            ![bool]$Map.developer_harness.target_policy.rom_exact -and
            $Map.developer_harness.target_policy.holder -eq
                'native scene-style hoop approach' -and
            $Map.developer_harness.target_policy.other_actor -eq
                'explicit linked/matchup actor coordinate' -and
            [bool]$Map.developer_harness.direction_quantizer_rom_exact -and
            $Map.developer_harness.zero_vector -eq
                'preserve prior direction' -and
            $Map.developer_harness.snapshot_fingerprint -eq
                'domain-separated canonical FNV1a32' -and
            $Map.developer_harness.movement_adapter.cli -eq
                '--gameplay-cpu-steering-movement-harness' -and
            $Map.developer_harness.movement_adapter.direction_to_input -eq
                'exact same-pack TGMO direction identity' -and
            $Map.developer_harness.movement_adapter.movement_kernel -eq
                'exact TGMO-1 transactional step' -and
            [bool]$Map.developer_harness.movement_adapter.one_update_latency_rom_exact -and
            [bool]$Map.developer_harness.movement_adapter.secondary_actor_path -and
            $Map.developer_harness.movement_adapter.zero_vector_input -eq
                'native neutral policy; TGAI no-write remains exact' -and
            [bool]$Map.developer_harness.movement_adapter.selected_actor_coordinate_reconciled_each_step -and
            ![bool]$Map.developer_harness.movement_adapter.live_wired -and
            [bool]$Map.developer_harness.movement_adapter.transactional -and
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
        throw "TGAI-1 source-map provenance is incomplete or malformed."
    }

    $First = Invoke-Inspect '0' '20' '10' `
        'offset=\$0000 cpu=\$9F2E opcode=4 args=0A,00,00,00 handler=\$8FFA kind=actor-target'
    if ($First -notmatch 'delta=\(20,10\) direction=0 name=right live=0') {
        throw "Inclusive 2:1 horizontal direction vector changed.`n$First"
    }
    $Diagonal = Invoke-Inspect '0' '19' '10' `
        'delta=\(19,10\) direction=3 name=down-right live=0'
    $FreeThrowA = Invoke-Inspect '0x7D' '-10' '10' `
        'offset=\$007D cpu=\$9FAB opcode=3 args=08,00,00,00 handler=\$905E kind=control'
    if ($FreeThrowA -notmatch 'direction=4 name=down-left') {
        throw "Free-throw-A/direction inspect vector changed.`n$FreeThrowA"
    }
    $FreeThrowB = Invoke-Inspect '0xD7' '10' '-10' `
        'offset=\$00D7 cpu=\$A005 opcode=2 args=B4,00,96,00 handler=\$9280 kind=absolute-target'
    if ($FreeThrowB -notmatch 'direction=6 name=up-right') {
        throw "Free-throw-B/direction inspect vector changed.`n$FreeThrowB"
    }
    $Last = Invoke-Inspect '0xD43' '-20' '-10' `
        'offset=\$0D43 cpu=\$AC71 opcode=1 args=80,0C,00,00 handler=\$934B kind=control'
    if ($Last -notmatch 'direction=1 name=left') {
        throw "Last-command/dominant direction vector changed.`n$Last"
    }
    $WrappedExtreme = Invoke-Inspect '0' '-32768' '-32768' `
        'delta=\(-32768,-32768\) direction=1 name=left live=0'

    foreach ($InvalidInspect in @(
        @('1', '1', '1', 'CPU steering command offset rejected'),
        @('0', '0', '0', 'CPU steering zero/invalid direction vector rejected'),
        @('0', '32768', '1', 'CPU steering inspect argument rejected')
    )) {
        $Output = @(& $Executable --gameplay-cpu-steering-inspect `
            $PackPath $InvalidInspect[0] $InvalidInspect[1] `
            $InvalidInspect[2] 2>&1)
        if ($LASTEXITCODE -eq 0 -or
            ($Output -join [Environment]::NewLine) -notmatch
                [regex]::Escape($InvalidInspect[3])) {
            throw "Invalid CPU-steering inspect input was accepted.`n$(Get-ShortTail $Output)"
        }
    }

    $CourtCoordinates = @(
        '384,148','420,120','440,180','360,100','400,200',
        '500,160','520,110','540,190','460,90','480,210'
    )
    $Linked = Invoke-Harness '5' '0' '0' '0' '0' '2' $CourtCoordinates
    $LinkedRepeat = Invoke-Harness '5' '0' '0' '0' '0' '2' `
        $CourtCoordinates
    if ($Linked -cne $LinkedRepeat -or
        $Linked -notmatch
            '^TGAI-1 harness actor=5 team=1 possession=0 orientation=0 holder=0 matchup=0 difficulty=2 snapshot=15AEBE1B live=0' -or
        $Linked -notmatch
            'court=0:\(384,148\);1:\(420,120\);2:\(440,180\);3:\(360,100\);4:\(400,200\);5:\(500,160\);6:\(520,110\);7:\(540,190\);8:\(460,90\);9:\(480,210\)' -or
        $Linked -notmatch
            'target=linked-actor target_actor=0 from=\(500,160\) to=\(384,148\) delta=\(-116,-12\) direction=1 name=left write=1') {
        throw "Linked-actor harness golden changed.`n$Linked"
    }

    $LeftHoop = Invoke-Harness '0' '0' '0' '0' '5' '0' `
        $CourtCoordinates
    if ($LeftHoop -notmatch
            'snapshot=57B29369 live=0' -or
        $LeftHoop -notmatch
            'target=hoop-approach target_actor=none from=\(384,148\) to=\(208,148\) delta=\(-176,0\) direction=1 name=left write=1') {
        throw "Left-hoop/easy harness golden changed.`n$LeftHoop"
    }
    $RightHoop = Invoke-Harness '0' '0' '1' '0' '5' '2' `
        $CourtCoordinates
    if ($RightHoop -notmatch
            'snapshot=34F66F76 live=0' -or
        $RightHoop -notmatch
            'target=hoop-approach target_actor=none from=\(384,148\) to=\(568,148\) delta=\(184,0\) direction=0 name=right write=1') {
        throw "Right-hoop/hard harness golden changed.`n$RightHoop"
    }

    $BaselineFingerprint = Get-HarnessFingerprint $Linked
    for ($ActorIndex = 0; $ActorIndex -lt 10; ++$ActorIndex) {
        $MutatedCoordinates = @($CourtCoordinates)
        $Parts = $MutatedCoordinates[$ActorIndex].Split(',')
        $MutatedCoordinates[$ActorIndex] =
            (([int]$Parts[0] + 1).ToString() + ',' + $Parts[1])
        $Mutated = Invoke-Harness '5' '0' '0' '0' '0' '2' `
            $MutatedCoordinates
        if ((Get-HarnessFingerprint $Mutated) -eq $BaselineFingerprint) {
            throw "Harness fingerprint ignored actor $ActorIndex coordinates."
        }
    }
    $ContextVariants = @(
        @{ actor='6'; possession='0'; orientation='0'; holder='0'; matchup='1'; difficulty='2' },
        @{ actor='5'; possession='0'; orientation='1'; holder='0'; matchup='0'; difficulty='2' },
        @{ actor='5'; possession='0'; orientation='0'; holder='1'; matchup='0'; difficulty='2' },
        @{ actor='5'; possession='0'; orientation='0'; holder='0'; matchup='1'; difficulty='2' },
        @{ actor='5'; possession='0'; orientation='0'; holder='0'; matchup='0'; difficulty='1' },
        @{ actor='0'; possession='1'; orientation='0'; holder='5'; matchup='5'; difficulty='2' }
    )
    foreach ($Variant in $ContextVariants) {
        $VariantOutput = Invoke-Harness $Variant.actor $Variant.possession `
            $Variant.orientation $Variant.holder $Variant.matchup `
            $Variant.difficulty $CourtCoordinates
        if ((Get-HarnessFingerprint $VariantOutput) -eq
            $BaselineFingerprint) {
            throw "Harness fingerprint ignored a coherent context field."
        }
    }

    $ZeroCoordinates = @($CourtCoordinates)
    $ZeroCoordinates[5] = $ZeroCoordinates[0]
    $Zero = Invoke-Harness '5' '0' '0' '0' '0' '1' $ZeroCoordinates
    if ($Zero -notmatch
            'target=linked-actor target_actor=0 from=\(384,148\) to=\(384,148\) delta=\(0,0\) direction=keep name=keep write=0') {
        throw "Harness zero-vector keep-direction contract changed.`n$Zero"
    }

    $Moved = Invoke-MovementHarness '5' '0' '0' '0' '0' '2' `
        '20' '100' '1' '5' $CourtCoordinates
    $MovedRepeat = Invoke-MovementHarness '5' '0' '0' '0' '0' '2' `
        '20' '100' '1' '5' $CourtCoordinates
    if ($Moved -cne $MovedRepeat -or
        $Moved -notmatch
            'frame=1 snapshot=15AEBE1B from=\(500,160\) target=\(384,148\) steering=left write=1 held=left x=500 y=160 action=2 direction=1 fraction=0 animation=50 boundary=0' -or
        $Moved -notmatch
            'frame=2 snapshot=15AEBE1B from=\(500,160\) target=\(384,148\) steering=left write=1 held=left x=499 y=160 action=2 direction=1 fraction=3 animation=40 boundary=0' -or
        $Moved -notmatch
            'frame=5 snapshot=C1689E4A from=\(497,160\) target=\(384,148\) steering=left write=1 held=left x=496 y=160 action=2 direction=1 fraction=12 animation=10 boundary=0') {
        throw "TGAI-to-TGMO cardinal/latency golden changed.`n$Moved"
    }

    $DiagonalCoordinates = @($CourtCoordinates)
    $DiagonalCoordinates[0] = '510,170'
    $Diagonal = Invoke-MovementHarness '5' '0' '0' '0' '0' '1' `
        '20' '100' '1' '4' $DiagonalCoordinates
    if ($Diagonal -notmatch
            'frame=1 snapshot=ADC902B2 from=\(500,160\) target=\(510,170\) steering=down-right write=1 held=down-right x=500 y=160 action=5 direction=3 fraction=0 animation=50 boundary=0' -or
        $Diagonal -notmatch
            'frame=3 snapshot=ADC902B2 from=\(500,160\) target=\(510,170\) steering=down-right write=1 held=down-right x=501 y=161 action=5 direction=3 fraction=14 animation=30 boundary=0') {
        throw "TGAI-to-TGMO diagonal golden changed.`n$Diagonal"
    }

    $ZeroMovement = Invoke-MovementHarness '5' '0' '0' '0' '0' '1' `
        '20' '100' '1' '2' $ZeroCoordinates
    if ($ZeroMovement -notmatch
            'frame=1 snapshot=BD6D2646 from=\(384,148\) target=\(384,148\) steering=keep write=0 held=neutral x=384 y=148 action=0 direction=1 fraction=0 animation=20 boundary=0' -or
        $ZeroMovement -notmatch
            'frame=2 snapshot=BD6D2646 from=\(384,148\) target=\(384,148\) steering=keep write=0 held=neutral x=384 y=148 action=0 direction=1 fraction=0 animation=10 boundary=0') {
        throw "TGAI-to-TGMO zero-vector neutral golden changed.`n$ZeroMovement"
    }

    $BoundaryCoordinates = @($CourtCoordinates)
    $BoundaryCoordinates[0] = '0,148'
    $BoundaryCoordinates[5] = '149,148'
    $Boundary = Invoke-MovementHarness '5' '0' '0' '0' '0' '1' `
        '20' '100' '1' '3' $BoundaryCoordinates
    if ($Boundary -notmatch
            'frame=3 snapshot=2E289D43 from=\(149,148\) target=\(0,148\) steering=left write=1 held=left x=149 y=148 action=2 direction=1 fraction=6 animation=30 boundary=0') {
        throw "TGAI-to-TGMO secondary-actor clamp golden changed.`n$Boundary"
    }

    Invoke-MovementHarness -Actor '5' -Possession '0' -Orientation '0' `
        -Holder '0' -Matchup '0' -Difficulty '2' -Rating '20' `
        -Condition '100' -Speed '1' -Frames '1' `
        -Coordinates @($CourtCoordinates[0..8]) -ExpectSuccess $false `
        -ExpectedFailure 'CPU steering movement harness requires' | Out-Null
    foreach ($InvalidMovementScalar in @(
        @{ rating='256'; condition='100'; speed='1'; frames='1'; holder='0'; matchup='0'; failure='argument rejected' },
        @{ rating='20'; condition='101'; speed='1'; frames='1'; holder='0'; matchup='0'; failure='argument rejected' },
        @{ rating='20'; condition='100'; speed='3'; frames='1'; holder='0'; matchup='0'; failure='argument rejected' },
        @{ rating='20'; condition='100'; speed='1'; frames='4097'; holder='0'; matchup='0'; failure='argument rejected' },
        @{ rating='0'; condition='100'; speed='2'; frames='1'; holder='0'; matchup='0'; failure='step 1 rejected' },
        @{ rating='20'; condition='100'; speed='1'; frames='1'; holder='5'; matchup='0'; failure='step 1 rejected' },
        @{ rating='20'; condition='100'; speed='1'; frames='1'; holder='0'; matchup='6'; failure='step 1 rejected' }
    )) {
        Invoke-MovementHarness -Actor '5' -Possession '0' `
            -Orientation '0' -Holder $InvalidMovementScalar.holder `
            -Matchup $InvalidMovementScalar.matchup -Difficulty '2' `
            -Rating $InvalidMovementScalar.rating `
            -Condition $InvalidMovementScalar.condition `
            -Speed $InvalidMovementScalar.speed `
            -Frames $InvalidMovementScalar.frames `
            -Coordinates $CourtCoordinates -ExpectSuccess $false `
            -ExpectedFailure $InvalidMovementScalar.failure | Out-Null
    }

    Invoke-Harness -Actor '5' -Possession '0' -Orientation '0' `
        -Holder '0' -Matchup '0' -Difficulty '2' `
        -Coordinates @($CourtCoordinates[0..8]) -ExpectSuccess $false `
        -ExpectedFailure 'CPU steering harness requires' | Out-Null
    Invoke-Harness -Actor '5' -Possession '0' -Orientation '0' `
        -Holder '0' -Matchup '0' -Difficulty '2' `
        -Coordinates @($CourtCoordinates + '200,100') `
        -ExpectSuccess $false `
        -ExpectedFailure 'CPU steering harness requires' | Out-Null
    foreach ($InvalidScalar in @(
        @{ actor='10'; possession='0'; orientation='0'; holder='0'; matchup='0'; difficulty='2'; failure='argument rejected' },
        @{ actor='5'; possession='2'; orientation='0'; holder='0'; matchup='0'; difficulty='2'; failure='argument rejected' },
        @{ actor='5'; possession='0'; orientation='2'; holder='0'; matchup='0'; difficulty='2'; failure='argument rejected' },
        @{ actor='5'; possession='0'; orientation='0'; holder='10'; matchup='0'; difficulty='2'; failure='argument rejected' },
        @{ actor='5'; possession='0'; orientation='0'; holder='0'; matchup='10'; difficulty='2'; failure='argument rejected' },
        @{ actor='5'; possession='0'; orientation='0'; holder='0'; matchup='0'; difficulty='3'; failure='argument rejected' },
        @{ actor='5'; possession='0'; orientation='0'; holder='5'; matchup='0'; difficulty='2'; failure='state rejected' },
        @{ actor='5'; possession='0'; orientation='0'; holder='0'; matchup='6'; difficulty='2'; failure='state rejected' }
    )) {
        Invoke-Harness -Actor $InvalidScalar.actor `
            -Possession $InvalidScalar.possession `
            -Orientation $InvalidScalar.orientation `
            -Holder $InvalidScalar.holder -Matchup $InvalidScalar.matchup `
            -Difficulty $InvalidScalar.difficulty `
            -Coordinates $CourtCoordinates -ExpectSuccess $false `
            -ExpectedFailure $InvalidScalar.failure | Out-Null
    }
    foreach ($InvalidCoordinate in @('768,0','0,240','-1,0',
                                      '0,-1','20x,30','20;30')) {
        $BadCoordinates = @($CourtCoordinates)
        $BadCoordinates[9] = $InvalidCoordinate
        Invoke-Harness -Actor '5' -Possession '0' -Orientation '0' `
            -Holder '0' -Matchup '0' -Difficulty '2' `
            -Coordinates $BadCoordinates -ExpectSuccess $false `
            -ExpectedFailure 'argument rejected' | Out-Null
    }

    foreach ($Mutation in @(
        @{ id="magic"; offset=0 },
        @{ id="version"; offset=4 },
        @{ id="dependency"; offset=24 },
        @{ id="revision-sha"; offset=52 },
        @{ id="reserved-header"; offset=86 },
        @{ id="direction-map"; offset=88 },
        @{ id="descriptor"; offset=96 },
        @{ id="handler-table"; offset=216 },
        @{ id="target-mask"; offset=264 },
        @{ id="header-tail"; offset=269 },
        @{ id="source-record"; offset=320 },
        @{ id="actor-source"; offset=640 },
        @{ id="actor-padding"; offset=861 },
        @{ id="reference-source"; offset=864 },
        @{ id="target-direction-source"; offset=1136 },
        @{ id="dispatch-source"; offset=1584 },
        @{ id="dispatch-padding"; offset=1665 },
        @{ id="handlers-source"; offset=1680 },
        @{ id="handlers-padding"; offset=3303 },
        @{ id="target-apply-source"; offset=3312 },
        @{ id="formation-source"; offset=3488 },
        @{ id="trampoline-source"; offset=4160 },
        @{ id="reader-source"; offset=4176 },
        @{ id="command-opcode"; offset=4208 },
        @{ id="last-command"; offset=7603 },
        @{ id="trailing-padding"; offset=7615 }
    )) {
        Write-PayloadMutationAndReject $PackBytes $SteeringEntry `
            $Mutation.id $Mutation.offset
    }

    foreach ($Case in @(
        @{ id="undersized-steering"; entry=$SteeringEntry; size=7615;
           status="entry missing or wrong-sized" },
        @{ id="oversized-steering"; entry=$SteeringEntry; size=7617;
           status="entry missing or wrong-sized" },
        @{ id="undersized-movement"; entry=$MovementEntry; size=1663;
           status="same-pack TGMO-1 dependency missing" },
        @{ id="oversized-movement"; entry=$MovementEntry; size=1665;
           status="same-pack TGMO-1 dependency missing" }
    )) {
        $Path = Join-Path $Scratch ($Case.id + ".assetpack")
        $Bytes = [byte[]]$PackBytes.Clone()
        [BitConverter]::GetBytes([uint64]$Case.size).CopyTo(
            $Bytes, [int]$Case.entry.directory_offset + 92)
        [IO.File]::WriteAllBytes($Path, $Bytes)
        Invoke-SteeringTest $Path $false $Case.status
    }
    foreach ($Case in @(
        @{ id="missing-steering"; entry=$SteeringEntry;
           status="entry missing or wrong-sized" },
        @{ id="missing-movement"; entry=$MovementEntry;
           status="same-pack TGMO-1 dependency missing" }
    )) {
        $Path = Join-Path $Scratch ($Case.id + ".assetpack")
        $Bytes = [byte[]]$PackBytes.Clone()
        $Bytes[[int]$Case.entry.directory_offset] = [byte][char]'x'
        [IO.File]::WriteAllBytes($Path, $Bytes)
        Invoke-SteeringTest $Path $false $Case.status
    }
    $MalformedDependencyPath =
        Join-Path $Scratch "malformed-movement.assetpack"
    $MalformedDependency = [byte[]]$PackBytes.Clone()
    $MalformedDependency[[int]$MovementEntry.pack_offset] =
        $MalformedDependency[[int]$MovementEntry.pack_offset] -bxor 1
    [IO.File]::WriteAllBytes($MalformedDependencyPath, $MalformedDependency)
    Invoke-SteeringTest $MalformedDependencyPath $false `
        "same-pack TGMO-1 dependency rejected"

    $RomBytes = [IO.File]::ReadAllBytes($RomPath)
    $Trainer = if (($RomBytes[6] -band 4) -ne 0) { 512 } else { 0 }
    $Prg = 16 + $Trainer
    $RomMutationCount = 0
    foreach ($Span in $ExpectedSpans) {
        $CpuBase = if ($Span.fixed) { 0xC000 } else { 0x8000 }
        $Offset = $Prg + $Span.bank * 0x4000 + ($Span.start - $CpuBase)
        $MutatedRom = Join-Path $Scratch `
            ("rom-{0}-{1:X4}.nes" -f $Span.bank, $Span.start)
        $Bytes = [byte[]]$RomBytes.Clone()
        $Bytes[$Offset] = $Bytes[$Offset] -bxor 1
        [IO.File]::WriteAllBytes($MutatedRom, $Bytes)
        $Output = @(& $Executable --gameplay-cpu-steering-source-test `
            $MutatedRom 2>&1)
        if ($LASTEXITCODE -eq 0 -or
            ($Output -join [Environment]::NewLine) -notmatch
                'TGAI-1 import requires the exact Rev1 ROM fingerprint') {
            throw ("Rev1 source mutation at Bank$($Span.bank) " +
                "$($Span.start) was accepted.`n$(Get-ShortTail $Output)")
        }
        ++$RomMutationCount
    }

    Write-Host ("TGAI-1 focused tests passed: exact Rev1 importer and ten " +
        "source spans, 680 aligned commands, 24 handlers, eight exact " +
        "direction codes, deterministic ten-coordinate/context harness, " +
        "transactional TGMO direction/movement composition, " +
        "strict provenance/dependency/parser/input mutations, " +
        "$RomMutationCount ROM mutations, live wiring intentionally 0")
    $global:LASTEXITCODE = 0
} finally {
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
}
