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
    $RomPath = Join-Path (Split-Path -Parent $ProjectRoot) `
        "disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes"
}
if (!(Test-Path $RomPath)) {
    throw "Rev1 ROM not found. Pass -RomPath."
}
$RomPath = (Resolve-Path $RomPath).Path
if (!$DecompRoot) {
    $DecompRoot = Join-Path (Split-Path -Parent $ProjectRoot) `
        "disassem\tecmo-basketball-decompilation"
}
if (!(Test-Path $DecompRoot)) {
    throw "Decompilation root not found. Pass -DecompRoot."
}
$DecompRoot = (Resolve-Path $DecompRoot).Path

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
$Scratch = Join-Path $BuildDir `
    ("team_management_test_" + [Guid]::NewGuid().ToString("N"))
$Scratch = [IO.Path]::GetFullPath($Scratch)
$BuildPrefix = [IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/') + `
    [IO.Path]::DirectorySeparatorChar
if (!$Scratch.StartsWith($BuildPrefix,
                         [StringComparison]::OrdinalIgnoreCase)) {
    throw "TEAM management scratch path escaped build\."
}
New-Item -ItemType Directory -Path $Scratch | Out-Null

$PreviousSkipShortcut = $env:TECMO_SKIP_SHORTCUT
$PreviousAssetPack = $env:TECMO_ASSETPACK

