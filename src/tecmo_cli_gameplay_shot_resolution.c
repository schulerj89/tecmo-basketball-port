#include "tecmo_asset_pack.h"
#include "tecmo_gameplay_shot_resolution.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tecmo_cli_internal.h"

static bool validate_shot_resolution_point_contract(
    TecmoGameplayShotResolutionAssets *assets)
{
    uint8_t point_value = 0U;
    TecmoGameplayShotOutcome outcome = TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN;
        if (!tecmo_gameplay_shot_resolution_classify_point_value(
                assets, 0U, 0U, 0U, 0x01U, &point_value) ||
            point_value != 1U ||
            !tecmo_gameplay_shot_resolution_classify_point_value(
                assets, 0xFFFFU, 0xFFU, 1U, 0xFEU, &point_value) ||
            point_value != 1U ||
            !tecmo_gameplay_shot_resolution_classify_point_value(
                assets, 0x0111U, 0x6CU, 0U, 0U, &point_value) ||
            point_value != 2U ||
            !tecmo_gameplay_shot_resolution_classify_point_value(
                assets, 0x0112U, 0x6CU, 0U, 0U, &point_value) ||
            point_value != 3U ||
            !tecmo_gameplay_shot_resolution_classify_point_value(
                assets, 0x01EDU, 0x6CU, 1U, 0U, &point_value) ||
            point_value != 2U ||
            !tecmo_gameplay_shot_resolution_classify_point_value(
                assets, 0x01ECU, 0x6CU, 1U, 0U, &point_value) ||
            point_value != 3U ||
            !tecmo_gameplay_shot_resolution_classify_point_value(
                assets, 0x0108U, 0x6CU, 0U, 0U, &point_value) ||
            point_value != 2U ||
            !tecmo_gameplay_shot_resolution_classify_point_value(
                assets, 0x01F6U, 0x6CU, 1U, 0U, &point_value) ||
            point_value != 2U ||
            !tecmo_gameplay_shot_resolution_classify_point_value(
                assets, 0x0164U, 0x6CU, 0U, 0U, &point_value) ||
            point_value != 3U ||
            !tecmo_gameplay_shot_resolution_classify_point_value(
                assets, 0x019AU, 0x6CU, 1U, 0U, &point_value) ||
            point_value != 3U ||
            !tecmo_gameplay_shot_resolution_classify_point_value(
                assets, 0U, 0x5AU, 0U, 0U, &point_value) ||
            point_value != 3U ||
            !tecmo_gameplay_shot_resolution_classify_point_value(
                assets, 0xFFFFU, 0xD7U, 1U, 0U, &point_value) ||
            point_value != 3U ||
            tecmo_gameplay_shot_resolution_classify_point_value(
                assets, 0U, 0x6CU, 2U, 0U, &point_value) ||
            tecmo_gameplay_shot_resolution_classify_point_value(
                assets, 0U, 0x6CU, 0U, 0U, NULL)) {
            printf("Shot-resolution asset test failed: B995 point classifier\n");
            return false;
        }
        for (unsigned arc_index = 0U;
             arc_index < TECMO_GAMEPLAY_SHOT_POINT_ARC_COUNT;
             ++arc_index) {
            const uint8_t table_value =
                assets->point_arc_boundary[arc_index];
            const uint16_t orientation0_boundary = (uint16_t)(
                ((arc_index >= 0x06U && arc_index < 0x6DU)
                     ? 0x0100U
                     : 0U) +
                table_value);
            const uint16_t orientation1_boundary = (uint16_t)(
                ((arc_index >= 0x06U && arc_index < 0x6EU)
                     ? 0x0100U
                     : 0x0200U) +
                (uint8_t)(0xFFU - table_value));
            const uint8_t world_y = (uint8_t)(0x5BU + arc_index);
            if (!tecmo_gameplay_shot_resolution_classify_point_value(
                    assets, (uint16_t)(orientation0_boundary - 1U),
                    world_y, 0U, 0U, &point_value) ||
                point_value != 2U ||
                !tecmo_gameplay_shot_resolution_classify_point_value(
                    assets, orientation0_boundary,
                    world_y, 0U, 0U, &point_value) ||
                point_value != 3U ||
                !tecmo_gameplay_shot_resolution_classify_point_value(
                    assets, (uint16_t)(orientation1_boundary - 1U),
                    world_y, 1U, 0U, &point_value) ||
                point_value != 3U ||
                !tecmo_gameplay_shot_resolution_classify_point_value(
                    assets, orientation1_boundary,
                    world_y, 1U, 0U, &point_value) ||
                point_value != 2U) {
                printf(
                    "Shot-resolution asset test failed: B995 boundary %u\n",
                    arc_index);
                return false;
            }
        }
        if (tecmo_gameplay_shot_resolution_classify_terminal_outcome(
                assets, false, 0x80U, &outcome) ||
            !tecmo_gameplay_shot_resolution_classify_terminal_outcome(
                assets, true, 0x00U, &outcome) ||
            outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE ||
            !tecmo_gameplay_shot_resolution_classify_terminal_outcome(
                assets, true, 0x80U, &outcome) ||
            outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS ||
            !tecmo_gameplay_shot_resolution_classify_terminal_outcome(
                assets, true, 0x7FU, &outcome) ||
            outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE ||
            !tecmo_gameplay_shot_resolution_classify_terminal_outcome(
                assets, true, 0xFFU, &outcome) ||
            outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS) {
            printf("Shot-resolution asset test failed: terminal polarity contract\n");
            return false;
        }
    return true;
}

