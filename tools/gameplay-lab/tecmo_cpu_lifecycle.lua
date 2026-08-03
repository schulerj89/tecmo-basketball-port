-- Read-only original-reference CPU lifecycle trace for canonical Rev1.
-- This script supplies complete joypads only. It never writes emulated RAM,
-- loads a state, enables a cheat, or patches the ROM.

local output_root = assert(os.getenv("TECMO_CPU_LIFECYCLE_OUTPUT"),
    "TECMO_CPU_LIFECYCLE_OUTPUT is required")
local map_path = assert(os.getenv("TECMO_CPU_LIFECYCLE_MAP"),
    "TECMO_CPU_LIFECYCLE_MAP is required")
output_root = string.gsub(output_root, "\\", "/")
map_path = string.gsub(map_path, "\\", "/")
local map = assert(dofile(map_path))
assert(map.schema == "TGLCPU-1" and map.schema_version == 1,
    "unsupported CPU lifecycle map")
assert(os.getenv("TECMO_CPU_LIFECYCLE_ROM_SHA256") == map.rom_sha256,
    "runner ROM fingerprint mismatch")
assert(os.getenv("TECMO_CPU_LIFECYCLE_FCEUX_SHA256") == map.fceux_sha256,
    "runner FCEUX fingerprint mismatch")

local max_frames = tonumber(os.getenv("TECMO_CPU_LIFECYCLE_MAX_FRAMES") or
    tostring(map.caps.max_frames)) or map.caps.max_frames
assert(max_frames >= map.reference_window.frames and
       max_frames <= map.caps.max_frames,
    "CPU lifecycle frame cap is outside the map contract")

local function path_join(a, b)
    if string.sub(a, -1) == "/" then return a .. b end
    return a .. "/" .. b
end

local metadata = assert(io.open(path_join(output_root, "metadata.txt"), "w"))
local trace = assert(io.open(path_join(output_root, "trace.csv"), "w"))
local actors = assert(io.open(path_join(output_root, "actors.csv"), "w"))
local status_path = path_join(output_root, "status.txt")
local progress_path = path_join(output_root, "progress.txt")
local tracked_text_bytes = 0
local trace_rows = 0
local actor_rows = 0
local screenshot_count = 0
local frame = 0
local setup_seen = false
local setup_frame = -1
local tip_started = false
local tip_start_frame = -1
local tip_not_running_seen = false
local clock_stopped_seen = false
local clock_running_seen = false
local clock_running_observed = false
local running_clock_live_seen = false
local live_seen = false
local live_start_frame = -1
local capture_start_frame = -1
local captured = 0
local final_progress_written = false
local hook_order = 0
local stopped = false
local stop_reason = "running"
local deferred_failure = nil
local final_pads_neutral = false
local lifecycle = {
    fetch_events = 0,
    opcode_observations = 0,
    dispatch_events = 0,
    handler_events = 0,
    advance_events = 0,
    rewind_events = 0,
    aligned_stream_offsets = 0,
    fixed_link_observations = 0,
    fixed_link_mismatches = 0,
    invalid_fetches = 0,
    misaligned_fetches = 0,
    actors = {},
    opcodes = {},
    handlers = {}
}

local function emit(file, value)
    tracked_text_bytes = tracked_text_bytes + string.len(value)
    if tracked_text_bytes > map.caps.tracked_text_bytes then
        deferred_failure = "CPU lifecycle tracked-text byte cap exceeded"
        return false
    end
    file:write(value)
    return true
end

