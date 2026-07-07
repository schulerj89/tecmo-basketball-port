#include "tecmo_game.h"

#include <stdio.h>
#include <string.h>

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

static bool flow_press_menu_up(TecmoRuntime *runtime,
                               size_t count,
                               size_t expected,
                               const char *label,
                               char *message,
                               size_t message_size)
{
    TecmoInput input;

    for (size_t i = 0; i < count; ++i) {
        memset(&input, 0, sizeof(input));
        input.up = true;
        flow_step(runtime, input);
    }
    return flow_expect_menu_item(runtime, expected, label, message, message_size);
}

static size_t flow_selected_team_player_count(const TecmoRuntime *runtime)
{
    size_t count = 0;

    if (runtime->team_count == 0U || runtime->selected_team >= runtime->team_count) {
        return 0U;
    }
    for (size_t i = 0; i < runtime->roster.count; ++i) {
        if (strcmp(runtime->roster.records[i].team, runtime->teams[runtime->selected_team]) == 0) {
            ++count;
        }
    }
    return count;
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
    if (runtime->team_count < 2U || runtime->roster.count < 2U) {
        set_flow_test_message(message, message_size, "not enough roster data to browse teams and players");
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

    if (!flow_press_menu_down(runtime, 4U, 4U, "navigate to rosters", message, message_size)) {
        return false;
    }
    memset(&input, 0, sizeof(input));
    input.confirm = true;
    flow_step(runtime, input);
    if (!flow_expect_mode(runtime, TECMO_MODE_ROSTERS, "enter rosters", message, message_size)) {
        return false;
    }
    if (flow_selected_team_player_count(runtime) < 2U) {
        set_flow_test_message(message, message_size, "selected roster has fewer than two players");
        return false;
    }
    memset(&input, 0, sizeof(input));
    input.down = true;
    flow_step(runtime, input);
    if (runtime->selected_player != 1U || tecmo_cpu_ram_read(runtime->memory, 0x0002) != 1U) {
        set_flow_test_message(message, message_size, "roster player navigation failed");
        return false;
    }
    memset(&input, 0, sizeof(input));
    input.cancel = true;
    flow_step(runtime, input);
    if (!flow_expect_mode(runtime, TECMO_MODE_MAIN_MENU, "exit rosters", message, message_size)) {
        return false;
    }

    if (!flow_press_menu_up(runtime, 1U, 3U, "navigate to play game", message, message_size)) {
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
                                          180U,
                                          "play game did not advance to arena intro step",
                                          message,
                                          message_size)) {
        return false;
    }

    memset(&input, 0, sizeof(input));
    input.cancel = true;
    flow_step(runtime, input);
    if (!flow_expect_mode(runtime, TECMO_MODE_MAIN_MENU, "first sprite cancel", message, message_size)) {
        return false;
    }

    if (!flow_press_menu_down(runtime, 2U, 5U, "navigate to quit", message, message_size)) {
        return false;
    }
    memset(&input, 0, sizeof(input));
    input.confirm = true;
    flow_step(runtime, input);
    if (!runtime->quit_requested) {
        set_flow_test_message(message, message_size, "quit menu item did not request quit");
        return false;
    }

    set_flow_test_message(message, message_size, "FLOW TEST PASS: menu rosters play-intro quit");
    return true;
}