static bool validate_shot_resolution_direction_and_evaluator(
    TecmoGameplayShotResolutionAssets *assets)
{
    static const struct {
        int16_t x;
        int16_t y;
        TecmoGameplayShotDirectionSlot direction;
    } cases[] = {
        {1, 0, TECMO_GAMEPLAY_SHOT_DIRECTION_RIGHT},
        {-1, 0, TECMO_GAMEPLAY_SHOT_DIRECTION_LEFT},
        {0, 1, TECMO_GAMEPLAY_SHOT_DIRECTION_DOWN},
        {0, -1, TECMO_GAMEPLAY_SHOT_DIRECTION_UP},
        {4, 1, TECMO_GAMEPLAY_SHOT_DIRECTION_RIGHT},
        {-4, 1, TECMO_GAMEPLAY_SHOT_DIRECTION_LEFT},
        {4, -1, TECMO_GAMEPLAY_SHOT_DIRECTION_RIGHT},
        {-4, -1, TECMO_GAMEPLAY_SHOT_DIRECTION_LEFT},
        {3, 1, TECMO_GAMEPLAY_SHOT_DIRECTION_DOWN_RIGHT},
        {-3, 1, TECMO_GAMEPLAY_SHOT_DIRECTION_DOWN_LEFT},
        {3, -1, TECMO_GAMEPLAY_SHOT_DIRECTION_UP_RIGHT},
        {-3, -1, TECMO_GAMEPLAY_SHOT_DIRECTION_UP_LEFT},
        {1, 4, TECMO_GAMEPLAY_SHOT_DIRECTION_DOWN},
        {-1, 4, TECMO_GAMEPLAY_SHOT_DIRECTION_DOWN},
        {1, -4, TECMO_GAMEPLAY_SHOT_DIRECTION_UP},
        {-1, -4, TECMO_GAMEPLAY_SHOT_DIRECTION_UP},
        {INT16_MIN, 1, TECMO_GAMEPLAY_SHOT_DIRECTION_LEFT},
        {1, INT16_MIN, TECMO_GAMEPLAY_SHOT_DIRECTION_UP}
    };
    TecmoGameplayShotDirectionSlot direction;
    TecmoGameplayShotEvaluationInput input;
    TecmoGameplayShotEvaluation evaluation;
    TecmoGameplayShotEvaluation repeated;
    uint8_t profile;
    size_t index;

    /* Public inputs are target-minus-actor.  The four equality cases above
       prove the source actor-minus-target inversion and inclusive 4:1 axis
       boundary; the INT16_MIN cases prove widened absolute-value handling. */
    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        if (!tecmo_gameplay_shot_resolution_direction_for_delta(
                cases[index].x, cases[index].y, &direction) ||
            direction != cases[index].direction) {
            printf("Shot-resolution asset test failed: direction case %u\n",
                   (unsigned)index);
            return false;
        }
    }
    direction = TECMO_GAMEPLAY_SHOT_DIRECTION_UP_LEFT;
    if (tecmo_gameplay_shot_resolution_direction_for_delta(0, 0, &direction) ||
        direction != TECMO_GAMEPLAY_SHOT_DIRECTION_UP_LEFT ||
        tecmo_gameplay_shot_resolution_direction_for_delta(1, 1, NULL) ||
        !tecmo_gameplay_shot_profile_from_profile_byte2(0U, &profile) ||
        profile != 0U ||
        !tecmo_gameplay_shot_profile_from_profile_byte2(0x20U, &profile) ||
        profile != 1U ||
        !tecmo_gameplay_shot_profile_from_profile_byte2(0xFFU, &profile) ||
        profile != 1U ||
        tecmo_gameplay_shot_profile_from_profile_byte2(0U, NULL)) {
        printf("Shot-resolution asset test failed: direction/profile bounds\n");
        return false;
    }

    memset(&input, 0, sizeof(input));
    input.player_rating = 80U;
    input.point_value = 3U;
    input.horizontal_distance = -156;
    input.vertical_distance = -1;
    input.family = 0U;
    input.profile = 0U;
    input.direction = 1U;
    input.stable_sample = 99U;
    if (!tecmo_gameplay_shot_resolution_evaluate(
            assets, &input, &evaluation) ||
        evaluation.schedule !=
            TECMO_GAMEPLAY_SHOT_SCHEDULE_EXACT_THREE_POINT ||
        evaluation.outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS ||
        evaluation.make_probability < 5U ||
        evaluation.make_probability > 95U ||
        !tecmo_gameplay_shot_resolution_evaluate(
            assets, &input, &repeated) ||
        memcmp(&evaluation, &repeated, sizeof(evaluation)) != 0) {
        printf("Shot-resolution asset test failed: exact deterministic evaluator\n");
        return false;
    }
    input.point_value = 2U;
    input.direction = 0U;
    input.stable_sample = 0U;
    if (!tecmo_gameplay_shot_resolution_evaluate(
            assets, &input, &evaluation) ||
        evaluation.schedule !=
            TECMO_GAMEPLAY_SHOT_SCHEDULE_NATIVE_APPROXIMATION ||
        evaluation.outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE) {
        printf("Shot-resolution asset test failed: ordinary two-point make\n");
        return false;
    }
    input.point_value = 1U;
    input.close_context = false;
    input.numeric_variant = 0U;
    input.stable_sample = 0U;
    if (!tecmo_gameplay_shot_resolution_evaluate(
            assets, &input, &evaluation) ||
        evaluation.point_value != 1U ||
        evaluation.schedule !=
            TECMO_GAMEPLAY_SHOT_SCHEDULE_NATIVE_APPROXIMATION) {
        printf("Shot-resolution asset test failed: one-point evaluator\n");
        return false;
    }
    input.close_context = true;
    input.numeric_variant = 1U;
    input.contact_context = true;
    input.contest_context = true;
    if (!tecmo_gameplay_shot_resolution_evaluate(
            assets, &input, &evaluation) ||
        evaluation.schedule !=
            TECMO_GAMEPLAY_SHOT_SCHEDULE_CLOSE_NUMERIC_1 ||
        !evaluation.contact_context || !evaluation.contest_context ||
        evaluation.make_probability < 5U ||
        evaluation.make_probability > 95U) {
        printf("Shot-resolution asset test failed: close/contact evaluator\n");
        return false;
    }
    {
        TecmoGameplayShotEvaluation unchanged = evaluation;
        input.contact_context = true;
        input.contest_context = false;
        if (tecmo_gameplay_shot_resolution_evaluate(
                assets, &input, &evaluation) ||
            memcmp(&evaluation, &unchanged, sizeof(evaluation)) != 0) {
            printf("Shot-resolution asset test failed: contact/contest invariant\n");
            return false;
        }
        input.contact_context = true;
        input.contest_context = true;
    }
    {
        TecmoGameplayShotEvaluation unchanged = evaluation;
        input.point_value = 0U;
        if (tecmo_gameplay_shot_resolution_evaluate(
                assets, &input, &evaluation) ||
            memcmp(&evaluation, &unchanged, sizeof(evaluation)) != 0) {
            printf("Shot-resolution asset test failed: point-zero rollback\n");
            return false;
        }
    }
    {
        TecmoGameplayShotEvaluation unchanged = evaluation;
        input.point_value = 2U;
        input.close_context = false;
        input.numeric_variant = 1U;
        if (tecmo_gameplay_shot_resolution_evaluate(
                assets, &input, &evaluation) ||
            memcmp(&evaluation, &unchanged, sizeof(evaluation)) != 0) {
            printf("Shot-resolution asset test failed: non-close numeric invariant\n");
            return false;
        }
        input.close_context = true;
        input.numeric_variant = 3U;
        unchanged = evaluation;
        if (tecmo_gameplay_shot_resolution_evaluate(
                assets, &input, &evaluation) ||
            memcmp(&evaluation, &unchanged, sizeof(evaluation)) != 0) {
            printf("Shot-resolution asset test failed: typed numeric bound\n");
            return false;
        }
        input.numeric_variant = 0U;
    }
    input.point_value = 2U;
    input.family = 2U;
    {
        TecmoGameplayShotEvaluation unchanged = evaluation;
        if (tecmo_gameplay_shot_resolution_evaluate(
                assets, &input, &evaluation) ||
            memcmp(&evaluation, &unchanged, sizeof(evaluation)) != 0) {
        printf("Shot-resolution asset test failed: family bound accepted\n");
        return false;
        }
    }
    return true;
}

