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
    throw "TGAI-3 tests require the exact Tecmo NBA Basketball Rev1 ROM."
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
                '^TGAI-3 CPU steering isolated: commands=680 handlers=24 directions=8 tgmo_adapter=1 scene_adapter=1 route_kernel=1 route_live=1 rom_policy=0$') {
            throw "TGAI-3 loader/vector goldens failed.`n$(Get-ShortTail $Output)"
        }
    } elseif ($ExitCode -eq 0 -or
              $Text -notmatch 'Gameplay CPU steering test failed:' -or
              ($ExpectedFailure -and
               $Text -notmatch [regex]::Escape($ExpectedFailure))) {
        throw "Malformed TGAI-3 pack was accepted or failure changed.`n$(Get-ShortTail $Output)"
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
        throw "TGAI-3 inspect vector changed.`n$(Get-ShortTail $Output)"
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
            $Text -notmatch '^TGAI-3 harness ' -or
            $Text -notmatch 'target_policy=native-harness ' -or
            $Text -notmatch
                'quantizer=rom-exact scene_adapter=1 normal_flow=0$') {
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
            $Text -notmatch
                'movement=rom-exact primary=0 secondary=1 scene_adapter=1 normal_flow=0') {
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
    Invoke-SteeringTest $Path $false "TGAI-3"
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
            '^Built strict ROM-derived TGAI-3 CPU steering evidence asset') {
        throw "Direct Rev1 TGAI-3 source gate failed.`n$(Get-ShortTail $SourceOutput)"
    }
    $PackOutput = @(& $Executable --build-assetpack `
        $RomPath $PackPath 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "Rev1 TGAI-3 asset-pack build failed.`n$(Get-ShortTail $PackOutput)"
    }

    $PackBytes = [IO.File]::ReadAllBytes($PackPath)
    $SteeringEntry = Get-AssetPackEntry $PackBytes "gameplay/cpu-steering"
    $MovementEntry = Get-AssetPackEntry $PackBytes "gameplay/movement"
    $SourceMapEntry = Get-AssetPackEntry $PackBytes "system/source-map"
    $Payload = Get-EntryBytes $PackBytes $SteeringEntry
    if ($SteeringEntry.byte_count -ne 8016 -or
        (Get-Fnv1a32 $Payload) -ne "D56EE070") {
        throw "gameplay/cpu-steering size or canonical fingerprint changed."
    }
    Invoke-SteeringTest $PackPath $true
    $Opcode15HarnessText = (@(& $Executable `
        --gameplay-cpu-steering-opcode15-harness $PackPath 2>&1) -join
        [Environment]::NewLine).Trim()
    if ($LASTEXITCODE -ne 0) {
        throw "Opcode-15 raw harness failed.`n$Opcode15HarnessText"
    }
    $Opcode15Harness = $Opcode15HarnessText | ConvertFrom-Json
    if ($Opcode15Harness.schema -ne
            'tecmo.gameplay-cpu-steering/opcode15-raw-harness/TGAI-3' -or
        $Opcode15Harness.mode -ne 'harness-only' -or
        (@($Opcode15Harness.canonical_records) -join ',') -ne '0037,004B' -or
        $Opcode15Harness.branches.gate_noop -ne 'gate-noop' -or
        $Opcode15Harness.branches.primary_bit2_return -ne 'primary-bit2-return-9179' -or
        $Opcode15Harness.branches.primary_swap -ne 'deferred-primary-swap' -or
        $Opcode15Harness.branches.qualified_bit3_return -ne 'qualified-bit3-return-9179' -or
        $Opcode15Harness.branches.selected_defender -ne 'defender-replaced' -or
        ![bool]$Opcode15Harness.selected_defender.committed -or
        (@($Opcode15Harness.selected_defender.raw_0308) -join ',') -ne '4,4' -or
        (@($Opcode15Harness.selected_defender.raw_0309) -join ',') -ne '9,6' -or
        (@($Opcode15Harness.selected_defender.old_defender_stream) -join ',') -ne '4660,90' -or
        (@($Opcode15Harness.selected_defender.old_defender_state) -join ',') -ne '8,4' -or
        (@($Opcode15Harness.selected_defender.new_actor_state) -join ',') -ne '4,7' -or
        (@($Opcode15Harness.selected_defender.raw_059E) -join ',') -ne '5,6' -or
        (@($Opcode15Harness.selected_defender.selection_06D5) -join ',') -ne '6,9' -or
        (@($Opcode15Harness.selected_defender.selection_06D6) -join ',') -ne '2,9' -or
        $Opcode15Harness.selected_defender.c711.selector -ne 4 -or
        $Opcode15Harness.selected_defender.c711.x -ne 6 -or
        $Opcode15Harness.selected_defender.c711.y -ne 6 -or
        ![bool]$Opcode15Harness.selected_defender.c711.observed_unexecuted) {
        throw "Opcode-15 raw harness proof changed or broadened."
    }

    $ListOutput = @(& $Executable --assetpack-list $PackPath 2>&1)
    if ($LASTEXITCODE -ne 0 -or
        @($ListOutput | Where-Object {
            $_ -match '^gameplay/cpu-steering\s' -and
            $_ -match 'bank=6' -and $_ -match 'cpu=0x8B90' -and
            $_ -match 'bytes=8016'
        }).Count -ne 1) {
        throw "Asset-pack listing omitted the exact TGAI-3 entry.`n$(Get-ShortTail $ListOutput)"
    }

    $ExpectedHandlers = @(
        37088,37707,37504,36958,36858,36754,36653,36626,
        36567,36805,36048,35904,36431,37157,37190,37234,
        36997,35866,35866,35866,36914,35830,35809,36722
    )
    $ExpectedSpans = @(
        @{ bank=6; fixed=$false; start=0x81F7; size=221;  hash="23BB7271"; payload=784 },
        @{ bank=6; fixed=$false; start=0x87AE; size=258;  hash="F866B06C"; payload=1008 },
        @{ bank=6; fixed=$false; start=0x88DA; size=444;  hash="9616E586"; payload=1280 },
        @{ bank=6; fixed=$false; start=0x8A96; size=94;   hash="939C6882"; payload=1728 },
        @{ bank=6; fixed=$false; start=0x8AF4; size=156;  hash="C2E05331"; payload=1824 },
        @{ bank=6; fixed=$false; start=0x8B90; size=81;   hash="9AD2BA91"; payload=1984 },
        @{ bank=6; fixed=$false; start=0x8BE1; size=1623; hash="344298FE"; payload=2080 },
        @{ bank=6; fixed=$false; start=0x9280; size=170;  hash="C82E6853"; payload=3712 },
        @{ bank=6; fixed=$false; start=0x938B; size=662;  hash="47818A62"; payload=3888 },
        @{ bank=7; fixed=$true;  start=0xC006; size=3;    hash="14B2472E"; payload=4560 },
        @{ bank=7; fixed=$true;  start=0xCBE0; size=23;   hash="41C5B5C8"; payload=4576 },
        @{ bank=4; fixed=$false; start=0x9F2E; size=3400; hash="71331A96"; payload=4608 }
    )
    $LifecycleAnchorSpans = @(
        @{ label="Bank04 AC76-ACF0"; bank=4; fixed=$false; start=0xAC76; size=0x7B },
        @{ label="Bank04 ACD9-ACE3"; bank=4; fixed=$false; start=0xACD9; size=0x0B },
        @{ label="Bank04 ADD6-ADDF"; bank=4; fixed=$false; start=0xADD6; size=0x0A },
        @{ label="Bank05 96B6-9708"; bank=5; fixed=$false; start=0x96B6; size=0x53 },
        @{ label="Bank06 8374-84B6"; bank=6; fixed=$false; start=0x8374; size=0x143 },
        @{ label="Bank06 B081-B365"; bank=6; fixed=$false; start=0xB081; size=0x2E5 },
        @{ label="Bank06 9BD8-9C6E route divider"; bank=6; fixed=$false; start=0x9BD8; size=151 },
        @{ label="Bank05 9709-970A route table"; bank=5; fixed=$false; start=0x9709; size=2 }
    )
    # Regulation-entry control flow is a runner-only semantic anchor rather
    # than copied TGAI payload. Pin the exact canonical bytes independently:
    # fixed $E740 is both the JMP operand at $E73F and the table read by
    # $E723, so its raw overlap must remain inside the fixed span.
    $RegulationEntryAnchorSpans = @(
        @{ label="Bank05 A2A4-A2D5 P1 selector source"; bank=5; fixed=$false; start=0xA2A4; size=0x32; hash="ED9BAB3B" },
        @{ label="Fixed E51B-E548 P1 selector projection"; bank=7; fixed=$true; start=0xE51B; size=0x2E; hash="145DE16E" },
        @{ label="Fixed E5E9-E61D overtime recurrence"; bank=7; fixed=$true; start=0xE5E9; size=0x35; hash="5B32743D" },
        @{ label="Fixed E73F-E747 direct index table"; bank=7; fixed=$true; start=0xE73F; size=0x09; hash="6447E4ED" },
        @{ label="Fixed E71B-E756 entry/E740 overlap"; bank=7; fixed=$true; start=0xE71B; size=0x3C; hash="63D4F5A3" },
        @{ label="Bank05 8F97-8FAC equality"; bank=5; fixed=$false; start=0x8F97; size=0x16; hash="62809A8D" },
        @{ label="Bank05 8FAD-8FE7 mismatch"; bank=5; fixed=$false; start=0x8FAD; size=0x3B; hash="7C94E5EA" },
        @{ label="Bank05 8FE8-902D selected reset"; bank=5; fixed=$false; start=0x8FE8; size=0x46; hash="FFA12025" },
        @{ label="Bank05 BFA8-BFC8 all-actor reset"; bank=5; fixed=$false; start=0xBFA8; size=0x21; hash="7AD3EC16" }
    )
    $AutoPassAnchorSpans = @(
        @{ label="Bank05 8FAD-9041 score trigger"; bank=5; fixed=$false; start=0x8FAD; size=0x95; hash="8F50F4E2" },
        @{ label="Bank06 805B-8089 state dispatch"; bank=6; fixed=$false; start=0x805B; size=0x2F; hash="B58B86CB" },
        @{ label="Bank06 8661-8727 selector"; bank=6; fixed=$false; start=0x8661; size=0xC7; hash="5CF7B7F5" },
        @{ label="Bank06 8728-8773 refresh"; bank=6; fixed=$false; start=0x8728; size=0x4C; hash="4DD31C29" },
        @{ label="Bank06 88B0-88D9 old-primary reset"; bank=6; fixed=$false; start=0x88B0; size=0x2A; hash="AD834719" }
    )
    # The first entry below is a separately copied raw helper. The remaining
    # handler/tail anchors overlap the retained command-handler source span;
    # they are semantic anchors, not additional copied source entries.
    $Opcode15AnchorSpans = @(
        @{ label="Bank06 88B0-88D9 helper"; bank=6; fixed=$false; start=0x88B0; size=0x2A },
        @{ label="Bank06 8B90-8BE0 dispatcher"; bank=6; fixed=$false; start=0x8B90; size=0x51 },
        @{ label="Bank06 9146-9216 handler"; bank=6; fixed=$false; start=0x9146; size=0xD1 },
        @{ label="Bank06 9208-9216 canonical tail"; bank=6; fixed=$false; start=0x9208; size=0x0F },
        @{ label="Bank04 9F65-9F69 canonical record A"; bank=4; fixed=$false; start=0x9F65; size=5 },
        @{ label="Bank04 9F79-9F7D canonical record B"; bank=4; fixed=$false; start=0x9F79; size=5 }
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
            $Map.schema -eq "tecmo.gameplay-cpu-steering/TGAI-3" -and
            $Map.size -eq 8016 -and
            $Map.fingerprint_fnv1a32 -eq "D56EE070" -and
            $Map.revision_sha256_identity -eq $ExpectedRomSha256 -and
            $Map.revision_full_rom_fnv1a32 -eq "0650F5B0" -and
            [bool]$Map.revision_full_rom_sha256_verified -and
            [bool]$Map.revision_full_rom_fnv1a32_verified -and
            [bool]$Map.revision_source_fingerprints_verified -and
            $Map.dependency.entry -eq "gameplay/movement" -and
            $Map.dependency.size -eq 1664 -and
            $Map.dependency.fingerprint_fnv1a32 -eq "6C82A137" -and
            [bool]$Map.dependency.same_pack_required -and
            @($Map.source_spans).Count -eq 12 -and
            $Map.opcode15_source_contract.scope -eq
                'harness-only; LIVE opcode 15 remains deferred' -and
            $Map.opcode15_source_contract.dispatch.bank -eq 6 -and
            $Map.opcode15_source_contract.dispatch.address -eq '$8B90-$8BE0' -and
            $Map.opcode15_source_contract.dispatch.handler -eq '$9172' -and
            @($Map.opcode15_source_contract.canonical_records).Count -eq 2 -and
            $Map.opcode15_source_contract.canonical_records[0].address -eq '$9F65-$9F69' -and
            $Map.opcode15_source_contract.canonical_records[0].stream_offset -eq '$0037' -and
            $Map.opcode15_source_contract.canonical_records[1].address -eq '$9F79-$9F7D' -and
            $Map.opcode15_source_contract.canonical_records[1].stream_offset -eq '$004B' -and
            [bool]$Map.opcode15_source_contract.parent_source_span.copied_source -and
            @($Map.opcode15_source_contract.semantic_anchors).Count -eq 3 -and
            $Map.opcode15_source_contract.semantic_anchors[0].address -eq '$9146-$9216' -and
            $Map.opcode15_source_contract.semantic_anchors[0].fnv1a32 -eq 'FA3E6C5E' -and
            $Map.opcode15_source_contract.semantic_anchors[1].address -eq '$9208-$9216' -and
            [bool]$Map.opcode15_source_contract.semantic_anchors[1].overlaps_parent_source -and
            $Map.opcode15_source_contract.semantic_anchors[1].fnv1a32 -eq '839F9D07' -and
            $Map.opcode15_source_contract.semantic_anchors[2].address -eq '$88B0-$88D9' -and
            [bool]$Map.opcode15_source_contract.semantic_anchors[2].raw_contract_payload -and
            $Map.opcode15_source_contract.lifted_source_discrepancy.authority -eq
                'canonical Rev1 ROM' -and
            $Map.opcode15_source_contract.lifted_source_discrepancy.lifted_listing_omits -eq '$9208-$9211' -and
            (@($Map.opcode15_source_contract.exact_no_advance_returns) -join '|') -eq
                '$9185 D0 F2 -> $9179 RTS|$91C6 D0 B1 -> $9179 RTS' -and
            (@($Map.opcode15_source_contract.classified_deferred) -join '|') -eq
                '$9187 primary swap|missing raw owner|invalid $0463 direction' -and
            $Map.opcode15_source_contract.c711.selector -eq 4 -and
            [bool]$Map.opcode15_source_contract.c711.observed_unexecuted -and
            $Map.opcode15_source_contract.conditional_06d5.gate -eq
                '$91F1-$91F5: CPX $06D5; BNE' -and
            $Map.opcode15_source_contract.conditional_06d5.store -eq
                '$91F6-$91F8: STY $06D5' -and
            $Map.opcode15_source_contract.conditional_06d5.'when' -eq
                'new X == $06D5' -and
            $Map.opcode15_source_contract.conditional_06d5.'then' -eq
                '$06D5=old Y' -and
            $Map.opcode15_source_contract.conditional_06d5.'otherwise' -eq
                'preserve $06D5' -and
            $Map.opcode15_source_contract.live_missing_raw_reason -match
                'deferred_missing_raw_0499' -and
            $Map.opcode15_source_contract.natural_fceux_capture -match
                'synthetic.*not a natural \$91C8 capture' -and
            (@($Map.opcode15_source_contract.required_memory_watch) -join '|') -match
                '\$0499 slot 10.*\$059E' -and
            $Map.native_contract.actor_slots -eq 10 -and
            $Map.native_contract.script_state -eq 4 -and
            $Map.native_contract.command_transport.record_bytes -eq 5 -and
            $Map.native_contract.command_transport.record_count -eq 680 -and
            $Map.native_contract.command_transport.code_resumes_cpu -eq '$AC76' -and
            $Map.native_contract.opcode_dispatch.count -eq 24 -and
            (@($Map.native_contract.opcode_dispatch.handler_cpu) -join ',') -eq
                ($ExpectedHandlers -join ',') -and
            $Map.native_contract.opcode4_ball_target.caller_path -match
                '\$81F7-\$82D3.*\$8B90-\$8BE0.*\$8FFA' -and
            $Map.native_contract.opcode4_ball_target.handler -eq
                'canonical Rev1 Bank06 $8FFA-$9031; $8FFA loads C8 and the following handler begins at $9032' -and
            $Map.native_contract.opcode4_ball_target.corpus_count -eq 2 -and
            (@($Map.native_contract.opcode4_ball_target.record_offsets) -join ',') -eq
                '0,365' -and
            $Map.native_contract.opcode4_ball_target.object_slot -eq 10 -and
            $Map.native_contract.opcode4_ball_target.object_kind -match
                'ball object; never an actor stream' -and
            $Map.native_contract.opcode4_ball_target.x_delta -match
                '16-bit borrow' -and
            $Map.native_contract.opcode4_ball_target.depth_delta -match
                'sign-extended' -and
            $Map.native_contract.opcode4_ball_target.zero_vector -match
                'skips \$88DA' -and
            $Map.native_contract.opcode4_ball_target.c_contract.input -eq
                'TecmoGameplayCpuSteeringPlayInput.ball_position' -and
            $Map.native_contract.opcode4_ball_target.c_contract.state -eq
                'TecmoGameplayCpuSteeringPlayState.target_object' -and
            $Map.native_contract.opcode4_ball_target.c_contract.production_adapter -match
                'captures the current Q8 ball snapshot.*launches LIVE state 5' -and
            $Map.native_contract.planar_route_kernel.scope -match
                'exact planar arithmetic subset' -and
            (@($Map.native_contract.planar_route_kernel.launch_sources) -join ',') -eq
                '$88DA-$8A95,$8A96-$8AF3' -and
            $Map.native_contract.planar_route_kernel.step_source -eq '$8AF4-$8B8F' -and
            $Map.native_contract.planar_route_kernel.division_anchor.address -eq '$9BD8-$9C6E' -and
            $Map.native_contract.planar_route_kernel.division_anchor.fnv1a32 -eq '74DD2AC6' -and
            [bool]$Map.native_contract.planar_route_kernel.division_anchor.functional -and
            [bool]$Map.native_contract.planar_route_kernel.division_anchor.mutation_rejected -and
            $Map.native_contract.planar_route_kernel.motion -match 'no TGMO/fixed clamp' -and
            $Map.native_contract.planar_route_kernel.capture -match 'frozen.*no dynamic chase' -and
            [bool]$Map.native_contract.planar_route_kernel.production_bound -and
            $Map.native_contract.planar_route_kernel.production_profile -match
                'exact.*native approximation' -and
            $Map.native_contract.direction_quantizer.cpu -eq
                '$92D4-$92DD; $92FE -> $88DA-$899D' -and
            $Map.native_contract.direction_quantizer.dominant_axis_ratio -match
                '16-bit doubling wrap retained' -and
            (@($Map.native_contract.direction_quantizer.octant_map) -join ',') -eq
                '3,6,4,7,0,1,2,5' -and
            [bool]$Map.native_contract.direction_quantizer.zero_vector_keeps_prior_direction -and
            [bool]$Map.native_contract.live_wired -and
            [bool]$Map.native_contract.transactional -and
            $Map.evidence_limits.next_integration -match
                'pose/action side effects' -and
            (@($Map.evidence_limits.not_claimed) -join '|') -match
                'candidate scan \$B081-\$B32E' -and
            [bool]$Map.live_scene_adapter.enabled -and
            ![bool]$Map.live_scene_adapter.target_policy_rom_exact -and
            $Map.live_scene_adapter.holder_target -eq
                'orientation-aware 48/48/40 hoop approach' -and
            $Map.live_scene_adapter.other_target -eq
                'scene-owned explicit native coordinate' -and
            (@($Map.live_scene_adapter.offense_formation_orientation_0 |
                ForEach-Object { @($_) -join ',' }) -join '|') -eq
                '256,148|288,112|288,184|352,96|352,200' -and
            $Map.live_scene_adapter.offense_formation_orientation_1 -eq
                'X mirrored as 767-X' -and
            (@($Map.live_scene_adapter.defender_goal_side_x_by_orientation) -join ',') -eq
                '-32,32' -and
            (@($Map.live_scene_adapter.defender_depth_split) -join ',') -eq
                '0,-10,10,-14,14' -and
            $Map.live_scene_adapter.defender_out_of_bounds_fallback -eq
                'equal 32-pixel offset toward court side before final validation' -and
            $Map.live_scene_adapter.fixed_opposing_link_use -match
                'not an implicit target coordinate' -and
            [bool]$Map.live_scene_adapter.immutable_ten_actor_snapshot -and
            [bool]$Map.live_scene_adapter.transactional_actor_commit -and
            $Map.live_scene_adapter.rom_command_offset -eq
                'explicit no-command sentinel' -and
            ![bool]$Map.live_scene_adapter.rom_command_advance_owned -and
            [bool]$Map.live_scene_adapter.direction_quantizer_rom_exact -and
            [bool]$Map.live_scene_adapter.movement_kernel_rom_exact -and
            ![bool]$Map.live_scene_adapter.shot_choice_and_cadence_rom_exact -and
            [bool]$Map.developer_harness.deterministic -and
            [bool]$Map.developer_harness.cli_only -and
            $Map.developer_harness.coordinate_slots -eq 10 -and
            $Map.developer_harness.coordinate_space -eq
                'TGCT-1 canonical X=0..767 Y=0..239' -and
            @($Map.developer_harness.inputs).Count -eq 8 -and
            [bool]$Map.developer_harness.possession_holder_coherent -and
            [bool]$Map.developer_harness.matchup_assignment_caller_owned -and
            ![bool]$Map.developer_harness.target_policy.rom_exact -and
            $Map.developer_harness.target_policy.holder -eq
                'native scene-style hoop approach' -and
            $Map.developer_harness.target_policy.other_actor_default -eq
                'explicit linked/matchup actor coordinate' -and
            $Map.developer_harness.target_policy.explicit_override -match
                'live scene uses this for non-holders' -and
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
            [bool]$Map.developer_harness.movement_adapter.primary_holder_path -and
            [bool]$Map.developer_harness.movement_adapter.secondary_nonholder_path -and
            [bool]$Map.developer_harness.movement_adapter.role_coherent -and
            $Map.developer_harness.movement_adapter.zero_vector_input -eq
                'native neutral policy; TGAI no-write remains exact' -and
            [bool]$Map.developer_harness.movement_adapter.selected_actor_coordinate_reconciled_each_step -and
            [bool]$Map.developer_harness.movement_adapter.live_wired -and
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
        throw "TGAI-3 source-map provenance is incomplete or malformed."
    }

    $First = Invoke-Inspect '0' '20' '10' `
        'offset=\$0000 cpu=\$9F2E opcode=4 args=0A,00,00,00 handler=\$8FFA kind=actor-target'
    if ($First -notmatch
        'delta=\(20,10\) direction=0 name=right normal_flow=0') {
        throw "Inclusive 2:1 horizontal direction vector changed.`n$First"
    }
    $Diagonal = Invoke-Inspect '0' '19' '10' `
        'delta=\(19,10\) direction=3 name=down-right normal_flow=0'
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
        'delta=\(-32768,-32768\) direction=1 name=left normal_flow=0'

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
            '^TGAI-3 harness actor=5 team=1 possession=0 orientation=0 holder=0 matchup=0 difficulty=2 snapshot=15AEBE1B normal_flow=0' -or
        $Linked -notmatch
            'court=0:\(384,148\);1:\(420,120\);2:\(440,180\);3:\(360,100\);4:\(400,200\);5:\(500,160\);6:\(520,110\);7:\(540,190\);8:\(460,90\);9:\(480,210\)' -or
        $Linked -notmatch
            'target=linked-actor target_actor=0 from=\(500,160\) to=\(384,148\) delta=\(-116,-12\) direction=1 name=left write=1') {
        throw "Linked-actor harness golden changed.`n$Linked"
    }

    $LeftHoop = Invoke-Harness '0' '0' '0' '0' '5' '0' `
        $CourtCoordinates
    if ($LeftHoop -notmatch
            'snapshot=57B29369 normal_flow=0' -or
        $LeftHoop -notmatch
            'target=hoop-approach target_actor=none from=\(384,148\) to=\(208,148\) delta=\(-176,0\) direction=1 name=left write=1') {
        throw "Left-hoop/easy harness golden changed.`n$LeftHoop"
    }
    $RightHoop = Invoke-Harness '0' '0' '1' '0' '5' '2' `
        $CourtCoordinates
    if ($RightHoop -notmatch
            'snapshot=34F66F76 normal_flow=0' -or
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
        @{ id="opcode15-contract-descriptor"; offset=272 },
        @{ id="opcode15-contract-raw-helper"; offset=288 },
        @{ id="source-record"; offset=336 },
        @{ id="actor-source"; offset=656 },
        @{ id="actor-padding"; offset=877 },
        @{ id="reference-source"; offset=880 },
        @{ id="target-direction-source"; offset=1152 },
        @{ id="dispatch-source"; offset=1600 },
        @{ id="dispatch-padding"; offset=1681 },
        @{ id="handlers-source"; offset=1696 },
        @{ id="opcode15-handler-tail"; offset=(1696 + (0x9208 - 0x8BE1)) },
        @{ id="handlers-padding"; offset=3319 },
        @{ id="target-apply-source"; offset=3328 },
        @{ id="formation-source"; offset=3504 },
        @{ id="trampoline-source"; offset=4176 },
        @{ id="reader-source"; offset=4192 },
        @{ id="opcode15-record-a"; offset=(4224 + 0x37) },
        @{ id="opcode15-record-b"; offset=(4224 + 0x4B) },
        @{ id="command-opcode"; offset=4224 },
        @{ id="last-command"; offset=7619 },
        @{ id="trailing-padding"; offset=7631 }
    )) {
        Write-PayloadMutationAndReject $PackBytes $SteeringEntry `
            $Mutation.id $Mutation.offset
    }

    foreach ($Case in @(
        @{ id="undersized-steering"; entry=$SteeringEntry; size=7631;
           status="entry missing or wrong-sized" },
        @{ id="oversized-steering"; entry=$SteeringEntry; size=7633;
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
    foreach ($Span in $RegulationEntryAnchorSpans) {
        $CpuBase = if ([bool]$Span.fixed) { 0xC000 } else { 0x8000 }
        $Offset = $Prg + $Span.bank * 0x4000 + ($Span.start - $CpuBase)
        $Raw = New-Object byte[] ([int]$Span.size)
        [Array]::Copy($RomBytes, $Offset, $Raw, 0, [int]$Span.size)
        if ((Get-Fnv1a32 $Raw) -ne $Span.hash) {
            throw ("Canonical regulation-entry anchor changed at " +
                "$($Span.label).")
        }
    }
    foreach ($Span in $AutoPassAnchorSpans) {
        $CpuBase = if ([bool]$Span.fixed) { 0xC000 } else { 0x8000 }
        $Offset = $Prg + $Span.bank * 0x4000 + ($Span.start - $CpuBase)
        $Raw = New-Object byte[] ([int]$Span.size)
        [Array]::Copy($RomBytes, $Offset, $Raw, 0, [int]$Span.size)
        if ((Get-Fnv1a32 $Raw) -ne $Span.hash) {
            throw "Canonical auto-pass anchor changed at $($Span.label)."
        }
    }
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
                'TGAI-3 import requires the exact Rev1 ROM fingerprint') {
            throw ("Rev1 source mutation at Bank$($Span.bank) " +
                "$($Span.start) was accepted.`n$(Get-ShortTail $Output)")
        }
        ++$RomMutationCount
    }
    $LifecycleRomMutationCount = 0
    foreach ($Span in $LifecycleAnchorSpans) {
        $CpuBase = if ([bool]$Span.fixed) { 0xC000 } else { 0x8000 }
        $Offset = $Prg + $Span.bank * 0x4000 + ($Span.start - $CpuBase)
        $MutatedRom = Join-Path $Scratch `
            ("rom-lifecycle-{0}-{1:X4}.nes" -f $Span.bank, $Span.start)
        $Bytes = [byte[]]$RomBytes.Clone()
        $Bytes[$Offset] = $Bytes[$Offset] -bxor 1
        [IO.File]::WriteAllBytes($MutatedRom, $Bytes)
        $Output = @(& $Executable --gameplay-cpu-steering-source-test `
            $MutatedRom 2>&1)
        if ($LASTEXITCODE -eq 0 -or
            ($Output -join [Environment]::NewLine) -notmatch
                'TGAI-3 import requires the exact Rev1 ROM fingerprint') {
            throw ("Rev1 lifecycle-anchor mutation at $($Span.label) was " +
                "accepted.`n$(Get-ShortTail $Output)")
        }
        ++$RomMutationCount
        ++$LifecycleRomMutationCount
    }

    $RegulationEntryRomMutationCount = 0
    foreach ($Span in $RegulationEntryAnchorSpans) {
        $CpuBase = if ([bool]$Span.fixed) { 0xC000 } else { 0x8000 }
        $Offset = $Prg + $Span.bank * 0x4000 + ($Span.start - $CpuBase)
        $MutatedRom = Join-Path $Scratch `
            ("rom-regulation-entry-{0}-{1:X4}.nes" -f `
                $Span.bank, $Span.start)
        $Bytes = [byte[]]$RomBytes.Clone()
        $Bytes[$Offset] = $Bytes[$Offset] -bxor 1
        [IO.File]::WriteAllBytes($MutatedRom, $Bytes)
        $Output = @(& $Executable --gameplay-cpu-steering-source-test `
            $MutatedRom 2>&1)
        if ($LASTEXITCODE -eq 0 -or
            ($Output -join [Environment]::NewLine) -notmatch
                'TGAI-3 import requires the exact Rev1 ROM fingerprint') {
            throw ("Rev1 regulation-entry anchor mutation at " +
                "$($Span.label) was accepted.`n$(Get-ShortTail $Output)")
        }
        ++$RomMutationCount
        ++$RegulationEntryRomMutationCount
    }

    $Opcode15RomMutationCount = 0
    foreach ($Span in $Opcode15AnchorSpans) {
        $CpuBase = if ([bool]$Span.fixed) { 0xC000 } else { 0x8000 }
        $Offset = $Prg + $Span.bank * 0x4000 + ($Span.start - $CpuBase)
        $MutatedRom = Join-Path $Scratch `
            ("rom-opcode15-{0}-{1:X4}.nes" -f $Span.bank, $Span.start)
        $Bytes = [byte[]]$RomBytes.Clone()
        $Bytes[$Offset] = $Bytes[$Offset] -bxor 1
        [IO.File]::WriteAllBytes($MutatedRom, $Bytes)
        $Output = @(& $Executable --gameplay-cpu-steering-source-test `
            $MutatedRom 2>&1)
        if ($LASTEXITCODE -eq 0 -or
            ($Output -join [Environment]::NewLine) -notmatch
                'TGAI-3 import requires the exact Rev1 ROM fingerprint') {
            throw ("Rev1 opcode-15 source/anchor mutation at $($Span.label) " +
                "was accepted.`n$(Get-ShortTail $Output)")
        }
        ++$RomMutationCount
        ++$Opcode15RomMutationCount
    }

    $AutoPassRomMutationCount = 0
    foreach ($Span in $AutoPassAnchorSpans) {
        $CpuBase = if ([bool]$Span.fixed) { 0xC000 } else { 0x8000 }
        $Offset = $Prg + $Span.bank * 0x4000 + ($Span.start - $CpuBase)
        $MutatedRom = Join-Path $Scratch `
            ("rom-auto-pass-{0}-{1:X4}.nes" -f $Span.bank, $Span.start)
        $Bytes = [byte[]]$RomBytes.Clone()
        $Bytes[$Offset] = $Bytes[$Offset] -bxor 1
        [IO.File]::WriteAllBytes($MutatedRom, $Bytes)
        $Output = @(& $Executable --gameplay-cpu-steering-source-test `
            $MutatedRom 2>&1)
        if ($LASTEXITCODE -eq 0 -or
            ($Output -join [Environment]::NewLine) -notmatch
                'TGAI-3 import requires the exact Rev1 ROM fingerprint') {
            throw ("Rev1 auto-pass anchor mutation at $($Span.label) was " +
                "accepted.`n$(Get-ShortTail $Output)")
        }
        ++$RomMutationCount
        ++$AutoPassRomMutationCount
    }

    Write-Host ("TGAI-3 focused tests passed: exact Rev1 importer and twelve " +
        "source spans plus eight lifecycle anchor/table spans, nine exact " +
        "regulation-entry spans, five auto-pass spans, and six opcode-15 " +
        "source/semantic-anchor spans, 680 aligned " +
        "commands, 24 handlers, eight exact " +
        "direction codes, deterministic ten-coordinate/context harness, " +
        "transactional TGMO direction/movement composition, " +
        "strict provenance/dependency/parser/input mutations, " +
        "$RomMutationCount ROM mutations ($LifecycleRomMutationCount lifecycle " +
        "anchor/table; $RegulationEntryRomMutationCount regulation entry; " +
        "$Opcode15RomMutationCount opcode-15; $AutoPassRomMutationCount " +
        "auto-pass), bounded live scene " +
        "adapter enabled")
    $global:LASTEXITCODE = 0
} finally {
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
}
