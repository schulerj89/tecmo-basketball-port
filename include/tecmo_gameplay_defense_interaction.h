#ifndef TECMO_GAMEPLAY_DEFENSE_INTERACTION_H
#define TECMO_GAMEPLAY_DEFENSE_INTERACTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_DEFENSE_INTERACTION_INPUT_TAG 0x49444654U
#define TECMO_GAMEPLAY_DEFENSE_INTERACTION_RESULT_TAG 0x52444654U

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

/* Pure `$9F2F-$9FA1`, followed by the caller-supplied `$94C6` result and the
 * exact `$9FAC-$9FE2` terminal selection. Rejection is byte-exact. */
bool tecmo_gameplay_defense_interaction_resolve(
    const TecmoGameplayDefenseInteractionInput *input,
    TecmoGameplayDefenseInteractionResult *result_out);

bool tecmo_gameplay_defense_interaction_self_test(
    char *message, size_t message_size);

#endif