static bool validate_shot_resolution_rim_contract(
    TecmoGameplayShotResolutionAssets *assets)
{
    TecmoGameplayShotRimRoute route;
    TecmoGameplayShotRimRattle rattle;
    bool eligible = false;
    bool repeat_dmc = false;
    bool completed = false;
    TecmoGameplayShotSettlementDecision decision;
        {
            static const uint16_t targets[4] = {
                0xA708U, 0xA7A9U, 0xA8E9U, 0xA708U
            };
            static const TecmoGameplayShotRimRouteKind kinds[4] = {
                TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A708,
                TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A7A9,
                TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A8E9,
                TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A708
            };
            for (unsigned selector = 0U; selector < 8U; ++selector) {
                if (!tecmo_gameplay_shot_resolution_resolve_rim_route(
                        assets, (uint8_t)selector, &route) ||
                    route.selector != (selector & 3U) ||
                    route.kind != kinds[selector & 3U] ||
                    route.source_target_cpu != targets[selector & 3U]) {
                    printf("Shot-resolution asset test failed: rim route %u\n",
                           selector);
                    return false;
                }
            }
        }
        for (uint8_t source = 0U; source < 4U; ++source) {
            unsigned repeat_count = 0U;
            unsigned step_count =
                (unsigned)(source + 1U) *
                assets->rim_rattle.pass_timer_updates;
            if (!tecmo_gameplay_shot_rim_rattle_begin(
                    assets, &rattle, 0U, source, 0x05U,
                    0x0040, -0x0020) ||
                rattle.passes_remaining != (uint8_t)(source + 1U) ||
                rattle.animation_phase !=
                    (uint8_t)(((source + 1U) << 4U) | 0x05U) ||
                rattle.x != 0x009D || rattle.y != 0x0093 ||
                rattle.horizontal_velocity_q6 != -0x0040 ||
                rattle.vertical_velocity_q6 != 0 ||
                rattle.render_script_address != 0xBAB9U) {
                printf("Shot-resolution asset test failed: rim-rattle begin %u\n",
                       (unsigned)source);
                return false;
            }
            for (unsigned step = 1U; step <= step_count; ++step) {
                if (!tecmo_gameplay_shot_rim_rattle_step(
                        assets, &rattle, &repeat_dmc, &completed) ||
                    (step < step_count && completed) ||
                    (step == step_count && !completed)) {
                    printf("Shot-resolution asset test failed: rim-rattle step %u/%u\n",
                           step, step_count);
                    return false;
                }
                if (repeat_dmc) ++repeat_count;
            }
            if (!rattle.complete || rattle.active ||
                rattle.object_state != 0U ||
                rattle.horizontal_velocity_q6 != 0x0040 ||
                rattle.vertical_velocity_q6 != -0x0020 ||
                rattle.render_script_address != 0xBADDU ||
                rattle.x !=
                    ((source & 1U) == 0U ? 0x0099 : 0x009D) ||
                repeat_count != source ||
                tecmo_gameplay_shot_rim_rattle_step(
                    assets, &rattle, &repeat_dmc, &completed)) {
                printf("Shot-resolution asset test failed: rim-rattle completion %u\n",
                       (unsigned)source);
                return false;
            }
        }
        if (!tecmo_gameplay_shot_rim_rattle_begin(
                assets, &rattle, 1U, 0U, 0U, -0x0040, 0) ||
            rattle.x != 0x0263 ||
            rattle.horizontal_velocity_q6 != 0x0040 ||
            rattle.render_script_address != 0xBACBU) {
            printf("Shot-resolution asset test failed: rim-rattle orientation\n");
            return false;
        }
        for (unsigned step = 0U; step < 4U; ++step) {
            if (!tecmo_gameplay_shot_rim_rattle_step(
                    assets, &rattle, &repeat_dmc, &completed)) {
                printf("Shot-resolution asset test failed: rim-rattle mirrored step\n");
                return false;
            }
        }
        if (!completed || rattle.x != 0x0267 ||
            rattle.horizontal_velocity_q6 != -0x0040 ||
            rattle.render_script_address != 0xBB01U) {
            printf("Shot-resolution asset test failed: rim-rattle bounds\n");
            return false;
        }
        {
            TecmoGameplayShotRimRattle before_invalid = rattle;
            TecmoGameplayShotResolutionAssets malformed = *assets;
            if (tecmo_gameplay_shot_rim_rattle_begin(
                    assets, &rattle, 2U, 0U, 0U, 0x0040, 0) ||
                memcmp(&rattle, &before_invalid, sizeof(rattle)) != 0) {
                printf("Shot-resolution asset test failed: rattle orientation rollback\n");
                return false;
            }
            malformed.rim_rattle.object_state = 0U;
            if (tecmo_gameplay_shot_rim_rattle_begin(
                    &malformed, &rattle, 0U, 0U, 0U, 0, 0) ||
                memcmp(&rattle, &before_invalid, sizeof(rattle)) != 0) {
                printf("Shot-resolution asset test failed: rattle contract rollback\n");
                return false;
            }
        }
        if (!tecmo_gameplay_shot_rim_rattle_begin(
                assets, &rattle, 0U, 0U, 0U, 0, 0)) {
            printf("Shot-resolution asset test failed: late-rattle setup\n");
            return false;
        }
        {
            TecmoGameplayShotRimRattle corrupted;
            bool repeat_before = true;
            bool completed_before = true;
            rattle.timer_remaining = 0U;
            corrupted = rattle;
            if (tecmo_gameplay_shot_rim_rattle_step(
                    assets, &rattle, &repeat_before, &completed_before) ||
                memcmp(&rattle, &corrupted, sizeof(rattle)) != 0 ||
                repeat_before != true || completed_before != true) {
                printf("Shot-resolution asset test failed: rattle late rejection\n");
                return false;
            }
        }
        if (!tecmo_gameplay_shot_resolution_claimant_is_eligible(
                assets, -11, -7, 0U, 39U, &eligible) || !eligible ||
            !tecmo_gameplay_shot_resolution_claimant_is_eligible(
                assets, 10, 6, 12U, 71U, &eligible) || !eligible ||
            !tecmo_gameplay_shot_resolution_claimant_is_eligible(
                assets, -12, 0, 0U, 0U, &eligible) || eligible ||
            !tecmo_gameplay_shot_resolution_claimant_is_eligible(
                assets, 0, 7, 0U, 0U, &eligible) || eligible ||
            !tecmo_gameplay_shot_resolution_claimant_is_eligible(
                assets, 0, 0, 0U, 40U, &eligible) || eligible ||
            !tecmo_gameplay_shot_resolution_claimant_is_eligible(
                assets, 0, 0, 12U, 72U, &eligible) || eligible ||
            !tecmo_gameplay_shot_resolution_claimant_is_eligible(
                assets, 0, 0, 12U, 11U, &eligible) || !eligible) {
            printf("Shot-resolution asset test failed: claimant thresholds\n");
            return false;
        }
        if (!tecmo_gameplay_shot_resolution_decide_claimant_settlement(
                assets, true,
                TECMO_GAMEPLAY_SHOT_CLAIMANT_SAME_TEAM, &decision) ||
            decision.select_claimant || decision.change_possession ||
            decision.replace_other_handler_with_previous ||
            !tecmo_gameplay_shot_resolution_decide_claimant_settlement(
                assets, false,
                TECMO_GAMEPLAY_SHOT_CLAIMANT_SAME_TEAM, &decision) ||
            !decision.select_claimant || decision.change_possession ||
            decision.replace_other_handler_with_previous ||
            !tecmo_gameplay_shot_resolution_decide_claimant_settlement(
                assets, false,
                TECMO_GAMEPLAY_SHOT_CLAIMANT_OTHER_TEAM, &decision) ||
            !decision.select_claimant || !decision.change_possession ||
            !decision.replace_other_handler_with_previous ||
            tecmo_gameplay_shot_resolution_decide_claimant_settlement(
                assets, true,
                TECMO_GAMEPLAY_SHOT_CLAIMANT_OTHER_TEAM, &decision)) {
            printf("Shot-resolution asset test failed: claimant settlement\n");
            return false;
        }

    return true;
}

