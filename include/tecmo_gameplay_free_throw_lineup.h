#ifndef TECMO_GAMEPLAY_FREE_THROW_LINEUP_H
#define TECMO_GAMEPLAY_FREE_THROW_LINEUP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_COUNT 4U
#define TECMO_GAMEPLAY_FREE_THROW_LINEUP_ACTOR_COUNT 10U
#define TECMO_GAMEPLAY_FREE_THROW_LINEUP_ORIENTATION_COUNT 2U
#define TECMO_GAMEPLAY_FREE_THROW_LINEUP_UNDEFINED_INDEX 0xFFU

typedef enum TecmoGameplayFreeThrowLineupSourceKind {
    TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_POSE_LOOKUP = 1,
    TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_ROUND_SETUP = 2,
    TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_FOLLOWUP = 3,
    TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_TABLES = 4
} TecmoGameplayFreeThrowLineupSourceKind;

typedef struct TecmoGameplayFreeThrowLineupSourceSpan {
    TecmoGameplayFreeThrowLineupSourceKind kind;
    uint8_t bank;
    bool fixed_bank;
    uint16_t cpu_start;
    uint16_t cpu_end;
    uint32_t byte_count;
    uint32_t fingerprint;
    const uint8_t *bytes;
} TecmoGameplayFreeThrowLineupSourceSpan;

/*
 * Coordinates are the original raw world values. They are deliberately not
 * clamped or projected into the native camera. For non-shooters, raw pose
 * offsets are the even byte offsets written by Bank06 $88B0; pose_index is the
 * native TGPL pointer-table index raw_pose_offset / 2. The follow-up routine
 * does not call $88B0 for the shooter, so that actor's pose fields are
 * explicitly undefined rather than synthesized.
 */
typedef struct TecmoGameplayFreeThrowLineupActor {
    uint16_t raw_world_x;
    uint8_t raw_world_y;
    uint8_t direction_index;
    uint16_t raw_pose_offset;
    uint16_t pose_index;
    uint8_t position_pair_index;
    uint8_t pose_stream_index;
    uint8_t raw_action_phase;       /* $046E */
    uint8_t raw_actor_state;        /* $057C */
    uint8_t raw_palette_attributes; /* $0458 */
    uint8_t raw_pose_flags;         /* $0479 */
    uint8_t raw_aux_state;          /* $048F */
    bool position_defined;
    bool pose_defined;
    bool shooter;
    bool secondary;
} TecmoGameplayFreeThrowLineupActor;

typedef struct TecmoGameplayFreeThrowLineup {
    uint8_t orientation;
    uint8_t shooter_slot;
    uint8_t secondary_slot;
    TecmoGameplayFreeThrowLineupActor
        actors[TECMO_GAMEPLAY_FREE_THROW_LINEUP_ACTOR_COUNT];
} TecmoGameplayFreeThrowLineup;

typedef struct TecmoGameplayFreeThrowLineupAssets {
    uint32_t lifecycle_tag;
    bool available;
    char status[160];
    uint8_t *storage;
    size_t storage_size;
    TecmoGameplayFreeThrowLineupSourceSpan
        sources[TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_COUNT];
    const uint8_t *pose_lookup;
    const uint8_t *round_setup;
    const uint8_t *followup;
    const uint8_t *tables;
    uint32_t gameplay_core_fingerprint;
} TecmoGameplayFreeThrowLineupAssets;

void tecmo_gameplay_free_throw_lineup_init(
    TecmoGameplayFreeThrowLineupAssets *assets);
void tecmo_gameplay_free_throw_lineup_destroy(
    TecmoGameplayFreeThrowLineupAssets *assets);

bool tecmo_gameplay_free_throw_lineup_parse(
    TecmoGameplayFreeThrowLineupAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *gameplay_core,
    size_t gameplay_core_size);

bool tecmo_gameplay_free_throw_lineup_load(
    TecmoGameplayFreeThrowLineupAssets *assets,
    const char *asset_pack_path);

const TecmoGameplayFreeThrowLineupSourceSpan *
tecmo_gameplay_free_throw_lineup_find_source(
    const TecmoGameplayFreeThrowLineupAssets *assets,
    TecmoGameplayFreeThrowLineupSourceKind kind);

/*
 * The pure base resolver intentionally does not accept the original
 * side-control bytes. Consequently it does not apply the conditional shooter
 * script override or secondary-slot raw phase $15 override that follow the
 * base lineup seed.
 */
bool tecmo_gameplay_free_throw_lineup_derive(
    const TecmoGameplayFreeThrowLineupAssets *assets,
    uint8_t orientation,
    uint8_t shooter_slot,
    uint8_t secondary_slot,
    TecmoGameplayFreeThrowLineup *lineup);

bool tecmo_gameplay_free_throw_lineup_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size);

#endif
