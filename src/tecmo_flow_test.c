#include "tecmo_game.h"
#include "tecmo_intro_stage.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t flow_fnv1a32(const uint8_t *bytes, size_t count)
{
    uint32_t hash = 2166136261U;
    for (size_t i = 0U; i < count; ++i) {
        hash ^= bytes[i];
        hash *= 16777619U;
    }
    return hash;
}

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
    if (mode == TECMO_MODE_TEAM_DATA) {
        return "TEAM DATA";
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

static bool flow_placeholder_route_matches(const TecmoRuntime *runtime,
                                           bool from_season,
                                           uint8_t root_selection,
                                           uint8_t season_selection,
                                           uint8_t music_value,
                                           uint8_t speed_value,
                                           uint8_t period_index)
{
    const TecmoStartGameMenuState *state = &runtime->start_game_menu_state;

    if (runtime->mode != TECMO_MODE_START_GAME_MENU ||
        state->root_selection != root_selection ||
        state->music_value != music_value ||
        state->speed_value != speed_value ||
        state->period_index != period_index) {
        return false;
    }
    if (from_season) {
        return state->phase == TECMO_START_GAME_MENU_SEASON &&
               state->slide_frame == runtime->start_game_menu_asset.slide_frames &&
               state->season_selection == season_selection;
    }
    return state->phase == TECMO_START_GAME_MENU_ROOT &&
           state->slide_frame == 0U;
}

static bool flow_expect_placeholder_return(TecmoRuntime *runtime,
                                           bool from_season,
                                           uint8_t root_selection,
                                           uint8_t season_selection,
                                           const char *label,
                                           char *message,
                                           size_t message_size)
{
    TecmoInput input;
    const TecmoStartGameMenuAsset *asset = &runtime->start_game_menu_asset;
    uint8_t music_value = runtime->start_game_menu_state.music_value;
    uint8_t speed_value = runtime->start_game_menu_state.speed_value;
    uint8_t period_index = runtime->start_game_menu_state.period_index;
    unsigned held_frame;

    if (!runtime->normal_play_active || !runtime->start_menu_return_pending) {
        set_flow_test_message(message, message_size,
                              "normal placeholder did not retain its blue-menu route");
        return false;
    }
    memset(&input, 0, sizeof(input));
    input.cancel = true;
    tecmo_runtime_update(runtime, &input);
    if (!flow_expect_mode(runtime, TECMO_MODE_START_GAME_MENU,
                          label, message, message_size)) {
        return false;
    }
    if (!runtime->normal_play_active || runtime->start_menu_return_pending ||
        !runtime->start_menu_input_neutral_gate ||
        runtime->start_game_menu_state.frame != asset->stable_frame ||
        runtime->start_game_menu_state.direction_cooldown !=
            asset->accepted_input_seed ||
        runtime->start_game_menu_state.cursor_delay !=
            asset->cursor_commit_delay_frames ||
        !flow_placeholder_route_matches(runtime, from_season,
                                        root_selection, season_selection,
                                        music_value, speed_value, period_index)) {
        set_flow_test_message(message, message_size,
                              "placeholder return did not restore stable blue-menu state");
        return false;
    }
    for (held_frame = 0U; held_frame < 3U; ++held_frame) {
        tecmo_runtime_update(runtime, &input);
        if (!runtime->start_menu_input_neutral_gate ||
            runtime->start_game_menu_state.frame != asset->stable_frame ||
            !flow_placeholder_route_matches(runtime, from_season,
                                            root_selection, season_selection,
                                            music_value, speed_value, period_index)) {
            set_flow_test_message(message, message_size,
                                  "held placeholder B escaped the neutral-input gate");
            return false;
        }
    }
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
    if (!runtime->start_menu_input_neutral_gate ||
        runtime->start_game_menu_state.frame != asset->stable_frame ||
        !flow_placeholder_route_matches(runtime, from_season,
                                        root_selection, season_selection,
                                        music_value, speed_value, period_index)) {
        set_flow_test_message(message, message_size,
                              "placeholder B release was re-consumed by the blue menu");
        return false;
    }
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_menu_input_neutral_gate ||
        runtime->start_game_menu_state.frame != asset->stable_frame ||
        !flow_placeholder_route_matches(runtime, from_season,
                                        root_selection, season_selection,
                                        music_value, speed_value, period_index)) {
        set_flow_test_message(message, message_size,
                              "placeholder return did not settle on a neutral frame");
        return false;
    }
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_menu_input_neutral_gate ||
        runtime->start_game_menu_state.frame != asset->stable_frame + 1U ||
        !flow_placeholder_route_matches(runtime, from_season,
                                        root_selection, season_selection,
                                        music_value, speed_value, period_index)) {
        set_flow_test_message(message, message_size,
                              "restored blue menu changed route on its first live neutral frame");
        return false;
    }
    return true;
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

