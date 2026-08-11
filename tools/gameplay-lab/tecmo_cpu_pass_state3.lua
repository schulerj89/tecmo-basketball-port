-- Read-only original-ROM trace for the first CPU-controlled slot-10 state-3
-- transition. This script supplies complete joypads only; it never writes
-- emulated RAM, loads a state, enables cheats, or produces an FM2.

local output_root = assert(os.getenv("TECMO_CPU_PASS_STATE3_OUTPUT"),
    "TECMO_CPU_PASS_STATE3_OUTPUT is required")
local map_path = assert(os.getenv("TECMO_CPU_PASS_STATE3_MAP"),
    "TECMO_CPU_PASS_STATE3_MAP is required")
output_root = string.gsub(output_root, "\\\\", "/")
map_path = string.gsub(map_path, "\\\\", "/")
local M = assert(dofile(map_path))
assert(M.schema == "TGLPASS3-1" and M.schema_version == 1,
    "unsupported CPU pass-state-3 map")
assert(os.getenv("TECMO_CPU_PASS_STATE3_ROM_SHA256") == M.rom_sha256,
    "runner ROM fingerprint mismatch")
assert(os.getenv("TECMO_CPU_PASS_STATE3_FCEUX_SHA256") == M.fceux_sha256,
    "runner FCEUX fingerprint mismatch")

local R = M.ram
local S = {
    frame = 0,
    phase = "boot",
    stopped = false,
    stop_reason = "running",
    deferred_failure = nil,
    setup_seen = false,
    setup_frame = -1,
    tip_started = false,
    tip_start_frame = -1,
    clock_stopped_seen = false,
    clock_running_seen = false,
    live_seen = false,
    live_start_frame = -1,
    cpu_possession_seen = false,
    cpu_possession_frames = 0,
    cpu_possession_first_frame = -1,
    non_cpu_state3_writes = 0,
    write_events = 0,
    event_rows = 0,
    actor_rows = 0,
    screenshot_count = 0,
    tracked_text_bytes = 0,
    progress_written = false,
    speedmode_ok = false,
    final_pads_neutral = false,
    active_p1 = {},
    active_p2 = {},
    last_slot10_state = 0,
    mapper = { select = 0, regs = {0,0,0,0,0,0,0,0}, known = {false,false,false,false,false,false,false,false} },
    ring = {},
    chain = {
        captured = false,
        source_frame = -1,
        source_event = -1,
        source_prior = -1,
        source_new = -1,
        writer_pc = -1,
        writer_raw_bank = -1,
        writer_control = -1,
        pre_sequence = {},
        post_sequence = {},
        dispatch = -1,
        b074 = -1,
        b074_candidate = -1,
        b074_receiver = -1,
        b1e7 = -1,
        b500 = -1,
        b228 = -1,
        b24f = -1,
        b24f_prior_holder = -1,
        b24f_expected_holder = -1,
        holder_changed = false,
        holder_change_frame = -1,
        alternate_count = 0,
        alternates = {},
        contact_anchor_seen = false,
        nonzero_route_seen = false,
        pass_route_confirmed = false
    }
}

local function path_join(a, b)
    if string.sub(a, -1) == "/" then return a .. b end
    return a .. "/" .. b
end

local trace = assert(io.open(path_join(output_root, "events.csv"), "w"))
local actors = assert(io.open(path_join(output_root, "actors.csv"), "w"))
local status_path = path_join(output_root, "status.txt")
local progress_path = path_join(output_root, "progress.txt")
local sequence_path = path_join(output_root, "sequence.txt")

local function rb(address)
    local value = memory.readbyte(address)
    if value == nil then return 0 end
    return value
end

local function word(lo, hi)
    return rb(lo) + 256 * rb(hi)
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

local function emit(file, value)
    S.tracked_text_bytes = S.tracked_text_bytes + string.len(value)
    if S.tracked_text_bytes > M.caps.tracked_text_bytes then
        S.deferred_failure = "CPU pass-state-3 tracked-text byte cap exceeded"
        return false
    end
    file:write(value)
    return true
end

local function hex_or_na(value, width)
    if value == nil or value < 0 then return "NA" end
    return string.format("%0" .. width .. "X", value)
end

local blank_pad = {
    A = false, B = false, up = false, down = false,
    left = false, right = false, start = false, select = false
}

local function complete_pad(source)
    return {
        A = source.A == true, B = source.B == true,
        up = source.up == true, down = source.down == true,
        left = source.left == true, right = source.right == true,
        start = source.start == true, select = source.select == true
    }
