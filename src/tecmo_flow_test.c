#include "tecmo_game.h"
#include "tecmo_intro_stage.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLOW_INTRO_ARENA_BANK04_HANDOFF_FRAME 540U
#define FLOW_INTRO_TITLE_STEP 6U
#define FLOW_INTRO_LICENSE_STEP 7U
#define FLOW_INTRO_ARENA_STEP 8U
#define FLOW_INTRO_TITLE_HANDOFF_FRAME 133U
#define FLOW_INTRO_LICENSE_HANDOFF_FRAME 277U

static const char *flow_mode_name(TecmoPlayMode mode)
{
    if (mode == TECMO_MODE_MAIN_MENU) {
        return "MAIN MENU";
    }
    if (mode == TECMO_MODE_TITLE_SCREEN) {
        return "TITLE SCREEN";
    }
    if (mode == TECMO_MODE_INTRO_PROBE) {
        return "INTRO LAB";
    }
    if (mode == TECMO_MODE_CHR_PLAYGROUND) {
        return "CHR PLAYGROUND";
    }
    if (mode == TECMO_MODE_FIRST_SPRITE) {
        return "INTRO OUTPUT";
    }
    if (mode == TECMO_MODE_PLAY_SETUP) {
        return "PLAY SETUP";
    }
    if (mode == TECMO_MODE_ROSTERS) {
        return "ROSTERS";
    }
    if (mode == TECMO_MODE_COURT) {
        return "COURT";
    }
    if (mode == TECMO_MODE_START_GAME_MENU) {
        return "START GAME MENU";
    }
    if (mode == TECMO_MODE_PRESEASON_MENU) {
        return "PRESEASON MENU";
    }
    return "UNKNOWN";
}

static void set_flow_test_message(char *dest, size_t dest_size, const char *text)
{
    if (dest == NULL || dest_size == 0U) {
        return;
    }
    if (text == NULL) {
        text = "";
    }
    (void)snprintf(dest, dest_size, "%s", text);
}

static void flow_step(TecmoRuntime *runtime, TecmoInput input)
{
    TecmoInput released;

    tecmo_runtime_update(runtime, &input);
    memset(&released, 0, sizeof(released));
    tecmo_runtime_update(runtime, &released);
}

static bool flow_expect_mode(TecmoRuntime *runtime,
                             TecmoPlayMode expected,
                             const char *label,
                             char *message,
                             size_t message_size)
{
    if (runtime->mode != expected) {
        char detail[128];
        (void)snprintf(detail,
                       sizeof(detail),
                       "%s expected mode %s got %s",
                       label,
                       flow_mode_name(expected),
                       flow_mode_name(runtime->mode));
        set_flow_test_message(message, message_size, detail);
        return false;
    }
    if (tecmo_cpu_ram_read(runtime->memory, 0x0000) != (uint8_t)expected) {
        char detail[128];
        (void)snprintf(detail,
                       sizeof(detail),
                       "%s watched RAM mode expected %u got %u",
                       label,
                       (unsigned)expected,
                       (unsigned)tecmo_cpu_ram_read(runtime->memory, 0x0000));
        set_flow_test_message(message, message_size, detail);
        return false;
    }
    return true;
}

static bool flow_finish_start_menu_exit(TecmoRuntime *runtime,
                                        TecmoPlayMode expected_mode,
                                        bool from_season,
                                        const char *label,
                                        char *message,
                                        size_t message_size)
{
    TecmoInput input;
    const TecmoStartGameMenuAsset *asset = &runtime->start_game_menu_asset;
    unsigned frame;
    if (runtime->mode != TECMO_MODE_START_GAME_MENU ||
        runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_EXIT ||
        runtime->start_game_menu_state.transition_frame != 0U ||
        runtime->start_game_menu_state.exit_from_season != from_season ||
        runtime->start_game_menu_state.pending_action == TECMO_START_GAME_MENU_ACTION_NONE) {
        set_flow_test_message(message, message_size, "start-game menu exit did not begin at frame zero");
        return false;
    }
    for (frame = 0U; frame < asset->exit_handoff_frame; ++frame) {
        unsigned expected_stage = frame >= asset->exit_black_frame
            ? 4U : (unsigned)(TECMO_START_GAME_MENU_PALETTE_STAGE_COUNT - 1U) -
                   frame / asset->exit_palette_step_frames;
        if (runtime->start_game_menu_state.transition_frame != frame ||
            tecmo_start_game_menu_palette_stage(asset,
                                                &runtime->start_game_menu_state) != expected_stage ||
            tecmo_start_game_menu_cursor_visible(asset,
                                                 &runtime->start_game_menu_state)) {
            set_flow_test_message(message, message_size, "start-game menu exit fade/cursor checkpoint mismatch");
            return false;
        }
        memset(&input, 0, sizeof(input));
        if (frame == 0U) {
            input.up = true;
            input.down = true;
            input.shoot = true;
            input.cancel = true;
        }
        tecmo_runtime_update(runtime, &input);
        if (frame + 1U < asset->exit_handoff_frame &&
            (runtime->mode != TECMO_MODE_START_GAME_MENU ||
             runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_EXIT ||
             runtime->start_game_menu_state.transition_frame != frame + 1U)) {
            set_flow_test_message(message, message_size, "start-game menu exit handed off before frame eleven");
            return false;
        }
    }
    return flow_expect_mode(runtime, expected_mode, label, message, message_size);
}

static bool flow_finish_popup_setup(TecmoRuntime *runtime,
                                    TecmoStartGameMenuPhase popup_phase,
                                    const char *label,
                                    char *message,
                                    size_t message_size)
{
    TecmoInput input;
    const TecmoStartGameMenuAsset *asset = &runtime->start_game_menu_asset;
    size_t overlay_index = popup_phase == TECMO_START_GAME_MENU_MUSIC ? 0U :
                           popup_phase == TECMO_START_GAME_MENU_SPEED ? 1U : 2U;
    unsigned height = asset->overlays[overlay_index].height;
    unsigned total = height * asset->popup_row_cadence +
                     (popup_phase == TECMO_START_GAME_MENU_PERIOD
                          ? asset->period_setup_extra_frames : 0U);
    unsigned frame;
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_POPUP_SETUP ||
        runtime->start_game_menu_state.popup_phase != popup_phase ||
        runtime->start_game_menu_state.transition_frame != 0U ||
        tecmo_start_game_menu_overlay_visible_rows(asset,
                                                   &runtime->start_game_menu_state) != 1U ||
        tecmo_start_game_menu_cursor_visible(asset,
                                             &runtime->start_game_menu_state)) {
        set_flow_test_message(message, message_size, "popup setup did not install its first cursorless row");
        return false;
    }
    for (frame = 1U; frame <= total; ++frame) {
        memset(&input, 0, sizeof(input));
        if (frame == 1U) {
            input.down = true;
            input.shoot = true;
        }
        tecmo_runtime_update(runtime, &input);
        if (frame < total) {
            unsigned expected_rows = frame + 1U > height ? height : frame + 1U;
            if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_POPUP_SETUP ||
                runtime->start_game_menu_state.transition_frame != frame ||
                tecmo_start_game_menu_overlay_visible_rows(asset,
                                                           &runtime->start_game_menu_state) != expected_rows ||
                tecmo_start_game_menu_cursor_visible(asset,
                                                     &runtime->start_game_menu_state)) {
                set_flow_test_message(message, message_size, "popup setup row checkpoint mismatch");
                return false;
            }
        }
    }
    if (runtime->start_game_menu_state.phase != popup_phase ||
        runtime->start_game_menu_state.transition_frame != total ||
        tecmo_start_game_menu_overlay_visible_rows(asset,
                                                   &runtime->start_game_menu_state) != height ||
        tecmo_start_game_menu_cursor_visible(asset,
                                             &runtime->start_game_menu_state) ||
        runtime->start_game_menu_state.cursor_delay != asset->cursor_commit_delay_frames ||
        runtime->start_game_menu_state.direction_cooldown != 4U) {
        set_flow_test_message(message, message_size, label);
        return false;
    }
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != popup_phase ||
        runtime->start_game_menu_state.transition_frame != 0U ||
        runtime->start_game_menu_state.cursor_delay != 0U ||
        !tecmo_start_game_menu_cursor_visible(asset,
                                              &runtime->start_game_menu_state) ||
        runtime->start_game_menu_state.direction_cooldown != 3U) {
        set_flow_test_message(message, message_size, "popup cursor did not reach OAM on the following frame");
        return false;
    }
    return true;
}

static bool flow_finish_popup_teardown(TecmoRuntime *runtime,
                                       TecmoStartGameMenuPhase popup_phase,
                                       const char *label,
                                       char *message,
                                       size_t message_size)
{
    TecmoInput input;
    const TecmoStartGameMenuAsset *asset = &runtime->start_game_menu_asset;
    size_t overlay_index = popup_phase == TECMO_START_GAME_MENU_MUSIC ? 0U :
                           popup_phase == TECMO_START_GAME_MENU_SPEED ? 1U : 2U;
    unsigned height = asset->overlays[overlay_index].height;
    unsigned frame;
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_POPUP_TEARDOWN ||
        runtime->start_game_menu_state.popup_phase != popup_phase ||
        runtime->start_game_menu_state.transition_frame != 0U ||
        tecmo_start_game_menu_overlay_visible_rows(asset,
                                                   &runtime->start_game_menu_state) != height ||
        tecmo_start_game_menu_cursor_visible(asset,
                                             &runtime->start_game_menu_state)) {
        set_flow_test_message(message, message_size, "popup teardown did not begin full and cursorless");
        return false;
    }
    for (frame = 1U; frame <= height; ++frame) {
        memset(&input, 0, sizeof(input));
        if (frame == 1U) {
            input.up = true;
            input.cancel = true;
        }
        tecmo_runtime_update(runtime, &input);
        if (frame < height &&
            (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_POPUP_TEARDOWN ||
             runtime->start_game_menu_state.transition_frame != frame ||
             tecmo_start_game_menu_overlay_visible_rows(asset,
                                                        &runtime->start_game_menu_state) != height - frame ||
             tecmo_start_game_menu_cursor_visible(asset,
                                                  &runtime->start_game_menu_state))) {
            set_flow_test_message(message, message_size, "popup teardown row checkpoint mismatch");
            return false;
        }
    }
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_ROOT ||
        runtime->start_game_menu_state.transition_frame != height ||
        tecmo_start_game_menu_overlay_visible_rows(asset,
                                                   &runtime->start_game_menu_state) != 0U ||
        tecmo_start_game_menu_cursor_visible(asset,
                                             &runtime->start_game_menu_state) ||
        runtime->start_game_menu_state.cursor_delay != asset->cursor_commit_delay_frames ||
        runtime->start_game_menu_state.direction_cooldown != 4U) {
        set_flow_test_message(message, message_size, label);
        return false;
    }
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_ROOT ||
        runtime->start_game_menu_state.transition_frame != 0U ||
        runtime->start_game_menu_state.cursor_delay != 0U ||
        !tecmo_start_game_menu_cursor_visible(asset,
                                              &runtime->start_game_menu_state) ||
        runtime->start_game_menu_state.direction_cooldown != 3U) {
        set_flow_test_message(message, message_size, "root cursor did not reach OAM after popup teardown");
        return false;
    }
    return true;
}

