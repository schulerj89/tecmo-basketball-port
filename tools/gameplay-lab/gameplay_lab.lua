-- Read-only, fail-closed Rev 1 gameplay laboratory.
-- The only emulated inputs are complete two-port joypad tables.

local output_root = assert(os.getenv("TECMO_GAMEPLAY_LAB_OUTPUT"),
    "TECMO_GAMEPLAY_LAB_OUTPUT is required")
local map_path = assert(os.getenv("TECMO_GAMEPLAY_LAB_MAP"),
    "TECMO_GAMEPLAY_LAB_MAP is required")
output_root = string.gsub(output_root, "\\", "/")
map_path = string.gsub(map_path, "\\", "/")
local map = assert(dofile(map_path))
assert(map.schema == "TGLM-4" and map.schema_version == 4,
    "unsupported gameplay-lab map")
local profile_name = os.getenv("TECMO_GAMEPLAY_LAB_PROFILE") or
    "three_point_baseline"
local profile = map.profiles[profile_name]
assert(profile ~= nil and
       (profile_name == "three_point_baseline" or
        profile_name == "ordinary_two_point_make"),
    "unsupported gameplay-lab profile")
local shot_window = {}
for key, value in pairs(map.shot_policy) do shot_window[key] = value end
for key, value in pairs(profile.shot_window) do shot_window[key] = value end
map.shot_window = shot_window

local max_frames = tonumber(os.getenv("TECMO_GAMEPLAY_LAB_MAX_FRAMES") or "6000") or 6000
if max_frames < 3600 or max_frames > map.caps.max_frames then
    error("TECMO_GAMEPLAY_LAB_MAX_FRAMES is outside the canonical cap")
end
local movie_enabled = (os.getenv("TECMO_GAMEPLAY_LAB_RECORD_MOVIE") or "0") == "1"
local script_hash = os.getenv("TECMO_GAMEPLAY_LAB_SCRIPT_SHA256") or "missing"
local map_hash = os.getenv("TECMO_GAMEPLAY_LAB_MAP_SHA256") or "missing"
local rom_hash = os.getenv("TECMO_GAMEPLAY_LAB_ROM_SHA256") or "missing"
local fceux_hash = os.getenv("TECMO_GAMEPLAY_LAB_FCEUX_SHA256") or "missing"
assert(rom_hash == map.rom_sha256, "runner ROM fingerprint mismatch")
assert(fceux_hash == map.fceux_sha256, "runner FCEUX fingerprint mismatch")

pcall(FCEU.speedmode, "maximum")

local function path_join(a, b)
    if string.sub(a, -1) == "/" then return a .. b end
    return a .. "/" .. b
end

local telemetry = assert(io.open(path_join(output_root, "telemetry.csv"), "w"))
local events = assert(io.open(path_join(output_root, "events.csv"), "w"))
local phases = assert(io.open(path_join(output_root, "phases.csv"), "w"))
local detail = assert(io.open(path_join(output_root, "shot-detail.csv"), "w"))
local metadata = assert(io.open(path_join(output_root, "metadata.txt"), "w"))
local status_path = path_join(output_root, "status.txt")
local movie_path = path_join(output_root, "gameplay-lab.fm2")
local tracked_text_bytes = 0
local telemetry_rows = 0
local event_rows = 0
local phase_rows = 0
local detail_rows = 0
local screenshot_count = 0
local deferred_failure = nil
local files_closed = false
local movie_stopped = false

local function emit(file, text)
    tracked_text_bytes = tracked_text_bytes + string.len(text)
    if tracked_text_bytes > map.caps.tracked_text_bytes then
        deferred_failure = "gameplay-lab tracked-text byte guard exceeded"
        return
    end
    file:write(text)
end

emit(metadata, "schema=TGLAB-4\nschema_version=4\nmap_schema=" .. map.schema .. "\n")
emit(metadata, "profile=" .. profile_name .. "\n")
emit(metadata, "script_sha256=" .. script_hash .. "\nmap_sha256=" .. map_hash .. "\n")
emit(metadata, "rom_sha256=" .. rom_hash .. "\nfceux_sha256=" .. fceux_hash .. "\n")
emit(metadata, "policy=read-only RAM; complete two-port joypads; one Rev1 orientation-0 shot\n")
metadata:close()

local actor_fields = ""
for i = 0, 10 do
    actor_fields = actor_fields .. string.format(
        ",a%d_x,a%d_y,a%d_state,a%d_pose,a%d_facing,a%d_phase,a%d_altitude," ..
        "a%d_altitude_velocity,a%d_horizontal_velocity,a%d_vertical_velocity," ..
        "a%d_team_role,a%d_contact,a%d_timer",
        i, i, i, i, i, i, i, i, i, i, i, i, i)
end
emit(telemetry,
    "lab_frame,emu_frame,phase,action,p1,p2,mode,screen,period,minute,second,shot_clock," ..
    "offense_actor,defense_actor,offense_side,defense_side,control0,control1,orientation," ..
    "ball_distance,nearest_defender,nearest_distance,front_defender,front_distance," ..
    "shot_subtype,shot_flags,close_mode,score0,score1" .. actor_fields .. "\n")
emit(events,
    "lab_frame,emu_frame,hook_order,name,address,raw_bank,mapper_select,mapper_bank6," ..
    "mapper_bank7,pc,a,x,y,ba,point_value_0398,offense_actor,defense_actor," ..
    "offense_side,defense_side,control0,control1,score0,score1,shot_clock,close_mode," ..
    "shooter_facing,shooter_phase_low,shooter_x,shooter_y,state08_count," ..
    "target_x_0094_0095,target_y_0096_0097,slot10_x,slot10_y," ..
    "slot10_count_051d_0528," ..
    "slot10_altitude_048e_0499,slot10_altitude_velocity_04a4_04af," ..
    "miss_selector_6a,miss_selector_low2," ..
    "miss_selected_target,object_slot10_state_0478," ..
    "object_slot10_horizontal_velocity_04f1_04fc," ..
    "object_slot10_vertical_velocity_0507_0512," ..
    "saved_object_horizontal_velocity_038d_038e," ..
    "saved_object_vertical_velocity_038f_0390,sfx_mailbox_05b8\n")
emit(phases, "lab_frame,emu_frame,phase,reason\n")
emit(detail,
    "lab_frame,shot_frame,phase,p1,p2,shooter,defender,nearest,front," ..
    "shooter_x,shooter_y,shooter_state,shooter_pose,shooter_phase,shooter_altitude," ..
    "shooter_altitude_velocity,shooter_horizontal_velocity,shooter_vertical_velocity," ..
    "ball_x,ball_y,ball_state,ball_pose,ball_phase,ball_altitude," ..
    "ball_altitude_velocity,ball_horizontal_velocity,ball_vertical_velocity," ..
    "shot_subtype,shot_flags,close_mode,score0,score1\n")

local R = map.ram
local function rb(address)
    local value = memory.readbyte(address)
    if value == nil then return 0 end
    return value
end
local function word(lo, hi)
    return rb(lo) + 256 * rb(hi)
end
local function actor(index)
    return {
        index = index,
        x = rb(R.actor_x_lo + index) + 256 * rb(R.actor_x_hi + index),
        y = rb(R.actor_y + index),
        state = rb(R.actor_state + index),
        pose = rb(R.actor_pose_lo + index) + 256 * rb(R.actor_pose_hi + index),
        facing = rb(R.actor_facing + index),
        phase = rb(R.actor_phase + index),
        altitude = rb(R.actor_altitude_lo + index) + 256 * rb(R.actor_altitude_hi + index),
        altitude_velocity = word(
            R.actor_altitude_velocity_lo + index,
            R.actor_altitude_velocity_hi + index),
        horizontal_velocity = word(
            R.actor_horizontal_velocity_lo + index,
            R.actor_horizontal_velocity_hi + index),
        vertical_velocity = word(
            R.actor_vertical_velocity_lo + index,
            R.actor_vertical_velocity_hi + index),
        team_role = rb(R.actor_team_role + index),
        contact = rb(R.actor_contact + index),
        timer = rb(R.actor_timer + index)
    }
end
local function score(side)
    if side == 0 then return word(R.score0_lo, R.score0_hi) end
    return word(R.score1_lo, R.score1_hi)
end
local function safe_register(name)
    local ok, value = pcall(function()
        if memory and memory.getregister then return memory.getregister(name) end
        if emu and emu.getregister then return emu.getregister(name) end
        return 0
    end)
    if not ok or value == nil then return 0 end
    return value
end
local function safe_frame()
    local ok, value = pcall(emu.framecount)
    if not ok or value == nil then return 0 end
    return value
end
local function selected_miss_target(selector)
    return map.miss_variants.targets[AND(selector, map.miss_variants.selector_mask)]
end
local function distance(a, b)
    local dx, dy = a.x - b.x, a.y - b.y
    return math.sqrt(dx * dx + dy * dy), dx, dy
end
local function actor_side(a)
    return AND(a.team_role, 0x02) ~= 0 and 1 or 0
end
local function count_actor_state(value)
    local count = 0
    for i = 0, 10 do
        if actor(i).state == value then count = count + 1 end
    end
    return count
