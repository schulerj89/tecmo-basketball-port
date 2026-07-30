#ifndef TECMO_WIN32_KEYS_H
#define TECMO_WIN32_KEYS_H

#include "tecmo_controls.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum TecmoWin32VirtualKey {
    TECMO_WIN32_VK_BACK = 0x08,
    TECMO_WIN32_VK_TAB = 0x09,
    TECMO_WIN32_VK_RETURN = 0x0D,
    TECMO_WIN32_VK_SHIFT = 0x10,
    TECMO_WIN32_VK_ESCAPE = 0x1B,
    TECMO_WIN32_VK_SPACE = 0x20,
    TECMO_WIN32_VK_LEFT = 0x25,
    TECMO_WIN32_VK_UP = 0x26,
    TECMO_WIN32_VK_RIGHT = 0x27,
    TECMO_WIN32_VK_DOWN = 0x28,
    TECMO_WIN32_VK_DELETE = 0x2E,
    TECMO_WIN32_VK_NUMPAD0 = 0x60,
    TECMO_WIN32_VK_NUMPAD1 = 0x61,
    TECMO_WIN32_VK_NUMPAD2 = 0x62,
    TECMO_WIN32_VK_NUMPAD3 = 0x63,
    TECMO_WIN32_VK_NUMPAD4 = 0x64,
    TECMO_WIN32_VK_NUMPAD5 = 0x65,
    TECMO_WIN32_VK_NUMPAD6 = 0x66,
    TECMO_WIN32_VK_NUMPAD7 = 0x67,
    TECMO_WIN32_VK_NUMPAD8 = 0x68,
    TECMO_WIN32_VK_NUMPAD9 = 0x69,
    TECMO_WIN32_VK_F3 = 0x72,
    TECMO_WIN32_VK_LSHIFT = 0xA0,
    TECMO_WIN32_VK_RSHIFT = 0xA1
} TecmoWin32VirtualKey;

typedef struct TecmoWin32KeyBinding {
    unsigned player_index;
    TecmoControlButton button;
} TecmoWin32KeyBinding;

#define TECMO_WIN32_TRACKED_KEY_COUNT 256U

typedef struct TecmoWin32KeyboardState {
    bool physical_down[TECMO_WIN32_TRACKED_KEY_COUNT];
} TecmoWin32KeyboardState;

bool tecmo_win32_translate_key(uint32_t virtual_key,
                               TecmoWin32KeyBinding *binding_out);
void tecmo_win32_keyboard_init(TecmoWin32KeyboardState *state);
bool tecmo_win32_keyboard_update(TecmoWin32KeyboardState *state,
                                 uint32_t virtual_key,
                                 bool physical_down,
                                 TecmoWin32KeyBinding *binding_out,
                                 bool *logical_down_out);
bool tecmo_win32_keys_self_test(char *message, size_t message_size);

#endif
