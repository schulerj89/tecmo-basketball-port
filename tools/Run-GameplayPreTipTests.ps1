param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [string]$ReferenceRoot,
    [switch]$Build
)

$ErrorActionPreference = "Stop"
if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if (!$RomPath) { $RomPath = $env:TECMO_ROM_PATH }
if (!$RomPath -or !(Test-Path -LiteralPath $RomPath -PathType Leaf)) {
    throw "Pass -RomPath or set TECMO_ROM_PATH to the local Rev1 iNES ROM."
}
$RomPath = (Resolve-Path -LiteralPath $RomPath).Path
$ExpectedRomSha256 =
    "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4"
if ((Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash -ne
    $ExpectedRomSha256) {
    throw "TPTI-1 tests require the supported Rev1 ROM fingerprint."
}

$Scratch = Join-Path $ProjectRoot "build\gameplay-pretip-tests"
New-Item -ItemType Directory -Force -Path $Scratch | Out-Null
$Executable = Join-Path $ProjectRoot "build\tecmo_port.exe"
if ($Build) {
    $BuildLog = Join-Path $Scratch "build.log"
    & (Join-Path $ProjectRoot "build.ps1") *> $BuildLog
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed.`n$((Get-Content $BuildLog -Tail 40) -join "`n")"
    }
}
if (!(Test-Path -LiteralPath $Executable -PathType Leaf)) {
    throw "Native executable is missing. Pass -Build."
}

function Get-Entry {
    param([byte[]]$Bytes, [string]$Id)
    if ([Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -ne "TAP1") {
        throw "Malformed asset-pack header."
    }
    $Count = [BitConverter]::ToUInt32($Bytes, 16)
    $Directory = [BitConverter]::ToUInt64($Bytes, 20)
    for ($Index = 0; $Index -lt $Count; ++$Index) {
        $EntryOffset = [int]($Directory + [uint64]$Index * 128)
        $Name = [Text.Encoding]::ASCII.GetString($Bytes, $EntryOffset, 64).
            Split([char]0)[0]
        if ($Name -eq $Id) {
            return [pscustomobject]@{
                directory_offset = $EntryOffset
                pack_offset = [BitConverter]::ToUInt64($Bytes, $EntryOffset + 84)
                size = [BitConverter]::ToUInt64($Bytes, $EntryOffset + 92)
            }
        }
    }
    throw "Asset-pack entry '$Id' was not found."
}

function Get-Fnv32 {
    param([byte[]]$Bytes)
    [uint32]$Hash = 2166136261
    foreach ($Byte in $Bytes) {
        [uint64]$Product = [uint64]($Hash -bxor [uint32]$Byte) *
            [uint64]16777619
        $Hash = [uint32]($Product % [uint64]4294967296)
    }
    return $Hash.ToString("X8")
}

function Get-EntryBytes {
    param([byte[]]$Bytes, $Entry)
    $Result = New-Object byte[] ([int]$Entry.size)
    [Array]::Copy($Bytes, [int]$Entry.pack_offset,
                  $Result, 0, $Result.Length)
    return $Result
}

function Invoke-Native {
    param([string[]]$Arguments, [string]$LogName)
    $Log = Join-Path $Scratch $LogName
    & $Executable @Arguments *> $Log
    return [pscustomobject]@{
        code = $LASTEXITCODE
        text = (Get-Content -LiteralPath $Log -Raw)
        tail = ((Get-Content -LiteralPath $Log -Tail 30) -join "`n")
    }
}

function Assert-Rejected {
    param([string]$Pack, [string]$Label)
    $Run = Invoke-Native -Arguments @("--gameplay-pretip-test", $Pack) `
                         -LogName "$Label.log"
    if ($Run.code -eq 0 -or $Run.text -notmatch "TPTI-1") {
        throw "Malformed TPTI-1 pack '$Label' was accepted.`n$($Run.tail)"
    }
}

