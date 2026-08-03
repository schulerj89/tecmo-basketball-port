#ifndef TECMO_GAMEPLAY_LIVE_FOUNDATION_H
#define TECMO_GAMEPLAY_LIVE_FOUNDATION_H

#include "tecmo_gameplay_cpu_steering.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * LIVE owns the adapter state around the accepted TGAI play contract.  The
 * fixed links and startup seeds are source-backed; native_matchup_actor and
 * the caller workspaces are explicitly classified below and are not claims
 * about the incomplete ROM dynamic candidate vector.
 */
#define TECMO_GAMEPLAY_LIVE_FOUNDATION_TAG 0x4C564631U
#define TECMO_GAMEPLAY_LIVE_FOUNDATION_FORMATION_PINNED_LIMIT \
    TECMO_GAMEPLAY_CPU_STEERING_FORMATION_SOURCE_PINNED_COUNT

typedef struct TecmoGameplayLiveFoundation {
    uint32_t contract_tag;
    bool state_valid;
    bool initialized;
    bool formation_source_pinned;
    bool native_matchup_inferred;
    bool workspace_native_approximation;
    bool shot_request_native_approximation;
    bool first_sync_pending;
    uint8_t formation_index;
    uint8_t orientation;
    uint8_t primary_actor;
    uint8_t defender_actor;
    uint8_t static_primary_seed;
    uint8_t static_defender_seed;
    uint8_t last_possession;
    uint8_t last_ball_holder;
    uint8_t actor_team[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    uint8_t controller_team[
        TECMO_GAMEPLAY_CPU_STEERING_CONTROLLER_SLOT_COUNT];
    uint8_t last_controlled_actor[
        TECMO_GAMEPLAY_CPU_STEERING_CONTROLLER_SLOT_COUNT];
    uint32_t initialization_serial;
    /* Adapter observation counters wrap modulo 2^32; they are not gameplay
       time limits. The accepted play_state.step_serial remains uint16_t and
       follows the CPU contract's natural wrap. */
    uint32_t sync_serial;
    uint32_t tick_serial;
    uint16_t formation_start_offset[
        TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    uint16_t last_step_offset[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    uint8_t last_effect[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    uint8_t source_direction[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    bool source_target_valid[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    bool source_direction_valid[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    bool deferred[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    bool last_shot_request;
    bool last_shot_deferred;
    bool last_shot_playback_supported;
    /* Last actor evaluated by the deterministic shot predicate, including a
       negative result; NO_ACTOR means no predicate was evaluated this tick. */
    uint8_t last_shot_actor;
    TecmoGameplayCpuSteeringPlayState play_state;
} TecmoGameplayLiveFoundation;

void tecmo_gameplay_live_foundation_init(
    TecmoGameplayLiveFoundation *foundation);

bool tecmo_gameplay_live_foundation_valid(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayLiveFoundation *foundation);

/* Bank06 formation indexing is depth row * 12 + source X bucket. */
bool tecmo_gameplay_live_foundation_formation_index_for_coordinate(
    const TecmoGameplayCourtCoordinate *coordinate,
    uint8_t *formation_index_out);

/* Starts from the exact selected primary coordinate (actor slot 4). */
bool tecmo_gameplay_live_foundation_initialize(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayCourtCoordinate
        actor_position[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT],
    uint8_t orientation,
    uint8_t possession,
    uint8_t ball_holder,
    const uint8_t actor_team[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT],
    const uint8_t controller_team[
        TECMO_GAMEPLAY_CPU_STEERING_CONTROLLER_SLOT_COUNT],
    const uint8_t controlled_actor[
        TECMO_GAMEPLAY_CPU_STEERING_CONTROLLER_SLOT_COUNT],
    TecmoGameplayLiveFoundation *foundation_out);

/* Synchronizes holder/possession/controller observations transactionally. */
bool tecmo_gameplay_live_foundation_synchronize(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayCourtCoordinate
        actor_position[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT],
    uint8_t orientation,
    uint8_t possession,
    uint8_t ball_holder,
    const uint8_t actor_team[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT],
    const uint8_t controller_team[
        TECMO_GAMEPLAY_CPU_STEERING_CONTROLLER_SLOT_COUNT],
    const uint8_t controlled_actor[
        TECMO_GAMEPLAY_CPU_STEERING_CONTROLLER_SLOT_COUNT],
    TecmoGameplayLiveFoundation *foundation_io);

/* One accepted source play step. Caller commits foundation_io only as part
 * of its larger scene candidate transaction. */
bool tecmo_gameplay_live_foundation_play_step(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayCpuSteeringPlayInput *input,
    TecmoGameplayLiveFoundation *foundation_io,
    TecmoGameplayCpuSteeringPlayResult *result_out);

/* Thin classified wrapper around the exact source shot predicate. */
bool tecmo_gameplay_live_foundation_shot_request(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayCpuSteeringShotInput *input,
    uint8_t actor,
    TecmoGameplayLiveFoundation *foundation_io,
    TecmoGameplayCpuSteeringShotResult *result_out);

#endif