static bool validate_shot_resolution_dependencies(
    TecmoGameplayShotResolutionAssets *assets, const char *pack_path)
{
    uint8_t *payload = NULL;
    uint8_t *gameplay_core = NULL;
    uint8_t *mutation = NULL;
    uint8_t *core_mutation = NULL;
    uint8_t *committed_storage = NULL;
    uint64_t payload_size = 0U;
    uint64_t gameplay_core_size = 0U;
    bool ok = false;
        if (tecmo_asset_pack_read_entry_exact(
                pack_path, "gameplay/shot-resolution", 512U,
                &payload, &payload_size) != 0 ||
            tecmo_asset_pack_read_entry_exact(
                pack_path, "gameplay/core", 23416U,
                &gameplay_core, &gameplay_core_size) != 0) {
            printf("Shot-resolution asset test failed: dependencies unreadable\n");
            goto dependency_cleanup;
        }
        mutation = (uint8_t *)malloc((size_t)payload_size);
        core_mutation = (uint8_t *)malloc((size_t)gameplay_core_size);
        if (mutation == NULL || core_mutation == NULL) {
            printf("Shot-resolution asset test failed: mutation allocation\n");
            goto dependency_cleanup;
        }
        committed_storage = (uint8_t *)malloc(assets->storage_size);
        if (committed_storage == NULL) {
            printf("Shot-resolution asset test failed: committed snapshot allocation\n");
            goto dependency_cleanup;
        }
        memcpy(committed_storage, assets->storage, assets->storage_size);
        {
            TecmoGameplayShotResolutionAssets committed = assets[0];
        memcpy(mutation, payload, (size_t)payload_size);
        mutation[101U] = 1U;
        if (tecmo_gameplay_shot_resolution_parse(
                assets, mutation, (size_t)payload_size,
                gameplay_core, (size_t)gameplay_core_size) ||
            memcmp(assets, &committed, sizeof(committed)) != 0 ||
            memcmp(assets->storage, committed_storage,
                   assets->storage_size) != 0) {
            printf("Shot-resolution asset test failed: header reserved accepted\n");
            goto dependency_cleanup;
        }
        }
        memcpy(mutation, payload, (size_t)payload_size);
        mutation[128U] ^= 1U;
        if (tecmo_gameplay_shot_resolution_parse(
                assets, mutation, (size_t)payload_size,
                gameplay_core, (size_t)gameplay_core_size)) {
            printf("Shot-resolution asset test failed: source mutation accepted\n");
            goto dependency_cleanup;
        }
        memcpy(mutation, payload, (size_t)payload_size);
        mutation[256U] ^= 1U;
        if (tecmo_gameplay_shot_resolution_parse(
                assets, mutation, (size_t)payload_size,
                gameplay_core, (size_t)gameplay_core_size)) {
            printf("Shot-resolution asset test failed: metadata mutation accepted\n");
            goto dependency_cleanup;
        }
        memcpy(mutation, payload, (size_t)payload_size);
        mutation[320U] ^= 1U;
        if (tecmo_gameplay_shot_resolution_parse(
                assets, mutation, (size_t)payload_size,
                gameplay_core, (size_t)gameplay_core_size)) {
            printf("Shot-resolution asset test failed: route mutation accepted\n");
            goto dependency_cleanup;
        }
        memcpy(mutation, payload, (size_t)payload_size);
        mutation[352U] ^= 1U;
        if (tecmo_gameplay_shot_resolution_parse(
                assets, mutation, (size_t)payload_size,
                gameplay_core, (size_t)gameplay_core_size)) {
            printf("Shot-resolution asset test failed: point-arc mutation accepted\n");
            goto dependency_cleanup;
        }
        memcpy(mutation, payload, (size_t)payload_size);
        mutation[476U] = 1U;
        if (tecmo_gameplay_shot_resolution_parse(
                assets, mutation, (size_t)payload_size,
                gameplay_core, (size_t)gameplay_core_size)) {
            printf("Shot-resolution asset test failed: padding mutation accepted\n");
            goto dependency_cleanup;
        }
        memcpy(core_mutation, gameplay_core, (size_t)gameplay_core_size);
        core_mutation[128U] ^= 1U;
        if (tecmo_gameplay_shot_resolution_parse(
                assets, payload, (size_t)payload_size,
                core_mutation, (size_t)gameplay_core_size) ||
            tecmo_gameplay_shot_resolution_parse(
                assets, payload, (size_t)payload_size - 1U,
                gameplay_core, (size_t)gameplay_core_size) ||
            tecmo_gameplay_shot_resolution_parse(
                assets, payload, (size_t)payload_size + 1U,
                gameplay_core, (size_t)gameplay_core_size) ||
            !tecmo_gameplay_shot_resolution_parse(
                assets, payload, (size_t)payload_size,
                gameplay_core, (size_t)gameplay_core_size)) {
            printf("Shot-resolution asset test failed: dependency/size/reparse contract\n");
            goto dependency_cleanup;
        }
    ok = true;

dependency_cleanup:
    free(committed_storage);
    free(mutation);
    free(core_mutation);
    tecmo_asset_pack_free(payload);
    tecmo_asset_pack_free(gameplay_core);
    return ok;
}