static void flow_team_data_neutral(TecmoRuntime *runtime, size_t frames)
{
    TecmoInput input = {0};
    for (size_t i = 0U; i < frames; ++i) tecmo_runtime_update(runtime, &input);
}

static void flow_team_data_release(TecmoRuntime *runtime, bool accept)
{
    TecmoInput input = {0};
    if (accept) input.shoot = true;
    else input.cancel = true;
    tecmo_runtime_update(runtime, &input);
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
}

static bool flow_team_data_finish_transition(
    TecmoRuntime *runtime,
    TecmoTeamDataTransition transition,
    TecmoTeamDataPhase target_phase,
    char *message,
    size_t message_size)
{
    const TecmoTeamDataAsset *asset = &runtime->team_data_asset;
    if (transition == TECMO_TEAM_DATA_TRANSITION_ENTRY_TO_SELECTOR) {
        uint8_t on = asset->entry_transition_render_on_frame;
        uint8_t first = asset->entry_transition_first_visible_frame;
        uint8_t step = asset->entry_transition_palette_step_frames;
        uint8_t stable = asset->entry_transition_stable_frame;
        if (runtime->team_data_state.transition != transition ||
            runtime->team_data_state.transition_frame != 0U ||
            tecmo_team_data_transition_palette_stage(
                asset, &runtime->team_data_state) != 4U ||
            tecmo_team_data_transition_render_enabled(
                asset, &runtime->team_data_state))
            return false;
        for (uint8_t frame = 1U; frame < stable; ++frame) {
            flow_team_data_neutral(runtime, 1U);
            if (runtime->team_data_state.transition != transition ||
                runtime->team_data_state.transition_frame != frame)
                return false;
            if (frame == on &&
                (!tecmo_team_data_transition_render_enabled(
                     asset, &runtime->team_data_state) ||
                 tecmo_team_data_transition_palette_stage(
                     asset, &runtime->team_data_state) != 4U))
                return false;
            if (frame >= first && (uint8_t)(frame - first) % step == 0U) {
                unsigned expected = (unsigned)(frame - first) / step;
                if (expected > 3U) expected = 3U;
                if (tecmo_team_data_transition_palette_stage(
                        asset, &runtime->team_data_state) != expected)
                    return false;
            }
        }
        flow_team_data_neutral(runtime, 1U);
        if (runtime->team_data_state.transition !=
                TECMO_TEAM_DATA_TRANSITION_NONE ||
            runtime->team_data_state.phase != target_phase ||
            runtime->team_data_state.cursor_delay != 1U) {
            set_flow_test_message(message, message_size,
                                  "TEAM DATA entry transition did not stabilize");
            return false;
        }
        return true;
    }
    uint8_t black = transition ==
            TECMO_TEAM_DATA_TRANSITION_ROSTER_TO_DETAIL
        ? asset->detail_transition_black_frame
        : asset->selector_transition_black_frame;
    uint8_t off = transition ==
            TECMO_TEAM_DATA_TRANSITION_ROSTER_TO_DETAIL
        ? asset->detail_transition_render_off_frame
        : asset->selector_transition_render_off_frame;
    uint8_t on = transition ==
            TECMO_TEAM_DATA_TRANSITION_ROSTER_TO_DETAIL
        ? asset->detail_transition_render_on_frame
        : asset->selector_transition_render_on_frame;
    uint8_t first = transition ==
            TECMO_TEAM_DATA_TRANSITION_ROSTER_TO_DETAIL
        ? asset->detail_transition_first_visible_frame
        : asset->selector_transition_first_visible_frame;
    uint8_t step = transition ==
            TECMO_TEAM_DATA_TRANSITION_ROSTER_TO_DETAIL
        ? asset->detail_transition_palette_step_frames
        : asset->selector_transition_palette_step_frames;
    uint8_t stable = transition ==
            TECMO_TEAM_DATA_TRANSITION_ROSTER_TO_DETAIL
        ? asset->detail_transition_stable_frame
        : asset->selector_transition_stable_frame;
    if (runtime->team_data_state.transition != transition ||
        runtime->team_data_state.transition_frame != 0U ||
        tecmo_team_data_transition_palette_stage(
            asset, &runtime->team_data_state) != 3U ||
        !tecmo_team_data_transition_render_enabled(
            asset, &runtime->team_data_state)) {
        set_flow_test_message(message, message_size,
                              "TEAM DATA transition did not begin at full source");
        return false;
    }
    for (uint8_t frame = 1U; frame < stable; ++frame) {
        flow_team_data_neutral(runtime, 1U);
        if (runtime->team_data_state.transition != transition ||
            runtime->team_data_state.transition_frame != frame) {
            set_flow_test_message(message, message_size,
                                  "TEAM DATA transition frame cadence mismatch");
            return false;
        }
        if (frame == black &&
            tecmo_team_data_transition_palette_stage(
                asset, &runtime->team_data_state) != 4U)
            return false;
        if (frame == off &&
            tecmo_team_data_transition_render_enabled(
                asset, &runtime->team_data_state))
            return false;
        if (frame == on &&
            (!tecmo_team_data_transition_render_enabled(
                 asset, &runtime->team_data_state) ||
             tecmo_team_data_transition_palette_stage(
                 asset, &runtime->team_data_state) != 4U))
            return false;
        if (frame >= first && (uint8_t)(frame - first) % step == 0U) {
            unsigned expected = (unsigned)(frame - first) / step;
            if (expected > 3U) expected = 3U;
            if (tecmo_team_data_transition_palette_stage(
                    asset, &runtime->team_data_state) != expected)
                return false;
        }
    }
    flow_team_data_neutral(runtime, 1U);
    if (runtime->team_data_state.transition !=
            TECMO_TEAM_DATA_TRANSITION_NONE ||
        runtime->team_data_state.transition_frame != 0U ||
        runtime->team_data_state.phase != target_phase ||
        runtime->team_data_state.cursor_delay != 1U) {
        set_flow_test_message(message, message_size,
                              "TEAM DATA transition did not land on stable target");
        return false;
    }
    return true;
}