static bool flow_expect_menu_item(TecmoRuntime *runtime,
                                  size_t expected,
                                  const char *label,
                                  char *message,
                                  size_t message_size)
{
    if (runtime->selected_menu_item != expected) {
        char detail[128];
        (void)snprintf(detail,
                       sizeof(detail),
                       "%s expected menu item %u got %u",
                       label,
                       (unsigned)expected,
                       (unsigned)runtime->selected_menu_item);
        set_flow_test_message(message, message_size, detail);
        return false;
    }
    if (tecmo_cpu_ram_read(runtime->memory, 0x0003) != (uint8_t)expected) {
        char detail[128];
        (void)snprintf(detail,
                       sizeof(detail),
                       "%s watched RAM menu expected %u got %u",
                       label,
                       (unsigned)expected,
                       (unsigned)tecmo_cpu_ram_read(runtime->memory, 0x0003));
        set_flow_test_message(message, message_size, detail);
        return false;
    }
    return true;
}

static bool flow_press_menu_down(TecmoRuntime *runtime,
                                 size_t count,
                                 size_t expected,
                                 const char *label,
                                 char *message,
                                 size_t message_size)
{
    TecmoInput input;

    for (size_t i = 0; i < count; ++i) {
        memset(&input, 0, sizeof(input));
        input.down = true;
        flow_step(runtime, input);
    }
    return flow_expect_menu_item(runtime, expected, label, message, message_size);
}

static bool flow_wait_for_intro_step_advance(TecmoRuntime *runtime,
                                             uint8_t previous_step,
                                             size_t max_frames,
                                             const char *label,
                                             char *message,
                                             size_t message_size)
{
    TecmoInput input;

    memset(&input, 0, sizeof(input));
    for (size_t frame = 0; frame < max_frames; ++frame) {
        tecmo_runtime_update(runtime, &input);
        if (runtime->intro_output_step != previous_step) {
            return true;
        }
    }

    set_flow_test_message(message, message_size, label);
    return false;
}

static bool flow_hold_intro_step(TecmoRuntime *runtime,
                                 uint8_t expected_step,
                                 size_t frames,
                                 const char *label,
                                 char *message,
                                 size_t message_size)
{
    TecmoInput input;

    memset(&input, 0, sizeof(input));
    for (size_t frame = 0; frame < frames; ++frame) {
        tecmo_runtime_update(runtime, &input);
        if (runtime->intro_output_step != expected_step) {
            char detail[160];
            (void)snprintf(detail,
                           sizeof(detail),
                           "%s after %u intro frames",
                           label,
                           (unsigned)(frame + 1U));
            set_flow_test_message(message, message_size, detail);
            return false;
        }
    }
    return true;
}

static bool flow_expect_arena_bank04_handoff_frame(char *message, size_t message_size)
{
    TecmoIntroArenaTransitionState arena_state;

    tecmo_intro_arena_transition_state(FLOW_INTRO_ARENA_BANK04_HANDOFF_FRAME - 1U, &arena_state);
    if (arena_state.phase == TECMO_INTRO_ARENA_PHASE_WRAP) {
        set_flow_test_message(message, message_size, "arena Bank04 state wrapped before handoff frame");
        return false;
    }

    tecmo_intro_arena_transition_state(FLOW_INTRO_ARENA_BANK04_HANDOFF_FRAME, &arena_state);
    if (arena_state.phase != TECMO_INTRO_ARENA_PHASE_WRAP) {
        set_flow_test_message(message, message_size, "arena Bank04 state did not wrap at handoff frame");
        return false;
    }

    return true;
}

static bool flow_expect_opening_handoff_frames(TecmoRuntime *runtime,
                                               char *message,
                                               size_t message_size)
{
    TecmoInput input;
    memset(&input, 0, sizeof(input));

    tecmo_runtime_set_mode(runtime, TECMO_MODE_FIRST_SPRITE);
    runtime->intro_output_step = FLOW_INTRO_TITLE_STEP;
    runtime->mode_frame_counter = FLOW_INTRO_TITLE_HANDOFF_FRAME - 2U;
    tecmo_runtime_update(runtime, &input);
    if (runtime->intro_output_step != FLOW_INTRO_TITLE_STEP ||
        runtime->mode_frame_counter != FLOW_INTRO_TITLE_HANDOFF_FRAME - 1U) {
        set_flow_test_message(message, message_size, "title left before native frame 133");
        return false;
    }
    tecmo_runtime_update(runtime, &input);
    if (runtime->intro_output_step != FLOW_INTRO_LICENSE_STEP ||
        runtime->mode_frame_counter != 0U) {
        set_flow_test_message(message, message_size, "title did not hand off at native frame 133");
        return false;
    }

    runtime->mode_frame_counter = FLOW_INTRO_LICENSE_HANDOFF_FRAME - 2U;
    tecmo_runtime_update(runtime, &input);
    if (runtime->intro_output_step != FLOW_INTRO_LICENSE_STEP ||
        runtime->mode_frame_counter != FLOW_INTRO_LICENSE_HANDOFF_FRAME - 1U) {
        set_flow_test_message(message, message_size, "license left before native frame 277");
        return false;
    }
    tecmo_runtime_update(runtime, &input);
    if (runtime->intro_output_step != FLOW_INTRO_ARENA_STEP ||
        runtime->mode_frame_counter != 0U) {
        set_flow_test_message(message, message_size, "license did not hand off at native frame 277");
        return false;
    }

    tecmo_runtime_set_mode(runtime, TECMO_MODE_MAIN_MENU);
    return true;
}

static bool flow_expect_dual_controller_compatibility(TecmoRuntime *runtime,
                                                      char *message,
                                                      size_t message_size)
{
    TecmoInput player_one;
    TecmoInput player_two;

    memset(&player_one, 0, sizeof(player_one));
    memset(&player_two, 0, sizeof(player_two));
    tecmo_runtime_set_mode(runtime, TECMO_MODE_MAIN_MENU);
    runtime->selected_menu_item = 0U;
    player_two.down = true;
    tecmo_runtime_update_players(runtime, &player_one, &player_two);
    if (runtime->selected_menu_item != 0U ||
        runtime->previous_input.down ||
        !runtime->previous_player_two_input.down) {
        set_flow_test_message(message, message_size,
                              "player two input changed a player-one-only mode");
        return false;
    }

    tecmo_runtime_set_mode(runtime, TECMO_MODE_MAIN_MENU);
    if (runtime->previous_input.down || runtime->previous_player_two_input.down) {
        set_flow_test_message(message, message_size,
                              "mode reset did not clear both controller histories");
        return false;
    }

    runtime->selected_menu_item = 0U;
    player_one.down = true;
    tecmo_runtime_update(runtime, &player_one);
    if (runtime->selected_menu_item != 1U ||
        !runtime->previous_input.down ||
        runtime->previous_player_two_input.down) {
        set_flow_test_message(message, message_size,
                              "single-player update wrapper changed behavior");
        return false;
    }

    tecmo_runtime_set_mode(runtime, TECMO_MODE_MAIN_MENU);
    runtime->selected_menu_item = 0U;
    return true;
}

static void flow_preseason_neutral(TecmoRuntime *runtime, size_t frames)
{
    TecmoInput player_one = {0};
    TecmoInput player_two = {0};
    for (size_t frame = 0U; frame < frames; ++frame)
        tecmo_runtime_update_players(runtime, &player_one, &player_two);
}

static void flow_preseason_release(TecmoRuntime *runtime,
                                   bool player_two_owned,
                                   bool accept)
{
    TecmoInput player_one = {0};
    TecmoInput player_two = {0};
    TecmoInput *input = player_two_owned ? &player_two : &player_one;
    if (accept) input->shoot = true;
    else input->cancel = true;
    tecmo_runtime_update_players(runtime, &player_one, &player_two);
    memset(&player_one, 0, sizeof(player_one));
    memset(&player_two, 0, sizeof(player_two));
    tecmo_runtime_update_players(runtime, &player_one, &player_two);
}

static void flow_preseason_direction(TecmoRuntime *runtime,
                                     bool player_two_owned,
                                     TecmoControlButton button)
{
    TecmoInput player_one = {0};
    TecmoInput player_two = {0};
    TecmoInput *input = player_two_owned ? &player_two : &player_one;
    tecmo_input_set_button(input, button, true);
    tecmo_runtime_update_players(runtime, &player_one, &player_two);
    memset(&player_one, 0, sizeof(player_one));
    memset(&player_two, 0, sizeof(player_two));
    tecmo_runtime_update_players(runtime, &player_one, &player_two);
}

