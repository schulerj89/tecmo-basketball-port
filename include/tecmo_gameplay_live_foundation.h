#ifndef TECMO_GAMEPLAY_LIVE_FOUNDATION_H
#define TECMO_GAMEPLAY_LIVE_FOUNDATION_H

#include "tecmo_gameplay_cpu_steering.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * LIVE owns the adapter state around the accepted TGAI play contract.  The
 * fixed `$06CB` links and startup seeds are source-backed; fixed_link_target
 * is a native projection separate from dynamic `$037F/$07DF`. Caller
 * workspaces are also explicitly classified below.
 */
#define TECMO_GAMEPLAY_LIVE_FOUNDATION_TAG 0x4C564631U
#define TECMO_GAMEPLAY_LIVE_CLAIMANT_SETTLEMENT_TAG 0x4C435331U
#define TECMO_GAMEPLAY_LIVE_OPCODE15_TRACE_TAG 0x4C4F3135U
#define TECMO_GAMEPLAY_LIVE_FOUNDATION_FORMATION_PINNED_LIMIT \
    TECMO_GAMEPLAY_CPU_STEERING_FORMATION_SOURCE_PINNED_COUNT

/*
 * Typed observation of the bounded Bank05 $B87C-$B98A claimant settlement.
 * The raw labels identify the source-owned state slots; the booleans record
 * only branches proven by this routine. They do not attach a rebound, steal,
 * foul, or statistic label to the claimant.
 */
typedef struct TecmoGameplayLiveClaimantSettlement {
    uint32_t contract_tag;
    uint8_t raw_0308_before;
    uint8_t raw_0309_before;
    uint8_t raw_030a_before;
    uint8_t raw_030b_before;
    uint8_t raw_0308_after;
    uint8_t raw_0309_after;
    uint8_t raw_030a_after;
    uint8_t raw_030b_after;
    bool candidate_replaced_primary;
    bool side_context_swapped;
    bool raw_04b0_bit10_toggled;
    bool automatic_defender_scan_ran;
    bool automatic_defender_match_found;
    /* $035A is saved to $035B and toggled only on the side-cross branch.
       LIVE has no faithful typed owner for either address, so this remains an
       observation rather than a native mutation. */
    bool raw_035a_save_and_toggle_observed;
} TecmoGameplayLiveClaimantSettlement;

/* Passive observation made when a canonical opcode-15 record reaches the
 * LIVE executor. The native scene deliberately does not own $0499, $007E,
 * $06D5/$06D6, $0479, $059E, or the raw $0442/$044D pointer pair, so unavailable
 * fields are represented as availability flags rather than substituted
 * values. This is diagnostic evidence only, never gameplay state. */
typedef struct TecmoGameplayLiveOpcode15Trace {
    uint32_t contract_tag;
    uint32_t missing_raw_mask;
    uint16_t command_record_offset;
    uint16_t actor_stream_before;
    uint16_t actor_stream_after;
    uint8_t opcode;
    uint8_t actor_x;
    uint8_t raw_0308_before;
    uint8_t raw_0308_after;
    uint8_t raw_0309_before;
    uint8_t raw_0309_after;
    uint8_t actor_state_before;
    uint8_t actor_state_after;
    TecmoGameplayCpuSteeringOpcode15Branch branch;
    bool observed;
    bool raw_0499_available;
    bool raw_04b0_available;
    bool raw_007e_available;
    bool raw_06d5_06d6_available;
    bool raw_0479_available;
    bool raw_0442_044d_available;
    bool raw_059e_available;
    bool raw_actor_lifecycle_available;
} TecmoGameplayLiveOpcode15Trace;

