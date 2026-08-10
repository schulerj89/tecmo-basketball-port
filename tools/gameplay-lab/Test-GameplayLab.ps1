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
$CpuLuaPath = Join-Path $PSScriptRoot 'tecmo_cpu_lifecycle.lua'
$CpuMapPath = Join-Path $PSScriptRoot 'tecmo_cpu_lifecycle_rev1_map.lua'
$CpuRunnerPath = Join-Path $PSScriptRoot 'Run-GameplayCpuLifecycleProof.ps1'
$CpuDocsRoot = Join-Path $PSScriptRoot '..\..\docs\finish-tasks\R1-cpu-play-lifecycle'
$CpuProofDocPath = Join-Path $CpuDocsRoot 'PROOF.md'
$CpuLineageDocPath = Join-Path $CpuDocsRoot 'LINEAGE.md'
$CpuManifestPath = Join-Path $CpuDocsRoot 'proof-manifest.template.json'
foreach ($Path in @($LuaPath, $MapPath, $RunnerPath, $ReadmePath)) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Missing gameplay-lab file: $Path"
    }
}
foreach ($Path in @($CpuLuaPath, $CpuMapPath, $CpuRunnerPath)) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Missing CPU lifecycle lab file: $Path"
    }
}
foreach ($Path in @($CpuProofDocPath, $CpuLineageDocPath, $CpuManifestPath)) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Missing CPU lifecycle task document: $Path"
    }
}

$Lua = Get-Content -Raw -LiteralPath $LuaPath
$Map = Get-Content -Raw -LiteralPath $MapPath
$Runner = Get-Content -Raw -LiteralPath $RunnerPath
$All = $Lua + "`n" + $Map + "`n" + $Runner
$CpuLua = Get-Content -Raw -LiteralPath $CpuLuaPath
$CpuMap = Get-Content -Raw -LiteralPath $CpuMapPath
$CpuRunner = Get-Content -Raw -LiteralPath $CpuRunnerPath
$CpuProofDoc = Get-Content -Raw -LiteralPath $CpuProofDocPath
$CpuLineageDoc = Get-Content -Raw -LiteralPath $CpuLineageDocPath
$CpuManifest = Get-Content -Raw -LiteralPath $CpuManifestPath
$CpuAll = $CpuLua + "`n" + $CpuMap + "`n" + $CpuRunner
$ProgressFunctionText = [regex]::Match($CpuRunner,
    '(?s)function Get-ProgressSnapshot \{.*?\r?\n\}\r?\nfunction Get-Fnv1a32').Value
$TraceHeaderText = [regex]::Match($CpuLua, 'local trace_header = "([^"]+)"').Groups[1].Value -replace '\\n$', ''
$TraceFormatText = [regex]::Match($CpuLua, 'local trace_format = "([^"]+)"').Groups[1].Value
$TraceValuesText = [regex]::Match($CpuLua, '(?s)local trace_values = \{(.*?)\r?\n\s*\}').Groups[1].Value
$TraceHeaderColumns = @($TraceHeaderText.Split(',')).Count
$TraceConversionTokens = @([regex]::Matches($TraceFormatText, '%[0-9]*[a-zA-Z]') |
    ForEach-Object { $_.Value })
$TraceValueIndexes = @([regex]::Matches($TraceValuesText, '(?m)^\s*\[(\d+)\]\s*=') |
    ForEach-Object { [int]$_.Groups[1].Value })
$RegisterExecCallbackText = [regex]::Match($CpuLua,
    '(?s)memory\.registerexec\(hook\.address, function\(\).*?\r?\n\s*end\)').Value
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

Assert-Lab ($Map -match 'schema = "TGLM-4"' -and $Map -match 'schema_version = 4' -and
    $Lua -match 'map\.schema == "TGLM-4" and map\.schema_version == 4' -and
    $Lua -match 'schema=TGLAB-4\\nschema_version=4' -and
    $Runner -match "schema = 'TGLAB-4'" -and $Runner -match "schema_version = '4'" -and
    $Runner -match "map_schema = 'TGLM-4'") `
    'Map or telemetry schema/version is missing.'
Assert-Lab ($Runner -match "ValidateSet\('three_point_baseline', 'ordinary_two_point_make'\)" -and
    $Runner -match "\[string\]\`$Profile = 'three_point_baseline'" -and
    $Runner -match 'TECMO_GAMEPLAY_LAB_PROFILE' -and
    $Lua -match 'profile_name == "three_point_baseline"' -and
    $Lua -match 'profile_name == "ordinary_two_point_make"') `
    'Closed runner profile selection is missing.'
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
Assert-Lab ($Runner -match "Line -notmatch '\^\(\[a-z0-9_\]\+\)=\(\.\*\)\$'" -and
    $Runner -match 'Status\.ContainsKey\(\$Name\)' -and
    $Runner -match "Profile -eq 'ordinary_two_point_make'" -and
    $Runner -match '\$RequiredTimingStatus' -and
    $Runner -match '\[int\]::TryParse' -and
    $Runner -match "timing_evidence_valid'\] -ne 'true'") `
    'Strict two-point status parsing/missing/malformed evidence rejection is absent.'

foreach ($Address in @('0x0308', '0x0309', '0x030A', '0x030B', '0x030C',
                       '0x030D', '0x0073', '0x00E8', '0x00F3', '0x046E',
                       '0x044D', '0x0442', '0x0463', '0x0458', '0x048F',
                       '0x0484', '0x04A5', '0x049A', '0x04B0', '0x0435',
                       '0x042A', '0x04E7', '0x04F2', '0x04FD', '0x0508',
                       '0x038D', '0x038E', '0x038F', '0x0390')) {
    Assert-Lab ($Map -match [regex]::Escape($Address)) "World-model address $Address is missing."
}
Assert-Lab ($Map -match 'actor_altitude_velocity_lo = 0x049A' -and
    $Map -match 'actor_altitude_velocity_hi = 0x04A5' -and
    $Map -notmatch '(?m)^\s*actor_velocity_(?:lo|hi)\s*=') `
    '$049A/$04A5 are not named as actor altitude velocity.'
Assert-Lab ($Lua -match 'a%d_altitude_velocity' -and
    $Lua -match 'a%d_horizontal_velocity' -and $Lua -match 'a%d_vertical_velocity' -and
    $Lua -match 'shooter_altitude_velocity,shooter_horizontal_velocity,shooter_vertical_velocity' -and
    $Lua -match 'ball_altitude_velocity,ball_horizontal_velocity,ball_vertical_velocity') `
    'Raw per-actor velocity fields are missing from telemetry or shot detail.'
Assert-Lab ($Map -match 'x = 0x00A0, y = 0x94' -and
    $Map -match 'x = 0x0260, y = 0x94') 'Hoop coordinates are missing.'
Assert-Lab ($Lua -match 'for i = 0, 10 do') 'All actor slots are not emitted.'
Assert-Lab ($Lua -match 'actor_side' -and $Lua -match 'front_sign') `
    'Opponent/front-threat scan is missing.'
Assert-Lab ($Map -match '0x91CB.*defender_switch_store.*gate = "bank06"' -and
    $Lua -match 'switch_pending_actor = hook_a' -and
    $Lua -match 'rb\(R\.defense_actor\) ~= switch_pending_actor' -and
    $Lua -match 'switch_saw_different and confirmed == switch_origin' -and
    $Lua -match 'defender_store_cap' -and
    $Lua -match 'defender_store_total > map\.shot_window\.defender_store_cap' -and
    $Lua -match 'defender_store_total >= map\.shot_window\.defender_store_cap' -and
    $Lua -notmatch 'switch_attempts >= 10') `
    'Confirmed pre-store defensive cycle/closure transaction is missing.'
Assert-Lab ($Lua -match 'offense_position_' -and $Lua -match 'no shooter coordinate progress') `
    'Coordinate-feedback positioning/progress deadline is missing.'