static bool flow_preseason_wait_cooldown(TecmoRuntime *runtime,
                                         char *message,
                                         size_t message_size)
{
    size_t frames = 0U;
    while (runtime->preseason_state.direction_cooldown != 0U && frames < 16U) {
        flow_preseason_neutral(runtime, 1U);
        ++frames;
    }
    if (runtime->preseason_state.direction_cooldown != 0U) {
        set_flow_test_message(message, message_size,
                              "preseason direction cooldown did not expire");
        return false;
    }
    return true;
}

static bool flow_preseason_unlock_selector(TecmoRuntime *runtime,
                                           const char *label,
                                           char *message,
                                           size_t message_size)
{
    uint16_t seed = runtime->preseason_asset.accepted_input_seed;
    uint16_t expected_after_first = seed == 0U ? 0U : (uint16_t)(seed - 1U);
    if (runtime->preseason_state.direction_cooldown != seed) {
        char detail[160];
        (void)snprintf(detail, sizeof(detail),
                       "%s selector gate expected %u got %u", label,
                       (unsigned)seed,
                       (unsigned)runtime->preseason_state.direction_cooldown);
        set_flow_test_message(message, message_size, detail);
        return false;
    }
    flow_preseason_neutral(runtime, 1U);
    if (runtime->preseason_state.direction_cooldown != expected_after_first) {
        char detail[160];
        (void)snprintf(detail, sizeof(detail),
                       "%s first interactive gate tick expected %u got %u", label,
                       (unsigned)expected_after_first,
                       (unsigned)runtime->preseason_state.direction_cooldown);
        set_flow_test_message(message, message_size, detail);
        return false;
    }
    return flow_preseason_wait_cooldown(runtime, message, message_size);
}

static bool flow_preseason_expect_phase(TecmoRuntime *runtime,
                                        TecmoPreseasonPhase phase,
                                        const char *label,
                                        char *message,
                                        size_t message_size)
{
    if (runtime->mode != TECMO_MODE_PRESEASON_MENU ||
        runtime->preseason_state.phase != phase) {
        char detail[160];
        (void)snprintf(detail, sizeof(detail), "%s expected %s got %s",
                       label, tecmo_preseason_phase_name(phase),
                       tecmo_preseason_phase_name(runtime->preseason_state.phase));
        set_flow_test_message(message, message_size, detail);
        return false;
    }
    return true;
}

static bool flow_preseason_invalid_state_guards(TecmoRuntime *runtime,
                                                char *message,
                                                size_t message_size)
{
    const uint32_t sentinel = 0xA55AA55AU;
    const size_t pixel_count = 256U * 240U;
    TecmoFramebuffer framebuffer;
    TecmoPreseasonState states[9];
    uint32_t *pixels = (uint32_t *)malloc(pixel_count * sizeof(*pixels));
    if (pixels == NULL) {
        set_flow_test_message(message, message_size,
                              "preseason invalid-state framebuffer allocation failed");
        return false;
    }
    for (size_t i = 0U; i < pixel_count; ++i) pixels[i] = sentinel;
    framebuffer.pixels = pixels;
    framebuffer.width = 256;
    framebuffer.height = 240;
    framebuffer.pitch_pixels = 256;
    for (size_t i = 0U; i < 9U; ++i)
        tecmo_preseason_state_init(&states[i]);
    states[0].phase = (TecmoPreseasonPhase)99;
    states[1].phase = (TecmoPreseasonPhase)-1;
    states[2].team_exit_target = (TecmoPreseasonTeamExitTarget)-1;
    states[3].active_player = 2U;
    states[4].division_selection[0] = 4U;
    states[5].difficulty_selection = 3U;
    states[6].committed_difficulty = 3U;
    states[7].phase = TECMO_PRESEASON_TEAM;
    states[7].transition_frame = runtime->preseason_asset.team_input_ready_frames;
    states[7].team_selection[0] =
        runtime->preseason_asset.division_counts[0];
    states[8].phase = TECMO_PRESEASON_TEAM_EXIT;
    states[8].transition_frame =
        (uint16_t)(runtime->preseason_asset.team_exit_frames + 1U);
    for (size_t state_index = 0U; state_index < 9U; ++state_index) {
        TecmoPreseasonState before = states[state_index];
        if (tecmo_preseason_draw(&framebuffer, &runtime->preseason_asset,
                                 &states[state_index],
                                 &runtime->start_game_menu_asset,
                                 runtime->title_chr_bytes,
                                 runtime->title_chr_byte_count,
                                 0, 0, 1)) {
            free(pixels);
            set_flow_test_message(message, message_size,
                                  "preseason draw accepted an invalid state");
            return false;
        }
        for (size_t i = 0U; i < pixel_count; ++i) {
            if (pixels[i] != sentinel) {
                free(pixels);
                set_flow_test_message(message, message_size,
                                      "preseason invalid-state draw changed output");
                return false;
            }
        }
        (void)tecmo_preseason_update(&states[state_index],
                                     &runtime->preseason_asset,
                                     &(TecmoControlFrame){0},
                                     &(TecmoControlFrame){0});
        if (memcmp(&states[state_index], &before, sizeof(before)) != 0) {
            free(pixels);
            set_flow_test_message(message, message_size,
                                  "preseason update mutated an invalid state");
            return false;
        }
    }
    {
        static const int invalid_pitches[4] = {0, -1, 255, 256};
        TecmoPreseasonState valid_control;
        tecmo_preseason_state_init(&valid_control);
        valid_control.phase = TECMO_PRESEASON_CONTROL;
        valid_control.transition_frame = runtime->preseason_asset.overlays[0].height;
        for (size_t case_index = 0U; case_index < 4U; ++case_index) {
            int scale = case_index == 3U ? INT_MAX : 1;
            framebuffer.pitch_pixels = invalid_pitches[case_index];
            if (tecmo_preseason_draw(&framebuffer, &runtime->preseason_asset,
                                     &valid_control,
                                     &runtime->start_game_menu_asset,
                                     runtime->title_chr_bytes,
                                     runtime->title_chr_byte_count,
                                     0, 0, scale)) {
                free(pixels);
                set_flow_test_message(message, message_size,
                                      "preseason draw accepted an invalid viewport");
                return false;
            }
            for (size_t i = 0U; i < pixel_count; ++i) {
                if (pixels[i] != sentinel) {
                    free(pixels);
                    set_flow_test_message(message, message_size,
                                          "preseason invalid viewport changed output");
                    return false;
                }
            }
        }
    }
    free(pixels);
    return true;
}

