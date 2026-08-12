#ifndef TECMO_GAMEPLAY_COURT_ORIENTATION_H
#define TECMO_GAMEPLAY_COURT_ORIENTATION_H

#include "tecmo_gameplay_court.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_COURT_ORIENTATION_SOURCE_COUNT 4U
#define TECMO_GAMEPLAY_COURT_ORIENTATION_COUNT 2U
#define TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_COUNT 2U
#define TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_AWAY 0U
#define TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_HOME 1U
#define TECMO_GAMEPLAY_COURT_ORIENTATION_STATE_TAG 0x524F4753U

typedef enum TecmoGameplayCourtOrientationSourceKind {
    TECMO_GAMEPLAY_COURT_ORIENTATION_SOURCE_POSSESSION_GATE_AND_SWAP = 1,
    TECMO_GAMEPLAY_COURT_ORIENTATION_SOURCE_ACTOR_ROLE_TOGGLE = 2,
    TECMO_GAMEPLAY_COURT_ORIENTATION_SOURCE_TARGET_DELTA = 3,
    TECMO_GAMEPLAY_COURT_ORIENTATION_SOURCE_TARGET_TABLE = 4
} TecmoGameplayCourtOrientationSourceKind;

typedef struct TecmoGameplayCourtOrientationSourceSpan {
    TecmoGameplayCourtOrientationSourceKind kind;
    uint8_t bank;
    bool fixed_bank;
    uint16_t cpu_start;
    uint16_t cpu_end;
    uint32_t byte_count;
    uint32_t fingerprint;
    const uint8_t *bytes;
} TecmoGameplayCourtOrientationSourceSpan;

typedef struct TecmoGameplayCourtOrientationAssets {
    uint32_t lifecycle_tag;
    bool available;
    char status[160];
    uint8_t *storage;
    size_t storage_size;
    TecmoGameplayCourtOrientationSourceSpan
        sources[TECMO_GAMEPLAY_COURT_ORIENTATION_SOURCE_COUNT];
    TecmoGameplayCourtCoordinate
        hoops[TECMO_GAMEPLAY_COURT_ORIENTATION_COUNT];
    uint8_t actor_role_bit;
    uint8_t transition_queue_id;
    uint8_t screen_id[TECMO_GAMEPLAY_COURT_ORIENTATION_COUNT];
    uint32_t gameplay_core_fingerprint;
    uint32_t shot_resolution_fingerprint;
} TecmoGameplayCourtOrientationAssets;

typedef struct TecmoGameplayCourtOrientationState {
    uint32_t contract_tag;
    uint32_t transition_serial;
    TecmoGameplayCourtCoordinate offensive_hoop;
    /* Bank05 $8FAD-$8FE7: live $035A attack direction. $035B is only saved
       in this span and has no proven reader; previous_attack_direction is
       typed validation history, not a claimed live $035B owner. */
    uint8_t attack_direction;
    uint8_t previous_attack_direction;
    uint8_t tracked_possession_team;
    uint8_t reserved;
    uint32_t reserved_padding;
} TecmoGameplayCourtOrientationState;

void tecmo_gameplay_court_orientation_init(
    TecmoGameplayCourtOrientationAssets *assets);
void tecmo_gameplay_court_orientation_destroy(
    TecmoGameplayCourtOrientationAssets *assets);

bool tecmo_gameplay_court_orientation_parse(
    TecmoGameplayCourtOrientationAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *gameplay_core,
    size_t gameplay_core_size,
    const uint8_t *shot_resolution,
    size_t shot_resolution_size);
bool tecmo_gameplay_court_orientation_load(
    TecmoGameplayCourtOrientationAssets *assets,
    const char *asset_pack_path);

const TecmoGameplayCourtOrientationSourceSpan *
tecmo_gameplay_court_orientation_find_source(
    const TecmoGameplayCourtOrientationAssets *assets,
    TecmoGameplayCourtOrientationSourceKind kind);

/* Retained for focused scalar provenance tests. Production scene code uses
   the complete transactional hoop accessor below. */
bool tecmo_gameplay_court_orientation_target_x(
    const TecmoGameplayCourtOrientationAssets *assets,
    uint8_t direction,
    uint16_t *target_x_out);
bool tecmo_gameplay_court_orientation_hoop(
    const TecmoGameplayCourtOrientationAssets *assets,
    uint8_t direction,
    TecmoGameplayCourtCoordinate *hoop_out);
/* Resolve the offensive hoop owned by a team from the validated TGOR state.
   The opposite team owns the opposite direction; callers never infer this
   from actor slot or screen position. */
bool tecmo_gameplay_court_orientation_team_hoop(
    const TecmoGameplayCourtOrientationAssets *assets,
    const TecmoGameplayCourtOrientationState *state,
    uint8_t team,
    TecmoGameplayCourtCoordinate *hoop_out);
bool tecmo_gameplay_court_orientation_state_initialize(
    const TecmoGameplayCourtOrientationAssets *assets,
    TecmoGameplayCourtOrientationState *state_out);
bool tecmo_gameplay_court_orientation_state_valid(
    const TecmoGameplayCourtOrientationAssets *assets,
    const TecmoGameplayCourtOrientationState *state);
bool tecmo_gameplay_court_orientation_synchronize(
    const TecmoGameplayCourtOrientationAssets *assets,
    TecmoGameplayCourtOrientationState *state,
    uint8_t possession_team);

bool tecmo_gameplay_court_orientation_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size);

#endif