$PreviousPack = $env:TECMO_ASSETPACK
try {
$Pack = Join-Path $Scratch "tecmo.assetpack"
$BuildPack = Invoke-Native -Arguments @("--build-assetpack", $RomPath, $Pack) `
                           -LogName "pack-build.log"
if ($BuildPack.code -ne 0) {
    throw "TPTI-1 pack build failed.`n$($BuildPack.tail)"
}
$PackBytes = [IO.File]::ReadAllBytes($Pack)
$PreTip = Get-Entry $PackBytes "gameplay/pre-tip"
$SourceMap = Get-Entry $PackBytes "system/source-map"
if ($PreTip.size -ne 5888) { throw "TPTI-1 directory size changed." }
$Payload = Get-EntryBytes $PackBytes $PreTip
if ((Get-Fnv32 $Payload) -ne "99ADFE3D" -or
    [Text.Encoding]::ASCII.GetString($Payload, 0, 4) -ne "TPTI" -or
    [BitConverter]::ToUInt16($Payload, 4) -ne 1 -or
    [BitConverter]::ToUInt16($Payload, 6) -ne 256 -or
    [BitConverter]::ToUInt32($Payload, 8) -ne 5888 -or
    [BitConverter]::ToUInt16($Payload, 12) -ne 20 -or
    [BitConverter]::ToUInt16($Payload, 14) -ne 32 -or
    [BitConverter]::ToUInt32($Payload, 16) -ne 256 -or
    $Payload[20] -ne 0x15 -or $Payload[21] -ne 0x3B -or
    $Payload[22] -ne 0x7D -or $Payload[23] -ne 8 -or
    $Payload[176] -ne 0x82 -or $Payload[177] -ne 0xC1 -or
    $Payload[178] -ne 4 -or $Payload[179] -ne 9 -or
    $Payload[185] -ne 38 -or $Payload[186] -ne 4 -or
    $Payload[187] -ne 33 -or $Payload[188] -ne 25 -or
    $Payload[189] -ne 0 -or $Payload[190] -ne 0xC6 -or
    $Payload[191] -ne 0xFA) {
    throw "TPTI-1 canonical header changed."
}
if (@($Payload[192..255] | Where-Object { $_ -ne 0 }).Count -ne 0) {
    throw "TPTI-1 reserved header bytes are nonzero."
}
for ($Index = 0; $Index -lt 20; ++$Index) {
    $Record = 256 + $Index * 32
    if ([BitConverter]::ToUInt16($Payload, $Record) -ne $Index + 1 -or
        @($Payload[($Record + 28)..($Record + 31)] |
            Where-Object { $_ -ne 0 }).Count -ne 0 -or
        [BitConverter]::ToUInt64($Payload, $Record + 16) -eq 0) {
        throw "TPTI-1 source record $Index is malformed."
    }
}
$MapText = [Text.Encoding]::UTF8.GetString(
    (Get-EntryBytes $PackBytes $SourceMap))
