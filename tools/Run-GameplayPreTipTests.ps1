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
    throw "TPTI-2 tests require the supported Rev1 ROM fingerprint."
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

Add-Type -AssemblyName System.Numerics
function Get-Fnv64 {
    param([byte[]]$Bytes)
    $Modulus = [System.Numerics.BigInteger]::One -shl 64
    $Hash = [System.Numerics.BigInteger]::Parse("14695981039346656037")
    $Prime = [System.Numerics.BigInteger]::Parse("1099511628211")
    foreach ($Byte in $Bytes) {
        $Hash = (($Hash -bxor [System.Numerics.BigInteger]$Byte) * $Prime) % $Modulus
    }
    return $Hash.ToString("X16")
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
    if ($Run.code -eq 0 -or $Run.text -notmatch "TPTI-2") {
        throw "Malformed TPTI-2 pack '$Label' was accepted.`n$($Run.tail)"
    }
}

$PreviousPack = $env:TECMO_ASSETPACK
try {
$Pack = Join-Path $Scratch "tecmo.assetpack"
$BuildPack = Invoke-Native -Arguments @("--build-assetpack", $RomPath, $Pack) `
                           -LogName "pack-build.log"
if ($BuildPack.code -ne 0) {
    throw "TPTI-2 pack build failed.`n$($BuildPack.tail)"
}
$PackBytes = [IO.File]::ReadAllBytes($Pack)
$PreTip = Get-Entry $PackBytes "gameplay/pre-tip"
$SourceMap = Get-Entry $PackBytes "system/source-map"
if ($PreTip.size -ne 7680) { throw "TPTI-2 directory size changed." }
$Payload = Get-EntryBytes $PackBytes $PreTip
if ((Get-Fnv32 $Payload) -ne "8E6367FC" -or
    (Get-Fnv64 $Payload) -ne "0D5FE9CF7B0A298DC" -or
    [Text.Encoding]::ASCII.GetString($Payload, 0, 4) -ne "TPTI" -or
    [BitConverter]::ToUInt16($Payload, 4) -ne 2 -or
    [BitConverter]::ToUInt16($Payload, 6) -ne 512 -or
    [BitConverter]::ToUInt32($Payload, 8) -ne 7680 -or
    [BitConverter]::ToUInt16($Payload, 12) -ne 29 -or
    [BitConverter]::ToUInt16($Payload, 14) -ne 32 -or
    [BitConverter]::ToUInt32($Payload, 16) -ne 512 -or
    $Payload[20] -ne 0x15 -or $Payload[21] -ne 0x3B -or
    $Payload[22] -ne 0x7D -or $Payload[23] -ne 8 -or
    $Payload[176] -ne 0x82 -or $Payload[177] -ne 0xC1 -or
    $Payload[178] -ne 4 -or $Payload[179] -ne 9 -or
    $Payload[185] -ne 38 -or $Payload[186] -ne 4 -or
    $Payload[187] -ne 33 -or $Payload[188] -ne 25 -or
    $Payload[189] -ne 0 -or $Payload[190] -ne 0xC6 -or
    $Payload[191] -ne 0xFA -or
    [BitConverter]::ToUInt16($Payload, 192) -ne 6 -or
    [BitConverter]::ToUInt16($Payload, 194) -ne 3 -or
    [BitConverter]::ToUInt32($Payload, 196) -ne 6560 -or
    [BitConverter]::ToUInt32($Payload, 200) -ne 352 -or
    [BitConverter]::ToUInt32($Payload, 204) -ne 7008 -or
    [BitConverter]::ToUInt32($Payload, 220) -ne 2776 -or
    [BitConverter]::ToUInt32($Payload, 224) -ne
        [Convert]::ToUInt32("A66EE873", 16) -or
    [BitConverter]::ToUInt16($Payload, 228) -ne 2 -or
    [BitConverter]::ToUInt16($Payload, 230) -ne 256 -or
    [BitConverter]::ToUInt32($Payload, 232) -ne
        [Convert]::ToUInt32("BE758F32", 16) -or
    [BitConverter]::ToUInt64($Payload, 236) -ne
        [Convert]::ToUInt64("A44321E848D881B2", 16)) {
    throw "TPTI-2 canonical header changed."
}
if (@($Payload[208..219] | Where-Object { $_ -ne 0 }).Count -ne 0 -or
    @($Payload[244..511] | Where-Object { $_ -ne 0 }).Count -ne 0) {
    throw "TPTI-2 reserved header bytes are nonzero."
}
for ($Index = 0; $Index -lt 29; ++$Index) {
    $Record = 512 + $Index * 32
    if ([BitConverter]::ToUInt16($Payload, $Record) -ne $Index + 1 -or
        @($Payload[($Record + 28)..($Record + 31)] |
            Where-Object { $_ -ne 0 }).Count -ne 0 -or
        [BitConverter]::ToUInt64($Payload, $Record + 16) -eq 0 -or
        [BitConverter]::ToUInt32($Payload, $Record + 24) -ge 7680) {
        throw "TPTI-2 source record $Index is malformed."
    }
}
if ([BitConverter]::ToUInt32($Payload, 512 + 20 * 32 + 24) -ne 4502 -or
    [BitConverter]::ToUInt32($Payload, 512 + 24 * 32 + 24) -ne 5699 -or
    [BitConverter]::ToUInt32($Payload, 512 + 21 * 32 + 24) -ne 7008 -or
    @($Payload[6912..7007] | Where-Object { $_ -ne 0 }).Count -ne 0 -or
    (Get-Fnv32 ($Payload[6656..6911])) -ne "93FCF6CB" -or
    (Get-Fnv64 ($Payload[6656..6911])) -ne "08407D4DA9578D56B") {
    throw "TPTI-2 exact-source overlap/padding contract changed."
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
    "center-tip-object-setup","later-general-collision-settlement",
    "pregame-launch-bridge","live-handoff",
    "tipoff-orientation-e537-e542-ordering",
    "b04-capture-error-exact-overlap","shared-actor-dispatcher",
    "automatic-actor-path","opposing-actor-dispatcher",
    "opposing-selected-actor-path","actor-jump-commit-state-0b",
    "slot10-claim-commit-state-17","e56e-one-byte-hook-anchor",
    "cd96-cdab-rng-mix"
)
$TipSetupSource = @($Mapped[0].sources | Where-Object {
    $_.role -eq "center-tip-object-setup"
})
$TipInputSource = $Mapped[0].native_contract.tip_input_source
if ($Mapped.Count -ne 1 -or
    $Mapped[0].schema -ne "tecmo.gameplay-pre-tip/TPTI-2" -or
    @($Mapped[0].dependencies).Count -ne 6 -or
    $Mapped[0].payload_size -ne 7680 -or
    $Mapped[0].payload_fingerprint_fnv1a32 -ne "8E6367FC" -or
    $Mapped[0].payload_fingerprint_fnv1a64 -ne "D5FE9CF7B0A298DC" -or
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
        "independent.*ballistic.*class-specific phases 2/3/4" -or
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
    $TipInputSource.proves -notmatch "985E.*986A.*current-B" -or
    $TipInputSource.proves -match "98E1|030C" -or
    $TipInputSource.does_not_prove -notmatch "98E1.*030C" -or
    $Mapped[0].native_contract.tip_input -notmatch
        "strict evolving-height.*no synthetic target frame" -or
    [int]$Mapped[0].native_contract.tpti_mechanics.version -ne 3 -or
    $Mapped[0].native_contract.tpti_mechanics.trajectory -notmatch
        "BDF7.*B32C.*AA84.*B500.*B678" -or
    $Mapped[0].native_contract.winner_query_gate -notmatch
        "rejects before jump-contest.*resolved.*nondeferred.*nonstalled" -or
    $Mapped[0].native_contract.winner_policy -notmatch
        "selector_00.*team ownership.*incomplete") {
    throw "TPTI-2 source-map provenance is incomplete or malformed."
}

