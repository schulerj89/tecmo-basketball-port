#ifndef TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACES_H
#define TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACES_H

#include "tecmo_gameplay_court.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * This is deliberately a harness-only boundary.  It does not create a RAM
 * mirror or provide a LIVE producer.  A caller may mark a bit observed only
 * when it captured the corresponding source owner at the same command point.
 */
#define TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_EVIDENCE_TAG 0x45575043U
#define TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_ASSESSMENT_TAG 0x41575043U
#define TECMO_GAMEPLAY_CPU_OPCODE10_WORKSPACE_INPUT_TAG 0x49315043U
#define TECMO_GAMEPLAY_CPU_OPCODE10_WORKSPACE_RESULT_TAG 0x52315043U
#define TECMO_GAMEPLAY_CPU_OPCODE10_LIVE_PROJECTION_TAG 0x4C315043U
#define TECMO_GAMEPLAY_CPU_OPCODE16_WORKSPACE_INPUT_TAG 0x49365043U
#define TECMO_GAMEPLAY_CPU_OPCODE16_WORKSPACE_RESULT_TAG 0x52365043U
#define TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_ACTOR_COUNT 10U

typedef enum TecmoGameplayCpuOpcodeWorkspaceObserved {
    /* Bank06 $8F12-$8F29: C8 indexes $046E, including object slot 10. */
    TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_046E_PROBE = 1U << 0U,
    /* Bank06 $8CD0-$8CE2: $07DF/$0478/$06CB/$0308 branch context. */
    TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_10_ENTRY_LINK = 1U << 1U,
    /* Bank06 $8D59-$8E21: captured relative-window output and timing. */
    TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_10_RELATIVE_WINDOW = 1U << 2U,
    /* Bank06 $8E4F-$8ED3 and $92A8 continuation after opcode 10. */
    TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_10_CONTINUATION = 1U << 3U,
    /* Bank06 $9085-$9089 command pointer and its typed $0309 target. */
    TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_16_POINTER_TARGET = 1U << 4U,
    /* Bank05 $9054-$90AF's $036E/$0370 values at the command point. */
    TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_16_DISTANCE = 1U << 5U,
    /* Bank06 $92BA-$9314's target-application continuation. */
    TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_TARGET_APPLY = 1U << 6U,
    /* Mutable $BA low two bits, never derived from a frame counter. */
    TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_BA_LOW_BITS = 1U << 7U
} TecmoGameplayCpuOpcodeWorkspaceObserved;

#define TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_KNOWN_MASK \
    ((uint32_t)TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_046E_PROBE | \
     (uint32_t)TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_10_ENTRY_LINK | \
     (uint32_t)TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_10_RELATIVE_WINDOW | \
     (uint32_t)TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_10_CONTINUATION | \
     (uint32_t)TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_16_POINTER_TARGET | \
     (uint32_t)TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_16_DISTANCE | \
     (uint32_t)TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_TARGET_APPLY | \
     (uint32_t)TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_BA_LOW_BITS)

typedef struct TecmoGameplayCpuOpcodeWorkspaceEvidence {
    uint32_t contract_tag;
    uint32_t observed_mask;
} TecmoGameplayCpuOpcodeWorkspaceEvidence;

typedef struct TecmoGameplayCpuOpcodeWorkspaceAssessment {
    uint32_t contract_tag;
    uint8_t opcode;
    uint16_t handler_cpu;
    uint32_t required_mask;
    uint32_t missing_mask;
    /* Complete means a capture/harness supplied every required owner.  It
       never means LIVE has a faithful producer or caller timing. */
    bool capture_complete;
    bool deferred;
    bool live_producer_available;
} TecmoGameplayCpuOpcodeWorkspaceAssessment;

/* A strict input for the Bank06 $8D59-$8E21 helper followed by the immediate
 * $8E22-$8E4E sign restoration. The target pair/triple is a captured
 * $055B/$0566/$0571 workspace, not a bounded actor-position substitute. */
typedef struct TecmoGameplayCpuOpcode10WorkspaceInput {
    uint32_t contract_tag;
    /* CPU X register at Bank06 $8CD0, not a court X coordinate. */
    uint8_t actor_index;
    uint8_t special_actor_07df;
    uint8_t primary_actor_0308;
    uint8_t dynamic_link_06cb;
    uint8_t orientation_035a;
    uint16_t linked_target_x;
    uint8_t linked_target_depth;
    uint8_t timer_0798;
    uint8_t rate_index_075f;
    uint8_t sample_006a;
    uint8_t timer_0760;
} TecmoGameplayCpuOpcode10WorkspaceInput;