$Map = $MapText | ConvertFrom-Json
$Mapped = @($Map.logical_entries | Where-Object id -eq "gameplay/pre-tip")
$ExpectedRoles = @(
    "blank-screen-descriptor","blank-screen-stream","blank-screen-palette",
    "presentation-screen-wait-helpers","matchup-sequence-and-team-text",
    "mode-and-versus-strings","mode-string-pointer-table",
    "character-to-tile-map","character-16px-metatile-table",
    "card-text-chr-selector-setup","tipoff-closeup-entry",
    "tipoff-closeup-palettes","tipoff-closeup-control",
    "tipoff-closeup-timing-and-lineup-tables",
    "fixed-d861-sprite-staging",
    "center-tip-object-setup",
    "center-tip-object-update","pregame-launch-bridge","live-handoff",
    "tipoff-orientation-select"
)
$TipSetupSource = @($Mapped[0].sources | Where-Object {
    $_.role -eq "center-tip-object-setup"
})
$TipInputSource = $Mapped[0].native_contract.tip_input_source
if ($Mapped.Count -ne 1 -or
    $Mapped[0].schema -ne "tecmo.gameplay-pre-tip/TPTI-1" -or
    @($Mapped[0].dependencies).Count -ne 5 -or
    (@($Mapped[0].sources.role) -join ",") -ne ($ExpectedRoles -join ",") -or
    @($Mapped[0].sources | Where-Object {
        $_.fingerprint_fnv1a32 -notmatch "^[0-9A-F]{8}$" -or
        $_.fingerprint_fnv1a64 -notmatch "^[0-9A-F]{16}$"
    }).Count -ne 0 -or
    $Mapped[0].native_contract.music -notmatch "track 8" -or
    $Mapped[0].native_contract.card_text -notmatch "2x2 metatiles" -or
    $Mapped[0].native_contract.cancel -notmatch "bit 0" -or
    $Mapped[0].native_contract.tip_lineup -notmatch "AC8C" -or
    $Mapped[0].native_contract.closeup_motion -notmatch "D861" -or
    $Mapped[0].native_contract.toss_cut_in -notmatch "nametable page 1" -or
    $Mapped[0].native_contract.ball_descent -notmatch "71..145" -or
    $Mapped[0].native_contract.tip_jumper_selectors.Count -ne 2 -or
    $Mapped[0].native_contract.tip_jumper_selectors[0] -ne 4 -or
    $Mapped[0].native_contract.tip_jumper_selectors[1] -ne 9 -or
    $Mapped[0].native_contract.tip_animation -notmatch
        "native 30-frame contest-input.*native 60-update.*projected altitude" -or
    $TipSetupSource.Count -ne 1 -or
    $TipInputSource.source_entry -ne "prg/bank05" -or
    [uint64]$TipInputSource.source_offset -ne
        ([uint64]$TipSetupSource[0].source_offset + 3) -or
    [int]$TipInputSource.bank -ne 5 -or
    [int]$TipInputSource.cpu_start -ne 0x985E -or
    [int]$TipInputSource.cpu_end -ne 0x986A -or
    [int]$TipInputSource.size -ne 13 -or
    $TipInputSource.fingerprint_fnv1a32 -ne "423816F1" -or
    $TipInputSource.fingerprint_fnv1a64 -ne "032F8A7A4F4439D1" -or
    -not [bool]$TipInputSource.rom_exact -or
    $TipInputSource.proves -notmatch "current-level NES B mask" -or
    $TipInputSource.does_not_prove -notmatch "winner settlement" -or
    $Mapped[0].native_contract.tip_input -notmatch
        "30 native jump-contest updates" -or
    $Mapped[0].native_contract.tip_input -notmatch "target frame 0" -or
    $Mapped[0].native_contract.winner_query_gate -notmatch
        "rejects before jump-contest.*caller output" -or
    $Mapped[0].native_contract.winner_policy -notmatch
        "exact original winner.*unported") {
    throw "TPTI-1 source-map provenance is incomplete or malformed."
}

$Self = Invoke-Native -Arguments @("--gameplay-pretip-test", $Pack) `
                      -LogName "self-test.log"
if ($Self.code -ne 0 -or $Self.text -notmatch "self-test passed") {
    throw "TPTI-1 self-test failed.`n$($Self.tail)"
}
$Scene = Invoke-Native -Arguments @("--gameplay-scene-test", $Pack) `
                       -LogName "scene-test.log"
if ($Scene.code -ne 0 -or $Scene.text -notmatch "SELF TEST PASS") {
    throw "TPTI-1 scene integration failed.`n$($Scene.tail)"
}
$Human = Invoke-Native -Arguments @(
    "--gameplay-pretip-human-checkpoint", $Pack
) -LogName "human-checkpoint.log"
if ($Human.code -ne 0 -or
    $Human.text -notmatch "TPTI-1 human checkpoint PASS frame=721 late-sample=29") {
    throw "TPTI-1 human-input frame-721 checkpoint failed.`n$($Human.tail)"
}