end
local function controller_readback(port)
    return rb(port == 1 and R.p1_current or R.p2_current)
end

local blank_pad = {
    A = false, B = false, up = false, down = false,
    left = false, right = false, start = false, select = false
}
local active_p1, active_p2 = {}, {}
local function complete_pad(source)
    return {
        A = source.A == true, B = source.B == true,
        up = source.up == true, down = source.down == true,
        left = source.left == true, right = source.right == true,
        start = source.start == true, select = source.select == true
    }
end
local function apply_pads(p1, p2)
    active_p1, active_p2 = complete_pad(p1 or blank_pad), complete_pad(p2 or blank_pad)
    joypad.set(1, active_p1)
    joypad.set(2, active_p2)
end
local function pad_text(p)
    local s = ""
    for _, key in ipairs({"A", "B", "up", "down", "left", "right", "start", "select"}) do
        if p[key] then s = s .. (s == "" and "" or "+") .. key end
    end
    return s == "" and "neutral" or s
end
local function routed_pads(port, button)
    local p1, p2 = {}, {}
    if port == 1 then p1[button] = true else p2[button] = true end
    return p1, p2
end

local movement_states = {right = 1, left = 2, down = 4, up = 8}
local function movement_state_valid(a, button)
    return a.state == 0 or a.state == movement_states[button]
end

local frame = 0
local phase = ""
local phase_started = 0
local last_action = "neutral"
local stopped = false
local stop_reason = "not stopped"
local result = "none"
local final_pads_neutral = false
local movie_started = false
local hook_queue = {}
local terminal_count = 0
local decision_frame = -1
local terminal_frame = -2
local make_seen, miss_seen = false, false
local release_seen, classifier_seen, result_seen, score_apply_seen = false, false, false, false
local point_classifier_seen, point_return_seen = false, false
local classified_point_value = -1
local settlement_seen, handoff_seen, close_launch_seen = false, false, false
local setup_control0, setup_control1 = -1, -1
local input_control0, input_control1 = -1, -1
local decision_control0, decision_control1 = -1, -1
local pre_score0, pre_score1 = -1, -1
local shot_side, shot_actor, shot_frame = -1, -1, -1
local possession_at_input = -1
local holder_proven, shot_window_proven = false, false
local proven_holder_actor = -1
local input_front, input_close_mode = -2, -1
local stable_holder = 0
local stable_safe = 0
local selected_threat = -1
local switch_origin = -1
local switch_seen = {}
local switch_unique_count = 0
local switch_saw_different = false
local switch_pending_actor = -1
local switch_pending_frame = -1
local switch_request_outstanding = false
local switch_request_deadline = -1
local defender_cycles_closed = 0
local defender_store_total = 0
local defender_passes = 0
local holder_changes = 0
local holder_seen = {}
local pass_origin_holder = -1
local pass_origin_side = -1
local pass_score0, pass_score1 = -1, -1
local pass_deadline = -1
local pass_pulsed = false
local reacquire_actor = -1
local reacquire_stable = 0
local held_direction = nil
local held_direction_port = -1
local held_direction_actor = -1
local pending_direction = nil
local movement_next_phase = nil
local movement_next_reason = nil
local progress_deadline = 0
local best_distance = 99999
local gameplay_frame = -1
local root_seen = false
local tail_started = -1
local hook_order = 0
local hook_first_order = {}
local hook_first_frame = {}
local hook_last_order = {}
local hook_last_frame = {}
local hook_seen_count = {}
local release_emu_frame = -1
local score_emu_frame = -1
local handoff_emu_frame = -1
local b100_entry_count = 0
local timing_capture = {
    target_x = -1, target_y = -1,
    shooter_x = -1, shooter_y = -1,
    slot10_x = -1, slot10_y = -1,
    slot10_count = -1, slot10_altitude = -1,
    slot10_altitude_velocity = -1,
    slot10_horizontal_velocity = -1,
    slot10_vertical_velocity = -1,
    pre_remap_direction = -1, post_remap_direction = -1,
    launch_direction = -1, launch_phase_low = -1, launch_close_mode = -1,
    solver_direction = -1, solver_phase_low = -1, solver_close_mode = -1,
    score_shot_clock = -1,
    score_before0 = -1, score_before1 = -1,
    score_after0 = -1, score_after1 = -1,
    transition_mailbox = -1, swap_entry_mailbox = -1,
    swap_complete_mailbox = -1,
    swap_before_side = -1, swap_before_holder = -1,
    swap_after_side = -1, swap_after_holder = -1
}

local function set_phase(next_phase, reason)
    if phase == next_phase then return end
    phase = next_phase
    phase_started = frame
    phase_rows = phase_rows + 1
    if phase_rows > map.caps.phase_rows then
        deferred_failure = "gameplay-lab phase-row cap exceeded"
        return
    end
    emit(phases, string.format("%d,%d,%s,%s\n", frame, safe_frame(), phase,
        string.gsub(reason or "", ",", ";")))
end

local function reset_defender_cycle(threat, reason)
    selected_threat = threat
    switch_origin = rb(R.defense_actor)
    switch_seen = {[switch_origin] = true}
    switch_unique_count = 1
    switch_saw_different = false
    switch_pending_actor = -1
    switch_pending_frame = -1
    switch_request_outstanding = false
    switch_request_deadline = -1
    set_phase("select_defender", reason)
end

local function begin_movement_neutral(next_phase, reason)
    movement_next_phase = next_phase
    movement_next_reason = reason
    set_phase("movement_neutral", "neutral gate before direction/controller change")
end

local slot10_timing_hooks = {
    flight_target_slot10_selected = true,
    target_motion_solver = true,
    flight_target_ready = true,
    flight_state5_update = true,
    flight_state5_hold_return = true,
    flight_state7_store_boundary = true,
    result_state7_dispatch = true,
    made_stat_update = true,
    score_apply = true,
    score_committed = true,
    state08_route_boundary = true,
    state09_route_entry = true
}

local function remember_timing_hook(h)
    if shot_frame < 0 then return end
    if slot10_timing_hooks[h.name] and h.x ~= 10 then return end
    hook_seen_count[h.name] = (hook_seen_count[h.name] or 0) + 1
    if hook_first_order[h.name] == nil then
        hook_first_order[h.name] = h.order
        hook_first_frame[h.name] = h.emu_frame
    end
    hook_last_order[h.name] = h.order
    hook_last_frame[h.name] = h.emu_frame
    if h.name == "ball_release" then
        timing_capture.pre_remap_direction = h.shooter_facing
    elseif h.name == "ordinary_direction_remap_ready" then
        timing_capture.post_remap_direction = h.shooter_facing
    elseif h.name == "flight_target_setup" then
        timing_capture.launch_direction = h.shooter_facing
        timing_capture.launch_phase_low = h.shooter_phase_low
        timing_capture.launch_close_mode = h.close_mode
    elseif h.name == "target_motion_solver" then
        timing_capture.solver_direction = h.shooter_facing
        timing_capture.solver_phase_low = h.shooter_phase_low
        timing_capture.solver_close_mode = h.close_mode
    elseif h.name == "flight_target_ready" then
        timing_capture.target_x, timing_capture.target_y = h.target_x, h.target_y
        timing_capture.shooter_x, timing_capture.shooter_y =
            h.shooter_x, h.shooter_y
        timing_capture.slot10_x, timing_capture.slot10_y =
            h.object_slot10_x, h.object_slot10_y
        timing_capture.slot10_count = h.object_slot10_count
        timing_capture.slot10_altitude = h.object_slot10_altitude
        timing_capture.slot10_altitude_velocity =
            h.object_slot10_altitude_velocity
        timing_capture.slot10_horizontal_velocity =
            h.object_slot10_horizontal_velocity
        timing_capture.slot10_vertical_velocity =
            h.object_slot10_vertical_velocity
    elseif h.name == "flight_state5_update" then
        b100_entry_count = b100_entry_count + 1
    elseif h.name == "made_stat_update" then
        timing_capture.score_shot_clock = h.shot_clock
    elseif h.name == "score_apply" then
        timing_capture.score_before0, timing_capture.score_before1 =
            h.score0, h.score1
    elseif h.name == "score_committed" then
        score_emu_frame = h.emu_frame
        timing_capture.score_after0, timing_capture.score_after1 =
            h.score0, h.score1
    elseif h.name == "possession_transition_gate" then
        timing_capture.transition_mailbox = h.sfx_mailbox
    elseif h.name == "possession_swap_entry" then
        timing_capture.swap_entry_mailbox = h.sfx_mailbox
        timing_capture.swap_before_side = h.offense_side
        timing_capture.swap_before_holder = h.offense_actor
    elseif h.name == "possession_swap_complete" then
        handoff_emu_frame = h.emu_frame
        timing_capture.swap_complete_mailbox = h.sfx_mailbox
        timing_capture.swap_after_side = h.offense_side
        timing_capture.swap_after_holder = h.offense_actor
    end
end