typedef struct TecmoGameplayCpuOpcode10WorkspaceResult {
    uint32_t contract_tag;
    uint8_t linked_actor;
    int16_t linked_relative_x;
    int16_t linked_relative_depth;
    uint8_t timer_0798_after;
    /* Canonical branches enter the shared shift sequence at $8DFB/$8E03/
       $8E0B/$8E13, yielding four/three/two/one shifts respectively. */
    uint8_t right_shift_count;
    bool timer_reloaded;
    bool timer_decremented;
} TecmoGameplayCpuOpcode10WorkspaceResult;

/* Narrow LIVE projection for the source branch at Bank06 $8CD0/$8D59.
 * Bank02 $BEE7-$BFD8 can only select a $04B0 bit-$10 actor for $07DF.
 * Therefore a bit-clear actor is proven to take its typed $06CB link without
 * retaining $07DF itself.  If that link is $0308, the timer-dependent primary
 * scaling branch remains unavailable; no timer value is fabricated. */
typedef struct TecmoGameplayCpuOpcode10LiveProjection {
    uint32_t contract_tag;
    bool branch_context_available;
    bool relative_workspace_available;
    uint8_t linked_actor;
    int16_t linked_relative_x;
    int16_t linked_relative_depth;
} TecmoGameplayCpuOpcode10LiveProjection;

/* A strict arithmetic-only capture of Bank05 $9054-$90AF.  It does not prove
 * that Bank05 was called before a particular Bank06 opcode-16 record. */
typedef struct TecmoGameplayCpuOpcode16WorkspaceInput {
    uint32_t contract_tag;
    uint8_t orientation_035a;
    TecmoGameplayCourtCoordinate actor_position;
} TecmoGameplayCpuOpcode16WorkspaceInput;

typedef struct TecmoGameplayCpuOpcode16WorkspaceResult {
    uint32_t contract_tag;
    uint16_t workspace_036e;
    uint16_t workspace_0370;
} TecmoGameplayCpuOpcode16WorkspaceResult;

/* Reports only source-owner completeness for the three focused handlers.
 * Unrelated opcodes return an all-zero, non-deferred assessment.  Malformed
 * evidence fails transactionally and leaves assessment_out unchanged. */
bool tecmo_gameplay_cpu_opcode_workspace_assess(
    uint8_t opcode,
    const TecmoGameplayCpuOpcodeWorkspaceEvidence *evidence,
    TecmoGameplayCpuOpcodeWorkspaceAssessment *assessment_out);

/* Exact Bank06 $8D59-$8E21/$8E22-$8E4E arithmetic harness.  It is not a
 * producer for PlayInput and its result must remain unavailable to LIVE until
 * an exact caller/timing contract exists. */
bool tecmo_gameplay_cpu_opcode10_workspace_harness(
    const TecmoGameplayCpuOpcode10WorkspaceInput *input,
    TecmoGameplayCpuOpcode10WorkspaceResult *result_out);

/* Resolves only the source-backed ordinary off-ball subset described above.
 * Valid but unowned branches return true with availability false. */
bool tecmo_gameplay_cpu_opcode10_live_projection(
    uint8_t actor,
    uint8_t primary_actor,
    uint8_t actor_selector_04b0,
    uint8_t dynamic_link_06cb,
    uint8_t orientation_035a,
    const TecmoGameplayCourtCoordinate actor_position[
        TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_ACTOR_COUNT],
    TecmoGameplayCpuOpcode10LiveProjection *projection_out);

/* Exact Bank05 $9054-$90AF absolute-distance arithmetic harness. */
bool tecmo_gameplay_cpu_opcode16_workspace_harness(
    const TecmoGameplayCpuOpcode16WorkspaceInput *input,
    TecmoGameplayCpuOpcode16WorkspaceResult *result_out);

/* Bank06 $92CA-$92CE consumes this gate only.  It is intentionally not a
 * lifecycle producer and cannot be replaced by frame_number & 3. */
uint8_t tecmo_gameplay_cpu_opcode_workspace_ba_low_bits(uint8_t flags_ba);

/* Standalone deterministic harness test; no asset pack, ROM, or scene is
 * loaded. The paired runner pins its provenance against the canonical ROM. */
bool tecmo_gameplay_cpu_opcode_workspace_self_test(char *message,
                                                   size_t message_size);

#endif
