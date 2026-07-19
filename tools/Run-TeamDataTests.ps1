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

function Get-MenuEntryPackOffset {
    param([byte[]]$Bytes)
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
        $Id = [Text.Encoding]::ASCII.GetString($Bytes, [int]$EntryOffset, 64).Trim([char]0)
        if ($Id -eq "menu/team-data") {
            $Size = [BitConverter]::ToUInt64($Bytes, [int]$EntryOffset + 92)
            if ($Size -ne 96372) {
                throw "TTDT-1 directory size was not 96372."
            }
            return [BitConverter]::ToUInt64($Bytes, [int]$EntryOffset + 84)
        }
    }
    throw "menu/team-data was missing from the private asset pack."
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
    $BuildOutput = Invoke-Tecmo @("--build-assetpack", $RomPath, $Pack)
    if ($BuildOutput -notmatch '64 entries') {
        throw "Private ROM-only pack did not contain the canonical entry count."
    }
    $PackBytes = [IO.File]::ReadAllBytes($Pack)
    [void](Get-MenuEntryPackOffset $PackBytes)
    $env:TECMO_ASSETPACK = $Pack

    $FlowOutput = Invoke-Tecmo @("--root", $DecompRoot, "--flow-test")
    if ($FlowOutput -notmatch 'FLOW TEST PASS') {
        throw "TEAM DATA state/input/all-star/transition flow did not pass."
    }

    $Checkpoints = @(
        @{ mode = "team-data-select"; hash = "C04A940E9BD78DC9D330AC9E41C2B6F03906A040CE52D442EB08BCE7FE4C7EB8"; status = "palette=3 render=1" },
        @{ mode = "team-data-profile"; hash = "C07DD2AA74EB014BD7892B97F7EFBECBFC03C158C548D930C3684F946E392C55"; status = "phase=TEAM PROFILE" },
        @{ mode = "team-data-player-detail"; hash = "0EEAA5599972BA5C58D6B8971DF9D6C55E8230F95E0862693DEEE560B292238B"; status = "phase=PLAYER DETAIL" },
        @{ mode = "team-data-entry-transition-frame0"; hash = "2377B0FF24274E21F5963CC35E43D0F666B7626E890A23C01A7621B842055F9A"; status = "transition-frame=0 palette=4 render=0" },
        @{ mode = "team-data-entry-transition-frame4"; hash = "2377B0FF24274E21F5963CC35E43D0F666B7626E890A23C01A7621B842055F9A"; status = "transition-frame=4 palette=4 render=1" },
        @{ mode = "team-data-entry-transition-frame7"; hash = "1C94880CC9919AFC5C7AB1C482B24C586556F3479421F1B5BBD29DC8808AB34A"; status = "transition-frame=7 palette=0 render=1" },
        @{ mode = "team-data-entry-transition-frame19"; hash = "C0F7882DC8C7D23A97B0864172B88446A886F9CC0C21005AEE9DE2DFA373DD07"; status = "transition-frame=19 palette=3 render=1" },
        @{ mode = "team-data-selector-profile-transition-frame10"; hash = "2377B0FF24274E21F5963CC35E43D0F666B7626E890A23C01A7621B842055F9A"; status = "transition-frame=10 palette=4 render=0" },
        @{ mode = "team-data-selector-profile-transition-frame16"; hash = "2377B0FF24274E21F5963CC35E43D0F666B7626E890A23C01A7621B842055F9A"; status = "transition-frame=16 palette=4 render=1" },
        @{ mode = "team-data-selector-profile-transition-frame19"; hash = "456FBF79F99EEA029F2CC19D9EE1CE1D0489E90995C92AB90ED232537E1D11DA"; status = "transition-frame=19 palette=0 render=1" },
        @{ mode = "team-data-selector-profile-transition-frame31"; hash = "5E1258B43E47DBB71E3FAE28C91487A2BD0BAC2ECC4E7F2E6E360AC5FB428B64"; status = "transition-frame=31 palette=3 render=1" },
        @{ mode = "team-data-roster-detail-transition-frame15"; hash = "2377B0FF24274E21F5963CC35E43D0F666B7626E890A23C01A7621B842055F9A"; status = "transition-frame=15 palette=4 render=1" },
        @{ mode = "team-data-roster-detail-transition-frame18"; hash = "495F6050A1FE64A498B056AC602C9928E0A1A95AFE0D47722AFD33FABC56DAC7"; status = "transition-frame=18 palette=0 render=1" },
        @{ mode = "team-data-roster-detail-transition-frame30"; hash = "0EEAA5599972BA5C58D6B8971DF9D6C55E8230F95E0862693DEEE560B292238B"; status = "transition-frame=30 palette=3 render=1" },
        @{ mode = "team-data-detail-roster-transition-frame31"; hash = "5E1258B43E47DBB71E3FAE28C91487A2BD0BAC2ECC4E7F2E6E360AC5FB428B64"; status = "transition-frame=31 palette=3 render=1" }
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
    $PayloadOffset = Get-MenuEntryPackOffset $MalformedBytes
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
