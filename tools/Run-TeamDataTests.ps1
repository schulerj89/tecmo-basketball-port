param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [string]$DecompRoot,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path $ProjectRoot).Path
$BuildDir = Join-Path $ProjectRoot "build"
$Executable = Join-Path $BuildDir "tecmo_port.exe"

if (!$RomPath) {
    $RomPath = $env:TECMO_ROM_PATH
}
if (!$RomPath) {
    $RomPath = Join-Path (Split-Path -Parent $ProjectRoot) `
        "disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes"
}
if (!(Test-Path $RomPath)) {
    throw "Rev1 ROM not found. Pass -RomPath or set TECMO_ROM_PATH."
}
$RomPath = (Resolve-Path $RomPath).Path

if (!$DecompRoot) {
    $DecompRoot = $env:TECMO_DECOMP_ROOT
}
if (!$DecompRoot) {
    $DecompRoot = Join-Path (Split-Path -Parent $ProjectRoot) `
        "disassem\tecmo-basketball-decompilation"
}
if (!(Test-Path $DecompRoot)) {
    throw "Decompilation root not found. Pass -DecompRoot or set TECMO_DECOMP_ROOT."
}
$DecompRoot = (Resolve-Path $DecompRoot).Path

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
$Scratch = Join-Path $BuildDir ("team_data_test_" + [Guid]::NewGuid().ToString("N"))
$Scratch = [System.IO.Path]::GetFullPath($Scratch)
$BuildPrefix = [System.IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/') + `
    [System.IO.Path]::DirectorySeparatorChar
if (!$Scratch.StartsWith($BuildPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "TEAM DATA scratch path escaped build\."
}
New-Item -ItemType Directory -Path $Scratch | Out-Null

$PreviousSkipShortcut = $env:TECMO_SKIP_SHORTCUT
$PreviousAssetPack = $env:TECMO_ASSETPACK

function Invoke-Tecmo {
    param(
        [string[]]$Arguments,
        [int]$ExpectedExit = 0
    )
    $Output = @(& $Executable @Arguments 2>&1)
    $ExitCode = $LASTEXITCODE
    if ($ExitCode -ne $ExpectedExit) {
        $Tail = @($Output | Select-Object -Last 12) -join [Environment]::NewLine
        throw "tecmo_port exit $ExitCode (expected $ExpectedExit):$([Environment]::NewLine)$Tail"
    }
    return ($Output -join [Environment]::NewLine)
}

function Get-PackEntry {
    param([byte[]]$Bytes, [string]$Id)
    if ($Bytes.Length -lt 40 -or
        [Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -ne "TAP1") {
        throw "Private asset pack header was malformed."
    }
    $EntryCount = [BitConverter]::ToUInt32($Bytes, 16)
    $DirectoryOffset = [BitConverter]::ToUInt64($Bytes, 20)
    for ($Index = 0; $Index -lt $EntryCount; ++$Index) {
        $EntryOffset = [uint64]$DirectoryOffset + [uint64]$Index * 128
        if ($EntryOffset + 128 -gt $Bytes.Length) {
            throw "Private asset pack directory was truncated."
        }
        $EntryId = [Text.Encoding]::ASCII.GetString(
            $Bytes, [int]$EntryOffset, 64).Trim([char]0)
        if ($EntryId -eq $Id) {
            $PayloadOffset = [BitConverter]::ToUInt64(
                $Bytes, [int]$EntryOffset + 84)
            $PayloadSize = [BitConverter]::ToUInt64(
                $Bytes, [int]$EntryOffset + 92)
            if ($PayloadOffset -lt 40 -or
                $PayloadOffset -gt $DirectoryOffset -or
                $PayloadSize -gt $DirectoryOffset - $PayloadOffset) {
                throw "$Id payload bounds were malformed."
            }
            return @{
                Offset = $PayloadOffset
                Size = $PayloadSize
            }
        }
    }
    throw "$Id was missing from the private asset pack."
}

try {
    if (!$SkipBuild) {
        $env:TECMO_SKIP_SHORTCUT = "1"
        $BuildLog = Join-Path $Scratch "build.log"
        @(& (Join-Path $ProjectRoot "build.ps1") 2>&1) | Set-Content $BuildLog
        if ($LASTEXITCODE -ne 0) {
            throw "Warning-clean build failed:`n$(@(Get-Content $BuildLog -Tail 30) -join "`n")"
        }
        if (Select-String -Path $BuildLog -Pattern 'warning C\d+' -Quiet) {
            throw "Warning-clean build emitted a compiler warning."
        }
    }
    if (!(Test-Path $Executable)) {
        throw "Executable not found at $Executable."
    }

    $Pack = Join-Path $Scratch "team-data.assetpack"
    [void](Invoke-Tecmo @("--build-assetpack", $RomPath, $Pack))
    $PackBytes = [IO.File]::ReadAllBytes($Pack)
    $SourceMap = Get-PackEntry $PackBytes "system/source-map"
    $Chr = Get-PackEntry $PackBytes "chr/all"
    $TeamData = Get-PackEntry $PackBytes "menu/team-data"
    if ($SourceMap.Size -eq 0 -or $Chr.Size -ne 262144 -or
        $TeamData.Size -ne 96372) {
        throw "Required source-map/CHR/TTDT-1 directory contracts were rejected."
    }
    $env:TECMO_ASSETPACK = $Pack

    $FlowOutput = Invoke-Tecmo @("--root", $DecompRoot, "--flow-test")
    if ($FlowOutput -notmatch 'FLOW TEST PASS') {
        throw "TEAM DATA state/input/all-star/transition flow did not pass."
    }

    $Checkpoints = @(
        @{ mode = "team-data-select"; hash = "A5AD7ACA359B812D2A17F9BA916D5EF8787B5376FA097879635C204DD16895C8"; status = "palette=3 render=1" },
        @{ mode = "team-data-profile"; hash = "C889D530748689585BBED1E21C9A34F26EA5A8B8D7ADFDF83AD1FEDEF45ADA81"; status = "phase=TEAM PROFILE" },
        @{ mode = "team-data-player-detail"; hash = "B9982C582D687CFC7B5439AE5B4B369421C5874754291F686D8EFFE3D4FAD6A3"; status = "phase=PLAYER DETAIL" },
        @{ mode = "team-data-entry-transition-frame0"; hash = "2377B0FF24274E21F5963CC35E43D0F666B7626E890A23C01A7621B842055F9A"; status = "transition-frame=0 palette=4 render=0" },
        @{ mode = "team-data-entry-transition-frame4"; hash = "2377B0FF24274E21F5963CC35E43D0F666B7626E890A23C01A7621B842055F9A"; status = "transition-frame=4 palette=4 render=1" },
        @{ mode = "team-data-entry-transition-frame7"; hash = "3EBDAFA0CB536A9A24D37D4866666C7A04C7B4D645160FA4D372CBEFA5B5457D"; status = "transition-frame=7 palette=0 render=1" },
        @{ mode = "team-data-entry-transition-frame19"; hash = "A749413AF069B7939F0FDD573DA4DB7DBE8B69123A7B8DC0F1A4207CE9232F5D"; status = "transition-frame=19 palette=3 render=1" },
        @{ mode = "team-data-selector-profile-transition-frame10"; hash = "2377B0FF24274E21F5963CC35E43D0F666B7626E890A23C01A7621B842055F9A"; status = "transition-frame=10 palette=4 render=0" },
        @{ mode = "team-data-selector-profile-transition-frame16"; hash = "2377B0FF24274E21F5963CC35E43D0F666B7626E890A23C01A7621B842055F9A"; status = "transition-frame=16 palette=4 render=1" },
        @{ mode = "team-data-selector-profile-transition-frame19"; hash = "284D95AB6B188AD67B123B9C98CFAC7E7DE9CECB30B21C065390C9ECE2D135B7"; status = "transition-frame=19 palette=0 render=1" },
        @{ mode = "team-data-selector-profile-transition-frame31"; hash = "2619ADAF230E754E4FF6CA71C37F2E97794A2EC5DA3FD6777559E8F1D28F46B0"; status = "transition-frame=31 palette=3 render=1" },
        @{ mode = "team-data-roster-detail-transition-frame15"; hash = "2377B0FF24274E21F5963CC35E43D0F666B7626E890A23C01A7621B842055F9A"; status = "transition-frame=15 palette=4 render=1" },
        @{ mode = "team-data-roster-detail-transition-frame18"; hash = "472F8D5149AF8EEAD2BAD1CC132EF6E3AE42AB4B222F50A774BFC91EA9F86BA1"; status = "transition-frame=18 palette=0 render=1" },
        @{ mode = "team-data-roster-detail-transition-frame30"; hash = "B9982C582D687CFC7B5439AE5B4B369421C5874754291F686D8EFFE3D4FAD6A3"; status = "transition-frame=30 palette=3 render=1" },
        @{ mode = "team-data-detail-roster-transition-frame31"; hash = "2619ADAF230E754E4FF6CA71C37F2E97794A2EC5DA3FD6777559E8F1D28F46B0"; status = "transition-frame=31 palette=3 render=1" }
    )
    foreach ($Checkpoint in $Checkpoints) {
        $Png = Join-Path $Scratch ($Checkpoint.mode + ".png")
        $Output = Invoke-Tecmo @("--render-test-mode", $Checkpoint.mode, $Png)
        if ($Output -notlike ("*" + $Checkpoint.status + "*")) {
            throw "Render state mismatch for $($Checkpoint.mode)."
        }
        $ActualHash = (Get-FileHash $Png -Algorithm SHA256).Hash
        if ($ActualHash -ne $Checkpoint.hash) {
            throw "Pixel checkpoint mismatch for $($Checkpoint.mode): $ActualHash"
        }
    }

    $Malformed = Join-Path $Scratch "team-data-malformed.assetpack"
    $MalformedBytes = [IO.File]::ReadAllBytes($Pack)
    $PayloadOffset = (Get-PackEntry $MalformedBytes "menu/team-data").Offset
    $MutationOffset = [uint64]$PayloadOffset + 128
    if ($MutationOffset -ge $MalformedBytes.Length) {
        throw "Malformed-pack mutation escaped the TTDT payload."
    }
    $MalformedBytes[[int]$MutationOffset] = $MalformedBytes[[int]$MutationOffset] -bxor 1
    [IO.File]::WriteAllBytes($Malformed, $MalformedBytes)
    $env:TECMO_ASSETPACK = $Malformed
    $RejectedPng = Join-Path $Scratch "rejected.png"
    [void](Invoke-Tecmo @("--render-test-mode", "team-data-profile", $RejectedPng) 1)
    if (Test-Path $RejectedPng) {
        throw "Malformed TTDT-1 unexpectedly produced a screenshot."
    }

    $global:LASTEXITCODE = 0
    Write-Host "TEAM DATA TEST PASS: ROM-only TTDT parser, all-star mapping, input/state transitions, malformed rejection, and 15 pixel checkpoints"
} finally {
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    $env:TECMO_ASSETPACK = $PreviousAssetPack
    if (Test-Path $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
}
