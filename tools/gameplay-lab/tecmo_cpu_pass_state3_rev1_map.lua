-- Read-only CPU pass-state-3 trace map for canonical Tecmo NBA Basketball Rev1.
-- This is a source-research map, not a native-runtime or asset-pack contract.

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
    schema = "TGLPASS3-1",
    schema_version = 1,
    output_schema = "TGLPASS3-TRACE-1",
    output_schema_version = 1,
    rom_sha256 = "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4",
    fceux_sha256 = "F89812F4E9506EF7090D9D0310D368ABD79BACA362B7BFC4A2E7E499754F2A1B",
    base_sha = "5a750ed7af05b18058ff1c2bc0c048118758475a",

    -- Fixed, conservative research limits. The runner deliberately exposes no
    -- command-line way to expand this natural-input observation window.
    caps = {
        max_frames = 6200,
        max_live_frames = 1800,
        trace_rows = 12000,
        actor_rows = 44,
        screenshots = 2,
        sequence_events = 128,
        ring_events = 64,
        chain_settle_frames = 240,
        progress_period = 10,
        progress_publish_attempts = 3,
        tracked_text_bytes = 8 * 1024 * 1024,
        session_bytes = 64 * 1024 * 1024
    },

    mapper = { select = 0x8000, data = 0x8001 },
    -- Raw MMC3 8 KiB values, not inferred bank names. $A000-$BFFF is R7.
    raw_banks = {
        bank05 = { [0x0A] = true, [0x0B] = true },
        bank06 = { [0x0C] = true, [0x0D] = true },
        fixed = { [0x0F] = true }
    },

    ram = {
        mode = 0x0055,
        screen = 0x0087,
        menu_selection = 0x00E2,
        menu_phase = 0x001F,
        menu_option_28 = 0x0028,
        menu_option_29 = 0x0029,
        period = 0x035C,
        minute = 0x0357,
        second = 0x0358,
        shot_clock = 0x058A,
        offense_actor = 0x0308,
        defense_actor = 0x0309,
        offense_side = 0x030A,
        defense_side = 0x030B,
        control0 = 0x030C,
        control1 = 0x030D,
        side_holder_0e = 0x000E,
        candidate_037f = 0x037F,
        orientation = 0x035A,
        team0 = 0x0765,
        team1 = 0x0766,
        foul_route = 0x05A1,
        violation_route = 0x0742,
        actor_x_lo = 0x0073,
        actor_x_hi = 0x00E8,
        actor_y = 0x00F3,
        actor_state_046e = 0x046E,
        object_slot10_state = 0x0478,
        actor_altitude_lo = 0x0484,
        actor_altitude_hi = 0x048F,
        actor_altitude_velocity_lo = 0x049A,
        actor_altitude_velocity_hi = 0x04A5,
        actor_horizontal_velocity_lo = 0x04E7,
        actor_horizontal_velocity_hi = 0x04F2,
        actor_vertical_velocity_lo = 0x04FD,
        actor_vertical_velocity_hi = 0x0508,
        saved_object_horizontal_velocity_lo = 0x038D,
        saved_object_horizontal_velocity_hi = 0x038E,
        saved_object_vertical_velocity_lo = 0x038F,
        saved_object_vertical_velocity_hi = 0x0390,
        flight_target_x_lo = 0x0094,
        flight_target_x_hi = 0x0095,
        flight_target_y_lo = 0x0096,
        flight_target_y_hi = 0x0097,
        object_slot10_count_lo = 0x051D,
        object_slot10_count_hi = 0x0528
    },

    setup = {
        mode = 0x5B,
        screen = 0x0F,
        period = 1,
        orientation = 0,
        human_side = 0,
        automatic_side = 1,
        control_human = 0,
        control_automatic = 1
    },
    clock = { stopped_minute = 4, stopped_second = 0, stopped_shot_clock = 0x18 },

    -- The only input schedule. It selects one human side and one automatic
    -- side, chooses distinct teams, then supplies the existing bounded tip
    -- start cadence. All live observation frames are neutral on both ports.
    boot_inputs = {
        { first = 2600, last = 4400, every = 120, width = 4, port = 1, button = "start", until_setup = true },
        { first = 2900, last = 2906, port = 1, button = "A" },
        -- The title mode cursor starts at MAN VS COM. Do not move it before
        -- confirming at frame 2965; the old MAN VS MAN profile moves down.
        { first = 2965, last = 2971, port = 1, button = "A" },
        { first = 3087, last = 3091, port = 1, button = "down" },
        { first = 3116, last = 3123, port = 1, button = "A" },
        { first = 3201, last = 3205, port = 1, button = "right" },
        { first = 3229, last = 3235, port = 1, button = "A" },
        -- In MAN VS COM, the human port chooses the second team as well;
        -- the P2 cadence is only for the MAN VS MAN profile.
        { first = 3321, last = 3325, port = 1, button = "up" },
        { first = 3341, last = 3347, port = 1, button = "A" },
        { first = 3402, last = 3406, port = 1, button = "right" },
        { first = 3413, last = 3418, port = 1, button = "right" },
        { first = 3424, last = 3429, port = 1, button = "right" },
        { first = 3434, last = 3438, port = 1, button = "right" },
        { first = 3445, last = 3451, port = 1, button = "A" },
        -- The one-player second selector enters its final confirm phase after
        -- the MAN VS MAN cadence's last edge; this is the single fixed
        -- follow-up acknowledgement, not a retry loop.
        { first = 3535, last = 3541, port = 1, button = "A" },
        -- Confirm the displayed default team on the one-player team grid.
        { first = 3650, last = 3656, port = 1, button = "A" }
    },
    tip = {
        deadline = 360,
        schedule = {
            { first_age = 30, last_age = 34, port = 1, buttons = { "A" } },
            { first_age = 35, last_age = 37, port = 1, buttons = { "A", "B" } },
            { first_age = 38, last_age = 55, port = 1, buttons = { "B" } }
        }
    },

    -- Fixed loop uses $F024 entry and $F059 backedge. $F058 is the final
    -- operand byte of JSR $C711, not an executable instruction boundary.
    hooks = {
        source_hook(0xF024, "fixed_slot10_loop_entry", "fixed", "loop", "exact_mechanics"),
        source_hook(0xF059, "fixed_slot10_loop_backedge", "fixed", "loop", "exact_mechanics"),
        source_hook(0xA214, "slot10_state_dispatch_A214", "bank05", "chain", "exact_mechanics"),
        source_hook(0xB074, "slot10_state3_consume_B074", "bank05", "chain", "exact_mechanics"),
        source_hook(0xB1E7, "slot10_state4_settle_entry_B1E7", "bank05", "chain", "exact_mechanics"),
        source_hook(0xB228, "slot10_state4_settle_gate_B228", "bank05", "chain", "exact_mechanics"),
        source_hook(0xB24F, "slot10_receiver_settle_B24F", "bank05", "chain", "exact_mechanics"),
        source_hook(0xB500, "slot10_state_countdown_B500", "bank05", "chain", "exact_mechanics"),

        -- These anchors can rule out a state-3-to-B24F pass-route result;
        -- their basketball labels remain deliberately deferred.
        source_hook(0x8C57, "alternate_8C57", "bank05", "alternate", "deferred_mechanics"),
        source_hook(0x8C7D, "alternate_8C7D", "bank05", "alternate", "deferred_mechanics"),
        source_hook(0x91BC, "alternate_91BC", "bank05", "alternate", "deferred_mechanics"),
        source_hook(0xAD4E, "alternate_AD4E", "bank05", "alternate", "deferred_mechanics"),
        source_hook(0xB100, "alternate_B100", "bank05", "alternate", "deferred_mechanics"),
        source_hook(0xB87C, "alternate_B87C", "bank05", "alternate", "deferred_mechanics"),
        source_hook(0x985B, "alternate_985B", "bank05", "alternate", "deferred_mechanics"),
        source_hook(0xB8F6, "alternate_B8F6", "bank05", "alternate", "deferred_mechanics")
    }
}

return map
