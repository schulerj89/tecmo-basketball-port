param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [switch]$Build
)

$ErrorActionPreference = "Stop"

if (!$ProjectRoot) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$BuildDir = Join-Path $ProjectRoot "build"
$TestDir = [IO.Path]::GetFullPath((Join-Path $BuildDir "gameplay_audio_test"))
$ProofRoot = [IO.Path]::GetFullPath((Join-Path $BuildDir "proof/r4-audio-foundation"))
$BuildPrefix = [IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/') +
    [IO.Path]::DirectorySeparatorChar
if (!$TestDir.StartsWith($BuildPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Gameplay-audio test directory escaped build/."
}
$ProofPrefix = [IO.Path]::GetFullPath((Join-Path $BuildDir "proof")).TrimEnd('\', '/') +
    [IO.Path]::DirectorySeparatorChar
if (!$ProofRoot.StartsWith($ProofPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Audio-proof evidence directory escaped build/proof/."
}
if (!$RomPath) { $RomPath = $env:TECMO_ROM_PATH }
if (!$RomPath -or !(Test-Path -LiteralPath $RomPath)) {
    throw "Pass -RomPath or set TECMO_ROM_PATH to the local Rev1 iNES ROM."
}
$RomPath = (Resolve-Path -LiteralPath $RomPath).Path
if ((Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash -ne
    "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4") {
    throw "Gameplay audio requires the supported Rev1 ROM fingerprint."
}

$ExePath = Join-Path $BuildDir "tecmo_port.exe"
$PackPath = Join-Path $TestDir "gameplay-audio.assetpack"
$Expected = "TSFX-1/TDMC-1 gameplay audio: sfx=968A5DE6 dmc=AD70E6E8 pcm=83E60072 state=17208C83 instructions=131 voices=14 events=pass override=pass cadence=pass gate=pass mailbox=pass independent=pass dmc-continuity=pass clear=pass crosspack=pass"
$ExpectedBaseSha = "6d8f9c7a99a7ce188f1a523247d3a9b9093860fb"
$ProofGolden = @{
    "audio-proof.wav" = "57573ABE791F4277AF6DCFC6E7AE22C7A7F319BC64554B0D7FDD8F16AFBC5D6B"
    "audio-proof.events" = "3E8FB445B0774F847A529B2BC9670F81862F7C6C04B77AEFE7AB7D7D024674AA"
    "audio-proof.manifest" = "47EA2304FFF12C9348E821423E8E0806C9E00FA79DBE8344ED44E3C245B24298"
    waveform_csv = "76642CA7B52835301EEE0BA6185D50103C6DBC2A411D452A7FBFDBDCCFD5F4E2"
    waveform_svg = "6A6ED51A4BB1A77A76ACAA50DF1FA30D367AF5A273C9AB20D5C553EBD2A5A66E"
}
$ExpectedProofVectors = @(
    [pscustomobject]@{ Name="TMUS7_START"; Queue="TRACK7_PASS"; Source="TMUS"; Termination="STARTUP"; MusicTicks=5; MusicAcc=16814794500; MusicPlaying=1; SfxId=0; SfxPlaying=0; DmcActive=0; DmcLevel=0; PcmFnv="AAAEB721" },
    [pscustomobject]@{ Name="TMUS7_TAIL_END"; Queue="TRACK7_PASS"; Source="TMUS"; Termination="CLEAN_END"; MusicTicks=2615; MusicAcc=43775235900; MusicPlaying=0; SfxId=0; SfxPlaying=0; DmcActive=0; DmcLevel=0; PcmFnv="401472A3" },
    [pscustomobject]@{ Name="TMUS5_LOOP"; Queue="TRACK5_PASS"; Source="TMUS"; Termination="LOOP_REPRESENTATIVE"; MusicTicks=1362; MusicAcc=22678021800; MusicPlaying=1; SfxId=0; SfxPlaying=0; DmcActive=0; DmcLevel=0; PcmFnv="BDF8A662" },
    [pscustomobject]@{ Name="TMUS6_LOOP"; Queue="TRACK6_PASS"; Source="TMUS"; Termination="LOOP_REPRESENTATIVE"; MusicTicks=1362; MusicAcc=22678021800; MusicPlaying=1; SfxId=0; SfxPlaying=0; DmcActive=0; DmcLevel=0; PcmFnv="C034740E" },
    [pscustomobject]@{ Name="TMUS8_END"; Queue="TRACK8_PASS"; Source="TMUS"; Termination="CLEAN_END"; MusicTicks=396; MusicAcc=9235724400; MusicPlaying=0; SfxId=0; SfxPlaying=0; DmcActive=0; DmcLevel=0; PcmFnv="8F310591" },
    [pscustomobject]@{ Name="TFSX8_DRY"; Queue="SFX8_PASS"; Source="TFSX"; Termination="DRY_CUE"; MusicTicks=0; MusicAcc=0; MusicPlaying=0; SfxId=8; SfxPlaying=1; DmcActive=0; DmcLevel=64; PcmFnv="100B5218" },
    [pscustomobject]@{ Name="TFSX10_DRY"; Queue="SFX10_PASS"; Source="TFSX"; Termination="DRY_CUE"; MusicTicks=0; MusicAcc=0; MusicPlaying=0; SfxId=10; SfxPlaying=1; DmcActive=0; DmcLevel=64; PcmFnv="09718C9D" },
    [pscustomobject]@{ Name="TSFX3_DRY"; Queue="SFX3_PASS"; Source="TSFX"; Termination="DRY_CUE"; MusicTicks=0; MusicAcc=0; MusicPlaying=0; SfxId=3; SfxPlaying=1; DmcActive=0; DmcLevel=64; PcmFnv="86F89803" },
    [pscustomobject]@{ Name="TSFX5_DRY"; Queue="SFX5_PASS"; Source="TSFX"; Termination="DRY_CUE"; MusicTicks=0; MusicAcc=0; MusicPlaying=0; SfxId=5; SfxPlaying=1; DmcActive=0; DmcLevel=64; PcmFnv="3ED46D03" },
    [pscustomobject]@{ Name="TSFX6_DRY"; Queue="SFX6_PASS"; Source="TSFX"; Termination="DRY_CUE"; MusicTicks=0; MusicAcc=0; MusicPlaying=0; SfxId=6; SfxPlaying=1; DmcActive=0; DmcLevel=64; PcmFnv="7E3C10F6" },
    [pscustomobject]@{ Name="TSFX11_DRY"; Queue="SFX11_PASS"; Source="TSFX"; Termination="DRY_CUE"; MusicTicks=0; MusicAcc=0; MusicPlaying=0; SfxId=11; SfxPlaying=1; DmcActive=0; DmcLevel=64; PcmFnv="2DA2ABE6" },
    [pscustomobject]@{ Name="TSFX12_DRY"; Queue="SFX12_PASS"; Source="TSFX"; Termination="DRY_CUE"; MusicTicks=0; MusicAcc=0; MusicPlaying=0; SfxId=12; SfxPlaying=1; DmcActive=0; DmcLevel=64; PcmFnv="B05C5279" },
    [pscustomobject]@{ Name="TSFX13_DRY"; Queue="SFX13_PASS"; Source="TSFX"; Termination="DRY_CUE"; MusicTicks=0; MusicAcc=0; MusicPlaying=0; SfxId=13; SfxPlaying=1; DmcActive=0; DmcLevel=64; PcmFnv="E2751277" },
    [pscustomobject]@{ Name="TSFX14_DRY"; Queue="SFX14_PASS"; Source="TSFX"; Termination="DRY_CUE"; MusicTicks=0; MusicAcc=0; MusicPlaying=0; SfxId=14; SfxPlaying=1; DmcActive=0; DmcLevel=64; PcmFnv="ED7AEB46" },
    [pscustomobject]@{ Name="TMUS5_TSFX3_OVERRIDE"; Queue="TRACK5_SFX3_PASS"; Source="MIXED"; Termination="MUSIC_TICKS"; MusicTicks=11; MusicAcc=4736547900; MusicPlaying=1; SfxId=3; SfxPlaying=1; DmcActive=0; DmcLevel=64; PcmFnv="5F52DDE8" },
    [pscustomobject]@{ Name="TDMC0_CLIP"; Queue="DMC0_PASS"; Source="TDMC"; Termination="INACTIVE_HELD_DAC"; MusicTicks=0; MusicAcc=0; MusicPlaying=0; SfxId=0; SfxPlaying=0; DmcActive=0; DmcLevel=80; PcmFnv="39D84D79" },
    [pscustomobject]@{ Name="TDMC1_CLIP"; Queue="DMC1_PASS"; Source="TDMC"; Termination="INACTIVE_HELD_DAC"; MusicTicks=0; MusicAcc=0; MusicPlaying=0; SfxId=0; SfxPlaying=0; DmcActive=0; DmcLevel=60; PcmFnv="CA0D323F" },
    [pscustomobject]@{ Name="TDMC2_CLIP"; Queue="DMC2_PASS"; Source="TDMC"; Termination="INACTIVE_HELD_DAC"; MusicTicks=0; MusicAcc=0; MusicPlaying=0; SfxId=0; SfxPlaying=0; DmcActive=0; DmcLevel=64; PcmFnv="CA447C39" },
    [pscustomobject]@{ Name="TDMC3_CLIP"; Queue="DMC3_PASS"; Source="TDMC"; Termination="INACTIVE_HELD_DAC"; MusicTicks=0; MusicAcc=0; MusicPlaying=0; SfxId=0; SfxPlaying=0; DmcActive=0; DmcLevel=64; PcmFnv="01CC4CC0" },
    [pscustomobject]@{ Name="TDMC4_CLIP"; Queue="DMC4_PASS"; Source="TDMC"; Termination="INACTIVE_HELD_DAC"; MusicTicks=0; MusicAcc=0; MusicPlaying=0; SfxId=0; SfxPlaying=0; DmcActive=0; DmcLevel=46; PcmFnv="DDFCE7B7" },
    [pscustomobject]@{ Name="TDMC_POST_END_HOLD"; Queue="DMC0_END_PASS"; Source="TDMC"; Termination="HELD_DAC"; MusicTicks=0; MusicAcc=0; MusicPlaying=0; SfxId=0; SfxPlaying=0; DmcActive=0; DmcLevel=80; PcmFnv="2FF41DC5" },
    [pscustomobject]@{ Name="TDMC_RETRIGGER"; Queue="DMC0_RETRIGGER_PASS"; Source="TDMC"; Termination="ACTIVE"; MusicTicks=0; MusicAcc=0; MusicPlaying=0; SfxId=0; SfxPlaying=0; DmcActive=1; DmcLevel=102; PcmFnv="D4927189" },
    [pscustomobject]@{ Name="TDMC_STOP_HOLD"; Queue="STOP_ALL_PASS"; Source="TDMC"; Termination="HELD_DAC"; MusicTicks=0; MusicAcc=0; MusicPlaying=0; SfxId=0; SfxPlaying=0; DmcActive=0; DmcLevel=102; PcmFnv="75403DC5" }
)
$PreviousPack = $env:TECMO_ASSETPACK
$PreviousSkip = $env:TECMO_SKIP_SHORTCUT

function Get-ShortTail([object[]]$Lines) {
    return (@($Lines | Select-Object -Last 8) -join [Environment]::NewLine)
}

function Remove-ValidatedDirectory([string]$Path, [string]$AllowedRoot,
                                    [string]$Label) {
    $PathFull = [IO.Path]::GetFullPath($Path)
    $RootFull = [IO.Path]::GetFullPath($AllowedRoot)
    $Prefix = $RootFull.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    if (!$PathFull.StartsWith($Prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "$Label cleanup path escaped its validated root."
    }
    if (Test-Path -LiteralPath $PathFull) {
        Remove-Item -LiteralPath $PathFull -Recurse -Force
    }
}

function Invoke-GameplaySourceGate([string]$SourcePath, [bool]$ExpectSuccess) {
    $Output = @(& $ExePath --gameplay-audio-source-test $SourcePath 2>&1)
    $ExitCode = $LASTEXITCODE
    $Text = ($Output -join [Environment]::NewLine)
    if ($ExpectSuccess) {
        if ($ExitCode -ne 0 -or
            $Text -notmatch "Built strict ROM-derived TSFX-1/TDMC-1 gameplay audio source") {
            throw "Isolated gameplay-audio importer rejected canonical Rev1.`n$(Get-ShortTail $Output)"
        }
    } elseif ($ExitCode -eq 0 -or
              $Text -notmatch "Rev1 gameplay audio" -or
              $Text -match "full-ROM SHA-256 mismatch") {
        throw "Gameplay source mutation was not rejected by gameplay revision validation.`n$(Get-ShortTail $Output)"
    }
}

function Invoke-PackIdentityGate([string]$MusicPack, [string]$GameplayPack,
                                  [string]$Expectation) {
    $Output = @(& $ExePath --audio-pack-identity-test $MusicPack $GameplayPack $Expectation 2>&1)
    $ExitCode = $LASTEXITCODE
    $Text = ($Output -join [Environment]::NewLine)
    $ExpectedText = if ($Expectation -eq "accept") {
        "Audio pack identity test pass: accept"
    } else {
        "Audio pack identity test pass: reject-preserve"
    }
    if ($ExitCode -ne 0 -or $Text.Trim() -ne $ExpectedText) {
        throw "Real-pack canonical identity gate '$Expectation' failed.`n$(Get-ShortTail $Output)"
    }
}

function Get-ProofEventRecords([string]$EventsPath) {
    $Records = @()
    $RequiredFields = @(
        "format", "vector", "name", "start", "count", "queue", "source",
        "termination", "music_ticks", "music_acc", "music_playing", "sfx_id",
        "sfx_playing", "dmc_active", "dmc_level", "pcm_fnv"
    )
    foreach ($Line in [IO.File]::ReadLines($EventsPath)) {
        if ($Line -notmatch "\|vector=") { continue }
        $Fields = @{}
        foreach ($Part in ($Line -split "\|")) {
            $Pair = $Part -split "=", 2
            if ($Pair.Count -eq 2) { $Fields[$Pair[0]] = $Pair[1] }
        }
        foreach ($Field in $RequiredFields) {
            if (!$Fields.ContainsKey($Field)) {
                throw "Audio proof event is missing required field '$Field'."
            }
        }
        if ($Fields.format -ne "TECMAUDIOPROOF-1") {
            throw "Audio proof event format is not TECMAUDIOPROOF-1."
        }
        $Records += [pscustomobject]@{
            vector = [int]$Fields.vector
            name = ([string]$Fields.name).Trim()
            start = [int64]$Fields.start
            count = [int64]$Fields.count
            queue = ([string]$Fields.queue).Trim()
            source = ([string]$Fields.source).Trim()
            termination = ([string]$Fields.termination).Trim()
            music_ticks = [int64]$Fields.music_ticks
            music_acc = [int64]$Fields.music_acc
            music_playing = [int]$Fields.music_playing
            sfx_id = [int]$Fields.sfx_id
            sfx_playing = [int]$Fields.sfx_playing
            dmc_active = [int]$Fields.dmc_active
            dmc_level = [int]$Fields.dmc_level
            pcm_fnv = ([string]$Fields.pcm_fnv).Trim().ToUpperInvariant()
        }
    }
    return $Records
}

function Get-ProofWavInfo([string]$WavPath) {
    $Wav = [IO.File]::ReadAllBytes($WavPath)
    if ($Wav.Length -lt 44) {
        throw "Audio proof WAV is shorter than the canonical 44-byte PCM header."
    }
    if ([Text.Encoding]::ASCII.GetString($Wav, 0, 4) -ne "RIFF" -or
        [Text.Encoding]::ASCII.GetString($Wav, 8, 4) -ne "WAVE" -or
        [Text.Encoding]::ASCII.GetString($Wav, 12, 4) -ne "fmt " -or
        [Text.Encoding]::ASCII.GetString($Wav, 36, 4) -ne "data") {
        throw "Audio proof WAV does not have the exact RIFF/WAVE/fmt/data layout."
    }
    $ExpectedRiffSize = [uint64]$Wav.Length - 8
    $ExpectedDataSize = [uint64]$Wav.Length - 44
    $RiffSize = [uint64][BitConverter]::ToUInt32($Wav, 4)
    $FormatSize = [uint64][BitConverter]::ToUInt32($Wav, 16)
    $DataSize = [uint64][BitConverter]::ToUInt32($Wav, 40)
    if ($RiffSize -ne $ExpectedRiffSize -or
        $FormatSize -ne 16 -or
        [BitConverter]::ToUInt16($Wav, 20) -ne 1 -or
        [BitConverter]::ToUInt16($Wav, 22) -ne 1 -or
        [BitConverter]::ToUInt32($Wav, 24) -ne 44100 -or
        [BitConverter]::ToUInt32($Wav, 28) -ne 88200 -or
        [BitConverter]::ToUInt16($Wav, 32) -ne 2 -or
        [BitConverter]::ToUInt16($Wav, 34) -ne 16 -or
        ($DataSize % 2) -ne 0 -or
        $DataSize -ne $ExpectedDataSize) {
        throw "Audio proof WAV header or data size is not exact 44.1 kHz mono 16-bit PCM."
    }
    return [pscustomobject]@{
        bytes = [uint64]$Wav.Length
        data_bytes = $DataSize
        samples = $DataSize / 2
        bytes_data = $Wav
    }
}

function Assert-ProofEventRecords([object[]]$Rows, [uint64]$WavSamples,
                                  [string]$Label) {
    if ($Rows.Count -ne $ExpectedProofVectors.Count) {
        throw "$Label audio proof event vector count is wrong."
    }
    [uint64]$NextStart = 0
    for ($Index = 0; $Index -lt $ExpectedProofVectors.Count; ++$Index) {
        $Row = $Rows[$Index]
        $ExpectedVector = $ExpectedProofVectors[$Index]
        if ($Row.vector -ne $Index -or
            $Row.name -ne $ExpectedVector.Name -or
            $Row.queue -ne $ExpectedVector.Queue -or
            $Row.source -ne $ExpectedVector.Source -or
            $Row.termination -ne $ExpectedVector.Termination) {
            throw "$Label audio proof vector $Index/name/order is wrong."
        }
        if ($Row.count -le 0 -or $Row.start -lt 0 -or
            [uint64]$Row.start -ne $NextStart -or
            [uint64]$Row.count -gt [uint64]::MaxValue - $NextStart) {
            throw "$Label audio proof vector $Index does not provide contiguous positive coverage."
        }
        $NextStart += [uint64]$Row.count
        if ($Row.music_ticks -ne $ExpectedVector.MusicTicks -or
            $Row.music_acc -ne $ExpectedVector.MusicAcc -or
            $Row.music_playing -ne $ExpectedVector.MusicPlaying -or
            $Row.sfx_id -ne $ExpectedVector.SfxId -or
            $Row.sfx_playing -ne $ExpectedVector.SfxPlaying -or
            $Row.dmc_active -ne $ExpectedVector.DmcActive -or
            $Row.dmc_level -ne $ExpectedVector.DmcLevel -or
            $Row.pcm_fnv -ne $ExpectedVector.PcmFnv) {
            throw "$Label audio proof state semantics drifted for vector $Index '$($Row.name)'."
        }
    }
    if ($NextStart -ne $WavSamples) {
        throw "$Label audio proof coverage ($NextStart samples) does not equal WAV data ($WavSamples samples)."
    }
}

function New-ProofWaveformEvidence([string]$EventsPath, [string]$WavPath,
                                    [string]$CsvPath, [string]$SvgPath) {
    $Invariant = [Globalization.CultureInfo]::InvariantCulture
    $WavInfo = Get-ProofWavInfo $WavPath
    $Wav = $WavInfo.bytes_data
    $Rows = @(Get-ProofEventRecords $EventsPath)
    Assert-ProofEventRecords $Rows $WavInfo.samples "Waveform"
    $Csv = New-Object System.Collections.Generic.List[string]
    [void]$Csv.Add("vector,name,sample_index,sample_offset,amplitude")
    $Points = 128
    foreach ($Row in $Rows) {
        for ($Point = 0; $Point -lt $Points; ++$Point) {
            $Offset = if ($Row.count -eq 0) { 0 } else {
                [int64][math]::Floor($Point * $Row.count / $Points)
            }
            $SampleIndex = $Row.start + $Offset
            $ByteIndex = [int](44 + $SampleIndex * 2)
            if ($ByteIndex -lt 44 -or $ByteIndex + 1 -ge $Wav.Length) {
                throw "Audio proof event sample range exceeded WAV data."
            }
            $Unsigned = [int]$Wav[$ByteIndex] -bor
                ([int]$Wav[$ByteIndex + 1] -shl 8)
            $Amplitude = if ($Unsigned -ge 32768) { $Unsigned - 65536 } else { $Unsigned }
            [void]$Csv.Add([string]::Format(
                $Invariant, "{0:D3},{1},{2:D20},{3:D4},{4}",
                $Row.vector, $Row.name, $SampleIndex, $Offset, $Amplitude))
        }
    }
    [IO.File]::WriteAllText($CsvPath, ($Csv -join "`n") + "`n",
                            [Text.Encoding]::ASCII)

    $SvgHeight = 24 + $Rows.Count * 34
    $Svg = New-Object System.Collections.Generic.List[string]
    [void]$Svg.Add([string]::Format($Invariant,
        '<svg xmlns="http://www.w3.org/2000/svg" width="1660" height="{0}" viewBox="0 0 1660 {0}">',
        $SvgHeight))
    [void]$Svg.Add('<rect width="100%" height="100%" fill="#101820"/>')
    foreach ($Row in $Rows) {
        $Base = 34 + $Row.vector * 34
        $PointsText = New-Object System.Collections.Generic.List[string]
        for ($Point = 0; $Point -lt $Points; ++$Point) {
            $Offset = if ($Row.count -eq 0) { 0 } else {
                [int64][math]::Floor($Point * $Row.count / $Points)
            }
            $SampleIndex = $Row.start + $Offset
            $ByteIndex = [int](44 + $SampleIndex * 2)
            $Unsigned = [int]$Wav[$ByteIndex] -bor
                ([int]$Wav[$ByteIndex + 1] -shl 8)
            $Amplitude = if ($Unsigned -ge 32768) { $Unsigned - 65536 } else { $Unsigned }
            $X = 250.0 + $Point * 11.0
            $Y = $Base - ($Amplitude * 11.0 / 32768.0)
            [void]$PointsText.Add([string]::Format($Invariant, "{0:F2},{1:F2}", $X, $Y))
        }
        [void]$Svg.Add([string]::Format($Invariant,
            '<text x="8" y="{0:F2}" fill="#E6EDF3" font-family="monospace" font-size="12">{1}</text>',
            ($Base + 4.0), $Row.name))
        [void]$Svg.Add([string]::Format($Invariant,
            '<line x1="250" y1="{0:F2}" x2="1650" y2="{0:F2}" stroke="#263746"/>',
            $Base))
        [void]$Svg.Add([string]::Format($Invariant,
            '<polyline fill="none" stroke="#56D364" stroke-width="1" points="{0}"/>',
            ($PointsText -join " ")))
    }
    [void]$Svg.Add("</svg>")
    [IO.File]::WriteAllText($SvgPath, ($Svg -join "`n") + "`n",
                            [Text.Encoding]::ASCII)
}

function Get-ProofRepositoryProvenance {
    $ProjectFull = [IO.Path]::GetFullPath($ProjectRoot)
    $GitRootText = @(& git -C $ProjectRoot rev-parse --show-toplevel 2>$null)
    $GitRootExit = $LASTEXITCODE
    if ($GitRootExit -ne 0 -or $GitRootText.Count -ne 1) {
        throw "Could not resolve the proof repository root."
    }
    $GitRootFull = [IO.Path]::GetFullPath(([string]$GitRootText[0]).Trim())
    if (![string]::Equals($GitRootFull, $ProjectFull,
                          [StringComparison]::OrdinalIgnoreCase)) {
        throw "Proof repository root does not exactly match ProjectRoot."
    }

    $HeadText = @(& git -C $ProjectRoot rev-parse HEAD 2>$null)
    $HeadExit = $LASTEXITCODE
    if ($HeadExit -ne 0 -or $HeadText.Count -ne 1) {
        throw "Could not resolve the proof-generation HEAD."
    }
    $Head = ([string]$HeadText[0]).Trim()
    if ($Head -notmatch "^[0-9a-fA-F]{40}$") {
        throw "Proof-generation HEAD is not a 40-hex commit."
    }

    $StatusLines = @(& git -C $ProjectRoot status --porcelain --untracked-files=no 2>$null)
    $StatusExit = $LASTEXITCODE
    if ($StatusExit -ne 0 -or $StatusLines.Count -ne 0) {
        throw "Proof repository has tracked or index dirt; commit the proof revision first."
    }

    $MergeBaseText = @(& git -C $ProjectRoot merge-base $ExpectedBaseSha HEAD 2>$null)
    $MergeBaseExit = $LASTEXITCODE
    if ($MergeBaseExit -ne 0 -or $MergeBaseText.Count -ne 1 -or
        ([string]$MergeBaseText[0]).Trim() -ne $ExpectedBaseSha) {
        throw "Proof-generation HEAD is not based exactly on the expected R4 base."
    }
    return [pscustomobject]@{
        base_sha = $ExpectedBaseSha
        head = $Head
    }
}

function Invoke-AudioProof([string]$PackPath) {
    if (Test-Path -LiteralPath $ProofRoot) {
        Remove-ValidatedDirectory $ProofRoot (Join-Path $BuildDir "proof") "audio-proof"
    }
    $Run1 = Join-Path $ProofRoot "run1"
    $Run2 = Join-Path $ProofRoot "run2"
    [void](New-Item -ItemType Directory -Force -Path $Run1, $Run2)
    $PackSha = (Get-FileHash -LiteralPath $PackPath -Algorithm SHA256).Hash
    foreach ($Run in @($Run1, $Run2)) {
        $Output = @(& $ExePath --audio-proof $PackPath $Run 2>&1)
        if ($LASTEXITCODE -ne 0 -or
            ($Output -join [Environment]::NewLine) -notmatch "Audio proof pass: vectors=23") {
            throw "Deterministic audio proof command failed.`n$(Get-ShortTail $Output)"
        }
    }
    $Artifacts = @("audio-proof.wav", "audio-proof.events", "audio-proof.manifest")
    $Hashes = @{}
    foreach ($Artifact in $Artifacts) {
        $First = Join-Path $Run1 $Artifact
        $Second = Join-Path $Run2 $Artifact
        if (!(Test-Path -LiteralPath $First) -or !(Test-Path -LiteralPath $Second) -or
            (Get-Item -LiteralPath $First).Length -ne (Get-Item -LiteralPath $Second).Length) {
            throw "Audio proof artifact '$Artifact' is not byte-identical across runs."
        }
        $Hash1 = (Get-FileHash -LiteralPath $First -Algorithm SHA256).Hash
        $Hash2 = (Get-FileHash -LiteralPath $Second -Algorithm SHA256).Hash
        if ($Hash1 -ne $Hash2 -or $Hash1 -ne $ProofGolden[$Artifact]) {
            throw "Audio proof artifact '$Artifact' is not byte-identical or has drifted from its golden identity."
        }
        $Hashes[$Artifact] = $Hash1
    }
    $EventsPath = Join-Path $Run1 "audio-proof.events"
    $EventsPath2 = Join-Path $Run2 "audio-proof.events"
    $WavPath = Join-Path $Run1 "audio-proof.wav"
    $WavPath2 = Join-Path $Run2 "audio-proof.wav"
    $WavInfo = Get-ProofWavInfo $WavPath
    $WavInfo2 = Get-ProofWavInfo $WavPath2
    if ($WavInfo.bytes -ne $WavInfo2.bytes -or
        $WavInfo.data_bytes -ne $WavInfo2.data_bytes -or
        $WavInfo.samples -ne $WavInfo2.samples) {
        throw "Audio proof WAV layouts are not identical across runs."
    }
    $Rows = @(Get-ProofEventRecords $EventsPath)
    $Rows2 = @(Get-ProofEventRecords $EventsPath2)
    Assert-ProofEventRecords $Rows $WavInfo.samples "Run1"
    Assert-ProofEventRecords $Rows2 $WavInfo2.samples "Run2"
    $EvidenceDir = Join-Path $ProofRoot "waveform"
    $EvidenceDir2 = Join-Path $EvidenceDir "run2"
    [void](New-Item -ItemType Directory -Force -Path $EvidenceDir, $EvidenceDir2)
    $CsvPath = Join-Path $EvidenceDir "audio-proof-waveform.csv"
    $SvgPath = Join-Path $EvidenceDir "audio-proof-waveform.svg"
    $CsvPath2 = Join-Path $EvidenceDir2 "audio-proof-waveform.csv"
    $SvgPath2 = Join-Path $EvidenceDir2 "audio-proof-waveform.svg"
    New-ProofWaveformEvidence $EventsPath $WavPath $CsvPath $SvgPath
    New-ProofWaveformEvidence $EventsPath2 $WavPath2 $CsvPath2 $SvgPath2
    $WaveformCsvHash = (Get-FileHash -LiteralPath $CsvPath -Algorithm SHA256).Hash
    $WaveformCsvHash2 = (Get-FileHash -LiteralPath $CsvPath2 -Algorithm SHA256).Hash
    $WaveformSvgHash = (Get-FileHash -LiteralPath $SvgPath -Algorithm SHA256).Hash
    $WaveformSvgHash2 = (Get-FileHash -LiteralPath $SvgPath2 -Algorithm SHA256).Hash
    if ($WaveformCsvHash -ne $WaveformCsvHash2 -or
        $WaveformCsvHash -ne $ProofGolden["waveform_csv"] -or
        $WaveformSvgHash -ne $WaveformSvgHash2 -or
        $WaveformSvgHash -ne $ProofGolden["waveform_svg"]) {
        throw "Independent waveform evidence is not byte-identical or has drifted from its golden identity."
    }
    $Provenance = Get-ProofRepositoryProvenance
    $ManifestPath = Join-Path $ProofRoot "proof-manifest.txt"
    $Manifest = @(
        "manifest=TECMAUDIOPROOF-SCRIPT-2",
        "base_sha=$($Provenance.base_sha)",
        "proof_generation_head=$($Provenance.head)",
        "canonical_rom_revision=Rev1",
        "canonical_rom_sha256=076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4",
        "canonical_rom_visibility=local_private",
        "pack_sha256=$PackSha",
        "tmus_payload_size=36784",
        "tmus_payload_fnv1a32=05C00ECB",
        "tfsx_payload_size=1792",
        "tfsx_payload_fnv1a32=985DC7ED",
        "tsfx_payload_size=2824",
        "tsfx_payload_fnv1a32=968A5DE6",
        "tdmc_payload_size=2515",
        "tdmc_payload_fnv1a32=AD70E6E8",
        "command=--audio-proof PACK OUTPUT_DIR",
        "source=explicit-validated-asset-pack",
        "vector_count=$($Rows.Count)",
        "sample_format=44100Hz_mono_s16le",
        "audio_proof_wav_sha256=$($Hashes['audio-proof.wav'])",
        "audio_proof_events_sha256=$($Hashes['audio-proof.events'])",
        "audio_proof_manifest_sha256=$($Hashes['audio-proof.manifest'])",
        "waveform_csv_sha256=$WaveformCsvHash",
        "waveform_svg_sha256=$WaveformSvgHash",
        "waveform_run2_csv_sha256=$WaveformCsvHash2",
        "waveform_run2_svg_sha256=$WaveformSvgHash2"
    )
    [IO.File]::WriteAllText($ManifestPath, ($Manifest -join "`n") + "`n",
                            [Text.Encoding]::ASCII)
    return [pscustomobject]@{
        root = $ProofRoot
        manifest = $ManifestPath
        wav_sha256 = $Hashes['audio-proof.wav']
        events_sha256 = $Hashes['audio-proof.events']
        proof_manifest_sha256 = $Hashes['audio-proof.manifest']
        waveform_csv_sha256 = $WaveformCsvHash
        waveform_svg_sha256 = $WaveformSvgHash
        waveform_run2_csv_sha256 = $WaveformCsvHash2
        waveform_run2_svg_sha256 = $WaveformSvgHash2
        proof_head = $Provenance.head
    }
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
        $EntryId = [Text.Encoding]::ASCII.GetString($Bytes, $At, $End - $At)
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

function Invoke-GameplayAudio([string]$AssetPack, [bool]$ExpectSuccess) {
    $env:TECMO_ASSETPACK = $AssetPack
    $Output = @(& $ExePath --gameplay-audio-test 2>&1)
    $ExitCode = $LASTEXITCODE
    if ($ExpectSuccess) {
        if ($ExitCode -ne 0 -or
            ($Output -join [Environment]::NewLine).Trim() -ne $Expected) {
            throw "Native gameplay-audio golden failed.`n$(Get-ShortTail $Output)"
        }
    } elseif ($ExitCode -eq 0 -or
              @($Output | Where-Object { $_ -match "TSFX-1/TDMC-1" }).Count -eq 0) {
        throw "Malformed/missing gameplay audio was not rejected.`n$(Get-ShortTail $Output)"
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
        Remove-ValidatedDirectory $TestDir $BuildDir "gameplay-audio test"
    }
    [void](New-Item -ItemType Directory -Path $TestDir)
    $Output = @(& $ExePath --build-assetpack $RomPath $PackPath 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "Rev1 gameplay-audio pack build failed.`n$(Get-ShortTail $Output)"
    }
    $Pack = [IO.File]::ReadAllBytes($PackPath)
    $Sfx = Get-PackEntry $Pack "audio/gameplay-sfx"
    $Dmc = Get-PackEntry $Pack "audio/gameplay-dmc"
    $SourceMap = Get-PackEntry $Pack "system/source-map"
    if ($Sfx.byte_count -ne 2824 -or $Dmc.byte_count -ne 2515) {
        throw "TSFX-1/TDMC-1 exact sizes are wrong."
    }
    $MapText = [Text.Encoding]::UTF8.GetString((Get-EntryBytes $Pack $SourceMap))
    $Map = $MapText | ConvertFrom-Json
    $SfxMap = @($Map.logical_entries | Where-Object id -eq "audio/gameplay-sfx")
    $DmcMap = @($Map.logical_entries | Where-Object id -eq "audio/gameplay-dmc")
    $MusicMap = @($Map.logical_entries | Where-Object id -eq "audio/music")
    $SfxSourceByRole = @{}
    foreach ($Source in @($SfxMap[0].sources)) {
        $SfxSourceByRole[[string]$Source.role] = $Source
    }
    $DmcSourceByRole = @{}
    foreach ($Source in @($DmcMap[0].sources)) {
        $DmcSourceByRole[[string]$Source.role] = $Source
    }
    $MusicSourceByRole = @{}
    foreach ($Source in @($MusicMap[0].sources)) {
        $MusicSourceByRole[[string]$Source.role] = $Source
    }
    if ($SfxMap.Count -ne 1 -or $DmcMap.Count -ne 1 -or $MusicMap.Count -ne 1 -or
        $SfxMap[0].schema -ne "tecmo.gameplay-audio/TSFX-1" -or
        $DmcMap[0].schema -ne "tecmo.gameplay-audio/TDMC-1" -or
        $SfxMap[0].native_contract.payload_fingerprint_fnv1a32 -ne "968A5DE6" -or
        $DmcMap[0].native_contract.payload_fingerprint_fnv1a32 -ne "AD70E6E8" -or
        $SfxMap[0].native_contract.semantic_events.PSObject.Properties["5"].Value -ne
            "bank05-9fec-cue" -or
        $SfxMap[0].native_contract.semantic_events.PSObject.Properties["6"].Value -ne
            "violation-cue" -or
        $SfxMap[0].native_contract.event_conditions.PSObject.Properties["5"].Value -ne
            "restart-after-violation-foul-or-period-reset-caller-gated-by-game-music" -or
        (@($SfxMap[0].native_contract.priority_masks) -join ",") -ne
            "16,32,64,128" -or
        $DmcMap[0].native_contract.clip_names.PSObject.Properties["2"].Value -ne
            "bank05-a9c5" -or
        $DmcMap[0].native_contract.clip_names.PSObject.Properties["3"].Value -ne
            "layup-sequence-abf5" -or
        (@($DmcMap[0].native_contract.unresolved_clip_ids) -join ",") -ne
            "0,1,2" -or
        $DmcMap[0].native_contract.semantic_boundary -ne
            "clip IDs 0, 1, and 2 remain address-bound and unresolved; ABF5 has sequence-level correlation only; no impact, rim, or exclusivity claim" -or
        $DmcMap[0].native_contract.delta_counter_persistence -ne
            "retrigger-end-and-clear" -or
        $DmcMap[0].native_contract.inactive_output -ne "held-dac-level" -or
        @($MusicMap[0].sources | Where-Object role -eq "pregame-matchup-stinger-8").Count -ne 1 -or
        [int]$SfxSourceByRole["clock-buzzer-id-3-a"].cpu_address -ne 0xE7DB -or
        [int]$SfxSourceByRole["clock-buzzer-id-3-a"].request_site_cpu_address -ne 0xE7DD -or
        [int]$SfxSourceByRole["clock-buzzer-id-3-a"].size -ne 5 -or
        $SfxSourceByRole["clock-buzzer-id-3-a"].fingerprint_fnv1a32 -ne "FA9A48DB" -or
        [int]$SfxSourceByRole["countdown-id-14"].cpu_address -ne 0xE863 -or
        [int]$SfxSourceByRole["countdown-id-14"].request_site_cpu_address -ne 0xE865 -or
        $SfxSourceByRole["countdown-id-14"].fingerprint_fnv1a32 -ne "E30ADA62" -or
        [int]$SfxSourceByRole["clock-buzzer-id-3-b"].cpu_address -ne 0xE86D -or
        [int]$SfxSourceByRole["clock-buzzer-id-3-b"].request_site_cpu_address -ne 0xE86F -or
        $SfxSourceByRole["clock-buzzer-id-3-b"].fingerprint_fnv1a32 -ne "FA9A48DB" -or
        [int]$SfxSourceByRole["gameplay-id-5"].size -ne 5 -or
        $SfxSourceByRole["gameplay-id-5"].fingerprint_fnv1a32 -ne "5824A080" -or
        [int]$SfxSourceByRole["crowd-response-id-11"].size -ne 14 -or
        $SfxSourceByRole["crowd-response-id-11"].fingerprint_fnv1a32 -ne "B7141C72" -or
        [int]$SfxSourceByRole["side-result-ids-12-13"].size -ne 22 -or
        $SfxSourceByRole["side-result-ids-12-13"].fingerprint_fnv1a32 -ne "CFCD9759" -or
        [int]$DmcSourceByRole["bank05-a9c5-trigger"].cpu_address -ne 0xA9C5 -or
        [int]$DmcSourceByRole["bank05-a9c5-trigger"].size -ne 21 -or
        $DmcSourceByRole["bank05-a9c5-trigger"].fingerprint_fnv1a32 -ne "567D5B90" -or
        [int]$DmcSourceByRole["layup-sequence-abf5-trigger"].cpu_address -ne 0xABF5 -or
        [int]$DmcSourceByRole["layup-sequence-abf5-trigger"].size -ne 21 -or
        $DmcSourceByRole["layup-sequence-abf5-trigger"].fingerprint_fnv1a32 -ne "1C158FAB" -or
        [int]$MusicSourceByRole["pregame-matchup-track-queue"].cpu_address -ne 0xA145 -or
        [int]$MusicSourceByRole["pregame-matchup-track-queue"].size -ne 5 -or
        $MusicSourceByRole["pregame-matchup-track-queue"].fingerprint_fnv1a32 -ne "1E564AC0") {
        throw "Gameplay-audio or track-8 source-map provenance is malformed."
    }
    Invoke-GameplayAudio $PackPath $true
    Invoke-GameplaySourceGate $RomPath $true
    $CanonicalAlias = "$TestDir\.\gameplay-audio.assetpack"
    Invoke-PackIdentityGate $PackPath $CanonicalAlias "accept"
    $DistinctPack = Join-Path $TestDir "gameplay-audio-copy.assetpack"
    [IO.File]::WriteAllBytes($DistinctPack, $Pack)
    Invoke-PackIdentityGate $PackPath $DistinctPack "reject"

    foreach ($Spec in @(
        [pscustomobject]@{ id = "sfx-payload"; entry = $Sfx; delta = 128 },
        [pscustomobject]@{ id = "dmc-payload"; entry = $Dmc; delta = 336 },
        [pscustomobject]@{ id = "cross-pack-token"; entry = $Dmc; delta = 12 }
    )) {
        $Mutated = [byte[]]$Pack.Clone()
        $At = [int]$Spec.entry.pack_offset + $Spec.delta
        $Mutated[$At] = $Mutated[$At] -bxor 1
        $Path = Join-Path $TestDir ("{0}.assetpack" -f $Spec.id)
        [IO.File]::WriteAllBytes($Path, $Mutated)
        Invoke-GameplayAudio $Path $false
    }
    foreach ($Spec in @(
        [pscustomobject]@{ id = "sfx-oversized"; entry = $Sfx; size = [uint64]2825 },
        [pscustomobject]@{ id = "dmc-oversized"; entry = $Dmc; size = [uint64]2516 }
    )) {
        $Mutated = [byte[]]$Pack.Clone()
        [BitConverter]::GetBytes($Spec.size).CopyTo(
            $Mutated, [int]$Spec.entry.directory_offset + 92)
        $Path = Join-Path $TestDir ("{0}.assetpack" -f $Spec.id)
        [IO.File]::WriteAllBytes($Path, $Mutated)
        Invoke-GameplayAudio $Path $false
    }
    foreach ($Spec in @(
        [pscustomobject]@{ id = "sfx-missing"; entry = $Sfx },
        [pscustomobject]@{ id = "dmc-missing"; entry = $Dmc }
    )) {
        $Mutated = [byte[]]$Pack.Clone()
        $Mutated[[int]$Spec.entry.directory_offset] = [byte][char]'x'
        $Path = Join-Path $TestDir ("{0}.assetpack" -f $Spec.id)
        [IO.File]::WriteAllBytes($Path, $Mutated)
        Invoke-GameplayAudio $Path $false
    }

    $Header = [IO.File]::ReadAllBytes($RomPath)[0..15]
    $Trainer = if (($Header[6] -band 4) -ne 0) { 512 } else { 0 }
    $Prg = 16 + $Trainer
    $Fixed = $Prg + ([int]$Header[4] - 1) * 0x4000
    $Sources = @(
        [pscustomobject]@{ id = "sfx-directory"; offset = $Prg + 4 * 0x4000 + 0x0AA4 },
        [pscustomobject]@{ id = "sfx-extension"; offset = $Prg + 4 * 0x4000 + 0x1D8B },
        [pscustomobject]@{ id = "dmc-pool"; offset = $Fixed + 0x80 },
        [pscustomobject]@{ id = "dmc-trigger"; offset = $Prg + 5 * 0x4000 + 0x28D6 },
        [pscustomobject]@{ id = "bank05-a9c5"; offset = $Prg + 5 * 0x4000 + (0xA9C5 - 0x8000) },
        [pscustomobject]@{ id = "layup-sequence-abf5"; offset = $Prg + 5 * 0x4000 + (0xABF5 - 0x8000) },
        [pscustomobject]@{ id = "clock-buzzer-a"; offset = $Fixed + (0xE7DD - 0xC000) },
        [pscustomobject]@{ id = "countdown"; offset = $Fixed + (0xE865 - 0xC000) },
        [pscustomobject]@{ id = "clock-buzzer-b"; offset = $Fixed + (0xE86F - 0xC000) },
        [pscustomobject]@{ id = "bank05-9fec"; offset = $Prg + 5 * 0x4000 + (0x9FEC - 0x8000) },
        [pscustomobject]@{ id = "bank05-ad01"; offset = $Prg + 5 * 0x4000 + (0xAD01 - 0x8000) },
        [pscustomobject]@{ id = "bank05-b1d1"; offset = $Prg + 5 * 0x4000 + (0xB1D1 - 0x8000) },
        [pscustomobject]@{ id = "bank05-b1e6"; offset = $Prg + 5 * 0x4000 + (0xB1E6 - 0x8000) },
        [pscustomobject]@{ id = "pregame-matchup-queue"; offset = $Prg + 6 * 0x4000 + (0xA145 - 0x8000) },
        [pscustomobject]@{ id = "game-music-gate"; offset = $Fixed + 0x2B2B },
        [pscustomobject]@{ id = "fixed-engine"; offset = $Fixed + 0x32F2 }
    )
    foreach ($Spec in $Sources) {
        $Rom = [IO.File]::ReadAllBytes($RomPath)
        $Rom[$Spec.offset] = $Rom[$Spec.offset] -bxor 1
        $MutatedRom = Join-Path $TestDir ("source-{0}.nes" -f $Spec.id)
        $RejectedPack = Join-Path $TestDir ("source-{0}.assetpack" -f $Spec.id)
        [IO.File]::WriteAllBytes($MutatedRom, $Rom)
        if ($Spec.id -ne "pregame-matchup-queue") {
            Invoke-GameplaySourceGate $MutatedRom $false
        }
        $Output = @(& $ExePath --build-assetpack $MutatedRom $RejectedPack 2>&1)
        if ($LASTEXITCODE -eq 0) {
            throw "Gameplay-audio source mutation '$($Spec.id)' was accepted.`n$(Get-ShortTail $Output)"
        }
    }
    $ProofFacts = Invoke-AudioProof $PackPath
    if (!(Test-Path -LiteralPath $ProofFacts.manifest) -or
        $ProofFacts.wav_sha256.Length -ne 64 -or
        $ProofFacts.events_sha256.Length -ne 64 -or
        $ProofFacts.proof_manifest_sha256.Length -ne 64 -or
        $ProofFacts.waveform_csv_sha256.Length -ne 64 -or
        $ProofFacts.waveform_svg_sha256.Length -ne 64 -or
        $ProofFacts.waveform_run2_csv_sha256.Length -ne 64 -or
        $ProofFacts.waveform_run2_svg_sha256.Length -ne 64 -or
        $ProofFacts.proof_head -notmatch "^[0-9a-fA-F]{40}$") {
        throw "Audio proof manifest or waveform evidence is incomplete."
    }
    $global:LASTEXITCODE = 0
    Write-Output "GAMEPLAY AUDIO TEST PASS: TSFX-1 TDMC-1 provenance parser mixer override cadence music-gate mailbox DMC-independence DMC-continuity clear-all malformed missing oversized cross-pack source-mutations"
} finally {
    $env:TECMO_ASSETPACK = $PreviousPack
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkip
    if (Test-Path -LiteralPath $TestDir) {
        Remove-ValidatedDirectory $TestDir $BuildDir "gameplay-audio test"
    }
}