static bool flow_team_data_wait_cooldown(TecmoRuntime *runtime,
                                         char *message,
                                         size_t message_size)
{
    size_t frames = 0U;
    while (runtime->team_data_state.direction_cooldown != 0U && frames < 32U) {
        flow_team_data_neutral(runtime, 1U);
        ++frames;
    }
    if (runtime->team_data_state.direction_cooldown != 0U) {
        set_flow_test_message(message, message_size,
                              "TEAM DATA direction cooldown did not expire");
        return false;
    }
    return true;
}

static void flow_team_data_direction(TecmoRuntime *runtime,
                                     TecmoControlButton control)
{
    TecmoInput input = {0};
    tecmo_input_set_button(&input, control, true);
    tecmo_runtime_update(runtime, &input);
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
}

static bool flow_expect_team_data_native_path(TecmoRuntime *runtime,
                                              bool from_season,
                                              bool exhaustive,
                                              char *message,
                                              size_t message_size)
{
    const TecmoTeamDataAsset *asset = &runtime->team_data_asset;
    if (runtime->mode != TECMO_MODE_TEAM_DATA || !asset->available ||
        !runtime->normal_play_active || !runtime->start_menu_return_pending ||
        runtime->start_menu_return_from_season != from_season ||
        runtime->start_menu_return_root_selection !=
            (from_season ? 1U : 3U) ||
        runtime->start_menu_return_season_selection !=
            (from_season ? 5U : 0U) ||
        runtime->start_menu_return_music_value != 0U ||
        runtime->start_menu_return_speed_value != 0U ||
        runtime->start_menu_return_period_index !=
            (from_season ? 2U : 3U) ||
        runtime->team_data_state.phase != TECMO_TEAM_DATA_TEAM_SELECT ||
        runtime->team_data_state.selector_index != 0U ||
        runtime->team_data_state.team_id != 28U ||
        runtime->team_data_state.transition !=
            TECMO_TEAM_DATA_TRANSITION_ENTRY_TO_SELECTOR ||
        runtime->team_data_state.transition_frame != 0U ||
        runtime->team_data_state.direction_cooldown !=
            asset->selector_initial_cooldown) {
        set_flow_test_message(message, message_size,
                              "TEAM DATA dispatch did not initialize strict TTDT-1 state");
        return false;
    }
    if (!flow_team_data_finish_transition(
            runtime, TECMO_TEAM_DATA_TRANSITION_ENTRY_TO_SELECTOR,
            TECMO_TEAM_DATA_TEAM_SELECT, message, message_size))
        return false;

    if (exhaustive) {
        TecmoTeamDataState invalid = runtime->team_data_state;
        TecmoTeamDataState before;
        TecmoControlFrame controls = {0};
        TecmoFramebuffer framebuffer;
        uint32_t *pixels;
        uint8_t logo_pairs[TECMO_TEAM_DATA_LOGO_CELL_LIMIT * 2U];
        static const uint8_t atl_meter_lengths[6] = {
            80U, 58U, 94U, 84U, 55U, 20U
        };
        const TecmoTeamDataTeam *atl = &asset->teams[0];
        const TecmoTeamDataPlayer *atl_player = &asset->players[0][0];
        if (atl->logo_width != 10U || atl->logo_height != 6U ||
            atl->logo_count != 60U || atl->logo_x != 16U ||
            asset->logo_y != 48U || atl->logo_selector != 0xE4U ||
            atl->profile_palette_group != 1U ||
            flow_fnv1a32(asset->profile_palettes[1], 16U) != 0x34F6B8DCU) {
            set_flow_test_message(message, message_size,
                                  "TEAM DATA ATL logo/palette contract mismatch");
            return false;
        }
        for (size_t i = 0U; i < TECMO_TEAM_DATA_LOGO_CELL_LIMIT; ++i) {
            logo_pairs[i * 2U] = asset->logos[0][i].tile_id;
            logo_pairs[i * 2U + 1U] = asset->logos[0][i].palette_index;
        }
        if (flow_fnv1a32(logo_pairs, sizeof(logo_pairs)) != 0x6F28E5C6U)
            return false;
        if (strcmp(tecmo_team_data_position_name(atl_player->attributes[0]),
                   "GUARD") != 0 ||
            strcmp(tecmo_team_data_condition_name(atl_player->condition_seed),
                   "EXCELLENT") != 0)
            return false;
        for (size_t meter = 0U; meter < 6U; ++meter)
            if (tecmo_team_data_meter_fill_length(atl_player->profile, meter) !=
                atl_meter_lengths[meter])
                return false;
        if (strcmp(tecmo_team_data_position_name(2U), "FORWARD") != 0 ||
            strcmp(tecmo_team_data_position_name(4U), "CENTER") != 0 ||
            tecmo_team_data_position_name(5U)[0] != '\0' ||
            strcmp(tecmo_team_data_condition_name(0x5EU), "EXCELLENT") != 0 ||
            strcmp(tecmo_team_data_condition_name(0x4CU), "GOOD") != 0 ||
            strcmp(tecmo_team_data_condition_name(0x38U), "AVERAGE") != 0 ||
            strcmp(tecmo_team_data_condition_name(0x1FU), "POOR") != 0 ||
            strcmp(tecmo_team_data_condition_name(1U), "BAD") != 0 ||
            strcmp(tecmo_team_data_condition_name(0U), "INJURED") != 0)
            return false;
        invalid.team_id = TECMO_TEAM_DATA_TEAM_COUNT;
        before = invalid;
        if (tecmo_team_data_update(&invalid, asset, &controls) !=
                TECMO_TEAM_DATA_ACTION_NONE ||
            memcmp(&invalid, &before, sizeof(invalid)) != 0) {
            set_flow_test_message(message, message_size,
                                  "TEAM DATA update accepted an invalid state");
            return false;
        }
        pixels = (uint32_t *)malloc(640U * 480U * sizeof(*pixels));
        if (pixels == NULL) {
            set_flow_test_message(message, message_size,
                                  "TEAM DATA invalid-state framebuffer allocation failed");
            return false;
        }
        for (size_t i = 0U; i < 640U * 480U; ++i) pixels[i] = 0x12345678U;
        framebuffer.pixels = pixels;
        framebuffer.width = 640;
        framebuffer.height = 480;
        framebuffer.pitch_pixels = 640;
        if (tecmo_team_data_draw(&framebuffer, asset, &invalid,
                                 runtime->title_chr_bytes,
                                 runtime->title_chr_byte_count,
                                 64, 0, 2)) {
            free(pixels);
            set_flow_test_message(message, message_size,
                                  "TEAM DATA draw accepted an invalid state");
            return false;
        }
        for (size_t i = 0U; i < 640U * 480U; ++i)
            if (pixels[i] != 0x12345678U) {
                free(pixels);
                set_flow_test_message(message, message_size,
                                      "TEAM DATA rejected draw changed output");
                return false;
            }
        free(pixels);

        if (!flow_team_data_wait_cooldown(runtime, message, message_size))
            return false;
        flow_team_data_direction(runtime, TECMO_CONTROL_DOWN);
        if (runtime->team_data_state.selector_index != 1U ||
            !flow_team_data_wait_cooldown(runtime, message, message_size))
            return false;
        flow_team_data_direction(runtime, TECMO_CONTROL_DOWN);
        if (runtime->team_data_state.selector_index != 2U ||
            asset->selectors[2].team_id != 0U)
            return false;
        flow_team_data_release(runtime, true);
        if (runtime->team_data_state.team_id != 0U ||
            !flow_team_data_finish_transition(
                runtime, TECMO_TEAM_DATA_TRANSITION_SELECTOR_TO_PROFILE,
                TECMO_TEAM_DATA_PROFILE, message, message_size) ||
            runtime->team_data_state.profile_selection != 0U) {
            set_flow_test_message(message, message_size,
                                  "TEAM DATA ATL selector did not enter profile");
            return false;
        }
        flow_team_data_release(runtime, true);
        if (runtime->team_data_state.phase != TECMO_TEAM_DATA_ROSTER ||
            runtime->team_data_state.roster_page != 0U ||
            runtime->team_data_state.roster_row != 0U ||
            runtime->team_data_state.cursor_delay != 0U) {
            set_flow_test_message(message, message_size,
                                  "TEAM DATA PLAYERS DATA did not enter roster");
            return false;
        }
        flow_team_data_neutral(runtime, 1U);
        if (runtime->team_data_state.cursor_delay != 1U) return false;
        if (!flow_team_data_wait_cooldown(runtime, message, message_size))
            return false;
        {
            TecmoInput input = {0};
            input.right = true;
            tecmo_runtime_update(runtime, &input);
        }
        if (runtime->team_data_state.slide_direction != 1 ||
            runtime->team_data_state.slide_frame != 0U ||
            runtime->team_data_state.slide_from_page != 0U ||
            runtime->team_data_state.slide_to_page != 1U) {
            set_flow_test_message(message, message_size,
                                  "TEAM DATA roster slide did not begin at frame zero");
            return false;
        }
        flow_team_data_neutral(runtime, 16U);
        if (runtime->team_data_state.slide_frame != 16U ||
            runtime->team_data_state.roster_page != 0U) {
            set_flow_test_message(message, message_size,
                                  "TEAM DATA roster slide midpoint mismatch");
            return false;
        }
        flow_team_data_neutral(runtime, 16U);
        if (runtime->team_data_state.slide_direction != 0 ||
            runtime->team_data_state.slide_frame != 0U ||
            runtime->team_data_state.roster_page != 1U) {
            set_flow_test_message(message, message_size,
                                  "TEAM DATA roster slide did not land on page two");
            return false;
        }
        flow_team_data_release(runtime, true);
        if (runtime->team_data_state.player_index != 6U ||
            !flow_team_data_finish_transition(
                runtime, TECMO_TEAM_DATA_TRANSITION_ROSTER_TO_DETAIL,
                TECMO_TEAM_DATA_PLAYER_DETAIL, message, message_size)) {
            set_flow_test_message(message, message_size,
                                  "TEAM DATA page-two row did not enter player detail");
            return false;
        }
        flow_team_data_release(runtime, false);
        if (!flow_team_data_finish_transition(
                runtime, TECMO_TEAM_DATA_TRANSITION_DETAIL_TO_ROSTER,
                TECMO_TEAM_DATA_ROSTER, message, message_size) ||
            runtime->team_data_state.roster_page != 1U ||
            runtime->team_data_state.roster_row != 0U)
            return false;
        flow_team_data_release(runtime, false);
        if (runtime->team_data_state.phase != TECMO_TEAM_DATA_PROFILE)
            return false;
        flow_team_data_release(runtime, false);
        if (!flow_team_data_finish_transition(
                runtime, TECMO_TEAM_DATA_TRANSITION_PROFILE_TO_SELECTOR,
                TECMO_TEAM_DATA_TEAM_SELECT, message, message_size) ||
            runtime->team_data_state.selector_index != 2U)
            return false;
    }

    runtime->start_game_menu_state.music_value = 1U;
    runtime->start_game_menu_state.speed_value = 2U;
    runtime->start_game_menu_state.period_index = 4U;
    flow_team_data_release(runtime, false);
    if (runtime->mode != TECMO_MODE_START_GAME_MENU ||
        !runtime->normal_play_active || runtime->start_menu_return_pending ||
        !runtime->start_menu_input_neutral_gate ||
        runtime->start_game_menu_state.frame !=
            runtime->start_game_menu_asset.stable_frame ||
        runtime->start_game_menu_state.direction_cooldown !=
            runtime->start_game_menu_asset.accepted_input_seed ||
        runtime->start_game_menu_state.cursor_delay !=
            runtime->start_game_menu_asset.cursor_commit_delay_frames ||
        runtime->start_game_menu_state.phase !=
            (from_season ? TECMO_START_GAME_MENU_SEASON
                         : TECMO_START_GAME_MENU_ROOT) ||
        runtime->start_game_menu_state.root_selection !=
            (from_season ? 1U : 3U) ||
        runtime->start_game_menu_state.season_selection !=
            (from_season ? 5U : 0U) ||
        runtime->start_game_menu_state.slide_frame !=
            (from_season ? runtime->start_game_menu_asset.slide_frames : 0U) ||
        runtime->start_game_menu_state.music_value != 0U ||
        runtime->start_game_menu_state.speed_value != 0U ||
        runtime->start_game_menu_state.period_index !=
            (from_season ? 2U : 3U)) {
        set_flow_test_message(message, message_size,
                              "TEAM DATA B did not restore its exact blue-menu origin");
        return false;
    }
    {
        TecmoInput input = {0};
        input.cancel = true;
        tecmo_runtime_update(runtime, &input);
        if (!runtime->start_menu_input_neutral_gate ||
            runtime->start_game_menu_state.frame !=
                runtime->start_game_menu_asset.stable_frame) {
            set_flow_test_message(message, message_size,
                                  "TEAM DATA return did not swallow held B");
            return false;
        }
        memset(&input, 0, sizeof(input));
        tecmo_runtime_update(runtime, &input);
        if (!runtime->start_menu_input_neutral_gate ||
            runtime->start_game_menu_state.frame !=
                runtime->start_game_menu_asset.stable_frame) {
            set_flow_test_message(message, message_size,
                                  "TEAM DATA return re-consumed B release");
            return false;
        }
        tecmo_runtime_update(runtime, &input);
        if (runtime->start_menu_input_neutral_gate ||
            runtime->start_game_menu_state.frame !=
                runtime->start_game_menu_asset.stable_frame) {
            set_flow_test_message(message, message_size,
                                  "TEAM DATA return did not settle neutral gate");
            return false;
        }
        tecmo_runtime_update(runtime, &input);
        if (runtime->start_game_menu_state.frame !=
                runtime->start_game_menu_asset.stable_frame + 1U) {
            set_flow_test_message(message, message_size,
                                  "TEAM DATA return did not resume stable menu");
            return false;
        }
    }
    return true;
}

