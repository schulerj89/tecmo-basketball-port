-- Canonical read-only address and hook map for the Rev 1 gameplay laboratory.
-- This module describes one bounded MAN VS MAN, orientation-0 shooting pilot.

local map = {
    schema = "TGLM-2",
    schema_version = 2,
    rom_sha256 = "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4",
    fceux_sha256 = "F89812F4E9506EF7090D9D0310D368ABD79BACA362B7BFC4A2E7E499754F2A1B",
    supported_orientation = 0,
    supported_offense_side = 0,

    caps = {
        max_frames = 7200,
        phase_rows = 64,
        event_rows = 2048,
        shot_detail_rows = 1200,
        screenshots = 8,
        tracked_text_bytes = 32 * 1024 * 1024
    },

    ram = {
        mode = 0x0055,
        screen = 0x0087,
        p1_current = 0x0005,
        p2_current = 0x0006,
        miss_variant_selector = 0x006A,
        shot_subtype = 0x0099,
        shot_flags = 0x00BA,
        offense_actor = 0x0308,
        defense_actor = 0x0309,
        offense_side = 0x030A,
        defense_side = 0x030B,
        control0 = 0x030C,
        control1 = 0x030D,
        orientation = 0x035A,
        period = 0x035C,
        minute = 0x0357,
        second = 0x0358,
        close_mode = 0x038C,
        shot_clock = 0x058A,
        action_gate = 0x0587,
        foul_route = 0x05A1,
        violation_route = 0x0742,
        team0 = 0x0765,
        team1 = 0x0766,
        score0_lo = 0x075B,
        score0_hi = 0x075C,
        score1_lo = 0x075D,
        score1_hi = 0x075E,

        actor_x_lo = 0x0073,
        actor_x_hi = 0x00E8,
        actor_y = 0x00F3,
        actor_contact = 0x0435,
        actor_timer = 0x042A,
        actor_pose_lo = 0x0442,
        actor_pose_hi = 0x044D,
        actor_phase = 0x0458,
        actor_facing = 0x0463,
        actor_state = 0x046E,
        actor_altitude_lo = 0x0484,
        actor_altitude_hi = 0x048F,
        actor_altitude_velocity_lo = 0x049A,
        actor_altitude_velocity_hi = 0x04A5,
        actor_team_role = 0x04B0,
        actor_horizontal_velocity_lo = 0x04E7,
        actor_horizontal_velocity_hi = 0x04F2,
        actor_vertical_velocity_lo = 0x04FD,
        actor_vertical_velocity_hi = 0x0508,
        object_slot10_state = 0x0478,
        saved_object_horizontal_velocity_lo = 0x038D,
        saved_object_horizontal_velocity_hi = 0x038E,
        saved_object_vertical_velocity_lo = 0x038F,
        saved_object_vertical_velocity_hi = 0x0390
    },

    hoops = {
        [0] = { x = 0x00A0, y = 0x94 },
        [1] = { x = 0x0260, y = 0x94 }
    },
    mapper = { select = 0x8000, data = 0x8001 },

    -- Raw MMC3 8 KiB banks 0A/0B are the two mapped halves called Bank05 by
    -- the decompilation. Every gameplay hook below is rejected in other banks.
    bank05_raw = { [0x0A] = true, [0x0B] = true },
    bank06_raw = { [0x0C] = true, [0x0D] = true },
    miss_variants = {
        selector_mask = 0x03,
        targets = {
            [0] = 0xA708,
            [1] = 0xA7A9,
            [2] = 0xA8E9,
            [3] = 0xA708
        }
    },
    hooks = {
        { address = 0x8C57, name = "ball_release", gate = "bank05" },
        { address = 0x8C7D, name = "close_launch", gate = "bank05" },
        { address = 0x8ABD, name = "shot_classifier", gate = "bank05" },
        { address = 0x91BC, name = "shot_result", gate = "bank05" },
        { address = 0x933B, name = "decision_anchor", gate = "bank05" },
        { address = 0x942D, name = "terminal_make_bit7_clear", gate = "bank05" },
        { address = 0x9434, name = "terminal_miss_bit7_set", gate = "bank05" },
        { address = 0xA6EE, name = "miss_variant_dispatch", gate = "bank05" },
        { address = 0xA708, name = "miss_variant_0_or_3", gate = "bank05" },
        { address = 0xA7A9, name = "miss_variant_1", gate = "bank05" },
        { address = 0xA8E9, name = "miss_variant_2", gate = "bank05" },
        { address = 0xBA02, name = "score_apply", gate = "bank05" },
        { address = 0xB87C, name = "settlement", gate = "bank05" },
        { address = 0x8FAD, name = "possession_handoff", gate = "bank05" },
        -- $9C79 is observed on one route only and is deliberately diagnostic,
        -- never a universal launch requirement.
        { address = 0x9C79, name = "route_9c79_optional", gate = "bank05" },
        { address = 0x91CB, name = "defender_switch_store", gate = "bank06" }
    },

    live = { mode = 0x5B, tip_mode = 0x16, screen = 0x0F, period = 1 },
    shot_window = {
        x_min = 0x0164,
        x_max = 0x0170,
        y_min = 0x6C,
        y_max = 0x74,
        stable_frames = 12,
        hold_b_frames = 8,
        release_frame = 9,
        holder_distance = 24,
        -- Conservative native-bot policy, deliberately wider than the
        -- original strict contact box abs(dx)<12, abs(dy)<7.
        threat_front_x = 20,
        threat_y = 12,
        progress_deadline = 90,
        acquire_deadline = 480,
        position_deadline = 720,
        resolve_deadline = 900,
        tail_frames = 180
    },

    -- Authentic power-on timing proven for this revision and FCEUX build.
    -- It selects PRESEASON -> MAN VS MAN -> distinct teams.
    boot_inputs = {
        { first = 2600, last = 4400, every = 120, width = 4, port = 1, button = "start", until_root = true },
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
