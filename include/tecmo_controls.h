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
    /* F4-F10 are developer-only violation-lab controls. The runtime ignores
       them unless F3's debug overlay is enabled; source preview works from
       menus, while production-state preview still requires an idle court. */
    TECMO_CONTROL_VIOLATION_LAB_TOGGLE,
    TECMO_CONTROL_VIOLATION_LAB_PREVIOUS,
    TECMO_CONTROL_VIOLATION_LAB_NEXT,
    TECMO_CONTROL_VIOLATION_LAB_PATH,
    TECMO_CONTROL_VIOLATION_LAB_PLAY_PAUSE,
    TECMO_CONTROL_VIOLATION_LAB_RESTART,
    TECMO_CONTROL_VIOLATION_LAB_STEP,
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
    bool violation_lab_toggle;
    bool violation_lab_previous;
    bool violation_lab_next;
    bool violation_lab_path;
    bool violation_lab_play_pause;
    bool violation_lab_restart;
    bool violation_lab_step;
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

/* F3 and F4-F10 are consumed exclusively by developer presentation tools.
 * Keep their raw edges available to those tools, but strip them before a
 * normal gameplay or menu decision receives a control frame. */
bool tecmo_control_button_is_developer_only(TecmoControlButton button);
void tecmo_control_frame_strip_developer_only(TecmoControlFrame *frame);
bool tecmo_control_frame_normal_input_is_neutral(
    const TecmoControlFrame *frame);

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