$env:TECMO_ASSETPACK = $Pack
$Modes = @(
    [pscustomobject]@{ mode="gameplay-start"; phase="preseason"; frame=0; hash="C40C940A6F8AA5FE36E3804022C993E1535AD1F1F14BE35C4F3F6C34C359347B" },
    [pscustomobject]@{ mode="gameplay-pretip-frame61"; phase="matchup"; frame=61; hash="602FB70C5E9711268DF0BDFD5F255BD85AD0CDC1087EE8B31E8ADDCF12882D29" },
    [pscustomobject]@{ mode="gameplay-pretip-frame182"; phase="first-period"; frame=182; hash="2377B0FF24274E21F5963CC35E43D0F666B7626E890A23C01A7621B842055F9A" },
    [pscustomobject]@{ mode="gameplay-pretip-frame211"; phase="first-period"; frame=211; hash="79DC564FFB41E197C415DA1482EA4D0662FE1EF510BE40C9BD9C94D1B40C577B" },
    [pscustomobject]@{ mode="gameplay-pretip-frame271"; phase="closeup"; frame=271; hash="A73F8C5E051EAE42462932DDE430FC50D1109BDAA1E7F96D2CE0EB22DAE36889" },
    [pscustomobject]@{ mode="gameplay-pretip-frame300"; phase="closeup"; frame=300; hash="7D3227F3D2256DBFA036F3C7761EB03A41C467C330E8A4097EBBD68D20DC45E1" },
    [pscustomobject]@{ mode="gameplay-pretip-frame330"; phase="closeup"; frame=330; hash="CF24E1A5BEFFB62DCA85304DBC739A11CABCAE50F112870669D7CCA4C2EBAC0B" },
    [pscustomobject]@{ mode="gameplay-pretip-frame481"; phase="ball-descent"; frame=481; hash="C22515C68D8E6F3E09855F186078EDDB37EB1149E7A5F68951517AA93FFB8C43" },
    [pscustomobject]@{ mode="gameplay-pretip-frame631"; phase="toss-closeup"; frame=631; hash="CDE4C17159C79207CA82281204547FD2794E81858A52A6FB312E937CEEDF162C" },
    [pscustomobject]@{ mode="gameplay-pretip-frame661"; phase="jump-contest"; frame=661; hash="3A9A6A48CA4E7650A2436C4AA4B6F1B6BCFEFCC1505F2AF53DB529EA06BBC0C5" },
    [pscustomobject]@{ mode="gameplay-pretip-frame662"; phase="jump-contest"; frame=662; hash="CBDA2DBAC7598E4AB9DCA0910A58193FE777510D7A4AC6A1FFD3539175375615" },
    [pscustomobject]@{ mode="gameplay-pretip-frame670"; phase="jump-contest"; frame=670; hash="736BB0330B3AD6762279392E2ADB22A7CA6026879D67B0DA6105E0238C132B8C" },
    [pscustomobject]@{ mode="gameplay-pretip-frame675"; phase="jump-contest"; frame=675; hash="AABD4E07F26AB3A38A602491F105682F066B0473DA3E79727254698191B3F2A6" },
    [pscustomobject]@{ mode="gameplay-pretip-frame680"; phase="jump-contest"; frame=680; hash="C323C49D63D9615C84F2F3743C79FFF32A1FE15479848899BC2729A057688CB7" },
    [pscustomobject]@{ mode="gameplay-pretip-frame690"; phase="jump-contest"; frame=690; hash="EF7A09D5D37B098B346A1CBF53D4F822ADD23B53E0D0283143E39F1CF2399CEE" },
    [pscustomobject]@{ mode="gameplay-pretip-frame696"; phase="jump-contest"; frame=696; hash="3A7048CF6BB7E8ADD411703D17C9B287EAEE83F436825C7BEAC111EEC84F4F6A" },
    [pscustomobject]@{ mode="gameplay-pretip-frame712"; phase="jump-contest"; frame=712; hash="27D24A4A710EAAFB907D13753A6AD4D8CA86E3ED65A4FC63DEC9E2DF7018F45D" },
    [pscustomobject]@{ mode="gameplay-pretip-frame720"; phase="jump-contest"; frame=720; hash="7F4CCED39202E6C213C811299D1DB0474DE54D657CDEC7A5A445B4F63E3FC1CC" },
    [pscustomobject]@{ mode="gameplay-pretip-bulls-pacers"; phase="jump-contest"; frame=661; hash="D35724DAD5420A3C09BDA51DEE8D2319CB33F88D103C77E3BD2BB84A053CDED6" },
    [pscustomobject]@{ mode="gameplay-tipoff-proof-frame661"; phase="jump-contest"; frame=661; hash="D35724DAD5420A3C09BDA51DEE8D2319CB33F88D103C77E3BD2BB84A053CDED6" },
    [pscustomobject]@{ mode="gameplay-tipoff-proof-frame668"; phase="jump-contest"; frame=668; hash="54CBFF1AE02235C66933397A0E09ACCD32FE3F68B1D26EEF2F5988156D4F121E" },
    [pscustomobject]@{ mode="gameplay-tipoff-proof-frame676"; phase="jump-contest"; frame=676; hash="F90E2328044DC6F56D705E654CAEAE4CC80D6330D12EB846B14E7931DB703053" },
    [pscustomobject]@{ mode="gameplay-tipoff-proof-frame686"; phase="jump-contest"; frame=686; hash="5915FE0073906193E619B3CC516D25C1B8D18F05FD84C6CFC103A90DCCA68059" },
    [pscustomobject]@{ mode="gameplay-tipoff-proof-frame687"; phase="jump-contest"; frame=687; hash="B2C594027F608B436099F31BDDF2D40D819385B7678C148BC147377D77EE0BCD" },
    [pscustomobject]@{ mode="gameplay-tipoff-proof-frame696"; phase="jump-contest"; frame=696; hash="BA2C0FE1DBE66F5BC60E4222BE860E1AC2A156388F36C20DD707607952188984" },
    [pscustomobject]@{ mode="gameplay-tipoff-proof-frame697"; phase="jump-contest"; frame=697; hash="5B70E59F52D1FE542B30F0D61D2BF2C6604A227DB2897D89B4C3A33334A7F014" },
    [pscustomobject]@{ mode="gameplay-tipoff-proof-frame712"; phase="jump-contest"; frame=712; hash="EF3DCC397A8D67ACD43DBD31FFC14B5568B544765FA11888759C16A8CC0BE7BE" },
    [pscustomobject]@{ mode="gameplay-tipoff-proof-frame713"; phase="jump-contest"; frame=713; hash="98877067298F8870047AC911BC56EE9676DA6360C87CEF2001FBD8761BF85007" },
    [pscustomobject]@{ mode="gameplay-tipoff-proof-frame720"; phase="jump-contest"; frame=720; hash="05AECDA2CD1CF9E0444CFC030C6F8A2C7E27128FCA53A84E4669B395E72F4607" },
    [pscustomobject]@{ mode="gameplay-tipoff-proof-frame721"; phase="live"; frame=721; hash="6CFC130D0A472BCB7877F1B48037BC1DD2DC7BB16D0460692060C699BE2C917A" },
    [pscustomobject]@{ mode="gameplay-tipoff-proof-frame725"; phase="live"; frame=725; hash="80419403637392438810992405944F50D1F9363744784A6F038D5F4F04613723" },
    [pscustomobject]@{ mode="gameplay-live-start"; phase="live"; frame=721; hash="DDE21802E85DD14AC85F8792CBB9694C0833E5DC103A1C567891B1501F6FA783" }
)
$RenderedHashes = @{}
foreach ($Spec in $Modes) {
    $Hashes = @()
    for ($Pass = 0; $Pass -lt 2; ++$Pass) {
        $Png = Join-Path $Scratch "$($Spec.mode)-$Pass.png"
        $Run = Invoke-Native -Arguments @(
            "--root", $ProjectRoot, "--render-test-mode", $Spec.mode, $Png
        ) -LogName "$($Spec.mode)-$Pass.log"
        if ($Run.code -ne 0 -or !(Test-Path -LiteralPath $Png) -or
            $Run.text -notmatch "frame=$($Spec.frame).*clock=3:00.*pretip=$($Spec.phase)") {
            throw "TPTI-1 render checkpoint '$($Spec.mode)' failed.`n$($Run.tail)"
        }
        $Hashes += (Get-FileHash -LiteralPath $Png -Algorithm SHA256).Hash
    }
    if ($Hashes[0] -ne $Hashes[1]) {
        throw "TPTI-1 render checkpoint '$($Spec.mode)' is nondeterministic."
    }
    if ($Hashes[0] -ne $Spec.hash) {
        throw "TPTI-1 render checkpoint '$($Spec.mode)' changed: $($Hashes[0])."
    }
    $RenderedHashes[$Spec.mode] = $Hashes[0]
}
$CloseupHashCount = @(
    @(
        $RenderedHashes["gameplay-pretip-frame271"],
        $RenderedHashes["gameplay-pretip-frame300"],
        $RenderedHashes["gameplay-pretip-frame330"]
    ) | Select-Object -Unique
).Count
if ($CloseupHashCount -ne 3) {
    throw "TPTI-1 close-up checkpoints 271/300/330 are not distinct."
}