bool tecmo_runtime_flow_self_test(TecmoRuntime *runtime, char *message, size_t message_size)
{
    TecmoInput input;
    TecmoMusicPlayer speed_music_before;
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
    input.right = true;
    flow_step(runtime, input);
    if (runtime->intro_output_step != FLOW_INTRO_LICENSE_STEP) {
        set_flow_test_message(message, message_size,
                              "debug intro RIGHT no longer advances the inspected step");
        return false;
    }
    memset(&input, 0, sizeof(input));
    input.left = true;
    flow_step(runtime, input);
    if (runtime->intro_output_step != FLOW_INTRO_TITLE_STEP) {
        set_flow_test_message(message, message_size,
                              "debug intro LEFT no longer rewinds the inspected step");
        return false;
    }
    memset(&input, 0, sizeof(input));
    input.cancel = true;
    flow_step(runtime, input);
    if (runtime->mode != TECMO_MODE_MAIN_MENU) {
        set_flow_test_message(message, message_size,
                              "debug intro B did not preserve the modern menu return");
        return false;
    }
    runtime->normal_play_active = true;
    tecmo_runtime_set_mode(runtime, TECMO_MODE_FIRST_SPRITE);
    runtime->intro_output_step = FLOW_INTRO_LICENSE_STEP;
    runtime->mode_frame_counter = 10U;
    memset(&input, 0, sizeof(input));
    input.left = true;
    flow_step(runtime, input);
    if (runtime->intro_output_step != FLOW_INTRO_LICENSE_STEP ||
        runtime->mode_frame_counter != 12U) {
        set_flow_test_message(message, message_size,
                              "normal-play LEFT scrubbed or restarted the intro");
        return false;
    }
    memset(&input, 0, sizeof(input));
    input.right = true;
    flow_step(runtime, input);
    if (runtime->intro_output_step != FLOW_INTRO_LICENSE_STEP ||
        runtime->mode_frame_counter != 14U) {
        set_flow_test_message(message, message_size,
                              "normal-play RIGHT scrubbed or restarted the intro");
        return false;
    }
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
    runtime->start_game_menu_state.music_value = 0U;
    runtime->start_game_menu_state.speed_value = 2U;
    runtime->start_game_menu_state.period_index = 4U;
    memset(&input, 0, sizeof(input));
    input.shoot = true;
    flow_step(runtime, input);
    if (!flow_expect_mode(runtime, TECMO_MODE_PLAY_SETUP,
                          "root ALL STAR submenu-boundary dispatch",
                          message, message_size)) return false;
    memset(&input, 0, sizeof(input));
    input.confirm = true;
    flow_step(runtime, input);
    if (!flow_expect_mode(runtime, TECMO_MODE_COURT,
                          "normal ALL STAR placeholder court entry",
                          message, message_size)) return false;
    memset(&input, 0, sizeof(input));
    input.cancel = true;
    flow_step(runtime, input);
    if (!flow_expect_mode(runtime, TECMO_MODE_PLAY_SETUP,
                          "normal ALL STAR court cancel",
                          message, message_size)) return false;
    if (!flow_expect_placeholder_return(runtime, false, 2U, 0U,
                                        "normal ALL STAR blue-menu return",
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
    runtime->start_game_menu_state.music_value = 0U;
    runtime->start_game_menu_state.speed_value = 0U;
    runtime->start_game_menu_state.period_index = 3U;
    memset(&input, 0, sizeof(input));
    input.shoot = true;
    flow_step(runtime, input);
    if (!flow_finish_start_menu_exit(runtime, TECMO_MODE_TEAM_DATA, false,
                                     "root A team-data frame-eleven dispatch",
                                     message, message_size)) return false;
    if (!flow_expect_team_data_native_path(runtime, false, true,
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
    speed_music_before = runtime->music_player;
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_SPEED ||
        runtime->start_game_menu_state.setting_selection != 0U ||
        runtime->start_game_menu_state.direction_cooldown != 7U) {
        set_flow_test_message(message, message_size, "SPEED A+B+Up+Down press was not direction-consumed");
        return false;
    }
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
    if (speed_music_before.asset != runtime->music_player.asset ||
        speed_music_before.sample_tick_accumulator !=
            runtime->music_player.sample_tick_accumulator ||
        speed_music_before.ticks_elapsed != runtime->music_player.ticks_elapsed ||
        speed_music_before.current_track_id !=
            runtime->music_player.current_track_id ||
        speed_music_before.pending_track_id !=
            runtime->music_player.pending_track_id ||
        speed_music_before.playing != runtime->music_player.playing ||
        speed_music_before.track_pending != runtime->music_player.track_pending ||
        speed_music_before.game_music_enabled !=
            runtime->music_player.game_music_enabled ||
        speed_music_before.opening_queued !=
            runtime->music_player.opening_queued ||
        speed_music_before.render_guard_failed !=
            runtime->music_player.render_guard_failed ||
        runtime->music_asset.tick_numerator != TECMO_MUSIC_TICK_NUMERATOR ||
        runtime->music_asset.tick_denominator != TECMO_MUSIC_TICK_DENOMINATOR) {
        set_flow_test_message(message, message_size,
                              "GAME SPEED changed native music cadence/state");
        return false;
    }
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
    runtime->start_game_menu_state.root_selection = 1U;
    runtime->start_game_menu_state.season_selection = 2U;
    runtime->start_game_menu_state.music_value = 0U;
    runtime->start_game_menu_state.speed_value = 2U;
    runtime->start_game_menu_state.period_index = 4U;
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
    if (!flow_expect_placeholder_return(runtime, true, 1U, 2U,
                                        "normal season GAME START blue-menu return",
                                        message, message_size)) return false;

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 64U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_SEASON;
    runtime->start_game_menu_state.slide_frame = 32U;
    runtime->start_game_menu_state.root_selection = 1U;
    runtime->start_game_menu_state.season_selection = 5U;
    runtime->start_game_menu_state.music_value = 0U;
    runtime->start_game_menu_state.speed_value = 0U;
    runtime->start_game_menu_state.period_index = 2U;
    memset(&input, 0, sizeof(input));
    input.shoot = true;
    flow_step(runtime, input);
    if (!flow_finish_start_menu_exit(runtime, TECMO_MODE_TEAM_DATA, true,
                                     "season TEAM DATA frame-eleven dispatch",
                                     message, message_size)) return false;
    if (!flow_expect_team_data_native_path(runtime, true, false,
                                           message, message_size)) return false;

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    tecmo_runtime_set_mode(runtime, TECMO_MODE_PLAY_SETUP);
    memset(&input, 0, sizeof(input));
    input.cancel = true;
    tecmo_runtime_update(runtime, &input);
    if (!flow_expect_mode(runtime, TECMO_MODE_PLAY_SETUP,
                          "normal PLAY SETUP without a return route",
                          message, message_size)) return false;

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    tecmo_runtime_set_mode(runtime, TECMO_MODE_ROSTERS);
    memset(&input, 0, sizeof(input));
    input.cancel = true;
    tecmo_runtime_update(runtime, &input);
    if (!flow_expect_mode(runtime, TECMO_MODE_ROSTERS,
                          "normal ROSTERS without a return route",
                          message, message_size)) return false;

    tecmo_runtime_set_mode(runtime, TECMO_MODE_MAIN_MENU);
    tecmo_runtime_set_mode(runtime, TECMO_MODE_PLAY_SETUP);
    memset(&input, 0, sizeof(input));
    input.cancel = true;
    tecmo_runtime_update(runtime, &input);
    if (!flow_expect_mode(runtime, TECMO_MODE_MAIN_MENU,
                          "debug PLAY SETUP modern-menu return",
                          message, message_size)) return false;

    tecmo_runtime_set_mode(runtime, TECMO_MODE_ROSTERS);
    memset(&input, 0, sizeof(input));
    input.cancel = true;
    tecmo_runtime_update(runtime, &input);
    if (!flow_expect_mode(runtime, TECMO_MODE_MAIN_MENU,
                          "debug ROSTERS modern-menu return",
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
