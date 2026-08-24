#ifndef TECMO_GAMEPLAY_CPU_STEERING_H
#define TECMO_GAMEPLAY_CPU_STEERING_H

#include "tecmo_gameplay_court.h"
#include "tecmo_gameplay_movement.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COUNT 12U
#define TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT 24U
#define TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT 8U
#define TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE 5U
#define TECMO_GAMEPLAY_CPU_STEERING_COMMAND_COUNT 680U
#define TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT 10U
/* Bank04 startup owns object slots 0..10: the ten player slots and the
   separately initialized ball object.  The bounded play executor never
   treats the ball as an actor stream; this identifier is valid only where a
   Bank06 handler reads an object coordinate such as opcode 4 C8. */
#define TECMO_GAMEPLAY_CPU_STEERING_BALL_OBJECT_SLOT \
    TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT
#define TECMO_GAMEPLAY_CPU_STEERING_OBJECT_COUNT \
    (TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT + 1U)
#define TECMO_GAMEPLAY_CPU_STEERING_TEAM_ACTOR_COUNT 5U
#define TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT 2U
#define TECMO_GAMEPLAY_CPU_STEERING_ORIENTATION_COUNT 2U
#define TECMO_GAMEPLAY_CPU_STEERING_DIFFICULTY_COUNT 3U
#define TECMO_GAMEPLAY_CPU_STEERING_FORMATION_START_COUNT 48U
#define TECMO_GAMEPLAY_CPU_STEERING_FORMATION_SOURCE_PINNED_COUNT 46U
#define TECMO_GAMEPLAY_CPU_STEERING_FIXED_LINK_COUNT 10U
#define TECMO_GAMEPLAY_CPU_STEERING_046E_PROBE_COUNT 11U
#define TECMO_GAMEPLAY_CPU_STEERING_CONTROLLER_SLOT_COUNT \
    TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT
#define TECMO_GAMEPLAY_CPU_STEERING_PLAY_STEP_BUDGET 32U
#define TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR 0xFFU
#define TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION 0xFFU
#define TECMO_GAMEPLAY_CPU_STEERING_HARNESS_INPUT_TAG 0x48414754U
#define TECMO_GAMEPLAY_CPU_STEERING_HARNESS_RESULT_TAG 0x52414754U
#define TECMO_GAMEPLAY_CPU_STEERING_MOVEMENT_INPUT_TAG 0x494D4754U
#define TECMO_GAMEPLAY_CPU_STEERING_MOVEMENT_RESULT_TAG 0x524D4754U
#define TECMO_GAMEPLAY_CPU_STEERING_PLAY_STATE_TAG 0x53504C54U
#define TECMO_GAMEPLAY_CPU_STEERING_PLAY_INPUT_TAG 0x49504C54U
#define TECMO_GAMEPLAY_CPU_STEERING_PLAY_RESULT_TAG 0x52504C54U
#define TECMO_GAMEPLAY_CPU_STEERING_FORMATION_RESULT_TAG 0x52464D54U
#define TECMO_GAMEPLAY_CPU_STEERING_ROUTE_INPUT_TAG 0x49525454U
#define TECMO_GAMEPLAY_CPU_STEERING_ROUTE_RESULT_TAG 0x52525454U
#define TECMO_GAMEPLAY_CPU_STEERING_SHOT_INPUT_TAG 0x49534854U
#define TECMO_GAMEPLAY_CPU_STEERING_SHOT_RESULT_TAG 0x52534854U
#define TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_INPUT_TAG 0x49353154U
#define TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_RESULT_TAG 0x52353154U
#define TECMO_GAMEPLAY_CPU_STEERING_ROUTE_LAUNCH_INPUT_TAG 0x494C5254U
#define TECMO_GAMEPLAY_CPU_STEERING_ROUTE_LAUNCH_RESULT_TAG 0x524C5254U
#define TECMO_GAMEPLAY_CPU_STEERING_ROUTE_MOTION_STATE_TAG 0x534D5254U
#define TECMO_GAMEPLAY_CPU_STEERING_ROUTE_STEP_RESULT_TAG 0x52535254U

typedef enum TecmoGameplayCpuSteeringSourceKind {
    TECMO_GAMEPLAY_CPU_STEERING_SOURCE_ACTOR_DISPATCH = 1,
    TECMO_GAMEPLAY_CPU_STEERING_SOURCE_REFERENCE_DIRECTION = 2,
    TECMO_GAMEPLAY_CPU_STEERING_SOURCE_TARGET_DIRECTION = 3,
    TECMO_GAMEPLAY_CPU_STEERING_SOURCE_ROUTE_PROJECTION = 4,
    TECMO_GAMEPLAY_CPU_STEERING_SOURCE_ROUTE_STEP = 5,
    TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COMMAND_DISPATCH = 6,
    TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COMMAND_HANDLERS = 7,
    TECMO_GAMEPLAY_CPU_STEERING_SOURCE_TARGET_APPLY = 8,
    TECMO_GAMEPLAY_CPU_STEERING_SOURCE_FORMATION_STREAM_SELECT = 9,
    TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COMMAND_TRAMPOLINE = 10,
    TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COMMAND_READER = 11,
    TECMO_GAMEPLAY_CPU_STEERING_SOURCE_PLAY_COMMANDS = 12
} TecmoGameplayCpuSteeringSourceKind;

