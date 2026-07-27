[CmdletBinding()]
param(
    [switch]$Smoke,
    [string]$RomPath,
    [string]$FceuxPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$LuaPath = Join-Path $PSScriptRoot 'gameplay_lab.lua'
$MapPath = Join-Path $PSScriptRoot 'tecmo_rev1_map.lua'
$RunnerPath = Join-Path $PSScriptRoot 'Run-GameplayLab.ps1'
$ReadmePath = Join-Path $PSScriptRoot 'README.md'
foreach ($Path in @($LuaPath, $MapPath, $RunnerPath, $ReadmePath)) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Missing gameplay-lab file: $Path"
    }
}

$Lua = Get-Content -Raw -LiteralPath $LuaPath
$Map = Get-Content -Raw -LiteralPath $MapPath
$Runner = Get-Content -Raw -LiteralPath $RunnerPath
$All = $Lua + "`n" + $Map + "`n" + $Runner
$Failures = [Collections.Generic.List[string]]::new()
function Assert-Lab {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { $script:Failures.Add($Message) }
}

$ForbiddenLua = @(
    'memory\s*\.\s*write(?:byte|word|dword)?\s*\(',
    'memory\s*\.\s*writebyterange\s*\(',
    '\bcheat\s*\.',
    '\bsavestate\s*\.',
    '\bFCEU\s*\.\s*load',
    '\bemu\s*\.\s*load'
)
foreach ($Pattern in $ForbiddenLua) {
    Assert-Lab ($Lua -notmatch $Pattern) "Forbidden Lua capability matched: $Pattern"
}
Assert-Lab ($All -notmatch '(?i)[A-Z]:\\Users\\|[A-Z]:\\Games\\|Tecmo NBA Basketball \(USA\).*\.nes') `
    'A private absolute path or ROM filename was hard-coded.'

Assert-Lab ($Map -match '076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4') `
    'Exact Rev 1 ROM SHA256 is missing.'
Assert-Lab ($Map -match 'F89812F4E9506EF7090D9D0310D368ABD79BACA362B7BFC4A2E7E499754F2A1B') `
    'Exact FCEUX 2.6.6 SHA256 is missing.'
Assert-Lab ($Runner -match 'Get-Process -Name fceux') 'Concurrent-FCEUX rejection is missing.'
Assert-Lab ($Runner -match 'while \(-not \$Process\.WaitForExit\(250\)\)' -and
    $Runner -match '\$Watch\.Elapsed\.TotalSeconds -gt \$TimeoutSeconds') `
    'Polled hard wall-clock timeout is missing.'
Assert-Lab ($Runner -match 'five-second startup sentinel' -and
    $Runner -match 'metadata\.txt' -and $Runner -match 'status\.txt') `
    'Bounded Lua startup sentinel is missing.'
Assert-Lab ($Runner -match 'stopped producing progress for five seconds') `
    'Bounded post-start progress watchdog is missing.'
Assert-Lab ($Runner -match 'RedirectStandardOutput' -and $Runner -match 'RedirectStandardError') `
    'FCEUX output redirection is missing.'
Assert-Lab ($Runner -match 'WindowStyle' -and $Runner -match 'Hidden') 'Hidden-by-default launch is missing.'
Assert-Lab ($Runner -match 'StartsWith\(\$OutputPrefix' -and $Runner -match 'temp-videos\\gameplay-lab') `
    'Ignored-output containment validation is missing.'
Assert-Lab ($Runner -match 'TECMO_GAMEPLAY_LAB_ROM_PATH' -and
    $Runner -match 'TECMO_GAMEPLAY_LAB_FCEUX_PATH') 'Explicit/named input path policy is missing.'

Assert-Lab ($Map -match 'schema = "TGLM-1"' -and $Lua -match 'schema=TGLAB-1') `
    'Map or telemetry schema/version is missing.'
foreach ($Cap in @('max_frames', 'phase_rows', 'event_rows', 'shot_detail_rows',
                    'screenshots', 'tracked_text_bytes')) {
    Assert-Lab ($Map -match [regex]::Escape($Cap)) "Cap '$Cap' is missing."
}
Assert-Lab ($Map -match 'phase_rows = 64' -and $Map -match 'event_rows = 2048' -and
    $Map -match 'shot_detail_rows = 1200' -and $Map -match 'screenshots = 8') `
    'Required row/screenshot caps have changed.'
