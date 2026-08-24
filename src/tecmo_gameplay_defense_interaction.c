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

bool tecmo_gameplay_defense_possession_apply_9fc3(
    const TecmoGameplayDefensePossessionInput *input,
    TecmoGameplayDefensePossessionState *state_io,
    TecmoGameplayDefensePossessionResult *result_out)
{
    TecmoGameplayDefensePossessionState state;
    TecmoGameplayDefensePossessionResult result;
    uint8_t claimant;
    uint8_t counter_side;
    if (input == NULL || state_io == NULL || result_out == NULL ||
        input->contract_tag != TECMO_GAMEPLAY_DEFENSE_POSSESSION_INPUT_TAG ||
        state_io->contract_tag !=
            TECMO_GAMEPLAY_DEFENSE_POSSESSION_STATE_TAG ||
        input->raw_0308_before >= 10U || input->raw_0309_before >= 10U ||
        input->raw_0308_before == input->raw_0309_before ||
        input->raw_030a_before >= 2U || input->raw_030b_before >= 2U ||
        input->raw_030a_before == input->raw_030b_before ||
        input->raw_0308_after >= 10U || input->raw_0309_after >= 10U ||
        input->raw_030a_after >= 2U || input->raw_030b_after >= 2U ||
        input->raw_030a_after == input->raw_030b_after ||
        /* `$9FF1` writes the selected defender to `$9C`; `$B87C` must have
           promoted that exact claimant before this scalar state commits. */
        input->raw_0308_after != input->raw_0309_before ||
        state_io->raw_05a1 != 0U) {
        return false;
    }
    state = *state_io;
    memset(&result, 0, sizeof(result));
    result.contract_tag = TECMO_GAMEPLAY_DEFENSE_POSSESSION_RESULT_TAG;

    /* `$9FC3-$9FCB`: C042(X=6), then the defense-side team counter. */
    result.counter6_requested = true;
    result.counter6_actor = input->raw_0309_before;
    state.raw_0756[input->raw_030b_before] = (uint8_t)(
        state.raw_0756[input->raw_030b_before] + 1U);

    /* `$9FF1-$A005`: clear bit $10, set bit $40, clear `$0743`, and select
       the pre-settlement defender as claimant `$9C`. */
    state.raw_0588 = (uint8_t)((state.raw_0588 & 0xEFU) | 0x40U);
    state.raw_0743 = 0U;
    claimant = input->raw_0309_before;
    result.claimant_9c = claimant;

    /* `$BA65-$BA89`: AD01 preserves gameplay RAM. A clear `$07DE` admits the
       bit-$80 team counter; the claimant comparison selects offense/defense
       side exactly as the 6502 X reloads do. */
    if (state.raw_07de == 0U && (state.raw_0588 & 0x80U) != 0U) {
        counter_side = claimant == input->raw_0308_before
            ? input->raw_030a_before : input->raw_030b_before;
        state.raw_0752[counter_side] =
            (uint8_t)(state.raw_0752[counter_side] + 1U);
    }

    /* `$BA8C-$BA9F`: A is known zero from `$05A1`; the three pending bytes
       clear before the already completed B87C settlement and 96B6 route. */
    state.raw_07de = 0U;
    state.raw_0587 = 0U;
    state.raw_0743 = 0U;
    result.b87c_called = true;
    result.route_96b6_called = true;

    /* `$BAA0-$BABD`: clear bit $40. A surviving direct-carom bit $80 takes
       the exact `$76|$02` mask, action $10, and C042 counter slot 8. */
    state.raw_0588 &= 0xBFU;
    if ((state.raw_0588 & 0x80U) != 0U) {
        state.raw_0588 = (uint8_t)((state.raw_0588 & 0x76U) | 0x02U);
        result.c711_action10_requested = true;
        result.counter8_requested = true;
        result.counter8_actor = claimant;
    }

    /* `$9FD4-$9FE1` always follows the returning BA65 call. */
    state.raw_07e2 = 0x14U;
    state.raw_0588 &= 0xBFU;
    *state_io = state;
    *result_out = result;
    return true;
}

