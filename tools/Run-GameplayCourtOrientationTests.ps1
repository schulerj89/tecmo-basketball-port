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
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $RomPath).Hash -ne
    $ExpectedRomSha256) {
    throw "TGOR-1 tests require the exact Tecmo NBA Basketball Rev1 ROM."
}

$BuildDir = Join-Path $ProjectRoot "build"
$Executable = Join-Path $BuildDir "tecmo_port.exe"
$Scratch = [IO.Path]::GetFullPath(
    (Join-Path $BuildDir "gameplay_court_orientation_test"))
$BuildPrefix = [IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/') +
    [IO.Path]::DirectorySeparatorChar
if (!$Scratch.StartsWith(
        $BuildPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Court-orientation scratch path escaped build\."
}
$PackPath = Join-Path $Scratch "court-orientation.assetpack"
$PreviousSkipShortcut = $env:TECMO_SKIP_SHORTCUT

function Get-ShortTail {
    param([string]$Path)
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { return "" }
    return (@(Get-Content -LiteralPath $Path | Select-Object -Last 10) -join
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

function Assert-OrientationResult {
    param(
        [string]$AssetPack,
        [string]$Label,
        [bool]$ExpectSuccess,
        [string]$SourceRom = $null,
        [string]$ExpectedFailure = $null
    )
    $Log = Join-Path $Scratch ("orientation-{0}.log" -f $Label)
    $Arguments = @("--gameplay-court-orientation-test", $AssetPack)
    if ($SourceRom) { $Arguments += $SourceRom }
    $Run = Invoke-Logged -Command $Executable -Arguments $Arguments `
        -LogPath $Log
    if ($ExpectSuccess) {
        if ($Run.exit_code -ne 0 -or
            $Run.tail.Trim() -ne
                "TGOR-1 court orientation passed: targets=00A0/0260 y=94 transitions=2 direction=0") {
            throw "TGOR-1 golden test failed.`n$($Run.tail)"
        }
    } elseif ($Run.exit_code -eq 0 -or
              ($ExpectedFailure -and
               $Run.tail -notmatch [regex]::Escape($ExpectedFailure))) {
        throw "TGOR-1 accepted $Label fixture.`n$($Run.tail)"
    }
}

function Assert-SceneRejected {
    param([string]$AssetPack, [string]$Label)
    $Log = Join-Path $Scratch ("scene-{0}.log" -f $Label)
    $Run = Invoke-Logged -Command $Executable -Arguments @(
        "--root", $ProjectRoot, "--gameplay-scene-test", $AssetPack
    ) -LogPath $Log
    if ($Run.exit_code -eq 0 -or
        $Run.tail -notmatch "Gameplay scene test failed") {
        throw "Gameplay scene accepted $Label fixture.`n$($Run.tail)"
    }
}

try {
    $env:TECMO_SKIP_SHORTCUT = "1"
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
    if ($Build) {
        $BuildLog = Join-Path $BuildDir "court-orientation-build.log"
        $BuildRun = Invoke-Logged `
            -Command (Join-Path $ProjectRoot "build.ps1") `
            -Arguments @() -LogPath $BuildLog
        if ($BuildRun.exit_code -ne 0 -or
            @(Select-String -LiteralPath $BuildLog `
                -Pattern 'warning [A-Z]+[0-9]+:').Count -ne 0) {
            throw "Warning-free TGOR-1 build failed.`n$($BuildRun.tail)"
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
        throw "Strict TGOR-1 pack build failed.`n$($PackRun.tail)"
    }

    $PackBytes = [IO.File]::ReadAllBytes($PackPath)
    $OrientationEntry =
        Get-AssetPackEntry $PackBytes "gameplay/court-orientation"
    $CoreEntry = Get-AssetPackEntry $PackBytes "gameplay/core"
    $ResolutionEntry =
        Get-AssetPackEntry $PackBytes "gameplay/shot-resolution"
    $SourceMapEntry = Get-AssetPackEntry $PackBytes "system/source-map"
    $Payload = Get-EntryBytes $PackBytes $OrientationEntry
    if ($OrientationEntry.byte_count -ne 640 -or
        (Get-Fnv1a32 $Payload) -ne "44B0C44E" -or
        [Text.Encoding]::ASCII.GetString($Payload, 0, 4) -ne "TGOR" -or
        [BitConverter]::ToUInt16($Payload, 4) -ne 1 -or
        [BitConverter]::ToUInt32($Payload, 8) -ne 640 -or
        [BitConverter]::ToUInt16($Payload, 12) -ne 4 -or
        [BitConverter]::ToUInt16($Payload, 128) -ne 0x00A0 -or
        [BitConverter]::ToUInt16($Payload, 130) -ne 0x0260 -or
        $Payload[132] -ne 0x94 -or $Payload[135] -ne 0x10 -or
        $Payload[136] -ne 0x17 -or $Payload[137] -ne 0x1B -or
        $Payload[138] -ne 0x2E) {
        throw "TGOR-1 canonical header or payload changed."
    }
    $ExpectedSources = @(
        [pscustomobject]@{ kind=1; bank=5; start=0x8FAD; end=0x8FE7; size=59; hash="7C94E5EA"; offset=384 },
        [pscustomobject]@{ kind=2; bank=5; start=0x9042; end=0x9053; size=18; hash="CE6C9466"; offset=448 },
        [pscustomobject]@{ kind=3; bank=5; start=0x9054; end=0x90AF; size=92; hash="FE092D62"; offset=480 },
        [pscustomobject]@{ kind=4; bank=5; start=0xBDEF; end=0xBDF2; size=4; hash="A27B0F6F"; offset=576 }
    )
    for ($Index = 0; $Index -lt $ExpectedSources.Count; ++$Index) {
        $Expected = $ExpectedSources[$Index]
        $Record = 256 + $Index * 32
        if ([BitConverter]::ToUInt16($Payload, $Record) -ne $Expected.kind -or
            $Payload[$Record + 2] -ne $Expected.bank -or
            [BitConverter]::ToUInt16($Payload, $Record + 4) -ne
                $Expected.start -or
            [BitConverter]::ToUInt16($Payload, $Record + 6) -ne
                $Expected.end -or
            [BitConverter]::ToUInt32($Payload, $Record + 8) -ne
                $Expected.size -or
            ("{0:X8}" -f
                [BitConverter]::ToUInt32($Payload, $Record + 12)) -ne
                $Expected.hash -or
            [BitConverter]::ToUInt32($Payload, $Record + 16) -ne
                $Expected.offset) {
            throw "TGOR-1 source record $Index changed."
        }
    }

    $SourceMap = [Text.Encoding]::UTF8.GetString(
        (Get-EntryBytes $PackBytes $SourceMapEntry)) | ConvertFrom-Json
    $Mapped = @($SourceMap.logical_entries | Where-Object {
        $_.id -eq "gameplay/court-orientation"
    })
    if ($Mapped.Count -ne 1 -or
        $Mapped[0].schema -ne "tecmo.gameplay-court-orientation/TGOR-1" -or
        $Mapped[0].dependencies.Count -ne 2 -or
        $Mapped[0].dependencies[0].id -ne "gameplay/core" -or
        $Mapped[0].dependencies[0].payload_fingerprint_fnv1a32 -ne
            "2047CCE0" -or
        $Mapped[0].dependencies[1].id -ne "gameplay/shot-resolution" -or
        $Mapped[0].dependencies[1].payload_fingerprint_fnv1a32 -ne
            "5376E82B" -or
        $Mapped[0].sources.Count -ne 4 -or
        $Mapped[0].sources[0].role -ne
            "possession-transition-gate-and-swap" -or
        $Mapped[0].state_contract.fresh_launch.direction -ne 0 -or
        $Mapped[0].state_contract.fresh_launch.tracked_possession -ne
            "away" -or
        (@($Mapped[0].state_contract.fresh_launch.offensive_hoop) -join
            ',') -ne "160,148" -or
        (@($Mapped[0].state_contract.hoop_landmarks.left) -join
            ',') -ne "160,148" -or
        (@($Mapped[0].state_contract.hoop_landmarks.right) -join
            ',') -ne "608,148" -or
        $Mapped[0].state_contract.hoop_landmarks.coordinate_space -notmatch
            "upper-left" -or
        $Mapped[0].evidence_limits -notmatch "no direct reads" -or
        $Mapped[0].supported_boundary -notmatch
            "production full-hoop selection for TGCP" -or
        $Mapped[0].supported_boundary -notmatch
            'flight Y=\$8F remains a separate endpoint' -or
        $Mapped[0].supported_boundary -notmatch
            "production TGFL-1 orientation selection" -or
        $Mapped[0].supported_boundary -notmatch
            "TGFL-1 remains the coordinate/pose-data owner") {
        throw "TGOR-1 source-map provenance or boundary changed."
    }

    Assert-OrientationResult -AssetPack $PackPath -Label "golden" `
        -ExpectSuccess $true -SourceRom $RomPath
    $SceneGoldenLog = Join-Path $Scratch "scene-golden.log"
    $SceneGolden = Invoke-Logged -Command $Executable -Arguments @(
        "--root", $ProjectRoot, "--gameplay-scene-test", $PackPath
    ) -LogPath $SceneGoldenLog
    if ($SceneGolden.exit_code -ne 0 -or
        $SceneGolden.tail.Trim() -ne "GAMEPLAY SCENE SELF TEST PASS") {
        throw "TGOR-1 scene handoff/restart golden failed.`n$($SceneGolden.tail)"
    }

    $Fixtures = @(
        [pscustomobject]@{ label="missing"; kind="directory"; offset=0; value=0; failure="entry missing or wrong-sized" },
        [pscustomobject]@{ label="oversized"; kind="size"; offset=0; value=641; failure="entry missing or wrong-sized" },
        [pscustomobject]@{ label="malformed-header"; kind="payload"; offset=0; value=1; failure="header/size/reserved contract rejected" },
        [pscustomobject]@{ label="reserved"; kind="payload"; offset=140; value=1; failure="header/size/reserved contract rejected" },
        [pscustomobject]@{ label="source-record"; kind="payload"; offset=256; value=1; failure="canonical source contract rejected" },
        [pscustomobject]@{ label="descriptor-bounds"; kind="payload"; offset=80; value=1; failure="header/size/reserved contract rejected" },
        [pscustomobject]@{ label="padding"; kind="payload"; offset=443; value=1; failure="canonical source contract rejected" }
    )
    foreach ($Fixture in $Fixtures) {
        $Mutated = [byte[]]$PackBytes.Clone()
        if ($Fixture.kind -eq "directory") {
            $Mutated[[int]$OrientationEntry.directory_offset] =
                [byte][char]'x'
        } elseif ($Fixture.kind -eq "size") {
            [BitConverter]::GetBytes([uint64]$Fixture.value).CopyTo(
                $Mutated, [int]$OrientationEntry.directory_offset + 92)
        } else {
            $Position = [int]$OrientationEntry.pack_offset + $Fixture.offset
            $Mutated[$Position] = $Mutated[$Position] -bxor
                [byte]$Fixture.value
        }
        $FixturePath = Join-Path $Scratch (
            "{0}.assetpack" -f $Fixture.label)
        [IO.File]::WriteAllBytes($FixturePath, $Mutated)
        Assert-OrientationResult -AssetPack $FixturePath `
            -Label $Fixture.label -ExpectSuccess $false `
            -ExpectedFailure $Fixture.failure
        if ($Fixture.label -eq "missing" -or
            $Fixture.label -eq "malformed-header") {
            Assert-SceneRejected -AssetPack $FixturePath `
                -Label $Fixture.label
        }
    }

    foreach ($Dependency in @(
        [pscustomobject]@{ label="missing-core"; entry=$CoreEntry; missing=$true },
        [pscustomobject]@{ label="crosspack-core"; entry=$CoreEntry; missing=$false },
        [pscustomobject]@{ label="missing-resolution"; entry=$ResolutionEntry; missing=$true },
        [pscustomobject]@{ label="crosspack-resolution"; entry=$ResolutionEntry; missing=$false }
    )) {
        $Mutated = [byte[]]$PackBytes.Clone()
        if ($Dependency.missing) {
            $Mutated[[int]$Dependency.entry.directory_offset] =
                [byte][char]'x'
            $ExpectedFailure = "dependency missing or wrong-sized"
        } else {
            $Position = [int]$Dependency.entry.pack_offset
            $Mutated[$Position] = $Mutated[$Position] -bxor 1
            $ExpectedFailure = "dependency contract rejected"
        }
        $FixturePath = Join-Path $Scratch (
            "{0}.assetpack" -f $Dependency.label)
        [IO.File]::WriteAllBytes($FixturePath, $Mutated)
        Assert-OrientationResult -AssetPack $FixturePath `
            -Label $Dependency.label -ExpectSuccess $false `
            -ExpectedFailure $ExpectedFailure
    }

    $RomBytes = [IO.File]::ReadAllBytes($RomPath)
    foreach ($Source in $ExpectedSources) {
        $Base = 16 + 5 * 16384 + ($Source.start - 0x8000)
        $Middle = [int]([math]::Floor([double]$Source.size / 2.0))
        $Last = ([int]$Source.size) - 1
        foreach ($Point in @(0, $Middle, $Last)) {
            $MutatedRom = [byte[]]$RomBytes.Clone()
            $MutatedRom[$Base + $Point] =
                $MutatedRom[$Base + $Point] -bxor 1
            $Label = "rom-{0:X4}-{1}" -f $Source.start, $Point
            $FixturePath = Join-Path $Scratch ("{0}.nes" -f $Label)
            [IO.File]::WriteAllBytes($FixturePath, $MutatedRom)
            Assert-OrientationResult -AssetPack $PackPath -Label $Label `
                -ExpectSuccess $false -SourceRom $FixturePath `
                -ExpectedFailure "source test failed"
        }
    }

    Write-Host (
        "TGOR-1 focused tests passed: canonical asset/state, provenance, " +
        "scene integration, strict malformed/cross-pack rejection, and " +
        "12 bounded ROM mutations.")
    $global:LASTEXITCODE = 0
} finally {
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
}
