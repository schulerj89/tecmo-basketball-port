param(
    [string]$ProjectRoot,
    [string]$DecompRoot,
    [string]$RomPath,
    [switch]$Build,
    [string]$AssetPackPath,
    [string]$ChrRenderPath,
    [string]$ReportPath
)

$ErrorActionPreference = "Stop"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path $ProjectRoot).Path

$BuildDir = Join-Path $ProjectRoot "build"

if (!$AssetPackPath) {
    $AssetPackPath = Join-Path $BuildDir "tecmo.assetpack"
}
if (![System.IO.Path]::IsPathRooted($AssetPackPath)) {
    $AssetPackPath = Join-Path $ProjectRoot $AssetPackPath
}
$AssetPackPath = [System.IO.Path]::GetFullPath($AssetPackPath)

if (!$ChrRenderPath) {
    $ChrRenderPath = Join-Path $BuildDir "asset_pack_chr_playground_test.png"
}
if (![System.IO.Path]::IsPathRooted($ChrRenderPath)) {
    $ChrRenderPath = Join-Path $ProjectRoot $ChrRenderPath
}
$ChrRenderPath = [System.IO.Path]::GetFullPath($ChrRenderPath)

if (!$ReportPath) {
    $ReportPath = Join-Path $BuildDir "asset_pack_test_report.json"
}
if (![System.IO.Path]::IsPathRooted($ReportPath)) {
    $ReportPath = Join-Path $ProjectRoot $ReportPath
}
$ReportPath = [System.IO.Path]::GetFullPath($ReportPath)

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

    if ($RomPath) {
        if (!(Test-Path $RomPath)) {
            throw "RomPath does not exist."
        }
        return (Resolve-Path $RomPath).Path
    }
    if ($env:TECMO_ROM_PATH -and (Test-Path $env:TECMO_ROM_PATH)) {
        return (Resolve-Path $env:TECMO_ROM_PATH).Path
    }
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
            return (Resolve-Path $Candidate).Path
        }
    }

    return $null
}

function Test-PathUnder {
    param(
        [string]$Path,
        [string]$Parent
    )

    $FullPath = [System.IO.Path]::GetFullPath($Path)
    $FullParent = [System.IO.Path]::GetFullPath($Parent).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    return $FullPath.StartsWith($FullParent, [System.StringComparison]::OrdinalIgnoreCase)
}

