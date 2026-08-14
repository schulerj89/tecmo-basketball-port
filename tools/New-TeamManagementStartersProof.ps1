param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [string]$OutputRoot,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
if (!$ProjectRoot) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }
$ProjectRoot = (Resolve-Path $ProjectRoot).Path
$BuildDir = Join-Path $ProjectRoot "build"
$Executable = Join-Path $BuildDir "tecmo_port.exe"
if (!$RomPath) {
    $RomPath = Join-Path (Split-Path -Parent $ProjectRoot) `
        "disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes"
}
if (!(Test-Path $RomPath)) { throw "Rev1 ROM not found. Pass -RomPath." }
$RomPath = (Resolve-Path $RomPath).Path
if (!$OutputRoot) {
    $OutputRoot = Join-Path $BuildDir "proof\team-data-starters-slice3"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
$BuildPrefix = [IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/') + `
    [IO.Path]::DirectorySeparatorChar
if (!$OutputRoot.StartsWith($BuildPrefix,
                            [StringComparison]::OrdinalIgnoreCase)) {
    throw "STARTERS proof output must stay under build\."
}
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$Scratch = Join-Path $BuildDir `
    ("starters_proof_" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $Scratch | Out-Null
$PreviousSkipShortcut = $env:TECMO_SKIP_SHORTCUT
$PreviousAssetPack = $env:TECMO_ASSETPACK

function Invoke-Tecmo {
    param([string[]]$Arguments)
    $Output = @(& $Executable @Arguments 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "tecmo_port failed:`n$(@($Output | Select-Object -Last 12) -join "`n")"
    }
    return ($Output -join [Environment]::NewLine)
}