static bool validate_shot_resolution_parse_reload_rollback(
    TecmoGameplayShotResolutionAssets *assets,
    const char *pack_path)
{
    uint8_t *payload = NULL;
    uint8_t *gameplay_core = NULL;
    uint8_t *mutation = NULL;
    uint8_t *committed_storage = NULL;
    uint64_t payload_size = 0U;
    uint64_t gameplay_core_size = 0U;
    TecmoGameplayShotResolutionAssets committed;
    bool ok = false;

    if (assets == NULL || pack_path == NULL ||
        tecmo_asset_pack_read_entry_exact(
            pack_path, "gameplay/shot-resolution", 512U,
            &payload, &payload_size) != 0 ||
        tecmo_asset_pack_read_entry_exact(
            pack_path, "gameplay/core", 23416U,
            &gameplay_core, &gameplay_core_size) != 0 ||
        assets->storage == NULL) {
        goto cleanup;
    }
    mutation = (uint8_t *)malloc((size_t)payload_size);
    committed_storage = (uint8_t *)malloc(assets->storage_size);
    if (mutation == NULL || committed_storage == NULL) goto cleanup;
    committed = *assets;
    memcpy(committed_storage, assets->storage, assets->storage_size);
    memcpy(mutation, payload, (size_t)payload_size);
    mutation[101U] = 1U;
    if (tecmo_gameplay_shot_resolution_parse(
            assets, mutation, (size_t)payload_size,
            gameplay_core, (size_t)gameplay_core_size) ||
        memcmp(assets, &committed, sizeof(committed)) != 0 ||
        memcmp(assets->storage, committed_storage,
               assets->storage_size) != 0) {
        goto cleanup;
    }
    ok = true;

cleanup:
    free(committed_storage);
    free(mutation);
    tecmo_asset_pack_free(payload);
    tecmo_asset_pack_free(gameplay_core);
    return ok;
}

