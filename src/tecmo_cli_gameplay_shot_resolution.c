#include "asm_inventory.h"
#include "png_writer.h"
#include "tecmo_asset_pack.h"
#include "asset_pack/tecmo_asset_pack_gameplay_audio.h"
#include "asset_pack/tecmo_asset_pack_gameplay_camera.h"
#include "asset_pack/tecmo_asset_pack_gameplay_movement.h"
#include "asset_pack/tecmo_asset_pack_gameplay_ball_dribble.h"
#include "asset_pack/tecmo_asset_pack_gameplay_fatigue.h"
#include "asset_pack/tecmo_asset_pack_gameplay_cpu_steering.h"
#include "asset_pack/tecmo_asset_pack_gameplay_hud.h"
#include "asset_pack/tecmo_asset_pack_gameplay_court_orientation.h"
#include "asset_pack/tecmo_asset_pack_gameplay_backcourt.h"
#include "asset_pack/tecmo_asset_pack_gameplay_free_throw_lineup.h"
#include "asset_pack/tecmo_asset_pack_gameplay_violation_referee.h"
#include "asset_pack/tecmo_asset_pack_music.h"
#include "tecmo_audio_output.h"
#include "tecmo_bank07.h"
#include "tecmo_game.h"
#include "tecmo_frontend_audio.h"
#include "tecmo_gameplay_audio.h"
#include "tecmo_gameplay_assets.h"
#include "tecmo_gameplay_camera.h"
#include "tecmo_gameplay_movement.h"
#include "tecmo_gameplay_ball_dribble.h"
#include "tecmo_gameplay_fatigue.h"
#include "tecmo_gameplay_cpu_steering.h"
#include "tecmo_gameplay_hud.h"
#include "tecmo_gameplay_court.h"
#include "tecmo_gameplay_court_orientation.h"
#include "tecmo_gameplay_backcourt.h"
#include "tecmo_gameplay_close_shots.h"
#include "tecmo_gameplay_dunk_cutaway.h"
#include "tecmo_gameplay_jump_shots.h"
#include "tecmo_gameplay_shot_resolution.h"
#include "tecmo_gameplay_penalties.h"
#include "tecmo_gameplay_violation_referee.h"
#include "tecmo_gameplay_free_throw_lineup.h"
#include "tecmo_gameplay_free_throw_projection_test.h"
#include "tecmo_gameplay_scene.h"
#include "tecmo_gameplay_state.h"
#include "tecmo_intro_arena_scene.h"
#include "tecmo_nes_video.h"
#include "tecmo_win32_keys.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
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
            for (unsigned selector = 0U; selector < 8U; ++selector) {
                if (!tecmo_gameplay_shot_resolution_resolve_rim_route(
                        assets, (uint8_t)selector, &route) ||
                    route.selector != (selector & 3U) ||
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
            rattle.render_script_address != 0xBB01U ||
            tecmo_gameplay_shot_rim_rattle_begin(
                assets, &rattle, 2U, 0U, 0U, 0x0040, 0) ||
            !tecmo_gameplay_shot_rim_rattle_begin(
                assets, &rattle, 0U, 0U, 0U, 0, 0) ||
            rattle.horizontal_velocity_q6 != -0x0040) {
            printf("Shot-resolution asset test failed: rim-rattle bounds\n");
            return false;
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
        memcpy(mutation, payload, (size_t)payload_size);
        mutation[101U] = 1U;
        if (tecmo_gameplay_shot_resolution_parse(
                assets, mutation, (size_t)payload_size,
                gameplay_core, (size_t)gameplay_core_size)) {
            printf("Shot-resolution asset test failed: header reserved accepted\n");
            goto dependency_cleanup;
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
    free(mutation);
    free(core_mutation);
    tecmo_asset_pack_free(payload);
    tecmo_asset_pack_free(gameplay_core);
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

        if (!validate_shot_resolution_rim_contract(&assets)) {
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