Assert-Lab ($Lua -match 'telemetry_rows > max_frames' -and
    $Lua -match 'tracked-text byte guard') 'Telemetry or tracked-text guard is missing.'
Assert-Lab ($Lua -match 'if deferred_failure ~= nil then fail\(deferred_failure\)' -and
    $Runner -match 'live 64 MiB whole-session output limit') `
    'Cap failures do not neutralize/stop cleanly.'
Assert-Lab ($Lua -match 'step_ok, step_failure = xpcall\(function\(\)' -and
    $Lua -match 'prefix_ok, prefix_failure = xpcall\(function\(\)' -and
    $Lua -match 'local function finalize_best_effort' -and
    $Lua -match 'pcall\(function\(\) apply_pads\(\{\}, \{\}\) end\)' -and
    $Lua -match 'pcall\(emu\.exit\)') `
    'Protected frame-step cleanup/finalizer is missing.'
Assert-Lab ($Lua -match 'final_pads_neutral = neutral_ok and') `
    'Neutral-pad status does not retain joypad write success.'
Assert-Lab ($Lua -match 'script_sha256=' -and
    $Lua -match 'map_sha256=') 'Script/map hashes are missing from output.'
Assert-Lab ($Runner -match '\$ExpectedStatus' -and
    $Runner -match 'status provenance mismatch') 'Status provenance validation is missing.'

foreach ($Address in @('0x0308', '0x0309', '0x030A', '0x030B', '0x030C',
                       '0x030D', '0x0073', '0x00E8', '0x00F3', '0x046E',
                       '0x044D', '0x0442', '0x0463', '0x0458', '0x048F',
                       '0x0484', '0x04A5', '0x049A', '0x04B0', '0x0435',
                       '0x042A')) {
    Assert-Lab ($Map -match [regex]::Escape($Address)) "World-model address $Address is missing."
}
Assert-Lab ($Map -match 'x = 0x00A0, y = 0x94' -and
    $Map -match 'x = 0x0260, y = 0x94') 'Hoop coordinates are missing.'
Assert-Lab ($Lua -match 'for i = 0, 10 do') 'All actor slots are not emitted.'
Assert-Lab ($Lua -match 'actor_side' -and $Lua -match 'front_sign') `
    'Opponent/front-threat scan is missing.'
Assert-Lab ($Lua -match 'defense_A_edge' -and $Lua -match 'rb\(R\.defense_actor\) == selected_threat') `
    'Observed defensive selection is missing.'
Assert-Lab ($Lua -match 'offense_position_' -and $Lua -match 'no shooter coordinate progress') `
    'Coordinate-feedback positioning/progress deadline is missing.'

Assert-Lab ($Map -match 'x_min = 0x0164' -and $Map -match 'x_max = 0x0170' -and
    $Map -match 'y_min = 0x6C' -and $Map -match 'y_max = 0x74') `
    'Proven shot window is missing.'
Assert-Lab ($Map -match 'stable_frames = 12' -and $Map -match 'hold_b_frames = 8' -and
    $Map -match 'release_frame = 9') 'Stable/press/release schedule is missing.'
Assert-Lab ($Lua -match 'shot hold entered close/foul/violation/control mismatch') `
    'Fail-closed pre-shot route checks are missing.'
Assert-Lab ($Lua -match 'ball\.state == 0' -and $Lua -match 'shooter\.state == 0' -and
    $Lua -match 'ball_distance <= map\.shot_window\.holder_distance' -and
    $Lua -match 'actor_side\(shooter\) == rb\(R\.offense_side\)') `
    'Stable holder proof is missing.'
Assert-Lab ($Lua -match 'rb\(R\.offense_actor\) ~= shot_actor') `
    'Shot hold does not retain the proven selected actor.'