local function two_point_timing_evidence_valid()
    if not profile.require_timing_evidence then return true end
    local contract = profile.timing_contract
    if contract == nil then return false end
    local required = {
        "shot_classifier", "point_classifier_local", "two_point_return_local",
        "ball_release", "ordinary_direction_remap_ready",
        "shot_result", "decision_anchor", "terminal_make_bit7_clear",
        "flight_target_setup", "flight_target_slot10_selected",
        "target_motion_solver", "flight_target_ready",
        "flight_state5_update", "flight_state5_hold_return",
        "flight_state7_store_boundary", "result_state7_dispatch",
        "made_stat_update", "score_apply", "score_committed",
        "state08_route_boundary", "state09_route_entry",
        "possession_transition_gate", "possession_swap_entry",
        "possession_swap_complete"
    }
    for _, name in ipairs(required) do
        if hook_first_order[name] == nil then return false end
    end
    local ordered = hook_first_order.shot_classifier <
        hook_first_order.point_classifier_local and
        hook_first_order.point_classifier_local <
            hook_first_order.two_point_return_local and
        hook_first_order.two_point_return_local <
            hook_first_order.ball_release and
        hook_first_order.ball_release <
            hook_first_order.ordinary_direction_remap_ready and
        hook_first_order.ordinary_direction_remap_ready <
            hook_first_order.shot_result and
        hook_first_order.shot_result <
            hook_first_order.decision_anchor and
        hook_first_order.decision_anchor <
            hook_first_order.terminal_make_bit7_clear and
        hook_first_order.terminal_make_bit7_clear <
            hook_first_order.flight_target_setup and
        hook_first_order.flight_target_setup <
            hook_first_order.flight_target_slot10_selected and
        hook_first_order.flight_target_slot10_selected <
        hook_first_order.target_motion_solver and
        hook_first_order.target_motion_solver <
            hook_first_order.flight_target_ready and
        hook_first_order.flight_target_ready <
            hook_first_order.flight_state5_update and
        hook_last_order.flight_state5_update <
            hook_first_order.flight_state5_hold_return and
        hook_first_order.flight_state5_hold_return <
            hook_first_order.flight_state7_store_boundary and
        hook_first_order.flight_state7_store_boundary <
            hook_first_order.result_state7_dispatch and
        hook_first_order.result_state7_dispatch <
            hook_first_order.made_stat_update and
        hook_first_order.made_stat_update < hook_first_order.score_apply and
        hook_first_order.score_apply < hook_first_order.score_committed and
        hook_first_order.score_committed <
            hook_first_order.state08_route_boundary and
        hook_last_order.state08_route_boundary <
            hook_first_order.state09_route_entry and
        hook_first_order.state09_route_entry <
            hook_first_order.possession_transition_gate and
        hook_first_order.possession_transition_gate <
            hook_first_order.possession_swap_entry and
        hook_first_order.possession_swap_entry <
            hook_first_order.possession_swap_complete
    local snapshots =
        timing_capture.pre_remap_direction ==
            contract.pre_remap_direction and
        timing_capture.post_remap_direction ==
            contract.post_remap_direction and
        timing_capture.launch_direction == contract.launch_direction and
        timing_capture.launch_phase_low == contract.launch_phase_low and
        timing_capture.launch_close_mode == contract.launch_close_mode and
        timing_capture.solver_direction == contract.launch_direction and
        timing_capture.solver_phase_low == contract.launch_phase_low and
        timing_capture.solver_close_mode == contract.launch_close_mode and
        timing_capture.target_x == contract.target_x and
        timing_capture.target_y == contract.target_y and
        timing_capture.slot10_count == contract.slot10_count and
        timing_capture.slot10_altitude == contract.slot10_altitude and
        timing_capture.slot10_altitude_velocity ==
            contract.slot10_altitude_velocity and
        timing_capture.slot10_x - timing_capture.shooter_x ==
            contract.slot10_x_offset and
        timing_capture.slot10_y - timing_capture.shooter_y ==
            contract.slot10_y_offset and
        timing_capture.slot10_horizontal_velocity >=
            contract.slot10_horizontal_velocity_min and
        timing_capture.slot10_horizontal_velocity <=
            contract.slot10_horizontal_velocity_max and
        timing_capture.slot10_vertical_velocity >=
            contract.slot10_vertical_velocity_min and
        timing_capture.slot10_vertical_velocity <=
            contract.slot10_vertical_velocity_max and
        timing_capture.score_shot_clock == contract.score_shot_clock and
        timing_capture.transition_mailbox ==
            contract.terminal_sfx_mailbox and
        timing_capture.swap_entry_mailbox ==
            contract.terminal_sfx_mailbox and
        timing_capture.swap_complete_mailbox ==
            contract.terminal_sfx_mailbox
    local exact_counts =
        hook_seen_count.shot_classifier == 1 and
        hook_seen_count.point_classifier_local == 1 and
        hook_seen_count.two_point_return_local == 1 and
        hook_seen_count.ball_release == 1 and
        hook_seen_count.ordinary_direction_remap_ready == 1 and
        hook_seen_count.shot_result == 1 and
        hook_seen_count.decision_anchor == 1 and
        hook_seen_count.terminal_make_bit7_clear == 1 and
        (hook_seen_count.terminal_miss_bit7_set or 0) == 0 and
        (hook_seen_count.close_launch or 0) == 0 and
        hook_seen_count.flight_target_setup == 1 and
        hook_seen_count.flight_target_slot10_selected == 1 and
        hook_seen_count.target_motion_solver == 1 and
        hook_seen_count.flight_target_ready == 1 and
        b100_entry_count == contract.flight_state5_updates and
        hook_seen_count.flight_state5_hold_return == 1 and
        hook_seen_count.flight_state7_store_boundary == 1 and
        hook_seen_count.result_state7_dispatch == 1 and
        hook_seen_count.made_stat_update == 1 and
        hook_seen_count.score_apply == 1 and
        hook_seen_count.score_committed == 1 and
        hook_seen_count.state08_route_boundary ==
            contract.state08_route_updates and
        hook_seen_count.state09_route_entry == 1 and
        hook_seen_count.possession_transition_gate == 1 and
        hook_seen_count.possession_swap_entry == 1 and
        hook_seen_count.possession_swap_complete == 1
    local exact_frames =
        hook_first_frame.shot_result == hook_first_frame.decision_anchor and
        hook_first_frame.decision_anchor ==
            hook_first_frame.terminal_make_bit7_clear and
        hook_first_frame.state09_route_entry ==
            hook_first_frame.possession_transition_gate and
        hook_first_frame.possession_transition_gate ==
            hook_first_frame.possession_swap_entry and
        hook_first_frame.possession_swap_entry ==
            hook_first_frame.possession_swap_complete
    local exact_score = timing_capture.score_before0 == pre_score0 and
        timing_capture.score_before1 == pre_score1 and
        timing_capture.score_after0 - timing_capture.score_before0 ==
            profile.expected_score_delta and
        timing_capture.score_after1 == timing_capture.score_before1
    local deltas = release_emu_frame >= 0 and score_emu_frame >= release_emu_frame and
        handoff_emu_frame >= score_emu_frame
    local actual_swap =
        timing_capture.swap_before_side == possession_at_input and
        timing_capture.swap_after_side == 1 - possession_at_input and
        timing_capture.swap_before_holder >= 0 and
        timing_capture.swap_after_holder >= 0 and
        timing_capture.swap_before_holder ~= timing_capture.swap_after_holder
    return ordered and snapshots and exact_counts and exact_frames and exact_score and
        deltas and actual_swap
end

local function save_screenshot(label)
    if screenshot_count >= map.caps.screenshots then return end
    screenshot_count = screenshot_count + 1
    local clean = string.gsub(label, "[^%w_%-]", "_")
    pcall(gui.savescreenshotas, path_join(output_root,
        string.format("%05d_%s.png", frame, clean)))
end

local bank_select = 0
local bank_registers = {0, 0, 0, 0, 0, 0, 0, 0}
local bank_known = {false, false, false, false, false, false, false, false}
local function mapped_raw_bank(address)
    if address >= 0x8000 and address <= 0x9FFF then
        if AND(bank_select, 0x40) ~= 0 then return 0x0E end
        if not bank_known[7] then return nil end
        return AND(bank_registers[7], 0x0F)
    elseif address <= 0xBFFF then
        if not bank_known[8] then return nil end
        return AND(bank_registers[8], 0x0F)
    elseif address <= 0xDFFF then
        if AND(bank_select, 0x40) == 0 then return 0x0E end
        if not bank_known[7] then return nil end
        return AND(bank_registers[7], 0x0F)
    end
    return 0x0F
