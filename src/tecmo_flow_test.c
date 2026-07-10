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
                              "finale left before terminator hold", message, message_size) ||
        runtime->intro_handoff_complete) {
        set_flow_test_message(message, message_size,
                              "finale marked handoff before the terminator route");
        return false;
    }
    tecmo_runtime_update(runtime, &input);
    if (runtime->intro_output_step != 14U ||
        runtime->mode_frame_counter != tecmo_intro_finale_hold_frame() ||
        runtime->mode != TECMO_MODE_FIRST_SPRITE || !runtime->intro_handoff_complete) {
        set_flow_test_message(message, message_size,
                              "finale did not enter the persistent terminator hold");
        return false;
    }
    if (!flow_hold_intro_step(runtime, 14U, 24U,
                              "terminator fell through to placeholder play setup",
                              message, message_size)) {
        return false;
    }

    memset(&input, 0, sizeof(input));
    input.cancel = true;
    flow_step(runtime, input);
    if (!flow_expect_mode(runtime, TECMO_MODE_MAIN_MENU, "first sprite cancel", message, message_size)) {
        return false;
    }

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

    set_flow_test_message(message, message_size, "FLOW TEST PASS: menu play-intro quit");
    return true;
}
