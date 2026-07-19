param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [switch]$Build
)

$ErrorActionPreference = "Stop"

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
$BuildDir = Join-Path $ProjectRoot "build"
$Executable = Join-Path $BuildDir "tecmo_port.exe"
$Scratch = [IO.Path]::GetFullPath(
    (Join-Path $BuildDir "gameplay_dunk_cutaway_test"))
$BuildPrefix = [IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/') +
    [IO.Path]::DirectorySeparatorChar
if (!$Scratch.StartsWith($BuildPrefix,
                         [StringComparison]::OrdinalIgnoreCase)) {
    throw "Dunk-cutaway test scratch path escaped build\."
}
$PackPath = Join-Path $Scratch "dunk-cutaway.assetpack"
$ExpectedOutput =
    "TGDK-1 dunk cutaway passed: sources=18 cells=1920 stages=7 sides=2 palette=939EBCBE render=AA508365"
$PreviousSkipShortcut = $env:TECMO_SKIP_SHORTCUT

function Get-ShortTail {
    param([object[]]$Lines)
    return (@($Lines | Select-Object -Last 10) -join
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
            $ByteCount -gt [uint64]$Bytes.Length - $PackOffset) {
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
    [Array]::Copy($PackBytes, [int]$Entry.pack_offset,
                  $Result, 0, $Result.Length)
    return $Result
}

function Get-Slice {
    param([byte[]]$Bytes, [int]$Offset, [int]$Count)
    $Result = New-Object byte[] $Count
    [Array]::Copy($Bytes, $Offset, $Result, 0, $Count)
    return $Result
}

function Invoke-DunkAssetTest {
    param([string]$AssetPack, [bool]$ExpectSuccess)
    $Output = @(& $Executable --gameplay-dunk-cutaway-test $AssetPack 2>&1)
    $ExitCode = $LASTEXITCODE
    if ($ExpectSuccess) {
        if ($ExitCode -ne 0 -or
            ($Output -join [Environment]::NewLine).Trim() -ne
                $ExpectedOutput) {
            throw "TGDK-1 loader/render golden failed.`n$(Get-ShortTail $Output)"
        }
    } elseif ($ExitCode -eq 0 -or
              @($Output | Where-Object {
                  $_ -match "TGDK-1|Dunk-cutaway asset"
              }).Count -eq 0) {
        throw "Malformed TGDK-1 pack was accepted.`n$(Get-ShortTail $Output)"
    }
}

function Write-MutatedPayloadAndReject {
    param([byte[]]$Original, [object]$Entry,
          [string]$Id, [int]$PayloadOffset)
    $Path = Join-Path $Scratch ("payload-" + $Id + ".assetpack")
    $Bytes = [byte[]]$Original.Clone()
    $Absolute = [int]$Entry.pack_offset + $PayloadOffset
    $Bytes[$Absolute] = $Bytes[$Absolute] -bxor 1
    [IO.File]::WriteAllBytes($Path, $Bytes)
    Invoke-DunkAssetTest $Path $false
}

function Write-MissingEntryAndReject {
    param([byte[]]$Original, [object]$Entry, [string]$Id)
    $Path = Join-Path $Scratch ("missing-" + $Id + ".assetpack")
    $Bytes = [byte[]]$Original.Clone()
    $Bytes[[int]$Entry.directory_offset] = [byte][char]'x'
    [IO.File]::WriteAllBytes($Path, $Bytes)
    Invoke-DunkAssetTest $Path $false
}

function Write-WrongSizeAndReject {
    param([byte[]]$Original, [object]$Entry,
          [string]$Id, [uint64]$ByteCount)
    $Path = Join-Path $Scratch ("size-" + $Id + ".assetpack")
    $Bytes = [byte[]]$Original.Clone()
    [Array]::Copy([BitConverter]::GetBytes($ByteCount), 0, $Bytes,
                  [int]$Entry.directory_offset + 92, 8)
    [IO.File]::WriteAllBytes($Path, $Bytes)
    Invoke-DunkAssetTest $Path $false
}

function Invoke-RejectedRomMutation {
    param(
        [byte[]]$Original,
        [string]$Id,
        [int]$Offset,
        [string]$ExpectedRange,
        [bool]$SharedWithCloseShot = $false
    )
    $MutatedRom = Join-Path $Scratch ("rom-" + $Id + ".nes")
    $MutatedPack = Join-Path $Scratch ("rom-" + $Id + ".assetpack")
    $Bytes = [byte[]]$Original.Clone()
    $Bytes[$Offset] = $Bytes[$Offset] -bxor 1
    [IO.File]::WriteAllBytes($MutatedRom, $Bytes)
    $Output = @(& $Executable --build-assetpack `
        $MutatedRom $MutatedPack 2>&1)
    $ExitCode = $LASTEXITCODE
    if (Test-Path -LiteralPath $MutatedPack) {
        Remove-Item -LiteralPath $MutatedPack -Force
    }
    $Text = $Output -join [Environment]::NewLine
    $SchemaOk = $Text -match "TGDK-1"
    if ($SharedWithCloseShot) {
        $SchemaOk = $SchemaOk -or $Text -match "TGCS-1"
    }
    if ($ExitCode -eq 0 -or !$SchemaOk -or
        (!$SharedWithCloseShot -and
         $Text -notmatch [regex]::Escape($ExpectedRange))) {
        throw "Rev1 source mutation '$Id' was not rejected by its strict source contract.`n$(Get-ShortTail $Output)"
    }
}

try {
    $env:TECMO_SKIP_SHORTCUT = "1"
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
    if ($Build) {
        $BuildOutput = @(& (Join-Path $ProjectRoot "build.ps1") 2>&1)
        if ($LASTEXITCODE -ne 0 -or
            @($BuildOutput | Where-Object {
                $_ -match "warning [A-Z]+[0-9]+"
            }).Count -ne 0) {
            throw "Warning-free build failed.`n$(Get-ShortTail $BuildOutput)"
        }
    }
    if (!(Test-Path -LiteralPath $Executable -PathType Leaf)) {
        throw "Build output is missing; rerun with -Build."
    }
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
    New-Item -ItemType Directory -Path $Scratch | Out-Null

    $PackOutput = @(& $Executable --build-assetpack `
        $RomPath $PackPath 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "Rev1 TGDK-1 asset-pack build failed.`n$(Get-ShortTail $PackOutput)"
    }
    $PackBytes = [IO.File]::ReadAllBytes($PackPath)
    $DunkEntry = Get-AssetPackEntry $PackBytes "gameplay/dunk-cutaway"
    $ChrEntry = Get-AssetPackEntry $PackBytes "chr/all"
    $SourceMapEntry = Get-AssetPackEntry $PackBytes "system/source-map"
    $Payload = Get-EntryBytes $PackBytes $DunkEntry
    if ($DunkEntry.byte_count -ne 20272 -or
        (Get-Fnv1a32 $Payload) -ne "BDAF064E" -or
        (Get-Fnv1a32 (Get-Slice $Payload 0 256)) -ne "D29DA5EF" -or
        (Get-Fnv1a32 (Get-Slice $Payload 832 3929)) -ne "3BCCC0F0" -or
        (Get-Fnv1a32 (Get-Slice $Payload 4768 15360)) -ne "7BE1D71B" -or
        (Get-Fnv1a32 (Get-Slice $Payload 20128 32)) -ne "939EBCBE" -or
        (Get-Fnv1a32 (Get-Slice $Payload 20160 112)) -ne "84FFDAB7") {
        throw "gameplay/dunk-cutaway canonical section contract changed."
    }
    Invoke-DunkAssetTest $PackPath $true

    $ListOutput = @(& $Executable --assetpack-list $PackPath 2>&1)
    if ($LASTEXITCODE -ne 0 -or
        @($ListOutput | Where-Object {
            $_ -match '^gameplay/dunk-cutaway\s' -and
            $_ -match 'bank=5' -and $_ -match 'cpu=0x856B' -and
            $_ -match 'bytes=20272'
        }).Count -ne 1) {
        throw "Asset-pack listing omitted the exact TGDK-1 directory entry.`n$(Get-ShortTail $ListOutput)"
    }

    $SourceMapText = [Text.Encoding]::UTF8.GetString(
        (Get-EntryBytes $PackBytes $SourceMapEntry))
    $SourceMap = $SourceMapText | ConvertFrom-Json
    $DunkMaps = @($SourceMap.logical_entries | Where-Object {
        $_.id -eq "gameplay/dunk-cutaway"
    })
    $ExpectedSpans = @(
        @{ bank=5; start=0x856B; size=61; hash="1E295DA9" },
        @{ bank=5; start=0x85F3; size=78; hash="EE19230E" },
        @{ bank=7; start=0xE770; size=30; hash="DFBAA89C" },
        @{ bank=7; start=0xDCD2; size=7; hash="B0CE0D3D" },
        @{ bank=0; start=0x9022; size=805; hash="C2FF0BCA" },
        @{ bank=0; start=0x9346; size=16; hash="DEFFF0C1" },
        @{ bank=1; start=0xB002; size=137; hash="73DEE21D" },
        @{ bank=1; start=0xB08B; size=60; hash="BD5410FC" },
        @{ bank=1; start=0xB0C7; size=38; hash="0C95FF12" },
        @{ bank=7; start=0xC711; size=43; hash="AF434105" },
        @{ bank=7; start=0xCAF5; size=62; hash="798F7231" },
        @{ bank=7; start=0xCB33; size=62; hash="CB4B3C42" },
        @{ bank=7; start=0xCB71; size=62; hash="CD228EDD" },
        @{ bank=6; start=0xB37C; size=72; hash="0C7AA2F7" },
        @{ bank=6; start=0xB3C4; size=38; hash="C5920587" },
        @{ bank=6; start=0xB3EA; size=2130; hash="D9F05A73" },
        @{ bank=1; start=0xB0ED; size=107; hash="B621395D" },
        @{ bank=7; start=0xEB8D; size=121; hash="32E920E6" }
    )
    $MapOk = $DunkMaps.Count -eq 1
    if ($MapOk) {
        $Map = $DunkMaps[0]
        $MapOk = $Map.schema -eq
                "tecmo.gameplay-dunk-cutaway/TGDK-1" -and
            $Map.size -eq 20272 -and
            $Map.fingerprint_fnv1a32 -eq "BDAF064E" -and
            @($Map.dependencies).Count -eq 1 -and
            $Map.dependencies[0].entry -eq "chr/all" -and
            $Map.dependencies[0].size -eq 262144 -and
            $Map.dependencies[0].fingerprint_fnv1a32 -eq "F6F6E854" -and
            $Map.dependencies[0].fingerprint_fnv1a64 -eq
                "96A64F53B240ABB4" -and
            @($Map.source_spans).Count -eq $ExpectedSpans.Count -and
            $Map.decoded_screen.encoded_size -eq 805 -and
            $Map.decoded_screen.decoded_size -eq 2048 -and
            $Map.decoded_screen.fingerprint_fnv1a32 -eq "5414E3B6" -and
            (@($Map.decoded_screen.page_fingerprints_fnv1a32) -join ",") -eq
                "BA33539B,7DF03FAC" -and
            $Map.stages.count -eq 7 -and
            (@($Map.stages.assignment_frames) -join ",") -eq
                "27,32,37,42,47,52,57" -and
            (@($Map.stages.visible_frames) -join ",") -eq
                "28,33,38,43,48,53,58" -and
            (@($Map.geometry.record_counts[0]) -join ",") -eq
                "10,29,39,39,51,49,48" -and
            (@($Map.geometry.record_counts[1]) -join ",") -eq
                "10,29,39,39,51,49,47" -and
            $Map.reference_palette.profile -eq 1 -and
            $Map.reference_palette.uniform_color -eq 48 -and
            $Map.reference_palette.combined_fingerprint_fnv1a32 -eq
                "939EBCBE" -and
            $Map.timing.dispatch -eq 23 -and
            (@($Map.timing.initial_black) -join ",") -eq "24,27" -and
            (@($Map.timing.visible_cutaway) -join ",") -eq "28,62" -and
            $Map.timing.palette_black_with_staged_sprites -eq 63 -and
            $Map.timing.sprites_cleared -eq 64 -and
            (@($Map.timing.court_rebuild) -join ",") -eq "66,70" -and
            $Map.timing.live_return -eq 71 -and
            $Map.timing.route_resume -eq 75 -and
            $Map.timing.dunk_sequence_dmc -eq 87 -and
            $Map.timing.action_resolution -eq 132 -and
            $Map.runtime_inputs -match "no decompilation"
    }
    if ($MapOk) {
        for ($Index = 0; $Index -lt $ExpectedSpans.Count; ++$Index) {
            $Actual = $Map.source_spans[$Index]
            $Expected = $ExpectedSpans[$Index]
            if ($Actual.bank -ne $Expected.bank -or
                $Actual.cpu_start -ne $Expected.start -or
                $Actual.cpu_end -ne $Expected.start + $Expected.size - 1 -or
                $Actual.size -ne $Expected.size -or
                $Actual.fingerprint_fnv1a32 -ne $Expected.hash) {
                $MapOk = $false
                break
            }
        }
    }
    if (!$MapOk) {
        throw "TGDK-1 source-map provenance/timing contract changed."
    }

    @(
        @{ id="magic"; offset=0 },
        @{ id="header-reserved"; offset=255 },
        @{ id="source-record"; offset=256 },
        @{ id="raw-source"; offset=832 },
        @{ id="padding"; offset=4761 },
        @{ id="resolved-cell"; offset=4768 },
        @{ id="reference-palette"; offset=20128 },
        @{ id="stage"; offset=20160 }
    ) | ForEach-Object {
        Write-MutatedPayloadAndReject $PackBytes $DunkEntry $_.id $_.offset
    }
    Write-MissingEntryAndReject $PackBytes $DunkEntry "tgdk"
    Write-MissingEntryAndReject $PackBytes $ChrEntry "chr"
    Write-WrongSizeAndReject $PackBytes $DunkEntry "short" 20271
    Write-WrongSizeAndReject $PackBytes $DunkEntry "oversized" 20273
    Write-MutatedPayloadAndReject $PackBytes $ChrEntry "chr-byte" 0

    $RomBytes = [IO.File]::ReadAllBytes($RomPath)
    if ($RomBytes.Length -lt 16 -or
        [Text.Encoding]::ASCII.GetString($RomBytes, 0, 4) -ne
            "NES$([char]0x1A)" -or $RomBytes[4] -ne 8 -or
        $RomBytes[5] -ne 32) {
        throw "ROM is not the expected Rev1 8-PRG/32-CHR iNES layout."
    }
    $Trainer = if (($RomBytes[6] -band 4) -ne 0) { 512 } else { 0 }
    $PrgOffset = 16 + $Trainer
    $ChrOffset = $PrgOffset + [int]$RomBytes[4] * 16384
    for ($Index = 0; $Index -lt $ExpectedSpans.Count; ++$Index) {
        $Span = $ExpectedSpans[$Index]
        $Fixed = $Span.bank -eq 7 -and $Span.start -ge 0xC000
        $Base = if ($Fixed) { 0xC000 } else { 0x8000 }
        $Offset = $PrgOffset + $Span.bank * 0x4000 +
            ($Span.start - $Base)
        # $9346 is deliberately both the D9F6 terminator and palette byte 0;
        # mutate palette byte 1 so this check reaches the narrower palette span.
        if ($Index -eq 5) { ++$Offset }
        $Range = '${0:X4}-${1:X4}' -f
            $Span.start, ($Span.start + $Span.size - 1)
        Invoke-RejectedRomMutation $RomBytes ("source-{0}" -f $Index) `
            $Offset $Range ($Index -lt 2)
    }
    Invoke-RejectedRomMutation $RomBytes "chr-revision" `
        ($ChrOffset + 0xD0 * 1024) "CHR revision fingerprint" $false

    Write-Host $ExpectedOutput
    Write-Host "Gameplay dunk-cutaway focused tests passed."
    $global:LASTEXITCODE = 0
}
finally {
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
}
