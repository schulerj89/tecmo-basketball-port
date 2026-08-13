#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_gameplay_cpu_route_profile.h"

#include <stdio.h>
#include <string.h>

static bool source_contract_valid(
    const TecmoGameplayMovementAssets *movement_assets,
    const TecmoGameplayReboundAuditAssets *c045_assets)
{
    const TecmoGameplayMovementSourceSpan *profile;
    const TecmoGameplayReboundAuditSourceSpan *c045;
    if (movement_assets == NULL || !movement_assets->available ||
        c045_assets == NULL || !c045_assets->available) {
        return false;
    }
    profile = tecmo_gameplay_movement_find_source(
        movement_assets, TECMO_GAMEPLAY_MOVEMENT_SOURCE_PROFILE_AND_SPEED);
    c045 = tecmo_gameplay_rebound_audit_find_source(
        c045_assets, TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_COUNTER_PLANE);
    return profile != NULL && profile->bank == 2U && !profile->fixed_bank &&
           profile->cpu_start == 0xA89EU && profile->cpu_end == 0xA90DU &&
           profile->byte_count == 112U && profile->fingerprint == 0x0BD2CB61U &&
           c045 != NULL && c045->bank == 7U && c045->fixed_bank &&
           c045->cpu_start == 0xCC00U && c045->cpu_end == 0xCC2FU &&
           c045->byte_count == 48U &&
           c045->fingerprint_fnv1a32 == 0x93ACD23FU;
}

bool tecmo_gameplay_cpu_route_profile_project(
    const TecmoGameplayMovementAssets *movement_assets,
    const TecmoGameplayReboundAuditAssets *c045_assets,
    const TecmoGameplayCpuRouteProfileInput *input,
    TecmoGameplayCpuRouteProfileResult *result_out)
{
    TecmoGameplayCpuRouteProfileResult result;
    uint8_t expected_team;
    uint8_t movement;
    if (!source_contract_valid(movement_assets, c045_assets) ||
        input == NULL || result_out == NULL ||
        input->contract_tag != TECMO_GAMEPLAY_CPU_ROUTE_PROFILE_INPUT_TAG ||
        input->actor >= 10U ||
        input->team >= 2U ||
        input->roster_index >= TECMO_GAMEPLAY_CPU_ROUTE_PROFILE_ROSTER_COUNT ||
        input->speed_value >= TECMO_GAMEPLAY_MOVEMENT_SPEED_COUNT ||
        input->difficulty >= TECMO_GAMEPLAY_MOVEMENT_SPEED_COUNT ||
        input->condition_7c48 > 100U ||
        !input->extra_adjust_admission_available) {
        return false;
    }
    expected_team = input->actor < 5U
                        ? 0U : 1U;
    if (input->team != expected_team) return false;

    memset(&result, 0, sizeof(result));
    result.contract_tag = TECMO_GAMEPLAY_CPU_ROUTE_PROFILE_RESULT_TAG;
    result.actor = input->actor;
    /* Fixed $C045->$CC00-$CC11: $05A9[$BF] + {0,12}[$BE]. */
    result.player_row_7c48 = (uint8_t)(
        input->roster_index + input->team *
            TECMO_GAMEPLAY_CPU_ROUTE_PROFILE_ROSTER_COUNT);
    result.condition_7c48 = input->condition_7c48;
    result.mode_adjustment =
        movement_assets->speed_adjustment[input->speed_value];
    movement = (uint8_t)(input->profile_movement_byte +
                         (uint8_t)result.mode_adjustment);
    if (input->extra_adjust_admitted) {
        result.extra_adjustment =
            movement_assets->route_extra_adjustment[input->difficulty];
        movement = (uint8_t)(movement +
                             (uint8_t)result.extra_adjustment);
        result.extra_adjustment_applied = true;
    }
    result.movement_value_06e7 = movement;
    *result_out = result;
    return true;
}

static bool result_unchanged_on_failure(
    const TecmoGameplayMovementAssets *movement,
    const TecmoGameplayReboundAuditAssets *c045,
    const TecmoGameplayCpuRouteProfileInput *input)
{
    TecmoGameplayCpuRouteProfileResult before;
    TecmoGameplayCpuRouteProfileResult result;
    memset(&before, 0xA5, sizeof(before));
    result = before;
    return !tecmo_gameplay_cpu_route_profile_project(
               movement, c045, input, &result) &&
           memcmp(&result, &before, sizeof(result)) == 0;
}