local safe_frame
local speedmode_ok = false
local function write_progress(stage)
    local temp_path = progress_path .. ".tmp"
    local sequence = frame
    local file = io.open(temp_path, "w")
    if not file then
        deferred_failure = "CPU lifecycle progress sentinel could not open"
        return false
    end
    file:write("schema=TGLCPU-PROGRESS-1\n")
    file:write("sequence=" .. sequence .. "\n")
    file:write("emu_frame=" .. safe_frame() .. "\n")
    file:write("stage=" .. stage .. "\n")
    file:write("setup_seen=" .. tostring(setup_seen) .. "\n")
    file:write("tip_started=" .. tostring(tip_started) .. "\n")
    file:write("running_clock_live_seen=" .. tostring(running_clock_live_seen) .. "\n")
    file:write("captured_frames=" .. captured .. "\n")
    file:write("speedmode_ok=" .. tostring(speedmode_ok) .. "\n")
    file:flush()
    file:close()
    local published = false
    local remove_state = "not_attempted"
    for attempt = 1, map.caps.progress_publish_attempts do
        local removed = os.remove(progress_path)
        remove_state = removed and "removed" or "missing_or_locked"
        local renamed = os.rename(temp_path, progress_path)
        if renamed then
            published = true
            break
        end
    end
    if not published then
        deferred_failure = "CPU lifecycle progress sentinel could not publish: " .. remove_state
        return false
    end
    return true
end

emit(metadata,
    "schema=" .. map.output_schema .. "\n" ..
    "schema_version=" .. map.output_schema_version .. "\n" ..
    "map_schema=" .. map.schema .. "\n" ..
    "rom_sha256=" .. map.rom_sha256 .. "\n" ..
    "fceux_sha256=" .. map.fceux_sha256 .. "\n" ..
    "policy=controller-input-only; no RAM writes; no cheats; no savestates\n" ..
    "address_evidence=exact_source_pinned; label_confidence=per-hook bounded classification\n" ..
    "decomp_comment_note=" .. map.command.decomp_comment_note .. "\n")
metadata:close()
local trace_header = "capture_frame,emu_frame,hook_order,name,address,raw_bank,mapper_select,mapper_bank6,mapper_bank7,pc,a,x,y,stream_actor,stream_offset,opcode,arg0,arg1,arg2,arg3,handler_cpu,actor_state,actor_x,actor_y,flags_ba,fixed_link,primary,defender,address_confidence,label_confidence\n"
local trace_format = "%d,%d,%d,%s,%04X,%02X,%02X,%02X,%02X,%04X,%02X,%02X,%02X,%d,%04X,%d,%02X,%02X,%02X,%02X,%04X,%02X,%04X,%02X,%02X,%s,%d,%d,%s,%s\n"
emit(trace, trace_header)
emit(actors,
    "capture_frame,emu_frame,actor,x,y,state_057C,state_046E,stream_offset," ..
    "fixed_link\n")

local R = map.ram
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
safe_frame = function()
    local ok, value = pcall(emu.framecount)
    if not ok or value == nil then return 0 end
    return value
end
speedmode_ok = pcall(FCEU.speedmode, "maximum")
if not speedmode_ok then
    deferred_failure = "CPU lifecycle FCEU maximum speed mode unavailable"
end
write_progress("startup")

