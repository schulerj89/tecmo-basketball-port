param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [switch]$Build
)

$ErrorActionPreference = "Stop"
$ExpectedRomSha256 =
    "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4"

if (!$ProjectRoot) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if (!$RomPath) { $RomPath = $env:TECMO_ROM_PATH }
if (!$RomPath -or !(Test-Path -LiteralPath $RomPath -PathType Leaf)) {
    throw "Pass -RomPath or set TECMO_ROM_PATH to the local Rev1 iNES ROM."
}
$RomPath = (Resolve-Path -LiteralPath $RomPath).Path
if ((Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash -ne
    $ExpectedRomSha256) {
    throw "TGRB-1 tests require the exact Tecmo NBA Basketball Rev1 ROM."
}

$BuildDir = Join-Path $ProjectRoot "build"
$Executable = Join-Path $BuildDir "tecmo_port.exe"
$Scratch = [IO.Path]::GetFullPath(
    (Join-Path $BuildDir "gameplay_rebound_audit_test"))
$BuildPrefix = [IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/') +
    [IO.Path]::DirectorySeparatorChar
if (!$Scratch.StartsWith($BuildPrefix,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw "Gameplay rebound-audit scratch path escaped build\."
}
$PackPath = Join-Path $Scratch "gameplay-rebound-audit.assetpack"
$ProofPath = Join-Path $Scratch "claimant-settlement.png"
$ProofJsonPath = Join-Path $Scratch "claimant-settlement.jsonl"
$PreviousSkipShortcut = $env:TECMO_SKIP_SHORTCUT

function Get-ShortTail {
    param([object[]]$Lines)
    return (@($Lines | Select-Object -Last 12) -join [Environment]::NewLine)
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
    $Count = [BitConverter]::ToUInt32($Bytes, 16)
    $Directory = [BitConverter]::ToUInt64($Bytes, 20)
    if ($Directory -gt [uint64]$Bytes.Length -or
        [uint64]$Count * 128 -gt [uint64]$Bytes.Length - $Directory) {
        throw "Asset pack directory is out of bounds."
    }
    for ($Index = 0; $Index -lt $Count; ++$Index) {
        $Offset = [int]$Directory + $Index * 128
        $Terminator = [Array]::IndexOf($Bytes, [byte]0, $Offset, 64)
        if ($Terminator -lt 0) { $Terminator = $Offset + 64 }
        $EntryId = [Text.Encoding]::ASCII.GetString(
            $Bytes, $Offset, $Terminator - $Offset)
        if ($EntryId -eq $Id) {
            return [pscustomobject]@{
                directory_offset = $Offset
                pack_offset = [BitConverter]::ToUInt64($Bytes, $Offset + 84)
                byte_count = [BitConverter]::ToUInt64($Bytes, $Offset + 92)
            }
        }
    }
    throw "Asset pack entry '$Id' was not found."
}

function Invoke-ReboundAudit {
    param([string]$AssetPack, [bool]$ExpectSuccess)
    $Output = @(& $Executable --gameplay-rebound-audit-test `
        $AssetPack $RomPath 2>&1)
    $Text = ($Output -join [Environment]::NewLine).Trim()
    if ($ExpectSuccess) {
        if ($LASTEXITCODE -ne 0 -or $Text -notmatch
            '^TGRB-1 rebound audit passed: strict A977/B6E5/BA56/C042/CC00 provenance;') {
            throw "TGRB-1 focused resolver/importer test failed.`n$(Get-ShortTail $Output)"
        }
    } elseif ($LASTEXITCODE -eq 0 -or
              $Text -notmatch 'Rebound audit test failed:') {
        throw "Malformed TGRB-1 descriptor was accepted.`n$(Get-ShortTail $Output)"
    }
}

try {
    $env:TECMO_SKIP_SHORTCUT = "1"
    if ($Build) {
        $BuildOutput = @(& (Join-Path $ProjectRoot "build.ps1") 2>&1)
        if ($LASTEXITCODE -ne 0 -or
            @($BuildOutput | Where-Object {
                $_ -match 'warning [A-Z]+[0-9]+:'
            }).Count -ne 0) {
            throw "Warning-free TGRB-1 build failed.`n$(Get-ShortTail $BuildOutput)"
        }
    }
    if (!(Test-Path -LiteralPath $Executable -PathType Leaf)) {
        throw "Build output is missing; rerun with -Build."
    }
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
    New-Item -ItemType Directory -Path $Scratch | Out-Null

    $PackOutput = @(& $Executable --build-assetpack $RomPath $PackPath 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "TGRB-1 Rev1 asset-pack build failed.`n$(Get-ShortTail $PackOutput)"
    }
    Invoke-ReboundAudit $PackPath $true

    $PackBytes = [IO.File]::ReadAllBytes($PackPath)
    $Entry = Get-AssetPackEntry $PackBytes "gameplay/rebound-audit"
    if ($Entry.byte_count -ne 816 -or
        $Entry.pack_offset -gt [uint64]$PackBytes.Length -or
        [uint64]$Entry.byte_count -gt
            [uint64]$PackBytes.Length - $Entry.pack_offset) {
        throw "TGRB-1 pack entry range/size changed."
    }
    $Payload = New-Object byte[] ([int]$Entry.byte_count)
    [Array]::Copy($PackBytes, [int64]$Entry.pack_offset,
        $Payload, 0, [int64]$Entry.byte_count)
    if ([Text.Encoding]::ASCII.GetString($Payload, 0, 4) -ne "TGRB" -or
        [BitConverter]::ToUInt16($Payload, 4) -ne 1 -or
        [BitConverter]::ToUInt32($Payload, 8) -ne 816 -or
        [BitConverter]::ToUInt16($Payload, 12) -ne 5 -or
        [BitConverter]::ToUInt32($Payload, 20) -ne 328 -or
        [BitConverter]::ToUInt32($Payload, 24) -ne 488 -or
        $Payload[28] -ne 3 -or $Payload[29] -ne 128 -or
        $Payload[40] -ne 8 -or $Payload[41] -ne 192) {
        throw "TGRB-1 strict header/gate/counter-plane contract changed."
    }

    $SourceMapEntry = Get-AssetPackEntry $PackBytes "system/source-map"
    if ($SourceMapEntry.pack_offset -gt [uint64]$PackBytes.Length -or
        [uint64]$SourceMapEntry.byte_count -gt
            [uint64]$PackBytes.Length - $SourceMapEntry.pack_offset) {
        throw "TGRB-1 source-map entry range is invalid."
    }
    $SourceMapBytes = New-Object byte[] ([int]$SourceMapEntry.byte_count)
    [Array]::Copy($PackBytes, [int64]$SourceMapEntry.pack_offset,
        $SourceMapBytes, 0, [int64]$SourceMapEntry.byte_count)
    $SourceMap = ([Text.Encoding]::UTF8.GetString($SourceMapBytes) |
        ConvertFrom-Json)
    $SourceEntry = @($SourceMap.logical_entries | Where-Object {
        $_.id -eq "gameplay/rebound-audit"
    })
    if ($SourceEntry.Count -ne 1 -or
        $SourceEntry[0].schema -ne "tecmo.gameplay-rebound-audit/TGRB-1" -or
        $SourceEntry[0].fingerprint_fnv1a32 -ne "D6363FBD" -or
        $SourceEntry[0].fingerprint_fnv1a64 -ne "B6B95695306094BD" -or
        @($SourceEntry[0].source_spans).Count -ne 5 -or
        $SourceEntry[0].source_spans[0].cpu_start -ne 43241 -or
        $SourceEntry[0].source_spans[2].cpu_start -ne 47702 -or
        $SourceEntry[0].source_spans[3].cpu_start -ne 49218 -or
        $SourceEntry[0].source_spans[4].cpu_start -ne 52224 -or
        [bool]$SourceEntry[0].native_contract.ledger_write_enabled -or
        [bool]$SourceEntry[0].native_contract.implemented_ledger_coverage_bit8 -or
        $SourceEntry[0].native_contract.team_data_rebounds -ne "---") {
        throw "TGRB-1 source-map provenance or fail-closed contract changed."
    }

    # Independent parser-descriptor mutation: the CLI also performs a direct
    # in-memory local-ROM byte mutation through its optional ROM argument.
    $MutatedPath = Join-Path $Scratch "mutated-descriptor.assetpack"
    $Mutated = [byte[]]$PackBytes.Clone()
    $Mutated[[int]$Entry.pack_offset + 128 + 4] =
        $Mutated[[int]$Entry.pack_offset + 128 + 4] -bxor 1
    [IO.File]::WriteAllBytes($MutatedPath, $Mutated)
    Invoke-ReboundAudit $MutatedPath $false

    & $Executable --root $ProjectRoot --gameplay-live-foundation-proof `
        $PackPath claimant-settlement $ProofPath *> $ProofJsonPath
    if ($LASTEXITCODE -ne 0 -or
        !(Test-Path -LiteralPath $ProofPath -PathType Leaf) -or
        (Get-Item -LiteralPath $ProofPath).Length -eq 0) {
        throw "TGRB-1 passive claimant proof did not produce its screenshot."
    }
    $Proof = (Get-Content -LiteralPath $ProofJsonPath -Raw | ConvertFrom-Json)
    $Audit = $Proof.rebound_audit
    if ($Audit.contract -ne "TGRB-1" -or
        [bool]$Audit.ledger_write_enabled -or
        [bool]$Audit.coverage_bit8 -or
        [bool]$Audit.scene_ledger_coverage_bit8 -or
        [bool]$Audit.scene_ledger_rebounds_nonzero -or
        ![bool]$Audit.assets_available -or
        [bool]$Audit.raw_ba_available -or
        [bool]$Audit.raw_0588_available -or
        [bool]$Audit.be_bf_identity_fresh -or
        ![bool]$Audit.claimant_bridge_observed -or
        [bool]$Audit.source_gate_eligible -or
        $Audit.decision -ne "raw-ba-unavailable") {
        throw "TGRB-1 TGLP negative proof stopped being fail-closed."
    }

    Write-Host (
        "TGRB-1 focused tests passed: exact Rev1 source/descriptor mutation " +
        "rejection, raw BA/0588/BE-BF/claimant serial gates, non-emitting " +
        "ledger invariant, TEAM DATA rebound placeholder coverage, and " +
        "deterministic claimant-bridge JSONL/screenshot diagnostics.")
    $global:LASTEXITCODE = 0
} finally {
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    if (Test-Path -LiteralPath $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
}
