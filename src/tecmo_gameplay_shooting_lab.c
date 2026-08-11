#include "tecmo_gameplay_shooting_lab.h"

#include "tecmo_gameplay_scene_internal.h"

#include <stdio.h>
#include <string.h>

void tecmo_gameplay_shooting_lab_init(TecmoGameplayShootingLab *lab)
{
    if (lab != NULL) memset(lab, 0, sizeof(*lab));
}

bool tecmo_gameplay_shooting_lab_open(TecmoGameplayShootingLab *lab,
                                      const TecmoGameplayScene *scene)
{
    if (lab == NULL || scene == NULL || lab->active || !scene->available ||
        !scene->jump_shots.available || !scene->assets.available) {
        return false;
    }
    tecmo_gameplay_shooting_lab_init(lab);
    lab->active = true;
    lab->paused = true;
    /* This numeric slot is the bounded captured route, not a semantic name. */
    lab->selection = 1U;
    return true;
}

void tecmo_gameplay_shooting_lab_close(TecmoGameplayShootingLab *lab)
{
    tecmo_gameplay_shooting_lab_init(lab);
}

bool tecmo_gameplay_shooting_lab_selection(
    const TecmoGameplayShootingLab *lab,
    TecmoGameplayJumpShotFamily *family_out,
    TecmoGameplayJumpShotProfile *profile_out,
    TecmoGameplayJumpShotDirection *direction_out)
{
    uint8_t selection;
    if (lab == NULL || family_out == NULL || profile_out == NULL ||
        direction_out == NULL ||
        lab->selection >= TECMO_GAMEPLAY_JUMP_SHOT_POSE_COUNT) {
        return false;
    }
    selection = lab->selection;
    *family_out = (TecmoGameplayJumpShotFamily)(
        selection / (TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_COUNT *
                     TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_COUNT));
    selection = (uint8_t)(selection %
        (TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_COUNT *
         TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_COUNT));
    *profile_out = (TecmoGameplayJumpShotProfile)(
        selection / TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_COUNT);
    *direction_out = (TecmoGameplayJumpShotDirection)(
        selection % TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_COUNT);
    return true;
}

bool tecmo_gameplay_shooting_lab_pose_pointer(
    const TecmoGameplayShootingLab *lab,
    const TecmoGameplayScene *scene,
    uint16_t *base_pointer_out,
    uint16_t *phase_pointer_out)
{
    TecmoGameplayJumpShotFamily family;
    TecmoGameplayJumpShotProfile profile;
    TecmoGameplayJumpShotDirection direction;
    uint16_t base;
    uint16_t phase;
    if (scene == NULL || base_pointer_out == NULL || phase_pointer_out == NULL ||
        lab == NULL || !lab->active || !scene->jump_shots.available ||
        lab->phase >= TECMO_GAMEPLAY_SHOOTING_LAB_PHASE_COUNT ||
        !tecmo_gameplay_shooting_lab_selection(lab, &family, &profile,
                                               &direction) ||
        !tecmo_gameplay_jump_shots_resolve_pose_pointer_index(
            &scene->jump_shots, family, profile, direction, &base) ||
        !tecmo_gameplay_jump_shots_resolve_phase_pose_pointer_index(
            &scene->jump_shots, family, profile, direction, lab->phase,
            &phase)) {
        return false;
    }
    *base_pointer_out = base;
    *phase_pointer_out = phase;
    return true;
}

bool tecmo_gameplay_shooting_lab_update(TecmoGameplayShootingLab *lab,
                                        const TecmoControlFrame *controls)
{
    if (lab == NULL || controls == NULL || !lab->active) return false;
    if (controls->pressed.violation_lab_previous) {
        lab->selection = lab->selection == 0U
            ? (uint8_t)(TECMO_GAMEPLAY_JUMP_SHOT_POSE_COUNT - 1U)
            : (uint8_t)(lab->selection - 1U);
        lab->phase = 0U;
        lab->phase_hold_frame = 0U;
    }
    if (controls->pressed.violation_lab_next) {
        lab->selection = (uint8_t)(
            (lab->selection + 1U) % TECMO_GAMEPLAY_JUMP_SHOT_POSE_COUNT);
        lab->phase = 0U;
        lab->phase_hold_frame = 0U;
    }
    if (controls->pressed.violation_lab_path) {
        lab->mirror_inspection = !lab->mirror_inspection;
    }
    if (controls->pressed.violation_lab_play_pause) {
        lab->paused = !lab->paused;
    }
    if (controls->pressed.violation_lab_restart) {
        lab->phase = 0U;
        lab->phase_hold_frame = 0U;
        lab->paused = true;
    }
    if (controls->pressed.violation_lab_step) {
        lab->phase = (uint8_t)(
            (lab->phase + 1U) % TECMO_GAMEPLAY_SHOOTING_LAB_PHASE_COUNT);
        lab->phase_hold_frame = 0U;
        lab->paused = true;
    } else if (!lab->paused) {
        ++lab->phase_hold_frame;
        if (lab->phase_hold_frame >=
            TECMO_GAMEPLAY_SHOOTING_LAB_PHASE_HOLD_FRAMES) {
            lab->phase_hold_frame = 0U;
            lab->phase = (uint8_t)(
                (lab->phase + 1U) %
                TECMO_GAMEPLAY_SHOOTING_LAB_PHASE_COUNT);
        }
    }
    return true;
}