end

local function apply_pads(p1, p2)
    S.active_p1 = complete_pad(p1 or blank_pad)
    S.active_p2 = complete_pad(p2 or blank_pad)
    joypad.set(1, S.active_p1)
    joypad.set(2, S.active_p2)
end

local function pads_neutral()
    local all = { S.active_p1, S.active_p2 }
    for _, pad in ipairs(all) do
        if pad.A or pad.B or pad.up or pad.down or pad.left or pad.right or
                pad.start or pad.select then return false end
    end
    return true
end

local function routed_pads(port, buttons)
    local p1, p2 = {}, {}
    local target = port == 1 and p1 or p2
    for _, button in ipairs(buttons) do target[button] = true end
    return p1, p2
end

local function mapper_register(number)
    return S.mapper.regs[number + 1] or 0
end

local function mapper_known(number)
    return S.mapper.known[number + 1] == true
end

local function mapped_raw_bank(address)
    if address >= 0x8000 and address <= 0x9FFF then
        if AND(S.mapper.select, 0x40) ~= 0 then return 0x0E end
        if not mapper_known(6) then return nil end
        return AND(mapper_register(6), 0x0F)
    elseif address >= 0xA000 and address <= 0xBFFF then
        if not mapper_known(7) then return nil end
        return AND(mapper_register(7), 0x0F)
    elseif address >= 0xC000 and address <= 0xDFFF then
        if AND(S.mapper.select, 0x40) == 0 then return 0x0E end
        if not mapper_known(6) then return nil end
        return AND(mapper_register(6), 0x0F)
    elseif address >= 0xE000 and address <= 0xFFFF then
        return 0x0F
    end
    return nil
end

memory.registerwrite(M.mapper.select, function(_, _, value)
    S.mapper.select = value or 0
end)
memory.registerwrite(M.mapper.data, function(_, _, value)
    local number = AND(S.mapper.select, 0x07)
    local value8 = value or 0
    if number == 6 or number == 7 then value8 = AND(value8, 0x0F) end
    S.mapper.regs[number + 1] = value8
    S.mapper.known[number + 1] = true
end)

local function actor_valid(index)
    return index ~= nil and index >= 0 and index <= 9
end

local function actor_values(index)
    if index == nil or index < 0 or index > 10 then
        return { x = -1, y = -1, state = -1, altitude = -1,
            altitude_velocity = -1, horizontal_velocity = -1, vertical_velocity = -1 }
    end
    return {
        x = word(R.actor_x_lo + index, R.actor_x_hi + index),
        y = rb(R.actor_y + index),
        state = rb(R.actor_state_046e + index),
        altitude = word(R.actor_altitude_lo + index, R.actor_altitude_hi + index),
        altitude_velocity = word(R.actor_altitude_velocity_lo + index,
            R.actor_altitude_velocity_hi + index),
        horizontal_velocity = word(R.actor_horizontal_velocity_lo + index,
            R.actor_horizontal_velocity_hi + index),
        vertical_velocity = word(R.actor_vertical_velocity_lo + index,
            R.actor_vertical_velocity_hi + index)
    }
end

local function snapshot()
    local side = rb(R.offense_side)
    local defense_side = rb(R.defense_side)
    local offense = rb(R.offense_actor)
    local defense = rb(R.defense_actor)
    local candidate = side <= 1 and rb(R.candidate_037f + side) or -1
    local side_holder = side <= 1 and rb(R.side_holder_0e + side) or -1
    local primary = actor_values(offense)
    local defender = actor_values(defense)
    local candidate_actor = actor_values(candidate)
    local ball = actor_values(10)
    local control = side <= 1 and rb(R.control0 + side) or -1
    return {
        offense = offense, defense = defense, side = side, defense_side = defense_side,
        menu_selection = rb(R.menu_selection), menu_phase = rb(R.menu_phase),
        menu_option_28 = rb(R.menu_option_28), menu_option_29 = rb(R.menu_option_29),
        control = control, candidate = candidate, side_holder = side_holder,
        primary = primary, defender = defender, candidate_actor = candidate_actor,
        ball = ball, slot10_state = rb(R.object_slot10_state),
        saved_horizontal_velocity = word(R.saved_object_horizontal_velocity_lo,
            R.saved_object_horizontal_velocity_hi),
        saved_vertical_velocity = word(R.saved_object_vertical_velocity_lo,
            R.saved_object_vertical_velocity_hi),
        target_x = word(R.flight_target_x_lo, R.flight_target_x_hi),
        target_y = word(R.flight_target_y_lo, R.flight_target_y_hi),
        slot10_count = word(R.object_slot10_count_lo, R.object_slot10_count_hi),
        foul_route = rb(R.foul_route), violation_route = rb(R.violation_route),
        shot_clock = rb(R.shot_clock)
    }