try {
    if (!$SkipBuild) {
        $env:TECMO_SKIP_SHORTCUT = "1"
        $BuildLog = Join-Path $OutputRoot "build.log"
        @(& (Join-Path $ProjectRoot "build.ps1") 2>&1) | Set-Content $BuildLog
        if ($LASTEXITCODE -ne 0 -or
            (Select-String $BuildLog -Pattern 'warning C\d+' -Quiet)) {
            throw "Warning-clean proof build failed."
        }
    }
    $FocusedOutput = @(& (Join-Path $PSScriptRoot `
        "Run-TeamManagementTests.ps1") -ProjectRoot $ProjectRoot `
        -RomPath $RomPath -SkipBuild 6>&1 2>&1)
    if (($FocusedOutput -join [Environment]::NewLine) -notmatch
            'TEAM MANAGEMENT TEST PASS') {
        throw "Focused STARTERS validation failed before proof capture:`n$(@($FocusedOutput | Select-Object -Last 12) -join "`n")"
    }
    $Pack = Join-Path $Scratch "team-management.assetpack"
    [void](Invoke-Tecmo @("--build-assetpack", $RomPath, $Pack))
    $env:TECMO_ASSETPACK = $Pack
    $Specs = @(
        @{ Mode="team-data-starters"; File="atlanta-base.png";
           Sha256="D0333C619C19BD710ACBEF302162BBB12E073D7DFE30A5877743E6603F937FE4";
           Matches=@(
             'team-data-starters team=0 palette-group=1 logo=16,48,10,6 view=1 lineup-cursor=1,16,113 bench-cursor=0,0,0',
             'lineup=0:G:"R.ROBINSON"\|1:G:"S.AUGMON"\|2:F:"D.WILKINS"\|3:F:"K.WILLIS"\|4:C:"B.RASMUSSEN"',
             'bench=5:G:"M.WILEY"\|6:G:"R.MONROE"\|7:G:"M.CHEEKS"\|8:F:"D.FERRELL"\|9:F:"P.GRAHAM"\|10:F:"A.VOLKOV"\|11:C:"J.KONCAK"') },
        @{ Mode="team-data-starters-chicago"; File="chicago-base.png";
           Sha256="EE6D62F36CA40A817D469A1F01C39EEC52721CAE74DEE329032A7AA79C055687";
           Matches=@(
             'team-data-starters team=3 palette-group=1 logo=32,48,6,6 view=1 lineup-cursor=1,16,113 bench-cursor=0,0,0',
             'lineup=0:G:"J.PAXSON"\|1:G:"M.JORDAN"\|2:F:"S.PIPPEN"\|3:F:"H.GRANT"\|4:C:"B.CARTWRIGH"',
             'bench=5:G:"B.ARMSTRONG"\|6:G:"B.HANSEN"\|7:G:"C.HODGES"\|8:F:"S.KING"\|9:F:"C.LEVINGSTO"\|10:C:"S.WILLIAMS"\|11:C:"W.PERDUE"') },
        @{ Mode="team-data-starters-chicago-row3";
           File="chicago-row3-cursor.png";
           Sha256="29923AC9C37003A15AF65AF44E7F044D0D7C5AFA361BA31896F67C60E757543D";
           Matches=@('lineup-cursor=1,24,145 bench-cursor=0,0,0') },
        @{ Mode="team-data-starters-chicago-substituted";
           File="chicago-substituted.png";
           Sha256="73B66F80E4162040736F03853376AFC8D8AD95C04BD6D920F6936B3F1ECA6C4E";
           Matches=@('lineup=5:G:"B.ARMSTRONG"\|1:G:"M.JORDAN"',
                     'bench=0:G:"J.PAXSON"\|6:G:"B.HANSEN"') },
        @{ Mode="team-data-starters-bench"; File="atlanta-bench-cursors.png";
           Sha256="61CD4960D71E61B4405D0C327980A3265B88A0E4F1AF554937089649F3148F87";
           Matches=@('view=3 lineup-cursor=1,24,129 bench-cursor=1,144,129') },
        @{ Mode="team-data-roster-chicago"; File="chicago-roster.png";
           Sha256="F8B527B5EABD65BB5FB7E7EE14106161B2BC97D522066534DAA6A48270DCCDB4";
           Matches=@('team-data-state phase=PLAYERS DATA .* team=3 ') },
        @{ Mode="team-data-roster-chicago-substituted";
           File="chicago-roster-substituted.png";
           Sha256="F8B527B5EABD65BB5FB7E7EE14106161B2BC97D522066534DAA6A48270DCCDB4";
           Matches=@('team-data-state phase=PLAYERS DATA .* team=3 ') }
    )
    $Records = @()
    foreach ($Spec in $Specs) {
        $Path = Join-Path $OutputRoot $Spec.File
        $Diagnostics = Invoke-Tecmo @("--render-test-mode", $Spec.Mode, $Path)
        $ActualHash = (Get-FileHash $Path -Algorithm SHA256).Hash
        if ($ActualHash -ne $Spec.Sha256) {
            throw "Pinned proof hash mismatch for $($Spec.Mode): $ActualHash"
        }
        foreach ($Pattern in $Spec.Matches) {
            if ($Diagnostics -notmatch $Pattern) {
                throw "Structured proof mismatch for $($Spec.Mode): $Pattern"
            }
        }
        $Records += [ordered]@{
            mode = $Spec.Mode
            file = $Spec.File
            sha256 = $ActualHash
            validation = "pinned-sha256-and-structured-diagnostics-pass"
            diagnostics = @($Diagnostics -split "`r?`n" | Where-Object {
                $_ -match '^team-data-(state|starters) '
            })
        }
    }
    $RosterHashes = @($Records | Where-Object {
        $_.mode -like 'team-data-roster-chicago*'
    } | ForEach-Object { $_.sha256 } | Select-Object -Unique)
    if ($RosterHashes.Count -ne 1) {
        throw "Ordinary Chicago roster changed with starter substitution."
    }
    $Manifest = [ordered]@{
        proof = "fresh-real-team-starters-slice3"
        generated_utc = [DateTime]::UtcNow.ToString("o")
        source_contract = [ordered]@{
            ttmg_size = 21061
            ttmg_fnv1a32 = "D192EAC6"
            real_team_range = @(0, 26)
            injured = "empty-deferred-no-condition-seed-proxy"
            reset_modal = "preexisting-transient-approximation"
            all_star_visuals = "fail-closed"
        }
        focused_validation = "Run-TeamManagementTests.ps1 PASS"
        assertions = @(
            "selected TTDT profile palette and existing TTDT logo",
            "five lineup plus ascending seven-player nonstarter complement",
            "initial-dot-surname formatting and fixed/dynamic position ownership",
            "Bank03 config 0D/0F framebuffer-visible cursor coordinates",
            "substitution changes presentation but not ordinary PLAYERS DATA roster"
        )
        frames = $Records
    }
    $Manifest | ConvertTo-Json -Depth 8 | Set-Content `
        (Join-Path $OutputRoot "manifest.json")
    Write-Host "STARTERS PROOF PASS: $OutputRoot"
} finally {
    $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    $env:TECMO_ASSETPACK = $PreviousAssetPack
    if (Test-Path $Scratch) {
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
}
