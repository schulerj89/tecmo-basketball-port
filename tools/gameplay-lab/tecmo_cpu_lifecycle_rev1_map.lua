-- Separate CPU-lifecycle proof map. This is not the closed shot-profile map.
-- It is read-only, mapper-gated, and pinned to canonical Rev1/FCEUX identity.

local function source_hook(address, name, gate, kind, label_confidence)
    return {
        address = address,
        name = name,
        gate = gate,
        kind = kind,
        address_confidence = "exact_source_pinned",
        label_confidence = label_confidence
    }
end

local map = {
    schema = "TGLCPU-1",
    schema_version = 1,
    output_schema = "TGLCPU-TRACE-1",
    output_schema_version = 1,
    rom_sha256 = "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4",
    fceux_sha256 = "F89812F4E9506EF7090D9D0310D368ABD79BACA362B7BFC4A2E7E499754F2A1B",
    base_sha = "6d8f9c7a99a7ce188f1a523247d3a9b9093860fb",

    caps = {
        -- Empirical deterministic schedule capacity for this proof surface;
        -- not ASM/source-pinned gameplay semantics.
        max_frames = 4320,
        live_window_frames = 180,
        trace_rows = 8192,
        actor_rows = 2048,
        screenshots = 12,
        progress_period = 10,
        progress_publish_attempts = 3,
        tracked_text_bytes = 8 * 1024 * 1024,
        session_bytes = 64 * 1024 * 1024
    },

    resolution = { reference_width = 256, reference_height = 240 },
    mapper = { select = 0x8000, data = 0x8001 },
    -- The decompilation calls these mapped halves Bank04/05/06. The map gates
    -- the raw MMC3 8 KiB values observed by the mapper, not inferred names.
    raw_banks = {
        bank04 = { [0x08] = true, [0x09] = true },
        bank05 = { [0x0A] = true, [0x0B] = true },
        bank06 = { [0x0C] = true, [0x0D] = true }
    },

    ram = {
        offense_actor = 0x0308,
        defense_actor = 0x0309,
        offense_side = 0x030A,
        defense_side = 0x030B,
        control0 = 0x030C,
        control1 = 0x030D,
        team0 = 0x0765,
        team1 = 0x0766,
        orientation_035A = 0x035A,
        global_0373 = 0x0373,
        age_0094 = 0x0094,
        flag_0095 = 0x0095,
        flags_BA = 0x00BA,
        minute = 0x0357,
        second = 0x0358,
        shot_clock = 0x058A,
        actor_x_lo = 0x0073,
        actor_x_hi = 0x00E8,
        actor_y = 0x00F3,
        actor_stream_lo = 0x0547,
        actor_stream_hi = 0x0551,
        actor_state = 0x057C,
        actor_state_046E = 0x046E,
        actor_04B0 = 0x04B0,
        fixed_link = 0x06CB,
        target_c8 = 0x00C8,
        target_c9 = 0x00C9,
        target_ca = 0x00CA,
        target_cb = 0x00CB,
        scratch_0790 = 0x0790,
        scratch_0791 = 0x0791,
        scratch_0792 = 0x0792,
        state_058A = 0x058A,
        state_0357 = 0x0357,
        state_0358 = 0x0358,
        flags_007E = 0x007E,
        aggregation_06DF = 0x06DF,
        aggregation_06E0 = 0x06E0,
        aggregation_06E1 = 0x06E1
    },

    command = {
        base_cpu = 0x9F2E,
        end_cpu = 0xAC75,
        resume_cpu = 0xAC76,
        record_size = 5,
        record_count = 680,
        fetch_cpu = 0x8B90,
        reader_trampoline = 0xC006,
        reader_cpu = 0xCBE0,
        copied_bytes = { 0x00C7, 0x00C8, 0x00C9, 0x00CA, 0x00CB },
        opcode_load_cpu = 0x8BA2,
        dispatch_cpu = 0x8BAE,
        advance_cpu = 0x8FD9,
        rewind_cpu = 0x8FE8,
        -- These are static ROM table anchors, not executable hook addresses.
        handler_table_low = 0x8BB1,
        handler_table_high = 0x8BC9,
        handler_cpu = {
            0x90E0,0x934B,0x9280,0x905E,0x8FFA,0x8F92,
            0x8F2D,0x8F12,0x8ED7,0x8FC5,0x8CD0,0x8C40,
            0x8E4F,0x9125,0x9146,0x9172,0x9085,0x8C1A,
            0x8C1A,0x8C1A,0x9032,0x8BF6,0x8BE1,0x8F72
        },
        opcode_histogram = {
            98,143,150,171,2,1,1,2,8,12,1,2,1,2,2,2,
            2,64,0,0,2,6,7,1
        },
        decomp_comment_note =
            "Canonical ROM bytes put LDA $0547,X at $8B90, JSR $C006 at $8B9F, LDY $C7 at $8BA2, JMP ($00A4) at $8BAE, and tables at $8BB1/$8BC9. C-0019 inline comments drift two bytes early through $8B90-$8BAE; those comments are not hook authority."
    },

    fixed_startup = {
        link_cpu = 0x06CB,
        links = { 5,6,7,8,9,0,1,2,3,4 },
        primary = 4,
        defender = 9,
        matchup_seed = { 2, 7 }
    },

    formation = {
        selector_start = 0x938B,
        selector_end = 0x9620,
        theoretical_starts = 48,
        source_pinned_starts = 46,
        rejected_indices = { 46, 47 }
    },

    route = {
        selector_start = 0x96B6,
        selector_end = 0x9708,
        table_cpu = 0x9709,
        table_bytes = { 0x00, 0x80 },
        short_offset = 0x007D,
        long_offset = 0x00D7,
        controller_slots = 2
    },

    shot_request = {
        predicate_start = 0x8431,
        predicate_end = 0x8475,
        success_jump = 0x9217,
        difficulty_table = { 0x12, 0x1C, 0x28 },
        outcome_deferred = true
    },

    -- Every entry carries exact address confidence and an explicit bounded
    -- semantic-label confidence. Descriptive labels do not imply exact intent.
    hooks = {
        source_hook(0xACD9, "fixed_link_store", "bank04", nil, "exact_mechanics"),
        source_hook(0xACE4, "startup_primary_seed", "bank04", nil, "exact_mechanics"),
        source_hook(0xACF0, "startup_seed_tail", "bank04", nil, "exact_mechanics"),
        source_hook(0x96B6, "route_selector_entry", "bank05", nil, "exact_mechanics"),
        source_hook(0x96EC, "route_compare_table", "bank05", nil, "exact_mechanics"),
        source_hook(0x96F9, "route_short_long_branch", "bank05", nil, "exact_mechanics"),
        source_hook(0x9703, "route_store_state4", "bank05", nil, "exact_mechanics"),

        -- The following canonical ROM addresses are independently re-read;
        -- $8BB1/$8BC9 remain static table data, not executable hooks.
        source_hook(0x8B90, "cpu_fetch_offset", "bank06", "fetch", "exact_mechanics"),
        source_hook(0x8B9F, "cpu_reader_trampoline_call", "bank06", "reader", "exact_mechanics"),
        source_hook(0x8BA2, "cpu_dispatch_opcode_load", "bank06", "opcode", "exact_mechanics"),
        source_hook(0x8BAE, "cpu_dispatch_indirect_jump", "bank06", "dispatch", "exact_mechanics"),
        source_hook(0x827E, "cpu_state4_loop_boundary", "bank06", nil, "exact_mechanics"),
        source_hook(0x8BE1, "opcode_22_handler", "bank06", "handler", "exact_opcode_entry"),
        source_hook(0x8FD9, "cpu_record_advance", "bank06", "advance", "exact_mechanics"),
        source_hook(0x8FE8, "cpu_record_rewind", "bank06", "rewind", "exact_mechanics"),
        source_hook(0x8C40, "fixed_link_pose_entry", "bank06", "handler", "exact_opcode_entry"),
        source_hook(0x8CD0, "fixed_link_proximity_entry", "bank06", "handler", "exact_opcode_entry"),
        source_hook(0x8CFA, "fixed_link_box_check", "bank06", nil, "deferred_mechanics"),
        source_hook(0x8D64, "fixed_link_target_build", "bank06", nil, "deferred_mechanics"),
        source_hook(0x8E4F, "fixed_link_followup_entry", "bank06", "handler", "exact_opcode_entry"),
        source_hook(0x8EA0, "target_gate_helper_a", "bank06", nil, "deferred_mechanics"),
        source_hook(0x8EB5, "target_gate_helper_b", "bank06", nil, "deferred_mechanics"),
        source_hook(0x92D4, "target_apply_guard", "bank06", nil, "deferred_mechanics"),
        source_hook(0x92E9, "target_apply_tail", "bank06", nil, "deferred_mechanics"),
        source_hook(0x92FE, "target_direction_jump", "bank06", nil, "deferred_mechanics"),
        source_hook(0xB081, "candidate_scan_entry", "bank06", nil, "inferred_label"),
        source_hook(0xB10F, "candidate_filter_entry", "bank06", nil, "inferred_label"),
        source_hook(0xB32C, "candidate_target_helper", "bank06", nil, "inferred_label"),
        source_hook(0x9172, "primary_switch_entry", "bank06", "handler", "exact_opcode_entry"),
        source_hook(0x918A, "primary_switch_gate", "bank06", nil, "inferred_label"),
        source_hook(0x91C2, "primary_switch_branch", "bank06", nil, "inferred_label"),
        source_hook(0x91CB, "primary_switch_store", "bank06", nil, "inferred_label"),
        source_hook(0x8431, "shot_request_predicate", "bank06", nil, "exact_mechanics"),
        source_hook(0x9217, "shot_request_success", "bank06", nil, "deferred_mechanics")
    },

    setup = {
        mode = 0x5B, screen = 0x0F, period = 1, orientation = 0,
        offense_side = 0, control0 = 0, control1 = 0
    },
    live = {
        mode = 0x5B, screen = 0x0F, period = 1, orientation = 0,
        offense_side = 0, defense_side = 1, control0 = 0, control1 = 0
    },
    tip = {
        deadline = 300,
        schedule = {
            { first_age = 30, last_age = 34, port = 1, buttons = { "A" } },
            { first_age = 35, last_age = 37, port = 1, buttons = { "A", "B" } },
            { first_age = 38, last_age = 55, port = 1, buttons = { "B" } }
        }
    },
    clock = { stopped_minute = 4, stopped_second = 0, stopped_shot_clock = 0x18 },
    reference_window = { post_live_delay = 24, frames = 120 },

    -- This is the accepted deterministic/authentic controller schedule. Its
    -- row timing is not claimed as ASM/source-pinned input semantics.
    boot_inputs = {
        { first = 2600, last = 4400, every = 120, width = 4, port = 1, button = "start", until_setup = true },
        { first = 2900, last = 2906, port = 1, button = "A" },
        { first = 2941, last = 2945, port = 1, button = "down" },
        { first = 2953, last = 2957, port = 1, button = "down" },
        { first = 2965, last = 2971, port = 1, button = "A" },
        { first = 3087, last = 3091, port = 1, button = "down" },
        { first = 3116, last = 3123, port = 1, button = "A" },
        { first = 3201, last = 3205, port = 1, button = "right" },
        { first = 3229, last = 3235, port = 1, button = "A" },
        { first = 3321, last = 3325, port = 2, button = "up" },
        { first = 3341, last = 3347, port = 2, button = "A" },
        { first = 3402, last = 3406, port = 2, button = "right" },
        { first = 3413, last = 3418, port = 2, button = "right" },
        { first = 3424, last = 3429, port = 2, button = "right" },
        { first = 3434, last = 3438, port = 2, button = "right" },
        { first = 3445, last = 3451, port = 2, button = "A" }
    }
}

return map
