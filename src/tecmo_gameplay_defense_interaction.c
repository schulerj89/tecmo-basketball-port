#include "tecmo_gameplay_defense_interaction.h"

#include <stdio.h>
#include <string.h>

static const uint8_t direction_property_bfa0[8] = {
    0x01U, 0x02U, 0x04U, 0x05U, 0x06U, 0x08U, 0x09U, 0x0AU
};

bool tecmo_gameplay_defense_interaction_resolve(
    const TecmoGameplayDefenseInteractionInput *input,
    TecmoGameplayDefenseInteractionResult *result_out)
{
    TecmoGameplayDefenseInteractionResult result;
    uint16_t larger;
    uint16_t smaller;
    uint8_t bonus;
    uint8_t horizontal_reject_mask;
    uint8_t depth_reject_mask;
    if (input == NULL || result_out == NULL ||
        input->contract_tag != TECMO_GAMEPLAY_DEFENSE_INTERACTION_INPUT_TAG ||
        input->actor_direction_0463 >= 8U ||
        input->selected_defender_direction >= 8U ||
        input->primary_direction >= 8U) {
        return false;
    }
    memset(&result, 0, sizeof(result));
    result.contract_tag = TECMO_GAMEPLAY_DEFENSE_INTERACTION_RESULT_TAG;
    result.outcome = TECMO_GAMEPLAY_DEFENSE_INTERACTION_REJECTED;
    larger = input->absolute_delta_x;
    smaller = input->absolute_delta_depth;
    if (larger < smaller) {
        uint16_t swap = larger;
        larger = smaller;
        smaller = swap;
    }
    result.metric_a184 = (uint16_t)(larger + (smaller >> 1U));
    if (result.metric_a184 > 0xFFU || result.metric_a184 >= 0x18U ||
        input->raw_component_0378 >= 0x0AU) {
        *result_out = result;
        return true;
    }
    /* `$A209` intentionally wraps the byte addition before LSR. */
    result.base_chance_a209 = (uint8_t)(
        (uint8_t)(input->timer_0760 + input->player_profile_0533) >> 1U);
    bonus = (uint8_t)(result.base_chance_a209 >> 1U);
    if (bonus < result.metric_a184) bonus = 0U;
    else bonus = (uint8_t)(bonus - (uint8_t)result.metric_a184);
    result.threshold_9f69 = (uint8_t)(result.base_chance_a209 + bonus);
    if (result.threshold_9f69 < input->gate_raw_006a) {
        *result_out = result;
        return true;
    }
    result.direction_property_bfa0 =
        direction_property_bfa0[input->actor_direction_0463];
    horizontal_reject_mask = input->delta_x_negative_0373 ? 0x02U : 0x01U;
    depth_reject_mask = input->delta_depth_negative_0375 ? 0x08U : 0x04U;
    if (((result.direction_property_bfa0 & 0x03U) != 0U &&
         (result.direction_property_bfa0 & horizontal_reject_mask) != 0U) ||
        ((result.direction_property_bfa0 & 0x0CU) != 0U &&
         (result.direction_property_bfa0 & depth_reject_mask) != 0U)) {
        *result_out = result;
        return true;
    }
    result.reached_9fa1 = true;
    result.reached_94c6 = true;
    if (input->value_05a1_after_94c6) {
        result.outcome =
            TECMO_GAMEPLAY_DEFENSE_INTERACTION_INTERRUPTED_94C6;
    } else if (input->selected_defender_direction ==
                   input->primary_direction ||
               input->post_9fa1_raw_006a >= 0x26U) {
        result.outcome =
            TECMO_GAMEPLAY_DEFENSE_INTERACTION_DEFLECTION_A0DD;
        result.reached_9fe2 = true;
    } else {
        result.outcome =
            TECMO_GAMEPLAY_DEFENSE_INTERACTION_POSSESSION_BA65;
    }
    *result_out = result;
    return true;
}

bool tecmo_gameplay_defense_interaction_self_test(
    char *message, size_t message_size)
{
    TecmoGameplayDefenseInteractionInput input;
    TecmoGameplayDefenseInteractionResult result;
    TecmoGameplayDefenseInteractionResult sentinel;
    if (message == NULL || message_size == 0U) return false;
    memset(&input, 0, sizeof(input));
    memset(&result, 0, sizeof(result));
    input.contract_tag = TECMO_GAMEPLAY_DEFENSE_INTERACTION_INPUT_TAG;
    input.absolute_delta_x = 4U;
    input.absolute_delta_depth = 7U;
    input.raw_component_0378 = 7U;
    input.actor_direction_0463 = 0U;
    input.delta_x_negative_0373 = true;
    input.delta_depth_negative_0375 = false;
    input.player_profile_0533 = 0x40U;
    input.gate_raw_006a = 0x10U;
    input.post_9fa1_raw_006a = 0x31U;
    input.selected_defender_direction = 0U;
    input.primary_direction = 4U;
    if (!tecmo_gameplay_defense_interaction_resolve(&input, &result) ||
        result.contract_tag != TECMO_GAMEPLAY_DEFENSE_INTERACTION_RESULT_TAG ||
        result.metric_a184 != 9U || result.base_chance_a209 != 0x20U ||
        result.threshold_9f69 != 0x27U || !result.reached_9fa1 ||
        !result.reached_94c6 || !result.reached_9fe2 ||
        result.outcome !=
            TECMO_GAMEPLAY_DEFENSE_INTERACTION_DEFLECTION_A0DD) {
        (void)snprintf(message, message_size,
                       "TGDI exact deflection vector failed");
        return false;
    }
    input.post_9fa1_raw_006a = 0x25U;
    if (!tecmo_gameplay_defense_interaction_resolve(&input, &result) ||
        result.outcome != TECMO_GAMEPLAY_DEFENSE_INTERACTION_POSSESSION_BA65 ||
        result.reached_9fe2) {
        (void)snprintf(message, message_size,
                       "TGDI possession branch failed");
        return false;
    }
    input.value_05a1_after_94c6 = true;
    if (!tecmo_gameplay_defense_interaction_resolve(&input, &result) ||
        result.outcome !=
            TECMO_GAMEPLAY_DEFENSE_INTERACTION_INTERRUPTED_94C6) {
        (void)snprintf(message, message_size,
                       "TGDI 94C6 interruption failed");
        return false;
    }
    input.value_05a1_after_94c6 = false;
    input.actor_direction_0463 = 1U;
    sentinel = result;
    if (!tecmo_gameplay_defense_interaction_resolve(&input, &result) ||
        result.outcome != TECMO_GAMEPLAY_DEFENSE_INTERACTION_REJECTED ||
        result.reached_9fa1) {
        (void)snprintf(message, message_size,
                       "TGDI facing rejection failed");
        return false;
    }
    input.contract_tag = 0U;
    result = sentinel;
    if (tecmo_gameplay_defense_interaction_resolve(&input, &result) ||
        memcmp(&result, &sentinel, sizeof(result)) != 0) {
        (void)snprintf(message, message_size,
                       "TGDI transactional rejection failed");
        return false;
    }
    (void)snprintf(message, message_size,
                   "TGDI Bank05 $9F2F-$9FE2 exact scalar gates pass");
    return true;
}