typedef struct TecmoGameplayLiveFoundation {
    uint32_t contract_tag;
    bool state_valid;
    bool initialized;
    bool formation_source_pinned;
    bool fixed_link_projection_active;
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
    uint8_t prior_selected_actor;
    uint8_t prior_defender_actor;
    bool selected_defender_handoff_active;
    /* Native typed controller assignment projection used by bounded C
       handoff policy. Runtime captures show that raw $030C/$030D cannot be
       classified as a zero-human/nonzero-automatic mirror, so these bytes
       deliberately carry no raw-RAM identity. */
    uint8_t control_mode[TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT];
    /* Explicit live mirrors of `$04B0&$10` and exact fixed `$06CB` pairing.
       The legacy field name predates proof that `$06CB` is not dynamic. */
    bool defender_eligible[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    uint8_t dynamic_link[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    /* Typed equivalents of $030A/$030B, $0E/$0F, $037F/$0380 and $04B0. */
    uint8_t offense_side;
    uint8_t defense_side;
    uint8_t selected_actor_by_side[TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT];
    uint8_t candidate_actor_by_side[TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT];
    uint8_t actor_selector_flags[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    uint16_t candidate_score_by_side[TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT];
    uint8_t candidate_sector_by_side[TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT];
    uint8_t actor_team[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    /* Last transactionally accepted live coordinates.  These are scene
       observations, not a replay of the Bank04 startup table. */
    TecmoGameplayCourtCoordinate actor_position[
        TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
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
    /* Exact `$86D2/$85EA` period-entry lifecycle. The last regulation period,
       OT count, and shared serial reject duplicate/out-of-order calls. */
    /* Binary `$0758` selector from the proven P1 tip winner/offense XOR 1
       projection. It is distinct from the P1 holder/offense side. */
    uint8_t period_entry_selector;
    uint8_t regulation_entry_seeded_period;
    /* Exact period-5 epoch and the binary selector observed for its last
       accepted banner. Zero count means no OT entry yet. */
    uint8_t overtime_entry_last_applied_count;
    uint8_t overtime_entry_last_selector_raw;
    bool regulation_entry_clamp_exemption_active;
    /* Exact actor carrying the typed `$0588&$08` staging exemption. Keeping
       this identity explicit lets a catch rebind primary before the old
       primary is clamped and the exemption is retired. */
    uint8_t regulation_entry_clamp_exempt_actor;
    uint32_t regulation_entry_seed_serial;
    uint16_t formation_start_offset[
        TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    uint16_t last_step_offset[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    uint8_t last_effect[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    uint8_t source_direction[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    /* Source handlers can defer for different missing caller-owned inputs.
       Keep the typed reason beside the existing per-actor defer bit so the
       developer diagnostics never relabel an unavailable RAM plane. */
    TecmoGameplayCpuSteeringDeferredReason deferred_reason[
        TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    bool source_target_valid[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    /* Opcode 13 stores raw 16-bit latch words in target_x/target_depth. This
       provenance accepts those bit patterns without advertising a semantic
       in-court movement target; source_direction carries the usable result. */
    bool source_raw_target_valid[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    /* Opcode 20 leaves the target storage bytes untouched while invalidating
       their prior meaning. This flag permits those exact inactive bits to
       survive validation; it is never movement-target provenance. */
    bool source_inactive_target_storage[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    bool source_direction_valid[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    bool deferred[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    bool last_shot_request;
    bool last_shot_deferred;
    bool last_shot_playback_supported;
    /* Last actor evaluated by the deterministic shot predicate, including a
       negative result; NO_ACTOR means no predicate was evaluated this tick. */
    uint8_t last_shot_actor;
    TecmoGameplayLiveOpcode15Trace opcode15_trace;
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
/* Atomic fixed `$E71B`/Bank05-role-reset/Bank06 `$85EA` regulation entry.
   P1 requires the explicit committed PRETIP winner selector `winner^1` and
   synchronized winner possession; P2-P4 require the stored cumulative
   selector. Rejection leaves the foundation byte-identical, so no replayable
   half-entry state is public. */
bool tecmo_gameplay_live_foundation_regulation_entry_apply(
    const TecmoGameplayCpuSteeringAssets *assets,
    uint8_t period,
    uint8_t target_offense_side,
    uint8_t period_entry_selector,
    bool ordinary_ba_low2_clear,
    TecmoGameplayLiveFoundation *foundation_io);
/* Atomic period-5 banner entry. `overtime_count` is a 1-based epoch key;
   duplicate, decreasing, or skipped epochs reject byte-identically. */
bool tecmo_gameplay_live_foundation_overtime_entry_apply(
    const TecmoGameplayCpuSteeringAssets *assets,
    uint8_t overtime_count,
    bool ordinary_ba_low2_clear,
    TecmoGameplayLiveFoundation *foundation_io);
bool tecmo_gameplay_live_foundation_refresh_formation(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayCourtCoordinate actor_position[
        TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT],
    TecmoGameplayLiveFoundation *foundation_io);

/* Starts from the current typed $0308-equivalent ball-holder coordinate. */
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

/* Bounded typed made-score/restart projection of the accepted Bank05
 * $8FB9-$9042 writes followed by
 * the leading Bank06 $9621 aggregation reset. This is not the miss-claimant
 * $B87C transaction, does not infer the preceding $8FAD raw $05A1/$BA
 * admission gates, and does not choose an inbound receiver. The accepted
 * transition swaps the selected offense/defense roles, clears both selected
 * actor lifecycles, and toggles the exact $04B0 bit-$10 mirrors. */
bool tecmo_gameplay_live_foundation_score_restart_transition(
    const TecmoGameplayCpuSteeringAssets *assets,
    uint8_t resulting_possession,
    TecmoGameplayLiveFoundation *foundation_io);

/* Playability-only normalization for a non-source miss fallback after the
 * scene has already synchronized a promoted automatic holder. No claimant,
 * rebound, or Bank05 $B87C identity is implied. */
bool tecmo_gameplay_live_foundation_normalize_automatic_primary(
    const TecmoGameplayCpuSteeringAssets *assets,
    uint8_t selected_actor,
    TecmoGameplayLiveFoundation *foundation_io);

/* Exact bounded Bank05 $B24F-$B32B pass-selection handoff. On failure the
 * caller-owned foundation is unchanged. The raw 6502 no-match underflow is
 * rejected instead of fabricating an actor outside slots 0..9. */
bool tecmo_gameplay_live_foundation_pass_handoff(
    const TecmoGameplayCpuSteeringAssets *assets,
    uint8_t new_selected_actor,
    TecmoGameplayLiveFoundation *foundation_io);

/* Bank05 $B074-$B0FD launch-time identity lock. This swaps the typed
 * $000E/$037F-shaped side roles while retaining the $0308-shaped primary and
 * ball holder until Bank05 $B24F performs the catch. */
bool tecmo_gameplay_live_foundation_pass_launch_lock(
    const TecmoGameplayCpuSteeringAssets *assets,
    uint8_t receiver_actor,
    TecmoGameplayLiveFoundation *foundation_io);

/* Bounded Bank05 $B87C-$B98A claimant settlement, callable only after a
 * caller has already established a selected claimant and its resulting scene
 * possession. It is deliberately distinct from generic possession handoff:
 * makes, period restarts, tip-offs, steals, fouls, and unproven recovery paths
 * must not call it. On failure the caller-owned foundation and result output
 * remain unchanged. */
bool tecmo_gameplay_live_foundation_claimant_settlement(
    const TecmoGameplayCpuSteeringAssets *assets,
    uint8_t selected_claimant,
    uint8_t resulting_possession,
    TecmoGameplayLiveFoundation *foundation_io,
    TecmoGameplayLiveClaimantSettlement *result_out);

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