$Self = Invoke-Native -Arguments @("--gameplay-pretip-test", $Pack) `
                      -LogName "self-test.log"
if ($Self.code -ne 0 -or $Self.text -notmatch "self-test passed") {
    throw "TPTI-2 self-test failed.`n$($Self.tail)"
}
$Scene = Invoke-Native -Arguments @("--gameplay-scene-test", $Pack) `
                       -LogName "scene-test.log"
if ($Scene.code -ne 0 -or $Scene.text -notmatch "SELF TEST PASS") {
    throw "TPTI-2 scene integration failed.`n$($Scene.tail)"
}
$Human = Invoke-Native -Arguments @(
    "--gameplay-pretip-human-checkpoint", $Pack
) -LogName "human-checkpoint.log"
if ($Human.code -ne 0 -or
    $Human.text -notmatch "TPTI-2 human checkpoint PASS capture-frame=452 simulation-frame=481 cinematic-frame=508 live-frame=598") {
    throw "TPTI-2 human-input timing checkpoint failed.`n$($Human.tail)"
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
    [pscustomobject]@{ mode="gameplay-pretip-frame481"; phase="ball-descent"; frame=481; hash="4043E63FCDDBCB3DED1B3E0D5EF8B088E7175BFFAC80DA99128B49CD34955C56" },
    [pscustomobject]@{ mode="gameplay-tipoff-proof-frame452"; phase="center-setup"; frame=452; hash="2377B0FF24274E21F5963CC35E43D0F666B7626E890A23C01A7621B842055F9A" },
    [pscustomobject]@{ mode="gameplay-tipoff-proof-frame481"; phase="ball-descent"; frame=481; hash="AEE825A9C7AF8E3790C73BF20438E7B99E01C8F002DBE628EE9455F567A5C487" },
    [pscustomobject]@{ mode="gameplay-tipoff-proof-frame483"; phase="ball-descent"; frame=483; hash="DEC93E0A49A8FFBC8A56F50E0542D0FBDF6605D59E609DB0658676ABCCB42105" },
    [pscustomobject]@{ mode="gameplay-tipoff-proof-frame490"; phase="ball-descent"; frame=490; hash="388239D56CC2EBDA8A33E58D288220C8D4F949A3576352807EC8BDCF444DBA6C" },
    [pscustomobject]@{ mode="gameplay-tipoff-proof-frame496"; phase="ball-descent"; frame=496; hash="DD3D1F489192E7188BBA52FF4CC0A4EBA2EDF2DDB44EB39EBAC562B1BAD36923" },
    [pscustomobject]@{ mode="gameplay-tipoff-proof-frame497"; phase="ball-descent"; frame=497; hash="2AB5A84AE89A786541DA8BD85FBB35040EA874C449137A1F275BB0036D65FD35" },
    [pscustomobject]@{ mode="gameplay-tipoff-proof-frame498"; phase="toss-closeup"; frame=498; hash="CDE4C17159C79207CA82281204547FD2794E81858A52A6FB312E937CEEDF162C" },
    [pscustomobject]@{ mode="gameplay-tipoff-proof-frame530"; phase="toss-closeup"; frame=530; hash="CDE4C17159C79207CA82281204547FD2794E81858A52A6FB312E937CEEDF162C" },
    [pscustomobject]@{ mode="gameplay-tipoff-proof-frame557"; phase="toss-closeup"; frame=557; hash="CDE4C17159C79207CA82281204547FD2794E81858A52A6FB312E937CEEDF162C" },
    [pscustomobject]@{ mode="gameplay-tipoff-proof-frame558"; phase="jump-contest"; frame=558; hash="68215183DD30E6B7EBAC8BE73C74A3811B6AB58C61EA30B3C3C93D4EC17A127F" },
    [pscustomobject]@{ mode="gameplay-tipoff-proof-frame559"; phase="jump-contest"; frame=559; hash="A01B57C51D93D178B9E8A09FF5B4AC42B6F866783CD90FC7F43916293D88E17E" },
    [pscustomobject]@{ mode="gameplay-tipoff-proof-frame580"; phase="jump-contest"; frame=580; hash="37E090FB6EBCA1F0D069CCD4624BA810B8762E5253380A0E2C5F0C69E188F1EB" },
    [pscustomobject]@{ mode="gameplay-tipoff-proof-frame597"; phase="live"; frame=597; hash="6B74C70F1C5309AACB9BFD305859A5DB3257941990BE57B5EA9D04992E0613C6" }
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
            throw "TPTI-2 render checkpoint '$($Spec.mode)' failed.`n$($Run.tail)"
        }
        $Hashes += (Get-FileHash -LiteralPath $Png -Algorithm SHA256).Hash
    }
    if ($Hashes[0] -ne $Hashes[1]) {
        throw "TPTI-2 render checkpoint '$($Spec.mode)' is nondeterministic."
    }
    if ($Hashes[0] -ne $Spec.hash) {
        throw "TPTI-2 render checkpoint '$($Spec.mode)' changed: $($Hashes[0])."
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
    throw "TPTI-2 close-up checkpoints 271/300/330 are not distinct."
}

