param(
    [string]$ProjectRoot,
    [string]$DecompRoot,
    [string]$EmulatorPath,
    [string]$OutputPath,
    [switch]$Launch
)

$ErrorActionPreference = "Stop"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path $ProjectRoot).Path

if (!$OutputPath) {
    $OutputPath = Join-Path $ProjectRoot "build\nes_reference_intro_check.json"
}
if (![System.IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath = Join-Path $ProjectRoot $OutputPath
}
$OutputPath = [System.IO.Path]::GetFullPath($OutputPath)

function Find-LocalDecompRoot {
    if ($DecompRoot -and (Test-Path $DecompRoot)) {
        return (Resolve-Path $DecompRoot).Path
    }
    if ($env:TECMO_DECOMP_ROOT -and (Test-Path $env:TECMO_DECOMP_ROOT)) {
        return (Resolve-Path $env:TECMO_DECOMP_ROOT).Path
    }

    $ProjectsRoot = Split-Path -Parent $ProjectRoot
    $Candidates = @(
        (Join-Path $ProjectsRoot "disassem\tecmo-basketball-decompilation"),
        (Join-Path $ProjectsRoot "tecmo-basketball-decompilation")
    )
    foreach ($Candidate in $Candidates) {
        if (Test-Path $Candidate) {
            return (Resolve-Path $Candidate).Path
        }
    }

    return $null
}

function Find-ReferenceRom {
    param([string]$LocalDecompRoot)

    if (!$LocalDecompRoot) {
        return $null
    }

    $Candidates = @(
        (Join-Path $LocalDecompRoot "build\out\tecmo_basketball_disassem.nes"),
        (Join-Path $LocalDecompRoot "build\out\tecmo_basketball_split_compile.nes"),
        (Join-Path $LocalDecompRoot "build\out\tecmo_basketball_split_bank01.nes"),
        (Join-Path $LocalDecompRoot "build\split_compile\build\out\tecmo_basketball_split_compile.nes")
    )
    foreach ($Candidate in $Candidates) {
        if (Test-Path $Candidate) {
            return (Get-Item $Candidate).FullName
        }
    }

    return $null
}

function Add-EmulatorCandidate {
    param(
        [System.Collections.Generic.List[string]]$Candidates,
        [string]$Path
    )

    if ($Path -and (Test-Path $Path)) {
        $Resolved = (Resolve-Path $Path).Path
        if (!$Candidates.Contains($Resolved)) {
            [void]$Candidates.Add($Resolved)
        }
    }
}

function Find-ReferenceEmulator {
    if ($EmulatorPath) {
        if (!(Test-Path $EmulatorPath)) {
            throw "EmulatorPath does not exist: $EmulatorPath"
        }
        return (Resolve-Path $EmulatorPath).Path
    }

    $Candidates = [System.Collections.Generic.List[string]]::new()
    $Commands = @("Mesen", "Mesen.exe", "fceux", "fceux.exe", "retroarch", "retroarch.exe", "EmuHawk", "EmuHawk.exe")
    foreach ($CommandName in $Commands) {
        $Command = Get-Command $CommandName -ErrorAction SilentlyContinue
        if ($Command -and $Command.Source) {
            Add-EmulatorCandidate $Candidates $Command.Source
        }
    }

    $DirectCandidates = [System.Collections.Generic.List[string]]::new()
    if ($env:LOCALAPPDATA) {
        [void]$DirectCandidates.Add((Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages\SourMesen.Mesen2_Microsoft.Winget.Source_8wekyb3d8bbwe\Mesen.exe"))
    }
    if ($env:ProgramFiles) {
        [void]$DirectCandidates.Add((Join-Path $env:ProgramFiles "Mesen\Mesen.exe"))
    }
    if (${env:ProgramFiles(x86)}) {
        [void]$DirectCandidates.Add((Join-Path ${env:ProgramFiles(x86)} "FCEUX\fceux.exe"))
    }
    foreach ($Candidate in $DirectCandidates) {
        Add-EmulatorCandidate $Candidates $Candidate
    }

    $SearchRoots = [System.Collections.Generic.List[string]]::new()
    if ($env:LOCALAPPDATA) {
        [void]$SearchRoots.Add((Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages"))
    }
    foreach ($Drive in Get-PSDrive -PSProvider FileSystem) {
        foreach ($FolderName in @("Games", "Emulators", "NES")) {
            $CandidateRoot = Join-Path $Drive.Root $FolderName
            if (!$SearchRoots.Contains($CandidateRoot)) {
                [void]$SearchRoots.Add($CandidateRoot)
            }
        }
    }

    foreach ($Root in $SearchRoots) {
        if (Test-Path $Root) {
            Get-ChildItem -Path $Root -Recurse -Include Mesen.exe,fceux.exe,retroarch.exe,EmuHawk.exe -ErrorAction SilentlyContinue |
                Select-Object -First 20 |
                ForEach-Object { Add-EmulatorCandidate $Candidates $_.FullName }
        }
    }

    if ($Candidates.Count -gt 0) {
        return $Candidates[0]
    }
    return $null
}

function Get-EmulatorKind {
    param([string]$Path)

    if (!$Path) {
        return "none"
    }
    $Name = [System.IO.Path]::GetFileNameWithoutExtension($Path).ToLowerInvariant()
    if ($Name -like "*mesen*") {
        return "mesen"
    }
    if ($Name -like "*fceux*") {
        return "fceux"
    }
    if ($Name -like "*retroarch*") {
        return "retroarch"
    }
    if ($Name -like "*emuhawk*") {
        return "emuhawk"
    }
    return "unknown"
}

$LocalDecompRoot = Find-LocalDecompRoot
$RomPath = Find-ReferenceRom $LocalDecompRoot
$ResolvedEmulatorPath = Find-ReferenceEmulator
$RomItem = if ($RomPath) { Get-Item $RomPath } else { $null }
$EmulatorItem = if ($ResolvedEmulatorPath) { Get-Item $ResolvedEmulatorPath } else { $null }
$CanLaunch = [bool]($RomPath -and $ResolvedEmulatorPath)

if ($Launch) {
    if (!$CanLaunch) {
        throw "Cannot launch reference game; missing local ROM or emulator."
    }
    Start-Process -FilePath $ResolvedEmulatorPath -ArgumentList @($RomPath) -WindowStyle Normal
}

$OutputDir = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$Report = [pscustomobject]@{
    schema_version = 1
    generated_by = "tools/Find-NesReferenceIntro.ps1"
    data_policy = "Local-only detector; report stores booleans, file names, sizes, and emulator kind only. No ROM bytes, CHR bytes, screenshots, ASM, or private paths."
    private_paths_included = $false
    local_decomp_root_found = [bool]$LocalDecompRoot
    reference_rom_found = [bool]$RomPath
    reference_rom_file = if ($RomItem) { $RomItem.Name } else { $null }
    reference_rom_size_bytes = if ($RomItem) { $RomItem.Length } else { $null }
    emulator_found = [bool]$ResolvedEmulatorPath
    emulator_kind = Get-EmulatorKind $ResolvedEmulatorPath
    emulator_file = if ($EmulatorItem) { $EmulatorItem.Name } else { $null }
    launch_available = $CanLaunch
    launch_was_requested = [bool]$Launch
    manual_reference_note = "Run this script with -Launch to open the local rebuilt NES in the discovered emulator for intro timing and title-screen comparison."
}

$Report | ConvertTo-Json -Depth 4 | Set-Content -Path $OutputPath -Encoding UTF8
$Report | ConvertTo-Json -Depth 4