Assert-Lab ($Map -match 'three_point_baseline = \{[\s\S]*?x_min = 0x0164, x_max = 0x0170,[\s\S]*?y_min = 0x6C, y_max = 0x74' -and
    $Map -match 'ordinary_two_point_make = \{[\s\S]*?expected_point_value = 2,[\s\S]*?expected_make = true,[\s\S]*?expected_score_delta = 2,[\s\S]*?x_min = 0x0108, x_max = 0x010F,[\s\S]*?y_min = 0x6C, y_max = 0x74') `
    'Closed three-point/two-point shot windows are missing.'
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
Assert-Lab ($Map -match 'holder_transfer_cap = 4' -and
    $Map -match 'holder_transfer_deadline = 90' -and
    $Lua -match 'phase == "pass_neutral"' -and
    $Lua -match 'controller_readback\(1\) == 0 and controller_readback\(2\) == 0' -and
    $Lua -match 'last_action = "offense_A_single_pass"' -and
    $Lua -match 'phase == "pass_wait_holder"' -and
    $Lua -match 'new_holder ~= pass_origin_holder' -and
    $Lua -match 'holder_seen\[new_holder\]' -and
    $Lua -match 'phase == "reacquire_holder"' -and
    $Lua -match 'reacquire_stable >= 8' -and
    $Lua -match 'last_action = "await_stable_reacquired_holder"' -and
    $Lua -match 'reacquire stable-holder deadline' -and
    $Lua -match 'holder_proven = false' -and
    $Lua -match 'holder pass transfer deadline' -and
    $Lua -match 'holder pass changed score' -and
    $Lua -match 'holder pass entered unsupported route') `
    'Bounded single-pass/holder-change/reacquisition proof is missing.'
Assert-Lab ($Map -match 'allow_holder_passes = false[\s\S]*?x_min = 0x0164' -and
    $Map -match 'allow_holder_passes = true[\s\S]*?x_min = 0x0108') `
    'Holder-transfer recovery is not confined to the closed two-point profile.'
Assert-Lab ($Lua -match 'movement_states = \{right = 1, left = 2, down = 4, up = 8\}' -and
    $Lua -match 'movement_state_valid' -and
    $Lua -match 'offense_position_held_' -and
    $Lua -match 'defense_away_held_' -and
    $Lua -match 'offense_direction_change_neutral' -and
    $Lua -match 'defense_direction_change_neutral' -and
    $Lua -match 'controller_readback\(rb\(R\.offense_side\) \+ 1\) ~= 0' -and
    $Lua -match 'controller_readback\(rb\(R\.defense_side\) \+ 1\) ~= 0' -and
    $Lua -notmatch 'move_pulse') `
    'Held cardinal movement, direction-state validation, or neutral gates are missing.'
Assert-Lab ($Lua -match 'if d > best_distance \+ 0\.5 then[\s\S]*?progress_deadline = frame \+ map\.shot_window\.progress_deadline' -and
    $Lua -match 'if metric < best_distance then[\s\S]*?progress_deadline = frame \+ map\.shot_window\.progress_deadline') `
    'Movement progress deadlines are not reset only by coordinate improvement.'

Assert-Lab ($Map -match '0x8C57.*ball_release' -and $Map -match '0x8ABD.*shot_classifier' -and
    $Map -match '0x91BC.*shot_result' -and
    $Map -match '0x933B.*decision_anchor') 'Required shot evidence hooks are missing.'
foreach ($Hook in @(
    @{ Address = '0x8C78'; Name = 'ordinary_direction_remap_ready' },
    @{ Address = '0xAD4E'; Name = 'flight_target_setup' },
    @{ Address = '0xAD50'; Name = 'flight_target_slot10_selected' },
    @{ Address = '0xB32C'; Name = 'target_motion_solver' },
    @{ Address = '0xAD68'; Name = 'flight_target_ready' },
    @{ Address = '0xB100'; Name = 'flight_state5_update' },
    @{ Address = '0xB139'; Name = 'flight_state5_hold_return' },
    @{ Address = '0xB13E'; Name = 'flight_state7_store_boundary' },
    @{ Address = '0xAB73'; Name = 'result_state7_dispatch' },
    @{ Address = '0xAB36'; Name = 'made_stat_update' },
    @{ Address = '0xBA02'; Name = 'score_apply' },
    @{ Address = '0xBA19'; Name = 'score_committed' },
    @{ Address = '0xAC0A'; Name = 'state08_route_boundary' },
    @{ Address = '0xAC6A'; Name = 'state09_route_entry' },
    @{ Address = '0x8FB9'; Name = 'possession_swap_entry' },
    @{ Address = '0x9042'; Name = 'possession_swap_complete' }
)) {
    Assert-Lab ($Map -match ($Hook.Address + '.*' + $Hook.Name + '.*gate = "bank05"')) `
        "Mapper-aware timing hook $($Hook.Address)/$($Hook.Name) is missing."
}
Assert-Lab ($Map -match '0x8FAD.*possession_transition_gate' -and
    $Map -notmatch '0x8FAD.*possession_handoff' -and
    $Map -notmatch 'distance_flight_solver|made_result_dispatch') `
    'Generic Bank05 helper/transition hook names are not conservative.'
Assert-Lab ($Map -match 'flight_target_x_lo = 0x0094' -and
    $Map -match 'flight_target_x_hi = 0x0095' -and
    $Map -match 'flight_target_y_lo = 0x0096' -and
    $Map -match 'flight_target_y_hi = 0x0097' -and
    $Map -match 'object_slot10_count_lo = 0x051D' -and
    $Map -match 'object_slot10_count_hi = 0x0528' -and
    $Map -match 'sfx_mailbox = 0x05B8') `
    'Exact target or slot-10 count addresses are missing.'
Assert-Lab ($Lua -match 'order = hook_order, lab_frame = frame' -and
    $Lua -match 'mapper_select = bank_select' -and
    $Lua -match 'mapper_bank6 = bank_registers\[7\]' -and
    $Lua -match 'mapper_bank7 = bank_registers\[8\]' -and
    $Lua -match 'local offense_actor = rb\(R\.offense_actor\)' -and
    $Lua -match 'offense_actor = offense_actor' -and
    $Lua -match 'defense_actor = rb\(R\.defense_actor\)' -and
    $Lua -match 'score0 = score\(0\)' -and $Lua -match 'score1 = score\(1\)' -and
    $Lua -match 'shot_clock = rb\(R\.shot_clock\)' -and
    $Lua -match 'close_mode = rb\(R\.close_mode\)' -and
    $Lua -match 'shooter_facing = offense_actor <= 9' -and
    $Lua -match 'shooter_phase_low = offense_actor <= 9' -and
    $Lua -match 'shooter_x = offense_actor <= 9' -and
    $Lua -match 'object_slot10_x = word\(' -and
    $Lua -match 'object_slot10_y = rb\(R\.actor_y \+ 10\)' -and
    $Lua -match 'state08_count = count_actor_state\(0x08\)' -and
    $Lua -match 'target_x = target_x' -and $Lua -match 'target_y = target_y' -and
    $Lua -match 'object_slot10_count = word\(' -and
    $Lua -match 'R\.object_slot10_count_lo,' -and
    $Lua -match 'R\.object_slot10_count_hi\)' -and
    $Lua -match 'object_slot10_altitude = word\(' -and
    $Lua -match 'object_slot10_altitude_velocity = word\(' -and
    $Lua -match 'sfx_mailbox = rb\(R\.sfx_mailbox\)') `
    'Timing evidence is not snapshotted in the mapper callback.'
Assert-Lab ($Lua -match 'h\.lab_frame, h\.emu_frame, h\.order' -and
    $Lua -match 'h\.mapper_select, h\.mapper_bank6, h\.mapper_bank7' -and
    $Lua -match 'h\.offense_side, h\.defense_side, h\.control0, h\.control1' -and
    $Lua -match 'h\.score0, h\.score1, h\.shot_clock, h\.close_mode, h\.shooter_facing,' -and
    $Lua -match 'h\.shooter_phase_low, h\.shooter_x, h\.shooter_y, h\.state08_count' -and
    $Lua -notmatch 'h\.point_value, rb\(R\.offense_side\)') `
    'Queued timing rows use flush-time state instead of hook-time snapshots.'
