#include "tecmo_gameplay_defense_interaction.h"

#include <stdio.h>
#include <string.h>

static const uint8_t direction_property_bfa0[8] = {
    0x01U, 0x02U, 0x04U, 0x05U, 0x06U, 0x08U, 0x09U, 0x0AU
};
static const uint8_t contact_direction_bf6c[8] = {
    0x07U, 0x04U, 0x06U, 0x03U, 0x01U, 0x00U, 0x05U, 0x02U
};
static const uint8_t contact_target_pose_96a6[8] = {
    0xB6U, 0x76U, 0x96U, 0xA6U, 0x86U, 0x56U, 0xC6U, 0x66U
};
static const uint8_t contact_defender_pose_9b3f[8] = {
    0xEEU, 0xDEU, 0xE6U, 0xEAU, 0xE2U, 0xD6U, 0xF2U, 0xDAU
};

static uint8_t contact_direction_9cea(
    uint16_t absolute_x, uint16_t absolute_depth,
    bool x_negative, bool depth_negative)
{
    uint8_t octant;
    if (absolute_x >= absolute_depth) {
        if (absolute_x >= (uint16_t)(absolute_depth << 2U)) {
            octant = (uint8_t)(4U + (x_negative ? 1U : 0U));
        } else {
            octant = (uint8_t)((x_negative ? 2U : 0U) +
                               (depth_negative ? 1U : 0U));
        }
    } else if (absolute_depth >= (uint16_t)(absolute_x << 2U)) {
        octant = (uint8_t)(6U + (depth_negative ? 1U : 0U));
    } else {
        octant = (uint8_t)((x_negative ? 2U : 0U) +
                           (depth_negative ? 1U : 0U));
    }
    return contact_direction_bf6c[octant];
}

