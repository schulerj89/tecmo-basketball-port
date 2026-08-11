#ifndef TECMO_GAMEPLAY_SHOOTING_LAB_H
#define TECMO_GAMEPLAY_SHOOTING_LAB_H

#include "tecmo_gameplay_scene.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_SHOOTING_LAB_PHASE_COUNT 8U
#define TECMO_GAMEPLAY_SHOOTING_LAB_PHASE_HOLD_FRAMES 10U

/* Developer-only source-table viewer. It never mutates gameplay state. */
typedef struct TecmoGameplayShootingLab {
    bool active;
    bool paused;
    bool mirror_inspection;
    uint8_t selection;
    uint8_t phase;
    uint8_t phase_hold_frame;
} TecmoGameplayShootingLab;

void tecmo_gameplay_shooting_lab_init(TecmoGameplayShootingLab *lab);
bool tecmo_gameplay_shooting_lab_open(TecmoGameplayShootingLab *lab,
                                      const TecmoGameplayScene *scene);
void tecmo_gameplay_shooting_lab_close(TecmoGameplayShootingLab *lab);
bool tecmo_gameplay_shooting_lab_update(TecmoGameplayShootingLab *lab,
                                        const TecmoControlFrame *controls);
bool tecmo_gameplay_shooting_lab_selection(
    const TecmoGameplayShootingLab *lab,
    TecmoGameplayJumpShotFamily *family_out,
    TecmoGameplayJumpShotProfile *profile_out,
    TecmoGameplayJumpShotDirection *direction_out);
bool tecmo_gameplay_shooting_lab_pose_pointer(
    const TecmoGameplayShootingLab *lab,
    const TecmoGameplayScene *scene,
    uint16_t *base_pointer_out,
    uint16_t *phase_pointer_out);
bool tecmo_gameplay_shooting_lab_draw(
    const TecmoGameplayShootingLab *lab,
    const TecmoGameplayScene *scene,
    TecmoFramebuffer *framebuffer,
    int origin_x,
    int origin_y,
    int scale);
bool tecmo_gameplay_shooting_lab_self_test(char *message,
                                           size_t message_size);

#endif