function ConvertTo-RepoRelativePath {
    param([string]$Path)

    $FullPath = [System.IO.Path]::GetFullPath($Path)
    $FullRoot = [System.IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    if ($FullPath.StartsWith($FullRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $FullPath.Substring($FullRoot.Length)
    }
    return "<outside-repo>"
}

function Read-AssetPackDirectory {
    param([string]$Path)

    $File = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
    $Reader = [System.IO.BinaryReader]::new($File)
    try {
        $Magic = [System.Text.Encoding]::ASCII.GetString($Reader.ReadBytes(4))
        $Version = $Reader.ReadUInt32()
        $HeaderSize = $Reader.ReadUInt32()
        $EntrySize = $Reader.ReadUInt32()
        $EntryCount = $Reader.ReadUInt32()
        $DirectoryOffset = $Reader.ReadUInt64()
        $DataOffset = $Reader.ReadUInt64()
        [void]$Reader.ReadUInt32()

        if ($Magic -ne "TAP1" -or $Version -ne 1 -or $HeaderSize -ne 40 -or $EntrySize -ne 128) {
            throw "Asset pack header is not the expected TAP1 v1 format."
        }
        if ($DataOffset -ne 40) {
            throw "Asset pack data offset is not the expected header size."
        }
        if ($DirectoryOffset -gt [uint64]$File.Length) {
            throw "Asset pack directory offset is outside the file."
        }
        if (($DirectoryOffset + ([uint64]$EntryCount * [uint64]$EntrySize)) -gt [uint64]$File.Length) {
            throw "Asset pack directory extends outside the file."
        }

        [void]$File.Seek([int64]$DirectoryOffset, [System.IO.SeekOrigin]::Begin)
        $Ids = [System.Collections.Generic.List[string]]::new()
        for ($Index = 0; $Index -lt $EntryCount; ++$Index) {
            $EntryBytes = $Reader.ReadBytes($EntrySize)
            if ($EntryBytes.Length -ne $EntrySize) {
                throw "Asset pack directory entry is truncated."
            }
            $IdLength = [Array]::IndexOf($EntryBytes, [byte]0, 0, 64)
            if ($IdLength -lt 0) {
                $IdLength = 64
            }
            $Id = [System.Text.Encoding]::ASCII.GetString($EntryBytes, 0, $IdLength)
            [void]$Ids.Add($Id)
        }

        return [pscustomobject]@{
            version = $Version
            entry_count = $EntryCount
            ids = $Ids
        }
    } finally {
        $Reader.Dispose()
        $File.Dispose()
    }
}

function Test-PngSmoke {
    param([string]$Path)

    $Bitmap = [System.Drawing.Bitmap]::FromFile($Path)
    try {
        $NonBackground = 0
        for ($Y = 56; $Y -lt 160; ++$Y) {
            for ($X = 28; $X -lt 610; ++$X) {
                $Pixel = $Bitmap.GetPixel($X, $Y)
                if ($Pixel.R -ne 8 -or $Pixel.G -ne 10 -or $Pixel.B -ne 16) {
                    ++$NonBackground
                }
            }
        }

        return [pscustomobject]@{
            width = $Bitmap.Width
            height = $Bitmap.Height
            non_background_grid_pixels = $NonBackground
        }
    } finally {
        $Bitmap.Dispose()
    }
}

if (!(Test-PathUnder $AssetPackPath $BuildDir)) {
    throw "Asset pack output must stay under build\."
}
if (!(Test-PathUnder $ChrRenderPath $BuildDir)) {
    throw "CHR playground render output must stay under build\."
}
if (!(Test-PathUnder $ReportPath $BuildDir)) {
    throw "Asset pack test report must stay under build\."
}
if (!$AssetPackPath.EndsWith(".assetpack", [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Asset pack output must use the .assetpack extension."
}
if (!$ChrRenderPath.EndsWith(".png", [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "CHR playground render output must be a PNG."
}

$ManifestPath = Join-Path $ProjectRoot "port_iteration.json"
$ExePath = Join-Path $ProjectRoot "build\tecmo_port.exe"
$LocalDecompRoot = Find-LocalDecompRoot

if (!$LocalDecompRoot) {
    throw "Could not find a local decomp root. Pass -DecompRoot or set TECMO_DECOMP_ROOT."
}

$ReferenceRom = Find-ReferenceRom $LocalDecompRoot
if (!$ReferenceRom) {
    throw "Could not find a local iNES ROM. Pass -RomPath or set TECMO_ROM_PATH."
}

if ($Build) {
    $PreviousSkipShortcut = $env:TECMO_SKIP_SHORTCUT
    $env:TECMO_SKIP_SHORTCUT = "1"
    try {
        & (Join-Path $ProjectRoot "build.ps1")
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed with exit code $LASTEXITCODE."
        }
    } finally {
        $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    }
}
if (!(Test-Path $ExePath)) {
    throw "Executable not found at $ExePath. Run .\build.ps1 or pass -Build."
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $AssetPackPath) | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $ChrRenderPath) | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $ReportPath) | Out-Null
Add-Type -AssemblyName System.Drawing

$Results = [System.Collections.Generic.List[object]]::new()
$Failures = 0
$AssetPackRelative = ConvertTo-RepoRelativePath $AssetPackPath
$ChrRenderRelative = ConvertTo-RepoRelativePath $ChrRenderPath
$ReportedPrgBanks = $null
$ReportedChrBanks = $null
$ReportedEntries = $null
$Directory = $null
$RequiredEntries = @("system/manifest", "system/source-map", "prg/bank00", "prg/fixed", "chr/all", "chr/bank00", "chr/bank31")

Write-Host "Building asset pack -> $AssetPackRelative"
$BuildOutput = & $ExePath --build-assetpack $ReferenceRom $AssetPackPath 2>&1
$BuildExitCode = $LASTEXITCODE
$BuildOutputText = (@($BuildOutput) | ForEach-Object { [string]$_ }) -join "`n"
if ($BuildOutputText -match "Wrote\s+([0-9]+)\s+PRG banks,\s+([0-9]+)\s+CHR banks,\s+([0-9]+)\s+entries to .+") {
    $ReportedPrgBanks = [int]$Matches[1]
    $ReportedChrBanks = [int]$Matches[2]
    $ReportedEntries = [int]$Matches[3]
}
$BuildPassed = $BuildExitCode -eq 0 -and $null -ne $ReportedPrgBanks -and $null -ne $ReportedChrBanks -and $null -ne $ReportedEntries
if (!$BuildPassed) {
    ++$Failures
}
$Results.Add([pscustomobject]@{
    id = "build-assetpack"
    passed = $BuildPassed
    exit_code = $BuildExitCode
    output_reported_prg_banks = $null -ne $ReportedPrgBanks
    output_reported_chr_banks = $null -ne $ReportedChrBanks
    output_reported_entry_count = $null -ne $ReportedEntries
    raw_output_persisted = $false
}) | Out-Null

if ($BuildPassed) {
    Write-Host ("Asset pack build reported {0} PRG banks, {1} CHR banks, {2} entries." -f $ReportedPrgBanks, $ReportedChrBanks, $ReportedEntries)
} else {
    Write-Host "Asset pack build failed or did not report PRG/CHR entry counts."
}

$PackCreated = Test-Path $AssetPackPath
if (!$PackCreated) {
    ++$Failures
}
$Results.Add([pscustomobject]@{
    id = "assetpack-file-created"
    passed = $PackCreated
    output = $AssetPackRelative
    bytes = if ($PackCreated) { (Get-Item $AssetPackPath).Length } else { $null }
}) | Out-Null

if ($PackCreated) {
    $Directory = Read-AssetPackDirectory $AssetPackPath
    $MissingEntries = @($RequiredEntries | Where-Object { !$Directory.ids.Contains($_) })
    $DirectoryPassed = $MissingEntries.Count -eq 0
    if (!$DirectoryPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "assetpack-required-entries"
        passed = $DirectoryPassed
        directory_entry_count = $Directory.entry_count
        required_entries = $RequiredEntries
        missing_entries = $MissingEntries
        raw_asset_bytes_persisted = $false
    }) | Out-Null
} else {
    $Results.Add([pscustomobject]@{
        id = "assetpack-required-entries"
        passed = $false
        error = "asset pack file was not created"
    }) | Out-Null
}

$PreviousAssetPack = $env:TECMO_ASSETPACK
$env:TECMO_ASSETPACK = $AssetPackPath
try {
    Write-Host "Rendering CHR playground from asset pack -> $ChrRenderRelative"
    $RenderOutput = & $ExePath --root $LocalDecompRoot --render-test-mode chr-playground $ChrRenderPath 2>&1
    $RenderExitCode = $LASTEXITCODE
    $RenderExists = Test-Path $ChrRenderPath
    $RenderSmoke = if ($RenderExists) { Test-PngSmoke $ChrRenderPath } else { $null }
    $RenderPassed = $RenderExitCode -eq 0 -and
        $RenderExists -and
        $RenderSmoke.width -eq 640 -and
        $RenderSmoke.height -eq 480 -and
        $RenderSmoke.non_background_grid_pixels -gt 200
    if (!$RenderPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "chr-playground-render"
        passed = $RenderPassed
        exit_code = $RenderExitCode
        output = $ChrRenderRelative
        width = if ($RenderSmoke) { $RenderSmoke.width } else { $null }
        height = if ($RenderSmoke) { $RenderSmoke.height } else { $null }
        non_background_grid_pixels = if ($RenderSmoke) { $RenderSmoke.non_background_grid_pixels } else { $null }
        asset_pack_env_used = $true
        raw_output_persisted = $false
    }) | Out-Null

    $Manifest = Get-Content -Raw $ManifestPath | ConvertFrom-Json
    $FlowTests = @($Manifest.native_flow_tests | Where-Object { $_.status -eq "active" })
    if ($FlowTests.Count -eq 0) {
        throw "No active native flow tests declared in port_iteration.json."
    }
    $FlowTest = $FlowTests[0]
    $ExpectedFlowOutput = [string]$FlowTest.expected_output
    if ([string]$FlowTest.command -notmatch '^\.\\build\\tecmo_port\.exe --root <LOCAL_DECOMP_ROOT> --flow-test$') {
        throw "Native flow test '$($FlowTest.id)' uses an unsupported command shape."
    }

    Write-Host "Running flow-test with asset pack present"
    $FlowOutput = & $ExePath --root $LocalDecompRoot --flow-test 2>&1
    $FlowExitCode = $LASTEXITCODE
    $ExpectedFlowOutputSeen = $false
    foreach ($Line in @($FlowOutput)) {
        if ([string]$Line -eq $ExpectedFlowOutput) {
            $ExpectedFlowOutputSeen = $true
        }
    }
    $FlowPassed = $FlowExitCode -eq 0 -and $ExpectedFlowOutputSeen
    if (!$FlowPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "flow-test"
        passed = $FlowPassed
        exit_code = $FlowExitCode
        expected_output_seen = $ExpectedFlowOutputSeen
        asset_pack_env_used = $true
        raw_output_persisted = $false
    }) | Out-Null
} finally {
    if ($null -eq $PreviousAssetPack) {
        Remove-Item Env:\TECMO_ASSETPACK -ErrorAction SilentlyContinue
    } else {
        $env:TECMO_ASSETPACK = $PreviousAssetPack
    }
}

$Report = [pscustomobject]@{
    schema_version = 1
    generated_by = "tools/Run-AssetPackTests.ps1"
    data_policy = "Sanitized asset-pack smoke report only; raw stdout/stderr, local paths, ROM bytes, extracted assets, ASM, CHR bytes, and screenshots outside ignored build output are not persisted."
    passed = $Failures -eq 0
    decomp_root_used = "<local>"
    reference_rom_used = "<local>"
    private_paths_included = $false
    raw_output_persisted = $false
    asset_pack = [pscustomobject]@{
        output = $AssetPackRelative
        prg_banks_reported = $ReportedPrgBanks
        chr_banks_reported = $ReportedChrBanks
        entries_reported = $ReportedEntries
        directory_entry_count = if ($Directory) { $Directory.entry_count } else { $null }
    }
    tests = $Results
}

$Report | ConvertTo-Json -Depth 6 | Set-Content -Path $ReportPath -Encoding UTF8
$Results | Format-Table -AutoSize
Write-Host "Wrote asset-pack test report: $ReportPath"

if ($Failures -ne 0) {
    throw "$Failures asset-pack test(s) failed."
}

Write-Host "All asset-pack tests passed."
