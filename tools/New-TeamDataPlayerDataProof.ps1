param(
    [string]$ProjectRoot,
    [Parameter(Mandatory = $true)]
    [string]$RomPath,
    [string]$OutputRoot,
    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$RomPath = (Resolve-Path -LiteralPath $RomPath).Path
if (!$OutputRoot) {
    $OutputRoot = Join-Path $ProjectRoot "build\proof\team-data-player-data"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
$Executable = Join-Path $ProjectRoot "build\tecmo_port.exe"
$BuildScript = Join-Path $ProjectRoot "build.ps1"
$ProvenancePath = Join-Path $ProjectRoot "docs\team-data-player-detail-provenance.json"
$PreviousAssetPack = $env:TECMO_ASSETPACK
$PreviousSkipShortcut = $env:TECMO_SKIP_SHORTCUT
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)

function Invoke-Tecmo {
    param([string[]]$Arguments)
    $Output = @(& $Executable @Arguments 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw ("tecmo_port failed ({0}): {1}" -f $LASTEXITCODE,
               ($Output -join [Environment]::NewLine))
    }
    return ($Output -join [Environment]::NewLine)
}

try {
    if (!(Test-Path -LiteralPath $ProvenancePath -PathType Leaf)) {
        throw "TEAM DATA player-detail provenance was not found at $ProvenancePath."
    }
    $Provenance = Get-Content -LiteralPath $ProvenancePath -Raw |
        ConvertFrom-Json
    if ($Provenance.schema -ne
            "tecmo.team-data.player-detail-provenance/1" -or
        $Provenance.cursor.generic_record_delta[1] -ne -4 -or
        $Provenance.statistics_ownership.native_source -ne
            "TecmoSeasonSession.player_stats_totals") {
        throw "TEAM DATA player-detail provenance contract was malformed."
    }
    if (!$SkipBuild) {
        $env:TECMO_SKIP_SHORTCUT = "1"
        & $BuildScript
        if ($LASTEXITCODE -ne 0) {
            throw "Native build failed before TEAM DATA proof rendering."
        }
    }
    if (!(Test-Path -LiteralPath $Executable -PathType Leaf)) {
        throw "Native executable was not found at $Executable."
    }

    New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
    $AssetPack = Join-Path $OutputRoot "team-data-player-data-proof.assetpack"
    [void](Invoke-Tecmo @("--build-assetpack", $RomPath, $AssetPack))
    $env:TECMO_ASSETPACK = $AssetPack

    $Modes = @(
        [ordered]@{
            mode = "team-data-profile"
            file = "team-data-profile-cursor.png"
            purpose = "Bank01 `$8031 generic cursor at the profile OAM anchor"
        },
        [ordered]@{
            mode = "team-data-roster-page1"
            file = "team-data-roster-cursor.png"
            purpose = "Bank01 `$8031 generic cursor at the roster OAM anchor"
        },
        [ordered]@{
            mode = "team-data-player-detail"
            file = "team-data-player-detail-fresh.png"
            purpose = "fresh-season player-detail row is known zero"
        },
        [ordered]@{
            mode = "team-data-player-detail-populated"
            file = "team-data-player-detail-populated.png"
            purpose = "ledger-seeded row is .600/.875/.500 and 21.0; unsupported counters are ---"
        }
    )
    $Frames = @()
    foreach ($Mode in $Modes) {
        $Png = Join-Path $OutputRoot $Mode.file
        $Status = Invoke-Tecmo @("--render-test-mode", $Mode.mode, $Png)
        if (!(Test-Path -LiteralPath $Png -PathType Leaf)) {
            throw "TEAM DATA proof renderer did not create $($Mode.file)."
        }
        $Frames += [ordered]@{
            mode = $Mode.mode
            file = $Mode.file
            purpose = $Mode.purpose
            sha256 = (Get-FileHash -LiteralPath $Png -Algorithm SHA256).Hash
            renderer_status = $Status
        }
    }
    $Manifest = [ordered]@{
        schema = "tecmo.team-data.player-data-proof/1"
        provenance = "docs/team-data-player-detail-provenance.json"
        source = "ROM-derived native asset pack; no decompilation or capture input is read by the renderer"
        frames = $Frames
    }
    $ManifestPath = Join-Path $OutputRoot "team-data-player-data-proof.json"
    [IO.File]::WriteAllText(
        $ManifestPath,
        ($Manifest | ConvertTo-Json -Depth 5),
        $Utf8NoBom)
    Write-Output "TEAM DATA PLAYER-DATA PROOF PASS: $ManifestPath"
} finally {
    $env:TECMO_ASSETPACK = $PreviousAssetPack
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
}
