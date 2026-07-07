#include "tecmo_controls.h"

#include <stdio.h>
#include <string.h>

static bool *input_button_slot(TecmoInput *input, TecmoControlButton button)
{
    if (input == 0) {
        return 0;
    }

    switch (button) {
    case TECMO_CONTROL_UP: return &input->up;
    case TECMO_CONTROL_DOWN: return &input->down;
    case TECMO_CONTROL_LEFT: return &input->left;
    case TECMO_CONTROL_RIGHT: return &input->right;
    case TECMO_CONTROL_CONFIRM: return &input->confirm;
    case TECMO_CONTROL_CANCEL: return &input->cancel;
    case TECMO_CONTROL_SHOOT: return &input->shoot;
    case TECMO_CONTROL_TAB: return &input->tab;
    case TECMO_CONTROL_BANK_PREV: return &input->bank_prev;
    case TECMO_CONTROL_BANK_NEXT: return &input->bank_next;
    case TECMO_CONTROL_TABLE_TOGGLE: return &input->table_toggle;
    case TECMO_CONTROL_SAVE: return &input->save;
    case TECMO_CONTROL_PRESET_RABBIT: return &input->preset_rabbit;
    case TECMO_CONTROL_PRESET_TECMO: return &input->preset_tecmo;
    case TECMO_CONTROL_PRESET_COMPOSITE: return &input->preset_composite;
    case TECMO_CONTROL_REMOVE: return &input->remove;
    case TECMO_CONTROL_DEBUG_TOGGLE: return &input->debug_toggle;
    case TECMO_CONTROL_COUNT:
    default:
        return 0;
    }
}

static void set_self_test_message(char *dest, size_t dest_size, const char *text)
{
    if (dest == 0 || dest_size == 0) {
        return;
    }
    if (text == 0) {
        text = "";
    }
    (void)snprintf(dest, dest_size, "%s", text);
}

void tecmo_input_clear(TecmoInput *input)
{
    if (input != 0) {
        memset(input, 0, sizeof(*input));
    }
}

void tecmo_input_set_button(TecmoInput *input, TecmoControlButton button, bool down)
{
    bool *slot = input_button_slot(input, button);
    if (slot != 0) {
        *slot = down;
    }
}

bool tecmo_input_button(const TecmoInput *input, TecmoControlButton button)
{
    TecmoInput copy;
    bool *slot;

    if (input == 0) {
        return false;
    }
    copy = *input;
    slot = input_button_slot(&copy, button);
    return slot != 0 && *slot;
}

void tecmo_control_frame_build(TecmoControlFrame *frame,
                               const TecmoInput *held,
                               const TecmoInput *previous)
{
    TecmoInput empty;

    if (frame == 0) {
        return;
    }

    tecmo_input_clear(&empty);
    frame->held = held != 0 ? *held : empty;
    tecmo_input_clear(&frame->pressed);
    tecmo_input_clear(&frame->released);

    for (int button = 0; button < (int)TECMO_CONTROL_COUNT; ++button) {
        TecmoControlButton control = (TecmoControlButton)button;
        bool now = tecmo_input_button(&frame->held, control);
        bool before = previous != 0 && tecmo_input_button(previous, control);
        tecmo_input_set_button(&frame->pressed, control, now && !before);
        tecmo_input_set_button(&frame->released, control, !now && before);
    }
}

void tecmo_controls_init(TecmoControls *controls)
{
    if (controls != 0) {
        memset(controls, 0, sizeof(*controls));
    }
}

void tecmo_controls_set_button(TecmoControls *controls, TecmoControlButton button, bool down)
{
    if (controls != 0) {
        tecmo_input_set_button(&controls->current, button, down);
    }
}

void tecmo_controls_begin_frame(TecmoControls *controls)
{
    if (controls == 0) {
        return;
    }

    tecmo_control_frame_build(&controls->frame, &controls->current, &controls->previous);
    controls->previous = controls->current;
}

const TecmoControlFrame *tecmo_controls_frame(const TecmoControls *controls)
{
    return controls != 0 ? &controls->frame : 0;
}

const TecmoInput *tecmo_controls_held(const TecmoControls *controls)
{
    return controls != 0 ? &controls->current : 0;
}

bool tecmo_controls_down(const TecmoControls *controls, TecmoControlButton button)
{
    return controls != 0 && tecmo_input_button(&controls->frame.held, button);
}