static bool flow_expect_preseason_native_path(TecmoRuntime *runtime,
                                              char *message,
                                              size_t message_size)
{
    TecmoInput player_one = {0};
    TecmoInput player_two = {0};
    const TecmoPreseasonAsset *asset = &runtime->preseason_asset;
    if (!asset->available ||
        !flow_preseason_expect_phase(runtime, TECMO_PRESEASON_CONTROL_SETUP,
                                     "preseason dispatch", message, message_size) ||
        runtime->preseason_state.transition_frame != 0U)
        return false;

    if (runtime->preseason_state.direction_cooldown !=
            asset->accepted_input_seed) {
        set_flow_test_message(message, message_size,
                              "preseason initial setup gate was not seeded");
        return false;
    }
    flow_preseason_neutral(runtime, asset->overlays[0].height - 1U);
    if (runtime->preseason_state.phase != TECMO_PRESEASON_CONTROL_SETUP ||
        runtime->preseason_state.direction_cooldown !=
            asset->accepted_input_seed) {
        set_flow_test_message(message, message_size,
                              "preseason CONTROL setup consumed its frozen gate");
        return false;
    }
    flow_preseason_neutral(runtime, 1U);
    if (!flow_preseason_expect_phase(runtime, TECMO_PRESEASON_CONTROL,
                                      "control setup", message, message_size) ||
        runtime->preseason_state.cursor_delay != asset->cursor_commit_delay_frames ||
        runtime->preseason_state.direction_cooldown != asset->accepted_input_seed)
        return false;
    flow_preseason_neutral(runtime, 1U);
    if (runtime->preseason_state.direction_cooldown !=
            (uint16_t)(asset->accepted_input_seed - 1U)) {
        set_flow_test_message(message, message_size,
                              "preseason first CONTROL frame did not tick its gate");
        return false;
    }

    player_one.confirm = true;
    player_one.tab = true;
    tecmo_runtime_update_players(runtime, &player_one, &player_two);
    memset(&player_one, 0, sizeof(player_one));
    tecmo_runtime_update_players(runtime, &player_one, &player_two);
    if (runtime->preseason_state.phase != TECMO_PRESEASON_CONTROL ||
        runtime->preseason_state.control_selection != 0U ||
        runtime->preseason_state.direction_cooldown !=
            (uint16_t)(asset->accepted_input_seed - 3U)) {
        set_flow_test_message(message, message_size,
                              "preseason START/SELECT were not ignored");
        return false;
    }
    if (!flow_preseason_wait_cooldown(runtime, message, message_size)) return false;

    {
        TecmoPreseasonState ownership_state;
        TecmoControlFrame p1_controls = {0};
        TecmoControlFrame p2_controls = {0};
        tecmo_preseason_state_init(&ownership_state);
        ownership_state.phase = TECMO_PRESEASON_DIVISION;
        ownership_state.control_selection = 1U;
        ownership_state.active_player = 1U;
        p1_controls.held.down = true;
        p2_controls.held.up = true;
        (void)tecmo_preseason_update(&ownership_state, asset,
                                     &p1_controls, &p2_controls);
        if (ownership_state.division_selection[1] != 1U) {
            set_flow_test_message(message, message_size,
                                  "non-MAN-VS-MAN P2 selection was not pad1-owned");
            return false;
        }
    }

    player_one.up = true;
    player_one.down = true;
    tecmo_runtime_update_players(runtime, &player_one, &player_two);
    if (runtime->preseason_state.control_selection != 0U ||
        runtime->preseason_state.direction_cooldown != 7U) {
        set_flow_test_message(message, message_size,
                              "preseason CONTROL Up+Down did not consume its gate");
        return false;
    }
    memset(&player_one, 0, sizeof(player_one));
    tecmo_runtime_update_players(runtime, &player_one, &player_two);
    if (!flow_preseason_wait_cooldown(runtime, message, message_size)) return false;

    flow_preseason_release(runtime, false, true);
    if (!flow_preseason_expect_phase(runtime, TECMO_PRESEASON_DIFFICULTY_SETUP,
                                      "difficulty open", message, message_size) ||
        runtime->preseason_state.difficulty_selection != 0U ||
        runtime->preseason_state.direction_cooldown != asset->accepted_input_seed)
        return false;
    flow_preseason_neutral(runtime, asset->overlays[2].height - 1U);
    if (runtime->preseason_state.phase != TECMO_PRESEASON_DIFFICULTY_SETUP ||
        runtime->preseason_state.direction_cooldown != asset->accepted_input_seed) {
        set_flow_test_message(message, message_size,
                              "preseason DIFFICULTY setup consumed its frozen gate");
        return false;
    }
    flow_preseason_neutral(runtime, 1U);
    if (!flow_preseason_expect_phase(runtime, TECMO_PRESEASON_DIFFICULTY,
                                     "difficulty setup", message, message_size) ||
        !flow_preseason_unlock_selector(runtime, "DIFFICULTY", message,
                                        message_size))
        return false;
    flow_preseason_direction(runtime, false, TECMO_CONTROL_DOWN);
    if (runtime->preseason_state.difficulty_selection != 1U ||
        !flow_preseason_wait_cooldown(runtime, message, message_size))
        return false;
    flow_preseason_release(runtime, false, true);
    if (runtime->preseason_state.committed_difficulty != 1U ||
        runtime->preseason_state.control_selection != 1U ||
        !flow_preseason_expect_phase(runtime, TECMO_PRESEASON_DIFFICULTY_TEARDOWN,
                                     "difficulty accept", message, message_size))
        return false;
    flow_preseason_neutral(runtime, asset->overlays[2].height);
    if (!flow_preseason_expect_phase(runtime, TECMO_PRESEASON_CONTROL,
                                     "difficulty teardown", message, message_size) ||
        !flow_preseason_unlock_selector(runtime, "CONTROL after difficulty",
                                        message, message_size))
        return false;

    flow_preseason_direction(runtime, false, TECMO_CONTROL_UP);
    if (runtime->preseason_state.control_selection != 0U ||
        !flow_preseason_wait_cooldown(runtime, message, message_size))
        return false;
    flow_preseason_release(runtime, false, true);
    if (runtime->preseason_state.difficulty_selection != 1U) {
        set_flow_test_message(message, message_size,
                              "difficulty reopen did not seed committed value");
        return false;
    }
    flow_preseason_neutral(runtime, asset->overlays[2].height);
    if (!flow_preseason_expect_phase(runtime, TECMO_PRESEASON_DIFFICULTY,
                                     "difficulty reopen setup", message,
                                     message_size) ||
        !flow_preseason_unlock_selector(runtime, "reopened DIFFICULTY",
                                        message, message_size))
        return false;
    flow_preseason_direction(runtime, false, TECMO_CONTROL_DOWN);
    if (runtime->preseason_state.difficulty_selection != 2U ||
        !flow_preseason_wait_cooldown(runtime, message, message_size))
        return false;
    flow_preseason_release(runtime, false, false);
    if (runtime->preseason_state.difficulty_selection != 1U ||
        runtime->preseason_state.committed_difficulty != 1U ||
        runtime->preseason_state.control_selection != 0U)
        return false;
    flow_preseason_neutral(runtime, asset->overlays[2].height);
    if (!flow_preseason_expect_phase(runtime, TECMO_PRESEASON_CONTROL,
                                     "difficulty cancel teardown", message,
                                     message_size) ||
        !flow_preseason_unlock_selector(runtime, "CONTROL after cancel",
                                        message, message_size))
        return false;

    flow_preseason_direction(runtime, false, TECMO_CONTROL_DOWN);
    if (!flow_preseason_wait_cooldown(runtime, message, message_size)) return false;
    flow_preseason_direction(runtime, false, TECMO_CONTROL_DOWN);
    if (runtime->preseason_state.control_selection != 2U ||
        !flow_preseason_wait_cooldown(runtime, message, message_size))
        return false;
    flow_preseason_release(runtime, false, true);
    flow_preseason_neutral(runtime, asset->overlays[1].height);
    if (!flow_preseason_expect_phase(runtime, TECMO_PRESEASON_DIVISION,
                                     "P1 division setup", message, message_size) ||
        !flow_preseason_unlock_selector(runtime, "P1 DIVISION", message,
                                        message_size))
        return false;

    player_one.up = true;
    player_one.down = true;
    tecmo_runtime_update_players(runtime, &player_one, &player_two);
    if (runtime->preseason_state.division_selection[0] != 0U ||
        runtime->preseason_state.direction_cooldown != 7U)
        return false;
    memset(&player_one, 0, sizeof(player_one));
    tecmo_runtime_update_players(runtime, &player_one, &player_two);
    if (!flow_preseason_wait_cooldown(runtime, message, message_size)) return false;
    player_two.down = true;
    tecmo_runtime_update_players(runtime, &player_one, &player_two);
    memset(&player_two, 0, sizeof(player_two));
    tecmo_runtime_update_players(runtime, &player_one, &player_two);
    if (runtime->preseason_state.division_selection[0] != 0U) {
        set_flow_test_message(message, message_size,
                              "P2 changed a P1-owned division menu");
        return false;
    }
    flow_preseason_direction(runtime, false, TECMO_CONTROL_DOWN);
    if (runtime->preseason_state.division_selection[0] != 1U ||
        !flow_preseason_wait_cooldown(runtime, message, message_size))
        return false;
    flow_preseason_release(runtime, false, true);
    if (runtime->preseason_state.team_selection[0] != 0U ||
        !flow_preseason_expect_phase(runtime, TECMO_PRESEASON_TEAM_ENTRY,
                                     "P1 Central team entry", message, message_size))
        return false;
    flow_preseason_neutral(runtime, asset->team_input_ready_frames - 1U);
    if (runtime->preseason_state.phase != TECMO_PRESEASON_TEAM_ENTRY ||
        runtime->preseason_state.direction_cooldown != asset->accepted_input_seed) {
        set_flow_test_message(message, message_size,
                              "preseason TEAM entry consumed its frozen gate");
        return false;
    }
    flow_preseason_neutral(runtime, 1U);
    if (!flow_preseason_expect_phase(runtime, TECMO_PRESEASON_TEAM,
                                     "P1 Central team", message, message_size) ||
        runtime->preseason_state.direction_cooldown != asset->accepted_input_seed)
        return false;
    player_two.left = true;
    tecmo_runtime_update_players(runtime, &player_one, &player_two);
    memset(&player_two, 0, sizeof(player_two));
    tecmo_runtime_update_players(runtime, &player_one, &player_two);
    if (runtime->preseason_state.team_selection[0] != 0U) return false;
    if (!flow_preseason_wait_cooldown(runtime, message, message_size)) return false;
    flow_preseason_direction(runtime, false, TECMO_CONTROL_LEFT);
    if (runtime->preseason_state.team_selection[0] != 6U ||
        !flow_preseason_wait_cooldown(runtime, message, message_size))
        return false;
    flow_preseason_release(runtime, false, false);
    if (runtime->preseason_state.direction_cooldown != asset->accepted_input_seed)
        return false;
    flow_preseason_neutral(runtime, asset->team_exit_frames - 1U);
    if (runtime->preseason_state.phase != TECMO_PRESEASON_TEAM_EXIT ||
        runtime->preseason_state.direction_cooldown != asset->accepted_input_seed) {
        set_flow_test_message(message, message_size,
                              "preseason TEAM exit consumed its frozen gate");
        return false;
    }
    flow_preseason_neutral(runtime, 1U);
    if (!flow_preseason_expect_phase(runtime, TECMO_PRESEASON_DIVISION_SETUP,
                                      "P1 team B division rebuild", message, message_size) ||
        !runtime->preseason_state.division_return_fade_active ||
        runtime->preseason_state.direction_cooldown != asset->accepted_input_seed)
        return false;
    flow_preseason_neutral(runtime, asset->overlays[1].height);
    if (!flow_preseason_expect_phase(runtime, TECMO_PRESEASON_DIVISION,
                                      "P1 division return", message, message_size) ||
        runtime->preseason_state.division_return_fade_frame != 0U)
        return false;
    if (!flow_preseason_unlock_selector(runtime, "P1 returned DIVISION", message,
                                        message_size))
        return false;
    flow_preseason_direction(runtime, false, TECMO_CONTROL_DOWN);
    if (runtime->preseason_state.division_selection[0] != 2U ||
        runtime->preseason_state.team_selection[0] != 6U)
        return false;
    flow_preseason_release(runtime, false, true);
    if (!flow_preseason_expect_phase(runtime, TECMO_PRESEASON_TEAM_ENTRY,
                                     "early-fade division accept", message, message_size) ||
        runtime->preseason_state.team_selection[0] != 0U ||
        runtime->preseason_state.division_return_fade_active)
        return false;
    flow_preseason_neutral(runtime, asset->team_input_ready_frames);
    flow_preseason_release(runtime, false, false);
    flow_preseason_neutral(runtime, asset->team_exit_frames +
                           asset->overlays[1].height);
    if (!flow_preseason_expect_phase(runtime, TECMO_PRESEASON_DIVISION,
                                      "second P1 division return", message, message_size))
        return false;
    if (!flow_preseason_unlock_selector(runtime, "second P1 DIVISION", message,
                                        message_size))
        return false;
    flow_preseason_direction(runtime, false, TECMO_CONTROL_UP);
    if (!flow_preseason_wait_cooldown(runtime, message, message_size)) return false;
    flow_preseason_direction(runtime, false, TECMO_CONTROL_UP);
    if (runtime->preseason_state.division_selection[0] != 0U ||
        !flow_preseason_wait_cooldown(runtime, message, message_size))
        return false;
    flow_preseason_release(runtime, false, true);
    flow_preseason_neutral(runtime, asset->team_input_ready_frames);
    if (runtime->preseason_state.team_selection[0] != 0U) return false;
    flow_preseason_release(runtime, false, true);
    flow_preseason_neutral(runtime, asset->team_exit_frames);
    if (runtime->preseason_state.active_player != 1U ||
        runtime->preseason_state.division_selection[1] != 0U ||
        runtime->preseason_state.team_selection[1] != 1U)
        return false;
    flow_preseason_neutral(runtime, asset->overlays[1].height);
    if (!flow_preseason_expect_phase(runtime, TECMO_PRESEASON_DIVISION,
                                     "P2 division setup", message, message_size) ||
        runtime->preseason_state.direction_cooldown != asset->accepted_input_seed)
        return false;

    player_one.down = true;
    tecmo_runtime_update_players(runtime, &player_one, &player_two);
    memset(&player_one, 0, sizeof(player_one));
    tecmo_runtime_update_players(runtime, &player_one, &player_two);
    if (runtime->preseason_state.division_selection[1] != 0U) {
        set_flow_test_message(message, message_size,
                              "P1 changed a P2-owned division menu");
        return false;
    }
    if (!flow_preseason_wait_cooldown(runtime, message, message_size)) return false;
    player_two.up = true;
    player_two.down = true;
    tecmo_runtime_update_players(runtime, &player_one, &player_two);
    if (runtime->preseason_state.division_selection[1] != 0U ||
        runtime->preseason_state.direction_cooldown != 7U)
        return false;
    memset(&player_two, 0, sizeof(player_two));
    tecmo_runtime_update_players(runtime, &player_one, &player_two);
    if (!flow_preseason_wait_cooldown(runtime, message, message_size)) return false;
    flow_preseason_release(runtime, true, true);
    flow_preseason_neutral(runtime, asset->team_input_ready_frames);
    if (!flow_preseason_expect_phase(runtime, TECMO_PRESEASON_TEAM,
                                      "terminal P2 team", message, message_size) ||
        runtime->preseason_state.team_selection[1] != 1U ||
        runtime->preseason_state.direction_cooldown != asset->accepted_input_seed)
        return false;
    player_one.left = true;
    tecmo_runtime_update_players(runtime, &player_one, &player_two);
    memset(&player_one, 0, sizeof(player_one));
    tecmo_runtime_update_players(runtime, &player_one, &player_two);
    if (runtime->preseason_state.team_selection[1] != 1U) return false;
    if (!flow_preseason_wait_cooldown(runtime, message, message_size)) return false;
    flow_preseason_direction(runtime, true, TECMO_CONTROL_RIGHT);
    if (runtime->preseason_state.team_selection[1] != 2U ||
        !flow_preseason_wait_cooldown(runtime, message, message_size))
        return false;
    flow_preseason_direction(runtime, true, TECMO_CONTROL_LEFT);
    if (runtime->preseason_state.team_selection[1] != 1U ||
        !flow_preseason_wait_cooldown(runtime, message, message_size))
        return false;
    flow_preseason_direction(runtime, true, TECMO_CONTROL_LEFT);
    if (runtime->preseason_state.team_selection[1] != 6U ||
        !flow_preseason_wait_cooldown(runtime, message, message_size))
        return false;
    flow_preseason_release(runtime, true, true);
    if (runtime->mode != TECMO_MODE_PRESEASON_MENU ||
        runtime->preseason_state.phase != TECMO_PRESEASON_TEAM ||
        runtime->preseason_state.team_selection[1] != 6U ||
        runtime->preseason_state.direction_cooldown != asset->accepted_input_seed) {
        set_flow_test_message(message, message_size,
                              "P2 A crossed the stop-before-launch boundary");
        return false;
    }
    flow_preseason_direction(runtime, true, TECMO_CONTROL_RIGHT);
    if (runtime->preseason_state.team_selection[1] != 6U) {
        set_flow_test_message(message, message_size,
                              "P2 A did not gate the following direction");
        return false;
    }
    if (!flow_preseason_wait_cooldown(runtime, message, message_size)) return false;
    flow_preseason_release(runtime, true, false);
    flow_preseason_neutral(runtime, asset->team_exit_frames +
                           asset->overlays[1].height);
    if (!flow_preseason_expect_phase(runtime, TECMO_PRESEASON_DIVISION,
                                     "P2 team B division return", message, message_size) ||
        runtime->preseason_state.division_return_fade_frame != 0U)
        return false;
    flow_preseason_release(runtime, true, false);
    if (runtime->preseason_state.division_return_fade_active ||
        runtime->preseason_state.active_player != 0U ||
        runtime->preseason_state.division_selection[0] != 0U ||
        runtime->preseason_state.division_selection[1] != 0U)
        return false;
    flow_preseason_neutral(runtime, asset->overlays[1].height + 1U);
    player_two.down = true;
    tecmo_runtime_update_players(runtime, &player_one, &player_two);
    memset(&player_two, 0, sizeof(player_two));
    tecmo_runtime_update_players(runtime, &player_one, &player_two);
    if (runtime->preseason_state.control_selection != 2U) return false;
    flow_preseason_release(runtime, false, false);
    if (runtime->preseason_state.direction_cooldown != asset->accepted_input_seed)
        return false;
    flow_preseason_neutral(runtime, asset->overlays[0].height - 1U);
    if (runtime->preseason_state.phase != TECMO_PRESEASON_CONTROL_TEARDOWN_ROOT ||
        runtime->preseason_state.direction_cooldown != asset->accepted_input_seed) {
        set_flow_test_message(message, message_size,
                              "preseason root teardown consumed its frozen gate");
        return false;
    }
    flow_preseason_neutral(runtime, 1U);
    if (!flow_expect_mode(runtime, TECMO_MODE_START_GAME_MENU,
                          "preseason CONTROL B root return", message, message_size) ||
        runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_ROOT ||
        runtime->start_game_menu_state.frame !=
            runtime->start_game_menu_asset.stable_frame ||
        runtime->start_game_menu_state.root_selection != 0U ||
        runtime->start_game_menu_state.direction_cooldown !=
            runtime->start_game_menu_asset.accepted_input_seed)
        return false;
    flow_preseason_neutral(runtime, 1U);
    if (runtime->start_game_menu_state.direction_cooldown !=
            (uint16_t)(runtime->start_game_menu_asset.accepted_input_seed - 1U)) {
        set_flow_test_message(message, message_size,
                              "start-menu root return did not tick its seeded gate");
        return false;
    }
    return flow_preseason_invalid_state_guards(runtime, message, message_size);
}