end
memory.registerwrite(map.mapper.select, function(_, _, value) bank_select = value or 0 end)
memory.registerwrite(map.mapper.data, function(_, _, value)
    local index = AND(bank_select, 0x07) + 1
    local value8 = value or 0
    if index == 7 or index == 8 then value8 = AND(value8, 0x0F) end
    bank_registers[index], bank_known[index] = value8, true
end)
local function register_hook(hook)
    memory.registerexec(hook.address, function()
        local bank = mapped_raw_bank(hook.address)
        local accepted = (hook.gate == "bank05" and map.bank05_raw[bank]) or
            (hook.gate == "bank06" and map.bank06_raw[bank])
        if hook.name == "shot_classifier" and
                (shot_frame < 0 or classifier_seen) then
            return
        end
        if accepted then
            if event_rows + #hook_queue >= map.caps.event_rows then
                deferred_failure = "gameplay-lab event-row cap exceeded"
                return
            end
            local miss_selector = rb(R.miss_variant_selector)
            local offense_actor = rb(R.offense_actor)
            hook_order = hook_order + 1
            local hook_a = safe_register("a")
            local target_x = word(R.flight_target_x_lo, R.flight_target_x_hi)
            local target_y = word(R.flight_target_y_lo, R.flight_target_y_hi)
            hook_queue[#hook_queue + 1] = {
                name = hook.name, address = hook.address, bank = bank,
                order = hook_order, lab_frame = frame,
                emu_frame = safe_frame(), pc = safe_register("pc"),
                a = hook_a, x = safe_register("x"), y = safe_register("y"),
                mapper_select = bank_select,
                mapper_bank6 = bank_registers[7],
                mapper_bank7 = bank_registers[8],
                shot_flags = rb(R.shot_flags),
                point_value = rb(R.point_value),
                offense_actor = offense_actor,
                defense_actor = rb(R.defense_actor),
                offense_side = rb(R.offense_side),
                defense_side = rb(R.defense_side),
                control0 = rb(R.control0),
                control1 = rb(R.control1),
                score0 = score(0),
                score1 = score(1),
                shot_clock = rb(R.shot_clock),
                close_mode = rb(R.close_mode),
                shooter_facing = offense_actor <= 9 and
                    rb(R.actor_facing + offense_actor) or 0xFF,
                shooter_phase_low = offense_actor <= 9 and
                    AND(rb(R.actor_phase + offense_actor), 0x0F) or 0xFF,
                shooter_x = offense_actor <= 9 and
                    word(R.actor_x_lo + offense_actor,
                        R.actor_x_hi + offense_actor) or 0xFFFF,
                shooter_y = offense_actor <= 9 and
                    rb(R.actor_y + offense_actor) or 0xFF,
                state08_count = count_actor_state(0x08),
                target_x = target_x,
                target_y = target_y,
                object_slot10_x = word(
                    R.actor_x_lo + 10,
                    R.actor_x_hi + 10),
                object_slot10_y = rb(R.actor_y + 10),
                object_slot10_count = word(
                    R.object_slot10_count_lo,
                    R.object_slot10_count_hi),
                object_slot10_altitude = word(
                    R.actor_altitude_lo + 10,
                    R.actor_altitude_hi + 10),
                object_slot10_altitude_velocity = word(
                    R.actor_altitude_velocity_lo + 10,
                    R.actor_altitude_velocity_hi + 10),
                miss_selector = miss_selector,
                miss_selector_low2 = AND(miss_selector, map.miss_variants.selector_mask),
                miss_selected_target = selected_miss_target(miss_selector),
                object_slot10_state = rb(R.object_slot10_state),
                object_slot10_horizontal_velocity = word(
                    R.actor_horizontal_velocity_lo + 10,
                    R.actor_horizontal_velocity_hi + 10),
                object_slot10_vertical_velocity = word(
                    R.actor_vertical_velocity_lo + 10,
                    R.actor_vertical_velocity_hi + 10),
                saved_object_horizontal_velocity = word(
                    R.saved_object_horizontal_velocity_lo,
                    R.saved_object_horizontal_velocity_hi),
                saved_object_vertical_velocity = word(
                    R.saved_object_vertical_velocity_lo,
                    R.saved_object_vertical_velocity_hi),
                sfx_mailbox = rb(R.sfx_mailbox)
            }
            if hook.name == "defender_switch_store" and phase == "select_defender" and
                    switch_request_outstanding and switch_pending_actor < 0 then
                switch_pending_actor = hook_a
                switch_pending_frame = safe_frame()
            end
        end
    end)
end
for _, hook in ipairs(map.hooks) do register_hook(hook) end

local function flush_hooks()
    for _, h in ipairs(hook_queue) do
        event_rows = event_rows + 1
        if event_rows > map.caps.event_rows then
            deferred_failure = "gameplay-lab event-row cap exceeded"
            return
        end
        emit(events, string.format(
            "%d,%d,%d,%s,%04X,%02X,%02X,%02X,%02X,%04X,%02X,%02X,%02X,%02X," ..
            "%02X,%d,%d,%d,%d,%d,%d,%d,%d,%d,%02X,%02X,%02X,%04X,%02X,%d," ..
            "%04X,%04X,%04X,%02X,%04X,%04X,%04X,%02X,%d,%04X,%02X,%04X,%04X,%04X,%04X,%02X\n",
            h.lab_frame, h.emu_frame, h.order, h.name, h.address, h.bank,
            h.mapper_select, h.mapper_bank6, h.mapper_bank7, h.pc, h.a, h.x, h.y,
            h.shot_flags, h.point_value, h.offense_actor, h.defense_actor,
            h.offense_side, h.defense_side, h.control0, h.control1,
            h.score0, h.score1, h.shot_clock, h.close_mode, h.shooter_facing,
            h.shooter_phase_low, h.shooter_x, h.shooter_y, h.state08_count,
            h.target_x, h.target_y, h.object_slot10_x, h.object_slot10_y,
            h.object_slot10_count, h.object_slot10_altitude,
            h.object_slot10_altitude_velocity, h.miss_selector, h.miss_selector_low2,
            h.miss_selected_target, h.object_slot10_state,
            h.object_slot10_horizontal_velocity, h.object_slot10_vertical_velocity,
            h.saved_object_horizontal_velocity, h.saved_object_vertical_velocity,
            h.sfx_mailbox))
        remember_timing_hook(h)
        if h.name == "ball_release" then
            release_seen = true
            release_emu_frame = h.emu_frame
            save_screenshot("ball_release")
        elseif h.name == "shot_classifier" and shot_frame >= 0 then
            classifier_seen = true
        elseif h.name == "shot_result" then
            result_seen = true
        elseif h.name == "point_classifier_local" and shot_frame >= 0 then
            point_classifier_seen = true
        elseif h.name == "two_point_return_local" and shot_frame >= 0 then
            point_return_seen = true
            classified_point_value = h.point_value
        elseif h.name == "decision_anchor" then
            decision_frame = h.emu_frame
            decision_control0, decision_control1 = h.control0, h.control1
            save_screenshot("decision")
        elseif h.name == "terminal_make_bit7_clear" or h.name == "terminal_miss_bit7_set" then
            if h.emu_frame == decision_frame then
                terminal_count = terminal_count + 1
                terminal_frame = h.emu_frame
                make_seen = h.name == "terminal_make_bit7_clear"
                miss_seen = h.name == "terminal_miss_bit7_set"
                result = make_seen and "make" or "miss"
            end
        elseif h.name == "score_apply" and shot_frame >= 0 then
            score_apply_seen = true
            save_screenshot("score_apply")
        elseif h.name == "settlement" and shot_frame >= 0 then
            settlement_seen = true
        elseif h.name == "possession_swap_entry" and shot_frame >= 0 then
            handoff_seen = true
        elseif h.name == "close_launch" and shot_frame >= 0 then
            close_launch_seen = true
        end
    end
    hook_queue = {}
end

local function scan_world()
    local offense_index = rb(R.offense_actor)
    local shooter = offense_index <= 9 and actor(offense_index) or actor(0)
    local ball = actor(10)
    local nearest, nearest_distance = -1, 99999
    local front, front_distance = -1, 99999
    local toward_hoop = map.hoops[rb(R.orientation)] or map.hoops[0]
    local front_sign = toward_hoop.x >= shooter.x and 1 or -1
    for i = 0, 9 do
        local candidate = actor(i)
        if i ~= offense_index and actor_side(candidate) ~= rb(R.offense_side) then
            local d, dx, dy = distance(shooter, candidate)
            if d < nearest_distance then nearest, nearest_distance = i, d end
            if (-dx) * front_sign > 0 and math.abs(dx) <= map.shot_window.threat_front_x and
                    math.abs(dy) <= map.shot_window.threat_y and d < front_distance then
                front, front_distance = i, d
            end
        end
    end
    local ball_distance = distance(shooter, ball)
    return shooter, ball, nearest, nearest_distance, front, front_distance, ball_distance
end

local function live_common_valid()
    return rb(R.mode) == map.live.mode and rb(R.screen) == map.live.screen and
        rb(R.period) == map.live.period and rb(R.team0) ~= rb(R.team1) and
        rb(R.control0) == 0 and rb(R.control1) == 0 and
        rb(R.defense_side) == 1 - rb(R.offense_side) and
        rb(R.orientation) == map.supported_orientation and
        rb(R.offense_actor) <= 9 and rb(R.defense_actor) <= 9
end
local function tip_base_valid()
    -- The original tip performs a bounded asynchronous presentation/load and
    -- temporarily changes mode/screen. Ownership and setup must remain exact;
    -- acquire is impossible until the canonical live pair returns.
    return rb(R.period) == map.live.period and rb(R.team0) ~= rb(R.team1) and
        rb(R.control0) == 0 and rb(R.control1) == 0 and
        rb(R.defense_side) == 1 - rb(R.offense_side) and
        rb(R.orientation) == map.supported_orientation and
        rb(R.offense_actor) <= 9 and rb(R.defense_actor) <= 9