end

local function compact_event(kind, name, address, pc, raw_bank, prior, new)
    return string.format("%s:%s@%d/%04X/%04X/%s/%s/%s>%s",
        kind, name, S.frame, address or 0, pc or 0, hex_or_na(raw_bank, 2),
        hex_or_na(mapped_raw_bank(pc or 0), 2), hex_or_na(prior, 2), hex_or_na(new, 2))
end

local function push_ring(value)
    table.insert(S.ring, value)
    if #S.ring > M.caps.ring_events then table.remove(S.ring, 1) end
    if S.chain.captured and #S.chain.post_sequence < M.caps.sequence_events then
        table.insert(S.chain.post_sequence, value)
    end
end

local trace_header = "event_index,emu_frame,frame,phase,kind,name,address,callback_pc,callback_raw_bank,mode,screen,period,menu_selection,menu_phase,menu_option_28,menu_option_29,mapper_select,mapper_r6,mapper_r7,prior_state,new_state,slot10_state,offense_actor,defense_actor,offense_side,defense_side,offense_control,candidate_037f,side_holder_0e,primary_x,primary_y,defender_x,defender_y,candidate_x,candidate_y,ball_x,ball_y,ball_state,ball_altitude,ball_altitude_velocity,ball_horizontal_velocity,ball_vertical_velocity,saved_horizontal_velocity,saved_vertical_velocity,target_x,target_y,slot10_count,shot_clock,foul_route,violation_route,address_confidence,label_confidence\n"
emit(trace, trace_header)
emit(actors, "event_index,stage,emu_frame,actor,x,y,state_046e,altitude,altitude_velocity,horizontal_velocity,vertical_velocity\n")

local function record_event(kind, name, address, raw_bank, prior, new, hook)
    if S.event_rows >= M.caps.trace_rows then
        S.deferred_failure = "CPU pass-state-3 trace-row cap exceeded"
        return -1
    end
    local p = safe_register("pc")
    local h = hook or { address_confidence = "exact_source_pinned", label_confidence = "exact_mechanics" }
    local q = snapshot()
    S.event_rows = S.event_rows + 1
    local index = S.event_rows
    local row = {
        tostring(index), tostring(safe_frame()), tostring(S.frame), S.phase, kind, name,
        hex_or_na(address, 4), hex_or_na(p, 4), hex_or_na(raw_bank, 2),
        hex_or_na(rb(R.mode), 2), hex_or_na(rb(R.screen), 2), hex_or_na(rb(R.period), 2),
        hex_or_na(q.menu_selection, 2), hex_or_na(q.menu_phase, 2),
        hex_or_na(q.menu_option_28, 2), hex_or_na(q.menu_option_29, 2),
        hex_or_na(S.mapper.select, 2), hex_or_na(mapper_register(6), 2), hex_or_na(mapper_register(7), 2),
        hex_or_na(prior, 2), hex_or_na(new, 2), hex_or_na(q.slot10_state, 2),
        hex_or_na(q.offense, 2), hex_or_na(q.defense, 2), hex_or_na(q.side, 2),
        hex_or_na(q.defense_side, 2), hex_or_na(q.control, 2), hex_or_na(q.candidate, 2),
        hex_or_na(q.side_holder, 2), hex_or_na(q.primary.x, 4), hex_or_na(q.primary.y, 2),
        hex_or_na(q.defender.x, 4), hex_or_na(q.defender.y, 2),
        hex_or_na(q.candidate_actor.x, 4), hex_or_na(q.candidate_actor.y, 2),
        hex_or_na(q.ball.x, 4), hex_or_na(q.ball.y, 2), hex_or_na(q.ball.state, 2),
        hex_or_na(q.ball.altitude, 4), hex_or_na(q.ball.altitude_velocity, 4),
        hex_or_na(q.ball.horizontal_velocity, 4), hex_or_na(q.ball.vertical_velocity, 4),
        hex_or_na(q.saved_horizontal_velocity, 4), hex_or_na(q.saved_vertical_velocity, 4),
        hex_or_na(q.target_x, 4), hex_or_na(q.target_y, 4), hex_or_na(q.slot10_count, 4),
        hex_or_na(q.shot_clock, 2), hex_or_na(q.foul_route, 2), hex_or_na(q.violation_route, 2),
        h.address_confidence, h.label_confidence
    }
    emit(trace, table.concat(row, ",") .. "\n")
    push_ring(compact_event(kind, name, address, p, raw_bank, prior, new))
    return index, q, p