$ReferenceComparisonMessage = ""
if (!$ReferenceRoot) {
    $CandidateReferenceRoot = Join-Path `
        (Split-Path -Parent $ProjectRoot) `
        "tecmo-basketball-port\temp-videos\gameplay-audit"
    if (Test-Path -LiteralPath $CandidateReferenceRoot -PathType Container) {
        $ReferenceRoot = $CandidateReferenceRoot
    }
}
if ($ReferenceRoot) {
    $ReferenceRoot = (Resolve-Path -LiteralPath $ReferenceRoot).Path
    Add-Type -AssemblyName System.Drawing
    $Pairs = @(
        [pscustomobject]@{ reference="tipoff_0450.png"; native="gameplay-pretip-frame211" },
        [pscustomobject]@{ reference="tipoff_0510.png"; native="gameplay-pretip-frame271" },
        [pscustomobject]@{ reference="tipoff_0540.png"; native="gameplay-pretip-frame300" },
        [pscustomobject]@{ reference="tipoff_0570.png"; native="gameplay-pretip-frame330" },
        [pscustomobject]@{ reference="tipoff_0720.png"; native="gameplay-pretip-bulls-pacers" }
    )
    $ComparisonPath = Join-Path $Scratch "reference-comparison.png"
    $Comparison = New-Object Drawing.Bitmap 1024,($Pairs.Count * 480)
    $Graphics = [Drawing.Graphics]::FromImage($Comparison)
    $Graphics.InterpolationMode =
        [Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
    $Graphics.PixelOffsetMode =
        [Drawing.Drawing2D.PixelOffsetMode]::Half
    try {
        for ($PairIndex = 0; $PairIndex -lt $Pairs.Count; ++$PairIndex) {
            $Pair = $Pairs[$PairIndex]
            $ReferencePath = Join-Path $ReferenceRoot $Pair.reference
            $NativePath = Join-Path $Scratch "$($Pair.native)-0.png"
            if (!(Test-Path -LiteralPath $ReferencePath -PathType Leaf) -or
                !(Test-Path -LiteralPath $NativePath -PathType Leaf)) {
                throw "TPTI-1 reference comparison input is missing."
            }
            $Reference = [Drawing.Bitmap]::FromFile($ReferencePath)
            $Native = [Drawing.Bitmap]::FromFile($NativePath)
            try {
                if ($Reference.Width -ne 256 -or $Reference.Height -ne 240 -or
                    $Native.Width -ne 640 -or $Native.Height -ne 480) {
                    throw "TPTI-1 reference comparison dimensions changed."
                }
                $Y = $PairIndex * 480
                $Graphics.DrawImage(
                    $Reference,
                    (New-Object Drawing.Rectangle 0,$Y,512,480),
                    0,0,256,240,[Drawing.GraphicsUnit]::Pixel)
                $Graphics.DrawImage(
                    $Native,
                    (New-Object Drawing.Rectangle 512,$Y,512,480),
                    64,0,512,480,[Drawing.GraphicsUnit]::Pixel)
                if ($PairIndex -eq 0) {
                    $MaskMismatch = 0
                    $VisiblePixels = 0
                    for ($YIndex = 0; $YIndex -lt 240; ++$YIndex) {
                        for ($XIndex = 0; $XIndex -lt 256; ++$XIndex) {
                            $ReferenceColor =
                                $Reference.GetPixel($XIndex, $YIndex)
                            $NativeColor =
                                $Native.GetPixel(64 + $XIndex * 2,
                                                 $YIndex * 2)
                            $ReferenceVisible =
                                $ReferenceColor.R -ne 0 -or
                                $ReferenceColor.G -ne 0 -or
                                $ReferenceColor.B -ne 0
                            $NativeVisible =
                                $NativeColor.R -ne 0 -or
                                $NativeColor.G -ne 0 -or
                                $NativeColor.B -ne 0
                            if ($ReferenceVisible) { ++$VisiblePixels }
                            if ($ReferenceVisible -ne $NativeVisible) {
                                ++$MaskMismatch
                            }
                        }
                    }
                    if ($VisiblePixels -ne 703 -or $MaskMismatch -ne 0) {
                        throw "Visible 1ST PERIOD frame no longer matches the reference mask."
                    }
                }
            } finally {
                $Reference.Dispose()
                $Native.Dispose()
            }
        }
        $Comparison.Save(
            $ComparisonPath, [Drawing.Imaging.ImageFormat]::Png)
        $ReferenceComparisonMessage =
            " reference-comparison=$ComparisonPath"
    } finally {
        $Graphics.Dispose()
        $Comparison.Dispose()
    }
}

$Cases = @(
    [pscustomobject]@{ label="tip-input-subspan"; mutate={
        param($Bytes) $Bytes[[int]$PreTip.pack_offset + 5091] =
            $Bytes[[int]$PreTip.pack_offset + 5091] -bxor 1
    }},
    [pscustomobject]@{ label="payload-mutation"; mutate={
        param($Bytes) $Bytes[[int]$PreTip.pack_offset + 3040] =
            $Bytes[[int]$PreTip.pack_offset + 3040] -bxor 1
    }},
    [pscustomobject]@{ label="reserved-byte"; mutate={
        param($Bytes) $Bytes[[int]$PreTip.pack_offset + 192] = 1
    }},
    [pscustomobject]@{ label="oversized-entry"; mutate={
        param($Bytes) [BitConverter]::GetBytes([uint64]5889).CopyTo(
            $Bytes, [int]$PreTip.directory_offset + 92)
    }},
    [pscustomobject]@{ label="missing-entry"; mutate={
        param($Bytes) $Name = [Text.Encoding]::ASCII.GetBytes("gameplay/pre-missing")
        [Array]::Copy($Name, 0, $Bytes, [int]$PreTip.directory_offset,
                      $Name.Length)
    }},
    [pscustomobject]@{ label="cross-pack-core"; mutate={
        param($Bytes) $Core = Get-Entry $Bytes "gameplay/core"
        $Bytes[[int]$Core.pack_offset] = $Bytes[[int]$Core.pack_offset] -bxor 1
    }},
    [pscustomobject]@{ label="cross-pack-team-data"; mutate={
        param($Bytes) $Team = Get-Entry $Bytes "menu/team-data"
        $Bytes[[int]$Team.pack_offset] = $Bytes[[int]$Team.pack_offset] -bxor 1
    }},
    [pscustomobject]@{ label="cross-pack-music"; mutate={
        param($Bytes) $Music = Get-Entry $Bytes "audio/music"
        $Bytes[[int]$Music.pack_offset] = $Bytes[[int]$Music.pack_offset] -bxor 1
    }},
    [pscustomobject]@{ label="cross-pack-closeup"; mutate={
        param($Bytes) $Closeup = Get-Entry $Bytes "arena/intro/warriors-transition"
        $Bytes[[int]$Closeup.pack_offset] =
            $Bytes[[int]$Closeup.pack_offset] -bxor 1
    }},
    [pscustomobject]@{ label="cross-pack-chr"; mutate={
        param($Bytes) $Chr = Get-Entry $Bytes "chr/all"
        $Bytes[[int]$Chr.pack_offset] = $Bytes[[int]$Chr.pack_offset] -bxor 1
    }}
)
foreach ($Case in $Cases) {
    $Bytes = [byte[]]$PackBytes.Clone()
    & $Case.mutate $Bytes
    $Path = Join-Path $Scratch "$($Case.label).assetpack"
    [IO.File]::WriteAllBytes($Path, $Bytes)
    Assert-Rejected -Pack $Path -Label $Case.label
}

$MutatedRom = Join-Path $Scratch "mutated-rev1.nes"
$RomBytes = [IO.File]::ReadAllBytes($RomPath)
$A125Offset = 16 + 6 * 0x4000 + (0xA125 - 0x8000)
$RomBytes[$A125Offset] = $RomBytes[$A125Offset] -bxor 1
[IO.File]::WriteAllBytes($MutatedRom, $RomBytes)
$MutatedPack = Join-Path $Scratch "mutated.assetpack"
$Mutation = Invoke-Native -Arguments @(
    "--build-assetpack", $MutatedRom, $MutatedPack
) -LogName "rom-mutation.log"
if ($Mutation.code -eq 0 -or $Mutation.text -notmatch "TPTI-1|Rev1") {
    throw "TPTI-1 Rev1 source mutation was accepted."
}

$global:LASTEXITCODE = 0
Write-Output ("TPTI-1 PRE-TIP TEST PASS: canonical/revision/FNV32+64/source-map " +
    "same-pack TGPL/TTDT/TMUS/TWAR/CHR missing/malformed/oversized/cross-pack " +
    "NES-B abort/freeze/track8-to-track5 scene integration and deterministic " +
    "preseason/matchup/visible-1ST-PERIOD/distinct-closeup/toss/jump/live renders" +
    $ReferenceComparisonMessage)
} finally {
    if ($null -eq $PreviousPack) {
        Remove-Item Env:TECMO_ASSETPACK -ErrorAction SilentlyContinue
    } else {
        $env:TECMO_ASSETPACK = $PreviousPack
    }
}
