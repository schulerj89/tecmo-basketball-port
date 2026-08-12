#ifndef TECMO_GAMEPLAY_FREE_THROW_LINEUP_H
#define TECMO_GAMEPLAY_FREE_THROW_LINEUP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_COUNT 4U
#define TECMO_GAMEPLAY_FREE_THROW_LINEUP_ACTOR_COUNT 10U
#define TECMO_GAMEPLAY_FREE_THROW_LINEUP_ORIENTATION_COUNT 2U
#define TECMO_GAMEPLAY_FREE_THROW_LINEUP_UNDEFINED_INDEX 0xFFU
#define TECMO_GAMEPLAY_FREE_THROW_LINEUP_UNDEFINED_BYTE 0xFFU

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
    /* The caller-tail script fields are not produced by the pure base
       resolver. They are defined only when the separately named caller-policy
       API applies the exact proven override. */
    uint8_t raw_script_0547;
    uint8_t raw_script_0551;
    bool raw_script_0547_defined;
    bool raw_script_0551_defined;
} TecmoGameplayFreeThrowLineupActor;

typedef struct TecmoGameplayFreeThrowLineup {
    uint8_t orientation;
    uint8_t shooter_slot;
    uint8_t secondary_slot;
    TecmoGameplayFreeThrowLineupActor
        actors[TECMO_GAMEPLAY_FREE_THROW_LINEUP_ACTOR_COUNT];
} TecmoGameplayFreeThrowLineup;

/*
 * Bank06 $9621-$9764's base round/setup branch is also carried by TGFL-1.
 * This is deliberately separate from the later $976F follow-up resolver:
 * the base branch takes primary ($0308) and defender ($0309) as distinct
 * slots, walks its two pointer streams only for the other eight actors, and
 * then writes the two selected coordinates from $98D1/$98D5 and
 * $98D3/$98D7.  It does not infer a free-throw, inbound, or possession rule.
 */
typedef struct TecmoGameplayRoundSetupActor {
    uint16_t raw_world_x;
    uint8_t raw_world_y;
    bool position_defined;
    bool primary;
    bool defender;
} TecmoGameplayRoundSetupActor;

typedef struct TecmoGameplayRoundSetup {
    uint8_t orientation;
    uint8_t primary_slot;
    uint8_t defender_slot;
    TecmoGameplayRoundSetupActor
        actors[TECMO_GAMEPLAY_FREE_THROW_LINEUP_ACTOR_COUNT];
} TecmoGameplayRoundSetup;

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
 * caller predicate bytes. Consequently it does not apply the conditional shooter
 * script override or secondary-slot raw phase $15 override that follow the
 * base lineup seed.
 */
bool tecmo_gameplay_free_throw_lineup_derive(
    const TecmoGameplayFreeThrowLineupAssets *assets,
    uint8_t orientation,
    uint8_t shooter_slot,
    uint8_t secondary_slot,
    TecmoGameplayFreeThrowLineup *lineup);

/*
 * Explicit caller-policy composition for only the conditional tail of Bank06
 * $976F-$985C. This is not production-scene integration and does not select
 * the shooter or secondary slot. A nonzero shooter predicate applies
 * raw-$0547=$36, raw-$0551=$01, and raw-$057C=$04 to the shooter. A zero
 * secondary predicate applies raw-$046E=$15 to the secondary. All other
 * script fields remain undefined/preserved; no aim, release, attempt, pose,
 * ownership, or scene semantics are inferred.
 */
bool tecmo_gameplay_free_throw_lineup_derive_caller_policy(
    const TecmoGameplayFreeThrowLineupAssets *assets,
    uint8_t orientation,
    uint8_t shooter_slot,
    uint8_t secondary_slot,
    uint8_t shooter_predicate,
    uint8_t secondary_predicate,
    TecmoGameplayFreeThrowLineup *lineup);

/* Pure base-branch decode of Bank06 $9621-$9764.  The caller supplies the
 * already typed primary/defender identities; this resolver makes no claim
 * about how those identities were selected or whether $976F follows. */
bool tecmo_gameplay_free_throw_lineup_derive_round_setup(
    const TecmoGameplayFreeThrowLineupAssets *assets,
    uint8_t orientation,
    uint8_t primary_slot,
    uint8_t defender_slot,
    TecmoGameplayRoundSetup *setup_out);

bool tecmo_gameplay_free_throw_lineup_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size);

#endif