end

local function write_actor_snapshot(stage, event_index)
    if S.actor_rows + 11 > M.caps.actor_rows then
        S.deferred_failure = "CPU pass-state-3 actor-row cap exceeded"
        return
    end
    for actor = 0, 10 do
        local a = actor_values(actor)
        S.actor_rows = S.actor_rows + 1
        emit(actors, string.format("%d,%s,%d,%d,%04X,%02X,%02X,%04X,%04X,%04X,%04X\n",
            event_index, stage, safe_frame(), actor, a.x, a.y, a.state,
            a.altitude, a.altitude_velocity, a.horizontal_velocity, a.vertical_velocity))
    end
end

local function save_screenshot(label)
    if S.screenshot_count >= M.caps.screenshots then return end
    S.screenshot_count = S.screenshot_count + 1
    local path = path_join(output_root, string.format("%02d-%s.png", S.screenshot_count, label))
    if not pcall(gui.savescreenshotas, path) then
        S.deferred_failure = "CPU pass-state-3 screenshot failed"
    end
end

local function cpu_context(q)
    return q.side >= 0 and q.side <= 1 and actor_valid(q.offense) and
        q.control ~= 0 and q.foul_route == 0 and q.violation_route == 0
end

local function copy_ring()
    local copy = {}
    for i, value in ipairs(S.ring) do copy[i] = value end
    return copy
end

local function start_source_transition(event_index, q, callback_pc, callback_bank, prior, new)
    local c = S.chain
    c.captured = true
    c.source_frame = S.frame
    c.source_event = event_index
    c.source_prior = prior
    c.source_new = new
    c.writer_pc = callback_pc
    c.writer_raw_bank = callback_bank or -1
    c.writer_control = q.control
    c.pre_sequence = copy_ring()
    S.phase = "state3_chain"
    write_actor_snapshot("state3_transition", event_index)
    save_screenshot("state3")
end

local function append_alternate(name)
    local c = S.chain
    if name == "alternate_B8F6" then
        c.contact_anchor_seen = true
        return
    end
    c.alternate_count = c.alternate_count + 1
    if #c.alternates < 8 then table.insert(c.alternates, name) end
end

local function update_chain(hook, event_index, q)
    local c = S.chain
    if not c.captured then return end
    if q.foul_route ~= 0 or q.violation_route ~= 0 then c.nonzero_route_seen = true end
    if hook.kind == "alternate" then append_alternate(hook.name) return end
    if hook.name == "slot10_state_dispatch_A214" and q.slot10_state == 3 and
            c.dispatch < 0 then
        c.dispatch = event_index
    elseif hook.name == "slot10_state3_consume_B074" and c.dispatch >= 0 and
            c.b074 < 0 and q.slot10_state == 3 and actor_valid(q.candidate) then
        c.b074 = event_index
        c.b074_candidate = q.candidate
        c.b074_receiver = q.candidate
    elseif hook.name == "slot10_state4_settle_entry_B1E7" and c.b074 >= 0 and
            c.b1e7 < 0 then
        c.b1e7 = event_index
    elseif hook.name == "slot10_state_countdown_B500" and c.b1e7 >= 0 and
            c.b500 < 0 then
        c.b500 = event_index
    elseif hook.name == "slot10_state4_settle_gate_B228" and c.b500 >= 0 and
            c.b228 < 0 then
        c.b228 = event_index
    elseif hook.name == "slot10_receiver_settle_B24F" and c.b228 >= 0 and
            c.b24f < 0 then
        c.b24f = event_index
        c.b24f_prior_holder = q.offense
        c.b24f_expected_holder = q.side_holder
        write_actor_snapshot("B24F_entry", event_index)
    end
end

local function source_hook_accepted(hook)
    local raw_bank = mapped_raw_bank(hook.address)
    return raw_bank ~= nil and M.raw_banks[hook.gate][raw_bank] == true, raw_bank
end

