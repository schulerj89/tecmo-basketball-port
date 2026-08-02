#include "tecmo_win32_keys.h"

#include <stdio.h>
#include <string.h>

static void set_test_message(char *dest, size_t dest_size, const char *text)
{
    if (dest == NULL || dest_size == 0U) {
        return;
    }
    if (text == NULL) {
        text = "";
    }
    (void)snprintf(dest, dest_size, "%s", text);
}

bool tecmo_win32_translate_key(uint32_t virtual_key,
                               TecmoWin32KeyBinding *binding_out)
{
    TecmoWin32KeyBinding binding = {0U, TECMO_CONTROL_COUNT};

    if (binding_out == NULL) {
        return false;
    }

    switch (virtual_key) {
    case TECMO_WIN32_VK_NUMPAD8:
        binding.player_index = 1U;
        binding.button = TECMO_CONTROL_UP;
        break;
    case TECMO_WIN32_VK_NUMPAD2:
        binding.player_index = 1U;
        binding.button = TECMO_CONTROL_DOWN;
        break;
    case TECMO_WIN32_VK_NUMPAD4:
        binding.player_index = 1U;
        binding.button = TECMO_CONTROL_LEFT;
        break;
    case TECMO_WIN32_VK_NUMPAD6:
        binding.player_index = 1U;
        binding.button = TECMO_CONTROL_RIGHT;
        break;
    case TECMO_WIN32_VK_NUMPAD9:
        binding.player_index = 1U;
        binding.button = TECMO_CONTROL_CONFIRM;
        break;
    case TECMO_WIN32_VK_NUMPAD3:
        binding.player_index = 1U;
        binding.button = TECMO_CONTROL_CANCEL;
        break;
    case TECMO_WIN32_VK_NUMPAD1:
        binding.player_index = 1U;
        binding.button = TECMO_CONTROL_SHOOT;
        break;
    case TECMO_WIN32_VK_NUMPAD7:
        binding.player_index = 1U;
        binding.button = TECMO_CONTROL_TAB;
        break;
    case TECMO_WIN32_VK_UP:
        binding.button = TECMO_CONTROL_UP;
        break;
    case TECMO_WIN32_VK_DOWN:
        binding.button = TECMO_CONTROL_DOWN;
        break;
    case TECMO_WIN32_VK_LEFT:
        binding.button = TECMO_CONTROL_LEFT;
        break;
    case TECMO_WIN32_VK_RIGHT:
        binding.button = TECMO_CONTROL_RIGHT;
        break;
    case TECMO_WIN32_VK_RETURN:
        binding.button = TECMO_CONTROL_CONFIRM;
        break;
    case 'X':
        binding.button = TECMO_CONTROL_CANCEL;
        break;
    case 'Z':
        binding.button = TECMO_CONTROL_SHOOT;
        break;
    case TECMO_WIN32_VK_SHIFT:
    case TECMO_WIN32_VK_LSHIFT:
    case TECMO_WIN32_VK_RSHIFT:
    case TECMO_WIN32_VK_SPACE:
        binding.button = TECMO_CONTROL_TAB;
        break;
    case 'Q':
        binding.button = TECMO_CONTROL_BANK_PREV;
        break;
    case 'E':
        binding.button = TECMO_CONTROL_BANK_NEXT;
        break;
    case 'T':
        binding.button = TECMO_CONTROL_TABLE_TOGGLE;
        break;
    case 'S':
        binding.button = TECMO_CONTROL_SAVE;
        break;
    case 'R':
        binding.button = TECMO_CONTROL_PRESET_RABBIT;
        break;
    case 'M':
        binding.button = TECMO_CONTROL_PRESET_TECMO;
        break;
    case 'C':
        binding.button = TECMO_CONTROL_PRESET_COMPOSITE;
        break;
    case TECMO_WIN32_VK_BACK:
    case TECMO_WIN32_VK_DELETE:
        binding.button = TECMO_CONTROL_REMOVE;
        break;
    case TECMO_WIN32_VK_F3:
        binding.button = TECMO_CONTROL_DEBUG_TOGGLE;
        break;
    default:
        return false;
    }

    *binding_out = binding;
    return true;
}

