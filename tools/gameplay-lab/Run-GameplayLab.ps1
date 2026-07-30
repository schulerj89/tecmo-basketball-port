[CmdletBinding()]
param(
    [string]$RomPath,
    [string]$FceuxPath,
    [ValidateSet('three_point_baseline', 'ordinary_two_point_make')]
    [string]$Profile = 'three_point_baseline',
    [ValidateRange(3600, 7200)]
    [int]$MaxFrames = 6000,
    [ValidateRange(30, 180)]
    [int]$TimeoutSeconds = 120,
    [switch]$RecordMovie,
    [switch]$Visible,
    [switch]$RequirePass
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ExpectedRomSha256 = '076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4'
$ExpectedFceuxSha256 = 'F89812F4E9506EF7090D9D0310D368ABD79BACA362B7BFC4A2E7E499754F2A1B'
$RomPath = if ($RomPath) { $RomPath } else { $env:TECMO_GAMEPLAY_LAB_ROM_PATH }
$FceuxPath = if ($FceuxPath) { $FceuxPath } else { $env:TECMO_GAMEPLAY_LAB_FCEUX_PATH }
if (-not $RomPath) {
    throw 'Pass -RomPath or set TECMO_GAMEPLAY_LAB_ROM_PATH.'
}
if (-not $FceuxPath) {
    throw 'Pass -FceuxPath or set TECMO_GAMEPLAY_LAB_FCEUX_PATH.'
}

$LuaPath = Join-Path $PSScriptRoot 'gameplay_lab.lua'
$MapPath = Join-Path $PSScriptRoot 'tecmo_rev1_map.lua'
foreach ($RequiredPath in @($RomPath, $FceuxPath, $LuaPath, $MapPath)) {
    if (-not (Test-Path -LiteralPath $RequiredPath -PathType Leaf)) {
        throw "Required file was not found: $RequiredPath"
    }
}
if (@(Get-Process -Name fceux -ErrorAction SilentlyContinue).Count -ne 0) {
    throw 'FCEUX is already running. Close it before starting the gameplay laboratory.'
}

$RomHash = (Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash.ToUpperInvariant()
$FceuxHash = (Get-FileHash -LiteralPath $FceuxPath -Algorithm SHA256).Hash.ToUpperInvariant()
if ($RomHash -ne $ExpectedRomSha256) {
    throw "Wrong ROM revision. Expected $ExpectedRomSha256 but got $RomHash."
}
if ($FceuxHash -ne $ExpectedFceuxSha256) {
    throw "Wrong FCEUX build. Expected $ExpectedFceuxSha256 but got $FceuxHash."
}

$ProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$OutputBase = [IO.Path]::GetFullPath((Join-Path $ProjectRoot 'temp-videos\gameplay-lab'))
$ProjectPrefix = $ProjectRoot.TrimEnd('\') + '\'
if (-not $OutputBase.StartsWith($ProjectPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Gameplay-lab output base escaped the project root.'
}
[void](New-Item -ItemType Directory -Force -Path $OutputBase)
$SessionName = Get-Date -Format 'yyyyMMdd-HHmmss'
$SessionPath = Join-Path $OutputBase $SessionName
$Suffix = 1
while (Test-Path -LiteralPath $SessionPath) {
    $SessionPath = Join-Path $OutputBase ('{0}-{1:D2}' -f $SessionName, $Suffix)
    $Suffix++
}
$SessionPath = [IO.Path]::GetFullPath($SessionPath)
$OutputPrefix = $OutputBase.TrimEnd('\') + '\'
if (-not $SessionPath.StartsWith($OutputPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Gameplay-lab session path escaped its ignored output directory.'
}
[void](New-Item -ItemType Directory -Path $SessionPath)

$ScriptHash = (Get-FileHash -LiteralPath $LuaPath -Algorithm SHA256).Hash.ToUpperInvariant()
$MapHash = (Get-FileHash -LiteralPath $MapPath -Algorithm SHA256).Hash.ToUpperInvariant()
$Previous = @{}
$EnvironmentNames = @(
    'TECMO_GAMEPLAY_LAB_OUTPUT', 'TECMO_GAMEPLAY_LAB_MAP',
    'TECMO_GAMEPLAY_LAB_PROFILE',
    'TECMO_GAMEPLAY_LAB_MAX_FRAMES', 'TECMO_GAMEPLAY_LAB_RECORD_MOVIE',
    'TECMO_GAMEPLAY_LAB_SCRIPT_SHA256', 'TECMO_GAMEPLAY_LAB_MAP_SHA256',
    'TECMO_GAMEPLAY_LAB_ROM_SHA256', 'TECMO_GAMEPLAY_LAB_FCEUX_SHA256'
)
foreach ($Name in $EnvironmentNames) {
    $Previous[$Name] = [Environment]::GetEnvironmentVariable($Name, 'Process')
}

$Process = $null
try {
    $env:TECMO_GAMEPLAY_LAB_OUTPUT = $SessionPath
    $env:TECMO_GAMEPLAY_LAB_MAP = $MapPath
    $env:TECMO_GAMEPLAY_LAB_PROFILE = $Profile
    $env:TECMO_GAMEPLAY_LAB_MAX_FRAMES = [string]$MaxFrames
    $env:TECMO_GAMEPLAY_LAB_RECORD_MOVIE = if ($RecordMovie) { '1' } else { '0' }
    $env:TECMO_GAMEPLAY_LAB_SCRIPT_SHA256 = $ScriptHash
    $env:TECMO_GAMEPLAY_LAB_MAP_SHA256 = $MapHash
    $env:TECMO_GAMEPLAY_LAB_ROM_SHA256 = $RomHash
    $env:TECMO_GAMEPLAY_LAB_FCEUX_SHA256 = $FceuxHash

    $ArgumentLine = '-sound 0 -lua "{0}" "{1}"' -f $LuaPath, $RomPath
    $Start = @{
        FilePath = $FceuxPath
        ArgumentList = $ArgumentLine
        WorkingDirectory = Split-Path -Parent $FceuxPath
        PassThru = $true
        RedirectStandardOutput = Join-Path $SessionPath 'fceux.stdout.log'
        RedirectStandardError = Join-Path $SessionPath 'fceux.stderr.log'
    }
    if (-not $Visible) {
        $Start.WindowStyle = 'Hidden'
    }
    $Process = Start-Process @Start
    Set-Content -LiteralPath (Join-Path $SessionPath 'fceux.pid') -Value $Process.Id -Encoding ASCII
    $OutputLimitBytes = 64MB
    $Watch = [Diagnostics.Stopwatch]::StartNew()
    $StartupSentinelSeen = $false
    $LastProgressTicks = 0L
    $LastProgressSeconds = 0.0
    while (-not $Process.WaitForExit(250)) {
        $RunningOutputBytes = (Get-ChildItem -LiteralPath $SessionPath -File |
            Measure-Object -Property Length -Sum).Sum
        if ($null -eq $RunningOutputBytes) { $RunningOutputBytes = 0 }
        if ($RunningOutputBytes -gt $OutputLimitBytes) {
            $Process.Kill()
            $Process.WaitForExit()
            throw "Gameplay laboratory exceeded its live 64 MiB whole-session output limit. Session: $SessionPath"
        }
        if (-not $StartupSentinelSeen) {
            $StartupSentinelSeen =
                (Test-Path -LiteralPath (Join-Path $SessionPath 'metadata.txt') -PathType Leaf) -or
                (Test-Path -LiteralPath (Join-Path $SessionPath 'status.txt') -PathType Leaf)
            if (-not $StartupSentinelSeen -and $Watch.Elapsed.TotalSeconds -gt 5) {
                $Process.Kill()
                $Process.WaitForExit()
                throw "Gameplay laboratory did not reach its five-second startup sentinel. Session: $SessionPath"
            }
        }
        if ($StartupSentinelSeen) {
            $ProgressTicks = 0L
            foreach ($ProgressName in @('status.txt', 'telemetry.csv')) {
                $ProgressPath = Join-Path $SessionPath $ProgressName
                if (Test-Path -LiteralPath $ProgressPath -PathType Leaf) {
                    $Ticks = (Get-Item -LiteralPath $ProgressPath).LastWriteTimeUtc.Ticks
                    if ($Ticks -gt $ProgressTicks) { $ProgressTicks = $Ticks }
                }
            }
            if ($ProgressTicks -gt $LastProgressTicks) {
                $LastProgressTicks = $ProgressTicks
                $LastProgressSeconds = $Watch.Elapsed.TotalSeconds
            } elseif ($Watch.Elapsed.TotalSeconds - $LastProgressSeconds -gt 5) {
                $Process.Kill()
                $Process.WaitForExit()
                throw "Gameplay laboratory stopped producing progress for five seconds. Session: $SessionPath"
            }
        }
        if ($Watch.Elapsed.TotalSeconds -gt $TimeoutSeconds) {
            $Process.Kill()
            $Process.WaitForExit()
            throw "Gameplay laboratory exceeded its $TimeoutSeconds-second wall-clock limit. Session: $SessionPath"
        }
    }
    $Watch.Stop()
    $Process.Refresh()
    if ($null -ne $Process.ExitCode -and $Process.ExitCode -ne 0) {
        throw "FCEUX exited with code $($Process.ExitCode). Session: $SessionPath"
    }

    $StatusPath = Join-Path $SessionPath 'status.txt'
    if (-not (Test-Path -LiteralPath $StatusPath -PathType Leaf)) {
        throw "Gameplay laboratory did not produce status.txt. Session: $SessionPath"
    }
    $Status = @{}
    foreach ($Line in Get-Content -LiteralPath $StatusPath) {
        if ($Line -notmatch '^([a-z0-9_]+)=(.*)$') {
            throw "Gameplay laboratory status contained a malformed row. Session: $SessionPath"
        }
        $Name = $Matches[1]
        if ($Status.ContainsKey($Name)) {
            throw "Gameplay laboratory status contained duplicate '$Name'. Session: $SessionPath"
        }
        $Status[$Name] = $Matches[2]
    }
    $ExpectedStatus = @{
        schema = 'TGLAB-4'
        schema_version = '4'
        map_schema = 'TGLM-4'
        profile = $Profile
        script_sha256 = $ScriptHash
        map_sha256 = $MapHash
        rom_sha256 = $RomHash
        fceux_sha256 = $FceuxHash
    }
    foreach ($Name in $ExpectedStatus.Keys) {
        if ($Status[$Name] -ne $ExpectedStatus[$Name]) {
            throw "Gameplay laboratory status provenance mismatch for '$Name'. Session: $SessionPath"
        }
    }
    if ($Profile -eq 'ordinary_two_point_make') {
        $RequiredTimingStatus = @(
            'timing_evidence_valid', 'defender_cycles_closed',
            'defender_confirmed_stores', 'defender_cycle_unique_actors',
            'holder_passes', 'holder_changes', 'b100_entry_count',
            'captured_target', 'captured_shooter_position',
            'captured_slot10_position', 'captured_slot10_count',
            'captured_slot10_altitude', 'captured_slot10_altitude_velocity',
            'captured_slot10_horizontal_velocity',
            'captured_slot10_vertical_velocity',
            'captured_pre_remap_direction', 'captured_post_remap_direction',
            'captured_launch_direction', 'captured_launch_phase_low',
            'captured_launch_close_mode', 'captured_solver_direction',
            'captured_solver_phase_low', 'captured_solver_close_mode',
            'captured_score_shot_clock', 'state08_route_updates',
            'captured_terminal_mailboxes',
            'score_apply_snapshot', 'score_commit_snapshot',
            'score_frame_delta', 'handoff_frame_delta', 'actual_swap'
        )
        foreach ($Name in $RequiredTimingStatus) {
            if (-not $Status.ContainsKey($Name)) {
                throw "Gameplay laboratory status omitted '$Name'. Session: $SessionPath"
            }
        }
        foreach ($Name in @(
            'defender_cycles_closed', 'defender_confirmed_stores',
            'defender_cycle_unique_actors', 'holder_passes', 'holder_changes',
            'b100_entry_count', 'captured_slot10_count',
            'captured_slot10_altitude', 'captured_slot10_altitude_velocity',
            'captured_slot10_horizontal_velocity',
            'captured_slot10_vertical_velocity',
            'captured_pre_remap_direction', 'captured_post_remap_direction',
            'captured_launch_direction', 'captured_launch_phase_low',
            'captured_launch_close_mode', 'captured_solver_direction',
            'captured_solver_phase_low', 'captured_solver_close_mode',
            'captured_score_shot_clock', 'state08_route_updates',
            'score_frame_delta', 'handoff_frame_delta'
        )) {
            $Parsed = 0
            if (-not [int]::TryParse($Status[$Name], [ref]$Parsed)) {
                throw "Gameplay laboratory status '$Name' was malformed. Session: $SessionPath"
            }
        }
        if ($Status['captured_target'] -notmatch '^-?\d+,-?\d+$' -or
                $Status['captured_shooter_position'] -notmatch '^-?\d+,-?\d+$' -or
                $Status['captured_slot10_position'] -notmatch '^-?\d+,-?\d+$' -or
                $Status['score_apply_snapshot'] -notmatch '^-?\d+,-?\d+$' -or
                $Status['score_commit_snapshot'] -notmatch '^-?\d+,-?\d+$' -or
                $Status['captured_terminal_mailboxes'] -notmatch '^-?\d+,-?\d+,-?\d+$' -or
                $Status['actual_swap'] -notmatch '^-?\d+,-?\d+,-?\d+,-?\d+$' -or
                $Status['timing_evidence_valid'] -notin @('true', 'false')) {
            throw "Gameplay laboratory timing status was malformed. Session: $SessionPath"
        }
        if ($Status['pilot_pass'] -eq 'true' -and
                $Status['timing_evidence_valid'] -ne 'true') {
            throw "Gameplay laboratory two-point pass lacked timing evidence. Session: $SessionPath"
        }
        if ($Status['pilot_pass'] -eq 'true') {
            $ExactTimingStatus = @{
                captured_target = '160,143'
                captured_slot10_count = '60'
                captured_slot10_altitude = '14592'
                captured_slot10_altitude_velocity = '1260'
                captured_pre_remap_direction = '5'
                captured_post_remap_direction = '0'
                captured_launch_direction = '0'
                captured_launch_phase_low = '5'
                captured_launch_close_mode = '0'
                captured_solver_direction = '0'
                captured_solver_phase_low = '5'
                captured_solver_close_mode = '0'
                captured_score_shot_clock = '24'
                b100_entry_count = '63'
                state08_route_updates = '26'
                captured_terminal_mailboxes = '11,11,11'
            }
            foreach ($Name in $ExactTimingStatus.Keys) {
                if ($Status[$Name] -ne $ExactTimingStatus[$Name]) {
                    throw "Gameplay laboratory two-point pass violated exact '$Name'. Session: $SessionPath"
                }
            }
            $HorizontalVelocity = [int]$Status['captured_slot10_horizontal_velocity']
            $VerticalVelocity = [int]$Status['captured_slot10_vertical_velocity']
            if ($HorizontalVelocity -lt 65416 -or $HorizontalVelocity -gt 65423 -or
                    $VerticalVelocity -lt 29 -or $VerticalVelocity -gt 38) {
                throw "Gameplay laboratory two-point pass violated exact velocity bounds. Session: $SessionPath"
            }
        }
    }
    if ($Status['final_pads_neutral'] -ne 'true') {
        throw "Gameplay laboratory did not prove neutral final pads. Session: $SessionPath"
    }
    $TotalOutputBytes = (Get-ChildItem -LiteralPath $SessionPath -File |
        Measure-Object -Property Length -Sum).Sum
    if ($null -eq $TotalOutputBytes) { $TotalOutputBytes = 0 }
    if ($TotalOutputBytes -gt $OutputLimitBytes) {
        throw "Gameplay laboratory exceeded its 64 MiB whole-session output limit. Session: $SessionPath"
    }
    if ($RequirePass -and $Status['pilot_pass'] -ne 'true') {
        throw "Gameplay laboratory pilot did not pass: $($Status['stop_reason']). Session: $SessionPath"
    }

    [pscustomobject]@{
        Session = $SessionPath
        Result = $Status['result']
        Pass = $Status['pilot_pass'] -eq 'true'
        Profile = $Status['profile']
        StopReason = $Status['stop_reason']
        Frames = [int]$Status['lab_frame']
        OutputBytes = [long]$TotalOutputBytes
        RomSha256 = $RomHash
        FceuxSha256 = $FceuxHash
    }
}
finally {
    if ($null -ne $Process -and -not $Process.HasExited) {
        try { $Process.Kill(); $Process.WaitForExit() } catch {}
    }
    foreach ($Name in $EnvironmentNames) {
        [Environment]::SetEnvironmentVariable($Name, $Previous[$Name], 'Process')
    }
}
