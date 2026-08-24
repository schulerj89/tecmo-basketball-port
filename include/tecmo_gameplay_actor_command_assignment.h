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

#define TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_COUNT 9U
#define TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_INPUT_TAG 0x31414341U
#define TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_RESULT_TAG 0x31514341U
#define TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_LATCH_TAG 0x314C4341U
#define TECMO_GAMEPLAY_OBJECT10_DISPATCH_RESULT_TAG 0x3144304FU
#define TECMO_GAMEPLAY_OBJECT10_DISPATCH_STATE_COUNT 28U

typedef enum TecmoGameplayActorCommandAssignmentSourceKind {
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_DISTANCE_HELPER = 1,
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_CALLER_AND_ASSIGNMENT = 2,
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_OBJECT_DISPATCH = 3,
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_OBJECT_STATE10 = 4,
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_OBJECT_STATE17_18 = 5,
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_ACTION_DISPATCH = 6,
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_ACTION_SELECTOR = 7,
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_ACTION_TABLE_LOW = 8,
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_ACTION_TABLE_HIGH = 9
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

/* Exact Bank05 producers of the four-byte `$038D-$0390` latch immediately
 * before `$A023` returns to the same gameplay loop's Bank06 traversal. */
typedef enum TecmoGameplayActorCommandAssignmentLatchProducer {
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_LATCH_PRODUCER_NONE = 0,
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_LATCH_PRODUCER_B721 = 1,
    TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_LATCH_PRODUCER_B783 = 2
} TecmoGameplayActorCommandAssignmentLatchProducer;

typedef struct TecmoGameplayActorCommandAssignmentSameFrameLatch {
    uint32_t contract_tag;
    TecmoGameplayCpuSteeringRawTarget16 target;
    TecmoGameplayActorCommandAssignmentLatchProducer producer_kind;
    bool valid;
    /* `$B79A-$B7A1` clears `$0588` bit `$20` only after `$B783` has stored
       all four bytes and called `$A023`. This is ordering evidence, not a
       retained raw-flags mirror. */
    bool b783_bit20_clear_follows_assignment;
    /* Exact actors written to immediate opcode-20 offset `$0019` by the same
       successful `$A023` transaction. Cursor coincidence alone is never
       authorization to consume the latch. */
    uint16_t immediate_opcode20_actor_mask;
} TecmoGameplayActorCommandAssignmentSameFrameLatch;

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

/* Exact Bank05 `$A214-$A225` slot-10 pointer-table dispatch.  The selected
 * handler is read from the validated Rev1 `$A227/$A243` low/high tables; it
 * is never inferred from a native scene enum or duplicated policy table. */
typedef struct TecmoGameplayObject10DispatchResult {
    uint32_t contract_tag;
    uint8_t raw_object_state;
    uint8_t reserved;
    uint16_t handler_cpu;
} TecmoGameplayObject10DispatchResult;

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
    /* Exact source bytes loaded by `$B721`/`$B783`: X is `$7D:$F2`, depth is
       `$FD:$00`. This is separate from the bounded court coordinate used by
       TGCA's distance scan. */
    bool object10_raw_target_valid;
    TecmoGameplayCpuSteeringRawTarget16 object10_raw_target;
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
    uint16_t immediate_opcode20_actor_mask;
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

bool tecmo_gameplay_object10_dispatch_resolve(
    const TecmoGameplayActorCommandAssignmentAssets *assets,
    uint8_t raw_object_state,
    TecmoGameplayObject10DispatchResult *result_out);

/* Transactional typed conversion of the owned $A0A6/$A046 writes only.  It
 * writes $0547/$0551 and $057C equivalents, never $046E, $0484/$048F, $C711,
 * or $9DF6 scratch.  A rejected caller/gate leaves both outputs unchanged. */
bool tecmo_gameplay_actor_command_assignment_apply(
    const TecmoGameplayActorCommandAssignmentAssets *assignment_assets,
    const TecmoGameplayCpuSteeringAssets *steering_assets,
    const TecmoGameplayActorCommandAssignmentInput *input,
    TecmoGameplayLiveFoundation *foundation_io,
    TecmoGameplayActorCommandAssignmentResult *result_out);

/* Atomic typed conversion of one exact B721/B783->$A023 event. The raw latch
 * must equal the court target used by that same assignment (with source-zero
 * depth high byte); assignment, foundation, result, and latch commit together.
 * Rejection leaves all three outputs byte-for-byte unchanged. A later accepted
 * producer overwrites every target/provenance byte. Production attaches this
 * transaction to the source-shaped state-$17 B783 path, A9DA's exact
 * next-update state-$10 B721 path, and the rare pass-catch state-$18 B7B6
 * path. The `$9F2F->$9FE2` interaction predecessor remains unbound. */
bool tecmo_gameplay_actor_command_assignment_apply_and_capture_same_frame_latch(
    const TecmoGameplayActorCommandAssignmentAssets *assignment_assets,
    const TecmoGameplayCpuSteeringAssets *steering_assets,
    const TecmoGameplayActorCommandAssignmentInput *input,
    TecmoGameplayLiveFoundation *foundation_io,
    TecmoGameplayActorCommandAssignmentResult *result_out,
    TecmoGameplayActorCommandAssignmentSameFrameLatch *latch_io);

bool tecmo_gameplay_actor_command_assignment_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size);

#endif