void tecmo_win32_keyboard_init(TecmoWin32KeyboardState *state)
{
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
    }
}

static bool keyboard_logical_button_down(
    const TecmoWin32KeyboardState *state,
    unsigned player_index,
    TecmoControlButton button)
{
    if (state == NULL || player_index >= TECMO_WIN32_CONTROLLER_COUNT ||
        button >= TECMO_CONTROL_COUNT) {
        return false;
    }
    for (uint32_t candidate = 0U;
         candidate < TECMO_WIN32_TRACKED_KEY_COUNT;
         ++candidate) {
        TecmoWin32KeyBinding candidate_binding;

        if (state->physical_down[candidate] &&
            tecmo_win32_translate_key(candidate, &candidate_binding) &&
            candidate_binding.player_index == player_index &&
            candidate_binding.button == button) {
            return true;
        }
    }
    return false;
}

bool tecmo_win32_keyboard_update(TecmoWin32KeyboardState *state,
                                 uint32_t virtual_key,
                                 bool physical_down,
                                 TecmoWin32KeyBinding *binding_out,
                                 bool *logical_down_out)
{
    TecmoWin32KeyBinding binding;
    bool logical_was_down;
    bool logical_down;

    if (state == NULL ||
        binding_out == NULL ||
        logical_down_out == NULL ||
        virtual_key >= TECMO_WIN32_TRACKED_KEY_COUNT ||
        !tecmo_win32_translate_key(virtual_key, &binding)) {
        return false;
    }

    logical_was_down = keyboard_logical_button_down(
        state, binding.player_index, binding.button);
    state->physical_down[virtual_key] = physical_down;
    logical_down = keyboard_logical_button_down(
        state, binding.player_index, binding.button);
    if (!logical_was_down && logical_down) {
        state->pending_press[binding.player_index][binding.button] = true;
    }

    *binding_out = binding;
    *logical_down_out = logical_down;
    return true;
}

void tecmo_win32_keyboard_begin_controls_frame(
    TecmoWin32KeyboardState *state,
    TecmoControls *controls,
    size_t control_count)
{
    size_t active_count;
    size_t player;
    unsigned button;

    if (state == NULL || controls == NULL) return;
    active_count = control_count < TECMO_WIN32_CONTROLLER_COUNT
                       ? control_count
                       : TECMO_WIN32_CONTROLLER_COUNT;
    for (player = 0U; player < active_count; ++player) {
        for (button = 0U; button < TECMO_CONTROL_COUNT; ++button) {
            if (state->pending_press[player][button]) {
                tecmo_controls_set_button(
                    &controls[player], (TecmoControlButton)button, true);
            }
        }
        tecmo_controls_begin_frame(&controls[player]);
    }
}

void tecmo_win32_keyboard_end_controls_frame(
    TecmoWin32KeyboardState *state,
    TecmoControls *controls,
    size_t control_count)
{
    size_t active_count;
    size_t player;
    unsigned button;

    if (state == NULL || controls == NULL) return;
    active_count = control_count < TECMO_WIN32_CONTROLLER_COUNT
                       ? control_count
                       : TECMO_WIN32_CONTROLLER_COUNT;
    for (player = 0U; player < active_count; ++player) {
        for (button = 0U; button < TECMO_CONTROL_COUNT; ++button) {
            if (state->pending_press[player][button]) {
                tecmo_controls_set_button(
                    &controls[player], (TecmoControlButton)button,
                    keyboard_logical_button_down(
                        state, (unsigned)player,
                        (TecmoControlButton)button));
                state->pending_press[player][button] = false;
            }
        }
    }
}

static bool expect_binding(uint32_t virtual_key,
                           unsigned player_index,
                           TecmoControlButton button)
{
    TecmoWin32KeyBinding binding = {99U, TECMO_CONTROL_COUNT};

    return tecmo_win32_translate_key(virtual_key, &binding) &&
           binding.player_index == player_index &&
           binding.button == button;
}