local blank_pad = {
    A=false, B=false, up=false, down=false,
    left=false, right=false, start=false, select=false
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
local function routed_pads(port, button)
    local p1, p2 = {}, {}
    if port == 1 then p1[button] = true else p2[button] = true end
    return p1, p2
end
local function flush_outputs()
    local trace_ok = pcall(function() trace:flush() end)
    local actors_ok = pcall(function() actors:flush() end)
    if not trace_ok or not actors_ok then
        deferred_failure = "CPU lifecycle trace/actor flush failed"
        return false
    end
    return true
end

local bank_select = 0
local bank_registers = {0,0,0,0,0,0,0,0}
local bank_known = {false,false,false,false,false,false,false,false}
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
memory.registerwrite(map.mapper.select, function(_, _, value)
    bank_select = value or 0
end)
memory.registerwrite(map.mapper.data, function(_, _, value)
    local index = AND(bank_select, 0x07) + 1
    local value8 = value or 0
    if index == 7 or index == 8 then value8 = AND(value8, 0x0F) end
    bank_registers[index], bank_known[index] = value8, true
end)

local registered_addresses = {}
local handler_addresses = {}
for _, address in ipairs(map.command.handler_cpu) do handler_addresses[address] = true end
local function stream_actor_offset(actor_index)
    if actor_index < 0 or actor_index > 9 then return -1 end
    return rb(R.actor_stream_lo + actor_index) +
        256 * rb(R.actor_stream_hi + actor_index)
end
local function command_fields(address)
    if address ~= map.command.opcode_load_cpu and
            address ~= map.command.dispatch_cpu and
            not handler_addresses[address] then
        return -1, 0, 0, 0, 0, 0
    end
    local opcode = rb(R.target_c8 - 1)
    local a0, a1, a2, a3 = rb(R.target_c8), rb(R.target_c9),
        rb(R.target_ca), rb(R.target_cb)
    local handler = 0
    if opcode >= 0 and opcode < 24 then handler = map.command.handler_cpu[opcode + 1] end
    return opcode, a0, a1, a2, a3, handler
end
local function fixed_link_text()
    local values = {}
    for index = 0, 9 do values[#values + 1] = string.format("%02X", rb(R.fixed_link + index)) end
    return table.concat(values, ":")
end
local function fixed_link_exact()
    for index = 0, 9 do
        if rb(R.fixed_link + index) ~= map.fixed_startup.links[index + 1] then
            return false
        end
    end
    return true
end
local function count_keys(values)
    local count = 0
    for _ in pairs(values) do count = count + 1 end
    return count
end
local function lifecycle_evidence_valid()
    return clock_stopped_seen and running_clock_live_seen and
        lifecycle.fetch_events > 0 and
        lifecycle.opcode_observations > 0 and
        lifecycle.dispatch_events > 0 and
        lifecycle.handler_events > 0 and
        lifecycle.advance_events > 0 and
        lifecycle.aligned_stream_offsets > 0 and
        lifecycle.fixed_link_observations > 0 and
        lifecycle.fixed_link_mismatches == 0 and
        lifecycle.invalid_fetches == 0 and
        lifecycle.misaligned_fetches == 0 and
        screenshot_count == map.caps.screenshots
end

local function record_hook(hook, raw_bank)
    if not live_seen or capture_start_frame < 0 or frame < capture_start_frame or
            captured < 1 or captured > map.reference_window.frames then return end
    if trace_rows >= map.caps.trace_rows then
        deferred_failure = "CPU lifecycle trace-row cap exceeded"
        return
    end
    local actor_index = safe_register("x")
    local address_confidence = hook.address_confidence
    local label_confidence = hook.label_confidence
    if address_confidence ~= "exact_source_pinned" or
            (label_confidence ~= "exact_mechanics" and
             label_confidence ~= "exact_opcode_entry" and
             label_confidence ~= "inferred_label" and
             label_confidence ~= "deferred_mechanics") then
        deferred_failure = "CPU lifecycle hook confidence classification invalid"
        return
    end
    local opcode, a0, a1, a2, a3, handler = command_fields(hook.address)
    local stream_offset = stream_actor_offset(actor_index)
    if hook.kind == "fetch" then
        local valid_actor = actor_index >= 0 and actor_index <= 9
        local in_range = stream_offset >= 0 and
            stream_offset < map.command.record_count * map.command.record_size
        local aligned = in_range and stream_offset % map.command.record_size == 0
        if not valid_actor then lifecycle.invalid_fetches = lifecycle.invalid_fetches + 1 end
        if not in_range then lifecycle.invalid_fetches = lifecycle.invalid_fetches + 1 end
        if in_range and not aligned then lifecycle.misaligned_fetches =
            lifecycle.misaligned_fetches + 1 end
        if not valid_actor or not in_range or not aligned then
            deferred_failure = "CPU lifecycle fetch actor/stream offset invalid"
            return
        end
        lifecycle.fetch_events = lifecycle.fetch_events + 1
        lifecycle.aligned_stream_offsets = lifecycle.aligned_stream_offsets + 1
    end
    if hook.kind == "opcode" then lifecycle.opcode_observations = lifecycle.opcode_observations + 1 end
    if hook.kind == "dispatch" then lifecycle.dispatch_events = lifecycle.dispatch_events + 1 end
    if hook.kind == "handler" then
        lifecycle.handler_events = lifecycle.handler_events + 1
        lifecycle.handlers[hook.address] = true
    end
    if hook.kind == "advance" then lifecycle.advance_events = lifecycle.advance_events + 1 end
    if hook.kind == "rewind" then lifecycle.rewind_events = lifecycle.rewind_events + 1 end
    if actor_index >= 0 and actor_index <= 9 then lifecycle.actors[actor_index] = true end
    if opcode >= 0 and opcode < 24 then lifecycle.opcodes[opcode] = true end
    if hook.kind == "fetch" then
        if fixed_link_exact() then lifecycle.fixed_link_observations =
            lifecycle.fixed_link_observations + 1
        else
            lifecycle.fixed_link_mismatches = lifecycle.fixed_link_mismatches + 1
            deferred_failure = "CPU lifecycle fixed-link bytes mismatch"
            return
        end
    end
    hook_order = hook_order + 1
    trace_rows = trace_rows + 1
    local trace_values = {
        [1] = captured,
        [2] = safe_frame(),
        [3] = hook_order,
        [4] = hook.name,
        [5] = hook.address,
        [6] = raw_bank or 0,
        [7] = bank_select,
        [8] = bank_registers[7],
        [9] = bank_registers[8],
        [10] = safe_register("pc"),
        [11] = safe_register("a"),
        [12] = actor_index,
        [13] = safe_register("y"),
        [14] = actor_index,
        [15] = stream_offset,
        [16] = opcode,
        [17] = a0,
        [18] = a1,
        [19] = a2,
        [20] = a3,
        [21] = handler,
        [22] = actor_index >= 0 and rb(R.actor_state + actor_index) or 0,
        [23] = actor_index >= 0 and word(R.actor_x_lo + actor_index, R.actor_x_hi + actor_index) or 0,
        [24] = actor_index >= 0 and rb(R.actor_y + actor_index) or 0,
        [25] = rb(R.flags_BA),
        [26] = fixed_link_text(),
        [27] = rb(R.offense_actor),
        [28] = rb(R.defense_actor),
        [29] = address_confidence,
        [30] = label_confidence
    }
    emit(trace, string.format(trace_format, unpack(trace_values)))
end

local function register_hook(hook)
    if hook.address_confidence ~= "exact_source_pinned" or
            (hook.label_confidence ~= "exact_mechanics" and
             hook.label_confidence ~= "exact_opcode_entry" and
             hook.label_confidence ~= "inferred_label" and
             hook.label_confidence ~= "deferred_mechanics") then
        deferred_failure = "CPU lifecycle hook confidence classification missing"
        return
    end
    if registered_addresses[hook.address] then return end
    registered_addresses[hook.address] = true
    local function defer_callback_failure(message)
        local value = string.gsub(tostring(message), "[%c]", " ")
        if string.len(value) > 160 then value = string.sub(value, 1, 160) end
        deferred_failure = "CPU lifecycle registerexec callback failed: " .. value
    end
    memory.registerexec(hook.address, function()
        local ok, failure = xpcall(function()
            local raw_bank = mapped_raw_bank(hook.address)
            local accepted = raw_bank ~= nil and map.raw_banks[hook.gate][raw_bank] == true
            if accepted then record_hook(hook, raw_bank) end
        end, function(message) return tostring(message) end)
        if not ok then defer_callback_failure(failure) end
    end)
end
for _, hook in ipairs(map.hooks) do register_hook(hook) end
for opcode, address in ipairs(map.command.handler_cpu) do
    register_hook({
        address = address,
        name = string.format("handler_%02d", opcode - 1),
        gate = "bank06",
        kind = "handler",
        address_confidence = "exact_source_pinned",
        label_confidence = "exact_opcode_entry"
    })
end

local function write_actor_rows()
    if actor_rows + 11 > map.caps.actor_rows then
        deferred_failure = "CPU lifecycle actor-row cap exceeded"
        return
    end
    for index = 0, 10 do
        actor_rows = actor_rows + 1
        local stream_offset = index <= 9 and string.format("%04X", stream_actor_offset(index)) or "NA"
        local fixed_link = index <= 9 and string.format("%02X", rb(R.fixed_link + index)) or "NA"
        emit(actors, string.format("%d,%d,%d,%d,%d,%02X,%02X,%s,%s\n",
            captured, safe_frame(), index, word(R.actor_x_lo + index,
                R.actor_x_hi + index), rb(R.actor_y + index), rb(R.actor_state + index),
            rb(R.actor_state_046E + index), stream_offset,
            fixed_link))
    end
end

local function save_reference_frame()
    if screenshot_count >= map.caps.screenshots then return end
    if captured % math.floor(map.reference_window.frames / map.caps.screenshots) ~= 1 then return end
    screenshot_count = screenshot_count + 1
    local path = path_join(output_root, string.format("reference-frame-%04d.png", screenshot_count))
    local ok = pcall(gui.savescreenshotas, path)
    if not ok then deferred_failure = "reference screenshot failed" end
end

local function setup_valid()
    return rb(0x0055) == map.setup.mode and rb(0x0087) == map.setup.screen and
        rb(0x035C) == map.setup.period and rb(R.orientation_035A) == map.setup.orientation and
        rb(R.offense_side) == map.setup.offense_side and
        rb(R.control0) == map.setup.control0 and rb(R.control1) == map.setup.control1 and
        rb(R.team0) ~= rb(R.team1)
end
local function clock_stopped()
    return rb(R.minute) == map.clock.stopped_minute and
        rb(R.second) == map.clock.stopped_second and
        rb(R.shot_clock) == map.clock.stopped_shot_clock
end
local function live_setup_valid()
    local offense_side = rb(R.offense_side)
    local defense_side = rb(R.defense_side)
    local offense_actor = rb(R.offense_actor)
    local defense_actor = rb(R.defense_actor)
    local actors_valid = offense_actor >= 0 and offense_actor <= 9 and
        defense_actor >= 0 and defense_actor <= 9
    return rb(0x0055) == map.live.mode and rb(0x0087) == map.live.screen and
        rb(0x035C) == map.live.period and rb(R.orientation_035A) == map.live.orientation and
        offense_side == map.live.offense_side and
        defense_side == (1 - offense_side) and actors_valid and
        rb(R.control0) == map.live.control0 and rb(R.control1) == map.live.control1 and
        rb(R.team0) ~= rb(R.team1)
end
local function clock_running()
    return rb(R.minute) < map.clock.stopped_minute or
        rb(R.second) > map.clock.stopped_second or
        rb(R.shot_clock) < map.clock.stopped_shot_clock
end
local function apply_tip_schedule(age)
    for _, command in ipairs(map.tip.schedule) do
        if age >= command.first_age and age <= command.last_age then
            local p1, p2 = {}, {}
            local target = command.port == 1 and p1 or p2
            for _, button in ipairs(command.buttons) do target[button] = true end
            apply_pads(p1, p2)
            return true
        end
    end
    return false
end

local function write_status(state, result)
    local file = io.open(status_path, "w")
    if not file then return end
    file:write("schema=" .. map.output_schema .. "\n")
    file:write("schema_version=" .. map.output_schema_version .. "\n")
    file:write("map_schema=" .. map.schema .. "\n")
    file:write("state=" .. state .. "\nresult=" .. result .. "\n")
    file:write("rom_sha256=" .. map.rom_sha256 .. "\nfceux_sha256=" .. map.fceux_sha256 .. "\n")
    file:write("speedmode_ok=" .. tostring(speedmode_ok) .. "\n")
    file:write("setup_seen=" .. tostring(setup_seen) .. "\n")
    file:write("setup_frame=" .. setup_frame .. "\ntip_started=" .. tostring(tip_started) .. "\n")
    file:write("tip_start_frame=" .. tip_start_frame .. "\ntip_not_running_seen=" ..
        tostring(tip_not_running_seen) .. "\n")
    file:write("clock_stopped_seen=" .. tostring(clock_stopped_seen) .. "\n")
    file:write("clock_running_seen=" .. tostring(clock_running_seen) .. "\n")
    file:write("clock_running_observed=" .. tostring(clock_running_observed) .. "\n")
    file:write("running_clock_live_seen=" .. tostring(running_clock_live_seen) .. "\n")
    file:write("live_seen=" .. tostring(live_seen) .. "\n")
    file:write("live_start_frame=" .. live_start_frame .. "\ncapture_start_frame=" .. capture_start_frame .. "\n")
    file:write("captured_frames=" .. captured .. "\nreference_frames=" .. map.reference_window.frames .. "\n")
    file:write("trace_rows=" .. trace_rows .. "\nactor_rows=" .. actor_rows .. "\n")
    file:write("fetch_events=" .. lifecycle.fetch_events .. "\n")
    file:write("opcode_observations=" .. lifecycle.opcode_observations .. "\n")
    file:write("dispatch_events=" .. lifecycle.dispatch_events .. "\n")
    file:write("handler_events=" .. lifecycle.handler_events .. "\n")
    file:write("advance_events=" .. lifecycle.advance_events .. "\n")
    file:write("rewind_events=" .. lifecycle.rewind_events .. "\n")
    file:write("aligned_stream_offsets=" .. lifecycle.aligned_stream_offsets .. "\n")
    file:write("fixed_link_observations=" .. lifecycle.fixed_link_observations .. "\n")
    file:write("fixed_link_mismatches=" .. lifecycle.fixed_link_mismatches .. "\n")
    file:write("invalid_fetches=" .. lifecycle.invalid_fetches .. "\n")
    file:write("misaligned_fetches=" .. lifecycle.misaligned_fetches .. "\n")
    file:write("observed_actor_count=" .. count_keys(lifecycle.actors) .. "\n")
    file:write("observed_opcode_count=" .. count_keys(lifecycle.opcodes) .. "\n")
    file:write("observed_handler_count=" .. count_keys(lifecycle.handlers) .. "\n")
    file:write("lifecycle_evidence_valid=" .. tostring(lifecycle_evidence_valid()) .. "\n")
    file:write("screenshot_count=" .. screenshot_count .. "\n")
    file:write("final_progress_written=" .. tostring(final_progress_written) .. "\n")
    file:write("ram_writes=0\ncheats=0\nsavestates=0\n")
    file:write("final_pads_neutral=" .. tostring(final_pads_neutral) .. "\n")
    file:write("capture_window_complete=" .. tostring(captured == map.reference_window.frames) .. "\n")
    file:write("stop_reason=" .. stop_reason .. "\n")
    file:close()
end

local function finish(reason, result)
    if stopped then return end
    if result == "pass" and not lifecycle_evidence_valid() then
        reason = "complete window lacked source-pinned CPU lifecycle evidence"
        result = "abort"
    end
    if not flush_outputs() and result == "pass" then
        reason = "CPU lifecycle trace/actor flush failed"
        result = "abort"
    end
    stopped = true
    stop_reason = reason
    local ok = pcall(function() apply_pads({}, {}) end)
    final_pads_neutral = ok and not active_p1.A and not active_p1.B and
        not active_p1.up and not active_p1.down and not active_p1.left and
        not active_p1.right and not active_p1.start and not active_p1.select and
        not active_p2.A and not active_p2.B and not active_p2.up and
        not active_p2.down and not active_p2.left and not active_p2.right and
        not active_p2.start and not active_p2.select
    local final_progress_call_ok, final_progress_result = pcall(write_progress, "finished")
    final_progress_written = final_progress_call_ok and final_progress_result == true
    if not final_progress_written and result == "pass" then
        reason = "CPU lifecycle final progress publish failed"
        result = "abort"
        stop_reason = reason
    end
    pcall(function() trace:flush(); actors:flush() end)
    pcall(function() trace:close(); actors:close() end)
    write_status(result == "pass" and captured == map.reference_window.frames and
        "complete" or "aborted", result)
    pcall(emu.exit)
end

function TECMO_CPU_LIFECYCLE_EMERGENCY(message)
    finish("uncaught Lua error: " .. tostring(message), "abort")
end

local function execute_frame()
    frame = frame + 1
    apply_pads({}, {})
    if deferred_failure ~= nil then finish(deferred_failure, "abort") return end
    if captured >= map.reference_window.frames then
        finish("bounded live window complete", "pass")
        return
    end
    if not setup_seen then
        for _, command in ipairs(map.boot_inputs) do
            local active = frame >= command.first and frame <= command.last
            if active and command.every then
                active = ((frame - command.first) % command.every) < command.width
            end
            if active and not (command.until_setup and setup_seen) then
                local p1, p2 = routed_pads(command.port, command.button)
                apply_pads(p1, p2)
            end
        end
        if setup_valid() then
            setup_seen = true
            setup_frame = frame
            tip_started = true
            tip_start_frame = frame
            write_progress("tip")
        end
    elseif not running_clock_live_seen then
        if not clock_running() then
            if clock_stopped() then
                clock_stopped_seen = true
                tip_not_running_seen = true
            end
            local age = frame - tip_start_frame
            apply_tip_schedule(age)
            if age > map.tip.deadline then
                finish("tip did not start the game clock before deadline", "abort")
                return
            end
        elseif clock_stopped_seen then
            clock_running_observed = true
            clock_running_seen = true
            if live_setup_valid() then
                running_clock_live_seen = true
                live_seen = true
                live_start_frame = frame
                capture_start_frame = frame + map.reference_window.post_live_delay
                local capture_end_frame = capture_start_frame + map.reference_window.frames - 1
                if capture_end_frame > max_frames then
                    finish("capture window exceeds bounded session max", "abort")
                    return
                end
                write_progress("running-clock-live")
            elseif frame - tip_start_frame > map.tip.deadline then
                finish("running clock did not restore canonical live setup", "abort")
                return
            end
        end
    end
    if live_seen and frame >= capture_start_frame and
            captured < map.reference_window.frames then
        captured = captured + 1
        write_actor_rows()
        save_reference_frame()
    end
    if deferred_failure ~= nil then finish(deferred_failure, "abort") return end
    if frame % map.caps.progress_period == 0 then
        flush_outputs()
        local stage = "boot"
        if setup_seen and not running_clock_live_seen then stage = "tip" end
        if running_clock_live_seen and captured == 0 then stage = "post-clock-delay" end
        if captured > 0 then stage = "capture" end
        write_progress(stage)
    end
    if frame >= max_frames then
        finish("maximum frame cap reached before complete live window", "abort")
    end
end

apply_pads({}, {})
while not stopped do
    local step_ok, step_failure = xpcall(execute_frame, function(message) return tostring(message) end)
    if not step_ok then
        TECMO_CPU_LIFECYCLE_EMERGENCY(step_failure)
    end
    if not stopped then FCEU.frameadvance() end
end