local function register_hook(hook)
    memory.registerexec(hook.address, function()
        local ok, failure = xpcall(function()
            local accepted, raw_bank = source_hook_accepted(hook)
            if not accepted then return end
            if not S.live_seen then return end
            local event_index, q = record_event("exec", hook.name, hook.address,
                raw_bank, -1, -1, hook)
            if event_index >= 0 then update_chain(hook, event_index, q) end
        end, function(message) return tostring(message) end)
        if not ok then
            local text = string.gsub(tostring(failure), "[%c]", " ")
            S.deferred_failure = "CPU pass-state-3 registerexec callback failed: " ..
                string.sub(text, 1, 160)
        end
    end)
end

for _, hook in ipairs(M.hooks) do register_hook(hook) end

memory.registerwrite(R.object_slot10_state, function(_, _, value)
    local ok, failure = xpcall(function()
        local prior = S.last_slot10_state
        local new = value
        if new == nil then new = rb(R.object_slot10_state) end
        S.last_slot10_state = new
        S.write_events = S.write_events + 1
        local callback_pc = safe_register("pc")
        local callback_bank = mapped_raw_bank(callback_pc)
        local event_index, q = record_event("write", "slot10_state_0478", R.object_slot10_state,
            callback_bank, prior, new, { address_confidence = "exact_source_pinned",
                label_confidence = "exact_mechanics" })
        if event_index < 0 then return end
        if new == 3 and prior ~= 3 then
            if S.live_seen and S.cpu_possession_frames >= 8 and cpu_context(q) and
                    not S.chain.captured then
                start_source_transition(event_index, q, callback_pc, callback_bank, prior, new)
            elseif S.live_seen and not S.chain.captured then
                S.non_cpu_state3_writes = S.non_cpu_state3_writes + 1
            end
        end
    end, function(message) return tostring(message) end)
    if not ok then
        local text = string.gsub(tostring(failure), "[%c]", " ")
        S.deferred_failure = "CPU pass-state-3 write callback failed: " ..
            string.sub(text, 1, 160)
    end
end)

local function setup_valid()
    return rb(R.mode) == M.setup.mode and rb(R.screen) == M.setup.screen and
        rb(R.period) == M.setup.period and rb(R.orientation) == M.setup.orientation and
        rb(R.team0) ~= rb(R.team1) and rb(R.control0) == M.setup.control_human and
        rb(R.control1) == M.setup.control_automatic
end

local function clock_stopped()
    return rb(R.minute) == M.clock.stopped_minute and
        rb(R.second) == M.clock.stopped_second and rb(R.shot_clock) == M.clock.stopped_shot_clock
end

local function clock_running()
    return rb(R.minute) < M.clock.stopped_minute or rb(R.second) > M.clock.stopped_second or
        rb(R.shot_clock) < M.clock.stopped_shot_clock
end

local function apply_tip_schedule(age)
    for _, command in ipairs(M.tip.schedule) do
        if age >= command.first_age and age <= command.last_age then
            local p1, p2 = routed_pads(command.port, command.buttons)
            apply_pads(p1, p2)
            return true
        end
    end
    return false
end

local function flush_outputs()
    local trace_ok = pcall(function() trace:flush() end)
    local actor_ok = pcall(function() actors:flush() end)
    if not trace_ok or not actor_ok then
        S.deferred_failure = "CPU pass-state-3 output flush failed"
        return false
    end
    return true
end

local function write_progress(stage)
    local temp_path = progress_path .. ".tmp"
    local file = io.open(temp_path, "w")
    if not file then
        S.deferred_failure = "CPU pass-state-3 progress sentinel could not open"
        return false
    end
    file:write("schema=TGLPASS3-PROGRESS-1\n")
    file:write("sequence=" .. S.frame .. "\n")
    file:write("emu_frame=" .. safe_frame() .. "\n")
    file:write("stage=" .. stage .. "\n")
    file:write("setup_seen=" .. tostring(S.setup_seen) .. "\n")
    file:write("live_seen=" .. tostring(S.live_seen) .. "\n")
    file:write("cpu_possession_seen=" .. tostring(S.cpu_possession_seen) .. "\n")
    file:write("state3_captured=" .. tostring(S.chain.captured) .. "\n")
    file:write("pass_route_confirmed=" .. tostring(S.chain.pass_route_confirmed) .. "\n")
    file:write("speedmode_ok=" .. tostring(S.speedmode_ok) .. "\n")
    file:flush()
    file:close()
    for _ = 1, M.caps.progress_publish_attempts do
        os.remove(progress_path)
        if os.rename(temp_path, progress_path) then return true end
    end
    S.deferred_failure = "CPU pass-state-3 progress sentinel could not publish"
    return false