bool tecmo_win32_keys_self_test(char *message, size_t message_size)
{
    static const struct {
        uint32_t key;
        TecmoControlButton button;
    } player_one_cases[] = {
        {TECMO_WIN32_VK_UP, TECMO_CONTROL_UP},
        {TECMO_WIN32_VK_DOWN, TECMO_CONTROL_DOWN},
        {TECMO_WIN32_VK_LEFT, TECMO_CONTROL_LEFT},
        {TECMO_WIN32_VK_RIGHT, TECMO_CONTROL_RIGHT},
        {'Z', TECMO_CONTROL_SHOOT},
        {'X', TECMO_CONTROL_CANCEL},
        {TECMO_WIN32_VK_RETURN, TECMO_CONTROL_CONFIRM},
        {TECMO_WIN32_VK_SHIFT, TECMO_CONTROL_TAB},
        {TECMO_WIN32_VK_LSHIFT, TECMO_CONTROL_TAB},
        {TECMO_WIN32_VK_RSHIFT, TECMO_CONTROL_TAB},
        {TECMO_WIN32_VK_SPACE, TECMO_CONTROL_TAB}
    };
    static const struct {
        uint32_t key;
        TecmoControlButton button;
    } player_two_cases[] = {
        {TECMO_WIN32_VK_NUMPAD8, TECMO_CONTROL_UP},
        {TECMO_WIN32_VK_NUMPAD2, TECMO_CONTROL_DOWN},
        {TECMO_WIN32_VK_NUMPAD4, TECMO_CONTROL_LEFT},
        {TECMO_WIN32_VK_NUMPAD6, TECMO_CONTROL_RIGHT},
        {TECMO_WIN32_VK_NUMPAD1, TECMO_CONTROL_SHOOT},
        {TECMO_WIN32_VK_NUMPAD3, TECMO_CONTROL_CANCEL},
        {TECMO_WIN32_VK_NUMPAD9, TECMO_CONTROL_CONFIRM},
        {TECMO_WIN32_VK_NUMPAD7, TECMO_CONTROL_TAB}
    };
    TecmoWin32KeyBinding sentinel = {7U, TECMO_CONTROL_DEBUG_TOGGLE};
    TecmoControls controls;
    TecmoWin32KeyboardState keyboard;
    bool logical_down = false;

    for (size_t i = 0U;
         i < sizeof(player_one_cases) / sizeof(player_one_cases[0]);
         ++i) {
        if (!expect_binding(player_one_cases[i].key, 0U,
                            player_one_cases[i].button)) {
            set_test_message(message, message_size,
                             "PLAYER ONE WIN32 KEY CONTRACT FAILED");
            return false;
        }
    }
    for (size_t i = 0U;
         i < sizeof(player_two_cases) / sizeof(player_two_cases[0]);
         ++i) {
        if (!expect_binding(player_two_cases[i].key, 1U,
                            player_two_cases[i].button)) {
            set_test_message(message, message_size,
                             "PLAYER TWO WIN32 KEY CONTRACT FAILED");
            return false;
        }
    }

    if (tecmo_win32_translate_key(TECMO_WIN32_VK_ESCAPE, &sentinel) ||
        sentinel.player_index != 7U ||
        sentinel.button != TECMO_CONTROL_DEBUG_TOGGLE ||
        tecmo_win32_translate_key(TECMO_WIN32_VK_TAB, &sentinel) ||
        sentinel.player_index != 7U ||
        sentinel.button != TECMO_CONTROL_DEBUG_TOGGLE ||
        tecmo_win32_translate_key('B', &sentinel) ||
        sentinel.player_index != 7U ||
        sentinel.button != TECMO_CONTROL_DEBUG_TOGGLE) {
        set_test_message(message, message_size,
                         "PHYSICAL B OR LEGACY ESCAPE/TAB KEY REMAINS BOUND");
        return false;
    }
    if (tecmo_win32_translate_key('Z', NULL)) {
        set_test_message(message, message_size,
                         "NULL WIN32 KEY OUTPUT WAS ACCEPTED");
        return false;
    }
    if (!expect_binding(TECMO_WIN32_VK_F3, 0U,
                        TECMO_CONTROL_DEBUG_TOGGLE)) {
        set_test_message(message, message_size,
                         "DEBUG KEY CONTRACT FAILED");
        return false;
    }

    tecmo_win32_keyboard_init(&keyboard);
    if (!tecmo_win32_keyboard_update(
            &keyboard, TECMO_WIN32_VK_SHIFT, true,
            &sentinel, &logical_down) ||
        sentinel.player_index != 0U ||
        sentinel.button != TECMO_CONTROL_TAB ||
        !logical_down ||
        !tecmo_win32_keyboard_update(
            &keyboard, TECMO_WIN32_VK_SPACE, true,
            &sentinel, &logical_down) ||
        !logical_down ||
        !tecmo_win32_keyboard_update(
            &keyboard, TECMO_WIN32_VK_SHIFT, false,
            &sentinel, &logical_down) ||
        !logical_down ||
        !tecmo_win32_keyboard_update(
            &keyboard, TECMO_WIN32_VK_SPACE, false,
            &sentinel, &logical_down) ||
        logical_down) {
        set_test_message(message, message_size,
                         "SHIFT SPACE ALIAS HOLD CONTRACT FAILED");
        return false;
    }

    tecmo_win32_keyboard_init(&keyboard);
    if (tecmo_win32_keyboard_update(
            &keyboard, 'B', true, &sentinel, &logical_down) ||
        logical_down ||
        tecmo_win32_keyboard_update(
            &keyboard, 'V', true, &sentinel, &logical_down) ||
        logical_down ||
        !tecmo_win32_keyboard_update(
            &keyboard, 'X', true, &sentinel, &logical_down) ||
        sentinel.player_index != 0U ||
        sentinel.button != TECMO_CONTROL_CANCEL ||
        !logical_down ||
        tecmo_win32_keyboard_update(
            &keyboard, 'B', false, &sentinel, &logical_down) ||
        !tecmo_win32_keyboard_update(
            &keyboard, 'X', false, &sentinel, &logical_down) ||
        logical_down) {
        set_test_message(message, message_size,
                         "PHYSICAL B MUST REMAIN UNMAPPED BESIDE X");
        return false;
    }

    tecmo_controls_init(&controls);
    tecmo_win32_keyboard_init(&keyboard);
    if (!tecmo_win32_keyboard_update(
            &keyboard, 'X', true, &sentinel, &logical_down) ||
        !logical_down ||
        !tecmo_win32_keyboard_update(
            &keyboard, 'X', false, &sentinel, &logical_down) ||
        logical_down || controls.current.cancel ||
        !keyboard.pending_press[0U][TECMO_CONTROL_CANCEL]) {
        set_test_message(message, message_size,
                         "FAST WIN32 X PRESS WAS NOT QUEUED");
        return false;
    }
    tecmo_win32_keyboard_begin_controls_frame(&keyboard, &controls, 1U);
    if (!tecmo_controls_held(&controls)->cancel) {
        set_test_message(message, message_size,
                         "FAST WIN32 X PRESS WAS NOT ONE FRAME");
        return false;
    }
    tecmo_win32_keyboard_end_controls_frame(&keyboard, &controls, 1U);
    if (controls.current.cancel ||
        keyboard.pending_press[0U][TECMO_CONTROL_CANCEL]) {
        set_test_message(message, message_size,
                         "FAST WIN32 X PRESS WAS NOT RESTORED");
        return false;
    }
    tecmo_win32_keyboard_begin_controls_frame(&keyboard, &controls, 1U);
    if (tecmo_controls_held(&controls)->cancel ||
        !tecmo_controls_released(&controls, TECMO_CONTROL_CANCEL)) {
        set_test_message(message, message_size,
                         "FAST WIN32 X PRESS REMAINED HELD");
        return false;
    }
    tecmo_win32_keyboard_end_controls_frame(&keyboard, &controls, 1U);

    set_test_message(message, message_size, "WIN32 KEY SELF TEST PASS");
    return true;
}