static bool validate_shot_resolution_load_reload_rollback(
    TecmoGameplayShotResolutionAssets *assets)
{
    TecmoGameplayShotResolutionAssets committed;
    TecmoGameplayShotResolutionAssets fresh;
    uint8_t *committed_storage = NULL;
    bool ok = false;
    const char *missing_path = NULL;

    if (assets == NULL || !assets->available || assets->storage == NULL) {
        return false;
    }
    committed = *assets;
    committed_storage = (uint8_t *)malloc(assets->storage_size);
    if (committed_storage == NULL) return false;
    memcpy(committed_storage, assets->storage, assets->storage_size);
    if (tecmo_gameplay_shot_resolution_load(assets, missing_path) ||
        memcmp(assets, &committed, sizeof(committed)) != 0 ||
        memcmp(assets->storage, committed_storage,
               assets->storage_size) != 0) {
        goto cleanup;
    }
    tecmo_gameplay_shot_resolution_init(&fresh);
    if (tecmo_gameplay_shot_resolution_load(&fresh, missing_path) ||
        fresh.available || strcmp(
            fresh.status,
            "TGSR-3 gameplay/shot-resolution entry missing or wrong-sized") != 0) {
        tecmo_gameplay_shot_resolution_destroy(&fresh);
        goto cleanup;
    }
    tecmo_gameplay_shot_resolution_destroy(&fresh);
    ok = true;

cleanup:
    free(committed_storage);
    return ok;
}

