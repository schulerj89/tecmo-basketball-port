#ifndef TECMO_GAMEPLAY_DEFENSE_INTERACTION_H
#define TECMO_GAMEPLAY_DEFENSE_INTERACTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_DEFENSE_INTERACTION_INPUT_TAG 0x49444654U
#define TECMO_GAMEPLAY_DEFENSE_INTERACTION_RESULT_TAG 0x52444654U
#define TECMO_GAMEPLAY_DEFENSE_POSSESSION_INPUT_TAG 0x49504454U
#define TECMO_GAMEPLAY_DEFENSE_POSSESSION_STATE_TAG 0x53504454U
#define TECMO_GAMEPLAY_DEFENSE_POSSESSION_RESULT_TAG 0x52504454U
#define TECMO_GAMEPLAY_DEFENSE_94C6_INPUT_TAG 0x49344354U
#define TECMO_GAMEPLAY_DEFENSE_94C6_RESULT_TAG 0x52344354U

typedef enum TecmoGameplayDefenseInteractionOutcome {
    TECMO_GAMEPLAY_DEFENSE_INTERACTION_REJECTED = 0,
    TECMO_GAMEPLAY_DEFENSE_INTERACTION_INTERRUPTED_94C6,
    TECMO_GAMEPLAY_DEFENSE_INTERACTION_POSSESSION_BA65,
    TECMO_GAMEPLAY_DEFENSE_INTERACTION_DEFLECTION_A0DD
} TecmoGameplayDefenseInteractionOutcome;

/* Exact scalar inputs retained by Bank05 `$9F2F-$9FE2`. The caller owns the
 * `$9E0A` component deltas and `$94C6` side effects; this helper deliberately
 * does not infer either from a native proximity policy. */
typedef struct TecmoGameplayDefenseInteractionInput {
    uint32_t contract_tag;
    uint16_t absolute_delta_x;
    uint16_t absolute_delta_depth;
    uint8_t raw_component_0378;
    uint8_t actor_direction_0463;
    bool delta_x_negative_0373;
    bool delta_depth_negative_0375;
    uint8_t timer_0760;
    uint8_t player_profile_0533;
    uint8_t gate_raw_006a;
    uint8_t post_9fa1_raw_006a;
    bool value_05a1_after_94c6;
    uint8_t selected_defender_direction;
    uint8_t primary_direction;
} TecmoGameplayDefenseInteractionInput;

typedef struct TecmoGameplayDefenseInteractionResult {
    uint32_t contract_tag;
    TecmoGameplayDefenseInteractionOutcome outcome;
    uint16_t metric_a184;
    uint8_t base_chance_a209;
    uint8_t threshold_9f69;
    uint8_t direction_property_bfa0;
    bool reached_9fa1;
    bool reached_94c6;
    bool reached_9fe2;
} TecmoGameplayDefenseInteractionResult;

/* Exact direct `$94C6-$95A9` selected-defender entry used by `$9FA4`.
 * External helper calls beginning at `$95AE` and their later actor-motion
 * writes are reported as a tail request; they are not guessed here. */
typedef struct TecmoGameplayDefense94c6Input {
    uint32_t contract_tag;
    uint8_t actor_bf;
    uint8_t side_be;
    uint8_t primary_0308;
    uint8_t defender_0309;
    uint8_t side_control_030c;
    uint8_t opposing_control_030c;
    uint8_t raw_0587;
    uint8_t raw_05a1;
    uint8_t route_0478;
    uint8_t clock_seconds_0358;
    uint8_t wait_0420;
    uint8_t actor_direction_0463;
    uint8_t primary_direction_0463;
    uint8_t raw_006a;
    uint16_t absolute_delta_x;
    uint16_t absolute_delta_depth;
    bool delta_x_negative_0373;
    bool delta_depth_negative_0375;
    uint8_t individual_fouls_before;
} TecmoGameplayDefense94c6Input;

