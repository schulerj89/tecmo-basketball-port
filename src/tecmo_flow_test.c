#include "tecmo_game.h"
#include "tecmo_intro_stage.h"

#include <stdio.h>
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
    if (!flow_expect_mode(runtime, TECMO_MODE_PLAY_SETUP,
                          "root A-release grace dispatch", message, message_size)) return false;

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
    if (!flow_expect_mode(runtime, TECMO_MODE_ROSTERS,
                          "root A team-data dispatch", message, message_size)) return false;

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
        runtime->start_game_menu_state.direction_cooldown != 4U) {
        set_flow_test_message(message, message_size, "season slide did not end at frame 32");
        return false;
    }
    input.cancel = true;
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_SEASON ||
        runtime->start_game_menu_state.direction_cooldown != 3U) {
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
        runtime->start_game_menu_state.direction_cooldown != 4U) {
        set_flow_test_message(message, message_size, "season reverse did not restore root at frame 32");
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
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_MUSIC ||
        runtime->start_game_menu_state.setting_selection != 1U ||
        runtime->start_game_menu_state.direction_cooldown != 5U) {
        set_flow_test_message(message, message_size, "GAME MUSIC did not open its native popup");
        return false;
    }
    input.shoot = true;
    tecmo_runtime_update(runtime, &input);
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_MUSIC ||
        runtime->start_game_menu_state.direction_cooldown != 3U) {
        set_flow_test_message(message, message_size, "held popup A activated before release");
        return false;
    }
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_ROOT ||
        runtime->start_game_menu_state.music_value != 1U ||
        runtime->start_game_menu_state.direction_cooldown != 5U) {
        set_flow_test_message(message, message_size, "popup A release did not accept");
        return false;
    }
    input.shoot = true;
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_ROOT ||
        runtime->start_game_menu_state.direction_cooldown != 4U) {
        set_flow_test_message(message, message_size, "held root A reopened popup before release");
        return false;
    }
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_MUSIC ||
        runtime->start_game_menu_state.direction_cooldown != 5U) {
        set_flow_test_message(message, message_size, "root A release did not bypass directional E1");
        return false;
    }
    for (size_t wait = 0U; wait < 5U; ++wait) tecmo_runtime_update(runtime, &input);
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
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_ROOT ||
        runtime->start_game_menu_state.music_value != 1U ||
        runtime->start_game_menu_state.direction_cooldown != 5U) {
        set_flow_test_message(message, message_size, "GAME MUSIC B did not cancel its edit");
        return false;
    }

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 32U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_MUSIC;
    runtime->start_game_menu_state.setting_selection = 0U;
    runtime->previous_input.cancel = true;
    memset(&input, 0, sizeof(input));
    tecmo_runtime_update(runtime, &input);
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_ROOT ||
        runtime->start_game_menu_state.music_value != 1U ||
        runtime->start_game_menu_state.direction_cooldown != 5U) {
        set_flow_test_message(message, message_size, "GAME MUSIC B-release grace did not cancel");
        return false;
    }

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
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_ROOT ||
        runtime->start_game_menu_state.speed_value != 0U ||
        runtime->start_game_menu_state.direction_cooldown != 5U) {
        set_flow_test_message(message, message_size, "SPEED A+B release did not accept with A priority");
        return false;
    }

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
    if (runtime->start_game_menu_state.phase != TECMO_START_GAME_MENU_ROOT ||
        runtime->start_game_menu_state.period_index != 2U ||
        runtime->start_game_menu_state.direction_cooldown != 5U) {
        set_flow_test_message(message, message_size, "PERIOD A did not accept after direction grace");
        return false;
    }

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
    if (!flow_expect_mode(runtime, TECMO_MODE_PLAY_SETUP,
                          "season A+B release GAME START dispatch", message, message_size)) return false;

    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
    runtime->start_game_menu_state.frame = 64U;
    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_SEASON;
    runtime->start_game_menu_state.slide_frame = 32U;
    runtime->start_game_menu_state.season_selection = 5U;
    memset(&input, 0, sizeof(input));
    input.shoot = true;
    flow_step(runtime, input);
    if (!flow_expect_mode(runtime, TECMO_MODE_ROSTERS,
                          "season TEAM DATA dispatch", message, message_size)) return false;

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
                          "FLOW TEST PASS: menu play-intro title start-game-menu quit");
    return true;
}
