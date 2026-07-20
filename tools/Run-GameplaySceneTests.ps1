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
if (!$RomPath) {
    $RomPath = $env:TECMO_ROM_PATH
}
if (!$RomPath -or !(Test-Path -LiteralPath $RomPath -PathType Leaf)) {
    throw "Pass -RomPath or set TECMO_ROM_PATH to the local Rev1 iNES ROM."
}
$RomPath = (Resolve-Path -LiteralPath $RomPath).Path
if ((Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash -ne
    $ExpectedRomSha256) {
    throw "Gameplay scene tests require the supported Rev1 ROM fingerprint."
}

$BuildDir = Join-Path $ProjectRoot "build"
$Executable = Join-Path $BuildDir "tecmo_port.exe"
$Scratch = [IO.Path]::GetFullPath((Join-Path $BuildDir "gameplay_scene_test"))
$BuildPrefix = [IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/') +
    [IO.Path]::DirectorySeparatorChar
if (!$Scratch.StartsWith($BuildPrefix,
                         [StringComparison]::OrdinalIgnoreCase)) {
    throw "Gameplay scene scratch path escaped build\."
}
$PackPath = Join-Path $Scratch "gameplay-scene.assetpack"
$PreviousPack = $env:TECMO_ASSETPACK
$PreviousSkipShortcut = $env:TECMO_SKIP_SHORTCUT

function Get-ShortTail {
    param([string]$Path)
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { return "" }
    return (@(Get-Content -LiteralPath $Path | Select-Object -Last 12) -join
        [Environment]::NewLine)
}

function Invoke-Logged {
    param(
        [string]$Command,
        [string[]]$Arguments,
        [string]$LogPath
    )
    & $Command @Arguments *> $LogPath
    return [pscustomobject]@{
        exit_code = $LASTEXITCODE
        tail = Get-ShortTail $LogPath
    }
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

function Assert-SceneRejected {
    param(
        [string]$AssetPack,
        [string]$Label,
        [string]$ExpectedStatus
    )
    $Log = Join-Path $Scratch ("reject-{0}.log" -f $Label)
    $Run = Invoke-Logged -Command $Executable -Arguments @(
        "--root", $ProjectRoot, "--gameplay-scene-test", $AssetPack
    ) -LogPath $Log
    if ($Run.exit_code -eq 0 -or
        $Run.tail -notmatch "Gameplay scene test failed" -or
        ($ExpectedStatus -and $Run.tail -notmatch $ExpectedStatus)) {
        throw "Gameplay scene accepted $Label fixture.`n$($Run.tail)"
    }
}

function Get-PngDimensions {
    param([string]$Path)
    $Bytes = [IO.File]::ReadAllBytes($Path)
    [byte[]]$Signature = 137,80,78,71,13,10,26,10
    if ($Bytes.Length -lt 24) { throw "PNG '$Path' is truncated." }
    for ($Index = 0; $Index -lt $Signature.Length; ++$Index) {
        if ($Bytes[$Index] -ne $Signature[$Index]) {
            throw "PNG '$Path' has an invalid signature."
        }
    }
    if ([Text.Encoding]::ASCII.GetString($Bytes, 12, 4) -ne "IHDR") {
        throw "PNG '$Path' has no leading IHDR chunk."
    }
    [uint32]$Width = ([uint32]$Bytes[16] -shl 24) -bor
        ([uint32]$Bytes[17] -shl 16) -bor
        ([uint32]$Bytes[18] -shl 8) -bor [uint32]$Bytes[19]
    [uint32]$Height = ([uint32]$Bytes[20] -shl 24) -bor
        ([uint32]$Bytes[21] -shl 16) -bor
        ([uint32]$Bytes[22] -shl 8) -bor [uint32]$Bytes[23]
    return [pscustomobject]@{ width = $Width; height = $Height }
}

function Invoke-RenderCheckpoint {
    param([string]$Mode, [string]$ExpectedState)
    $SafeName = $Mode -replace '[^A-Za-z0-9_-]', '_'
    $Hashes = @()
    for ($Pass = 1; $Pass -le 2; ++$Pass) {
        $Png = Join-Path $Scratch ("{0}-{1}.png" -f $SafeName, $Pass)
        $Log = Join-Path $Scratch ("render-{0}-{1}.log" -f $SafeName, $Pass)
        $Run = Invoke-Logged -Command $Executable -Arguments @(
            "--root", $ProjectRoot, "--render-test-mode", $Mode, $Png
        ) -LogPath $Log
        if ($Run.exit_code -ne 0 -or !(Test-Path -LiteralPath $Png) -or
            $Run.tail -notmatch $ExpectedState) {
            throw "Gameplay render '$Mode' failed.`n$($Run.tail)"
        }
        $Dimensions = Get-PngDimensions $Png
        if ($Dimensions.width -ne 640 -or $Dimensions.height -ne 480) {
            throw "Gameplay render '$Mode' is not 640x480."
        }
        $Hashes += (Get-FileHash -LiteralPath $Png -Algorithm SHA256).Hash
    }
    if ($Hashes[0] -ne $Hashes[1]) {
        throw "Gameplay render '$Mode' is not deterministic."
    }
    return $Hashes[0]
}

try {
    $env:TECMO_SKIP_SHORTCUT = "1"
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
    if ($Build) {
        $BuildLog = Join-Path $BuildDir "gameplay-scene-build.log"
        $BuildRun = Invoke-Logged `
            -Command (Join-Path $ProjectRoot "build.ps1") `
            -Arguments @() -LogPath $BuildLog
        if ($BuildRun.exit_code -ne 0 -or
            @(Select-String -LiteralPath $BuildLog `
                -Pattern 'warning [A-Z]+[0-9]+:').Count -ne 0) {
            throw "Warning-free gameplay scene build failed.`n$($BuildRun.tail)"
        }
    }
    if (!(Test-Path -LiteralPath $Executable -PathType Leaf)) {
        throw "Build output is missing; rerun with -Build."
    }
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
    New-Item -ItemType Directory -Path $Scratch | Out-Null

    $PackLog = Join-Path $Scratch "build-assetpack.log"
    $PackRun = Invoke-Logged -Command $Executable -Arguments @(
        "--build-assetpack", $RomPath, $PackPath
    ) -LogPath $PackLog
    if ($PackRun.exit_code -ne 0 -or
        !(Test-Path -LiteralPath $PackPath -PathType Leaf)) {
        throw "Strict gameplay asset-pack build failed.`n$($PackRun.tail)"
    }

    $PackBytes = [IO.File]::ReadAllBytes($PackPath)
    $Specs = @(
        [pscustomobject]@{ id="gameplay/core"; size=23416; hash="2047CCE0"; schema="tecmo.gameplay/TGPL-1" },
        [pscustomobject]@{ id="gameplay/court"; size=6559; hash="ECAB7A93"; schema="tecmo.gameplay-court/TGCT-1" },
        [pscustomobject]@{ id="gameplay/close-shots"; size=3144; hash="DACDC976"; schema="tecmo.gameplay-close-shots/TGCS-1" },
        [pscustomobject]@{ id="gameplay/dunk-cutaway"; size=20272; hash="E02F2D21"; schema="tecmo.gameplay-dunk-cutaway/TGDK-1" },
        [pscustomobject]@{ id="gameplay/jump-shots"; size=1648; hash="7587B099"; schema="tecmo.gameplay-jump-shots/TGJS-1" },
        [pscustomobject]@{ id="gameplay/shot-resolution"; size=384; hash="8486DB33"; schema="tecmo.gameplay-shot-resolution/TGSR-1" },
        [pscustomobject]@{ id="audio/music"; size=36784; hash="05C00ECB"; schema="tecmo.music/TMUS-1" },
        [pscustomobject]@{ id="audio/gameplay-sfx"; size=2824; hash="968A5DE6"; schema="tecmo.gameplay-audio/TSFX-1" },
        [pscustomobject]@{ id="audio/gameplay-dmc"; size=2515; hash="AD70E6E8"; schema="tecmo.gameplay-audio/TDMC-1" },
        [pscustomobject]@{ id="chr/all"; size=262144; hash="F6F6E854"; schema=$null }
    )
    $Entries = @{}
    foreach ($Spec in $Specs) {
        $Entry = Get-AssetPackEntry $PackBytes $Spec.id
        $Payload = Get-EntryBytes $PackBytes $Entry
        if ($Entry.byte_count -ne $Spec.size -or
            (Get-Fnv1a32 $Payload) -ne $Spec.hash) {
            throw "Asset '$($Spec.id)' size or canonical fingerprint changed."
        }
        $Entries[$Spec.id] = $Entry
    }
    $SourceMapEntry = Get-AssetPackEntry $PackBytes "system/source-map"
    $SourceMapText = [Text.Encoding]::UTF8.GetString(
        (Get-EntryBytes $PackBytes $SourceMapEntry))
    $SourceMap = $SourceMapText | ConvertFrom-Json
    foreach ($Spec in @($Specs | Where-Object { $_.schema })) {
        $Mapped = @($SourceMap.logical_entries | Where-Object {
            $_.id -eq $Spec.id
        })
        if ($Mapped.Count -ne 1 -or $Mapped[0].schema -ne $Spec.schema) {
            throw "Source-map provenance for '$($Spec.id)' is missing or malformed."
        }
    }

    $DunkLog = Join-Path $Scratch "dunk-cutaway-assets.log"
    $DunkRun = Invoke-Logged -Command $Executable -Arguments @(
        "--gameplay-dunk-cutaway-test", $PackPath
    ) -LogPath $DunkLog
    if ($DunkRun.exit_code -ne 0 -or
        $DunkRun.tail -notmatch "TGDK-1 dunk cutaway passed") {
        throw "Strict dunk-cutaway asset test failed.`n$($DunkRun.tail)"
    }

    $JumpLog = Join-Path $Scratch "jump-shot-assets.log"
    $JumpRun = Invoke-Logged -Command $Executable -Arguments @(
        "--gameplay-jump-shots-test", $PackPath
    ) -LogPath $JumpLog
    if ($JumpRun.exit_code -ne 0 -or
        $JumpRun.tail -notmatch "TGJS-1 jump-shot assets passed") {
        throw "Strict jump-shot asset test failed.`n$($JumpRun.tail)"
    }

    $SceneLog = Join-Path $Scratch "scene-self-test.log"
    $SceneRun = Invoke-Logged -Command $Executable -Arguments @(
        "--root", $ProjectRoot, "--gameplay-scene-test", $PackPath
    ) -LogPath $SceneLog
    if ($SceneRun.exit_code -ne 0 -or
        $SceneRun.tail.Trim() -ne "GAMEPLAY SCENE SELF TEST PASS") {
        throw "Native gameplay scene self-test failed.`n$($SceneRun.tail)"
    }

    $MissingPath = Join-Path $Scratch "missing-court.assetpack"
    $Missing = [byte[]]$PackBytes.Clone()
    $Missing[[int]$Entries["gameplay/court"].directory_offset] =
        [byte][char]'x'
    [IO.File]::WriteAllBytes($MissingPath, $Missing)
    Assert-SceneRejected -AssetPack $MissingPath -Label "missing-court"

    $MalformedPath = Join-Path $Scratch "malformed-close-shots.assetpack"
    $Malformed = [byte[]]$PackBytes.Clone()
    $Malformed[[int]$Entries["gameplay/close-shots"].pack_offset] =
        $Malformed[[int]$Entries["gameplay/close-shots"].pack_offset] -bxor 1
    [IO.File]::WriteAllBytes($MalformedPath, $Malformed)
    Assert-SceneRejected -AssetPack $MalformedPath `
        -Label "malformed-close-shots"

    $MissingDunkPath = Join-Path $Scratch "missing-dunk-cutaway.assetpack"
    $MissingDunk = [byte[]]$PackBytes.Clone()
    $MissingDunk[[int]$Entries["gameplay/dunk-cutaway"].directory_offset] =
        [byte][char]'x'
    [IO.File]::WriteAllBytes($MissingDunkPath, $MissingDunk)
    Assert-SceneRejected -AssetPack $MissingDunkPath `
        -Label "missing-dunk-cutaway"

    $MalformedDunkPath = Join-Path $Scratch "malformed-dunk-cutaway.assetpack"
    $MalformedDunk = [byte[]]$PackBytes.Clone()
    $DunkOffset = [int]$Entries["gameplay/dunk-cutaway"].pack_offset
    $MalformedDunk[$DunkOffset] = $MalformedDunk[$DunkOffset] -bxor 1
    [IO.File]::WriteAllBytes($MalformedDunkPath, $MalformedDunk)
    Assert-SceneRejected -AssetPack $MalformedDunkPath `
        -Label "malformed-dunk-cutaway"

    $OversizedDunkPath = Join-Path $Scratch "oversized-dunk-cutaway.assetpack"
    $OversizedDunk = [byte[]]$PackBytes.Clone()
    [BitConverter]::GetBytes([uint64]20273).CopyTo(
        $OversizedDunk,
        [int]$Entries["gameplay/dunk-cutaway"].directory_offset + 92)
    [IO.File]::WriteAllBytes($OversizedDunkPath, $OversizedDunk)
    Assert-SceneRejected -AssetPack $OversizedDunkPath `
        -Label "oversized-dunk-cutaway"

    $MissingJumpPath = Join-Path $Scratch "missing-jump-shots.assetpack"
    $MissingJump = [byte[]]$PackBytes.Clone()
    $MissingJump[[int]$Entries["gameplay/jump-shots"].directory_offset] =
        [byte][char]'x'
    [IO.File]::WriteAllBytes($MissingJumpPath, $MissingJump)
    Assert-SceneRejected -AssetPack $MissingJumpPath `
        -Label "missing-jump-shots"

    $MalformedJumpPath = Join-Path $Scratch "malformed-jump-shots.assetpack"
    $MalformedJump = [byte[]]$PackBytes.Clone()
    $MalformedJump[[int]$Entries["gameplay/jump-shots"].pack_offset] =
        $MalformedJump[[int]$Entries["gameplay/jump-shots"].pack_offset] `
            -bxor 1
    [IO.File]::WriteAllBytes($MalformedJumpPath, $MalformedJump)
    Assert-SceneRejected -AssetPack $MalformedJumpPath `
        -Label "malformed-jump-shots"

    $OversizedJumpPath = Join-Path $Scratch "oversized-jump-shots.assetpack"
    $OversizedJump = [byte[]]$PackBytes.Clone()
    [BitConverter]::GetBytes([uint64]1649).CopyTo(
        $OversizedJump,
        [int]$Entries["gameplay/jump-shots"].directory_offset + 92)
    [IO.File]::WriteAllBytes($OversizedJumpPath, $OversizedJump)
    Assert-SceneRejected -AssetPack $OversizedJumpPath `
        -Label "oversized-jump-shots"

    $MissingResolutionPath = Join-Path $Scratch "missing-shot-resolution.assetpack"
    $MissingResolution = [byte[]]$PackBytes.Clone()
    $MissingResolution[[int]$Entries["gameplay/shot-resolution"].directory_offset] =
        [byte][char]'x'
    [IO.File]::WriteAllBytes($MissingResolutionPath, $MissingResolution)
    Assert-SceneRejected -AssetPack $MissingResolutionPath `
        -Label "missing-shot-resolution" -ExpectedStatus "TGSR-1"

    $MalformedResolutionPath = Join-Path $Scratch "malformed-shot-resolution.assetpack"
    $MalformedResolution = [byte[]]$PackBytes.Clone()
    $ResolutionOffset = [int]$Entries["gameplay/shot-resolution"].pack_offset
    $MalformedResolution[$ResolutionOffset] =
        $MalformedResolution[$ResolutionOffset] -bxor 1
    [IO.File]::WriteAllBytes($MalformedResolutionPath, $MalformedResolution)
    Assert-SceneRejected -AssetPack $MalformedResolutionPath `
        -Label "malformed-shot-resolution" -ExpectedStatus "TGSR-1"

    $OversizedResolutionPath = Join-Path $Scratch "oversized-shot-resolution.assetpack"
    $OversizedResolution = [byte[]]$PackBytes.Clone()
    [BitConverter]::GetBytes([uint64]385).CopyTo(
        $OversizedResolution,
        [int]$Entries["gameplay/shot-resolution"].directory_offset + 92)
    [IO.File]::WriteAllBytes($OversizedResolutionPath, $OversizedResolution)
    Assert-SceneRejected -AssetPack $OversizedResolutionPath `
        -Label "oversized-shot-resolution" -ExpectedStatus "TGSR-1"

    $WrongResolutionPath = Join-Path $Scratch "wrong-revision-shot-resolution.assetpack"
    $WrongResolution = [byte[]]$PackBytes.Clone()
    $WrongResolution[$ResolutionOffset + 80] =
        $WrongResolution[$ResolutionOffset + 80] -bxor 1
    [IO.File]::WriteAllBytes($WrongResolutionPath, $WrongResolution)
    Assert-SceneRejected -AssetPack $WrongResolutionPath `
        -Label "wrong-revision-shot-resolution" -ExpectedStatus "TGSR-1"

    $CrossPackResolutionPath = Join-Path $Scratch "cross-pack-shot-resolution.assetpack"
    $CrossPackResolution = [byte[]]$PackBytes.Clone()
    $CoreOffset = [int]$Entries["gameplay/core"].pack_offset
    $CrossPackResolution[$CoreOffset + 128] =
        $CrossPackResolution[$CoreOffset + 128] -bxor 1
    [IO.File]::WriteAllBytes($CrossPackResolutionPath, $CrossPackResolution)
    Assert-SceneRejected -AssetPack $CrossPackResolutionPath `
        -Label "cross-pack-shot-resolution" -ExpectedStatus "TGSR-1"

    $OversizedPath = Join-Path $Scratch "oversized-core.assetpack"
    $Oversized = [byte[]]$PackBytes.Clone()
    [BitConverter]::GetBytes([uint64]23417).CopyTo(
        $Oversized, [int]$Entries["gameplay/core"].directory_offset + 92)
    [IO.File]::WriteAllBytes($OversizedPath, $Oversized)
    Assert-SceneRejected -AssetPack $OversizedPath -Label "oversized-core"

    $ChrMismatchPath = Join-Path $Scratch "chr-mismatch.assetpack"
    $ChrMismatch = [byte[]]$PackBytes.Clone()
    $ChrOffset = [int]$Entries["chr/all"].pack_offset
    $ChrMismatch[$ChrOffset] = $ChrMismatch[$ChrOffset] -bxor 1
    [IO.File]::WriteAllBytes($ChrMismatchPath, $ChrMismatch)
    Assert-SceneRejected -AssetPack $ChrMismatchPath -Label "chr-mismatch"

    $env:TECMO_ASSETPACK = $PackPath
    $RenderSpecs = @(
        [pscustomobject]@{ mode="gameplay-start"; state='gameplay-state frame=0 shot=none phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-frame1"; state='gameplay-state frame=1 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-frame2"; state='gameplay-state frame=2 shot=jump phase=live score=0/2' },
        [pscustomobject]@{ mode="gameplay-jump-frame4"; state='gameplay-state frame=4 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-frame21"; state='gameplay-state frame=21 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-frame22"; state='gameplay-state frame=22 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-frame39"; state='gameplay-state frame=39 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-frame40"; state='gameplay-state frame=40 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-frame45"; state='gameplay-state frame=45 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-frame46"; state='gameplay-state frame=46 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-frame72"; state='gameplay-state frame=72 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-frame73"; state='gameplay-state frame=73 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-frame74"; state='gameplay-state frame=74 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-frame75"; state='gameplay-state frame=75 shot=jump phase=live score=0/2' },
        [pscustomobject]@{ mode="gameplay-jump-frame86"; state='gameplay-state frame=86 shot=jump phase=live' },
        [pscustomobject]@{ mode="gameplay-jump-frame87"; state='gameplay-state frame=87 shot=none phase=live score=0/2' },
        [pscustomobject]@{ mode="gameplay-dunk-frame1"; state='gameplay-state frame=1 shot=dunk phase=live' },
        [pscustomobject]@{ mode="gameplay-dunk-frame8"; state='gameplay-state frame=8 shot=dunk phase=live' },
        [pscustomobject]@{ mode="gameplay-dunk-frame16"; state='gameplay-state frame=16 shot=dunk phase=live' },
        [pscustomobject]@{ mode="gameplay-close-shot-frame16"; state='gameplay-state frame=16 shot=dunk phase=live' },
        [pscustomobject]@{ mode="gameplay-dunk-frame24"; state='gameplay-state frame=24 shot=dunk phase=live' },
        [pscustomobject]@{ mode="gameplay-dunk-frame32"; state='gameplay-state frame=32 shot=dunk phase=live' },
        [pscustomobject]@{ mode="gameplay-dunk-frame48"; state='gameplay-state frame=48 shot=dunk phase=live' },
        [pscustomobject]@{ mode="gameplay-dunk-frame64"; state='gameplay-state frame=64 shot=dunk phase=live' },
        [pscustomobject]@{ mode="gameplay-dunk-frame80"; state='gameplay-state frame=80 shot=dunk phase=live' },
        [pscustomobject]@{ mode="gameplay-dunk-frame87"; state='gameplay-state frame=87 shot=dunk phase=live' },
        [pscustomobject]@{ mode="gameplay-dunk-frame132"; state='gameplay-state frame=132 shot=dunk phase=live' }
    )
    $RenderHashes = @{}
    foreach ($Spec in $RenderSpecs) {
        $RenderHashes[$Spec.mode] = Invoke-RenderCheckpoint `
            -Mode $Spec.mode -ExpectedState $Spec.state
    }
    $ExpectedJumpHashes = @{
        "gameplay-jump-frame2" =
            "E27AFD10FAFFA2C25033B0ECBCC2EBE9AF568AFDB0D8CB95983EEF03B8B0B889"
        "gameplay-jump-frame75" =
            "0D1A8B95CCF234FF7D83AF93BC4854952AFF447A4B09F61B60CE9EB9530F9816"
        "gameplay-jump-frame87" =
            "FA11CB1F0EEF6DA63A3534F61CE9EFF69BC1EAF86C5AA92A588C06F4D4875357"
    }
    foreach ($Mode in $ExpectedJumpHashes.Keys) {
        if ($RenderHashes[$Mode] -ne $ExpectedJumpHashes[$Mode]) {
            throw "Gameplay jump-miss render hash changed at '$Mode'."
        }
    }
    $VisualSentinels = @(
        $RenderHashes["gameplay-start"],
        $RenderHashes["gameplay-jump-frame21"],
        $RenderHashes["gameplay-dunk-frame16"],
        $RenderHashes["gameplay-dunk-frame32"],
        $RenderHashes["gameplay-dunk-frame64"],
        $RenderHashes["gameplay-dunk-frame80"]
    ) | Select-Object -Unique
    if ($VisualSentinels.Count -ne 6) {
        throw "Gameplay live/cutaway/black/return visual sentinels collapsed together."
    }
    if ($RenderHashes["gameplay-close-shot-frame16"] -ne
        $RenderHashes["gameplay-dunk-frame16"]) {
        throw "Legacy close-shot render mode diverged from canonical dunk mode."
    }

    $global:LASTEXITCODE = 0
    Write-Output ("GAMEPLAY SCENE TEST PASS: Rev1 full-pack provenance " +
        "scene controls TGDK TGJS TGSR jump-miss shots dunk-cutaway frame75/audio state " +
        "halftime/final render-hashes/determinism missing malformed oversized " +
        "chr-mismatch")
} finally {
    $env:TECMO_ASSETPACK = $PreviousPack
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
}