/* This classification names only the bounded effect visible at each exact
   Bank06 handler entry. It is not a complete play-policy or decision model. */
typedef enum TecmoGameplayCpuSteeringCommandKind {
    TECMO_GAMEPLAY_CPU_STEERING_COMMAND_CONTROL = 0,
    TECMO_GAMEPLAY_CPU_STEERING_COMMAND_RELATIVE_TARGET = 1,
    TECMO_GAMEPLAY_CPU_STEERING_COMMAND_ABSOLUTE_TARGET = 2,
    TECMO_GAMEPLAY_CPU_STEERING_COMMAND_ACTOR_TARGET = 3,
    TECMO_GAMEPLAY_CPU_STEERING_COMMAND_DIRECT_DIRECTION = 4,
    TECMO_GAMEPLAY_CPU_STEERING_COMMAND_LINKED_TARGET = 5,
    TECMO_GAMEPLAY_CPU_STEERING_COMMAND_GLOBAL_TARGET = 6,
    TECMO_GAMEPLAY_CPU_STEERING_COMMAND_POINTER_ACTOR_TARGET = 7
} TecmoGameplayCpuSteeringCommandKind;

/* One conservative semantic effect per exact Bank06 dispatch entry. The
   effect names describe bounded writes/control flow, not basketball plays. */
typedef enum TecmoGameplayCpuSteeringEffectKind {
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_RELATIVE_TARGET = 0,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_GOTO,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_ABSOLUTE_TARGET,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_WAIT_COUNTDOWN,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_ACTOR_TARGET,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_DIRECTION_POSE,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_TRANSITION_RESET,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_ACTOR_STATE_BRANCH,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_BOUNDARY_BRANCH,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_STATE_ANIMATION,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_FIXED_LINK_PROXIMITY,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_FIXED_LINK_RELATIVE_POSE,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_FIXED_LINK_FOLLOW_UP,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_GLOBAL_SCRATCH_TARGET,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_GROUP_RESEED,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_PRIMARY_DEFENDER_SWITCH,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_POINTER_ACTOR_TARGET,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_AGGREGATION_BARRIER,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_GLOBAL_TARGET,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_CONDITIONAL_ADVANCE,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_GLOBAL_TIMERS,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_DIRECTION_POSE_ALT,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_COUNT
} TecmoGameplayCpuSteeringEffectKind;

typedef enum TecmoGameplayCpuSteeringAdvancePolicy {
    TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_NONE = 0,
    TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_PLUS_FIVE,
    TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_CONDITIONAL_NONE_OR_FIVE,
    TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_CONDITIONAL_FIVE_OR_TEN,
    TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_PLUS_FIVE_OR_BRANCH_PLUS_FIVE,
    TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_JUMP,
    TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_WAIT,
    TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_PLUS_FIVE_OR_RETRY_CANCEL,
    TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_CONDITIONAL_BA_NONE_OR_FIVE_OR_RETRY_CANCEL
} TecmoGameplayCpuSteeringAdvancePolicy;

/* A deferred command is not a generic failure: Bank06 handlers consume
 * different caller-owned RAM/workspace planes.  Keep the reason typed so LIVE
 * can show exactly which missing owner prevented a source effect. */
typedef enum TecmoGameplayCpuSteeringDeferredReason {
    TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE = 0,
    TECMO_GAMEPLAY_CPU_STEERING_DEFER_INVALID_TARGET_OBJECT,
    TECMO_GAMEPLAY_CPU_STEERING_DEFER_UNSUPPORTED_HANDLER_INPUTS,
    TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_SPECIAL_ACTOR_07DF,
    TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_LINKED_ACTOR_BRANCH_CONTEXT,
    TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_LINKED_RELATIVE_WORKSPACE,
    TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_POINTER_WORKSPACE,
    TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_ACTOR_046E_PROBE,
    TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_COMMON_TAIL_BA,
    TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_OPCODE21_GATE_INPUTS,
    TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_OPCODE15_RAW_LIFECYCLE,
    TECMO_GAMEPLAY_CPU_STEERING_DEFER_NATIVE_TARGET_OUTSIDE_COURT,
    TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_GLOBAL_TARGET,
    TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_OPCODE6_CONTEXT,
    TECMO_GAMEPLAY_CPU_STEERING_DEFER_OPCODE6_CONTROLLED_BRANCH,
    TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_OPCODE23_CONTEXT,
    TECMO_GAMEPLAY_CPU_STEERING_DEFER_OPCODE23_CONTROLLED_BRANCH,
    TECMO_GAMEPLAY_CPU_STEERING_DEFER_REASON_COUNT
} TecmoGameplayCpuSteeringDeferredReason;

typedef struct TecmoGameplayCpuSteeringEffectMetadata {
    TecmoGameplayCpuSteeringEffectKind kind;
    uint16_t handler_cpu;
    uint16_t corpus_count;
    /* True only when the native executor writes the bounded fields proven by
       the handler. Deferred handlers retain their source metadata but do not
       pretend that missing RAM/workspace inputs are command arguments. */
    bool exact_bounded;
    bool deferred_inputs;
    bool native_approximation;
    bool can_jump;
    bool intent_inferred;
    TecmoGameplayCpuSteeringAdvancePolicy advance_policy;
} TecmoGameplayCpuSteeringEffectMetadata;