end

local function sequence_text(values)
    if #values == 0 then return "none\n" end
    return table.concat(values, "\n") .. "\n"
end

local function write_sequence()
    local file = io.open(sequence_path, "w")
    if not file then return false end
    file:write("schema=TGLPASS3-SEQUENCE-1\n")
    file:write("source_event=" .. S.chain.source_event .. "\n")
    file:write("pre_sequence\n")
    file:write(sequence_text(S.chain.pre_sequence))
    file:write("post_sequence\n")
    file:write(sequence_text(S.chain.post_sequence))
    file:close()
    return true
end

local function status_value(value)
    return string.gsub(tostring(value), "[\r\n]", " ")
end

local function write_status(result)
    local c = S.chain
    local final = snapshot()
    local file = io.open(status_path, "w")
    if not file then return end
    file:write("schema=" .. M.output_schema .. "\n")
    file:write("schema_version=" .. M.output_schema_version .. "\n")
    file:write("map_schema=" .. M.schema .. "\n")
    file:write("result=" .. result .. "\n")
    file:write("rom_sha256=" .. M.rom_sha256 .. "\n")
    file:write("fceux_sha256=" .. M.fceux_sha256 .. "\n")
    file:write("max_frames=" .. M.caps.max_frames .. "\n")
    file:write("max_live_frames=" .. M.caps.max_live_frames .. "\n")
    file:write("speedmode_ok=" .. tostring(S.speedmode_ok) .. "\n")
    file:write("setup_seen=" .. tostring(S.setup_seen) .. "\n")
    file:write("setup_frame=" .. S.setup_frame .. "\n")
    file:write("tip_started=" .. tostring(S.tip_started) .. "\n")
    file:write("clock_stopped_seen=" .. tostring(S.clock_stopped_seen) .. "\n")
    file:write("clock_running_seen=" .. tostring(S.clock_running_seen) .. "\n")
    file:write("live_seen=" .. tostring(S.live_seen) .. "\n")
    file:write("live_start_frame=" .. S.live_start_frame .. "\n")
    file:write("cpu_possession_seen=" .. tostring(S.cpu_possession_seen) .. "\n")
    file:write("cpu_possession_first_frame=" .. S.cpu_possession_first_frame .. "\n")
    file:write("cpu_possession_frames=" .. S.cpu_possession_frames .. "\n")
    file:write("state3_captured=" .. tostring(c.captured) .. "\n")
    file:write("state3_source_frame=" .. c.source_frame .. "\n")
    file:write("state3_source_event=" .. c.source_event .. "\n")
    file:write("state3_prior=" .. hex_or_na(c.source_prior, 2) .. "\n")
    file:write("state3_new=" .. hex_or_na(c.source_new, 2) .. "\n")
    file:write("writer_callback_pc=" .. hex_or_na(c.writer_pc, 4) .. "\n")
    file:write("writer_callback_raw_bank=" .. hex_or_na(c.writer_raw_bank, 2) .. "\n")
    file:write("writer_offense_control=" .. hex_or_na(c.writer_control, 2) .. "\n")
    file:write("a214_after_state3_event=" .. c.dispatch .. "\n")
    file:write("b074_event=" .. c.b074 .. "\n")
    file:write("b074_candidate_037f=" .. hex_or_na(c.b074_candidate, 2) .. "\n")
    file:write("b074_receiver_0e=" .. hex_or_na(c.b074_receiver, 2) .. "\n")
    file:write("b1e7_event=" .. c.b1e7 .. "\n")
    file:write("b500_event=" .. c.b500 .. "\n")
    file:write("b228_event=" .. c.b228 .. "\n")
    file:write("b24f_event=" .. c.b24f .. "\n")
    file:write("b24f_prior_holder=" .. hex_or_na(c.b24f_prior_holder, 2) .. "\n")
    file:write("b24f_expected_holder=" .. hex_or_na(c.b24f_expected_holder, 2) .. "\n")
    file:write("holder_changed_through_b24f=" .. tostring(c.holder_changed) .. "\n")
    file:write("holder_change_frame=" .. c.holder_change_frame .. "\n")
    file:write("alternate_count=" .. c.alternate_count .. "\n")
    file:write("alternates=" .. status_value(table.concat(c.alternates, ";")) .. "\n")
    file:write("contact_anchor_seen=" .. tostring(c.contact_anchor_seen) .. "\n")
    file:write("nonzero_route_seen=" .. tostring(c.nonzero_route_seen) .. "\n")
    file:write("pass_route_confirmed=" .. tostring(c.pass_route_confirmed) .. "\n")
    file:write("non_cpu_state3_writes=" .. S.non_cpu_state3_writes .. "\n")
    file:write("write_events=" .. S.write_events .. "\n")
    file:write("trace_rows=" .. S.event_rows .. "\n")
    file:write("actor_rows=" .. S.actor_rows .. "\n")
    file:write("screenshot_count=" .. S.screenshot_count .. "\n")
    file:write("final_progress_written=" .. tostring(S.progress_written) .. "\n")
    file:write("ram_writes=0\ncheats=0\nsavestates=0\nfm2=0\n")
    file:write("final_pads_neutral=" .. tostring(S.final_pads_neutral) .. "\n")
    file:write("final_mode=" .. hex_or_na(rb(R.mode), 2) .. "\n")
    file:write("final_screen=" .. hex_or_na(rb(R.screen), 2) .. "\n")
    file:write("final_period=" .. hex_or_na(rb(R.period), 2) .. "\n")
    file:write("final_menu_selection=" .. hex_or_na(final.menu_selection, 2) .. "\n")
    file:write("final_menu_phase=" .. hex_or_na(final.menu_phase, 2) .. "\n")
    file:write("final_control0=" .. hex_or_na(rb(R.control0), 2) .. "\n")
    file:write("final_control1=" .. hex_or_na(rb(R.control1), 2) .. "\n")
    file:write("final_offense_actor=" .. hex_or_na(final.offense, 2) .. "\n")
    file:write("final_offense_side=" .. hex_or_na(final.side, 2) .. "\n")
    file:write("final_offense_control=" .. hex_or_na(final.control, 2) .. "\n")
    file:write("stop_reason=" .. status_value(S.stop_reason) .. "\n")
    file:close()
