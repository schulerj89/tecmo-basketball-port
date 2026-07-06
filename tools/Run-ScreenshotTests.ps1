param(
    [string]$ProjectRoot,
    [string]$DecompRoot,
    [switch]$Build,
    [string]$ReportPath
)

$ErrorActionPreference = "Stop"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path $ProjectRoot).Path

if (!$ReportPath) {
    $ReportPath = Join-Path $ProjectRoot "build\screenshot_test_report.json"
}

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

function Test-PathUnder {
    param(
        [string]$Path,
        [string]$Parent
    )

    $FullPath = [System.IO.Path]::GetFullPath($Path)
    $FullParent = [System.IO.Path]::GetFullPath($Parent).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    return $FullPath.StartsWith($FullParent, [System.StringComparison]::OrdinalIgnoreCase)
}

function Get-BooleanProperty {
    param(
        [object]$Object,
        [string]$Name
    )

    $Property = $Object.PSObject.Properties[$Name]
    if ($null -eq $Property) {
        return $false
    }
    return [bool]$Property.Value
}

$ManifestPath = Join-Path $ProjectRoot "port_iteration.json"
$ExePath = Join-Path $ProjectRoot "build\tecmo_port.exe"
$BuildDir = Join-Path $ProjectRoot "build"
$DocsScreenshotsDir = Join-Path $ProjectRoot "docs\screenshots"
$LocalDecompRoot = Find-LocalDecompRoot

if (!$LocalDecompRoot) {
    throw "Could not find a local decomp root. Pass -DecompRoot or set TECMO_DECOMP_ROOT."
}

if ($Build) {
    & (Join-Path $ProjectRoot "build.ps1")
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE."
    }
}
if (!(Test-Path $ExePath)) {
    throw "Executable not found at $ExePath. Run .\build.ps1 or pass -Build."
}

$Manifest = Get-Content -Raw $ManifestPath | ConvertFrom-Json
$Tests = @($Manifest.screenshot_tests | Where-Object { $_.status -eq "active" })
if ($Tests.Count -eq 0) {
    throw "No active screenshot tests declared in port_iteration.json."
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
New-Item -ItemType Directory -Force -Path $DocsScreenshotsDir | Out-Null
Add-Type -AssemblyName System.Drawing

$Results = New-Object System.Collections.Generic.List[object]
$Failures = 0
$CommandPattern = '^\.\\build\\tecmo_port\.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode ([A-Za-z0-9-]+) ([^\s]+)$'

foreach ($Test in $Tests) {
    $Id = [string]$Test.id
    $Command = [string]$Test.command
    if ($Command -notmatch $CommandPattern) {
        throw "Screenshot test '$Id' uses an unsupported command shape: $Command"
    }

    $Mode = $Matches[1]
    $OutputRelative = $Matches[2]
    if ([System.IO.Path]::IsPathRooted($OutputRelative)) {
        throw "Screenshot test '$Id' must use a repo-relative output path."
    }

    $OutputPath = [System.IO.Path]::GetFullPath((Join-Path $ProjectRoot $OutputRelative))
    if (!$OutputPath.EndsWith(".png", [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Screenshot test '$Id' must write a PNG."
    }
    if (!(Test-PathUnder $OutputPath $BuildDir) -and !(Test-PathUnder $OutputPath $DocsScreenshotsDir)) {
        throw "Screenshot test '$Id' output must stay under build\ or docs\screenshots\."
    }
    if ((Test-PathUnder $OutputPath $DocsScreenshotsDir) -and !(Get-BooleanProperty $Test "commit_safe")) {
        throw "Screenshot test '$Id' writes to docs\screenshots but is not marked commit_safe."
    }

    $OutputDir = Split-Path -Parent $OutputPath
    New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

    Write-Host "Rendering $Id ($Mode) -> $OutputRelative"
    & $ExePath --root $LocalDecompRoot --render-test-mode $Mode $OutputPath
    if ($LASTEXITCODE -ne 0) {
        ++$Failures
        $Results.Add([pscustomobject]@{
            id = $Id
            mode = $Mode
            output = $OutputRelative
            passed = $false
            error = "renderer exited with $LASTEXITCODE"
        })
        continue
    }

    if (!(Test-Path $OutputPath)) {
        ++$Failures
        $Results.Add([pscustomobject]@{
            id = $Id
            mode = $Mode
            output = $OutputRelative
            passed = $false
            error = "PNG was not created"
        })
        continue
    }

    $Image = [System.Drawing.Image]::FromFile($OutputPath)
    try {
        $Passed = $Image.Width -eq 640 -and $Image.Height -eq 480
        if (!$Passed) {
            ++$Failures
        }
        $Results.Add([pscustomobject]@{
            id = $Id
            mode = $Mode
            output = $OutputRelative
            passed = $Passed
            width = $Image.Width
            height = $Image.Height
            bytes = (Get-Item $OutputPath).Length
            commit_safe = (Get-BooleanProperty $Test "commit_safe")
        })
    } finally {
        $Image.Dispose()
    }
}

$Report = [pscustomobject]@{
    generated_at_utc = [DateTime]::UtcNow.ToString("o")
    project = $Manifest.project
    decomp_root_used = "<local>"
    test_count = $Results.Count
    failure_count = $Failures
    results = $Results
}

$ReportDir = Split-Path -Parent $ReportPath
if ($ReportDir) {
    New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null
}
$Report | ConvertTo-Json -Depth 6 | Set-Content -Path $ReportPath -Encoding ASCII

$Results | Format-Table id, mode, passed, width, height, output -AutoSize
Write-Host "Wrote screenshot test report: $ReportPath"

if ($Failures -ne 0) {
    throw "$Failures screenshot test(s) failed."
}

Write-Host "All screenshot tests passed."