Assert-Lab ($Lua -match 'local function two_point_timing_evidence_valid' -and
    $Lua -match 'hook_first_order\.flight_target_setup <' -and
    $Lua -match 'hook_first_order\.point_classifier_local <' -and
    $Lua -match 'hook_first_order\.two_point_return_local <' -and
    $Lua -match 'hook_first_order\.shot_result <' -and
    $Lua -match 'hook_first_order\.decision_anchor <' -and
    $Lua -match 'hook_first_order\.terminal_make_bit7_clear <' -and
    $Lua -match 'hook_first_order\.flight_target_slot10_selected <' -and
    $Lua -match 'hook_last_order\.flight_state5_update <' -and
    $Lua -match 'hook_last_order\.state08_route_boundary <' -and
    $Lua -match 'hook_first_order\.possession_swap_entry <' -and
    $Lua -match 'actual_swap' -and
    $Lua -match 'b100_entry_count == contract\.flight_state5_updates' -and
    $Lua -match 'hook_seen_count\.state08_route_boundary ==' -and
    $Lua -match 'hook_seen_count\.point_classifier_local == 1' -and
    $Lua -match 'hook_seen_count\.terminal_make_bit7_clear == 1' -and
    $Lua -match 'hook_seen_count\.terminal_miss_bit7_set or 0' -and
    $Lua -match 'hook_first_frame\.state09_route_entry ==' -and
    $Lua -match 'timing_capture\.target_x == contract\.target_x' -and
    $Lua -match 'timing_capture\.slot10_altitude_velocity ==' -and
    $Lua -match 'timing_capture\.pre_remap_direction ==' -and
    $Lua -match 'timing_capture\.launch_phase_low == contract\.launch_phase_low' -and
    $Lua -match 'timing_capture\.solver_close_mode == contract\.launch_close_mode' -and
    $Lua -match 'timing_capture\.transition_mailbox ==' -and
    $Lua -match 'timing_capture\.score_after0 - timing_capture\.score_before0 ==' -and
    $Lua -match 'profile\.require_timing_evidence' -and
    $Lua -match 'timing_evidence_valid=' -and
    $Lua -match 'defender_cycles_closed=' -and
    $Lua -match 'holder_passes=' -and $Lua -match 'holder_changes=' -and
    $Lua -match 'score_frame_delta=' -and $Lua -match 'handoff_frame_delta=' -and
    $Lua -match 'captured_target=' -and $Lua -match 'captured_slot10_count=' -and
    $Lua -match 'captured_shooter_position=' -and
    $Lua -match 'captured_slot10_position=' -and
    $Lua -match 'captured_terminal_mailboxes=' -and
    $Lua -match 'captured_slot10_altitude=' -and
    $Lua -match 'captured_slot10_horizontal_velocity=' -and
    $Lua -match 'state08_route_updates=' -and
    $Lua -match 'score_apply_snapshot=' -and
    $Lua -match 'score_commit_snapshot=') `
    'TGLAB status lacks bounded cycle/pass/timing/swap evidence or fail-closed validation.'
Assert-Lab ($Map -match 'pre_remap_direction = 0x05' -and
    $Map -match 'post_remap_direction = 0x00' -and
    $Map -match 'launch_direction = 0x00' -and
    $Map -match 'launch_phase_low = 0x05' -and
    $Map -match 'launch_close_mode = 0x00' -and
    $Map -match 'target_x = 0x00A0' -and $Map -match 'target_y = 0x008F' -and
    $Map -match 'slot10_count = 0x3C' -and
    $Map -match 'slot10_x_offset = 2' -and
    $Map -match 'slot10_y_offset = -1' -and
    $Map -match 'slot10_altitude = 0x3900' -and
    $Map -match 'slot10_altitude_velocity = 0x04EC' -and
    $Map -match 'slot10_horizontal_velocity_min = 0xFF88' -and
    $Map -match 'slot10_horizontal_velocity_max = 0xFF8F' -and
    $Map -match 'slot10_vertical_velocity_min = 0x001D' -and
    $Map -match 'slot10_vertical_velocity_max = 0x0026' -and
    $Map -match 'flight_state5_updates = 63' -and
    $Map -match 'state08_route_updates = 26' -and
    $Map -match 'score_shot_clock = 0x18' -and
    $Map -match 'terminal_sfx_mailbox = 0x0B' -and
    $Runner -match "captured_target = '160,143'" -and
    $Runner -match "captured_pre_remap_direction = '5'" -and
    $Runner -match "captured_post_remap_direction = '0'" -and
    $Runner -match "captured_launch_direction = '0'" -and
    $Runner -match "captured_solver_phase_low = '5'" -and
    $Runner -match "captured_slot10_altitude = '14592'" -and
    $Runner -match "captured_terminal_mailboxes = '11,11,11'" -and
    $Runner -match 'HorizontalVelocity -lt 65416' -and
    $Runner -match 'VerticalVelocity -gt 38' -and
    $Runner -match "b100_entry_count = '63'" -and
    $Runner -match "state08_route_updates = '26'") `
    'Exact ROM-derived two-point timing contract is missing from map or runner.'
Assert-Lab ($Map -match 'point_value = 0x0398' -and
    $Map -match '0xB995.*point_classifier_local.*gate = "bank05"' -and
    $Map -match '0xB9D7.*two_point_return_local.*gate = "bank05"' -and
    $Lua -match 'point_classifier_seen = true' -and
    $Lua -match 'point_return_seen = true' -and
    $Lua -match 'classified_point_value = h\.point_value' -and
    $Lua -match 'classified_point_value == profile\.expected_point_value') `
    'Mapper-aware point-classifier evidence is missing.'
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
Assert-Lab ($Map -match '0xA6EE.*miss_variant_dispatch.*gate = "bank05"' -and
    $Map -match '0xA708.*miss_variant_0_or_3.*gate = "bank05"' -and
    $Map -match '0xA7A9.*miss_variant_1.*gate = "bank05"' -and
    $Map -match '0xA8E9.*miss_variant_2.*gate = "bank05"') `
    'Mapper-gated Bank05 miss-variant hooks are missing.'
