#ifndef TECMO_GAMEPLAY_CAMERA_H
#define TECMO_GAMEPLAY_CAMERA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_CAMERA_SOURCE_COUNT 6U
#define TECMO_GAMEPLAY_CAMERA_ORIENTATION_COUNT 2U

typedef enum TecmoGameplayCameraSourceKind {
    TECMO_GAMEPLAY_CAMERA_SOURCE_INITIALIZE = 1,
    TECMO_GAMEPLAY_CAMERA_SOURCE_STREAM_COLUMNS = 2,
    TECMO_GAMEPLAY_CAMERA_SOURCE_ATTRIBUTE_QUADRANTS = 3,
    TECMO_GAMEPLAY_CAMERA_SOURCE_FOLLOW = 4,
    TECMO_GAMEPLAY_CAMERA_SOURCE_FORCED_SETTLE = 5,
    TECMO_GAMEPLAY_CAMERA_SOURCE_ACTOR_PROJECTION = 6
} TecmoGameplayCameraSourceKind;

typedef struct TecmoGameplayCameraSourceSpan {
    TecmoGameplayCameraSourceKind kind;
    uint8_t bank;
    bool fixed_bank;
    uint16_t cpu_start;
    uint16_t cpu_end;
    uint32_t byte_count;
    uint32_t fingerprint;
    const uint8_t *bytes;
} TecmoGameplayCameraSourceSpan;

typedef struct TecmoGameplayCameraAssets {
    uint32_t lifecycle_tag;
    bool available;
    char status[160];
    uint8_t *storage;
    size_t storage_size;
    TecmoGameplayCameraSourceSpan
        sources[TECMO_GAMEPLAY_CAMERA_SOURCE_COUNT];
    const uint8_t *initialize_routine;
    const uint8_t *stream_columns;
    const uint8_t *attribute_quadrants;
    const uint8_t *follow_routine;
    const uint8_t *forced_settle_routine;
    const uint8_t *actor_projection_routine;
    uint32_t gameplay_core_fingerprint;
    uint32_t gameplay_court_fingerprint;
} TecmoGameplayCameraAssets;

typedef struct TecmoGameplayCameraState {
    uint16_t camera_x;
    uint8_t scroll_x;
    uint8_t scroll_aux;
    uint8_t nametable_page;
    uint8_t aux;
    uint8_t stream_direction;
    uint8_t layout_cursor;
    uint8_t left_threshold;
    uint8_t right_threshold;
    bool thresholds_valid;
    bool endpoint_latched;
} TecmoGameplayCameraState;

typedef struct TecmoGameplayCameraFollowInput {
    uint16_t focus_world_x;
    uint8_t orientation;
    uint8_t action_route;
    bool camera_disabled;
} TecmoGameplayCameraFollowInput;

typedef struct TecmoGameplayActorProjection {
    bool visible;
    uint8_t screen_x;
    uint8_t screen_y;
} TecmoGameplayActorProjection;

void tecmo_gameplay_camera_assets_init(
    TecmoGameplayCameraAssets *assets);
void tecmo_gameplay_camera_assets_destroy(
    TecmoGameplayCameraAssets *assets);

bool tecmo_gameplay_camera_assets_parse(
    TecmoGameplayCameraAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *gameplay_core,
    size_t gameplay_core_size,
    const uint8_t *gameplay_court,
    size_t gameplay_court_size);

bool tecmo_gameplay_camera_assets_load(
    TecmoGameplayCameraAssets *assets,
    const char *asset_pack_path);

const TecmoGameplayCameraSourceSpan *
tecmo_gameplay_camera_find_source(
    const TecmoGameplayCameraAssets *assets,
    TecmoGameplayCameraSourceKind kind);

/* Reproduces fixed $DE13. On failure, state is not changed. */
bool tecmo_gameplay_camera_state_initialize(
    const TecmoGameplayCameraAssets *assets,
    TecmoGameplayCameraState *state);

/*
 * Production-only first-column priming. The pure $DE13 initializer remains
 * unchanged at cursor $20; this mirrors the bounded $DDFB->$DF05 first
 * prefetch and advances a freshly initialized live state to cursor $21.
 * On failure, state is not changed.
 */
bool tecmo_gameplay_camera_state_prime_live(
    const TecmoGameplayCameraAssets *assets,
    TecmoGameplayCameraState *state);

/*
 * Strong live-scene invariants. This is deliberately stricter than the pure
 * vector API so isolated follow tests may retain synthetic scroll/page seeds.
 */
bool tecmo_gameplay_camera_state_live_valid(
    const TecmoGameplayCameraAssets *assets,
    const TecmoGameplayCameraState *state);

/*
 * Reproduces one fixed $E16E-$E2E6 follow update, including scroll carry,
 * nametable-page toggling, coarse-boundary stream direction/cursor changes,
 * cursor bounds, and the final action-route movement gate. On failure, state
 * is not changed.
 */
bool tecmo_gameplay_camera_follow(
    const TecmoGameplayCameraAssets *assets,
    TecmoGameplayCameraState *state,
    const TecmoGameplayCameraFollowInput *input);

/*
 * Models the pure state portion of fixed $EB4F-$EB8C: temporarily use action
 * route zero and repeat follow updates until scroll_x is unchanged. PPU
 * commits are intentionally outside this API. The operation is bounded and
 * transactional.
 */
bool tecmo_gameplay_camera_settle(
    const TecmoGameplayCameraAssets *assets,
    TecmoGameplayCameraState *state,
    const TecmoGameplayCameraFollowInput *input);

/*
 * Reproduces the coordinate prefix at fixed $F1CB-$F1F1. X is visible only
 * when the high byte of world_x-camera_x is zero. Offscreen actors return the
 * deterministic API sentinel visible=false, screen_x=0, screen_y=0; the ROM
 * branches before calculating Y in that case. Visible Y subtracts altitude
 * and saturates to zero on borrow. On failure, projection is not changed.
 */
bool tecmo_gameplay_camera_project_actor(
    const TecmoGameplayCameraAssets *assets,
    const TecmoGameplayCameraState *state,
    uint16_t world_x,
    uint8_t world_y,
    uint8_t altitude,
    TecmoGameplayActorProjection *projection);

bool tecmo_gameplay_camera_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size);

#endif
