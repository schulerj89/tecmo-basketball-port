#ifndef TECMO_GAMEPLAY_BALL_DRIBBLE_H
#define TECMO_GAMEPLAY_BALL_DRIBBLE_H

#include "tecmo_gameplay_movement.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_BALL_DRIBBLE_SOURCE_COUNT 2U
#define TECMO_GAMEPLAY_BALL_DRIBBLE_DIRECTION_COUNT 8U
#define TECMO_GAMEPLAY_BALL_DRIBBLE_PHASE_COUNT 8U
#define TECMO_GAMEPLAY_BALL_DRIBBLE_HALF_COUNT 2U
#define TECMO_GAMEPLAY_BALL_DRIBBLE_FRAME_TAG 0x44424754U

typedef enum TecmoGameplayBallDribbleSourceKind {
    TECMO_GAMEPLAY_BALL_DRIBBLE_SOURCE_ROUTINE = 1,
    TECMO_GAMEPLAY_BALL_DRIBBLE_SOURCE_TABLES = 2
} TecmoGameplayBallDribbleSourceKind;

typedef struct TecmoGameplayBallDribbleSourceSpan {
    TecmoGameplayBallDribbleSourceKind kind;
    uint8_t bank;
    bool fixed_bank;
    uint16_t cpu_start;
    uint16_t cpu_end;
    uint32_t byte_count;
    uint32_t fingerprint;
    const uint8_t *bytes;
} TecmoGameplayBallDribbleSourceSpan;

typedef struct TecmoGameplayBallDribbleAssets {
    uint32_t lifecycle_tag;
    bool available;
    char status[176];
    uint8_t *storage;
    size_t storage_size;
    TecmoGameplayBallDribbleSourceSpan
        sources[TECMO_GAMEPLAY_BALL_DRIBBLE_SOURCE_COUNT];
    const uint8_t *direction_half;
    const uint8_t *bounce_height;
    const uint8_t *y_offset;
    const uint8_t *x_offset_low;
    const uint8_t *x_offset_high;
    uint8_t sound_phase;
    uint8_t sound_high_nibble;
    uint32_t gameplay_core_fingerprint;
    uint32_t movement_fingerprint;
} TecmoGameplayBallDribbleAssets;

typedef struct TecmoGameplayBallDribbleFrame {
    uint32_t contract_tag;
    TecmoGameplayCourtCoordinate base_position;
    TecmoGameplayCourtCoordinate visible_position;
    int16_t x_offset;
    int8_t y_offset;
    uint8_t bounce_height;
    uint8_t table_half;
    uint8_t direction;
    uint8_t animation_phase;
    bool sound_trigger;
} TecmoGameplayBallDribbleFrame;

void tecmo_gameplay_ball_dribble_assets_init(
    TecmoGameplayBallDribbleAssets *assets);
void tecmo_gameplay_ball_dribble_assets_destroy(
    TecmoGameplayBallDribbleAssets *assets);

bool tecmo_gameplay_ball_dribble_assets_parse(
    TecmoGameplayBallDribbleAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *gameplay_core,
    size_t gameplay_core_size,
    const uint8_t *movement,
    size_t movement_size);
bool tecmo_gameplay_ball_dribble_assets_load(
    TecmoGameplayBallDribbleAssets *assets,
    const char *asset_pack_path);

const TecmoGameplayBallDribbleSourceSpan *
tecmo_gameplay_ball_dribble_find_source(
    const TecmoGameplayBallDribbleAssets *assets,
    TecmoGameplayBallDribbleSourceKind kind);

/* Exact ordinary held-ball geometry and phase-$03 DMC trigger from Bank05
   $B52E-$B677. The scene flattens the returned altitude into visible Y at its
   existing canonical-coordinate boundary. Failed validation is transactional. */
bool tecmo_gameplay_ball_dribble_resolve(
    const TecmoGameplayBallDribbleAssets *assets,
    const TecmoGameplayMovementAssets *movement_assets,
    const TecmoGameplayMovementState *holder,
    const TecmoGameplayCourtCoordinate *linked_position,
    TecmoGameplayBallDribbleFrame *frame_out);

bool tecmo_gameplay_ball_dribble_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size);

#endif