bool tecmo_gameplay_defense_interaction_self_test(
    char *message, size_t message_size)
{
    TecmoGameplayDefenseInteractionInput input;
    TecmoGameplayDefenseInteractionResult result;
    TecmoGameplayDefenseInteractionResult sentinel;
    TecmoGameplayDefensePossessionInput possession_input;
    TecmoGameplayDefensePossessionState possession_state;
    TecmoGameplayDefensePossessionState possession_sentinel;
    TecmoGameplayDefensePossessionResult possession_result;
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
    memset(&possession_input, 0, sizeof(possession_input));
    memset(&possession_state, 0, sizeof(possession_state));
    memset(&possession_result, 0, sizeof(possession_result));
    possession_input.contract_tag =
        TECMO_GAMEPLAY_DEFENSE_POSSESSION_INPUT_TAG;
    possession_input.raw_0308_before = 2U;
    possession_input.raw_0309_before = 7U;
    possession_input.raw_030a_before = 0U;
    possession_input.raw_030b_before = 1U;
    possession_input.raw_0308_after = 7U;
    possession_input.raw_0309_after = 2U;
    possession_input.raw_030a_after = 1U;
    possession_input.raw_030b_after = 0U;
    possession_state.contract_tag =
        TECMO_GAMEPLAY_DEFENSE_POSSESSION_STATE_TAG;
    possession_state.raw_0588 = 0x98U;
    possession_state.raw_0587 = 3U;
    possession_state.raw_0743 = 9U;
    possession_state.raw_0752[1U] = 0xFFU;
    possession_state.raw_0756[1U] = 0xFFU;
    if (!tecmo_gameplay_defense_possession_apply_9fc3(
            &possession_input, &possession_state, &possession_result) ||
        possession_state.raw_0588 != 0x02U ||
        possession_state.raw_0587 != 0U ||
        possession_state.raw_0743 != 0U ||
        possession_state.raw_07e2 != 0x14U ||
        possession_state.raw_0752[1U] != 0U ||
        possession_state.raw_0756[1U] != 0U ||
        !possession_result.counter6_requested ||
        possession_result.counter6_actor != 7U ||
        !possession_result.counter8_requested ||
        possession_result.counter8_actor != 7U ||
        !possession_result.b87c_called ||
        !possession_result.route_96b6_called ||
        !possession_result.c711_action10_requested) {
        (void)snprintf(message, message_size,
                       "TGDI $9FC3/$BA65 bit-80 transaction failed");
        return false;
    }
    possession_state.raw_0588 = 0x18U;
    possession_state.raw_0752[1U] = 5U;
    possession_state.raw_0756[1U] = 6U;
    if (!tecmo_gameplay_defense_possession_apply_9fc3(
            &possession_input, &possession_state, &possession_result) ||
        possession_state.raw_0588 != 0x08U ||
        possession_state.raw_0752[1U] != 5U ||
        possession_state.raw_0756[1U] != 7U ||
        possession_result.counter8_requested ||
        possession_result.c711_action10_requested) {
        (void)snprintf(message, message_size,
                       "TGDI $9FC3/$BA65 ordinary transaction failed");
        return false;
    }
    possession_sentinel = possession_state;
    possession_input.raw_0308_after = 4U;
    if (tecmo_gameplay_defense_possession_apply_9fc3(
            &possession_input, &possession_state, &possession_result) ||
        memcmp(&possession_state, &possession_sentinel,
               sizeof(possession_state)) != 0) {
        (void)snprintf(message, message_size,
                       "TGDI $9FC3 transactional rejection failed");
        return false;
    }
    (void)snprintf(message, message_size,
                   "TGDI Bank05 $9F2F-$9FE2 plus $9FC3/$BA65 transaction pass");
    return true;
}
