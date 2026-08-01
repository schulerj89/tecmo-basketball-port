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
    $ReportPath = Join-Path $ProjectRoot "build\native_flow_test_report.json"
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

function Test-PathUnder {
    param(
        [string]$Path,
        [string]$Parent
    )

    $FullPath = [System.IO.Path]::GetFullPath($Path)
    $FullParent = [System.IO.Path]::GetFullPath($Parent).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    return $FullPath.StartsWith($FullParent, [System.StringComparison]::OrdinalIgnoreCase)
}

$ManifestPath = Join-Path $ProjectRoot "port_iteration.json"
$ExePath = Join-Path $ProjectRoot "build\tecmo_port.exe"
$BuildDir = Join-Path $ProjectRoot "build"
$LocalDecompRoot = Find-LocalDecompRoot

if (!$LocalDecompRoot) {
    throw "Could not find a local decomp root. Pass -DecompRoot or set TECMO_DECOMP_ROOT."
}

if (!(Test-PathUnder $ReportPath $BuildDir)) {
    throw "Native flow report must stay under build\."
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

$Manifest = Get-Content -Raw $ManifestPath | ConvertFrom-Json
$Tests = @($Manifest.native_flow_tests | Where-Object { $_.status -eq "active" })
if ($Tests.Count -eq 0) {
    throw "No active native flow tests declared in port_iteration.json."
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

$Results = New-Object System.Collections.Generic.List[object]
$Failures = 0
$CommandPattern = '^\.\\build\\tecmo_port\.exe --root <LOCAL_DECOMP_ROOT> --flow-test$'

foreach ($Test in $Tests) {
    $Id = [string]$Test.id
    $Command = [string]$Test.command
    $ExpectedOutput = [string]$Test.expected_output
    if ($Command -notmatch $CommandPattern) {
        throw "Native flow test '$Id' uses an unsupported command shape: $Command"
    }

    Write-Host "Running $Id (--flow-test)"
    $Output = & $ExePath --root $LocalDecompRoot --flow-test 2>&1
    $ExitCode = $LASTEXITCODE
    $ExpectedOutputSeen = $false
    foreach ($Line in @($Output)) {
        if ([string]$Line -eq $ExpectedOutput) {
            $ExpectedOutputSeen = $true
        }
    }

    $Passed = $ExitCode -eq 0 -and $ExpectedOutputSeen
    if (!$Passed) {
        ++$Failures
    }

    $Results.Add([pscustomobject]@{
        id = $Id
        command = $Command
        passed = $Passed
        exit_code = $ExitCode
        expected_output_seen = $ExpectedOutputSeen
        decomp_root_used = "<local>"
        raw_output_persisted = $false
        error = if ($Passed) { $null } else { "native flow test failed; rerun command directly for local-only output" }
    })
}

function Invoke-CliBoundaryCase {
    param(
        [string]$Label,
        [string[]]$Arguments,
        [int]$ExpectedExitCode,
        [string]$ExpectedPattern
    )

    $Output = @(& $ExePath @Arguments 2>&1)
    $ExitCode = $LASTEXITCODE
    $Text = ($Output -join [Environment]::NewLine)
    if ($ExitCode -ne $ExpectedExitCode -or
        $Text -notmatch $ExpectedPattern) {
        throw "CLI boundary '$Label' failed (exit $ExitCode)."
    }
    Write-Host "CLI boundary passed: $Label"
}

Invoke-CliBoundaryCase "default-summary" @(
    "--summary"
) 0 '^Tecmo Basketball C port inventory'
Invoke-CliBoundaryCase "representative-family-dispatch" @(
    "--controls-test"
) 0 '^CONTROLS SELF TEST PASS:'
Invoke-CliBoundaryCase "missing-root-argument" @(
    "--root"
) 2 '^Usage:'
Invoke-CliBoundaryCase "unknown-command" @(
    "--definitely-unknown"
) 2 '^Usage:'
Invoke-CliBoundaryCase "missing-required-pack" @(
    "--assetpack-list"
) 2 '^Usage:'
Invoke-CliBoundaryCase "numeric-render-suffix" @(
    "--root", $LocalDecompRoot, "--render-test-mode",
    "intro-arena-clean-frame0"
) 0 '^intro-capture-status kind=arena'
Invoke-CliBoundaryCase "render-suffix-missing-number" @(
    "--root", $LocalDecompRoot, "--render-test-mode",
    "intro-arena-clean-frame"
) 1 '^Unsupported render-test mode:'
Invoke-CliBoundaryCase "render-suffix-trailing-character" @(
    "--root", $LocalDecompRoot, "--render-test-mode",
    "intro-arena-clean-frame1x"
) 1 '^Unsupported render-test mode:'
Invoke-CliBoundaryCase "render-suffix-overflow" @(
    "--root", $LocalDecompRoot, "--render-test-mode",
    "intro-arena-clean-frame184467440737095516160"
) 1 '^Unsupported render-test mode:'
Invoke-CliBoundaryCase "unknown-start-game-menu-prefix" @(
    "--root", $LocalDecompRoot, "--render-test-mode",
    "start-game-menu-unknown"
) 1 '^Unsupported render-test mode:'
Invoke-CliBoundaryCase "unknown-preseason-prefix" @(
    "--root", $LocalDecompRoot, "--render-test-mode",
    "preseason-unknown"
) 1 '^Unsupported render-test mode:'
Invoke-CliBoundaryCase "unknown-all-star-prefix" @(
    "--root", $LocalDecompRoot, "--render-test-mode",
    "all-star-unknown"
) 1 '^Unsupported render-test mode:'
Invoke-CliBoundaryCase "unknown-team-data-prefix" @(
    "--root", $LocalDecompRoot, "--render-test-mode",
    "team-data-unknown"
) 1 '^Unsupported render-test mode:'
Invoke-CliBoundaryCase "unknown-season-prefix" @(
    "--root", $LocalDecompRoot, "--render-test-mode",
    "season-unknown"
) 1 '^Unsupported render-test mode:'
Invoke-CliBoundaryCase "signed-court-coordinate" @(
    "--gameplay-cpu-steering-harness", "__cli-parser-boundary-pack__", "0", "0",
    "0", "0", "0", "0", "+1,+0", "+1,+0", "+1,+0", "+1,+0",
    "+1,+0", "+1,+0", "+1,+0", "+1,+0", "+1,+0", "+1,+0"
) 1 '^CPU steering harness load failed'

$Report = [pscustomobject]@{
    schema_version = 1
    generated_by = "tools/Run-NativeFlowTests.ps1"
    data_policy = "Sanitized native flow report only; raw stdout/stderr, local paths, ROM bytes, ASM, CHR bytes, screenshots, and roster names are not persisted."
    passed = $Failures -eq 0
    decomp_root_used = "<local>"
    raw_output_persisted = $false
    tests = $Results
}

$Report | ConvertTo-Json -Depth 5 | Set-Content -Path $ReportPath -Encoding UTF8
$Results | Format-Table -AutoSize
Write-Host "Wrote native flow test report: $ReportPath"

if ($Failures -ne 0) {
    throw "$Failures native flow test(s) failed."
}
Write-Host "All native flow tests passed."