bool tecmo_runtime_flow_self_test(TecmoRuntime *runtime, char *message, size_t message_size)
{
    TecmoInput input;
    uint8_t previous_intro_step;

    if (message != NULL && message_size > 0U) {
        message[0] = '\0';
    }
    if (runtime == NULL || runtime->memory == NULL) {
        set_flow_test_message(message, message_size, "runtime or memory is null");
        return false;
    }
    if (runtime->team_count == 0U) {
        set_flow_test_message(message, message_size, "no roster teams loaded");
        return false;
    }
    if (!runtime->start_game_menu_asset.available ||
        runtime->start_game_menu_asset.setting_cursor_x[0] != 47U ||
        runtime->start_game_menu_asset.setting_cursor_y[0] != 200U ||
        runtime->start_game_menu_asset.setting_cursor_x[1] != 47U ||
        runtime->start_game_menu_asset.setting_cursor_y[1] != 167U ||
        runtime->start_game_menu_asset.setting_cursor_x[2] != 71U ||
        runtime->start_game_menu_asset.setting_cursor_y[2] != 200U) {
        set_flow_test_message(message, message_size,
                              "ROM-derived settings cursor anchors were not loaded");
        return false;
    }
    if (!flow_expect_dual_controller_compatibility(runtime, message, message_size)) {
        return false;
    }
    if (!flow_expect_opening_handoff_frames(runtime, message, message_size)) {
        return false;
    }

    memset(&input, 0, sizeof(input));
    flow_step(runtime, input);
    if (!flow_expect_mode(runtime, TECMO_MODE_MAIN_MENU, "boot menu", message, message_size)) {
        return false;
    }
    if (!flow_expect_menu_item(runtime, 0U, "initial menu", message, message_size)) {
        return false;
    }

    memset(&input, 0, sizeof(input));
    input.confirm = true;
    flow_step(runtime, input);
    if (!flow_expect_mode(runtime, TECMO_MODE_FIRST_SPRITE, "enter play game", message, message_size)) {
        return false;
    }
    memset(&input, 0, sizeof(input));
    flow_step(runtime, input);
    input.cancel = true;
    flow_step(runtime, input);
    if (runtime->mode != TECMO_MODE_MAIN_MENU) {
        set_flow_test_message(message, message_size,
                              "debug intro B did not preserve the modern menu return");
        return false;
    }
    runtime->normal_play_active = true;
    tecmo_runtime_set_mode(runtime, TECMO_MODE_FIRST_SPRITE);
    memset(&input, 0, sizeof(input));
    input.cancel = true;
    flow_step(runtime, input);
    if (runtime->mode != TECMO_MODE_FIRST_SPRITE) {
        set_flow_test_message(message, message_size,
                              "normal-play intro B returned to the modern debug menu");
        return false;
    }
    memset(&input, 0, sizeof(input));
    flow_step(runtime, input);

    previous_intro_step = runtime->intro_output_step;
    if (!flow_wait_for_intro_step_advance(runtime,
                                          previous_intro_step,
                                          180U,
                                          "play game did not advance to NBA license step",
                                          message,
                                          message_size)) {
        return false;
    }
    previous_intro_step = runtime->intro_output_step;
    if (!flow_wait_for_intro_step_advance(runtime,
                                          previous_intro_step,
                                          300U,
                                          "play game did not advance to arena intro step",
                                          message,
                                          message_size)) {
        return false;
    }
    previous_intro_step = runtime->intro_output_step;
    if (!flow_expect_arena_bank04_handoff_frame(message, message_size)) {
        return false;
    }
    if (!flow_hold_intro_step(runtime,
                              previous_intro_step,
                              FLOW_INTRO_ARENA_BANK04_HANDOFF_FRAME - 1U,
                              "play game left arena before Bank04 handoff",
                              message,
                              message_size)) {
        return false;
    }
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
    if (runtime->intro_output_step == previous_intro_step) {
        set_flow_test_message(message, message_size, "play game did not advance at Bank04 handoff");
        return false;
    }
    if (runtime->mode_frame_counter != 0U) {
        set_flow_test_message(message, message_size, "arena handoff did not reset intro frame counter");
        return false;
    }
    previous_intro_step = runtime->intro_output_step;
    if (!flow_wait_for_intro_step_advance(runtime,
                                          previous_intro_step,
                                          120U,
                                          "play game did not advance to Warriors intro step",
                                          message,
                                          message_size)) {
        return false;
    }
    previous_intro_step = runtime->intro_output_step;
    if (!flow_hold_intro_step(runtime,
                              previous_intro_step,
                              TECMO_INTRO_WARRIORS_HANDOFF_FRAME - 1U,
                              "play game left Warriors before ROM handoff",
                              message,
                              message_size)) {
        return false;
    }
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
    if (runtime->intro_output_step != 11U || runtime->mode_frame_counter != 0U ||
        runtime->intro_next_screen != TECMO_INTRO_WARRIORS_NEXT_SCREEN ||
        runtime->mode != TECMO_MODE_FIRST_SPRITE) {
        set_flow_test_message(message,
                              message_size,
                              "Warriors did not hand off cleanly to native Clippers");
        return false;
    }
    if (!flow_hold_intro_step(runtime, 11U, TECMO_INTRO_CLIPPERS_HANDOFF_FRAME - 1U,
                              "Clippers left before BUCKS handoff", message, message_size)) {
        return false;
    }
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
    if (runtime->intro_output_step != 12U || runtime->mode_frame_counter != 0U) {
        set_flow_test_message(message, message_size, "Clippers did not hand off cleanly to native BUCKS");
        return false;
    }
    if (!flow_hold_intro_step(runtime, 12U, TECMO_INTRO_BUCKS_HANDOFF_FRAME - 1U,
                              "BUCKS left before PASS handoff", message, message_size)) {
        return false;
    }
    tecmo_runtime_update(runtime, &input);
    if (runtime->intro_output_step != 13U || runtime->mode_frame_counter != 0U) {
        set_flow_test_message(message, message_size, "BUCKS did not hand off cleanly to native PASS");
        return false;
    }
    if (!flow_hold_intro_step(runtime, 13U, TECMO_INTRO_PASS_HANDOFF_FRAME - 1U,
                              "PASS left before finale handoff", message, message_size)) {
        return false;
    }
    tecmo_runtime_update(runtime, &input);
    if (runtime->intro_output_step != 14U || runtime->mode_frame_counter != 0U ||
        runtime->mode != TECMO_MODE_FIRST_SPRITE || runtime->intro_handoff_complete) {
        set_flow_test_message(message,
                              message_size,
                              "PASS did not hand off cleanly to the native intro finale");
        return false;
    }
    if (!flow_hold_intro_step(runtime, 14U, tecmo_intro_finale_hold_frame() - 1U,
                              "finale left before terminator boundary", message, message_size) ||
        runtime->intro_handoff_complete) {
        set_flow_test_message(message, message_size,
                              "finale marked handoff before the terminator boundary");
        return false;
    }
    tecmo_runtime_update(runtime, &input);
    if (runtime->intro_output_step != 15U || runtime->mode_frame_counter != 0U ||
        runtime->mode != TECMO_MODE_FIRST_SPRITE || runtime->intro_handoff_complete) {
        set_flow_test_message(message, message_size,
                              "finale did not enter the native title attract continuation");
        return false;
    }
    if (!flow_hold_intro_step(runtime, 15U, 620U,
                              "title attract reset before its natural completion",
                              message, message_size)) {
        return false;
    }
    tecmo_runtime_update(runtime, &input);
    if (runtime->intro_output_step != 15U || !runtime->intro_handoff_complete ||
        runtime->mode_frame_counter != 621U) {
        set_flow_test_message(message, message_size, "title attract did not signal natural completion at frame 621");
        return false;
    }
    if (!flow_hold_intro_step(runtime, 15U, 20U,
                              "title attract reset before dispatch frame 642",
                              message, message_size)) return false;
    tecmo_runtime_update(runtime, &input);
    if (runtime->intro_output_step != FLOW_INTRO_TITLE_STEP ||
        runtime->mode_frame_counter != 0U || runtime->intro_handoff_complete) {
        set_flow_test_message(message, message_size, "title attract did not restart TECMO opening at frame 642");
        return false;
    }

    memset(&input, 0, sizeof(input));
    input.confirm = true;
    tecmo_runtime_update(runtime, &input);
    if (!flow_expect_mode(runtime, TECMO_MODE_TITLE_SCREEN, "first START title entry", message, message_size)) return false;
    tecmo_runtime_update(runtime, &input);
    if (runtime->title_confirming || runtime->title_start_armed) {
        set_flow_test_message(message, message_size, "held first START incorrectly retriggered the title");
        return false;
    }
    memset(&input, 0, sizeof(input));
    while (runtime->mode_frame_counter < TECMO_TITLE_START_LOAD_FRAMES) {
        tecmo_runtime_update(runtime, &input);
    }
    if (!runtime->title_start_armed || runtime->title_confirming ||
        runtime->mode_frame_counter != TECMO_TITLE_START_LOAD_FRAMES) {
        set_flow_test_message(message, message_size, "title did not load and arm after START release");
        return false;
    }
    input.cancel = true;
    tecmo_runtime_update(runtime, &input);
    if (runtime->mode != TECMO_MODE_TITLE_SCREEN || runtime->title_confirming) {
        set_flow_test_message(message, message_size,
                              "title B returned to the modern debug menu");
        return false;
    }
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
    input.confirm = true;
    tecmo_runtime_update(runtime, &input);
    if (!runtime->title_confirming || runtime->title_confirmation_frame != 1U) {
        set_flow_test_message(message, message_size, "second START did not begin title confirmation");
        return false;
    }
    memset(&input, 0, sizeof(input));
    for (size_t flash = 1U; flash < 126U; ++flash) tecmo_runtime_update(runtime, &input);
    if (runtime->mode != TECMO_MODE_TITLE_SCREEN || runtime->title_confirmation_frame != 126U) {
        set_flow_test_message(message, message_size, "title confirmation did not remain visible through frame 126");
        return false;
    }
    tecmo_runtime_update(runtime, &input);
    if (!flow_expect_mode(runtime, TECMO_MODE_START_GAME_MENU,
                          "title confirmation frame 127 handoff", message, message_size)) return false;

    for (size_t reveal = 0U; reveal < 31U; ++reveal)
        tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.frame != 31U ||
        runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_REVEAL) {
        set_flow_test_message(message, message_size,
                              "start-game menu accepted input before reveal frame 32");
        return false;
    }
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.frame != 32U ||
        runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_ROOT ||
        runtime->start_game_menu_state.root_selection != 0U) {
        set_flow_test_message(message, message_size,
                              "start-game menu did not become stable at frame 32");
        return false;
    }

    memset(&input, 0, sizeof(input));
    input.confirm = true;
    flow_step(runtime, input);
    if (runtime->mode != TECMO_MODE_START_GAME_MENU ||
        runtime->start_game_menu_state.root_selection != 0U) {
        set_flow_test_message(message, message_size, "START was not ignored by start-game menu");
        return false;
    }
    memset(&input, 0, sizeof(input));
    input.left = true;
    flow_step(runtime, input);
    input.left = false;
    input.right = true;
    flow_step(runtime, input);
    if (runtime->start_game_menu_state.root_selection != 0U) {
        set_flow_test_message(message, message_size, "left/right changed root menu selection");
        return false;
    }
    memset(&input, 0, sizeof(input));
    input.up = true;
    input.down = true;
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.root_selection != 0U ||
        runtime->start_game_menu_state.direction_cooldown != 7U) {
        set_flow_test_message(message, message_size, "root Up+Down did not consume a direction gate");
        return false;
    }
    memset(&input, 0, sizeof(input));
    for (size_t wait = 0U; wait < 7U; ++wait) tecmo_runtime_update(runtime, &input);
    input.up = true;
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.root_selection != 6U ||
        runtime->start_game_menu_state.direction_cooldown != 7U) {
        set_flow_test_message(message, message_size, "root UP did not wrap to GAME MUSIC");
        return false;
    }
    memset(&input, 0, sizeof(input));
    for (size_t wait = 0U; wait < 7U; ++wait) tecmo_runtime_update(runtime, &input);
    input.down = true;
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.root_selection != 0U) {
        set_flow_test_message(message, message_size, "root DOWN did not wrap to PRESEASON");
        return false;
    }

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 32U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_ROOT;
    memset(&input, 0, sizeof(input));
    input.down = true;
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.root_selection != 1U) {
        set_flow_test_message(message, message_size, "root DOWN edge was not immediate");
        return false;
    }
    for (size_t held = 0U; held < 7U; ++held) tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.root_selection != 1U) {
        set_flow_test_message(message, message_size, "root direction repeated before eight frames");
        return false;
    }
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.root_selection != 2U) {
        set_flow_test_message(message, message_size, "root direction did not repeat at eight frames");
        return false;
    }
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 32U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_ROOT;
    memset(&input, 0, sizeof(input));
    input.cancel = true;
    for (size_t held = 0U; held < 120U; ++held) tecmo_runtime_update(runtime, &input);
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
    if (runtime->mode != TECMO_MODE_START_GAME_MENU ||
        runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_ROOT ||
        runtime->start_game_menu_state.root_selection != 0U) {
        set_flow_test_message(message, message_size, "root B was not ignored");
        return false;
    }

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 32U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_ROOT;
    runtime->previous_input.shoot = true;
    memset(&input, 0, sizeof(input));
    input.left = true;
    tecmo_runtime_update(runtime, &input);
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
    if (runtime->mode != TECMO_MODE_START_GAME_MENU ||
        runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_ROOT ||
        runtime->start_game_menu_state.direction_cooldown != 0U) {
        set_flow_test_message(message, message_size, "current button did not suppress root A-release grace");
        return false;
    }

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 32U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_ROOT;
    runtime->previous_input.shoot = true;
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
    if (!flow_expect_mode(runtime, TECMO_MODE_PRESEASON_MENU,
                          "root PRESEASON submenu-boundary dispatch",
                          message, message_size)) return false;
    if (!flow_expect_preseason_native_path(runtime, message, message_size))
        return false;

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 32U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_ROOT;
    runtime->start_game_menu_state.root_selection = 2U;
    memset(&input, 0, sizeof(input));
    input.shoot = true;
    flow_step(runtime, input);
    if (!flow_expect_mode(runtime, TECMO_MODE_PLAY_SETUP,
                          "root ALL STAR submenu-boundary dispatch",
                          message, message_size)) return false;

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 32U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_ROOT;
    memset(&input, 0, sizeof(input));
    input.shoot = true;
    input.down = true;
    tecmo_runtime_update(runtime, &input);
    if (runtime->mode != TECMO_MODE_START_GAME_MENU ||
        runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_ROOT ||
        runtime->start_game_menu_state.root_selection != 1U ||
        runtime->start_game_menu_state.direction_cooldown != 7U) {
        set_flow_test_message(message, message_size, "root A+Down press did not move before release");
        return false;
    }
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_SEASON_SLIDE_IN ||
        runtime->start_game_menu_state.direction_cooldown != 5U) {
        set_flow_test_message(message, message_size, "root A+Down release did not activate moved selection");
        return false;
    }

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 32U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_ROOT;
    runtime->start_game_menu_state.root_selection = 3U;
    memset(&input, 0, sizeof(input));
    input.shoot = true;
    flow_step(runtime, input);
    if (!flow_finish_start_menu_exit(runtime, TECMO_MODE_ROSTERS, false,
                                     "root A team-data frame-eleven dispatch",
                                     message, message_size)) return false;

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 32U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_ROOT;
    memset(&input, 0, sizeof(input));
    input.down = true;
    tecmo_runtime_update(runtime, &input);
    input.down = false;
    input.shoot = true;
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_ROOT ||
        runtime->start_game_menu_state.root_selection != 1U ||
        runtime->start_game_menu_state.direction_cooldown != 6U) {
        set_flow_test_message(message, message_size, "held root A changed state during direction gate");
        return false;
    }
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_SEASON_SLIDE_IN ||
        runtime->start_game_menu_state.direction_cooldown != 5U) {
        set_flow_test_message(message, message_size, "root A release was gated by directional E1");
        return false;
    }

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 32U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_ROOT;
    runtime->start_game_menu_state.root_selection = 1U;
    memset(&input, 0, sizeof(input));
    input.shoot = true;
    flow_step(runtime, input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_SEASON_SLIDE_IN ||
        runtime->start_game_menu_state.slide_frame != 0U) {
        set_flow_test_message(message, message_size, "season route did not start at slide frame zero");
        return false;
    }
    memset(&input, 0, sizeof(input));
    for (size_t slide = 0U; slide < 31U; ++slide) tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_SEASON_SLIDE_IN ||
        runtime->start_game_menu_state.slide_frame != 31U) {
        set_flow_test_message(message, message_size, "season slide ended before frame 32");
        return false;
    }
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_SEASON ||
        runtime->start_game_menu_state.slide_frame != 32U ||
        runtime->start_game_menu_state.direction_cooldown != 4U ||
        runtime->start_game_menu_state.cursor_delay !=
            runtime->start_game_menu_asset.cursor_commit_delay_frames ||
        tecmo_start_game_menu_cursor_visible(&runtime->start_game_menu_asset,
                                             &runtime->start_game_menu_state)) {
        set_flow_test_message(message, message_size, "season slide did not end at frame 32");
        return false;
    }
    input.cancel = true;
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_SEASON ||
        runtime->start_game_menu_state.direction_cooldown != 3U ||
        runtime->start_game_menu_state.cursor_delay != 0U ||
        !tecmo_start_game_menu_cursor_visible(&runtime->start_game_menu_asset,
                                              &runtime->start_game_menu_state)) {
        set_flow_test_message(message, message_size, "held season B activated before release");
        return false;
    }
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_SEASON_SLIDE_OUT ||
        runtime->start_game_menu_state.slide_frame != 32U ||
        runtime->start_game_menu_state.direction_cooldown != 5U) {
        set_flow_test_message(message, message_size, "season B did not begin exact reverse slide");
        return false;
    }
    memset(&input, 0, sizeof(input));
    for (size_t slide = 0U; slide < 31U; ++slide) tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_SEASON_SLIDE_OUT ||
        runtime->start_game_menu_state.slide_frame != 1U) {
        set_flow_test_message(message, message_size, "season reverse ended before frame 32");
        return false;
    }
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_ROOT ||
        runtime->start_game_menu_state.slide_frame != 0U ||
        runtime->start_game_menu_state.direction_cooldown != 4U ||
        runtime->start_game_menu_state.cursor_delay !=
            runtime->start_game_menu_asset.cursor_commit_delay_frames ||
        tecmo_start_game_menu_cursor_visible(&runtime->start_game_menu_asset,
                                             &runtime->start_game_menu_state)) {
        set_flow_test_message(message, message_size, "season reverse did not restore root at frame 32");
        return false;
    }
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_ROOT ||
        runtime->start_game_menu_state.direction_cooldown != 3U ||
        runtime->start_game_menu_state.cursor_delay != 0U ||
        !tecmo_start_game_menu_cursor_visible(&runtime->start_game_menu_asset,
                                              &runtime->start_game_menu_state)) {
        set_flow_test_message(message, message_size, "root cursor did not reach OAM after season reverse");
        return false;
    }

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 32U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_ROOT;
    runtime->start_game_menu_state.root_selection = 6U;
    memset(&input, 0, sizeof(input));
    input.shoot = true;
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_ROOT ||
        runtime->start_game_menu_state.direction_cooldown != 0U) {
        set_flow_test_message(message, message_size, "held root A opened GAME MUSIC before release");
        return false;
    }
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_POPUP_SETUP ||
        runtime->start_game_menu_state.popup_phase != TECMO_START_GAME_MENU_MUSIC ||
        runtime->start_game_menu_state.setting_selection != 1U ||
        runtime->start_game_menu_state.direction_cooldown != 5U) {
        set_flow_test_message(message, message_size, "GAME MUSIC did not begin native row setup");
        return false;
    }
    if (!flow_finish_popup_setup(runtime, TECMO_START_GAME_MENU_MUSIC,
                                 "GAME MUSIC setup did not finish at frame six",
                                 message, message_size)) return false;
    input.shoot = true;
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_MUSIC ||
        runtime->start_game_menu_state.direction_cooldown != 2U) {
        set_flow_test_message(message, message_size, "held popup A activated before release");
        return false;
    }
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_POPUP_TEARDOWN ||
        runtime->start_game_menu_state.music_value != 1U ||
        runtime->start_game_menu_state.direction_cooldown != 5U) {
        set_flow_test_message(message, message_size, "popup A release did not begin teardown");
        return false;
    }
    if (!flow_finish_popup_teardown(runtime, TECMO_START_GAME_MENU_MUSIC,
                                    "GAME MUSIC teardown did not restore root at frame six",
                                    message, message_size)) return false;
    input.shoot = true;
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_ROOT ||
        runtime->start_game_menu_state.direction_cooldown != 2U ||
        !tecmo_start_game_menu_cursor_visible(&runtime->start_game_menu_asset,
                                              &runtime->start_game_menu_state)) {
        set_flow_test_message(message, message_size, "held root A reopened popup before release");
        return false;
    }
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_POPUP_SETUP ||
        runtime->start_game_menu_state.direction_cooldown != 5U) {
        set_flow_test_message(message, message_size, "root A release did not bypass directional E1");
        return false;
    }
    if (!flow_finish_popup_setup(runtime, TECMO_START_GAME_MENU_MUSIC,
                                 "second GAME MUSIC setup did not finish",
                                 message, message_size)) return false;
    for (size_t wait = 0U; wait < 3U; ++wait) tecmo_runtime_update(runtime, &input);
    input.down = true;
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.setting_selection != 0U ||
        runtime->start_game_menu_state.direction_cooldown != 7U) {
        set_flow_test_message(message, message_size, "GAME MUSIC DOWN did not wrap");
        return false;
    }
    memset(&input, 0, sizeof(input));
    for (size_t wait = 0U; wait < 7U; ++wait) tecmo_runtime_update(runtime, &input);
    input.cancel = true;
    flow_step(runtime, input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_POPUP_TEARDOWN ||
        runtime->start_game_menu_state.music_value != 1U ||
        runtime->start_game_menu_state.direction_cooldown != 5U) {
        set_flow_test_message(message, message_size, "GAME MUSIC B did not begin cancel teardown");
        return false;
    }
    if (!flow_finish_popup_teardown(runtime, TECMO_START_GAME_MENU_MUSIC,
                                    "GAME MUSIC cancel teardown did not restore root",
                                    message, message_size)) return false;

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 32U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_MUSIC;
    runtime->start_game_menu_state.setting_selection = 0U;
    runtime->previous_input.cancel = true;
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_POPUP_TEARDOWN ||
        runtime->start_game_menu_state.music_value != 1U ||
        runtime->start_game_menu_state.direction_cooldown != 5U) {
        set_flow_test_message(message, message_size, "GAME MUSIC B-release grace did not begin teardown");
        return false;
    }
    if (!flow_finish_popup_teardown(runtime, TECMO_START_GAME_MENU_MUSIC,
                                    "GAME MUSIC B-release teardown did not finish",
                                    message, message_size)) return false;

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 32U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_ROOT;
    runtime->start_game_menu_state.root_selection = 4U;
    memset(&input, 0, sizeof(input));
    input.shoot = true;
    flow_step(runtime, input);
    if (!flow_finish_popup_setup(runtime, TECMO_START_GAME_MENU_SPEED,
                                 "GAME SPEED setup did not finish at frame eight",
                                 message, message_size)) return false;
    memset(&input, 0, sizeof(input));
    input.cancel = true;
    flow_step(runtime, input);
    if (!flow_finish_popup_teardown(runtime, TECMO_START_GAME_MENU_SPEED,
                                    "GAME SPEED teardown did not finish at frame eight",
                                    message, message_size)) return false;

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 32U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_ROOT;
    runtime->start_game_menu_state.root_selection = 5U;
    memset(&input, 0, sizeof(input));
    input.shoot = true;
    flow_step(runtime, input);
    if (!flow_finish_popup_setup(runtime, TECMO_START_GAME_MENU_PERIOD,
                                 "PERIOD setup did not preserve its extra frame",
                                 message, message_size)) return false;
    memset(&input, 0, sizeof(input));
    input.cancel = true;
    flow_step(runtime, input);
    if (!flow_finish_popup_teardown(runtime, TECMO_START_GAME_MENU_PERIOD,
                                    "PERIOD teardown did not finish at frame six",
                                    message, message_size)) return false;

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 32U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_SPEED;
    runtime->start_game_menu_state.setting_selection = 0U;
    memset(&input, 0, sizeof(input));
    input.up = true;
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.setting_selection != 2U ||
        runtime->start_game_menu_state.direction_cooldown != 7U) {
        set_flow_test_message(message, message_size, "GAME SPEED UP did not wrap");
        return false;
    }

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 32U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_SPEED;
    runtime->start_game_menu_state.setting_selection = 0U;
    memset(&input, 0, sizeof(input));
    input.up = true;
    input.down = true;
    input.shoot = true;
    input.cancel = true;
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_SPEED ||
        runtime->start_game_menu_state.setting_selection != 0U ||
        runtime->start_game_menu_state.direction_cooldown != 7U) {
        set_flow_test_message(message, message_size, "SPEED A+B+Up+Down press was not direction-consumed");
        return false;
    }
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_POPUP_TEARDOWN ||
        runtime->start_game_menu_state.speed_value != 0U ||
        runtime->start_game_menu_state.direction_cooldown != 5U) {
        set_flow_test_message(message, message_size, "SPEED A+B release did not begin A-priority teardown");
        return false;
    }
    if (!flow_finish_popup_teardown(runtime, TECMO_START_GAME_MENU_SPEED,
                                    "SPEED A-priority teardown did not finish",
                                    message, message_size)) return false;

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 32U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_PERIOD;
    runtime->start_game_menu_state.setting_selection = 1U;
    memset(&input, 0, sizeof(input));
    input.up = true;
    input.shoot = true;
    input.cancel = true;
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_PERIOD ||
        runtime->start_game_menu_state.setting_selection != 2U ||
        runtime->start_game_menu_state.direction_cooldown != 5U) {
        set_flow_test_message(message, message_size, "PERIOD Up did not precede A+B");
        return false;
    }
    memset(&input, 0, sizeof(input));
    input.shoot = true;
    for (size_t wait = 0U; wait < 5U; ++wait) tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_PERIOD ||
        runtime->start_game_menu_state.direction_cooldown != 0U) {
        set_flow_test_message(message, message_size, "held PERIOD A activated before release");
        return false;
    }
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_POPUP_TEARDOWN ||
        runtime->start_game_menu_state.period_index != 2U ||
        runtime->start_game_menu_state.direction_cooldown != 5U) {
        set_flow_test_message(message, message_size, "PERIOD A did not begin teardown after direction grace");
        return false;
    }
    if (!flow_finish_popup_teardown(runtime, TECMO_START_GAME_MENU_PERIOD,
                                    "PERIOD accept teardown did not finish",
                                    message, message_size)) return false;

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 32U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_PERIOD;
    runtime->start_game_menu_state.setting_selection = 3U;
    memset(&input, 0, sizeof(input));
    input.shoot = true;
    input.cancel = true;
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_PERIOD ||
        runtime->start_game_menu_state.setting_selection != 3U ||
        runtime->start_game_menu_state.direction_cooldown != 0U) {
        set_flow_test_message(message, message_size, "held PERIOD A+B changed state before release");
        return false;
    }
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_PERIOD ||
        runtime->start_game_menu_state.setting_selection != 3U ||
        runtime->start_game_menu_state.direction_cooldown != 5U) {
        set_flow_test_message(message, message_size, "PERIOD A+B release was not a consumed no-op");
        return false;
    }

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 32U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_PERIOD;
    runtime->start_game_menu_state.setting_selection = 3U;
    runtime->previous_input.shoot = true;
    runtime->previous_input.cancel = true;
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_PERIOD ||
        runtime->start_game_menu_state.setting_selection != 3U ||
        runtime->start_game_menu_state.direction_cooldown != 5U) {
        set_flow_test_message(message, message_size, "PERIOD A+B release did not remain a consumed no-op");
        return false;
    }

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 32U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_PERIOD;
    runtime->start_game_menu_state.setting_selection = 1U;
    runtime->previous_input.up = true;
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_PERIOD ||
        runtime->start_game_menu_state.setting_selection != 2U ||
        runtime->start_game_menu_state.direction_cooldown != 5U) {
        set_flow_test_message(message, message_size, "PERIOD Up-release was not consumed");
        return false;
    }

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 32U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_PERIOD;
    runtime->start_game_menu_state.setting_selection = 3U;
    memset(&input, 0, sizeof(input));
    input.up = true;
    input.down = true;
    input.shoot = true;
    input.cancel = true;
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_PERIOD ||
        runtime->start_game_menu_state.setting_selection != 3U ||
        runtime->start_game_menu_state.direction_cooldown != 5U) {
        set_flow_test_message(message, message_size, "PERIOD Up+Down did not consume A+B");
        return false;
    }
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_PERIOD ||
        runtime->start_game_menu_state.setting_selection != 3U ||
        runtime->start_game_menu_state.direction_cooldown != 4U) {
        set_flow_test_message(message, message_size, "PERIOD direction release did not suppress A+B");
        return false;
    }

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 32U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_PERIOD;
    runtime->start_game_menu_state.setting_selection = 2U;
    memset(&input, 0, sizeof(input));
    input.down = true;
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.setting_selection != 1U) {
        set_flow_test_message(message, message_size, "PERIOD DOWN did not decrease minutes");
        return false;
    }

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 32U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_PERIOD;
    runtime->start_game_menu_state.setting_selection = 4U;
    memset(&input, 0, sizeof(input));
    input.up = true;
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.setting_selection != 4U) {
        set_flow_test_message(message, message_size, "PERIOD values did not clamp at twelve minutes");
        return false;
    }

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 32U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_PERIOD;
    runtime->start_game_menu_state.setting_selection = 0U;
    memset(&input, 0, sizeof(input));
    input.down = true;
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.setting_selection != 0U) {
        set_flow_test_message(message, message_size, "PERIOD values did not clamp at two minutes");
        return false;
    }

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 64U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_SEASON;
    runtime->start_game_menu_state.slide_frame = 32U;
    runtime->start_game_menu_state.season_selection = 0U;
    memset(&input, 0, sizeof(input));
    input.shoot = true;
    flow_step(runtime, input);
    if (runtime->mode != TECMO_MODE_START_GAME_MENU ||
        runtime->start_game_menu_state.direction_cooldown != 5U) {
        set_flow_test_message(message, message_size, "unported season route did not stay explicit");
        return false;
    }

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 64U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_SEASON;
    runtime->start_game_menu_state.slide_frame = 32U;
    runtime->start_game_menu_state.season_selection = 2U;
    memset(&input, 0, sizeof(input));
    input.shoot = true;
    input.cancel = true;
    flow_step(runtime, input);
    if (runtime->start_game_menu_state.season_selection != 2U) {
        set_flow_test_message(message, message_size, "season A+B release changed selection");
        return false;
    }
    if (!flow_finish_start_menu_exit(runtime, TECMO_MODE_PLAY_SETUP, true,
                                     "season A+B GAME START frame-eleven dispatch",
                                     message, message_size)) return false;

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 64U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_SEASON;
    runtime->start_game_menu_state.slide_frame = 32U;
    runtime->start_game_menu_state.season_selection = 5U;
    memset(&input, 0, sizeof(input));
    input.shoot = true;
    flow_step(runtime, input);
    if (!flow_finish_start_menu_exit(runtime, TECMO_MODE_ROSTERS, true,
                                     "season TEAM DATA frame-eleven dispatch",
                                     message, message_size)) return false;

    tecmo_runtime_set_mode(runtime, TECMO_MODE_MAIN_MENU);
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);

    if (!flow_press_menu_down(runtime, 1U, 1U, "navigate to quit", message, message_size)) {
        return false;
    }
    memset(&input, 0, sizeof(input));
    input.confirm = true;
    flow_step(runtime, input);
    if (!runtime->quit_requested) {
        set_flow_test_message(message, message_size, "quit menu item did not request quit");
        return false;
    }

    set_flow_test_message(message, message_size,
                          "FLOW TEST PASS: menu play-intro title start-game-menu preseason quit");
    return true;
}
