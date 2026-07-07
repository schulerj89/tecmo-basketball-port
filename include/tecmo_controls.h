#ifndef TECMO_CONTROLS_H
#define TECMO_CONTROLS_H

#include <stdbool.h>
#include <stddef.h>

typedef enum TecmoControlButton {
    TECMO_CONTROL_UP,
    TECMO_CONTROL_DOWN,
    TECMO_CONTROL_LEFT,
    TECMO_CONTROL_RIGHT,
    TECMO_CONTROL_CONFIRM,
    TECMO_CONTROL_CANCEL,
    TECMO_CONTROL_SHOOT,
    TECMO_CONTROL_TAB,
    TECMO_CONTROL_BANK_PREV,
    TECMO_CONTROL_BANK_NEXT,
    TECMO_CONTROL_TABLE_TOGGLE,
    TECMO_CONTROL_SAVE,
    TECMO_CONTROL_PRESET_RABBIT,
    TECMO_CONTROL_PRESET_TECMO,
    TECMO_CONTROL_PRESET_COMPOSITE,
    TECMO_CONTROL_REMOVE,
    TECMO_CONTROL_DEBUG_TOGGLE,
    TECMO_CONTROL_COUNT
} TecmoControlButton;

typedef struct TecmoInput {
    bool up;
    bool down;
    bool left;
    bool right;
    bool confirm;
    bool cancel;
    bool shoot;
    bool tab;
    bool bank_prev;
    bool bank_next;
    bool table_toggle;
    bool save;
    bool preset_rabbit;
    bool preset_tecmo;
    bool preset_composite;
    bool remove;
    bool debug_toggle;
} TecmoInput;

typedef struct TecmoControlFrame {
    TecmoInput held;
    TecmoInput pressed;
    TecmoInput released;
} TecmoControlFrame;

typedef struct TecmoControls {
    TecmoInput current;
    TecmoInput previous;
    TecmoControlFrame frame;
} TecmoControls;

void tecmo_input_clear(TecmoInput *input);
void tecmo_input_set_button(TecmoInput *input, TecmoControlButton button, bool down);
bool tecmo_input_button(const TecmoInput *input, TecmoControlButton button);

void tecmo_control_frame_build(TecmoControlFrame *frame,
                               const TecmoInput *held,
                               const TecmoInput *previous);

void tecmo_controls_init(TecmoControls *controls);
void tecmo_controls_set_button(TecmoControls *controls, TecmoControlButton button, bool down);
void tecmo_controls_begin_frame(TecmoControls *controls);
const TecmoControlFrame *tecmo_controls_frame(const TecmoControls *controls);
const TecmoInput *tecmo_controls_held(const TecmoControls *controls);
bool tecmo_controls_down(const TecmoControls *controls, TecmoControlButton button);
bool tecmo_controls_pressed(const TecmoControls *controls, TecmoControlButton button);
bool tecmo_controls_released(const TecmoControls *controls, TecmoControlButton button);

const char *tecmo_control_button_name(TecmoControlButton button);
bool tecmo_controls_self_test(char *message, size_t message_size);

#endif