bool tecmo_controls_pressed(const TecmoControls *controls, TecmoControlButton button)
{
    return controls != 0 && tecmo_input_button(&controls->frame.pressed, button);
}

bool tecmo_controls_released(const TecmoControls *controls, TecmoControlButton button)
{
    return controls != 0 && tecmo_input_button(&controls->frame.released, button);
}

const char *tecmo_control_button_name(TecmoControlButton button)
{
    switch (button) {
    case TECMO_CONTROL_UP: return "up";
    case TECMO_CONTROL_DOWN: return "down";
    case TECMO_CONTROL_LEFT: return "left";
    case TECMO_CONTROL_RIGHT: return "right";
    case TECMO_CONTROL_CONFIRM: return "confirm";
    case TECMO_CONTROL_CANCEL: return "cancel";
    case TECMO_CONTROL_SHOOT: return "shoot";
    case TECMO_CONTROL_TAB: return "tab";
    case TECMO_CONTROL_BANK_PREV: return "bank-prev";
    case TECMO_CONTROL_BANK_NEXT: return "bank-next";
    case TECMO_CONTROL_TABLE_TOGGLE: return "table-toggle";
    case TECMO_CONTROL_SAVE: return "save";
    case TECMO_CONTROL_PRESET_RABBIT: return "preset-rabbit";
    case TECMO_CONTROL_PRESET_TECMO: return "preset-tecmo";
    case TECMO_CONTROL_PRESET_COMPOSITE: return "preset-composite";
    case TECMO_CONTROL_REMOVE: return "remove";
    case TECMO_CONTROL_DEBUG_TOGGLE: return "debug-toggle";
    case TECMO_CONTROL_COUNT:
    default:
        return "unknown";
    }
}

bool tecmo_controls_self_test(char *message, size_t message_size)
{
    TecmoControls controls;
    TecmoControlFrame frame;
    TecmoInput held;
    TecmoInput previous;

    tecmo_controls_init(&controls);
    tecmo_controls_set_button(&controls, TECMO_CONTROL_CONFIRM, true);
    tecmo_controls_begin_frame(&controls);
    if (!tecmo_controls_down(&controls, TECMO_CONTROL_CONFIRM) ||
        !tecmo_controls_pressed(&controls, TECMO_CONTROL_CONFIRM) ||
        tecmo_controls_released(&controls, TECMO_CONTROL_CONFIRM)) {
        set_self_test_message(message, message_size, "FIRST PRESS EDGE FAILED");
        return false;
    }

    tecmo_controls_begin_frame(&controls);
    if (!tecmo_controls_down(&controls, TECMO_CONTROL_CONFIRM) ||
        tecmo_controls_pressed(&controls, TECMO_CONTROL_CONFIRM) ||
        tecmo_controls_released(&controls, TECMO_CONTROL_CONFIRM)) {
        set_self_test_message(message, message_size, "HELD BUTTON EDGE FAILED");
        return false;
    }

    tecmo_controls_set_button(&controls, TECMO_CONTROL_CONFIRM, false);
    tecmo_controls_begin_frame(&controls);
    if (tecmo_controls_down(&controls, TECMO_CONTROL_CONFIRM) ||
        tecmo_controls_pressed(&controls, TECMO_CONTROL_CONFIRM) ||
        !tecmo_controls_released(&controls, TECMO_CONTROL_CONFIRM)) {
        set_self_test_message(message, message_size, "RELEASE EDGE FAILED");
        return false;
    }

    tecmo_input_clear(&held);
    tecmo_input_clear(&previous);
    held.up = true;
    held.shoot = true;
    previous.up = true;
    previous.cancel = true;
    tecmo_control_frame_build(&frame, &held, &previous);
    if (!frame.held.up ||
        !frame.held.shoot ||
        frame.pressed.up ||
        !frame.pressed.shoot ||
        !frame.released.cancel ||
        frame.released.shoot) {
        set_self_test_message(message, message_size, "FRAME BUILD CONTRACT FAILED");
        return false;
    }

    if (strcmp(tecmo_control_button_name(TECMO_CONTROL_SHOOT), "shoot") != 0) {
        set_self_test_message(message, message_size, "BUTTON NAME CONTRACT FAILED");
        return false;
    }

    set_self_test_message(message, message_size, "CONTROLS SELF TEST PASS");
    return true;
}
