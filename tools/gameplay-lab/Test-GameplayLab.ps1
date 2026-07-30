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
Write-Host 'GAMEPLAY LAB STATIC TEST PASS: closed profiles, read-only controller policy, exact revisions, bounded output, point/velocity evidence, fail-closed shot evidence, neutral cleanup'

if ($Smoke) {
    if (-not $RomPath -or -not $FceuxPath) {
        throw '-Smoke requires explicit -RomPath and -FceuxPath.'
    }
    & $RunnerPath -RomPath $RomPath -FceuxPath $FceuxPath -RequirePass
}