int tecmo_cli_run_gameplay_shot_resolution_command(const TecmoCliContext *context)
{
    const char *command;
    int argc;
    char **argv;
    int index;

    if (context == NULL) return TECMO_CLI_NOT_HANDLED;
    command = context->command;
    argc = context->argc;
    argv = context->argv;
    index = context->index;
    if (strcmp(command, "--gameplay-shot-resolution-test") == 0) {
        const char *pack_path = index < argc ? argv[index] : NULL;
        TecmoGameplayShotResolutionAssets assets;
        const TecmoGameplayShotResolutionSourceSpan *settlement_source;
        TecmoGameplayShotOutcome outcome = TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN;
        TecmoGameplayShotRimRoute route;
        TecmoGameplayShotSettlementDecision decision;
        uint8_t point_value = 0U;
        bool eligible = false;
        bool ok = false;

        tecmo_gameplay_shot_resolution_init(&assets);
        {
            TecmoGameplayShotResolutionAssets fresh_parse;
            tecmo_gameplay_shot_resolution_init(&fresh_parse);
            if (tecmo_gameplay_shot_resolution_parse(
                    &fresh_parse, NULL, 0U, NULL, 0U) ||
                fresh_parse.available || fresh_parse.storage != NULL ||
                strcmp(fresh_parse.status,
                       "TGSR-3 header/size/reserved contract rejected") != 0) {
                printf("Shot-resolution asset test failed: fresh parse diagnostic\n");
                tecmo_gameplay_shot_resolution_destroy(&fresh_parse);
                tecmo_gameplay_shot_resolution_destroy(&assets);
                return 1;
            }
            tecmo_gameplay_shot_resolution_destroy(&fresh_parse);
        }
        if (tecmo_gameplay_shot_resolution_classify_terminal_outcome(
                &assets, true, 0U, &outcome) ||
            tecmo_gameplay_shot_resolution_resolve_rim_route(
                &assets, 0U, &route) ||
            tecmo_gameplay_shot_resolution_claimant_is_eligible(
                &assets, 0, 0, 0U, 0U, &eligible) ||
            tecmo_gameplay_shot_resolution_decide_claimant_settlement(
                &assets, false,
                TECMO_GAMEPLAY_SHOT_CLAIMANT_SAME_TEAM, &decision) ||
            tecmo_gameplay_shot_resolution_classify_point_value(
                &assets, 0x0108U, 0x6CU, 0U, 0U, &point_value)) {
            printf("Shot-resolution asset test failed: unavailable API accepted\n");
            goto shot_resolution_test_cleanup;
        }
        if (pack_path == NULL ||
            !tecmo_gameplay_shot_resolution_load(&assets, pack_path) ||
            !tecmo_gameplay_shot_resolution_load(&assets, pack_path)) {
            printf("Shot-resolution asset test failed: %s\n",
                   pack_path != NULL ? assets.status : "PACK path required");
            goto shot_resolution_test_cleanup;
        }
        settlement_source = tecmo_gameplay_shot_resolution_find_source(
            &assets,
            TECMO_GAMEPLAY_SHOT_RESOLUTION_SOURCE_CLAIMANT_SETTLEMENT);
        if (settlement_source == NULL || settlement_source->bank != 5U ||
            settlement_source->fixed_bank ||
            settlement_source->cpu_start != 0xB87CU ||
            settlement_source->cpu_end != 0xB8F5U ||
            settlement_source->byte_count != 122U ||
            settlement_source->fingerprint_fnv1a32 != 0x9E2F1F28U ||
            settlement_source->fingerprint_fnv1a64 !=
                0xC4F3A0BCC17BFCA8ULL ||
            assets.claimant_count != 10U ||
            assets.outcome_flag_mask != 0x80U ||
            assets.route_selector_mask != 0x03U ||
            assets.claimant_other_team_flag_mask != 0x10U ||
            assets.point_shot_flags_mask != 0x03U ||
            assets.point_y_min_inclusive != 0x5BU ||
            assets.point_y_max_exclusive != 0xD7U ||
            assets.point_orientation_count != 2U ||
            assets.point_arc_boundary[0U] != 0xF1U ||
            assets.point_arc_boundary[123U] != 0xDEU ||
            assets.rim_rattle.object_state != 0x15U ||
            assets.rim_rattle.orientation_start_x[0U] != 0x009DU ||
            assets.rim_rattle.orientation_start_x[1U] != 0x0263U ||
            assets.rim_rattle.start_y != 0x93U ||
            assets.rim_rattle.horizontal_velocity_q6 != 0x0040U ||
            assets.rim_rattle.altitude != 0x38U ||
            assets.rim_rattle.pass_timer_updates != 4U ||
            assets.rim_rattle.pass_source_mask != 3U ||
            assets.rim_rattle.pass_source_bias != 1U ||
            assets.rim_rattle.pass_animation_shift != 4U ||
            assets.rim_rattle.animation_low_mask != 0x0FU ||
            assets.rim_rattle.repeat_dmc_length != 0x0AU ||
            assets.rim_rattle.render_script_addresses[3U] != 0xBAC5U ||
            assets.rim_rattle.render_script_addresses[7U] != 0xBAD7U ||
            assets.rim_rattle.exit_render_script_addresses[0U] !=
                0xBADDU ||
            assets.rim_rattle.exit_render_script_addresses[1U] !=
                0xBB01U ||
            tecmo_gameplay_shot_resolution_find_source(
                &assets,
                (TecmoGameplayShotResolutionSourceKind)0) != NULL ||
            tecmo_gameplay_shot_resolution_find_source(
                &assets,
                (TecmoGameplayShotResolutionSourceKind)5) != NULL) {
            printf("Shot-resolution asset test failed: source/constants contract\n");
            goto shot_resolution_test_cleanup;
        }
        if (!validate_shot_resolution_point_contract(&assets)) {
            goto shot_resolution_test_cleanup;
        }
        if (!validate_shot_resolution_direction_and_evaluator(&assets)) {
            goto shot_resolution_test_cleanup;
        }

        if (!validate_shot_resolution_rim_contract(&assets)) {
            goto shot_resolution_test_cleanup;
        }
        if (!validate_shot_resolution_parse_reload_rollback(
                &assets, pack_path) ||
            !validate_shot_resolution_load_reload_rollback(&assets)) {
            printf("Shot-resolution asset test failed: valid-to-invalid parse/load rollback\n");
            goto shot_resolution_test_cleanup;
        }

        if (!validate_shot_resolution_dependencies(&assets, pack_path)) {
            goto shot_resolution_test_cleanup;
        }
        ok = true;

shot_resolution_test_cleanup:
        tecmo_gameplay_shot_resolution_destroy(&assets);
        tecmo_gameplay_shot_resolution_destroy(&assets);
        if (!ok) return 1;
        printf("TGSR-3 shot resolution passed: source-spans=4+4-focused point-value=B995/1,2,3 polarity=clear:make,set:miss routes=A708/A7A9/A8E9/A708 rim-rattle=1..4-pass claimant=bounded settlement=team-driven\n");
        return 0;
    }

    return TECMO_CLI_NOT_HANDLED;
}