Assert-Lab ($Map -match '0x8C57.*ball_release' -and $Map -match '0x8ABD.*shot_classifier' -and
    $Map -match '0x91BC.*shot_result' -and
    $Map -match '0x933B.*decision_anchor') 'Required shot evidence hooks are missing.'
Assert-Lab ($Lua -match 'classifier_seen = true' -and
    $Lua -match 'release_seen and classifier_seen and result_seen') `
    'Post-armed classifier evidence is not required for pass.'
Assert-Lab ($Map -match '0x942D.*terminal_make_bit7_clear' -and
    $Map -match '0x9434.*terminal_miss_bit7_set') 'Outcome polarity is wrong or missing.'
Assert-Lab ($Lua -match 'make_seen = h\.name == "terminal_make_bit7_clear"' -and
    $Lua -match 'miss_seen = h\.name == "terminal_miss_bit7_set"') `
    'Runtime outcome polarity is wrong.'
Assert-Lab ($Lua -match 'h\.emu_frame == decision_frame' -and
    $Lua -match 'terminal_count == 1') 'Same-frame anchored single-terminal proof is missing.'
Assert-Lab ($Map -match 'route_9c79_optional' -and
    $Lua -notmatch 'route_9c79_optional"\s*then\s+\w+_seen') `
    '$9C79 became a universal pass requirement.'
Assert-Lab ($Map -match '0x8C7D.*close_launch' -and $Lua -match 'close_launch_seen') `
    'Close-route detection is missing.'
Assert-Lab ($Lua -match 'score_apply_seen and \(score0_delta == 2 or score0_delta == 3\)' -and
    $Lua -match 'score1_delta == 0' -and $Lua -match 'settlement_seen') `
    'Make/miss scoring and settlement criteria are incomplete.'
Assert-Lab ($Lua -match 'local dx, dy = a\.x - b\.x, a\.y - b\.y' -and
    $Lua -match 'if \(-dx\) \* front_sign > 0' -and
    $Lua -match 'button = dx >= 0 and "left" or "right"' -and
    $Lua -match 'button = dy >= 0 and "up" or "down"') `
    'Front/away coordinate signs are inconsistent.'

foreach ($Button in @('A', 'B', 'up', 'down', 'left', 'right', 'start', 'select')) {
    Assert-Lab ($Lua -match ([regex]::Escape($Button) + ' = source\.' + [regex]::Escape($Button))) `
        "Complete joypad field '$Button' is missing."
}
Assert-Lab ($Lua -match 'joypad\.set\(1, active_p1\)' -and
    $Lua -match 'joypad\.set\(2, active_p2\)') 'Both pads are not written every frame.'
Assert-Lab ($Lua -match 'final_pads_neutral = neutral_ok and not active_p1' -and
    $Runner -match "final_pads_neutral'\] -ne 'true'") 'Final neutral-pad proof is missing.'
Assert-Lab ($Runner -match 'if \(\$null -ne \$Process -and -not \$Process\.HasExited\)' -and
    $Runner -match '\$Process\.Kill\(\)') 'FCEUX cleanup is missing.'
Assert-Lab ($Lua -match 'requested FM2 recording could not start') `
    'Requested FM2 failure is not fail-closed.'

$RunnerTokens = $null
$RunnerErrors = $null
[void][Management.Automation.Language.Parser]::ParseFile(
    $RunnerPath, [ref]$RunnerTokens, [ref]$RunnerErrors)
Assert-Lab ($RunnerErrors.Count -eq 0) ('Runner PowerShell parse errors: ' +
    (($RunnerErrors | ForEach-Object Message) -join '; '))

if ($Failures.Count -ne 0) {
    $Failures | ForEach-Object { Write-Error $_ }
    throw "$($Failures.Count) gameplay-lab static test(s) failed."
}
Write-Host 'GAMEPLAY LAB STATIC TEST PASS: read-only controller policy, exact revisions, bounded output, world schema, fail-closed shot evidence, neutral cleanup'

if ($Smoke) {
    if (-not $RomPath -or -not $FceuxPath) {
        throw '-Smoke requires explicit -RomPath and -FceuxPath.'
    }
    & $RunnerPath -RomPath $RomPath -FceuxPath $FceuxPath -RequirePass
}