$ReferenceComparisonMessage = ""
if (!$ReferenceRoot) {
    $CandidateReferenceRoot = Join-Path `
        (Split-Path -Parent $ProjectRoot) `
        "tecmo-basketball-port\temp-videos\gameplay-audit"
    $CandidateReferenceFiles = @(
        "tipoff_0450.png", "tipoff_0510.png", "tipoff_0540.png",
        "tipoff_0570.png", "tipoff_0720.png"
    )
    $CandidateReferenceMissing = @(
        $CandidateReferenceFiles | Where-Object {
            $CandidateReferencePath = Join-Path $CandidateReferenceRoot $_
            !(Test-Path -LiteralPath $CandidateReferencePath -PathType Leaf)
        }
    )
    if ((Test-Path -LiteralPath $CandidateReferenceRoot -PathType Container) -and
        $CandidateReferenceMissing.Count -eq 0) {
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
        [pscustomobject]@{ reference="tipoff_0720.png"; native="gameplay-tipoff-proof-frame481" }
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
                throw "TPTI-2 reference comparison input is missing."
            }
            $Reference = [Drawing.Bitmap]::FromFile($ReferencePath)
            $Native = [Drawing.Bitmap]::FromFile($NativePath)
            try {
                if ($Reference.Width -ne 256 -or $Reference.Height -ne 240 -or
                    $Native.Width -ne 640 -or $Native.Height -ne 480) {
                    throw "TPTI-2 reference comparison dimensions changed."
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
        param($Bytes) $Bytes[[int]$PreTip.pack_offset + 5699] =
            $Bytes[[int]$PreTip.pack_offset + 5699] -bxor 1
    }},
    [pscustomobject]@{ label="payload-mutation"; mutate={
        param($Bytes) $Bytes[[int]$PreTip.pack_offset + 3040] =
            $Bytes[[int]$PreTip.pack_offset + 3040] -bxor 1
    }},
    [pscustomobject]@{ label="reserved-byte"; mutate={
        param($Bytes) $Bytes[[int]$PreTip.pack_offset + 208] = 1
    }},
    [pscustomobject]@{ label="wrong-padding"; mutate={
        param($Bytes) $Bytes[[int]$PreTip.pack_offset + 6688] = 1
    }},
    [pscustomobject]@{ label="stale-tpti1-header"; mutate={
        param($Bytes)
        [BitConverter]::GetBytes([uint16]1).CopyTo(
            $Bytes, [int]$PreTip.pack_offset + 4)
        [BitConverter]::GetBytes([uint16]256).CopyTo(
            $Bytes, [int]$PreTip.pack_offset + 6)
        [BitConverter]::GetBytes([uint32]5888).CopyTo(
            $Bytes, [int]$PreTip.pack_offset + 8)
    }},
    [pscustomobject]@{ label="source-alias"; mutate={
        param($Bytes) [BitConverter]::GetBytes([uint32]4502).CopyTo(
            $Bytes, [int]$PreTip.pack_offset + 512 + 21 * 32 + 24)
    }},
    [pscustomobject]@{ label="source-bounds"; mutate={
        param($Bytes) [BitConverter]::GetBytes([uint32]7680).CopyTo(
            $Bytes, [int]$PreTip.pack_offset + 512 + 21 * 32 + 24)
    }},
    [pscustomobject]@{ label="mechanics-a2d1-non-hook"; mutate={
        param($Bytes) [BitConverter]::GetBytes([uint16]0xA2D1).CopyTo(
            $Bytes, [int]$PreTip.pack_offset + 6560 + 22)
    }},
    [pscustomobject]@{ label="false-friend-8642"; mutate={
        param($Bytes) [BitConverter]::GetBytes([uint16]0x8642).CopyTo(
            $Bytes, [int]$PreTip.pack_offset + 512 + 21 * 32 + 4)
    }},
    [pscustomobject]@{ label="orientation-e537-e542-ordering"; mutate={
        param($Bytes) [BitConverter]::GetBytes([uint16]0xE56D).CopyTo(
            $Bytes, [int]$PreTip.pack_offset + 512 + 19 * 32 + 6)
    }},
    [pscustomobject]@{ label="recurring-e56e-count"; mutate={
        param($Bytes) [BitConverter]::GetBytes([uint16]2).CopyTo(
            $Bytes, [int]$PreTip.pack_offset + 6560 + 30)
    }},
    [pscustomobject]@{ label="oversized-entry"; mutate={
        param($Bytes) [BitConverter]::GetBytes([uint64]7681).CopyTo(
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
    [pscustomobject]@{ label="cross-pack-jump-shots"; mutate={
        param($Bytes) $JumpShots = Get-Entry $Bytes "gameplay/jump-shots"
        $Bytes[[int]$JumpShots.pack_offset] =
            $Bytes[[int]$JumpShots.pack_offset] -bxor 1
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
if ($Mutation.code -eq 0 -or $Mutation.text -notmatch "TPTI-2|Rev1") {
    throw "TPTI-2 Rev1 source mutation was accepted."
}

$global:LASTEXITCODE = 0
Write-Output ("TPTI-2 PRE-TIP TEST PASS: canonical/revision/FNV32+64/source-map " +
    "same-pack TGPL/TTDT/TMUS/TWAR/TGJS-2/CHR missing/malformed/oversized/cross-pack " +
    "stale-TPTI-1 header, 8642-false-friend, A2D1-non-hook, E537-E542-ordering, " +
    "recurring-E56E-count, overlap/bounds/padding, " +
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