Assert-Lab ($Map -match 'miss_variant_selector = 0x006A' -and
    $Map -match 'object_slot10_state = 0x0478' -and
    $Map -match 'saved_object_horizontal_velocity_lo = 0x038D' -and
    $Map -match 'saved_object_horizontal_velocity_hi = 0x038E' -and
    $Map -match 'saved_object_vertical_velocity_lo = 0x038F' -and
    $Map -match 'saved_object_vertical_velocity_hi = 0x0390' -and
    $Map -match '\[0\] = 0xA708' -and $Map -match '\[1\] = 0xA7A9' -and
    $Map -match '\[2\] = 0xA8E9' -and $Map -match '\[3\] = 0xA708') `
    'Exact miss selector/state/saved-velocity addresses or selector-to-target table are missing.'
Assert-Lab ($Lua -match 'score0,score1,shot_clock,close_mode,' -and
    $Lua -match 'shooter_facing,shooter_phase_low,shooter_x,shooter_y,state08_count,' -and
    $Lua -match 'target_x_0094_0095,target_y_0096_0097,slot10_x,slot10_y,' -and
    $Lua -match 'slot10_count_051d_0528' -and
    $Lua -match 'sfx_mailbox_05b8' -and
    $Lua -match 'miss_selector_6a,miss_selector_low2,' -and
    $Lua -match 'miss_selected_target,object_slot10_state_0478' -and
    $Lua -match 'object_slot10_horizontal_velocity_04f1_04fc' -and
    $Lua -match 'object_slot10_vertical_velocity_0507_0512' -and
    $Lua -match 'saved_object_horizontal_velocity_038d_038e' -and
    $Lua -match 'saved_object_vertical_velocity_038f_0390' -and
    $Lua -match 'local miss_selector = rb\(R\.miss_variant_selector\)' -and
    $Lua -match 'miss_selected_target = selected_miss_target\(miss_selector\)' -and
    $Lua -match 'object_slot10_state = rb\(R\.object_slot10_state\)' -and
    $Lua -match 'object_slot10_horizontal_velocity = word\(' -and
    $Lua -match 'object_slot10_vertical_velocity = word\(' -and
    $Lua -match 'saved_object_horizontal_velocity = word\(' -and
    $Lua -match 'saved_object_vertical_velocity = word\(' -and
    $Lua -match 'h\.score0, h\.score1, h\.shot_clock, h\.close_mode, h\.shooter_facing,' -and
    $Lua -match 'h\.shooter_phase_low, h\.shooter_x, h\.shooter_y, h\.state08_count,' -and
    $Lua -match 'h\.target_x, h\.target_y, h\.object_slot10_x, h\.object_slot10_y,' -and
    $Lua -match 'h\.object_slot10_count, h\.object_slot10_altitude,' -and
    $Lua -match 'h\.object_slot10_altitude_velocity, h\.miss_selector, h\.miss_selector_low2,' -and
    $Lua -match 'h\.miss_selected_target, h\.object_slot10_state,' -and
    $Lua -match 'h\.object_slot10_horizontal_velocity, h\.object_slot10_vertical_velocity,' -and
    $Lua -match 'h\.saved_object_horizontal_velocity, h\.saved_object_vertical_velocity,' -and
    $Lua -match 'h\.sfx_mailbox') `
    'Miss dispatch and velocity evidence is not snapshotted at hook time and emitted from the bounded queue.'
Assert-Lab ($Lua -match '%02X,%d,%04X,%02X,%04X,%04X,%04X,%04X,%02X\\n') `
    'Hook velocity snapshots are not emitted as raw four-digit hexadecimal words.'
Assert-Lab ($Lua -match 'score_apply_seen and \(score0_delta == 2 or score0_delta == 3\)' -and
    $Lua -match 'score1_delta == 0' -and $Lua -match 'settlement_seen') `
    'Make/miss scoring and settlement criteria are incomplete.'
Assert-Lab ($Lua -match 'score0_delta == profile\.expected_score_delta' -and
    $Lua -match 'profile\.expected_make and point_evidence' -and
    $Lua -match 'profile outcome mismatch' -and
    $Lua -match 'point-classifier evidence mismatch') `
    'Two-point make profile does not fail closed on point/outcome/score evidence.'
Assert-Lab ($Lua -notmatch 'positioning_valid' -and
    $Lua -notmatch 'offense_clear_front_' -and
    $Lua -match 'movement_neutral_gate' -and
    $Lua -match 'front threat entered safety window') `
    'Holder/front-threat/controller invariants were weakened.'
Assert-Lab ($Lua -match 'local dx, dy = a\.x - b\.x, a\.y - b\.y' -and
    $Lua -match 'if \(-dx\) \* front_sign > 0' -and
    $Lua -match 'button = dx >= 0 and "left" or "right"' -and
    $Lua -match 'button = dy >= 0 and "up" or "down"') `
    'Front/away coordinate signs are inconsistent.'

Assert-Lab ($CpuMap -match 'schema = "TGLCPU-1"' -and
    $CpuMap -match 'output_schema = "TGLCPU-TRACE-1"' -and
    $CpuLua -match 'map\.schema == "TGLCPU-1" and map\.schema_version == 1' -and
    $CpuRunner -match "schema = 'tecmo.r1-cpu-play-lifecycle-proof/2'") `
    'CPU lifecycle schema/version locks are missing.'
Assert-Lab ($CpuMap -match '076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4' -and
    $CpuMap -match 'F89812F4E9506EF7090D9D0310D368ABD79BACA362B7BFC4A2E7E499754F2A1B' -and
    $CpuRunner -match '076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4' -and
    $CpuRunner -match 'F89812F4E9506EF7090D9D0310D368ABD79BACA362B7BFC4A2E7E499754F2A1B') `
    'CPU lifecycle ROM/FCEUX identity locks are missing.'
foreach ($Pattern in $ForbiddenLua) {
    Assert-Lab ($CpuLua -notmatch $Pattern) "Forbidden CPU Lua capability matched: $Pattern"
}
Assert-Lab ($CpuLua -match 'memory\.registerexec' -and
    $CpuLua -match 'memory\.registerwrite' -and
    $CpuLua -match 'mapped_raw_bank' -and
    $CpuLua -match 'map\.raw_banks\[hook\.gate\]\[raw_bank\]') `
    'CPU lifecycle mapper-gated hook policy is missing.'