bool tecmo_gameplay_shooting_lab_draw(
    const TecmoGameplayShootingLab *lab,
    const TecmoGameplayScene *scene,
    TecmoFramebuffer *framebuffer,
    int origin_x,
    int origin_y,
    int scale)
{
    uint16_t base;
    uint16_t pose;
    if (lab == NULL || scene == NULL || framebuffer == NULL || scale <= 0 ||
        !tecmo_gameplay_shooting_lab_pose_pointer(lab, scene, &base, &pose)) {
        return false;
    }
    (void)base;
    return tecmo_gameplay_scene_render_draw_source_pose(
        scene, pose, framebuffer, 64, 69, origin_x, origin_y, scale,
        lab->mirror_inspection);
}

static void shooting_lab_test_press(TecmoControlFrame *controls,
                                    TecmoControlButton button)
{
    memset(controls, 0, sizeof(*controls));
    tecmo_input_set_button(&controls->pressed, button, true);
}

bool tecmo_gameplay_shooting_lab_self_test(char *message,
                                           size_t message_size)
{
    TecmoGameplayScene scene;
    TecmoGameplayShootingLab lab;
    TecmoControlFrame controls;
    TecmoGameplayJumpShotFamily family;
    TecmoGameplayJumpShotProfile profile;
    TecmoGameplayJumpShotDirection direction;
    unsigned tick;

    tecmo_gameplay_scene_init(&scene);
    scene.available = true;
    scene.assets.available = true;
    scene.jump_shots.available = true;
    tecmo_gameplay_shooting_lab_init(&lab);
    if (!tecmo_gameplay_shooting_lab_open(&lab, &scene) || !lab.active ||
        !lab.paused || lab.selection != 1U ||
        !tecmo_gameplay_shooting_lab_selection(
            &lab, &family, &profile, &direction) ||
        family != TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_0 ||
        profile != TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_0 ||
        direction != TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_1) {
        (void)snprintf(message, message_size,
                       "SHOOTING LAB OPEN/MAP FAILED");
        return false;
    }
    shooting_lab_test_press(&controls, TECMO_CONTROL_VIOLATION_LAB_PREVIOUS);
    if (!tecmo_gameplay_shooting_lab_update(&lab, &controls) ||
        lab.selection != 0U) {
        (void)snprintf(message, message_size, "SHOOTING LAB PREVIOUS FAILED");
        return false;
    }
    shooting_lab_test_press(&controls, TECMO_CONTROL_VIOLATION_LAB_PREVIOUS);
    if (!tecmo_gameplay_shooting_lab_update(&lab, &controls) ||
        lab.selection != TECMO_GAMEPLAY_JUMP_SHOT_POSE_COUNT - 1U ||
        !tecmo_gameplay_shooting_lab_selection(
            &lab, &family, &profile, &direction) ||
        family != TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_1 ||
        profile != TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_1 ||
        direction != TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_7) {
        (void)snprintf(message, message_size, "SHOOTING LAB WRAP FAILED");
        return false;
    }
    shooting_lab_test_press(&controls, TECMO_CONTROL_VIOLATION_LAB_PATH);
    if (!tecmo_gameplay_shooting_lab_update(&lab, &controls) ||
        !lab.mirror_inspection) {
        (void)snprintf(message, message_size, "SHOOTING LAB MIRROR FAILED");
        return false;
    }
    shooting_lab_test_press(&controls, TECMO_CONTROL_VIOLATION_LAB_STEP);
    if (!tecmo_gameplay_shooting_lab_update(&lab, &controls) ||
        lab.phase != 1U || !lab.paused) {
        (void)snprintf(message, message_size, "SHOOTING LAB STEP FAILED");
        return false;
    }
    shooting_lab_test_press(&controls,
                            TECMO_CONTROL_VIOLATION_LAB_PLAY_PAUSE);
    if (!tecmo_gameplay_shooting_lab_update(&lab, &controls) || lab.paused) {
        (void)snprintf(message, message_size, "SHOOTING LAB PLAY FAILED");
        return false;
    }
    memset(&controls, 0, sizeof(controls));
    for (tick = 0U;
         tick < TECMO_GAMEPLAY_SHOOTING_LAB_PHASE_HOLD_FRAMES; ++tick) {
        if (!tecmo_gameplay_shooting_lab_update(&lab, &controls)) return false;
    }
    if (lab.phase != 2U) {
        (void)snprintf(message, message_size, "SHOOTING LAB CADENCE FAILED");
        return false;
    }
    tecmo_gameplay_shooting_lab_close(&lab);
    if (lab.active) {
        (void)snprintf(message, message_size, "SHOOTING LAB CLOSE FAILED");
        return false;
    }
    (void)snprintf(message, message_size,
                   "SHOOTING LAB SELF TEST PASS");
    return true;
}
