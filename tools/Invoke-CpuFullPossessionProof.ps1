param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [string]$OutputDirectory,
    [switch]$Build,
    [switch]$ExpectBaselineFailure
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
    throw "CPU full-possession proof requires the exact Rev1 ROM."
}

$BuildRoot = [IO.Path]::GetFullPath((Join-Path $ProjectRoot "build"))
if (!$OutputDirectory) {
    $OutputDirectory = Join-Path $BuildRoot "cpu-full-possession-proof"
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$BuildPrefix = $BuildRoot.TrimEnd('\', '/') +
    [IO.Path]::DirectorySeparatorChar
if (!$OutputDirectory.StartsWith($BuildPrefix,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw "Proof output must stay under the ignored build directory."
}

$Executable = Join-Path $BuildRoot "tecmo_port.exe"
if ($Build -or !(Test-Path -LiteralPath $Executable -PathType Leaf)) {
    $PreviousSkipShortcut = $env:TECMO_SKIP_SHORTCUT
    try {
        $env:TECMO_SKIP_SHORTCUT = "1"
        $BuildOutput = @(& (Join-Path $ProjectRoot "build.ps1") 2>&1)
        if ($LASTEXITCODE -ne 0 -or
            @($BuildOutput | Where-Object {
                $_ -match 'warning [A-Z]+[0-9]+:'
            }).Count -ne 0) {
            throw "Warning-clean CPU full-possession proof build failed."
        }
    } finally {
        $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    }
}

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$PackPath = Join-Path $OutputDirectory "cpu-full-possession.assetpack"
& $Executable --build-assetpack $RomPath $PackPath | Out-Null
if ($LASTEXITCODE -ne 0 -or
    !(Test-Path -LiteralPath $PackPath -PathType Leaf)) {
    throw "Could not build the ephemeral proof asset pack."
}

$Records = @()
foreach ($Repeat in 1, 2) {
    $RunDirectory = Join-Path $OutputDirectory ("run-{0}" -f $Repeat)
    New-Item -ItemType Directory -Force -Path $RunDirectory | Out-Null
    $TracePath = Join-Path $RunDirectory "frames.ndjson"
    $MidPath = Join-Path $RunDirectory "mid-max-overhang.png"
    $TerminalPath = Join-Path $RunDirectory "terminal.png"
    $Output = @(& $Executable --root $ProjectRoot `
        --gameplay-cpu-possession-proof $PackPath $TracePath `
        $MidPath $TerminalPath 2>&1)
    $ExitCode = $LASTEXITCODE
    $Text = ($Output -join [Environment]::NewLine).Trim()
    $JsonStart = $Text.IndexOf('{')
    if ($JsonStart -lt 0) {
        throw "CPU full-possession run $Repeat emitted no structured summary.`n$Text"
    }
    try { $Summary = $Text.Substring($JsonStart) | ConvertFrom-Json } catch {
        throw "CPU full-possession run $Repeat emitted invalid summary JSON."
    }
    if (!(Test-Path -LiteralPath $TracePath -PathType Leaf) -or
        !(Test-Path -LiteralPath $MidPath -PathType Leaf) -or
        !(Test-Path -LiteralPath $TerminalPath -PathType Leaf) -or
        $Summary.schema -ne "tecmo.cpu-possession-proof/TGPH-1" -or
        ![bool]$Summary.structured_state_authority -or
        [int]$Summary.outer_update_limit -ne 1085 -or
        [bool]$Summary.anchor_oob) {
        throw "CPU full-possession run $Repeat violated its evidence contract."
    }
    if ($ExpectBaselineFailure) {
        if ($ExitCode -eq 0 -or [bool]$Summary.passed -or
            [bool]$Summary.legitimate_outcome -or
            [string]$Summary.outcome -ne "shot-clock-violation") {
            throw "Run $Repeat did not reproduce the bounded baseline failure."
        }
    } elseif ($ExitCode -ne 0 -or ![bool]$Summary.passed -or
             ![bool]$Summary.legitimate_outcome -or
             [string]$Summary.outcome -eq "shot-clock-violation") {
        throw "CPU full-possession run $Repeat did not resolve legitimately."
    }
    $SummaryPath = Join-Path $RunDirectory "summary.json"
    $Summary | ConvertTo-Json -Depth 8 |
        Set-Content -LiteralPath $SummaryPath -Encoding UTF8
    $Records += [pscustomobject]@{
        trace_sha256 = (Get-FileHash $TracePath -Algorithm SHA256).Hash
        mid_png_sha256 = (Get-FileHash $MidPath -Algorithm SHA256).Hash
        terminal_png_sha256 =
            (Get-FileHash $TerminalPath -Algorithm SHA256).Hash
        summary_sha256 = (Get-FileHash $SummaryPath -Algorithm SHA256).Hash
        summary = $Summary
    }
}

foreach ($Field in 'trace_sha256','mid_png_sha256',
                   'terminal_png_sha256','summary_sha256') {
    if ($Records[0].$Field -ne $Records[1].$Field) {
        throw "CPU full-possession proof was not deterministic: $Field."
    }
}

$ManifestPath = Join-Path $OutputDirectory "manifest.json"
([pscustomobject]@{
    schema = "tecmo.cpu-possession-proof-run/TGPH-1"
    status = if ($ExpectBaselineFailure) {
        "EXPECTED_BASELINE_FAILURE"
    } else { "PASS" }
    assertion_authority = "structured per-frame NDJSON and summary"
    screenshot_scope = "presentation-only"
    records = $Records
} | ConvertTo-Json -Depth 12) |
    Set-Content -LiteralPath $ManifestPath -Encoding UTF8

Write-Output ("CPU full-possession proof complete: status={0} manifest={1}" -f
    $(if ($ExpectBaselineFailure) { "EXPECTED_BASELINE_FAILURE" } else { "PASS" }),
    $ManifestPath)