bool tecmo_gameplay_defense_94c6_direct_plan(
    const TecmoGameplayDefense94c6Input *input,
    TecmoGameplayDefense94c6Result *result_out)
{
    TecmoGameplayDefense94c6Result result;
    uint8_t wait_after;
    if (input == NULL || result_out == NULL ||
        input->contract_tag != TECMO_GAMEPLAY_DEFENSE_94C6_INPUT_TAG ||
        input->actor_bf >= 10U || input->primary_0308 >= 10U ||
        input->defender_0309 >= 10U ||
        input->primary_0308 == input->defender_0309 ||
        input->actor_bf != input->defender_0309 || input->side_be >= 2U ||
        input->side_control_030c > 1U ||
        input->opposing_control_030c > 1U ||
        input->actor_direction_0463 >= 8U ||
        input->primary_direction_0463 >= 8U ||
        input->individual_fouls_before > 6U) {
        return false;
    }
    memset(&result, 0, sizeof(result));
    result.contract_tag = TECMO_GAMEPLAY_DEFENSE_94C6_RESULT_TAG;
    result.wait_0420_after = input->wait_0420;
    result.route_0478_after = input->route_0478;

    /* `$94C6-$94CF`: this direct entry is inert for automatic-side control. */
    if (input->side_control_030c != 0U) {
        *result_out = result;
        return true;
    }

    /* `$94D0-$94DD` always precedes every later early return. */
    result.entry_writes_applied = true;
    result.raw_042a_after = 0x04U;
    result.raw_038a_after = 0x2FU;
    result.raw_0435_after = input->primary_0308;

    /* The admitted production caller supplies BF==$0309, so `$94E5-$94F6`
       bypasses only the `$0478==5` alternate-actor return. */
    if (input->raw_0587 != 0U || input->raw_05a1 != 0U ||
        input->clock_seconds_0358 < 2U) {
        *result_out = result;
        return true;
    }

    /* `$9500-$9521`: the selected defender is distinct from `$0308`, so the
       `$048F` primary-only branch is unreachable and INC wraps as a byte. */
    wait_after = (uint8_t)(input->wait_0420 + 1U);
    result.wait_0420_after = wait_after;
    result.wait_incremented = true;
    result.direction_property_actor =
        direction_property_bfa0[input->actor_direction_0463];
    result.direction_property_primary =
        direction_property_bfa0[input->primary_direction_0463];
    if (wait_after >= 0x24U &&
        (result.direction_property_actor &
         result.direction_property_primary) != 0U) {
        result.direction_overlap_admitted = true;
    } else {
        /* Direct `$94C6` wrote `$038A[BE]=$2F`; four LSRs select
           `$9675[2]=$0E`. `$956D` rejects only when table < `$006A`. */
        result.random_gate_used = true;
        result.random_threshold_9675 = 0x0EU;
        if (input->raw_006a > result.random_threshold_9675) {
            *result_out = result;
            return true;
        }
    }

    /* `$9571-$95A9`: `$0435[BF]` is the primary written above. */
    result.wait_0420_after = 0U;
    result.saved_route_07e3 = input->route_0478;
    result.route_0478_after = input->route_0478;
    if (input->route_0478 != 0x05U && input->route_0478 != 0x07U &&
        input->route_0478 != 0x08U) {
        result.route_0478_after = 0x19U;
        result.route_replaced_with_19 = true;
    }
    result.external_tail_requested = true;
    result.sets_05a1 = true;
    result.target_action_046e = 0x1FU;
    result.defender_action_046e = 0x14U;
    result.raw_ba_or_mask = 0x04U;
    result.target_pose_low_0442 =
        contact_target_pose_96a6[input->actor_direction_0463];
    result.target_pose_high_044d = 0x08U;
    result.target_packed_action_0458 = 0x30U;
    result.target_velocity_low_049a = 0xF0U;
    result.target_velocity_high_04a5 = 0x02U;
    result.defender_direction_after_9cea = contact_direction_9cea(
        input->absolute_delta_x, input->absolute_delta_depth,
        input->delta_x_negative_0373, input->delta_depth_negative_0375);
    result.defender_pose_low_0442 = contact_defender_pose_9b3f[
        result.defender_direction_after_9cea];
    result.defender_pose_high_044d = 0x08U;
    result.defender_sprite_flags_0479 = 0x81U;
    result.defender_packed_action_0458 = 0U;
    result.individual_fouls_after = input->individual_fouls_before;
    if (result.individual_fouls_after < 6U) {
        ++result.individual_fouls_after;
        result.individual_foul_incremented = true;
    }
    result.sets_target_state_057c_08 = input->opposing_control_030c != 0U;
    *result_out = result;
    return true;
}

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
    TecmoGameplayDefense94c6Input contact_input;
    TecmoGameplayDefense94c6Result contact_result;
    TecmoGameplayDefense94c6Result contact_sentinel;
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
    memset(&contact_input, 0, sizeof(contact_input));
    memset(&contact_result, 0, sizeof(contact_result));
    contact_input.contract_tag = TECMO_GAMEPLAY_DEFENSE_94C6_INPUT_TAG;
    contact_input.actor_bf = 7U;
    contact_input.side_be = 1U;
    contact_input.primary_0308 = 2U;
    contact_input.defender_0309 = 7U;
    contact_input.clock_seconds_0358 = 30U;
    contact_input.wait_0420 = 0x22U;
    contact_input.actor_direction_0463 = 1U;
    contact_input.primary_direction_0463 = 4U;
    contact_input.raw_006a = 0x0EU;
    contact_input.opposing_control_030c = 1U;
    if (!tecmo_gameplay_defense_94c6_direct_plan(
            &contact_input, &contact_result) ||
        !contact_result.entry_writes_applied ||
        contact_result.raw_042a_after != 0x04U ||
        contact_result.raw_038a_after != 0x2FU ||
        contact_result.raw_0435_after != 2U ||
        !contact_result.wait_incremented ||
        !contact_result.random_gate_used ||
        contact_result.random_threshold_9675 != 0x0EU ||
        !contact_result.external_tail_requested ||
        !contact_result.sets_05a1 ||
        contact_result.wait_0420_after != 0U ||
        contact_result.route_0478_after != 0x19U ||
        contact_result.target_action_046e != 0x1FU ||
        contact_result.defender_action_046e != 0x14U ||
        contact_result.target_pose_low_0442 != 0x76U ||
        contact_result.target_pose_high_044d != 0x08U ||
        contact_result.target_packed_action_0458 != 0x30U ||
        contact_result.target_velocity_low_049a != 0xF0U ||
        contact_result.target_velocity_high_04a5 != 0x02U ||
        contact_result.defender_direction_after_9cea != 0x01U ||
        contact_result.defender_pose_low_0442 != 0xDEU ||
        contact_result.defender_pose_high_044d != 0x08U ||
        contact_result.defender_sprite_flags_0479 != 0x81U ||
        contact_result.defender_packed_action_0458 != 0U ||
        contact_result.individual_fouls_after != 1U ||
        !contact_result.individual_foul_incremented ||
        contact_result.raw_ba_or_mask != 0x04U ||
        !contact_result.sets_target_state_057c_08) {
        (void)snprintf(message, message_size,
                       "TGDI $94C6 direct random admission failed");
        return false;
    }
    contact_input.raw_006a = 0x0FU;
    if (!tecmo_gameplay_defense_94c6_direct_plan(
            &contact_input, &contact_result) ||
        contact_result.external_tail_requested ||
        contact_result.wait_0420_after != 0x23U) {
        (void)snprintf(message, message_size,
                       "TGDI $94C6 direct random rejection failed");
        return false;
    }
    contact_input.wait_0420 = 0x23U;
    contact_input.raw_006a = 0xFFU;
    contact_input.actor_direction_0463 = 3U;
    contact_input.primary_direction_0463 = 0U;
    contact_input.route_0478 = 0x07U;
    if (!tecmo_gameplay_defense_94c6_direct_plan(
            &contact_input, &contact_result) ||
        !contact_result.direction_overlap_admitted ||
        contact_result.random_gate_used ||
        !contact_result.external_tail_requested ||
        contact_result.route_0478_after != 0x07U ||
        contact_result.route_replaced_with_19) {
        (void)snprintf(message, message_size,
                       "TGDI $94C6 direction-overlap admission failed");
        return false;
    }
    contact_input.individual_fouls_before = 6U;
    if (!tecmo_gameplay_defense_94c6_direct_plan(
            &contact_input, &contact_result) ||
        contact_result.individual_fouls_after != 6U ||
        contact_result.individual_foul_incremented) {
        (void)snprintf(message, message_size,
                       "TGDI $94C6 capped individual foul failed");
        return false;
    }
    contact_input.side_control_030c = 1U;
    if (!tecmo_gameplay_defense_94c6_direct_plan(
            &contact_input, &contact_result) ||
        contact_result.entry_writes_applied ||
        contact_result.external_tail_requested) {
        (void)snprintf(message, message_size,
                       "TGDI $94C6 automatic-side return failed");
        return false;
    }
    contact_sentinel = contact_result;
    contact_input.contract_tag = 0U;
    if (tecmo_gameplay_defense_94c6_direct_plan(
            &contact_input, &contact_result) ||
        memcmp(&contact_result, &contact_sentinel,
               sizeof(contact_result)) != 0) {
        (void)snprintf(message, message_size,
                       "TGDI $94C6 transactional rejection failed");
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
                   "TGDI Bank05 $9F2F-$9FE2, direct $94C6-$95A9, and $9FC3/$BA65 pass");
    return true;
}
