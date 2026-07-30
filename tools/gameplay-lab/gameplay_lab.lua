-- Read-only, fail-closed Rev 1 gameplay laboratory.
-- The only emulated inputs are complete two-port joypad tables.

local output_root = assert(os.getenv("TECMO_GAMEPLAY_LAB_OUTPUT"),
    "TECMO_GAMEPLAY_LAB_OUTPUT is required")
local map_path = assert(os.getenv("TECMO_GAMEPLAY_LAB_MAP"),
    "TECMO_GAMEPLAY_LAB_MAP is required")
output_root = string.gsub(output_root, "\\", "/")
map_path = string.gsub(map_path, "\\", "/")
local map = assert(dofile(map_path))
assert(map.schema == "TGLM-2" and map.schema_version == 2,
    "unsupported gameplay-lab map")

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

emit(metadata, "schema=TGLAB-2\nschema_version=2\nmap_schema=" .. map.schema .. "\n")
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
    "lab_frame,emu_frame,name,address,raw_bank,pc,a,x,y,ba,offense_side," ..
    "control0,control1,score0,score1,miss_selector_6a,miss_selector_low2," ..
    "miss_selected_target,object_slot10_state_0478," ..
    "object_slot10_horizontal_velocity_04f1_04fc," ..
    "object_slot10_vertical_velocity_0507_0512," ..
    "saved_object_horizontal_velocity_038d_038e," ..
    "saved_object_vertical_velocity_038f_0390\n")
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
local settlement_seen, handoff_seen, close_launch_seen = false, false, false
local setup_control0, setup_control1 = -1, -1
local input_control0, input_control1 = -1, -1
local decision_control0, decision_control1 = -1, -1
local pre_score0, pre_score1 = -1, -1
local shot_side, shot_actor, shot_frame = -1, -1, -1
local possession_at_input = -1
local holder_proven, shot_window_proven = false, false
local input_front, input_close_mode = -2, -1
local stable_holder = 0
local stable_safe = 0
local selected_threat = -1
local switch_origin = -1
local switch_pulse = false
local switch_attempts = 0
local move_pulse = false
local progress_deadline = 0
local best_distance = 99999
local gameplay_frame = -1
local root_seen = false
local tail_started = -1

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
            hook_queue[#hook_queue + 1] = {
                name = hook.name, address = hook.address, bank = bank,
                emu_frame = safe_frame(), pc = safe_register("pc"),
                a = safe_register("a"), x = safe_register("x"), y = safe_register("y"),
                shot_flags = rb(R.shot_flags),
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
                    R.saved_object_vertical_velocity_hi)
            }
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
            "%d,%d,%s,%04X,%02X,%04X,%02X,%02X,%02X,%02X,%d,%d,%d,%d,%d," ..
            "%02X,%d,%04X,%02X,%04X,%04X,%04X,%04X\n",
            frame, h.emu_frame, h.name, h.address, h.bank, h.pc, h.a, h.x, h.y,
            h.shot_flags, rb(R.offense_side), rb(R.control0), rb(R.control1),
            score(0), score(1), h.miss_selector, h.miss_selector_low2,
            h.miss_selected_target, h.object_slot10_state,
            h.object_slot10_horizontal_velocity, h.object_slot10_vertical_velocity,
            h.saved_object_horizontal_velocity, h.saved_object_vertical_velocity))
        if h.name == "ball_release" then
            release_seen = true
            save_screenshot("ball_release")
        elseif h.name == "shot_classifier" and shot_frame >= 0 then
            classifier_seen = true
        elseif h.name == "shot_result" then
            result_seen = true
        elseif h.name == "decision_anchor" then
            decision_frame = h.emu_frame
            decision_control0, decision_control1 = rb(R.control0), rb(R.control1)
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
        elseif h.name == "possession_handoff" and shot_frame >= 0 then
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