end

local function direct_chain_complete()
    local c = S.chain
    return c.captured and c.dispatch >= 0 and c.b074 >= 0 and c.b1e7 >= 0 and
        c.b500 >= 0 and c.b228 >= 0 and c.b24f >= 0 and c.holder_changed and
        c.b074_receiver == c.b24f_expected_holder and c.b24f_prior_holder ~= c.b24f_expected_holder and
        c.alternate_count == 0 and not c.nonzero_route_seen and c.writer_raw_bank >= 0
end

local function finish(reason, result)
    if S.stopped then return end
    if result == "pass" and not direct_chain_complete() then
        result = "abort"
        reason = "source state3 chain did not meet direct B24F settlement predicate"
    end
    S.stopped = true
    S.stop_reason = reason
    local neutral_ok = pcall(function() apply_pads({}, {}) end)
    S.final_pads_neutral = neutral_ok and pads_neutral()
    if not S.chain.captured then save_screenshot("no-state3") end
    flush_outputs()
    if not write_sequence() and result == "pass" then
        result = "abort"
        S.stop_reason = "CPU pass-state-3 sequence output failed"
    end
    local progress_call_ok, progress_result = pcall(write_progress, "finished")
    S.progress_written = progress_call_ok and progress_result == true
    if not S.progress_written and result == "pass" then
        result = "abort"
        S.stop_reason = "CPU pass-state-3 final progress publish failed"
    end
    pcall(function() trace:flush(); actors:flush() end)
    pcall(function() trace:close(); actors:close() end)
    write_status(result)
    pcall(emu.exit)
end

function TECMO_CPU_PASS_STATE3_EMERGENCY(message)
    finish("uncaught Lua error: " .. tostring(message), "abort")
end