end
local function live_base_valid()
    return live_common_valid() and rb(R.offense_side) == map.supported_offense_side
end

local function pre_action_valid(shooter, ball, ball_distance)
    return live_base_valid() and rb(R.foul_route) == 0 and rb(R.violation_route) == 0 and
        rb(R.action_gate) == 0 and AND(rb(R.shot_flags), 0x40) == 0 and
        actor_side(shooter) == rb(R.offense_side) and
        ball.state == 0 and shooter.state == 0 and ball_distance <= map.shot_window.holder_distance and
        rb(R.close_mode) == 0
end

local function movement_pre_action_valid(shooter, ball, ball_distance, button)
    return live_base_valid() and rb(R.foul_route) == 0 and
        rb(R.violation_route) == 0 and rb(R.action_gate) == 0 and
        AND(rb(R.shot_flags), 0x40) == 0 and
        actor_side(shooter) == rb(R.offense_side) and ball.state == 0 and
        ball_distance <= map.shot_window.holder_distance and rb(R.close_mode) == 0 and
        movement_state_valid(shooter, button)
end

local function write_telemetry()
    telemetry_rows = telemetry_rows + 1
    if telemetry_rows > max_frames then
        deferred_failure = "gameplay-lab telemetry-row cap exceeded"
        return
    end
    local shooter, ball, nearest, nearest_distance, front, front_distance, ball_distance = scan_world()
    local line = string.format(
        "%d,%d,%s,%s,%s,%s,%02X,%02X,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.2f,%d,%.2f,%d,%.2f,%02X,%02X,%02X,%d,%d",
        frame, safe_frame(), phase, last_action, pad_text(active_p1), pad_text(active_p2),
        rb(R.mode), rb(R.screen), rb(R.period), rb(R.minute), rb(R.second), rb(R.shot_clock),
        rb(R.offense_actor), rb(R.defense_actor), rb(R.offense_side), rb(R.defense_side),
        rb(R.control0), rb(R.control1), rb(R.orientation), ball_distance,
        nearest, nearest_distance, front, front_distance, rb(R.shot_subtype),
        rb(R.shot_flags), rb(R.close_mode), score(0), score(1))
    for i = 0, 10 do
        local a = actor(i)
        line = line .. string.format(
            ",%d,%d,%02X,%04X,%02X,%02X,%04X,%04X,%04X,%04X,%02X,%02X,%02X",
            a.x, a.y, a.state, a.pose, a.facing, a.phase, a.altitude,
            a.altitude_velocity, a.horizontal_velocity, a.vertical_velocity,
            a.team_role, a.contact, a.timer)
    end
    emit(telemetry, line .. "\n")
end

local function write_detail()
    if shot_frame < 0 or detail_rows >= map.caps.shot_detail_rows then return end
    detail_rows = detail_rows + 1
    local shooter, ball, nearest, _, front = scan_world()
    emit(detail, string.format(
        "%d,%d,%s,%s,%s,%d,%d,%d,%d,%d,%d,%02X,%04X,%02X,%04X,%04X,%04X,%04X," ..
        "%d,%d,%02X,%04X,%02X,%04X,%04X,%04X,%04X,%02X,%02X,%02X,%d,%d\n",
        frame, frame - shot_frame, phase, pad_text(active_p1), pad_text(active_p2),
        shooter.index, rb(R.defense_actor), nearest, front,
        shooter.x, shooter.y, shooter.state, shooter.pose, shooter.phase,
        shooter.altitude, shooter.altitude_velocity, shooter.horizontal_velocity,
        shooter.vertical_velocity, ball.x, ball.y, ball.state, ball.pose,
        ball.phase, ball.altitude, ball.altitude_velocity, ball.horizontal_velocity,
        ball.vertical_velocity, rb(R.shot_subtype),
        rb(R.shot_flags), rb(R.close_mode), score(0), score(1)))
end

local function status_acceptance()
    local pass = false
    local score0_delta = pre_score0 < 0 and 0 or score(0) - pre_score0
    local score1_delta = pre_score1 < 0 and 0 or score(1) - pre_score1
    local point_evidence = point_classifier_seen and point_return_seen and
        classified_point_value == profile.expected_point_value
    local timing_evidence = two_point_timing_evidence_valid()
    if terminal_count == 1 and release_seen and classifier_seen and result_seen and
            setup_control0 == 0 and setup_control1 == 0 and
            input_control0 == 0 and input_control1 == 0 and
            decision_control0 == 0 and decision_control1 == 0 and
            shot_side == 0 and rb(R.team0) ~= rb(R.team1) and
            holder_proven and shot_window_proven and input_front == -1 and
            input_close_mode == 0 and not close_launch_seen and final_pads_neutral then
        if make_seen then
            if profile.require_point_evidence then
                pass = profile.expected_make and point_evidence and score_apply_seen and
                    score0_delta == profile.expected_score_delta and score1_delta == 0 and
                    (handoff_seen or rb(R.offense_side) ~= possession_at_input) and
                    timing_evidence
            else
                pass = score_apply_seen and (score0_delta == 2 or score0_delta == 3) and
                    score1_delta == 0 and
                    (handoff_seen or rb(R.offense_side) ~= possession_at_input)
            end
        elseif miss_seen then
            pass = not profile.expected_make and score0_delta == 0 and
                score1_delta == 0 and settlement_seen
        end
    end
    return pass, timing_evidence
end

local function write_status_overview(file, state, pass, timing_evidence)
    file:write("schema=TGLAB-4\nschema_version=4\nmap_schema=" .. map.schema .. "\n")
    file:write("profile=" .. profile_name .. "\n")
    file:write("script_sha256=" .. script_hash .. "\nmap_sha256=" .. map_hash .. "\n")
    file:write("rom_sha256=" .. rom_hash .. "\nfceux_sha256=" .. fceux_hash .. "\n")
    file:write("state=" .. state .. "\nlab_frame=" .. frame .. "\nemu_frame=" .. safe_frame() .. "\n")
    file:write("phase=" .. phase .. "\nlast_action=" .. last_action .. "\n")
    file:write("result=" .. result .. "\npilot_pass=" .. tostring(pass) .. "\n")
    file:write("terminal_count=" .. terminal_count .. "\nrelease_seen=" .. tostring(release_seen) .. "\n")
    file:write("classifier_seen=" .. tostring(classifier_seen) .. "\n")
    file:write("point_classifier_seen=" .. tostring(point_classifier_seen) .. "\n")
    file:write("point_return_seen=" .. tostring(point_return_seen) .. "\n")
    file:write("classified_point_value=" .. classified_point_value .. "\n")
    file:write("expected_point_value=" .. profile.expected_point_value .. "\n")
    file:write("result_seen=" .. tostring(result_seen) .. "\nscore_apply_seen=" .. tostring(score_apply_seen) .. "\n")
    file:write("settlement_seen=" .. tostring(settlement_seen) .. "\nhandoff_seen=" .. tostring(handoff_seen) .. "\n")
    file:write("timing_evidence_valid=" .. tostring(timing_evidence) .. "\n")
end

local function write_status_capture(file)
    file:write("defender_cycles_closed=" .. defender_cycles_closed .. "\n")
    file:write("defender_confirmed_stores=" .. defender_store_total .. "\n")
    file:write("defender_cycle_unique_actors=" .. switch_unique_count .. "\n")
    file:write("holder_passes=" .. defender_passes .. "\nholder_changes=" .. holder_changes .. "\n")
    file:write("b100_entry_count=" .. b100_entry_count .. "\n")
    file:write("captured_target=" .. timing_capture.target_x .. "," ..
        timing_capture.target_y .. "\n")
    file:write("captured_shooter_position=" .. timing_capture.shooter_x .. "," ..
        timing_capture.shooter_y .. "\n")
    file:write("captured_slot10_position=" .. timing_capture.slot10_x .. "," ..
        timing_capture.slot10_y .. "\n")
    file:write("captured_slot10_count=" .. timing_capture.slot10_count .. "\n")
    file:write("captured_slot10_altitude=" ..
        timing_capture.slot10_altitude .. "\n")
    file:write("captured_slot10_altitude_velocity=" ..
        timing_capture.slot10_altitude_velocity .. "\n")
    file:write("captured_slot10_horizontal_velocity=" ..
        timing_capture.slot10_horizontal_velocity .. "\n")
    file:write("captured_slot10_vertical_velocity=" ..
        timing_capture.slot10_vertical_velocity .. "\n")
    file:write("captured_pre_remap_direction=" ..
        timing_capture.pre_remap_direction .. "\n")
    file:write("captured_post_remap_direction=" ..
        timing_capture.post_remap_direction .. "\n")
    file:write("captured_launch_direction=" ..
        timing_capture.launch_direction .. "\n")
    file:write("captured_launch_phase_low=" ..
        timing_capture.launch_phase_low .. "\n")
    file:write("captured_launch_close_mode=" ..
        timing_capture.launch_close_mode .. "\n")
    file:write("captured_solver_direction=" ..
        timing_capture.solver_direction .. "\n")
    file:write("captured_solver_phase_low=" ..
        timing_capture.solver_phase_low .. "\n")
    file:write("captured_solver_close_mode=" ..
        timing_capture.solver_close_mode .. "\n")
    file:write("captured_score_shot_clock=" ..
        timing_capture.score_shot_clock .. "\n")
    file:write("captured_terminal_mailboxes=" ..
        timing_capture.transition_mailbox .. "," ..
        timing_capture.swap_entry_mailbox .. "," ..
        timing_capture.swap_complete_mailbox .. "\n")
    file:write("state08_route_updates=" ..
        (hook_seen_count.state08_route_boundary or 0) .. "\n")
    file:write("score_apply_snapshot=" .. timing_capture.score_before0 .. "," ..
        timing_capture.score_before1 .. "\n")
    file:write("score_commit_snapshot=" .. timing_capture.score_after0 .. "," ..
        timing_capture.score_after1 .. "\n")
    file:write("score_frame_delta=" ..
        ((release_emu_frame < 0 or score_emu_frame < 0) and -1 or
            score_emu_frame - release_emu_frame) .. "\n")
    file:write("handoff_frame_delta=" ..
        ((release_emu_frame < 0 or handoff_emu_frame < 0) and -1 or
            handoff_emu_frame - release_emu_frame) .. "\n")
    file:write("actual_swap=" .. timing_capture.swap_before_side .. "," ..
        timing_capture.swap_after_side .. "," ..
        timing_capture.swap_before_holder .. "," ..
        timing_capture.swap_after_holder .. "\n")