typedef struct TecmoGameplayDefense94c6Result {
    uint32_t contract_tag;
    uint8_t raw_042a_after;
    uint8_t raw_038a_after;
    uint8_t raw_0435_after;
    uint8_t wait_0420_after;
    uint8_t direction_property_actor;
    uint8_t direction_property_primary;
    uint8_t random_threshold_9675;
    uint8_t saved_route_07e3;
    uint8_t route_0478_after;
    uint8_t target_action_046e;
    uint8_t defender_action_046e;
    uint8_t target_pose_low_0442;
    uint8_t target_pose_high_044d;
    uint8_t target_packed_action_0458;
    uint8_t target_velocity_low_049a;
    uint8_t target_velocity_high_04a5;
    uint8_t defender_direction_after_9cea;
    uint8_t defender_pose_low_0442;
    uint8_t defender_pose_high_044d;
    uint8_t defender_sprite_flags_0479;
    uint8_t defender_packed_action_0458;
    uint8_t individual_fouls_after;
    uint8_t raw_ba_or_mask;
    bool entry_writes_applied;
    bool wait_incremented;
    bool direction_overlap_admitted;
    bool random_gate_used;
    bool route_replaced_with_19;
    bool external_tail_requested;
    bool sets_05a1;
    bool sets_target_state_057c_08;
    bool individual_foul_incremented;
} TecmoGameplayDefense94c6Result;

/* Persistent scalar RAM owned by the admitted `$9FC3-$9FE1` possession
 * transaction.  The two-byte planes preserve the original modulo-256 team
 * counters at `$0752` and `$0756`; player counter requests remain explicit in
 * the result because `$C042` resolves a separate side/roster identity. */
typedef struct TecmoGameplayDefensePossessionState {
    uint32_t contract_tag;
    uint8_t raw_05a1;
    uint8_t raw_07de;
    uint8_t raw_0588;
    uint8_t raw_0587;
    uint8_t raw_0743;
    uint8_t raw_07e2;
    uint8_t raw_0752[2];
    uint8_t raw_0756[2];
} TecmoGameplayDefensePossessionState;

/* The before/after selectors bind the scalar wrapper to the already proven
 * `$B87C-$B98A` claimant transaction.  This entry is intentionally narrower
 * than arbitrary `$BA65`, `$BA8C`, or `$BAB6` entry-point calls: it represents
 * the exact ordinary defense possession caller whose `$9FF1` claimant is the
 * pre-settlement selected defender. */
typedef struct TecmoGameplayDefensePossessionInput {
    uint32_t contract_tag;
    uint8_t raw_0308_before;
    uint8_t raw_0309_before;
    uint8_t raw_030a_before;
    uint8_t raw_030b_before;
    uint8_t raw_0308_after;
    uint8_t raw_0309_after;
    uint8_t raw_030a_after;
    uint8_t raw_030b_after;
} TecmoGameplayDefensePossessionInput;

typedef struct TecmoGameplayDefensePossessionResult {
    uint32_t contract_tag;
    uint8_t claimant_9c;
    uint8_t counter6_actor;
    uint8_t counter8_actor;
    bool counter6_requested;
    bool counter8_requested;
    bool b87c_called;
    bool route_96b6_called;
    bool c711_action10_requested;
} TecmoGameplayDefensePossessionResult;

/* Pure `$9F2F-$9FA1`, followed by the caller-supplied `$94C6` result and the
 * exact `$9FAC-$9FE2` terminal selection. Rejection is byte-exact. */
bool tecmo_gameplay_defense_interaction_resolve(
    const TecmoGameplayDefenseInteractionInput *input,
    TecmoGameplayDefenseInteractionResult *result_out);

bool tecmo_gameplay_defense_94c6_direct_plan(
    const TecmoGameplayDefense94c6Input *input,
    TecmoGameplayDefense94c6Result *result_out);

/* Exact admitted `$9FC3-$9FE1 -> $9FF1 -> $BA65-$BAC0` scalar transaction.
 * On rejection, state_io and result_out are byte-identical. */
bool tecmo_gameplay_defense_possession_apply_9fc3(
    const TecmoGameplayDefensePossessionInput *input,
    TecmoGameplayDefensePossessionState *state_io,
    TecmoGameplayDefensePossessionResult *result_out);

bool tecmo_gameplay_defense_interaction_self_test(
    char *message, size_t message_size);

#endif