local function write_status(state)
    local pass = false
    local score0_delta = pre_score0 < 0 and 0 or score(0) - pre_score0
    local score1_delta = pre_score1 < 0 and 0 or score(1) - pre_score1
    if terminal_count == 1 and release_seen and classifier_seen and result_seen and
            setup_control0 == 0 and setup_control1 == 0 and
            input_control0 == 0 and input_control1 == 0 and
            decision_control0 == 0 and decision_control1 == 0 and
            shot_side == 0 and rb(R.team0) ~= rb(R.team1) and
            holder_proven and shot_window_proven and input_front == -1 and
            input_close_mode == 0 and not close_launch_seen and final_pads_neutral then
        if make_seen then
            pass = score_apply_seen and (score0_delta == 2 or score0_delta == 3) and
                score1_delta == 0 and (handoff_seen or rb(R.offense_side) ~= possession_at_input)
        elseif miss_seen then
            pass = score0_delta == 0 and score1_delta == 0 and settlement_seen
        end
    end
    local file = io.open(status_path, "w")
    if not file then return end
    file:write("schema=TGLAB-2\nschema_version=2\nmap_schema=" .. map.schema .. "\n")
    file:write("script_sha256=" .. script_hash .. "\nmap_sha256=" .. map_hash .. "\n")
    file:write("rom_sha256=" .. rom_hash .. "\nfceux_sha256=" .. fceux_hash .. "\n")
    file:write("state=" .. state .. "\nlab_frame=" .. frame .. "\nemu_frame=" .. safe_frame() .. "\n")
    file:write("phase=" .. phase .. "\nlast_action=" .. last_action .. "\n")
    file:write("result=" .. result .. "\npilot_pass=" .. tostring(pass) .. "\n")
    file:write("terminal_count=" .. terminal_count .. "\nrelease_seen=" .. tostring(release_seen) .. "\n")
    file:write("classifier_seen=" .. tostring(classifier_seen) .. "\n")
    file:write("result_seen=" .. tostring(result_seen) .. "\nscore_apply_seen=" .. tostring(score_apply_seen) .. "\n")
    file:write("settlement_seen=" .. tostring(settlement_seen) .. "\nhandoff_seen=" .. tostring(handoff_seen) .. "\n")
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
                if front >= 0 then
                    selected_threat = front
                    switch_attempts = 0
                    set_phase("select_defender", "front threat requires controlled clearance")
                else
                    best_distance = 99999
                    progress_deadline = frame + map.shot_window.progress_deadline
                    set_phase("position_shooter", "stable holder acquired")
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
        local shooter, _, _, _, front = scan_world()
        if not live_base_valid() or rb(R.offense_side) ~= map.supported_offense_side then
            fail("defender selection invariant mismatch")
        elseif front < 0 then
            best_distance = 99999
            progress_deadline = frame + map.shot_window.progress_deadline
            set_phase("position_shooter", "threat cleared before selection")
        elseif rb(R.defense_actor) == selected_threat then
            best_distance = distance(shooter, actor(selected_threat))
            progress_deadline = frame + map.shot_window.progress_deadline
            move_pulse = false
            set_phase("move_defender", "threat selected")
        elseif switch_attempts >= 10 then
            fail("defensive A-cycle could not select threat")
        elseif not switch_pulse then
            switch_origin = rb(R.defense_actor)
            local port = rb(R.defense_side) + 1
            local p1, p2 = routed_pads(port, "A")
            apply_pads(p1, p2)
            last_action = "defense_A_edge"
            switch_pulse = true
            switch_attempts = switch_attempts + 1
        else
            switch_pulse = false
            last_action = "observe_defense_selection"
            if rb(R.defense_actor) == switch_origin then
                -- One neutral observation frame is allowed; the next loop
                -- emits another edge rather than assuming Bank06's cycle.
            end
        end
        end, error_text)
    elseif phase == "move_defender" then
        step_ok, step_failure = xpcall(function()
        local shooter, _, _, _, front = scan_world()
        local defender = actor(selected_threat)
        local d, dx, dy = distance(shooter, defender)
        if not live_base_valid() or rb(R.defense_actor) ~= selected_threat then
            fail("controlled defender changed during clearance")
        elseif front < 0 then
            best_distance = 99999
            progress_deadline = frame + map.shot_window.progress_deadline
            set_phase("position_shooter", "front threat cleared")
        elseif frame > progress_deadline then
            fail("no defensive coordinate progress")
        else
            if d > best_distance + 0.5 then
                best_distance = d
                progress_deadline = frame + map.shot_window.progress_deadline
            end
            if not move_pulse then
                local button
                if math.abs(dx) >= math.abs(dy) then
                    button = dx >= 0 and "left" or "right"
                else
                    button = dy >= 0 and "up" or "down"
                end
                local p1, p2 = routed_pads(rb(R.defense_side) + 1, button)
                apply_pads(p1, p2)
                last_action = "defense_away_" .. button
                move_pulse = true
            else
                move_pulse = false
                last_action = "defense_neutral_pulse"
            end
        end
        end, error_text)
    elseif phase == "position_shooter" then
        step_ok, step_failure = xpcall(function()
        local shooter, ball, _, _, front, _, ball_distance = scan_world()
        if not pre_action_valid(shooter, ball, ball_distance) then
            fail("positioning lost holder or entered unsupported action")
        elseif front >= 0 then
            selected_threat = front
            switch_attempts, switch_pulse = 0, false
            set_phase("select_defender", "front threat entered safety window")
        elseif frame - phase_started > map.shot_window.position_deadline then
            fail("shooter positioning deadline")
        else
            local dx = 0
            if shooter.x < map.shot_window.x_min then dx = map.shot_window.x_min - shooter.x
            elseif shooter.x > map.shot_window.x_max then dx = map.shot_window.x_max - shooter.x end
            local dy = 0
            if shooter.y < map.shot_window.y_min then dy = map.shot_window.y_min - shooter.y
            elseif shooter.y > map.shot_window.y_max then dy = map.shot_window.y_max - shooter.y end
            local metric = math.abs(dx) + math.abs(dy)
            if metric == 0 then
                stable_safe = 0
                set_phase("stable_safe", "shooter entered proven coordinate window")
            elseif frame > progress_deadline then
                fail("no shooter coordinate progress")
            else
                if metric < best_distance then
                    best_distance = metric
                    progress_deadline = frame + map.shot_window.progress_deadline
                end
                if not move_pulse then
                    local button
                    if math.abs(dx) >= math.abs(dy) then button = dx > 0 and "right" or "left"
                    else button = dy > 0 and "down" or "up" end
                    local p1, p2 = routed_pads(rb(R.offense_side) + 1, button)
                    apply_pads(p1, p2)
                    last_action = "offense_position_" .. button
                    move_pulse = true
                else
                    move_pulse = false
                    last_action = "offense_neutral_pulse"
                end
            end
        end
        end, error_text)
    elseif phase == "stable_safe" then
        step_ok, step_failure = xpcall(function()
        local shooter, ball, _, _, front, _, ball_distance = scan_world()
        local in_window = shooter.x >= map.shot_window.x_min and
            shooter.x <= map.shot_window.x_max and shooter.y >= map.shot_window.y_min and
            shooter.y <= map.shot_window.y_max
        if not pre_action_valid(shooter, ball, ball_distance) or not in_window then
            fail("stable-safe invariant changed")
        elseif front >= 0 then
            selected_threat = front
            switch_attempts, switch_pulse = 0, false
            set_phase("select_defender", "front threat interrupted stable window")
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
            else
                if tail_started < 0 then tail_started = frame end
                local score0_delta, score1_delta = score(0) - pre_score0, score(1) - pre_score1
                local settled = false
                if make_seen then
                    settled = score_apply_seen and (score0_delta == 2 or score0_delta == 3) and
                        score1_delta == 0 and (handoff_seen or rb(R.offense_side) ~= possession_at_input)
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