typedef struct TecmoGameplayCpuSteeringSourceSpan {
    TecmoGameplayCpuSteeringSourceKind kind;
    uint8_t bank;
    bool fixed_bank;
    uint16_t cpu_start;
    uint16_t cpu_end;
    uint32_t byte_count;
    uint32_t fingerprint;
    const uint8_t *bytes;
} TecmoGameplayCpuSteeringSourceSpan;

typedef struct TecmoGameplayCpuSteeringCommand {
    uint16_t stream_offset;
    uint16_t cpu_address;
    uint8_t opcode;
    uint8_t arguments[4];
    uint16_t handler_cpu;
    TecmoGameplayCpuSteeringCommandKind kind;
    TecmoGameplayCpuSteeringEffectKind effect;
} TecmoGameplayCpuSteeringCommand;

typedef struct TecmoGameplayCpuSteeringAssets {
    uint32_t lifecycle_tag;
    bool available;
    char status[192];
    uint8_t *storage;
    size_t storage_size;
    TecmoGameplayCpuSteeringSourceSpan
        sources[TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COUNT];
    uint16_t command_base_cpu;
    uint16_t command_record_count;
    uint16_t handler_cpu[TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT];
    uint16_t command_count_by_opcode[
        TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT];
    TecmoGameplayCpuSteeringEffectMetadata effect_metadata[
        TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT];
    uint8_t direction_map[TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT];
    /* Exact Bank06 $88B0 helper table, imported as a separately
       fingerprinted opcode-15 source contract. This remains a harness-only
       input to the raw resolver; LIVE does not manufacture its raw owners. */
    uint8_t opcode15_pose_low_0442[
        TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT];
    uint8_t opcode15_pose_high_044d[
        TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT];
    uint8_t fixed_link[TECMO_GAMEPLAY_CPU_STEERING_FIXED_LINK_COUNT];
    uint16_t formation_start_count;
    uint16_t formation_source_pinned_count;
    uint16_t formation_stream_offsets[
        TECMO_GAMEPLAY_CPU_STEERING_FORMATION_START_COUNT]
        [TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    uint8_t formation_source_pinned[
        TECMO_GAMEPLAY_CPU_STEERING_FORMATION_START_COUNT];
    uint32_t movement_fingerprint;
} TecmoGameplayCpuSteeringAssets;

typedef struct TecmoGameplayCpuSteeringFormationResult {
    uint32_t contract_tag;
    uint8_t formation_index;
    uint8_t actor_count;
    uint16_t stream_offset[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    bool source_pinned;
} TecmoGameplayCpuSteeringFormationResult;

/* Exact Bank05 $96B6-$9708 route branch. Route names are intentionally not
   exposed: only the two source-pinned command offsets are meaningful here. */
typedef struct TecmoGameplayCpuSteeringRouteInput {
    uint32_t contract_tag;
    uint8_t route_slot;
    uint8_t actor;
    uint8_t control_flags[
        TECMO_GAMEPLAY_CPU_STEERING_CONTROLLER_SLOT_COUNT];
    uint8_t global_0373;
    uint8_t table_index_035A;
    uint8_t flag_0095;
    uint8_t age_0094;
} TecmoGameplayCpuSteeringRouteInput;

typedef struct TecmoGameplayCpuSteeringRouteResult {
    uint32_t contract_tag;
    uint8_t route_slot;
    uint8_t actor;
    uint16_t stream_offset;
    uint8_t actor_state;
    bool wrote_route;
    bool used_long_route;
} TecmoGameplayCpuSteeringRouteResult;

/* Exact planar-arithmetic subset of Bank06 $88DA-$8AF3. Direction, duration,
   Q6 velocity, accumulator seeding, and state-5 activation are reproduced;
   the selected/ordinary pose pointers and $0458/$0479/$046E presentation and
   action side effects remain explicitly outside this API. `condition_7c48` and
   `movement_value_06e7` are explicit raw bytes because LIVE does not yet own
   their complete original actor/profile projection. */
typedef struct TecmoGameplayCpuSteeringRouteLaunchInput {
    uint32_t contract_tag;
    TecmoGameplayCourtCoordinate actor_position;
    int16_t horizontal_delta;
    int16_t depth_delta;
    uint8_t condition_7c48;
    uint8_t movement_value_06e7;
} TecmoGameplayCpuSteeringRouteLaunchInput;

/* Actor-local Q6 state written by Bank06 $89D6-$8A8A and consumed by state
   5 at $8AF4-$8B8F. The accumulators and velocities deliberately retain raw
   wrapping 16-bit representation. */
typedef struct TecmoGameplayCpuSteeringRouteMotionState {
    uint32_t contract_tag;
    uint16_t horizontal_accumulator_q6;
    uint16_t depth_accumulator_q6;
    int16_t horizontal_velocity_q6;
    int16_t depth_velocity_q6;
    uint16_t remaining_timer;
    bool active;
} TecmoGameplayCpuSteeringRouteMotionState;

typedef struct TecmoGameplayCpuSteeringRouteLaunchResult {
    uint32_t contract_tag;
    TecmoGameplayCpuSteeringRouteMotionState motion;
    uint8_t direction;
    uint16_t duration;
    bool launched;
} TecmoGameplayCpuSteeringRouteLaunchResult;

typedef struct TecmoGameplayCpuSteeringRouteStepResult {
    uint32_t contract_tag;
    uint8_t actor;
    uint16_t horizontal_position;
    uint8_t depth_position;
    uint16_t timer_before;
    uint16_t timer_after;
    bool finished;
} TecmoGameplayCpuSteeringRouteStepResult;

typedef struct TecmoGameplayCpuSteeringPlayState {
    uint32_t contract_tag;
    uint16_t stream_offset[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    uint8_t actor_state[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    uint8_t wait_counter[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    uint8_t direction[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    /* Exact actor-local raw `$0442` pose-low and packed `$0458` action bytes.
       These are source state, not native scene pose indexes. */
    uint8_t pose[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    uint8_t action[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    /* Bounded actor/object-state $046E[X] projection for converted Bank06
       command handlers and Bank05 $B24F-$B2CB pass/defender handoff resets.
       This remains incomplete object-lifecycle ownership. */
    uint8_t action_state_046e[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    /* Source target-object identity. Slots 0..9 identify player
       coordinates; slot 10 is the typed ball coordinate for the exact
       opcode-4 C8 lookup. NO_ACTOR remains the no-object sentinel. Opcode 13
       stores its two raw latch words in the signed fields bit-for-bit; callers
       must use raw-target provenance rather than court-coordinate semantics. */
    uint8_t target_object[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    int16_t target_x[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    int16_t target_depth[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    uint8_t fixed_link[TECMO_GAMEPLAY_CPU_STEERING_FIXED_LINK_COUNT];
    /* Native fixed-link projection used by bounded integration. This is not
       live ownership of the ROM's dynamic $037F candidate or $06CB link
       vectors. Exact startup seeds remain separate in matchup_seed and
       fixed_link. */
    uint8_t fixed_link_target[
        TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    uint8_t primary_actor;
    uint8_t defender_actor;
    uint8_t candidate_actor;
    uint8_t matchup_seed[2];
    /* Exact opcode-22 mirrors: C8->$0791, C9->$0792, CA ORs $0790. */
    uint8_t global_0790;
    uint8_t global_0791;
    uint8_t global_0792;
    /* Exact opcode-17/18/19 aggregation/barrier fields. */
    uint8_t aggregation_06e0;
    uint8_t aggregation_06df;
    uint8_t aggregation_06e1;
    /* LIVE owner for the exact Bank06 state-5 planar route kernel. Opcode 4
       captures its target once; lifecycle transitions cancel the route
       transaction instead of retaining stale target/Q6 state. */
    TecmoGameplayCpuSteeringRouteMotionState
        route_motion[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    uint16_t step_serial;
} TecmoGameplayCpuSteeringPlayState;

typedef struct TecmoGameplayCpuSteeringRawTarget16 {
    uint16_t x;
    uint16_t depth;
} TecmoGameplayCpuSteeringRawTarget16;

typedef struct TecmoGameplayCpuSteeringPlayInput {
    uint32_t contract_tag;
    uint8_t actor;
    uint8_t step_budget;
    /* Exact $035A orientation selector; only 0 and 1 are valid. */
    uint8_t orientation_035a;
    /* Bank06 $92CA consumes $BA low bits for the common target tail. A value
       is meaningful only when the caller has a faithful owner; ordinary LIVE
       may supply the typed no-transient-action zero, while all other paths
       remain unavailable rather than treating zero as raw RAM. */
    bool common_tail_ba_available;
    uint8_t flags_ba;
    /* Exact $04B0 values for opcode 14; bit $10 is the qualification gate. */
    uint8_t actor_04b0[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    /* Opcode 7 indexes the original $046E table with C8. The corpus uses
       C8=$0A, so this bounded probe intentionally has eleven entries.  The
       table is never read unless the caller can retain its lifecycle. */
    bool actor_046e_probe_available;
    uint8_t actor_046e_probe[TECMO_GAMEPLAY_CPU_STEERING_046E_PROBE_COUNT];
    /* Bank06 opcode 6 branches on selected-primary controller ownership.
       Only the automatic/no-controller path is bounded; the controlled path
       reaches the wider `$89DB` lifecycle and remains explicitly deferred. */
    bool opcode6_context_available;
    bool opcode6_automatic;
    /* The sole opcode-23 predecessor is bounded only for the selected
       automatic/uncontrolled branch, which advances without RNG reads or a
       direction write. */
    bool opcode23_context_available;
    bool opcode23_uncontrolled;
    /* Exact opcode-21 gate inputs ($058A/$0357/$0358/$7E). */
    bool opcode21_gate_inputs_available;
    uint8_t state_058a;
    uint8_t state_0357;
    uint8_t state_0358;
    uint8_t flags_007e;
    /* Bank06 opcode 13 consumes `$038D:$038E/$038F:$0390` as two raw
       little-endian 16-bit words. They are not court-coordinate fields: both
       high bytes are live source data. Availability requires a faithful
       producer lifecycle; ordinary LIVE deliberately leaves it false. */
    bool global_target_available;
    TecmoGameplayCpuSteeringRawTarget16 global_target;
    /* Bounded opcode-10 workspace produced by $8D59-$8E21. Callers must
       explicitly prove these signed relative offsets; absence defers the
       command rather than silently substituting zero. */
    bool linked_relative_valid;
    int16_t linked_relative_x;
    int16_t linked_relative_depth;
    /* $07DF selects the exceptional primary-actor route in $8CD0-$8D64.
       NO_ACTOR is the explicit unavailable sentinel, not a replacement
       value for slot zero. */
    bool special_actor_07df_available;
    uint8_t special_actor_07df;
    /* Bank06 $8CD0 also consumes $0478/$06CB/$0308 before $8D59. This bit
       requires that exact branch context; a fixed matchup actor is not it. */
    bool linked_actor_branch_context_available;
    /* A caller may provide the already resolved $0308/$06CB result when it
       can prove the $07DF branch without mirroring $07DF. */
    bool linked_actor_resolved_valid;
    uint8_t linked_actor;
    /* Opcode 16 compares the two exact 16-bit workspaces at $036E/$0370.
       The command's $0309 pointer is resolved through typed play state. */
    bool pointer_workspace_valid;
    uint16_t workspace_036e;
    uint16_t workspace_0370;
    TecmoGameplayCourtCoordinate
        actor_position[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    /* Production supplies the canonical visible ball coordinate separately
       from the ten player coordinates.  This narrowly represents Bank06
       opcode 4's validated C8=$0A object lookup; it is not an eleventh actor
       stream or a CPU play-selection input. */
    TecmoGameplayCourtCoordinate ball_position;
} TecmoGameplayCpuSteeringPlayInput;

typedef struct TecmoGameplayCpuSteeringPlayResult {
    uint32_t contract_tag;
    uint8_t actor;
    uint8_t steps_executed;
    uint8_t step_budget;
    TecmoGameplayCpuSteeringCommand command;
    TecmoGameplayCpuSteeringEffectKind effect;
    uint16_t previous_offset;
    uint16_t next_offset;
    uint16_t jump_offset;
    uint8_t target_object;
    int16_t target_x;
    int16_t target_depth;
    /* Opcodes 13 and 20 retain the exact raw latch words as subtraction
       evidence; they are not necessarily court-valid. Only opcode 13 writes
       those bit patterns into the actor target-storage planes. */
    bool raw_target_valid;
    uint16_t raw_target_x;
    uint16_t raw_target_depth;
    /* Exact opcode-11 raw pose/state evidence. `$0479=$C1` is excluded until
       a typed sprite-plane owner exists. */
    bool raw_pose_valid;
    uint8_t raw_pose_low_0442;
    uint8_t raw_pose_high_044d;
    uint8_t raw_packed_action_0458;
    /* Exact automatic opcode-6 writes retained by this bounded result. The
       other handler writes `$0743/$0588` remain observations only. */
    bool opcode6_action10_written;
    bool opcode6_object10_state_written;
    uint8_t opcode6_object10_state;
    /* Exact opcode-4/opcode-13/opcode-20 subtraction evidence. Opcode 4 uses
       its 16-bit-X/8-bit-depth object coordinate. Opcodes 13 and 20 subtract
       the actor's 16-bit X and zero-extended 8-bit depth from the raw latch. */
    int16_t target_horizontal_delta;
    int16_t target_depth_delta;
    bool fetched;
    bool advanced;
    bool jumped;
    bool waiting;
    bool budget_exhausted;
    bool deferred;
    TecmoGameplayCpuSteeringDeferredReason deferred_reason;
    bool proximity_met;
    /* The handlers OR both 16-bit deltas and skip $88DA on zero, preserving
       prior direction. This flag is meaningful for opcodes 4 and 13. */
    bool target_vector_zero;
} TecmoGameplayCpuSteeringPlayResult;

/* Bank06 $9172 opcode-15 branch classification. Only GATE_NOOP and the
   selected-defender write path are executable in the raw harness. The other
   labels are source-control-flow observations, deliberately not native
   gameplay policy. */
typedef enum TecmoGameplayCpuSteeringOpcode15Branch {
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_NONE = 0,
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_GATE_NOOP,
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_MISSING_RAW,
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_PRIMARY_RETRY,
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_PRIMARY_SWAP,
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_MARK_OTHER,
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_INVALID_DIRECTION,
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFENDER_REPLACED
} TecmoGameplayCpuSteeringOpcode15Branch;

/* The raw contract intentionally names RAM ownership rather than providing
   values through PlayInput. A caller may only claim a bit after observing the
   corresponding value at the exact opcode-15 execution point. */
typedef enum TecmoGameplayCpuSteeringOpcode15RawObserved {
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_SLOT10_0499 = 1U << 0U,
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_ACTOR_04B0 = 1U << 1U,
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_FLAGS_007E = 1U << 2U,
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_PRIMARY_0308 = 1U << 3U,
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_DEFENDER_0309 = 1U << 4U,
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_OFFENSE_SIDE_030A = 1U << 5U,
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_DEFENSE_SIDE_030B = 1U << 6U,
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_SIDE_SELECTION_000E = 1U << 7U,
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_SELECTION_06D5 = 1U << 8U,
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_SELECTION_06D6 = 1U << 9U,
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_ACTOR_LIFECYCLE = 1U << 10U,
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_DIRECTION_0463 = 1U << 11U,
    /* Canonical Rev1 $920D stores the selected X into $059E before it
       passes selector 4 to Bank07 $C711. */
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_SELECTION_059E = 1U << 12U
} TecmoGameplayCpuSteeringOpcode15RawObserved;

#define TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_KNOWN_MASK \
    ((uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_SLOT10_0499 | \
     (uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_ACTOR_04B0 | \
     (uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_FLAGS_007E | \
     (uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_PRIMARY_0308 | \
     (uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_DEFENDER_0309 | \
     (uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_OFFENSE_SIDE_030A | \
     (uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_DEFENSE_SIDE_030B | \
     (uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_SIDE_SELECTION_000E | \
     (uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_SELECTION_06D5 | \
     (uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_SELECTION_06D6 | \
     (uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_ACTOR_LIFECYCLE | \
     (uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_DIRECTION_0463 | \
     (uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_SELECTION_059E)

typedef struct TecmoGameplayCpuSteeringOpcode15RawActor {
    uint16_t raw_0547_0551_stream_offset;
    uint8_t raw_057c_state;
    uint8_t raw_046e_timer;
    uint8_t raw_0463_direction;
    uint8_t raw_0442_pose_low;
    uint8_t raw_044d_pose_high;
    uint8_t raw_0479_sprite_flags;
    uint8_t raw_0458_action;
} TecmoGameplayCpuSteeringOpcode15RawActor;

/* Complete externally captured raw snapshot for one opcode-15 record. It is
   intentionally separate from the LIVE scene and uses no shadow mirrors. */
typedef struct TecmoGameplayCpuSteeringOpcode15RawInput {
    uint32_t contract_tag;
    uint32_t observed_mask;
    uint16_t command_record_offset;
    uint8_t actor_x;
    uint8_t raw_0499_slot10;
    uint8_t raw_04b0_actor_x;
    uint8_t raw_007e;
    uint8_t raw_0308_primary;
    uint8_t raw_0309_defender;
    uint8_t raw_030a_offense_side;
    uint8_t raw_030b_defense_side;
    uint8_t raw_000e_000f_selected_actor[
        TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT];
    uint8_t raw_06d5;
    uint8_t raw_06d6;
    uint8_t raw_059e;
    TecmoGameplayCpuSteeringOpcode15RawActor actor[
        TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
} TecmoGameplayCpuSteeringOpcode15RawInput;

typedef struct TecmoGameplayCpuSteeringOpcode15RawResult {
    uint32_t contract_tag;
    uint32_t observed_mask;
    uint32_t missing_raw_mask;
    uint16_t command_record_offset;
    uint8_t opcode;
    uint8_t actor_x;
    uint8_t raw_0499_slot10;
    uint8_t raw_04b0_actor_x;
    uint8_t raw_007e;
    uint8_t raw_0308_before;
    uint8_t raw_0308_after;
    uint8_t raw_0309_before;
    uint8_t raw_0309_after;
    uint16_t defender_stream_before;
    uint16_t defender_stream_after;
    uint8_t defender_state_before;
    uint8_t defender_state_after;
    uint8_t new_actor_state_before;
    uint8_t new_actor_state_after;
    uint8_t raw_06d5_before;
    uint8_t raw_06d5_after;
    uint8_t raw_06d6_before;
    uint8_t raw_06d6_after;
    uint8_t raw_059e_before;
    uint8_t raw_059e_after;
    uint8_t c711_selector;
    uint8_t c711_x_actor;
    uint8_t c711_y_actor;
    TecmoGameplayCpuSteeringOpcode15Branch branch;
    bool committed;
    /* The resolver captures the immediately preceding selector/X/Y state;
       it does not claim to execute or translate Bank07 $C711. */
    bool c711_selector_observed_unexecuted;
} TecmoGameplayCpuSteeringOpcode15RawResult;

typedef struct TecmoGameplayCpuSteeringShotInput {
    uint32_t contract_tag;
    uint8_t state_0588;
    uint8_t flags_ba;
    uint8_t target_delta_low;
    uint8_t target_delta_high;
    uint8_t gate_0478;
    uint8_t timer_0798;
    uint8_t difficulty;
    uint8_t timer_0760;
    uint8_t rating_0533;
    uint8_t random_byte;
} TecmoGameplayCpuSteeringShotInput;

typedef struct TecmoGameplayCpuSteeringShotResult {
    uint32_t contract_tag;
    uint8_t difficulty_threshold;
    uint8_t timer_sum;
    uint8_t rating_bucket;
    uint8_t actor_state;
    uint8_t action;
    uint8_t timer;
    uint8_t handoff_code;
    bool request;
    bool wrote_state;
} TecmoGameplayCpuSteeringShotResult;

/* Bounded native composition policy for exercising the isolated TGAI
   direction boundary with a complete canonical court snapshot. The CLI and
   live scene share it. Its legacy matchup input is separate from the
   lifecycle's exact fixed-link seed {5,6,7,8,9,0,1,2,3,4}; it is not a claim
   that the harness owns the ROM's dynamic candidate/$037F assignment.
   Non-holder live targets are explicit canonical coordinates supplied by the
   native policy. Slots 0..4 are away/team 0 and slots 5..9 are home/team 1,
   matching TecmoGameplayScene. This is not a reconstructed ROM play
   selector. */
typedef enum TecmoGameplayCpuSteeringHarnessTargetKind {
    TECMO_GAMEPLAY_CPU_STEERING_HARNESS_LINKED_ACTOR = 0,
    TECMO_GAMEPLAY_CPU_STEERING_HARNESS_HOOP_APPROACH,
    TECMO_GAMEPLAY_CPU_STEERING_HARNESS_EXPLICIT_TARGET,
    /* LIVE-only source opcode-4 target: Bank04 object slot 10's canonical
       ball coordinate. The general harness does not select this policy. */
    TECMO_GAMEPLAY_CPU_STEERING_HARNESS_BALL_OBJECT_TARGET,
    TECMO_GAMEPLAY_CPU_STEERING_HARNESS_TARGET_KIND_COUNT
} TecmoGameplayCpuSteeringHarnessTargetKind;

typedef struct TecmoGameplayCpuSteeringHarnessInput {
    uint32_t contract_tag;
    TecmoGameplayCourtCoordinate
        actor_position[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    uint8_t actor;
    uint8_t possession;
    uint8_t orientation;
    uint8_t ball_holder;
    uint8_t matchup_actor;
    uint8_t difficulty;
    /* When false, preserve the existing harness selection: the holder uses
       the hoop-approach target and a non-holder uses the linked-actor
       coordinate. The live scene sets this true for every non-holder so its
       live target is an explicit native-policy coordinate; the fixed
       matchup/pose/defender reference remains matchup_actor. */
    bool has_explicit_target;
    TecmoGameplayCourtCoordinate explicit_target;
} TecmoGameplayCpuSteeringHarnessInput;

typedef struct TecmoGameplayCpuSteeringHarnessResult {
    uint32_t contract_tag;
    uint32_t input_fingerprint;
    TecmoGameplayCourtCoordinate actor_position;
    TecmoGameplayCourtCoordinate target_position;
    int16_t horizontal_delta;
    int16_t depth_delta;
    uint8_t actor;
    uint8_t actor_team;
    uint8_t possession;
    uint8_t orientation;
    uint8_t ball_holder;
    /* Fixed matchup/pose/defender-reference metadata; this does not replace
       the non-holder's explicit live target coordinate. */
    uint8_t matchup_actor;
    uint8_t difficulty;
    uint8_t target_actor;
    TecmoGameplayCpuSteeringHarnessTargetKind target_kind;
    uint8_t direction;
    bool writes_direction;
} TecmoGameplayCpuSteeringHarnessResult;

/* Transactional CLI/live composition input. The selected actor's canonical
   coordinate must exactly match movement.position. TGAI selects the target
   and direction; the remaining fields are passed to the exact TGMO movement
   kernel as a secondary (non-player-selected) actor. */
typedef struct TecmoGameplayCpuSteeringMovementInput {
    uint32_t contract_tag;
    TecmoGameplayCpuSteeringHarnessInput steering;
    TecmoGameplayMovementState movement;
    uint8_t player_movement_rating;
    uint8_t condition;
    uint8_t speed_value;
    uint8_t global_object_state;
    uint8_t movement_flags;
    /* True only for the current offensive primary/ball holder. CPU control
       does not by itself make an actor secondary in Bank05's clamp route. */
    bool primary_selected_actor;
} TecmoGameplayCpuSteeringMovementInput;

typedef struct TecmoGameplayCpuSteeringMovementResult {
    uint32_t contract_tag;
    TecmoGameplayCpuSteeringHarnessResult steering;
    TecmoGameplayMovementState movement;
    uint8_t held_direction_bits;
} TecmoGameplayCpuSteeringMovementResult;

void tecmo_gameplay_cpu_steering_assets_init(
    TecmoGameplayCpuSteeringAssets *assets);
void tecmo_gameplay_cpu_steering_assets_destroy(
    TecmoGameplayCpuSteeringAssets *assets);

bool tecmo_gameplay_cpu_steering_assets_parse(
    TecmoGameplayCpuSteeringAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *movement,
    size_t movement_size);
bool tecmo_gameplay_cpu_steering_assets_load(
    TecmoGameplayCpuSteeringAssets *assets,
    const char *asset_pack_path);

const TecmoGameplayCpuSteeringSourceSpan *
tecmo_gameplay_cpu_steering_find_source(
    const TecmoGameplayCpuSteeringAssets *assets,
    TecmoGameplayCpuSteeringSourceKind kind);

/* `stream_offset` is the exact actor-local $0547/$0551 offset added to
   Bank04 CPU base $9F2E. It must be aligned to a five-byte command record. */
bool tecmo_gameplay_cpu_steering_decode_command(
    const TecmoGameplayCpuSteeringAssets *assets,
    uint16_t stream_offset,
    TecmoGameplayCpuSteeringCommand *command_out);

/* Exact $92D4-$92DD target gate plus $88DA-$899D octant decision, including
   the 6502 routine's unsigned 16-bit magnitude/doubling behavior. Deltas are
   target minus actor in the ROM's horizontal and court-depth axes. The gate
   skips a zero vector, represented here by a transactional false return. */
bool tecmo_gameplay_cpu_steering_direction_for_delta(
    const TecmoGameplayCpuSteeringAssets *assets,
    int16_t horizontal_delta,
    int16_t depth_delta,
    uint8_t *direction_out);

const char *tecmo_gameplay_cpu_steering_direction_name(uint8_t direction);
const char *tecmo_gameplay_cpu_steering_command_kind_name(
    TecmoGameplayCpuSteeringCommandKind kind);
const TecmoGameplayCpuSteeringEffectMetadata *
tecmo_gameplay_cpu_steering_effect_metadata(
    const TecmoGameplayCpuSteeringAssets *assets,
    uint8_t opcode);
const char *tecmo_gameplay_cpu_steering_effect_name(
    TecmoGameplayCpuSteeringEffectKind kind);

bool tecmo_gameplay_cpu_steering_formation_select(
    const TecmoGameplayCpuSteeringAssets *assets,
    uint8_t formation_index,
    TecmoGameplayCpuSteeringFormationResult *result_out);
bool tecmo_gameplay_cpu_steering_route_select(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayCpuSteeringRouteInput *input,
    TecmoGameplayCpuSteeringRouteResult *result_out);
bool tecmo_gameplay_cpu_steering_route_launch(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayCpuSteeringRouteLaunchInput *input,
    TecmoGameplayCpuSteeringRouteLaunchResult *result_out);
bool tecmo_gameplay_cpu_steering_route_step(
    uint8_t actor,
    uint8_t completion_side_bit_0359,
    const TecmoGameplayCpuSteeringRouteMotionState *state_in,
    TecmoGameplayCpuSteeringRouteMotionState *state_out,
    TecmoGameplayCpuSteeringRouteStepResult *result_out);
bool tecmo_gameplay_cpu_steering_play_state_initialize(
    const TecmoGameplayCpuSteeringAssets *assets,
    uint8_t formation_index,
    TecmoGameplayCpuSteeringPlayState *state_out);
bool tecmo_gameplay_cpu_steering_play_step(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayCpuSteeringPlayState *state_in,
    const TecmoGameplayCpuSteeringPlayInput *input,
    TecmoGameplayCpuSteeringPlayState *state_out,
    TecmoGameplayCpuSteeringPlayResult *result_out);
const char *tecmo_gameplay_cpu_steering_deferred_reason_name(
    TecmoGameplayCpuSteeringDeferredReason reason);

/* Harness-only Bank06 $9172-$9216 source contract. This resolver never reads
   or writes LIVE scene state. It copies input to output transactionally,
   classifies gate/primary/mark-other branches without inventing their missing
   owners, and applies the exact selected-defender stores only when the raw
   snapshot explicitly observes every required field. */
bool tecmo_gameplay_cpu_steering_opcode15_resolve_raw(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayCpuSteeringOpcode15RawInput *input,
    TecmoGameplayCpuSteeringOpcode15RawInput *output,
    TecmoGameplayCpuSteeringOpcode15RawResult *result_out);
const char *tecmo_gameplay_cpu_steering_opcode15_branch_name(
    TecmoGameplayCpuSteeringOpcode15Branch branch);
bool tecmo_gameplay_cpu_steering_shot_request(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayCpuSteeringShotInput *input,
    TecmoGameplayCpuSteeringShotResult *result_out);

/* Pure and transactional. Every one of the ten coordinates is validated and
   included in input_fingerprint. A caller may set has_explicit_target and
   provide a validated canonical coordinate; otherwise the holder uses the
   current native hoop-approach policy and every other actor uses its explicit
   opposing linked/matchup actor. Only the final octant is ROM-exact. A zero
   delta succeeds with writes_direction=false, mirroring the ROM gate that
   preserves the caller's prior direction. */
bool tecmo_gameplay_cpu_steering_harness_evaluate(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayCpuSteeringHarnessInput *input,
    TecmoGameplayCpuSteeringHarnessResult *result_out);
const char *tecmo_gameplay_cpu_steering_harness_target_kind_name(
    TecmoGameplayCpuSteeringHarnessTargetKind kind);

/* `play_step` is transactional: invalid tags, actors, offsets, positions,
   or budgets return false without writing either output. `state_out` and
   `result_out` may not alias state_in, input, or one another; every such
   cross-object alias is rejected before any write. Only opcode 1 chains into
   another record in the same tick; all other handlers stop after one bounded
   effect/transport decision. A bounded step budget prevents opcode-1 loops
   from becoming an unbounded native tick. Deferred effect inputs do not defer
   a transport path that is independently proven (for example, opcode 5's
   +5 advance). */

/* Pure and transactional TGAI -> TGMO composition. A nonzero TGAI
   result is inverted through TGMO's validated direction table and supplied
   as NES held-direction bits. TGAI's zero-vector no-write case has no NES
   input equivalent, so this adapter-owned boundary supplies neutral while
   preserving TGMO's exact one-update action-state latency. The live scene
   uses this API with scene-owned fixed opposing links. It still does not own
   or invoke the isolated ROM command/link lifecycle. Normal scene lifecycle
   integration remains a later R1-LIVE boundary. */
bool tecmo_gameplay_cpu_steering_movement_step(
    const TecmoGameplayCpuSteeringAssets *steering_assets,
    const TecmoGameplayMovementAssets *movement_assets,
    const TecmoGameplayCpuSteeringMovementInput *input,
    TecmoGameplayCpuSteeringMovementResult *result_out);

bool tecmo_gameplay_cpu_steering_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size);

#endif
