#ifndef TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_H
#define TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_H

/*
 * Strict, bounded observation of Bank05 $A023-$A0DC.  This is deliberately
 * not a generic post-possession, post-tip, rebound, or handoff operation.
 * The callable translation accepts only the three object-dispatch callers
 * whose raw gate inputs are represented explicitly below.  The $9F2F->$9FE2
 * interaction caller remains diagnostic-only: its preceding geometry/property
 * state has no faithful native owner yet.
 */

#include "tecmo_gameplay_live_foundation.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_COUNT 8U
#define TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_INPUT_TAG 0x31414341U
#define TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_RESULT_TAG 0x31514341U

typedef enum TecmoGameplayActorCommandAssignmentSourceKind {
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_DISTANCE_HELPER = 1,
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_CALLER_AND_ASSIGNMENT = 2,
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_OBJECT_STATE10 = 3,
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_OBJECT_STATE17 = 4,
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_ACTION_DISPATCH = 5,
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_ACTION_SELECTOR = 6,
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_ACTION_TABLE_LOW = 7,
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_ACTION_TABLE_HIGH = 8
} TecmoGameplayActorCommandAssignmentSourceKind;

typedef enum TecmoGameplayActorCommandAssignmentCaller {
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_CALLER_NONE = 0,
    /* Bank05 $9F2F -> $9FE2. Not callable until its full predecessor state
       has an owned native representation. */
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_CALLER_INTERACTION_9FE2 = 1,
    /* $A214 state $10 -> $B6E5 -> $B73A -> $A023. */
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_CALLER_OBJECT_STATE10_B73A = 2,
    /* $A214 state $17 -> $B775 -> $B783 -> $A023. */
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_CALLER_OBJECT_STATE17_B783 = 3,
    /* $A214 state $18 -> $B7B6 -> $B783 -> $A023. */
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_CALLER_OBJECT_STATE18_B7B6 = 4
} TecmoGameplayActorCommandAssignmentCaller;

typedef enum TecmoGameplayActorCommandAssignmentNoopReason {
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_NOOP_NONE = 0,
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_NOOP_BAD_CONTRACT = 1,
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_NOOP_INTERACTION_UNOWNED = 2,
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_NOOP_CALLER_GATES = 3,
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_NOOP_TARGET_UNOWNED = 4,
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_NOOP_FOUNDATION_INVALID = 5
} TecmoGameplayActorCommandAssignmentNoopReason;

typedef struct TecmoGameplayActorCommandAssignmentSourceSpan {
    TecmoGameplayActorCommandAssignmentSourceKind kind;
    uint8_t bank;
    bool fixed_bank;
    uint16_t cpu_start;
    uint16_t cpu_end;
    uint32_t byte_count;
    uint32_t fingerprint_fnv1a32;
    uint64_t fingerprint_fnv1a64;
    const uint8_t *bytes;
} TecmoGameplayActorCommandAssignmentSourceSpan;

typedef struct TecmoGameplayActorCommandAssignmentAssets {
    uint32_t lifecycle_tag;
    bool available;
    char status[176];
    uint8_t *storage;
    size_t storage_size;
    TecmoGameplayActorCommandAssignmentSourceSpan
        sources[TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_COUNT];
} TecmoGameplayActorCommandAssignmentAssets;

/* Raw-gate inputs are intentionally explicit.  Current scene state does not
 * retain them, so production must not synthesize this structure from a tip,
 * possession handoff, shot-state number, or visual transition.  target is a
 * source-normalized typed object-slot-10 coordinate consumed by $9E0A/$A046
 * when a source caller has established it.  TGCA does not claim a general
 * mapping from the native court coordinate or CPU opcode-4 target to that raw
 * Bank05 object coordinate. */
typedef struct TecmoGameplayActorCommandAssignmentInput {
    uint32_t contract_tag;
    TecmoGameplayActorCommandAssignmentCaller caller;
    uint8_t raw_object_state;
    uint8_t raw_ba;
    uint8_t raw_05a1;
    uint8_t raw_0499;
    uint8_t raw_0588;
    uint8_t raw_0067;
    uint8_t raw_0068;
    uint8_t raw_04af;
    bool object10_target_valid;
    TecmoGameplayCourtCoordinate object10_target;
} TecmoGameplayActorCommandAssignmentInput;

typedef struct TecmoGameplayActorCommandAssignmentScan {
    bool executed;
    uint8_t required_04b0_bit10;
    uint8_t winner_actor;
    uint16_t winner_score;
    uint8_t excluded_primary_actor;
    uint8_t excluded_defender_actor;
    bool no_candidate;
} TecmoGameplayActorCommandAssignmentScan;

typedef struct TecmoGameplayActorCommandAssignmentResult {
    uint32_t contract_tag;
    TecmoGameplayActorCommandAssignmentCaller caller;
    TecmoGameplayActorCommandAssignmentNoopReason noop_reason;
    bool applied;
    bool primary_automatic;
    bool defender_automatic;
    bool unsupported_primary_046e18_observed;
    bool unsupported_clear_0484_048f_observed;
    /* $A046 dispatches fixed $C711 with action #$1D.  TGCA records that
       source action as an observation only; no native C711/table owner is
       installed by this bounded resolver. */
    bool unsupported_c711_action1d_observed;
    bool unsupported_terminal_9df6_scratch_observed;
    uint16_t primary_stream_before;
    uint16_t primary_stream_after;
    uint8_t primary_state_before;
    uint8_t primary_state_after;
    uint16_t defender_stream_before;
    uint16_t defender_stream_after;
    uint8_t defender_state_before;
    uint8_t defender_state_after;
    TecmoGameplayActorCommandAssignmentScan side10_scan;
    TecmoGameplayActorCommandAssignmentScan side00_scan;
} TecmoGameplayActorCommandAssignmentResult;

void tecmo_gameplay_actor_command_assignment_assets_init(
    TecmoGameplayActorCommandAssignmentAssets *assets);
void tecmo_gameplay_actor_command_assignment_assets_destroy(
    TecmoGameplayActorCommandAssignmentAssets *assets);
bool tecmo_gameplay_actor_command_assignment_assets_parse(
    TecmoGameplayActorCommandAssignmentAssets *assets,
    const uint8_t *payload,
    size_t payload_size);
bool tecmo_gameplay_actor_command_assignment_assets_load(
    TecmoGameplayActorCommandAssignmentAssets *assets,
    const char *asset_pack_path);
const TecmoGameplayActorCommandAssignmentSourceSpan *
tecmo_gameplay_actor_command_assignment_find_source(
    const TecmoGameplayActorCommandAssignmentAssets *assets,
    TecmoGameplayActorCommandAssignmentSourceKind kind);

/* Transactional typed conversion of the owned $A0A6/$A046 writes only.  It
 * writes $0547/$0551 and $057C equivalents, never $046E, $0484/$048F, $C711,
 * or $9DF6 scratch.  A rejected caller/gate leaves both outputs unchanged. */
bool tecmo_gameplay_actor_command_assignment_apply(
    const TecmoGameplayActorCommandAssignmentAssets *assignment_assets,
    const TecmoGameplayCpuSteeringAssets *steering_assets,
    const TecmoGameplayActorCommandAssignmentInput *input,
    TecmoGameplayLiveFoundation *foundation_io,
    TecmoGameplayActorCommandAssignmentResult *result_out);

bool tecmo_gameplay_actor_command_assignment_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size);

#endif
