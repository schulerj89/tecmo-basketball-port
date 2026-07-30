param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [string]$DecompRoot,
    [switch]$Build
)

$ErrorActionPreference = "Stop"

if (!$ProjectRoot) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$BuildDir = Join-Path $ProjectRoot "build"
$TestDir = [IO.Path]::GetFullPath((Join-Path $BuildDir "frontend_audio_test"))
$BuildPrefix = [IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/') +
    [IO.Path]::DirectorySeparatorChar
if (!$TestDir.StartsWith($BuildPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Frontend-audio test directory escaped build/."
}
if (!$RomPath) { $RomPath = $env:TECMO_ROM_PATH }
if (!$RomPath -or !(Test-Path -LiteralPath $RomPath)) {
    throw "Pass -RomPath or set TECMO_ROM_PATH to the local Rev1 iNES ROM."
}
$RomPath = (Resolve-Path -LiteralPath $RomPath).Path
if ((Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash -ne
    "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4") {
    throw "Frontend audio requires the supported Rev1 ROM fingerprint."
}
if ($DecompRoot) {
    $DecompRoot = (Resolve-Path -LiteralPath $DecompRoot).Path
}

$ExePath = Join-Path $BuildDir "tecmo_port.exe"
$PackPath = Join-Path $TestDir "frontend-audio.assetpack"
$Expected = "TFSX-1 frontend audio ok title=09718C9D menu=100B5218"
$RomBytes = [IO.File]::ReadAllBytes($RomPath)
$Header = $RomBytes[0..15]
$Trainer = if (($Header[6] -band 4) -ne 0) { 512 } else { 0 }
$Prg = 16 + $Trainer
$Fixed = $Prg + ([int]$Header[4] - 1) * 0x4000
$PreviousPack = $env:TECMO_ASSETPACK
$PreviousSkip = $env:TECMO_SKIP_SHORTCUT

function Get-ShortTail([object[]]$Lines) {
    return (@($Lines | Select-Object -Last 8) -join [Environment]::NewLine)
}

function Get-PackEntry([byte[]]$Bytes, [string]$Id) {
    if ($Bytes.Length -lt 40 -or
        [Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -ne "TAP1" -or
        [BitConverter]::ToUInt32($Bytes, 4) -ne 1 -or
        [BitConverter]::ToUInt32($Bytes, 8) -ne 40 -or
        [BitConverter]::ToUInt32($Bytes, 12) -ne 128) {
        throw "Asset pack header is not TAP1 v1."
    }
    $Count = [BitConverter]::ToUInt32($Bytes, 16)
    $Directory = [BitConverter]::ToUInt64($Bytes, 20)
    if ($Directory -gt [uint64]$Bytes.Length -or
        [uint64]$Count * 128 -gt [uint64]$Bytes.Length - $Directory) {
        throw "Asset-pack directory is out of bounds."
    }
    for ($Index = 0; $Index -lt $Count; ++$Index) {
        $At = [int]$Directory + $Index * 128
        $End = [Array]::IndexOf($Bytes, [byte]0, $At, 64)
        if ($End -lt 0) { $End = $At + 64 }
        $EntryId = [Text.Encoding]::ASCII.GetString(
            $Bytes, $At, $End - $At)
        if ($EntryId -ne $Id) { continue }
        $Offset = [BitConverter]::ToUInt64($Bytes, $At + 84)
        $Size = [BitConverter]::ToUInt64($Bytes, $At + 92)
        if ($Offset -gt [uint64]$Bytes.Length -or
            $Size -gt [uint64]$Bytes.Length - $Offset) {
            throw "Asset-pack entry '$Id' is out of bounds."
        }
        return [pscustomobject]@{
            directory_offset = $At
            pack_offset = $Offset
            byte_count = $Size
        }
    }
    throw "Asset-pack entry '$Id' was not found."
}

function Get-EntryBytes([byte[]]$Bytes, $Entry) {
    $Result = New-Object byte[] ([int]$Entry.byte_count)
    [Array]::Copy($Bytes, [int64]$Entry.pack_offset,
                  $Result, 0, [int64]$Entry.byte_count)
    return $Result
}

function Assert-FrontendSourceMap([byte[]]$Bytes) {
    $SourceMapEntry = Get-PackEntry $Bytes "system/source-map"
    $MapText = [Text.Encoding]::UTF8.GetString(
        (Get-EntryBytes $Bytes $SourceMapEntry))
    $Map = $MapText | ConvertFrom-Json
    $FrontendMap = @(
        $Map.logical_entries | Where-Object id -eq "audio/frontend-sfx")
    $ExpectedSources = @(
        [pscustomobject]@{ role="sfx-directory"; entry="prg/bank04"; offset=$Prg + 4 * 0x4000 + 0x0AA4; bank=4; cpu=0x8AA4; size=32; hash="6283F255" },
        [pscustomobject]@{ role="accepted-menu-a-release-sfx-8"; entry="prg/bank04"; offset=$Prg + 4 * 0x4000 + 0x0BF7; bank=4; cpu=0x8BF7; size=51; hash="AC9D4C1F" },
        [pscustomobject]@{ role="title-confirm-sfx-10"; entry="prg/bank04"; offset=$Prg + 4 * 0x4000 + 0x0B97; bank=4; cpu=0x8B97; size=96; hash="963DC35E" },
        [pscustomobject]@{ role="title-setup"; entry="prg/bank03"; offset=$Prg + 3 * 0x4000 + 0x0056; bank=3; cpu=0x8056; size=32; hash="4A97C61D" },
        [pscustomobject]@{ role="title-confirm-input-and-queue"; entry="prg/bank03"; offset=$Prg + 3 * 0x4000 + 0x0076; bank=3; cpu=0x8076; size=27; hash="0C902C97" },
        [pscustomobject]@{ role="title-setup-transition-bridge"; entry="prg/fixed"; offset=$Fixed + 0x0003; bank=$null; cpu=0xC003; size=3; hash="0F4103F2" },
        [pscustomobject]@{ role="title-setup-three-yield-flow"; entry="prg/fixed"; offset=$Fixed + 0x192E; bank=$null; cpu=0xD92E; size=119; hash="105B38A7" },
        [pscustomobject]@{ role="title-setup-zero-state-two-yield-flow"; entry="prg/fixed"; offset=$Fixed + 0x1B25; bank=$null; cpu=0xDB25; size=99; hash="5D98AB7A" },
        [pscustomobject]@{ role="task-frame-yield-helper"; entry="prg/fixed"; offset=$Fixed + 0x23FA; bank=$null; cpu=0xE3FA; size=32; hash="B8BA175B" },
        [pscustomobject]@{ role="title-stop-bridge"; entry="prg/fixed"; offset=$Fixed + 0x0024; bank=$null; cpu=0xC024; size=6; hash="B4100BD2" },
        [pscustomobject]@{ role="title-stop-dispatch"; entry="prg/fixed"; offset=$Fixed + 0x0BAF; bank=$null; cpu=0xCBAF; size=12; hash="AB3677A6" },
        [pscustomobject]@{ role="title-stop-and-wait"; entry="prg/fixed"; offset=$Fixed + 0x2C06; bank=$null; cpu=0xEC06; size=32; hash="F1BCC8E2" },
        [pscustomobject]@{ role="menu-accepted-release-dispatch"; entry="prg/fixed"; offset=$Fixed + 0x1768; bank=$null; cpu=0xD768; size=43; hash="68D40771" },
        [pscustomobject]@{ role="menu-track-transition-flow"; entry="prg/fixed"; offset=$Fixed + 0x2477; bank=$null; cpu=0xE477; size=42; hash="71B4A4A8" },
        [pscustomobject]@{ role="audio-mailboxes"; entry="prg/fixed"; offset=$Fixed + 0x32F2; bank=$null; cpu=0xF2F2; size=8; hash="17DE7030" }
    )
    if ($FrontendMap.Count -ne 1) {
        throw "TFSX-1 source-map entry count is not exactly one."
    }
    $Entry = $FrontendMap[0]
    $Dependencies = @($Entry.runtime_dependencies)
    if ($Entry.kind -ne "native-frontend-sfx" -or
        $Entry.schema -ne "tecmo.frontend-audio/TFSX-1" -or
        $Entry.input_contract -ne "ines-only" -or
        $Dependencies.Count -ne 1 -or
        $Dependencies[0].entry -ne "audio/music" -or
        $Dependencies[0].schema -ne "TMUS-1" -or
        $Dependencies[0].same_pack_required -ne $true -or
        $Entry.native_contract.payload_size -ne 1792 -or
        $Entry.native_contract.payload_fingerprint_fnv1a32 -ne
            "985DC7ED" -or
        (@($Entry.native_contract.effect_ids) -join ",") -ne "8,10" -or
        $Entry.native_contract.instruction_count -ne 87 -or
        $Entry.native_contract.voice_count -ne 3 -or
        (@($Entry.native_contract.channels) -join ",") -ne
            "pulse1,pulse2,triangle,noise" -or
        $Entry.native_contract.title_stop_frame -ne 5 -or
        $Entry.native_contract.title_setup_yield_count -ne 5 -or
        $Entry.native_contract.title_setup_yield_proof_fnv1a32 -ne
            "CA4CA88A" -or
        $Entry.native_contract.title_confirmation_cue_frame -ne 1 -or
        $Entry.native_contract.title_handoff_frame -ne 127 -or
        $Entry.native_contract.title_animation_frames -ne 126 -or
        $Entry.native_contract.menu_music_track -ne 6 -or
        $Entry.native_contract.menu_cue_condition -ne
            "accepted-player1-a-release-only" -or
        $Entry.native_contract.runtime_raw_pointer_or_opcode_dependency -ne
            $false -or
        @($Entry.sources).Count -ne $ExpectedSources.Count) {
        throw "TFSX-1 source-map contract metadata is malformed."
    }
    for ($Index = 0; $Index -lt $ExpectedSources.Count; ++$Index) {
        $Actual = @($Entry.sources)[$Index]
        $ExpectedSource = $ExpectedSources[$Index]
        $HasBank = $Actual.PSObject.Properties.Name -contains "bank"
        if ($Actual.role -ne $ExpectedSource.role -or
            $Actual.source_entry -ne $ExpectedSource.entry -or
            [uint64]$Actual.source_offset -ne
                [uint64]$ExpectedSource.offset -or
            [int]$Actual.cpu_address -ne $ExpectedSource.cpu -or
            [int]$Actual.size -ne $ExpectedSource.size -or
            $Actual.fingerprint_fnv1a32 -ne $ExpectedSource.hash -or
            ($null -eq $ExpectedSource.bank -and $HasBank) -or
            ($null -ne $ExpectedSource.bank -and
             (!$HasBank -or [int]$Actual.bank -ne $ExpectedSource.bank))) {
            throw "TFSX-1 source-map source[$Index] is malformed."
        }
    }
}

function Invoke-FrontendAudio([string]$AssetPack, [bool]$ExpectSuccess) {
    $env:TECMO_ASSETPACK = $AssetPack
    $Output = @(& $ExePath --frontend-audio-test 2>&1)
    $ExitCode = $LASTEXITCODE
    if ($ExpectSuccess) {
        if ($ExitCode -ne 0 -or
            ($Output -join [Environment]::NewLine).Trim() -ne $Expected) {
            throw "Native frontend-audio golden failed.`n$(Get-ShortTail $Output)"
        }
    } elseif ($ExitCode -eq 0 -or
              @($Output | Where-Object { $_ -match "TFSX-1" }).Count -eq 0) {
        throw "Malformed/missing frontend audio was not rejected.`n$(Get-ShortTail $Output)"
    }
}

try {
    $env:TECMO_SKIP_SHORTCUT = "1"
    if ($Build) {
        $Output = @(& (Join-Path $ProjectRoot "build.ps1") 2>&1)
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed.`n$(Get-ShortTail $Output)"
        }
    }
    if (!(Test-Path -LiteralPath $ExePath)) {
        throw "Build output is missing; rerun with -Build."
    }
    if (Test-Path -LiteralPath $TestDir) {
        Remove-Item -LiteralPath $TestDir -Recurse -Force
    }
    [void](New-Item -ItemType Directory -Path $TestDir)
    $Output = @(& $ExePath --build-assetpack $RomPath $PackPath 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "Rev1 frontend-audio pack build failed.`n$(Get-ShortTail $Output)"
    }
    $Pack = [IO.File]::ReadAllBytes($PackPath)
    $Frontend = Get-PackEntry $Pack "audio/frontend-sfx"
    $Music = Get-PackEntry $Pack "audio/music"
    $SourceMap = Get-PackEntry $Pack "system/source-map"
    if ($Frontend.byte_count -ne 1792 -or $Music.byte_count -ne 36784) {
        throw "TFSX-1/TMUS-1 exact sizes are wrong."
    }
    Assert-FrontendSourceMap $Pack
    Invoke-FrontendAudio $PackPath $true

    $SameAlias = Join-Path $TestDir "..\frontend_audio_test\frontend-audio.assetpack"
    $Output = @(
        & $ExePath --frontend-audio-cross-pack-test $PackPath $SameAlias 2>&1)
    if ($LASTEXITCODE -ne 0 -or
        ($Output -join [Environment]::NewLine).Trim() -ne
            "TFSX-1/TMUS-1 canonical-same-pack provenance accepted") {
        throw "Canonical same-pack alias was rejected.`n$(Get-ShortTail $Output)"
    }
    $SecondPackPath = Join-Path $TestDir "second-container.assetpack"
    [IO.File]::Copy($PackPath, $SecondPackPath, $true)
    $Output = @(
        & $ExePath --frontend-audio-cross-pack-test `
            $PackPath $SecondPackPath 2>&1)
    if ($LASTEXITCODE -ne 0 -or
        ($Output -join [Environment]::NewLine).Trim() -ne
            "TFSX-1/TMUS-1 cross-pack provenance rejected") {
        throw "Distinct pack containers were not rejected.`n$(Get-ShortTail $Output)"
    }

    $MalformedMap = [byte[]]$Pack.Clone()
    $MalformedMap[[int]$SourceMap.pack_offset] = [byte][char]'x'
    $MalformedMapRejected = $false
    try {
        Assert-FrontendSourceMap $MalformedMap
    } catch {
        $MalformedMapRejected = $true
    }
    if (!$MalformedMapRejected) {
        throw "Malformed frontend source map escaped the provenance audit."
    }

    foreach ($Spec in @(
        [pscustomobject]@{ id = "payload"; entry = $Frontend; delta = 128 },
        [pscustomobject]@{ id = "header"; entry = $Frontend; delta = 4 },
        [pscustomobject]@{ id = "timing"; entry = $Frontend; delta = 96 },
        [pscustomobject]@{ id = "reserved"; entry = $Frontend; delta = 127 },
        [pscustomobject]@{ id = "music-dependency"; entry = $Music; delta = 128 }
    )) {
        $Mutated = [byte[]]$Pack.Clone()
        $At = [int]$Spec.entry.pack_offset + $Spec.delta
        $Mutated[$At] = $Mutated[$At] -bxor 1
        $Path = Join-Path $TestDir ("{0}.assetpack" -f $Spec.id)
        [IO.File]::WriteAllBytes($Path, $Mutated)
        Invoke-FrontendAudio $Path $false
    }
    foreach ($Spec in @(
        [pscustomobject]@{
            id = "frontend-oversized"; entry = $Frontend; size = [uint64]1793
        },
        [pscustomobject]@{
            id = "music-oversized"; entry = $Music; size = [uint64]36785
        }
    )) {
        $Mutated = [byte[]]$Pack.Clone()
        [BitConverter]::GetBytes($Spec.size).CopyTo(
            $Mutated, [int]$Spec.entry.directory_offset + 92)
        $Path = Join-Path $TestDir ("{0}.assetpack" -f $Spec.id)
        [IO.File]::WriteAllBytes($Path, $Mutated)
        Invoke-FrontendAudio $Path $false
    }
    foreach ($Spec in @(
        [pscustomobject]@{ id = "frontend-missing"; entry = $Frontend },
        [pscustomobject]@{ id = "music-missing"; entry = $Music }
    )) {
        $Mutated = [byte[]]$Pack.Clone()
        $Mutated[[int]$Spec.entry.directory_offset] = [byte][char]'x'
        $Path = Join-Path $TestDir ("{0}.assetpack" -f $Spec.id)
        [IO.File]::WriteAllBytes($Path, $Mutated)
        Invoke-FrontendAudio $Path $false
    }

    $Sources = @(
        [pscustomobject]@{ id = "directory"; offset = $Prg + 4 * 0x4000 + 0x0AA4 },
        [pscustomobject]@{ id = "sfx8"; offset = $Prg + 4 * 0x4000 + (0x8BF7 - 0x8000) },
        [pscustomobject]@{ id = "sfx10"; offset = $Prg + 4 * 0x4000 + (0x8B97 - 0x8000) },
        [pscustomobject]@{ id = "title-setup"; offset = $Prg + 3 * 0x4000 + 0x56 },
        [pscustomobject]@{ id = "title-confirm"; offset = $Prg + 3 * 0x4000 + 0x76 },
        [pscustomobject]@{ id = "title-setup-bridge"; offset = $Fixed + (0xC003 - 0xC000) },
        [pscustomobject]@{ id = "title-setup-three-yield-flow"; offset = $Fixed + (0xD92E - 0xC000) },
        [pscustomobject]@{ id = "title-setup-two-yield-flow"; offset = $Fixed + (0xDB25 - 0xC000) },
        [pscustomobject]@{ id = "frame-yield-helper"; offset = $Fixed + (0xE3FA - 0xC000) },
        [pscustomobject]@{ id = "stop-bridge"; offset = $Fixed + (0xC024 - 0xC000) },
        [pscustomobject]@{ id = "stop-dispatch"; offset = $Fixed + (0xCBAF - 0xC000) },
        [pscustomobject]@{ id = "stop"; offset = $Fixed + (0xEC06 - 0xC000) },
        [pscustomobject]@{ id = "menu-accept"; offset = $Fixed + (0xD768 - 0xC000) },
        [pscustomobject]@{ id = "menu-flow"; offset = $Fixed + (0xE477 - 0xC000) },
        [pscustomobject]@{ id = "mailboxes"; offset = $Fixed + (0xF2F2 - 0xC000) }
    )
    $Output = @(
        & $ExePath --frontend-audio-source-test $RomPath 2>&1)
    if ($LASTEXITCODE -ne 0 -or
        ($Output -join [Environment]::NewLine).Trim() -ne
            "Built strict ROM-derived TFSX-1 frontend audio source.") {
        throw "Isolated TFSX source importer rejected Rev1.`n$(Get-ShortTail $Output)"
    }
    foreach ($Spec in $Sources) {
        $Rom = [IO.File]::ReadAllBytes($RomPath)
        $Rom[$Spec.offset] = $Rom[$Spec.offset] -bxor 1
        $MutatedRom = Join-Path $TestDir ("source-{0}.nes" -f $Spec.id)
        [IO.File]::WriteAllBytes($MutatedRom, $Rom)
        $Output = @(
            & $ExePath --frontend-audio-source-test $MutatedRom 2>&1)
        if ($LASTEXITCODE -eq 0 -or
            ($Output -join [Environment]::NewLine).Trim() -ne
                "Frontend audio source test failed: Frontend audio revision fingerprint mismatch.") {
            throw "Frontend-specific source mutation '$($Spec.id)' was not rejected by TFSX.`n$(Get-ShortTail $Output)"
        }
    }
    if ($DecompRoot) {
        $env:TECMO_ASSETPACK = $PackPath
        $Output = @(& $ExePath --root $DecompRoot --flow-test 2>&1)
        if ($LASTEXITCODE -ne 0 -or
            @($Output | Where-Object { $_ -match "^FLOW TEST PASS:" }).Count -ne 1) {
            throw "Frontend timing/input flow regression failed.`n$(Get-ShortTail $Output)"
        }
    }
    $global:LASTEXITCODE = 0
    Write-Output "FRONTEND AUDIO TEST PASS: TFSX-1 exact-provenance parser stable-PCM title-stop-frame5 SFX10 frame1 track6 frame127 accepted-A-release SFX8 same-pack malformed missing oversized dependency frontend-source-mutations"
} finally {
    $env:TECMO_ASSETPACK = $PreviousPack
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkip
    if (Test-Path -LiteralPath $TestDir) {
        Remove-Item -LiteralPath $TestDir -Recurse -Force
    }
}