foreach ($Address in @('0x8B90', '0x8B9F', '0x8BA2', '0x8BAE', '0x8FD9', '0x8FE8')) {
    Assert-Lab ($CpuMap -match [regex]::Escape($Address)) `
        "CPU lifecycle hook '$Address' is missing."
}
Assert-Lab ($CpuMap -match 'dispatch_cpu = 0x8BAE' -and
    $CpuMap -match 'opcode_load_cpu = 0x8BA2' -and
    $CpuMap -match 'handler_table_low = 0x8BB1' -and
    $CpuMap -match 'handler_table_high = 0x8BC9' -and
    $CpuMap -match 'opcode_22_handler' -and
    $CpuMap -notmatch '\{ address=0x8BB1' -and
    $CpuMap -notmatch '\{ address=0x8BC9') `
    'CPU dispatcher/table distinction is missing or static table bytes are registered as hooks.'
Assert-Lab ($CpuMap -match 'decomp_comment_note' -and
    $CpuMap -match 'drift two bytes early' -and
    $CpuMap -match 'following canonical ROM addresses' -and
    $CpuMap -notmatch 'These six addresses' -and
    $CpuLua -match 'decomp_comment_note=') `
    'Canonical ROM/decomp comment-drift note is missing.'
Assert-Lab ($CpuMap -match 'accepted deterministic/authentic controller schedule' -and
    $CpuProofDoc -match 'accepted deterministic/authentic controller schedule' -and
    $CpuProofDoc -match 'row timing is not claimed as ASM/source-pinned input semantics' -and
    $CpuProofDoc -notmatch 'navigation schedule is source-pinned' -and
    $CpuProofDoc -notmatch 'source-pinned tip schedule') `
    'CPU lifecycle controller schedule is overclaimed as source-pinned input semantics.'
Assert-Lab ($CpuProofDoc -notmatch '\u00C3|\u00E2' -and $CpuLineageDoc -notmatch '\u00C3|\u00E2' -and
    $CpuProofDoc -notmatch '[\u00D7\u2014\u2013]' -and $CpuLineageDoc -notmatch '[\u00D7\u2014\u2013]') `
    'CPU lifecycle proof documents contain mojibake or nonportable punctuation.'
Assert-Lab ($CpuLua -match 'address_evidence=exact_source_pinned' -and
    $CpuLua -match 'address_confidence,label_confidence' -and
    $CpuLua -match 'hook\.address_confidence' -and
    $CpuLua -match 'hook\.label_confidence' -and
    $CpuMap -match 'source_hook' -and
    $CpuMap -match 'label_confidence = label_confidence') `
    'CPU lifecycle exact address evidence and semantic label confidence are not split.'
Assert-Lab ($CpuMap -match 'record_count = 680' -and
    $CpuMap -match 'record_size = 5' -and
    $CpuMap -match 'opcode_histogram' -and
    $CpuLua -match 'stream_offset < map\.command\.record_count \* map\.command\.record_size') `
    'CPU lifecycle aligned stream/corpus contract is missing.'
Assert-Lab ($CpuMap -match 'first_age = 30, last_age = 34' -and
    $CpuMap -match 'first_age = 35, last_age = 37' -and
    $CpuMap -match 'first_age = 38, last_age = 55' -and
    $CpuLua -match 'setup_valid\(\)' -and
    $CpuLua -match 'tip_not_running_seen' -and
    $CpuLua -match 'clock_stopped\(\)' -and
    $CpuLua -match 'clock_stopped_seen' -and
    $CpuLua -match 'clock_running\(\)' -and
    $CpuLua -match 'capture_start_frame = frame \+ map\.reference_window\.post_live_delay') `
    'CPU lifecycle setup/tip/clock-running gate is missing.'
Assert-Lab ($CpuMap -match 'max_frames = 4320' -and
    $CpuMap -match 'trace_rows = 8192' -and
    $CpuRunner -match "TECMO_CPU_LIFECYCLE_MAX_FRAMES = '4320'" -and
    $CpuMap -notmatch 'max_frames\s*=\s*4200' -and
    $CpuMap -notmatch 'trace_rows\s*=\s*4096' -and
    $CpuRunner -notmatch "TECMO_CPU_LIFECYCLE_MAX_FRAMES = '4200'" -and
    $CpuMap -match 'Empirical deterministic schedule capacity' -and
    $CpuProofDoc -match 'bounded at 4320 emulator frames' -and
    $CpuProofDoc -match '8192 trace rows' -and
    $CpuProofDoc -notmatch 'bounded at 4200 emulator frames' -and
    $CpuProofDoc -notmatch '4096 trace rows') `
    'CPU lifecycle empirical capacity is stale, mismatched, or still claims the old 4200/4096 bounds.'
Assert-Lab ($CpuLua -match 'local capture_end_frame = capture_start_frame \+ map\.reference_window\.frames - 1' -and
    $CpuLua -match 'if capture_end_frame > max_frames then' -and
    $CpuLua -match 'capture window exceeds bounded session max' -and
    $CpuLua -match 'finish\("capture window exceeds bounded session max", "abort"\)') `
    'CPU lifecycle inclusive capture-window feasibility fail-fast is missing.'
Assert-Lab ($CpuLua -match 'lifecycle_evidence_valid\(\)' -and
    $CpuLua -match 'lifecycle\.fetch_events > 0' -and
    $CpuLua -match 'lifecycle\.opcode_observations > 0' -and
    $CpuLua -match 'lifecycle\.dispatch_events > 0' -and
    $CpuLua -match 'lifecycle\.handler_events > 0' -and
    $CpuLua -match 'lifecycle\.advance_events > 0' -and
    $CpuLua -match 'lifecycle\.aligned_stream_offsets > 0' -and
    $CpuLua -match 'lifecycle\.fixed_link_observations > 0' -and
    $CpuLua -match 'complete window lacked source-pinned CPU lifecycle evidence') `
    'CPU lifecycle proof does not fail closed on missing source-pinned execution evidence.'
Assert-Lab ($TraceHeaderColumns -eq 30 -and $TraceConversionTokens.Count -eq 30 -and $TraceValueIndexes.Count -eq 30 -and ($TraceValueIndexes -join ',') -eq '1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30' -and $TraceHeaderText.Split(',')[25] -eq 'fixed_link' -and $TraceHeaderText.Split(',')[28] -eq 'address_confidence' -and $TraceHeaderText.Split(',')[29] -eq 'label_confidence' -and $TraceConversionTokens[13] -eq '%d' -and $TraceConversionTokens[14] -eq '%04X' -and $TraceConversionTokens[15] -eq '%d' -and $TraceConversionTokens[20] -eq '%04X' -and $TraceConversionTokens[25] -eq '%s' -and $TraceConversionTokens[28] -eq '%s' -and $TraceConversionTokens[29] -eq '%s' -and $CpuLua -match 'string\.format\(trace_format, unpack\(trace_values\)\)') 'CPU lifecycle trace header, format conversion, value count, or fixed-link/confidence positions are misaligned.'
Assert-Lab ($RegisterExecCallbackText -match 'local ok, failure = xpcall\(function\(\)' -and $RegisterExecCallbackText -match 'mapped_raw_bank' -and $RegisterExecCallbackText -match 'record_hook\(hook, raw_bank\)' -and $RegisterExecCallbackText -match 'if not ok then defer_callback_failure\(failure\) end' -and $RegisterExecCallbackText -notmatch 'FCEU\.frameadvance' -and $CpuLua -match 'string\.gsub\(tostring\(message\), "\[%c\]", " "\)' -and $CpuLua -match 'string\.len\(value\) > 160' -and $CpuLua -match 'deferred_failure = "CPU lifecycle registerexec callback failed: " \.\. value') 'CPU lifecycle registerexec callback errors are not bounded, sanitized, and fail-closed.'
Assert-Lab ($CpuLua -match 'frame < capture_start_frame' -and $CpuLua -match 'captured < 1 or captured > map\.reference_window\.frames' -and $CpuLua -match 'capture_start_frame = frame \+ map\.reference_window\.post_live_delay') 'CPU lifecycle trace evidence is recorded before the defined capture window.'
Assert-Lab ($CpuLua -match 'index <= 9 and string\.format\("%04X", stream_actor_offset\(index\)\) or "NA"' -and
    $CpuLua -match 'index <= 9 and string\.format\("%02X", rb\(R\.fixed_link \+ index\)\) or "NA"' -and
    $CpuLua -match 'for index = 0, 10 do') `
    'CPU lifecycle actor slot 10 incorrectly serializes stream or fixed-link data.'
foreach ($Button in @('A', 'B', 'up', 'down', 'left', 'right', 'start', 'select')) {
    Assert-Lab ($CpuLua -match ([regex]::Escape($Button) + ' = source\.' + [regex]::Escape($Button))) `
        "CPU lifecycle complete joypad field '$Button' is missing."
}
Assert-Lab ($CpuLua -match 'joypad\.set\(1, active_p1\)' -and
    $CpuLua -match 'joypad\.set\(2, active_p2\)' -and
    $CpuLua -match 'apply_pads\(\{\}, \{\}\)' -and
    $CpuRunner -match 'Process\.Kill\(\)' -and
    $CpuRunner -match '64 MiB') `
    'CPU lifecycle complete-pad cleanup or bounded output is missing.'
Assert-Lab ($CpuRunner -match 'Run-GameplayCpuSteeringTests\.ps1' -and
    $CpuRunner -match 'gameplay-cpu-steering-frame' -and
    $CpuRunner -match '640x480' -and
    $CpuRunner -match 'nondeterministic' -and
    $CpuRunner -match 'contact-sheet' -and
    $CpuRunner -match 'ffmpeg' -and $CpuRunner -match 'ffprobe' -and
    $CpuRunner -match 'RequireVideo' -and
    $CpuRunner -match '39375000/655171' -and
    $CpuRunner -match 'temp-videos\\gameplay-lab') `
    'CPU lifecycle native/reference proof runner contract is incomplete.'
Assert-Lab ($CpuRunner -match 'lifecycle_evidence_valid' -and
    $CpuRunner -match '\[int\]::TryParse' -and
    $CpuRunner -match 'fixed_link_mismatches') `
    'CPU lifecycle runner does not validate positive execution evidence.'
Assert-Lab ($CpuRunner -match '\$ProcessExitCode = \$null' -and
    $CpuRunner -match '(?s)finally \{.*?\$Process\.WaitForExit\(\).*?\$ProcessExitCode = \[int\]\$Process\.ExitCode.*?\$Process\.Dispose\(\)' -and
    $CpuRunner -match 'if \(\$ProcessExitCode -ne 0\)' -and
    $CpuRunner -notmatch '\$Process\.ExitCode -ne 0' -and
    $CpuRunner -notmatch '(?s)\$Process\.Dispose\(\).*?\$Process\.ExitCode') `
    'CPU lifecycle process status is read before cached post-WaitForExit bookkeeping or after disposal.'
Assert-Lab ($CpuRunner -match 'Get-Command powershell\.exe -CommandType Application' -and
    $CpuRunner -match '\$FocusedArguments = @\(' -and
    $CpuRunner -match "'-NoProfile'" -and
    $CpuRunner -match "'-ExecutionPolicy'" -and
    $CpuRunner -match "'-File'" -and
    $CpuRunner -match "'-RomPath'" -and
    $CpuRunner -match "'-ProjectRoot'" -and
    $CpuRunner -match "'-Build'" -and
    $CpuRunner -match "Invoke-Logged -Command 'powershell\.exe'") `
    'CPU lifecycle focused wrapper does not transport named parameters through a nested PowerShell process.'
Assert-Lab ($CpuRunner -match '\$InvocationError = \$null' -and
    $CpuRunner -match '\$ErrorActionPreference = ''Continue''' -and
    $CpuRunner -match '\$Raw \+= \$_.ToString\(\)' -and
    $CpuRunner -match '\$Code = 1' -and
    $CpuRunner -match 'invocation_failed=true' -and
    $CpuRunner -match '\$Rows \| Set-Content' -and
    $CpuRunner -match 'Get-ArtifactInventory') `
    'CPU lifecycle failed child commands can escape before nonempty inventoried runner metadata is written.'
Assert-Lab ($CpuLua -match 'local step_ok, step_failure = xpcall\(execute_frame' -and
    $CpuLua -match 'if not stopped then FCEU\.frameadvance\(\) end' -and
    $CpuLua -notmatch 'xpcall\(function\(\)\s*while not stopped do') `
    'CPU lifecycle Lua frameadvance is enclosed by a yield-crossing outer xpcall.'
Assert-Lab ($CpuLua -match 'pcall\(FCEU\.speedmode, "maximum"\)' -and
    $CpuLua -match 'progress\.txt' -and $CpuLua -match 'os\.rename\(temp_path, progress_path\)' -and
    $CpuMap -match 'progress_publish_attempts = 3' -and
    $CpuLua -match 'for attempt = 1, map\.caps\.progress_publish_attempts do' -and
    $CpuLua -match 'local removed = os\.remove\(progress_path\)' -and
    $CpuLua -match 'local renamed = os\.rename\(temp_path, progress_path\)' -and
    $CpuLua -match 'if renamed then' -and
    $CpuLua -match 'final_progress_written' -and $CpuLua -match 'final progress publish failed' -and
    $CpuLua -match 'map\.caps\.progress_period' -and
    $CpuRunner -match 'Get-ProgressSnapshot' -and
    $ProgressFunctionText -match '\[IO\.File\]::Open' -and
    $ProgressFunctionText -match '\[IO\.FileShare\]::ReadWrite' -and
    $ProgressFunctionText -match '\[IO\.FileShare\]::Delete' -and
    $ProgressFunctionText -match '\[IO\.StreamReader\]::new' -and
    $ProgressFunctionText -match '\$Reader\.Dispose\(\)' -and
    $ProgressFunctionText -match 'CharCount -gt 65536' -and
    $ProgressFunctionText -notmatch 'ReadAllText' -and
    $CpuRunner -match 'stage -ne .finished' -and $CpuRunner -match 'ProgressSequence' -and
    $CpuRunner -match 'speedmode_ok' -and $CpuLua -match 'local speedmode_ok = false' -and
    $CpuLua -match 'speedmode_ok = pcall\(FCEU\.speedmode' -and
    $CpuLua -match 'file:write\("speedmode_ok=' -and
    $CpuRunner -match 'progress\.txt\.tmp' -and
    $CpuRunner -match 'Join-Path.*progress\.txt' -and
    $CpuRunner -notmatch 'SentinelSeen = .*metadata\.txt' -and
    $CpuRunner -notmatch 'foreach \(\$Name in @\(\x27status\.txt\x27, \x27trace\.csv\x27') `
    'CPU lifecycle boot progress sentinel/watchdog is missing or uses buffered files.'
Assert-Lab ($ProgressFunctionText -match '\[IO\.File\]::Open' -and
    $ProgressFunctionText -match '\[IO\.FileShare\]::ReadWrite' -and
    $ProgressFunctionText -match '\[IO\.FileShare\]::Delete' -and
    $ProgressFunctionText -match '\[IO\.StreamReader\]::new' -and
    $ProgressFunctionText -match '\$Reader\.Dispose\(\)' -and
    $ProgressFunctionText -match 'CharCount -gt 65536' -and
    $ProgressFunctionText -notmatch 'ReadAllText') `
    'CPU lifecycle progress reader uses a blocking whole-file API or lacks rename-safe bounded sharing.'
Assert-Lab ($CpuMap -match 'progress_publish_attempts = 3' -and
    $CpuLua -match 'for attempt = 1, map\.caps\.progress_publish_attempts do' -and
    $CpuLua -match 'local removed = os\.remove\(progress_path\)' -and
    $CpuLua -match 'local renamed = os\.rename\(temp_path, progress_path\)' -and
    $CpuLua -match 'if renamed then' -and
    $CpuLua -notmatch 'os\.remove\(progress_path\)\s*local ok = os\.rename') `
    'CPU lifecycle progress publisher uses a single-shot remove/rename instead of bounded retry.'
Assert-Lab ($CpuLua -match 'live_setup_valid\(\)' -and
    $CpuLua -match 'running_clock_live_seen' -and
    $CpuLua -match 'clock_running\(\)' -and
    $CpuLua -match 'map\.live\.mode' -and $CpuLua -match 'map\.live\.screen' -and
    $CpuLua -match 'map\.live\.control0' -and $CpuLua -match 'map\.live\.control1' -and
    $CpuLua -match 'defense_side == \(1 - offense_side\)' -and
    $CpuLua -match 'offense_actor >= 0 and offense_actor <= 9' -and
    $CpuLua -match 'defense_actor >= 0 and defense_actor <= 9' -and
    $CpuMap -match 'defense_side = 1' -and
    $CpuLua -notmatch 'if not live_seen and live_valid' -and
    $CpuRunner -match 'clock_stopped_seen') `
    'CPU lifecycle running-clock/live-invariant gate regressed to pre-tip mode/screen detection.'
Assert-Lab ($CpuLua -match 'invalid_fetches' -and $CpuLua -match 'misaligned_fetches' -and
    $CpuLua -match 'valid_actor = actor_index >= 0 and actor_index <= 9' -and
    $CpuLua -match 'stream_offset < map\.command\.record_count \* map\.command\.record_size' -and
    $CpuLua -match 'stream_offset % map\.command\.record_size == 0' -and
    $CpuLua -match 'fixed_link_mismatches = lifecycle\.fixed_link_mismatches \+ 1') `
    'CPU lifecycle per-fetch actor/stream/fixed-link fail-closed validation is missing.'
foreach ($HandlerAddress in @('0x8C40', '0x8CD0', '0x8E4F', '0x9172', '0x8BE1')) {
    Assert-Lab ($CpuMap -match ('source_hook\(' + [regex]::Escape($HandlerAddress) + '[^\n]*"handler", "exact_opcode_entry"\)')) `
        "Explicit handler '$HandlerAddress' is not classified for deduplicated handler evidence."
}
Assert-Lab ($CpuLua -match 'handler_addresses = \{\}' -and
    $CpuLua -match 'handler_addresses\[address\] = true' -and
    $CpuLua -match 'not handler_addresses\[address\]' -and
    $CpuLua -match 'registered_addresses\[hook\.address\]') `
    'CPU lifecycle handler deduplication does not preserve handler-kind evidence or command fields.'
Assert-Lab ($CpuLua -match 'flush_outputs\(\)' -and
    $CpuLua -match 'trace:flush' -and $CpuLua -match 'actors:flush' -and
    $CpuLua -match 'screenshot_count == map\.caps\.screenshots' -and
    $CpuRunner -match 'screenshot_count' -and
    $CpuRunner -match '\$ReferenceScreenshotCount = 12') `
    'CPU lifecycle deterministic flush or exact screenshot-count gate is missing.'
Assert-Lab ($CpuRunner -match '\$IncompletePath' -and
    $CpuRunner -match 'Set-Content -LiteralPath \$IncompletePath' -and
    $CpuRunner -match 'Remove-Item -LiteralPath \$IncompletePath' -and
    $CpuRunner -match 'Get-ReferenceFrameRecords' -and
    $CpuRunner -match '\$ExpectedName = .reference-frame-\{0:D4\}\.png' -and
    $CpuRunner -match 'Get-FileRecord \$Files\[\$Index - 1\]\.FullName \$ExpectedName \$ReferenceWidth \$ReferenceHeight' -and
    $CpuRunner -match '\$Item\.Length -le 0' -and
    $CpuRunner -match 'Expected exactly \$ReferenceScreenshotCount' -and
    $CpuRunner -match 'No files matched fingerprint pattern' -and
    $CpuRunner -match 'selected asset-pack entry offset/count is out of bounds') `
    'CPU lifecycle incomplete-sentinel, frame inventory, or asset-pack bounds contract is missing.'
Assert-Lab ($CpuRunner -match 'Get-GitState' -and
    $CpuRunner -match 'tracked_clean' -and
    $CpuRunner -match 'nonignored_clean' -and
    $CpuRunner -match 'RequirePass refuses tracked worktree dirtiness' -and
    $CpuRunner -match 'RequirePass refuses untracked nonignored worktree entries' -and
    $CpuRunner -match 'RequirePass requires -RequireVideo' -and
    $CpuRunner -match '\$GitState\.head' -and
    $CpuRunner -match 'draft_pass' -and
    $CpuRunner -match '\$RequirePass -and \$ManifestJson -match' -and
    $CpuRunner -match 'PENDING_FINAL_SHA_UNTIL_COMMIT' -and
    $CpuRunner -notmatch 'accepted_core_sha' -and
    $CpuRunner -notmatch 'dea1fd7c2c2761fe08a6a27ab13a5e661e2b7094') `
    'CPU lifecycle Git cleanliness/final-SHA/pending-metadata contract is missing.'
Assert-Lab ($CpuRunner -match '\$ExpectedTgaiVersion = ''TGAI-2''' -and
    $CpuRunner -match '\$ExpectedTgaiPayloadVersion = 2' -and
    $CpuRunner -match '\$ExpectedTgaiBytes = 7632' -and
    $CpuRunner -match '\$ExpectedTgaiFnv1a32 = ''C8CFFDC0''' -and
    $CpuRunner -match '\$ExpectedFocusedRomMutations = 23' -and
    $CpuRunner -match '\$ExpectedFocusedMutationSummary' -and
    $CpuRunner -match '\$TgaiPayloadHeaderVersion' -and
    $CpuRunner -match 'Fresh CPU proof pack failed \$ExpectedTgaiVersion identity' -and
    $CpuRunner -match '\{0\} CPU steering isolated' -and
    $CpuRunner -match '\$RequirePass -and \$GitState\.branch -ne \$GitState\.expected_branch' -and
    $CpuRunner -match 'formal -RequirePass remains restricted' -and
    $CpuRunner -match 'opcode-15 selected-defender resolver is harness-only; this proof is not a natural FCEUX \$91C8 capture') `
    'CPU lifecycle TGAI-2 identity, 23-mutation, strict PASS-branch, or opcode-15 harness-only contract is missing.'
Assert-Lab ($CpuRunner -match 'generated_utc' -and
    $CpuRunner -match 'scripts = \[ordered\]@' -and
    $CpuRunner -match 'reference_frames = \$FrameDetails' -and
    $CpuRunner -match 'repeat_frame_hashes' -and
    $CpuRunner -match 'artifacts = \$ArtifactInventory' -and
    $CpuRunner -match 'legacy gameplay-cpu-steering-frameN continuity/regression' -and
    $CpuRunner -match 'R1-LIVE' -and
    $CpuRunner -match 'log_files = \$LogHashes') `
    'CPU lifecycle generated manifest does not carry the required evidence inventory/limitations.'
Assert-Lab ($CpuRunner -match '39375000/655171' -and
    $CpuRunner -match '-count_frames' -and
    $CpuRunner -match 'nb_read_frames' -and
    $CpuRunner -match 'show_entries.*nb_frames,nb_read_frames' -and
    $CpuRunner -match '\$Streams = @\(\$ProbeJson\.streams\)' -and
    $CpuRunner -match 'avg_frame_rate' -and
    $CpuRunner -match 'deterministic_sha256_equal' -and
    $CpuRunner -match 'ffmpeg-primary\.log' -and $CpuRunner -match 'ffmpeg-repeat\.log' -and
    $CpuRunner -match '\[int\]\$Stream\.width -ne \$NativeWidth' -and
    $CpuRunner -match '\[int\]\$Stream\.nb_read_frames -ne \$NativeFrameCount' -and
    $CpuRunner -match "Arguments = '-version'") `
    'CPU lifecycle video cadence is not the exact NTSC contract.'
Assert-Lab ($CpuRunner -match '\$NativeVideoTrackTimescale = 39375000' -and
    $CpuRunner -match '(?s)-video_track_timescale.*\$NativeVideoTrackTimescale' -and
    $CpuRunner -match 'show_entries.*r_frame_rate.*avg_frame_rate.*time_base.*nb_frames,nb_read_frames' -and
    $CpuRunner -match '\[string\]\$Stream\.r_frame_rate -ne \$NativeFrameRate' -and
    $CpuRunner -match '\[string\]\$Stream\.avg_frame_rate -ne \$NativeFrameRate' -and
    $CpuRunner -match '\[string\]\$Stream\.time_base -ne \$NativeVideoTimeBase' -and
    $CpuRunner -match '\[int\]\$Stream\.nb_frames -ne \$NativeFrameCount' -and
    $CpuRunner -match '\[int\]\$Stream\.nb_read_frames -ne \$NativeFrameCount' -and
    $CpuRunner -match 'r_frame_rate = \[string\]\$Stream\.r_frame_rate' -and
    $CpuRunner -match 'time_base = \[string\]\$Stream\.time_base' -and
    $CpuRunner -match 'track_timescale = \$NativeVideoTrackTimescale' -and
    $CpuManifest -match '"track_timescale": 39375000' -and
    $CpuManifest -match '"time_base": "1/39375000"' -and
    $CpuProofDoc -match 'r_frame_rate' -and $CpuProofDoc -match 'time_base') `
    'CPU lifecycle exact video track-timescale, r/avg rate, time_base, or frame-count validation is missing or incomplete.'
Assert-Lab ($CpuRunner -match '\$RequirePass -and !\$RequireVideo' -and
    $CpuRunner -match 'pass status cannot carry unavailable video' -and
    $CpuRunner -match 'repeat_sha256' -and $CpuRunner -match 'probe_commands' -and
    $CpuRunner -match 'VideoEncodeSummary' -and $CpuRunner -match 'VideoProbeSummary') `
    'CPU lifecycle RequirePass/video and deterministic repeat-video contract is missing.'
Assert-Lab ($CpuRunner -match 'reference-contact-sheet\.png' -and
    $CpuRunner -match '\$ReferenceWidth = 256' -and
    $CpuRunner -match '\$ReferenceHeight = 224' -and
    $CpuRunner -match '\$ReferenceSheetWidth = 768' -and
    $CpuRunner -match '\$ReferenceSheetHeight = 896' -and
    $CpuRunner -notmatch '\$ReferenceHeight = 240' -and
    $CpuRunner -notmatch '\$ReferenceSheetHeight = 960' -and
    $CpuRunner -match '\$ReferenceScreenshotCount = 12' -and
    $CpuRunner -match 'Expected exactly \$ReferenceScreenshotCount reference frames' -and
    $CpuRunner -match 'has dimensions .*expected' -and
    $CpuRunner -match '\$NativeSheetWidth = 1920' -and
    $CpuRunner -match '\$NativeSheetHeight = 1920' -and
    $CpuRunner -match 'Get-FileRecord \$SheetPath .native-contact-sheet\.png. \$NativeSheetWidth \$NativeSheetHeight' -and
    $CpuRunner -match 'contact_sheet_hashes_equal' -and
    $CpuRunner -match 'contact_sheet\.sha256' -and
    $CpuRunner -match 'resolution = .256x224 FCEUX gui\.savescreenshotas PNG raster/crop' -and
    $CpuRunner -match 'video_resolution = .256x240 original AVI/video contract' -and
    $CpuRunner -match 'original_contact_sheets=two separate 768x896' -and
    $CpuRunner -notmatch 'original_contact_sheets=two separate 768x960' -and
    $CpuRunner -notmatch 'original=.*256x240 source traces' -and
    $CpuProofDoc -match '256x224' -and $CpuProofDoc -match '768x896' -and
    $CpuProofDoc -match 'original AVI/video resolution' -and $CpuProofDoc -match '256x240' -and
    $CpuManifest -match '256x224 FCEUX' -and
    $CpuManifest -match '768x896' -and
    $CpuManifest -match '256x240.*video' -and
    $CpuLineageDoc -match 'Two 768x896 original sheets and one 1920x1920 native sheet are required' -and
    $CpuLineageDoc -notmatch '(?m)^\| Original/native contact-sheet review was incomplete \| .*768x960') `
    'CPU lifecycle reference PNG/sheet dimensions, exact 12-frame inventory, or separate video-resolution contract is missing or stale.'
Assert-Lab ($CpuRunner -match '(?s)function New-ContactSheet .*?\[Drawing\.Bitmap\]::new\(\s*\[int\]\(\$CellWidth \* 3\),\s*\[int\]\(\$CellHeight \* 4\)\)' -and
    $CpuRunner -match '(?s)\$SheetPath = Join-Path \$NativeRoot.*?\[Drawing\.Bitmap\]::new\(\s*\[int\]\(\$NativeWidth \* 3\),\s*\[int\]\(\$NativeHeight \* 4\)\)' -and
    $CpuRunner -notmatch 'New-Object\s+Drawing\.Bitmap\s*\(') `
    'CPU lifecycle contact-sheet constructors are ambiguous, untyped, or missing independent original/native 3x4 coverage.'
Assert-Lab ($CpuRunner.Contains('$FrameKey = ''{0:D4}'' -f $Frame') -and
    $CpuRunner.Contains('$NativeFrameHashes[$FrameKey] = $Hash') -and
    $CpuRunner.Contains('$NativeFrameDetails[$FrameKey] = $Record') -and
    $CpuRunner.Contains('$NativeRepeatFrameDetails[$FrameKey] = $Record') -and
    $CpuRunner.Contains('if ($NativeFrameHashes[$FrameKey] -ne $Hash)') -and
    $CpuRunner -notmatch '\$NativeFrameHashes\[\$Frame\]' -and
    $CpuRunner -notmatch '\$NativeFrameDetails\[\$Frame\]' -and
    $CpuRunner -notmatch '\$NativeRepeatFrameDetails\[\$Frame\]') `
    'CPU lifecycle native frame dictionaries use numeric OrderedDictionary indexing or lack the canonical D4 string key.'
Assert-Lab ($CpuRunner -match 'Get-ArtifactInventory' -and
    $CpuRunner -match 'Empty proof artifact' -and
    $CpuRunner -match 'log_files' -and
    $CpuRunner -match 'no stdout/stderr emitted; exit=' -and
    $CpuRunner -match 'Add-ProcessLogMetadata' -and
    $CpuRunner -match 'start_utc' -and $CpuRunner -match 'end_utc' -and
    $CpuRunner -match 'lacks complete runner metadata' -and
    $CpuRunner -match 'lacks the explicit no-output record' -and
    $CpuRunner -match 'LogHashes\.Count -eq 0' -and
    $CpuRunner -match '\(\(Get-SessionBytes \$Path\) -gt 64MB\)' -and
    $CpuRunner -match '\(\(Get-SessionBytes \$OutputRoot\) -gt 64MB\)') `
    'CPU lifecycle log nonempty/inventory contract is missing.'
$PathCapMatches = [regex]::Matches($CpuRunner, '\(\(Get-SessionBytes \$Path\) -gt 64MB\)')
$OutputCapMatches = [regex]::Matches($CpuRunner, '\(\(Get-SessionBytes \$OutputRoot\) -gt 64MB\)')
Assert-Lab ($PathCapMatches.Count -eq 1 -and $OutputCapMatches.Count -eq 2 -and
    ($PathCapMatches.Count + $OutputCapMatches.Count) -eq 3 -and
    $CpuRunner -notmatch '(?m)^\s*if \(Get-SessionBytes (?:\$Path|\$OutputRoot)') `
    'CPU lifecycle session output cap comparison is unparenthesized or incomplete.'
Assert-Lab ($CpuMap -match 'source_hook\(0xB081, "candidate_scan_entry", "bank06", nil, "inferred_label"\)' -and
    $CpuMap -match 'source_hook\(0x9172, "primary_switch_entry", "bank06", "handler", "exact_opcode_entry"\)' -and
    $CpuMap -match 'source_hook\(0x8431, "shot_request_predicate", "bank06", nil, "exact_mechanics"\)' -and
    $CpuLua -match 'label_confidence ~= "inferred_label"' -and
    $CpuLua -match 'label_confidence = "exact_opcode_entry"') `
    'CPU lifecycle candidate/switch/shot label confidence is not explicitly bounded.'

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
$CpuRunnerTokens = $null
$CpuRunnerErrors = $null
[void][Management.Automation.Language.Parser]::ParseFile(
    $CpuRunnerPath, [ref]$CpuRunnerTokens, [ref]$CpuRunnerErrors)
Assert-Lab ($CpuRunnerErrors.Count -eq 0) ('CPU lifecycle runner PowerShell parse errors: ' +
    (($CpuRunnerErrors | ForEach-Object Message) -join '; '))

if ($Failures.Count -ne 0) {
    $Failures | ForEach-Object { Write-Error $_ }
    throw "$($Failures.Count) gameplay-lab static test(s) failed."
}
Write-Host 'GAMEPLAY LAB STATIC TEST PASS: closed profiles, CPU lifecycle proof surface, read-only controller policy, exact revisions, bounded output, point/velocity evidence, fail-closed shot/lifecycle evidence, neutral cleanup'

if ($Smoke) {
    if (-not $RomPath -or -not $FceuxPath) {
        throw '-Smoke requires explicit -RomPath and -FceuxPath.'
    }
    & $RunnerPath -RomPath $RomPath -FceuxPath $FceuxPath -RequirePass
}
