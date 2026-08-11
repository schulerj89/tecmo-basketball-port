#ifndef TECMO_GAMEPLAY_VIOLATION_LAB_H
#define TECMO_GAMEPLAY_VIOLATION_LAB_H

#include "tecmo_gameplay_scene.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Developer-only presentation lab state.  It snapshots only the scene state
 * which it temporarily replaces for the explicitly-labelled production-state
 * preview; ordinary gameplay never owns or consults this record.
 */
typedef enum TecmoGameplayViolationLabItem {
    TECMO_GAMEPLAY_VIOLATION_LAB_OUT_OF_BOUNDS = 0,
    TECMO_GAMEPLAY_VIOLATION_LAB_BACKCOURT,
    TECMO_GAMEPLAY_VIOLATION_LAB_FIVE_SECONDS,
    TECMO_GAMEPLAY_VIOLATION_LAB_TEN_SECONDS,
    TECMO_GAMEPLAY_VIOLATION_LAB_SHOT_CLOCK,
    TECMO_GAMEPLAY_VIOLATION_LAB_TRAVELING,
    TECMO_GAMEPLAY_VIOLATION_LAB_GOALTENDING,
    /* The fixed $E95E -> selector $22 referee route is deliberately
       separate from the seven strict TGVR violation messages. */
    TECMO_GAMEPLAY_VIOLATION_LAB_FIXED_FOUL,
    TECMO_GAMEPLAY_VIOLATION_LAB_ITEM_COUNT
} TecmoGameplayViolationLabItem;

typedef enum TecmoGameplayViolationLabPath {
    /* Direct TGVR source-presentation draw. */
    TECMO_GAMEPLAY_VIOLATION_LAB_SOURCE_PREVIEW = 0,
    /* Temporary public gameplay-state request, then production scene draw.
       This is a renderer/state preview only, never a detector claim. */
    TECMO_GAMEPLAY_VIOLATION_LAB_PRODUCTION_STATE_PREVIEW,
    TECMO_GAMEPLAY_VIOLATION_LAB_PATH_COUNT
} TecmoGameplayViolationLabPath;

typedef struct TecmoGameplayViolationLab {
    bool active;
    bool paused;
    bool snapshot_valid;
    bool state_path_available;
    bool state_path_rejected;
    uint8_t selection;
    TecmoGameplayViolationLabPath path;
    uint16_t phase_frame;
    TecmoGameplayState saved_state;
    TecmoGameplaySceneFoulPresentation saved_foul_presentation;
} TecmoGameplayViolationLab;

void tecmo_gameplay_violation_lab_init(TecmoGameplayViolationLab *lab);

/* Opens a paused source preview whenever strict TGVR assets are loaded. If an
 * active gameplay scene exists, `close` restores its captured state byte-for-
 * byte; menu/title use is source-only and does not manufacture scene state. */
bool tecmo_gameplay_violation_lab_open(TecmoGameplayViolationLab *lab,
                                       TecmoGameplayScene *scene);
void tecmo_gameplay_violation_lab_close(TecmoGameplayViolationLab *lab,
                                        TecmoGameplayScene *scene);

/* Consumes only developer lab controls while active.  The runtime is
 * responsible for calling this only with its F3 developer overlay enabled
 * and for freezing the ordinary scene update for that frame. */
bool tecmo_gameplay_violation_lab_update(TecmoGameplayViolationLab *lab,
                                         TecmoGameplayScene *scene,
                                         const TecmoControlFrame *controls);

bool tecmo_gameplay_violation_lab_set_item(
    TecmoGameplayViolationLab *lab,
    TecmoGameplayScene *scene,
    TecmoGameplayViolationLabItem item);
bool tecmo_gameplay_violation_lab_set_path(
    TecmoGameplayViolationLab *lab,
    TecmoGameplayScene *scene,
    TecmoGameplayViolationLabPath path);
bool tecmo_gameplay_violation_lab_set_frame(
    TecmoGameplayViolationLab *lab,
    TecmoGameplayScene *scene,
    uint16_t phase_frame);

/* Source mode calls TGVR directly.  Production-state mode calls the normal
 * scene compositor after installing its temporary state snapshot. */
bool tecmo_gameplay_violation_lab_draw(
    const TecmoGameplayViolationLab *lab,
    const TecmoGameplayScene *scene,
    TecmoFramebuffer *framebuffer,
    int origin_x,
    int origin_y,
    int scale);

const char *tecmo_gameplay_violation_lab_item_label(
    TecmoGameplayViolationLabItem item);
const char *tecmo_gameplay_violation_lab_item_token(
    TecmoGameplayViolationLabItem item);
const char *tecmo_gameplay_violation_lab_path_label(
    TecmoGameplayViolationLabPath path);
bool tecmo_gameplay_violation_lab_item_violation(
    TecmoGameplayViolationLabItem item,
    TecmoGameplayViolation *violation_out);
bool tecmo_gameplay_violation_lab_item_is_foul(
    TecmoGameplayViolationLabItem item);
bool tecmo_gameplay_violation_lab_item_state_supported(
    TecmoGameplayViolationLabItem item);
uint8_t tecmo_gameplay_violation_lab_item_sequence_id(
    TecmoGameplayViolationLabItem item);
uint16_t tecmo_gameplay_violation_lab_frame_count(
    TecmoGameplayViolationLabItem item);
bool tecmo_gameplay_violation_lab_group_id(
    const TecmoGameplayViolationLab *lab,
    const TecmoGameplayScene *scene,
    uint8_t *group_id_out);

/* Focused input/state transaction coverage.  The TGVR asset module owns the
 * parser/metasprite render tests; this test verifies lab selection, supported
 * state previews, frame stepping, and exact restoration without a ROM. */
bool tecmo_gameplay_violation_lab_self_test(char *message,
                                            size_t message_size);

#endif