function Invoke-Tecmo {
    param([string[]]$Arguments, [int]$ExpectedExit = 0)
    $Output = @(& $Executable @Arguments 2>&1)
    $ExitCode = $LASTEXITCODE
    if ($ExitCode -ne $ExpectedExit) {
        $Tail = @($Output | Select-Object -Last 12) -join `
            [Environment]::NewLine
        throw "tecmo_port exit $ExitCode (expected $ExpectedExit):`n$Tail"
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
        $Record = [uint64]$DirectoryOffset + [uint64]$Index * 128
        if ($Record + 128 -gt $Bytes.Length) {
            throw "Private asset pack directory was truncated."
        }
        $EntryId = [Text.Encoding]::ASCII.GetString(
            $Bytes, [int]$Record, 64).Trim([char]0)
        if ($EntryId -eq $Id) {
            $PayloadOffset = [BitConverter]::ToUInt64(
                $Bytes, [int]$Record + 84)
            $PayloadSize = [BitConverter]::ToUInt64(
                $Bytes, [int]$Record + 92)
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

    $Pack = Join-Path $Scratch "team-management.assetpack"
    [void](Invoke-Tecmo @("--build-assetpack", $RomPath, $Pack))
    $PackBytes = [IO.File]::ReadAllBytes($Pack)
    $SourceMap = Get-PackEntry $PackBytes "system/source-map"
    $Chr = Get-PackEntry $PackBytes "chr/all"
    $Management = Get-PackEntry $PackBytes "menu/team-management"
    $TeamData = Get-PackEntry $PackBytes "menu/team-data"
    if ($SourceMap.Size -eq 0 -or $Chr.Size -ne 262144 -or
        $Management.Size -ne 21061 -or $TeamData.Size -ne 96372) {
        throw "Required source-map/CHR/TTMG-1/TTDT-1 directory contracts were rejected."
    }
    $env:TECMO_ASSETPACK = $Pack

    $Focused = Invoke-Tecmo @("--team-management-test")
    if ($Focused -notmatch 'parser, persistence, substitution') {
        throw "Native TEAM management self-test did not pass."
    }
    $Flow = Invoke-Tecmo @("--root", $DecompRoot, "--flow-test")
    if ($Flow -notmatch 'FLOW TEST PASS') {
        throw "Native flow regression did not pass."
    }

    $Checkpoints = @(
        @{ mode="team-data-starters"; hash="066E5DE8B0C139BCB12FAC44172ADB1DE492700771D4F6860CC91FCEC7795863" },
        @{ mode="team-data-starters-reset"; hash="3A3729F54600C1D87F3CD2439F923D48B753CC97D923313FB75CE4FDCD7ED9DD" },
        @{ mode="team-data-starters-bench"; hash="E02B6DFAFB19096BD9D67065B85DC0821F574B9D8284CA014B972004F130065B" },
        @{ mode="team-data-playbook"; hash="7D19D72463A75826A5DEB15652A3D37912C00B1E522F4D8CBAF499A6AD2AA8CD" },
        @{ mode="team-data-playbook-replace-frame0"; hash="F5096D00FE3707A20BD08ADFAB66DD7E7CF8B3F34D924193D2B56503961C7DC5" },
        @{ mode="team-data-playbook-replace-frame1"; hash="CA32B93E7A807706D85E7B2A9172561E170C7791E7E8374E851444F4B34205A7" },
        @{ mode="team-data-playbook-replace-frame7"; hash="889CBE72F43D3865BBF1F220EDDB9D7365045EA93A880F3A034062CB74D41859" },
        @{ mode="team-data-playbook-replace-frame8"; hash="EDD17EB70DDD6E4DC994EBFA9C6525A01B0ADFB1892E7DB8279712418D7836A7" },
        @{ mode="team-data-playbook-reset"; hash="E85EC7C25C4FA0F4BB779553292ED4C3FB61506DE0C4767692564DF95E04E4AB" }
    )
    foreach ($Checkpoint in $Checkpoints) {
        $Png = Join-Path $Scratch ($Checkpoint.mode + ".png")
        [void](Invoke-Tecmo @("--render-test-mode", $Checkpoint.mode, $Png))
        $Actual = (Get-FileHash $Png -Algorithm SHA256).Hash
        if ($Actual -ne $Checkpoint.hash) {
            throw "Pixel checkpoint mismatch for $($Checkpoint.mode): $Actual"
        }
    }

    $Malformed = Join-Path $Scratch "management-malformed.assetpack"
    $MalformedBytes = [IO.File]::ReadAllBytes($Pack)
    $Mutation = [uint64]$Management.Offset + 100
    if ($Mutation -ge $MalformedBytes.Length) {
        throw "TTMG mutation escaped the pack."
    }
    $MalformedBytes[[int]$Mutation] = $MalformedBytes[[int]$Mutation] -bxor 1
    [IO.File]::WriteAllBytes($Malformed, $MalformedBytes)
    $env:TECMO_ASSETPACK = $Malformed
    $RejectedPng = Join-Path $Scratch "rejected-management.png"
    [void](Invoke-Tecmo @("--render-test-mode", "team-data-starters",
                          $RejectedPng) 1)
    if (Test-Path $RejectedPng) {
        throw "Malformed TTMG-1 unexpectedly rendered."
    }

    $CrossPack = Join-Path $Scratch "dependency-malformed.assetpack"
    $CrossBytes = [IO.File]::ReadAllBytes($Pack)
    $DependencyMutation = [uint64]$TeamData.Offset + 128
    if ($DependencyMutation -ge $CrossBytes.Length) {
        throw "TTDT dependency mutation escaped the pack."
    }
    $CrossBytes[[int]$DependencyMutation] =
        $CrossBytes[[int]$DependencyMutation] -bxor 1
    [IO.File]::WriteAllBytes($CrossPack, $CrossBytes)
    $env:TECMO_ASSETPACK = $CrossPack
    $RejectedDependency = Join-Path $Scratch "rejected-dependency.png"
    [void](Invoke-Tecmo @("--render-test-mode", "team-data-playbook",
                          $RejectedDependency) 1)
    if (Test-Path $RejectedDependency) {
        throw "Malformed same-pack TTDT dependency unexpectedly rendered."
    }

    $global:LASTEXITCODE = 0
    Write-Host "TEAM MANAGEMENT TEST PASS: strict ROM-only TTMG parser, session persistence, STARTERS substitution/detail/reset, PLAYBOOK release/carousel/reset, malformed dependency rejection, and 9 pixel checkpoints"
} finally {
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    $env:TECMO_ASSETPACK = $PreviousAssetPack
    if (Test-Path $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
}