end

local function write_status_context(file)
    file:write("setup_control_pair=" .. setup_control0 .. "," .. setup_control1 .. "\n")
    file:write("input_control_pair=" .. input_control0 .. "," .. input_control1 .. "\n")
    file:write("decision_control_pair=" .. decision_control0 .. "," .. decision_control1 .. "\n")
    file:write("holder_proven=" .. tostring(holder_proven) .. "\n")
    file:write("shot_window_proven=" .. tostring(shot_window_proven) .. "\n")
    file:write("input_front=" .. input_front .. "\ninput_close_mode=" .. input_close_mode .. "\n")
    file:write("pre_score=" .. pre_score0 .. "-" .. pre_score1 .. "\nscore=" .. score(0) .. "-" .. score(1) .. "\n")
    file:write("live_snapshot=" .. string.format(
        "%02X,%02X,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
        rb(R.mode), rb(R.screen), rb(R.period), rb(R.offense_actor),
        rb(R.defense_actor), rb(R.offense_side), rb(R.defense_side),
        rb(R.control0), rb(R.control1), rb(R.orientation), rb(R.team0), rb(R.team1)))
    file:write("telemetry_rows=" .. telemetry_rows .. "\nevent_rows=" .. event_rows .. "\n")
    file:write("phase_rows=" .. phase_rows .. "\nshot_detail_rows=" .. detail_rows .. "\n")
    file:write("screenshot_count=" .. screenshot_count ..
        "\ntracked_text_bytes=" .. tracked_text_bytes .. "\n")
    file:write("final_pads_neutral=" .. tostring(final_pads_neutral) .. "\n")
    file:write("stop_reason=" .. stop_reason .. "\n")
end

local function write_status(state)
    local pass, timing_evidence = status_acceptance()
    local file = io.open(status_path, "w")
    if not file then return end
    write_status_overview(file, state, pass, timing_evidence)
    write_status_capture(file)
    write_status_context(file)
    file:close()
end

local function finalize_best_effort()
    local neutral_ok = pcall(function() apply_pads({}, {}) end)
    final_pads_neutral = neutral_ok and not active_p1.A and not active_p1.B and
        not active_p1.up and not active_p1.down and not active_p1.left and
        not active_p1.right and not active_p1.start and not active_p1.select and
        not active_p2.A and not active_p2.B and not active_p2.up and
        not active_p2.down and not active_p2.left and not active_p2.right and
        not active_p2.start and not active_p2.select
    pcall(flush_hooks)
    if not files_closed then
        pcall(function() telemetry:flush() end)
        pcall(function() events:flush() end)
        pcall(function() phases:flush() end)
        pcall(function() detail:flush() end)
        pcall(function() write_status("stopped") end)
        pcall(function() telemetry:close() end)
        pcall(function() events:close() end)
        pcall(function() phases:close() end)
        pcall(function() detail:close() end)
        files_closed = true
    end
    if movie_started and not movie_stopped then
        pcall(movie.stop)
        movie_stopped = true
    end
    pcall(emu.exit)
end

local function stop(reason)
    if stopped then return end
    stopped, stop_reason = true, reason
    finalize_best_effort()
end

local function fail(reason)
    result = "abort"
    save_screenshot("abort")
    stop(reason)
end

function TECMO_GAMEPLAY_LAB_EMERGENCY(uncaught)
    if stopped then return end
    stopped = true
    stop_reason = "uncaught Lua error: " .. uncaught
    result = "abort"
    finalize_best_effort()
end

if movie_enabled then
    local ok, returned = pcall(movie.record, movie_path, 0,
        "Tecmo NBA Basketball Rev1 tracked gameplay laboratory")
    movie_started = ok and returned ~= false
    if not movie_started then
        fail("requested FM2 recording could not start")
    end
end
if not stopped then
    set_phase("boot_menu", "power-on navigation")
    save_screenshot("start")
    write_status("running")
end

local function error_text(message) return tostring(message) end