bool tecmo_gameplay_cpu_route_profile_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size)
{
    static const int8_t mode_delta[3] = {5, -1, -6};
    static const int8_t extra_delta[3] = {-3, -2, -1};
    TecmoGameplayMovementAssets movement;
    TecmoGameplayReboundAuditAssets c045;
    TecmoGameplayCpuRouteProfileInput input;
    TecmoGameplayCpuRouteProfileResult result;
    TecmoGameplayMovementAssets unavailable_movement;
    TecmoGameplayReboundAuditAssets unavailable_c045;
    size_t index;
    tecmo_gameplay_movement_assets_init(&movement);
    tecmo_gameplay_rebound_audit_init(&c045);
    if (!tecmo_gameplay_movement_assets_load(&movement, asset_pack_path) ||
        !tecmo_gameplay_rebound_audit_load(&c045, asset_pack_path)) {
        (void)snprintf(message, message_size,
                       "CPU route profile strict dependencies unavailable");
        goto failure;
    }
    memset(&input, 0, sizeof(input));
    input.contract_tag = TECMO_GAMEPLAY_CPU_ROUTE_PROFILE_INPUT_TAG;
    input.actor = 3U;
    input.team = 0U;
    input.roster_index = 3U;
    input.profile_movement_byte = 0x17U;
    input.speed_value = 1U;
    input.condition_7c48 = 0x64U;
    input.extra_adjust_admission_available = true;
    if (!tecmo_gameplay_cpu_route_profile_project(
            &movement, &c045, &input, &result) ||
        result.player_row_7c48 != 3U || result.condition_7c48 != 0x64U ||
        result.movement_value_06e7 != 0x16U ||
        result.mode_adjustment != -1 || result.extra_adjustment_applied) {
        (void)snprintf(message, message_size,
                       "CPU route profile human actor-row vector failed");
        goto failure;
    }
    input.actor = 5U;
    input.team = 1U;
    input.roster_index = 0U;
    input.profile_movement_byte = 0x19U;
    input.difficulty = 0U;
    input.extra_adjust_admitted = true;
    if (!tecmo_gameplay_cpu_route_profile_project(
            &movement, &c045, &input, &result) ||
        result.player_row_7c48 != 12U ||
        result.movement_value_06e7 != 0x15U ||
        result.extra_adjustment != -3 ||
        !result.extra_adjustment_applied) {
        (void)snprintf(message, message_size,
                       "CPU route profile COM actor-row vector failed");
        goto failure;
    }
    input.roster_index = 11U;
    if (!tecmo_gameplay_cpu_route_profile_project(
            &movement, &c045, &input, &result) ||
        result.player_row_7c48 != 23U) {
        (void)snprintf(message, message_size,
                       "CPU route profile lineup remap failed");
        goto failure;
    }
    input.actor = 3U;
    input.team = 0U;
    input.roster_index = 3U;
    input.profile_movement_byte = 100U;
    input.extra_adjust_admitted = false;
    for (index = 0U; index < 3U; ++index) {
        input.speed_value = (uint8_t)index;
        if (!tecmo_gameplay_cpu_route_profile_project(
                &movement, &c045, &input, &result) ||
            result.mode_adjustment != mode_delta[index] ||
            result.movement_value_06e7 !=
                (uint8_t)(100U + (uint8_t)mode_delta[index])) {
            (void)snprintf(message, message_size,
                           "CPU route profile mode delta %u failed",
                           (unsigned)index);
            goto failure;
        }
    }
    input.speed_value = 1U;
    input.extra_adjust_admitted = true;
    for (index = 0U; index < 3U; ++index) {
        input.difficulty = (uint8_t)index;
        if (!tecmo_gameplay_cpu_route_profile_project(
                &movement, &c045, &input, &result) ||
            result.extra_adjustment != extra_delta[index] ||
            result.movement_value_06e7 !=
                (uint8_t)(99U + (uint8_t)extra_delta[index])) {
            (void)snprintf(message, message_size,
                           "CPU route profile extra delta %u failed",
                           (unsigned)index);
            goto failure;
        }
    }
    input.profile_movement_byte = 1U;
    input.speed_value = 2U;
    input.difficulty = 0U;
    if (!tecmo_gameplay_cpu_route_profile_project(
            &movement, &c045, &input, &result) ||
        result.movement_value_06e7 != 0xF8U) {
        (void)snprintf(message, message_size,
                       "CPU route profile byte wrapping failed");
        goto failure;
    }

    input.actor = 10U;
    if (!result_unchanged_on_failure(&movement, &c045, &input)) goto invalid;
    input.actor = 3U;
    input.team = 1U;
    if (!result_unchanged_on_failure(&movement, &c045, &input)) goto invalid;
    input.team = 0U;
    input.roster_index = 12U;
    if (!result_unchanged_on_failure(&movement, &c045, &input)) goto invalid;
    input.roster_index = 3U;
    input.speed_value = 3U;
    if (!result_unchanged_on_failure(&movement, &c045, &input)) goto invalid;
    input.speed_value = 1U;
    input.difficulty = 3U;
    if (!result_unchanged_on_failure(&movement, &c045, &input)) goto invalid;
    input.difficulty = 0U;
    input.condition_7c48 = 101U;
    if (!result_unchanged_on_failure(&movement, &c045, &input)) goto invalid;
    input.condition_7c48 = 100U;
    input.extra_adjust_admission_available = false;
    if (!result_unchanged_on_failure(&movement, &c045, &input)) goto invalid;
    input.extra_adjust_admission_available = true;
    unavailable_movement = movement;
    unavailable_movement.available = false;
    if (!result_unchanged_on_failure(
            &unavailable_movement, &c045, &input)) goto invalid;
    unavailable_c045 = c045;
    unavailable_c045.available = false;
    if (!result_unchanged_on_failure(
            &movement, &unavailable_c045, &input)) goto invalid;
    input.contract_tag ^= 1U;
    if (!result_unchanged_on_failure(&movement, &c045, &input)) goto invalid;

    tecmo_gameplay_rebound_audit_destroy(&c045);
    tecmo_gameplay_movement_assets_destroy(&movement);
    (void)snprintf(message, message_size,
                   "CPU route profile passed: rows=3/12/23 deltas=3+3 wrap=F8");
    return true;

invalid:
    (void)snprintf(message, message_size,
                   "CPU route profile transactional rejection failed");
failure:
    tecmo_gameplay_rebound_audit_destroy(&c045);
    tecmo_gameplay_movement_assets_destroy(&movement);
    return false;
}