local function observe_holder_settle()
    local c = S.chain
    if not c.captured then return end
    local q = snapshot()
    if q.foul_route ~= 0 or q.violation_route ~= 0 then c.nonzero_route_seen = true end
    if c.b24f >= 0 and not c.holder_changed and actor_valid(c.b24f_expected_holder) and
            q.offense == c.b24f_expected_holder and q.offense ~= c.b24f_prior_holder then
        c.holder_changed = true
        c.holder_change_frame = S.frame
        local event_index = record_event("observe", "holder_changed_after_B24F", 0xB24F,
            mapped_raw_bank(0xB24F), -1, -1,
            { address_confidence = "exact_source_pinned", label_confidence = "exact_mechanics" })
        if event_index >= 0 then write_actor_snapshot("holder_changed", event_index) end
        save_screenshot("B24F-holder")
        if direct_chain_complete() then
            c.pass_route_confirmed = true
            finish("first CPU state3 transition completed direct B074/B24F holder-change chain", "pass")
            return
        end
    end
    if S.frame - c.source_frame > M.caps.chain_settle_frames then
        finish("first CPU state3 transition did not settle through bounded direct B24F chain", "abort")
    end
end

local function execute_frame()
    S.frame = S.frame + 1
    apply_pads({}, {})
    if S.deferred_failure ~= nil then finish(S.deferred_failure, "abort") return end

    if not S.setup_seen then
        for _, command in ipairs(M.boot_inputs) do
            local active = S.frame >= command.first and S.frame <= command.last
            if active and command.every then
                active = ((S.frame - command.first) % command.every) < command.width
            end
            if active and not (command.until_setup and S.setup_seen) then
                local p1, p2 = routed_pads(command.port, { command.button })
                apply_pads(p1, p2)
            end
            if S.frame == command.first then
                record_event("input", "boot_p" .. command.port .. "_" .. command.button,
                    0, -1, -1, -1,
                    { address_confidence = "controller_schedule",
                        label_confidence = "controller_schedule" })
            end
        end
        if setup_valid() then
            S.setup_seen = true
            S.setup_frame = S.frame
            S.tip_started = true
            S.tip_start_frame = S.frame
            S.phase = "tip"
            write_progress("tip")
        end
    elseif not S.live_seen then
        if clock_stopped() then
            S.clock_stopped_seen = true
            local age = S.frame - S.tip_start_frame
            apply_tip_schedule(age)
            if age > M.tip.deadline then
                finish("tip did not start the game clock before deadline", "abort")
                return
            end
        elseif S.clock_stopped_seen and clock_running() and setup_valid() then
            S.clock_running_seen = true
            S.live_seen = true
            S.live_start_frame = S.frame
            S.phase = "live_neutral"
            write_progress("live_neutral")
        elseif S.frame - S.tip_start_frame > M.tip.deadline then
            finish("running clock did not restore CPU-opponent live setup", "abort")
            return
        end
    else
        local q = snapshot()
        if cpu_context(q) then
            S.cpu_possession_frames = S.cpu_possession_frames + 1
            if not S.cpu_possession_seen and S.cpu_possession_frames >= 8 then
                S.cpu_possession_seen = true
                S.cpu_possession_first_frame = S.frame
            end
        else
            S.cpu_possession_frames = 0
        end
        observe_holder_settle()
        if S.stopped then return end
        if S.frame - S.live_start_frame >= M.caps.max_live_frames then
            finish("conservative natural CPU-observation bound reached without a complete pass-state3 chain", "abort")
            return
        end
    end

    if S.deferred_failure ~= nil then finish(S.deferred_failure, "abort") return end
    if S.frame % M.caps.progress_period == 0 then
        flush_outputs()
        write_progress(S.phase)
    end
    if S.frame >= M.caps.max_frames then
        finish("absolute frame cap reached", "abort")
    end
end

S.last_slot10_state = rb(R.object_slot10_state)
S.speedmode_ok = pcall(FCEU.speedmode, "maximum")
if not S.speedmode_ok then S.deferred_failure = "FCEU maximum speed mode unavailable" end
local metadata = assert(io.open(path_join(output_root, "metadata.txt"), "w"))
metadata:write("schema=" .. M.output_schema .. "\n")
metadata:write("map_schema=" .. M.schema .. "\n")
metadata:write("rom_sha256=" .. M.rom_sha256 .. "\n")
metadata:write("fceux_sha256=" .. M.fceux_sha256 .. "\n")
metadata:write("policy=controller-input-only; no RAM writes; no cheats; no savestates; no FM2\n")
metadata:write("scope=first CPU-controlled slot10 state3 write with source-pinned direct B074/B24F chain\n")
metadata:close()
write_progress("startup")
apply_pads({}, {})

while not S.stopped do
    local ok, failure = xpcall(execute_frame, function(message) return tostring(message) end)
    if not ok then TECMO_CPU_PASS_STATE3_EMERGENCY(failure) end
    if not S.stopped then FCEU.frameadvance() end
end