while not stopped do
        local prefix_ok, prefix_failure = xpcall(function()
            frame = frame + 1
            apply_pads({}, {})
            last_action = "neutral"
            if deferred_failure ~= nil then fail(deferred_failure) end
            if rb(R.mode) == 0x7D and rb(R.screen) == 0xA0 then root_seen = true end
        end, error_text)
        if not prefix_ok then TECMO_GAMEPLAY_LAB_EMERGENCY(prefix_failure) end
        if stopped then break end

        local step_ok, step_failure = true, nil
        if phase == "boot_menu" then
        step_ok, step_failure = xpcall(function()
        for _, command in ipairs(map.boot_inputs) do
            local active = frame >= command.first and frame <= command.last
            if active and command.every then
                active = ((frame - command.first) % command.every) < command.width
            end
            if active and not (command.until_root and root_seen) then
                local p1, p2 = routed_pads(command.port, command.button)
                apply_pads(p1, p2)
                last_action = "boot_" .. command.button
            end
        end
        if rb(R.mode) == map.live.mode and rb(R.screen) == map.live.screen then
            gameplay_frame = frame
            if not live_common_valid() then fail("live setup invariant mismatch")
            else
                setup_control0, setup_control1 = rb(R.control0), rb(R.control1)
                set_phase("tip", "validated MAN VS MAN live mode")
                save_screenshot("live_setup")
            end
        elseif frame > 4600 then
            fail("authentic boot navigation deadline")
        end
        end, error_text)
    elseif phase == "tip" then
        step_ok, step_failure = xpcall(function()
        if not tip_base_valid() then
            fail("tip invariant mismatch")
        else
            local age = frame - gameplay_frame
            local clock_running = rb(R.minute) < 4 or rb(R.second) > 0 or rb(R.shot_clock) < 24
            if not clock_running then
                if age >= 30 and age <= 34 then
                    apply_pads({A = true}, {}); last_action = "tip_A"
                elseif age >= 35 and age <= 37 then
                    apply_pads({A = true, B = true}, {}); last_action = "tip_AB"
                elseif age >= 38 and age <= 55 then
                    apply_pads({B = true}, {}); last_action = "tip_B"
                elseif age > 300 then fail("tip deadline") end
            elseif rb(R.mode) == map.live.mode then
                set_phase("acquire", "clock running")
            end
        end
        end, error_text)
    elseif phase == "acquire" then
        step_ok, step_failure = xpcall(function()
        local shooter, ball, _, _, front, _, ball_distance = scan_world()
        if not live_base_valid() then
            fail("acquire scene/control invariant mismatch")
        elseif rb(R.offense_side) ~= map.supported_offense_side then
            fail("unsupported possession after tip")
        elseif pre_action_valid(shooter, ball, ball_distance) then
            stable_holder = stable_holder + 1
            if stable_holder >= 8 then
                holder_proven = true
                proven_holder_actor = shooter.index
                holder_seen[shooter.index] = true
                if front >= 0 then
                    reset_defender_cycle(front,
                        "front threat requires confirmed defensive cycle")
                else
                    best_distance = 99999
                    progress_deadline = frame + map.shot_window.progress_deadline
                    begin_movement_neutral("position_shooter",
                        "stable holder acquired")
                end
            end
        else
            stable_holder = 0
            if frame - phase_started > map.shot_window.acquire_deadline then
                fail("stable holder acquisition deadline")
            end
        end
        end, error_text)
    elseif phase == "select_defender" then
        step_ok, step_failure = xpcall(function()
        local shooter, ball, _, _, front, _, ball_distance = scan_world()
        if shooter.index ~= proven_holder_actor or
                not pre_action_valid(shooter, ball, ball_distance) or
                rb(R.offense_side) ~= map.supported_offense_side then
            fail("defender selection invariant mismatch")
        elseif front < 0 and switch_pending_actor < 0 and
                not switch_request_outstanding then
            best_distance = 99999
            progress_deadline = frame + map.shot_window.progress_deadline
            begin_movement_neutral("position_shooter",
                "threat cleared before selection")
        elseif switch_pending_actor >= 0 then
            if safe_frame() ~= switch_pending_frame + 1 or
                    switch_pending_actor > 9 or
                    rb(R.defense_actor) ~= switch_pending_actor then
                fail("defender switch store did not commit on next frame")
            else
                local confirmed = switch_pending_actor
                switch_pending_actor, switch_pending_frame = -1, -1
                switch_request_outstanding = false
                defender_store_total = defender_store_total + 1
                if not switch_seen[confirmed] then
                    switch_seen[confirmed] = true
                    switch_unique_count = switch_unique_count + 1
                end
                if confirmed ~= switch_origin then switch_saw_different = true end
                if defender_store_total > map.shot_window.defender_store_cap then
                    fail("pilot exceeded confirmed defensive-store cap")
                elseif front < 0 then
                    best_distance = 99999
                    progress_deadline =
                        frame + map.shot_window.progress_deadline
                    begin_movement_neutral("position_shooter",
                        "threat cleared after confirmed store")
                elseif front ~= selected_threat then
                    reset_defender_cycle(front,
                        "front threat identity changed after confirmed store")
                elseif confirmed == selected_threat then
                    best_distance = distance(shooter, actor(selected_threat))
                    progress_deadline = frame + map.shot_window.progress_deadline
                    begin_movement_neutral("move_defender", "threat selected by confirmed store")
                elseif switch_saw_different and confirmed == switch_origin then
                    defender_cycles_closed = defender_cycles_closed + 1
                    if not profile.allow_holder_passes then
                        fail("defensive cycle closed without selecting threat")
                    elseif defender_passes >= map.shot_window.holder_transfer_cap then
                        fail("holder transfer cap reached")
                    else
                        pass_origin_holder = rb(R.offense_actor)
                        pass_origin_side = rb(R.offense_side)
                        pass_score0, pass_score1 = score(0), score(1)
                        pass_pulsed = false
                        set_phase("pass_neutral",
                            "threat absent from closed defensive cycle")
                    end
                elseif defender_store_total >= map.shot_window.defender_store_cap then
                    fail("pilot reached confirmed defensive-store cap")
                end
            end
        elseif switch_request_outstanding then
            if frame > switch_request_deadline then
                fail("defensive A store confirmation deadline")
            else
                last_action = "await_defender_store"
            end
        elseif front ~= selected_threat then
            reset_defender_cycle(front, "front threat identity changed")
        elseif rb(R.defense_actor) == selected_threat then
            best_distance = distance(shooter, actor(selected_threat))
            progress_deadline = frame + map.shot_window.progress_deadline
            begin_movement_neutral("move_defender",
                "threat already selected")
        elseif defender_store_total >= map.shot_window.defender_store_cap then
            fail("pilot reached confirmed defensive-store cap")
        else
            local port = rb(R.defense_side) + 1
            if controller_readback(port) == 0 then
                local p1, p2 = routed_pads(port, "A")
                apply_pads(p1, p2)
                last_action = "defense_A_confirmed_request"
                switch_request_outstanding = true
                switch_request_deadline = frame + map.shot_window.progress_deadline
            else
                last_action = "defense_neutral_gate"
            end
        end
        end, error_text)
    elseif phase == "pass_neutral" then
        step_ok, step_failure = xpcall(function()
        local shooter, ball, _, _, _, _, ball_distance = scan_world()
        if not live_base_valid() or rb(R.offense_side) ~= pass_origin_side or
                rb(R.offense_actor) ~= pass_origin_holder or
                score(0) ~= pass_score0 or score(1) ~= pass_score1 or
                not pre_action_valid(shooter, ball, ball_distance) then
            fail("holder pass neutral gate lost possession/score/safe route")
        elseif controller_readback(1) == 0 and controller_readback(2) == 0 then
            set_phase("pass_pulse", "both controller readbacks neutral")
        else
            last_action = "pass_neutral_gate"
        end
        end, error_text)
    elseif phase == "pass_pulse" then
        step_ok, step_failure = xpcall(function()
        local shooter, ball, _, _, _, _, ball_distance = scan_world()
        if pass_pulsed then
            fail("offense pass pulse repeated")
        elseif not live_base_valid() or rb(R.offense_side) ~= pass_origin_side or
                rb(R.offense_actor) ~= pass_origin_holder or
                score(0) ~= pass_score0 or score(1) ~= pass_score1 or
                not pre_action_valid(shooter, ball, ball_distance) or
                controller_readback(rb(R.offense_side) + 1) ~= 0 then
            fail("holder pass pulse lost neutral safe context")
        else
            local p1, p2 = routed_pads(rb(R.offense_side) + 1, "A")
            apply_pads(p1, p2)
            last_action = "offense_A_single_pass"
            pass_pulsed = true
            pass_deadline = frame + map.shot_window.holder_transfer_deadline
            set_phase("pass_wait_holder", "single offense A pulse emitted")
        end
        end, error_text)
    elseif phase == "pass_wait_holder" then
        step_ok, step_failure = xpcall(function()
        local new_holder = rb(R.offense_actor)
        if not live_base_valid() or rb(R.offense_side) ~= pass_origin_side then
            fail("holder pass changed possession or live route")
        elseif score(0) ~= pass_score0 or score(1) ~= pass_score1 then
            fail("holder pass changed score")
        elseif rb(R.foul_route) ~= 0 or rb(R.violation_route) ~= 0 or
                rb(R.close_mode) ~= 0 then
            fail("holder pass entered unsupported route")
        elseif frame > pass_deadline then
            fail("holder pass transfer deadline")
        elseif new_holder ~= pass_origin_holder then
            if new_holder > 9 or holder_seen[new_holder] then
                fail("holder pass repeated or selected invalid holder")
            else
                defender_passes = defender_passes + 1
                holder_changes = holder_changes + 1
                holder_seen[new_holder] = true
                reacquire_actor = new_holder
                reacquire_stable = 0
                holder_proven = false
                proven_holder_actor = -1
                stable_holder = 0
                set_phase("reacquire_holder",
                    "different holder observed after one pass")
            end
        else
            last_action = "await_holder_transfer"
        end
        end, error_text)
    elseif phase == "reacquire_holder" then
        step_ok, step_failure = xpcall(function()
        local shooter, ball, _, _, front, _, ball_distance = scan_world()
        if not live_base_valid() or rb(R.offense_side) ~= pass_origin_side or
                rb(R.offense_actor) ~= reacquire_actor then
            fail("reacquire repeated holder or lost possession")
        elseif score(0) ~= pass_score0 or score(1) ~= pass_score1 then
            fail("reacquire changed score")
        elseif rb(R.foul_route) ~= 0 or rb(R.violation_route) ~= 0 or
                rb(R.close_mode) ~= 0 then
            fail("reacquire entered unsupported route")
        elseif frame > pass_deadline then
            fail("reacquire stable-holder deadline")
        elseif pre_action_valid(shooter, ball, ball_distance) then
            reacquire_stable = reacquire_stable + 1
            if reacquire_stable >= 8 then
                holder_proven = true
                proven_holder_actor = shooter.index
                if front >= 0 then
                    reset_defender_cycle(front,
                        "reacquired holder has front threat")
                else
                    best_distance = 99999
                    progress_deadline = frame + map.shot_window.progress_deadline
                    begin_movement_neutral("position_shooter",
                        "reacquired holder proven for eight frames")
                end
            end
        else
            reacquire_stable = 0
            last_action = "await_stable_reacquired_holder"
        end
        end, error_text)
    elseif phase == "movement_neutral" then
        step_ok, step_failure = xpcall(function()
        local shooter, ball, _, _, _, _, ball_distance = scan_world()
        if shooter.index ~= proven_holder_actor or
                not movement_pre_action_valid(shooter, ball, ball_distance,
                    held_direction) or
                rb(R.offense_side) ~= map.supported_offense_side then
            fail("movement neutral gate lost live control")
        elseif controller_readback(1) == 0 and controller_readback(2) == 0 and
                shooter.state == 0 then
            local next_phase, reason = movement_next_phase, movement_next_reason
            movement_next_phase, movement_next_reason = nil, nil
            held_direction = nil
            held_direction_port = -1
            held_direction_actor = -1
            pending_direction = nil
            if next_phase == "restart_defender_cycle" then
                local _, _, _, _, front = scan_world()
                if front >= 0 then
                    reset_defender_cycle(front, reason)
                else
                    best_distance = 99999
                    progress_deadline =
                        frame + map.shot_window.progress_deadline
                    set_phase("position_shooter",
                        "front threat cleared during neutral gate")
                end
            else
                set_phase(next_phase, reason)
            end
        else
            last_action = "movement_neutral_gate"
        end
        end, error_text)
    elseif phase == "move_defender" then
        step_ok, step_failure = xpcall(function()
        local shooter, ball, _, _, front, _, ball_distance = scan_world()
        local defender = actor(selected_threat)
        local d, dx, dy = distance(shooter, defender)
        if shooter.index ~= proven_holder_actor or
                not pre_action_valid(shooter, ball, ball_distance) or
                rb(R.defense_actor) ~= selected_threat then
            fail("controlled defender changed during clearance")
        elseif front < 0 then
            best_distance = 99999
            progress_deadline = frame + map.shot_window.progress_deadline
            begin_movement_neutral("position_shooter", "front threat cleared")
        elseif front ~= selected_threat then
            begin_movement_neutral("restart_defender_cycle",
                "front threat identity changed during clearance")
        elseif frame > progress_deadline then
            fail("no defensive coordinate progress")
        else
            if d > best_distance + 0.5 then
                best_distance = d
                progress_deadline = frame + map.shot_window.progress_deadline
            end
            local button
            if math.abs(dx) >= math.abs(dy) then
                button = dx >= 0 and "left" or "right"
            else
                button = dy >= 0 and "up" or "down"
            end
            if not movement_state_valid(defender, held_direction or button) then
                fail("defender entered non-direction-correlated movement state")
            elseif pending_direction ~= nil then
                if controller_readback(rb(R.defense_side) + 1) == 0 and
                        defender.state == 0 then
                    held_direction = nil
                    held_direction_port = -1
                    held_direction_actor = -1
                    pending_direction = nil
                end
                last_action = "defense_direction_neutral_gate"
            elseif held_direction ~= nil and held_direction ~= button then
                pending_direction = button
                last_action = "defense_direction_change_neutral"
            elseif held_direction == nil and controller_readback(rb(R.defense_side) + 1) ~= 0 then
                last_action = "defense_direction_neutral_gate"
            else
                held_direction = button
                held_direction_port = rb(R.defense_side) + 1
                held_direction_actor = selected_threat
                local p1, p2 = routed_pads(rb(R.defense_side) + 1, button)
                apply_pads(p1, p2)
                last_action = "defense_away_held_" .. button
            end
        end
        end, error_text)
    elseif phase == "position_shooter" then
        step_ok, step_failure = xpcall(function()
        local shooter, ball, _, _, front, _, ball_distance = scan_world()
        local dx = 0
        if shooter.x < map.shot_window.x_min then dx = map.shot_window.x_min - shooter.x
        elseif shooter.x > map.shot_window.x_max then dx = map.shot_window.x_max - shooter.x end
        local dy = 0
        if shooter.y < map.shot_window.y_min then dy = map.shot_window.y_min - shooter.y
        elseif shooter.y > map.shot_window.y_max then dy = map.shot_window.y_max - shooter.y end
        local metric = math.abs(dx) + math.abs(dy)
        local button = nil
        if metric > 0 then
            if math.abs(dx) >= math.abs(dy) then button = dx > 0 and "right" or "left"
            else button = dy > 0 and "down" or "up" end
        end
        if shooter.index ~= proven_holder_actor or
                not movement_pre_action_valid(shooter, ball, ball_distance,
                held_direction or button) then
            fail("positioning lost holder or entered unsupported movement state")
        elseif front >= 0 then
            begin_movement_neutral("restart_defender_cycle",
                "front threat entered safety window")
        elseif frame - phase_started > map.shot_window.position_deadline then
            fail("shooter positioning deadline")
        elseif metric == 0 then
            stable_safe = 0
            begin_movement_neutral("stable_safe",
                "shooter entered proven coordinate window")
        elseif frame > progress_deadline then
            fail("no shooter coordinate progress")
        else
            if metric < best_distance then
                best_distance = metric
                progress_deadline = frame + map.shot_window.progress_deadline
            end
            if pending_direction ~= nil then
                if controller_readback(rb(R.offense_side) + 1) == 0 and
                        shooter.state == 0 then
                    held_direction = nil
                    held_direction_port = -1
                    held_direction_actor = -1
                    pending_direction = nil
                end
                last_action = "offense_direction_neutral_gate"
            elseif held_direction ~= nil and held_direction ~= button then
                pending_direction = button
                last_action = "offense_direction_change_neutral"
            elseif held_direction == nil and
                    controller_readback(rb(R.offense_side) + 1) ~= 0 then
                last_action = "offense_direction_neutral_gate"
            else
                held_direction = button
                held_direction_port = rb(R.offense_side) + 1
                held_direction_actor = shooter.index
                local p1, p2 = routed_pads(rb(R.offense_side) + 1, button)
                apply_pads(p1, p2)
                last_action = "offense_position_held_" .. button
            end
        end
        end, error_text)
    elseif phase == "stable_safe" then
        step_ok, step_failure = xpcall(function()
        local shooter, ball, _, _, front, _, ball_distance = scan_world()
        local in_window = shooter.x >= map.shot_window.x_min and
            shooter.x <= map.shot_window.x_max and shooter.y >= map.shot_window.y_min and
            shooter.y <= map.shot_window.y_max
        if shooter.index ~= proven_holder_actor or
                not pre_action_valid(shooter, ball, ball_distance) or not in_window then
            fail("stable-safe invariant changed")
        elseif front >= 0 then
            reset_defender_cycle(front,
                "front threat interrupted stable window")
        else
            stable_safe = stable_safe + 1
            if stable_safe >= map.shot_window.stable_frames then
                shot_window_proven = true
                shot_side, shot_actor = rb(R.offense_side), rb(R.offense_actor)
                possession_at_input = rb(R.offense_side)
                input_control0, input_control1 = rb(R.control0), rb(R.control1)
                input_front, input_close_mode = front, rb(R.close_mode)
                pre_score0, pre_score1 = score(0), score(1)
                shot_frame = frame
                set_phase("shot_hold", "12 neutral safe frames proven")
                local p1, p2 = routed_pads(rb(R.offense_side) + 1, "B")
                apply_pads(p1, p2)
                last_action = "offense_B_1"
                save_screenshot("shot_press")
            end
        end
        end, error_text)
    elseif phase == "shot_hold" then
        step_ok, step_failure = xpcall(function()
        local age = frame - shot_frame + 1
        if not live_base_valid() or rb(R.offense_side) ~= possession_at_input or
                rb(R.offense_actor) ~= shot_actor or
                rb(R.close_mode) ~= 0 or rb(R.foul_route) ~= 0 or rb(R.violation_route) ~= 0 then
            fail("shot hold entered close/foul/violation/control mismatch")
        elseif age <= map.shot_window.hold_b_frames then
            local p1, p2 = routed_pads(rb(R.offense_side) + 1, "B")
            apply_pads(p1, p2)
            last_action = "offense_B_" .. age
        else
            apply_pads({}, {})
            last_action = "offense_B_release_9"
            set_phase("resolve", "captured release frame 9")
            save_screenshot("shot_release")
        end
        end, error_text)
    elseif phase == "resolve" then
        step_ok, step_failure = xpcall(function()
        if rb(R.control0) ~= 0 or rb(R.control1) ~= 0 or
                rb(R.mode) ~= map.live.mode or rb(R.screen) ~= map.live.screen then
            fail("resolve scene/control mismatch")
        elseif rb(R.offense_side) ~= possession_at_input and terminal_count == 0 then
            fail("possession changed before anchored terminal")
        elseif rb(R.close_mode) ~= 0 or close_launch_seen then
            fail("ordinary shot became a close route")
        elseif terminal_count > 1 then
            fail("multiple anchored terminal helpers")
        elseif frame - shot_frame > map.shot_window.resolve_deadline then
            fail("shot resolution deadline")
        elseif terminal_count == 1 then
            if not release_seen or not classifier_seen or not result_seen or
                    terminal_frame ~= decision_frame then
                fail("terminal lacked release/classifier/result/same-frame anchor evidence")
            elseif profile.require_point_evidence and
                    (not point_classifier_seen or not point_return_seen or
                     classified_point_value ~= profile.expected_point_value) then
                fail("point-classifier evidence mismatch")
            elseif profile.expected_make and not make_seen then
                fail("profile outcome mismatch")
            else
                if tail_started < 0 then tail_started = frame end
                local score0_delta, score1_delta = score(0) - pre_score0, score(1) - pre_score1
                local settled = false
                if make_seen then
                    if profile.require_point_evidence then
                        settled = score_apply_seen and
                            score0_delta == profile.expected_score_delta and
                            score1_delta == 0 and
                            (handoff_seen or rb(R.offense_side) ~= possession_at_input) and
                            two_point_timing_evidence_valid()
                    else
                        settled = score_apply_seen and
                            (score0_delta == 2 or score0_delta == 3) and
                            score1_delta == 0 and
                            (handoff_seen or rb(R.offense_side) ~= possession_at_input)
                    end
                else
                    settled = score0_delta == 0 and score1_delta == 0 and settlement_seen
                end
                if settled and frame - tail_started >= map.shot_window.tail_frames then
                    save_screenshot("final")
                    stop("tracked ordinary shot complete")
                end
            end
        end
        end, error_text)
        end

        if not step_ok then
            TECMO_GAMEPLAY_LAB_EMERGENCY(step_failure)
        end

        if stopped then break end
        local tail_ok, tail_failure = xpcall(function()
            flush_hooks()
            write_telemetry()
            write_detail()
            if frame % 60 == 0 then
                telemetry:flush(); events:flush(); phases:flush(); detail:flush()
                write_status("running")
            end
            if frame >= max_frames then fail("bounded frame limit") end
        end, error_text)
        if not tail_ok then TECMO_GAMEPLAY_LAB_EMERGENCY(tail_failure) end
        if not stopped then FCEU.frameadvance() end
end
