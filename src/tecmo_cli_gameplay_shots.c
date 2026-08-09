#include "tecmo_asset_pack.h"
#include "tecmo_gameplay_close_shots.h"
#include "tecmo_gameplay_dunk_cutaway.h"
#include "tecmo_gameplay_jump_shots.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tecmo_cli_internal.h"

static bool validate_close_parse_reload_rollback(
    TecmoGameplayCloseShotAssets *assets,
    const char *pack_path)
{
    uint8_t *payload = NULL;
    uint8_t *gameplay_core = NULL;
    uint8_t *mutation = NULL;
    uint8_t *committed_storage = NULL;
    uint64_t payload_size = 0U;
    uint64_t gameplay_core_size = 0U;
    TecmoGameplayCloseShotAssets committed;
    bool ok = false;

    if (assets == NULL || pack_path == NULL ||
        tecmo_asset_pack_read_entry_exact(
            pack_path, "gameplay/close-shots", 3144U,
            &payload, &payload_size) != 0 ||
        tecmo_asset_pack_read_entry_exact(
            pack_path, "gameplay/core", 23416U,
            &gameplay_core, &gameplay_core_size) != 0 ||
        payload_size != 3144U || assets->storage == NULL) {
        goto cleanup;
    }
    mutation = (uint8_t *)malloc((size_t)payload_size);
    committed_storage = (uint8_t *)malloc(assets->storage_size);
    if (mutation == NULL || committed_storage == NULL) goto cleanup;
    committed = *assets;
    memcpy(committed_storage, assets->storage, assets->storage_size);
    memcpy(mutation, payload, (size_t)payload_size);
    mutation[128U] ^= 1U;
    if (tecmo_gameplay_close_shots_parse(
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

static bool validate_close_load_reload_rollback(
    TecmoGameplayCloseShotAssets *assets)
{
    TecmoGameplayCloseShotAssets committed;
    TecmoGameplayCloseShotAssets fresh;
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
    if (tecmo_gameplay_close_shots_load(assets, missing_path) ||
        memcmp(assets, &committed, sizeof(committed)) != 0 ||
        memcmp(assets->storage, committed_storage,
               assets->storage_size) != 0) {
        goto cleanup;
    }
    tecmo_gameplay_close_shots_init(&fresh);
    if (tecmo_gameplay_close_shots_load(&fresh, missing_path) ||
        fresh.available || strcmp(
            fresh.status,
            "TGCS-1 gameplay/close-shots entry missing or wrong-sized") != 0) {
        tecmo_gameplay_close_shots_destroy(&fresh);
        goto cleanup;
    }
    tecmo_gameplay_close_shots_destroy(&fresh);
    ok = true;

cleanup:
    free(committed_storage);
    return ok;
}

static bool validate_jump_parse_reload_rollback(
    TecmoGameplayJumpShotAssets *assets,
    const char *pack_path)
{
    uint8_t *payload = NULL;
    uint8_t *gameplay_core = NULL;
    uint8_t *close_shots = NULL;
    uint8_t *mutation = NULL;
    uint8_t *committed_storage = NULL;
    uint64_t payload_size = 0U;
    uint64_t gameplay_core_size = 0U;
    uint64_t close_shots_size = 0U;
    TecmoGameplayJumpShotAssets committed;
    bool ok = false;

    if (assets == NULL || pack_path == NULL ||
        tecmo_asset_pack_read_entry_exact(
            pack_path, "gameplay/jump-shots", 2776U,
            &payload, &payload_size) != 0 ||
        tecmo_asset_pack_read_entry_exact(
            pack_path, "gameplay/core", 23416U,
            &gameplay_core, &gameplay_core_size) != 0 ||
        tecmo_asset_pack_read_entry_exact(
            pack_path, "gameplay/close-shots", 3144U,
            &close_shots, &close_shots_size) != 0 ||
        assets->storage == NULL) {
        goto cleanup;
    }
    mutation = (uint8_t *)malloc((size_t)payload_size);
    committed_storage = (uint8_t *)malloc(assets->storage_size);
    if (mutation == NULL || committed_storage == NULL) goto cleanup;
    committed = *assets;
    memcpy(committed_storage, assets->storage, assets->storage_size);
    memcpy(mutation, payload, (size_t)payload_size);
    mutation[2672U] ^= 1U;
    if (tecmo_gameplay_jump_shots_parse(
            assets, mutation, (size_t)payload_size,
            gameplay_core, (size_t)gameplay_core_size,
            close_shots, (size_t)close_shots_size) ||
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
    tecmo_asset_pack_free(close_shots);
    return ok;
}

static bool validate_jump_load_reload_rollback(
    TecmoGameplayJumpShotAssets *assets)
{
    TecmoGameplayJumpShotAssets committed;
    TecmoGameplayJumpShotAssets fresh;
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
    if (tecmo_gameplay_jump_shots_load(assets, missing_path) ||
        memcmp(assets, &committed, sizeof(committed)) != 0 ||
        memcmp(assets->storage, committed_storage,
               assets->storage_size) != 0) {
        goto cleanup;
    }
    tecmo_gameplay_jump_shots_init(&fresh);
    if (tecmo_gameplay_jump_shots_load(&fresh, missing_path) ||
        fresh.available || strcmp(
            fresh.status,
            "TGJS-2 gameplay/jump-shots entry missing or wrong-sized") != 0) {
        tecmo_gameplay_jump_shots_destroy(&fresh);
        goto cleanup;
    }
    tecmo_gameplay_jump_shots_destroy(&fresh);
    ok = true;

cleanup:
    free(committed_storage);
    return ok;
}

int tecmo_cli_run_gameplay_close_shots_command(const TecmoCliContext *context)
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
    if (strcmp(command, "--gameplay-close-shots-test") == 0) {
        const char *pack_path = index < argc ? argv[index] : NULL;
        TecmoGameplayCloseShotAssets assets;
        TecmoGameplayCloseShotVariantInfo variant_info;
        const TecmoGameplayCloseShotSourceSpan *pose_table;
        static const TecmoGameplayCloseShotVariant variants[2] = {
            TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0,
            TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2
        };
        uint32_t phase_hash = 2166136261U;
        uint32_t pose_hash = 2166136261U;
        unsigned step_count = 0U;
        unsigned pose_count = 0U;
        uint8_t phase = 0U;
        uint16_t pointer = 0U;
        tecmo_gameplay_close_shots_init(&assets);
        {
            TecmoGameplayCloseShotAssets fresh_parse;
            tecmo_gameplay_close_shots_init(&fresh_parse);
            if (tecmo_gameplay_close_shots_parse(
                    &fresh_parse, NULL, 0U, NULL, 0U) ||
                fresh_parse.available || fresh_parse.storage != NULL ||
                strcmp(fresh_parse.status,
                       "TGCS-1 header/size/reserved contract rejected") != 0) {
                printf("Close-shot asset test failed: fresh parse diagnostic\n");
                tecmo_gameplay_close_shots_destroy(&fresh_parse);
                tecmo_gameplay_close_shots_destroy(&assets);
                return 1;
            }
            tecmo_gameplay_close_shots_destroy(&fresh_parse);
        }
        if (tecmo_gameplay_close_shots_get_variant_info(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0,
                &variant_info) ||
            tecmo_gameplay_close_shots_phase_for_step(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0, 0U, &phase) ||
            tecmo_gameplay_close_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0,
                TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0,
                TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0, 0U, &pointer) ||
            tecmo_gameplay_close_shots_find_source(
                &assets,
                TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_POSE_LOW_HIGH_TABLE) != NULL) {
            printf("Close-shot asset test failed: unavailable helper accepted\n");
            tecmo_gameplay_close_shots_destroy(&assets);
            return 1;
        }
        if (pack_path == NULL ||
            !tecmo_gameplay_close_shots_load(&assets, pack_path)) {
            printf("Close-shot asset test failed: %s\n",
                   pack_path != NULL ? assets.status : "PACK path required");
            tecmo_gameplay_close_shots_destroy(&assets);
            return 1;
        }
        if (!tecmo_gameplay_close_shots_load(&assets, pack_path)) {
            printf("Close-shot asset test failed: reload contract: %s\n",
                   assets.status);
            tecmo_gameplay_close_shots_destroy(&assets);
            return 1;
        }
        if (!validate_close_parse_reload_rollback(&assets, pack_path) ||
            !validate_close_load_reload_rollback(&assets)) {
            printf("Close-shot asset test failed: valid-to-invalid parse/load rollback\n");
            tecmo_gameplay_close_shots_destroy(&assets);
            return 1;
        }
        pose_table = tecmo_gameplay_close_shots_find_source(
            &assets, TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_POSE_LOW_HIGH_TABLE);
        if (pose_table == NULL || pose_table->bank != 5U ||
            pose_table->cpu_start != 0x8CEDU ||
            pose_table->cpu_end != 0x8D3CU ||
            pose_table->byte_count != 80U ||
            pose_table->fingerprint != 0x9BFCCE7CU ||
            tecmo_gameplay_close_shots_find_source(
                &assets, (TecmoGameplayCloseShotSourceKind)0) != NULL ||
            tecmo_gameplay_close_shots_find_source(
                &assets, (TecmoGameplayCloseShotSourceKind)14) != NULL) {
            printf("Close-shot asset test failed: source provenance contract\n");
            tecmo_gameplay_close_shots_destroy(&assets);
            return 1;
        }
        if (!tecmo_gameplay_close_shots_get_variant_info(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0,
                &variant_info) ||
            variant_info.numeric_variant != 0U ||
            variant_info.semantic_kind !=
                TECMO_GAMEPLAY_CLOSE_SHOT_SEMANTIC_DUNK ||
            variant_info.family_flags !=
                (TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_DIRECT |
                 TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_HELD_RELEASE) ||
            variant_info.step_count != 32U ||
            variant_info.pose_phase_count != 7U ||
            !tecmo_gameplay_close_shots_get_variant_info(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2,
                &variant_info) ||
            variant_info.numeric_variant != 2U ||
            variant_info.semantic_kind !=
                TECMO_GAMEPLAY_CLOSE_SHOT_SEMANTIC_LAYUP ||
            variant_info.family_flags !=
                (TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_ARC |
                 TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_LONGER_TRAJECTORY |
                 TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_CONTACTABLE) ||
            variant_info.step_count != 16U ||
            variant_info.pose_phase_count != 6U ||
            !tecmo_gameplay_close_shots_get_variant_info(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_1, &variant_info) ||
            variant_info.numeric_variant != 1U ||
            variant_info.semantic_kind !=
                TECMO_GAMEPLAY_CLOSE_SHOT_SEMANTIC_UNKNOWN ||
            variant_info.family_flags !=
                TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_UNPROVEN_NUMERIC ||
            variant_info.step_count != 0U ||
            variant_info.pose_phase_count != 0U ||
            tecmo_gameplay_close_shots_get_variant_info(
                &assets, (TecmoGameplayCloseShotVariant)3, &variant_info) ||
            tecmo_gameplay_close_shots_get_variant_info(
                &assets, (TecmoGameplayCloseShotVariant)-1, &variant_info) ||
            tecmo_gameplay_close_shots_get_variant_info(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0, NULL)) {
            printf("Close-shot asset test failed: numeric/semantic variant contract\n");
            tecmo_gameplay_close_shots_destroy(&assets);
            return 1;
        }
        for (unsigned variant_index = 0U; variant_index < 2U;
             ++variant_index) {
            unsigned steps = variant_index == 0U ? 32U : 16U;
            unsigned phases = variant_index == 0U ? 7U : 6U;
            for (unsigned step = 0U; step < steps; ++step) {
                if (!tecmo_gameplay_close_shots_phase_for_step(
                        &assets, variants[variant_index], (uint8_t)step,
                        &phase)) {
                    printf("Close-shot asset test failed: phase step %u/%u\n",
                           variant_index, step);
                    tecmo_gameplay_close_shots_destroy(&assets);
                    return 1;
                }
                phase_hash ^= phase;
                phase_hash *= 16777619U;
                ++step_count;
            }
            for (unsigned profile = 0U; profile < 2U; ++profile) {
                for (unsigned direction = 0U; direction < 8U; ++direction) {
                    for (unsigned pose_phase = 0U; pose_phase < phases;
                         ++pose_phase) {
                        if (!tecmo_gameplay_close_shots_resolve_pose_pointer_index(
                                &assets, variants[variant_index],
                                (TecmoGameplayCloseShotProfile)profile,
                                (TecmoGameplayCloseShotDirection)direction,
                                (uint8_t)pose_phase, &pointer)) {
                            printf("Close-shot asset test failed: pose %u/%u/%u/%u\n",
                                   variant_index, profile, direction,
                                   pose_phase);
                            tecmo_gameplay_close_shots_destroy(&assets);
                            return 1;
                        }
                        pose_hash ^= (uint8_t)(pointer & 0xFFU);
                        pose_hash *= 16777619U;
                        pose_hash ^= (uint8_t)(pointer >> 8U);
                        pose_hash *= 16777619U;
                        ++pose_count;
                    }
                }
            }
        }
        if (phase_hash != 0x0445C745U || step_count != 48U ||
            pose_hash != 0xBFDB4095U || pose_count != 208U ||
            !tecmo_gameplay_close_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0,
                TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0,
                TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0, 0U, &pointer) ||
            pointer != 637U ||
            !tecmo_gameplay_close_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0,
                TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_1,
                TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_7, 6U, &pointer) ||
            pointer != 664U ||
            !tecmo_gameplay_close_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2,
                TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0,
                TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0, 0U, &pointer) ||
            pointer != 807U ||
            !tecmo_gameplay_close_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2,
                TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_1,
                TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_7, 5U, &pointer) ||
            pointer != 830U) {
            printf("Close-shot asset test failed: phase/pose golden contract\n");
            tecmo_gameplay_close_shots_destroy(&assets);
            return 1;
        }
        if (tecmo_gameplay_close_shots_phase_for_step(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0, 32U, &phase) ||
            tecmo_gameplay_close_shots_phase_for_step(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2, 16U, &phase) ||
            tecmo_gameplay_close_shots_phase_for_step(
                &assets, (TecmoGameplayCloseShotVariant)1, 0U, &phase) ||
            tecmo_gameplay_close_shots_phase_for_step(
                &assets, (TecmoGameplayCloseShotVariant)-1, 0U, &phase) ||
            tecmo_gameplay_close_shots_phase_for_step(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0, 0U, NULL) ||
            tecmo_gameplay_close_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0,
                (TecmoGameplayCloseShotProfile)2,
                TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0, 0U, &pointer) ||
            tecmo_gameplay_close_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0,
                (TecmoGameplayCloseShotProfile)-1,
                TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0, 0U, &pointer) ||
            tecmo_gameplay_close_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0,
                TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0,
                (TecmoGameplayCloseShotDirection)8, 0U, &pointer) ||
            tecmo_gameplay_close_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0,
                TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0,
                (TecmoGameplayCloseShotDirection)-1, 0U, &pointer) ||
            tecmo_gameplay_close_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0,
                TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0,
                TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0, 7U, &pointer) ||
            tecmo_gameplay_close_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2,
                TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0,
                TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0, 6U, &pointer) ||
            tecmo_gameplay_close_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2,
                TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0,
                TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0, 0U, NULL)) {
            printf("Close-shot asset test failed: invalid input accepted\n");
            tecmo_gameplay_close_shots_destroy(&assets);
            return 1;
        }
        {
            static const uint16_t expected_numeric1[8] = {
                755U, 723U, 739U, 747U, 731U, 707U, 763U, 715U
            };
            TecmoGameplayCloseShotVariant selected;
            for (unsigned direction = 0U; direction < 8U; ++direction) {
                if (!tecmo_gameplay_close_shots_resolve_numeric_variant1_pose_pointer_index(
                        &assets,
                        (TecmoGameplayCloseShotDirection)direction,
                        &pointer) ||
                    pointer != expected_numeric1[direction]) {
                    printf("Close-shot asset test failed: numeric-1 pose %u\n",
                           direction);
                    tecmo_gameplay_close_shots_destroy(&assets);
                    return 1;
                }
            }
            if (!tecmo_gameplay_close_shots_select_numeric_variant(
                    0, 0, true, &selected) ||
                selected != TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_1 ||
                !tecmo_gameplay_close_shots_select_numeric_variant(
                    32, 0, false, &selected) ||
                selected != TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2 ||
                !tecmo_gameplay_close_shots_select_numeric_variant(
                    24, 80, false, &selected) ||
                selected != TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0 ||
                tecmo_gameplay_close_shots_select_numeric_variant(
                    -9, 0, false, &selected) ||
                tecmo_gameplay_close_shots_select_numeric_variant(
                    49, 0, false, &selected) ||
                tecmo_gameplay_close_shots_select_numeric_variant(
                    0, -65, false, &selected) ||
                tecmo_gameplay_close_shots_select_numeric_variant(
                    0, 81, false, &selected) ||
                tecmo_gameplay_close_shots_select_numeric_variant(
                    0, 0, false, NULL)) {
                printf("Close-shot asset test failed: neutral selector bounds\n");
                tecmo_gameplay_close_shots_destroy(&assets);
                return 1;
            }
            selected = TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2;
            if (tecmo_gameplay_close_shots_select_numeric_variant(
                    -9, 0, false, &selected) ||
                selected != TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2) {
                printf("Close-shot asset test failed: neutral selector output rollback\n");
                tecmo_gameplay_close_shots_destroy(&assets);
                return 1;
            }
            pointer = 0xBEEFU;
            if (tecmo_gameplay_close_shots_resolve_numeric_variant1_pose_pointer_index(
                    &assets, (TecmoGameplayCloseShotDirection)8, &pointer) ||
                pointer != 0xBEEFU ||
                tecmo_gameplay_close_shots_resolve_numeric_variant1_pose_pointer_index(
                    NULL, TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0, &pointer) ||
                pointer != 0xBEEFU) {
                printf("Close-shot asset test failed: numeric-1 pose output rollback\n");
                tecmo_gameplay_close_shots_destroy(&assets);
                return 1;
            }
        }
        printf("TGCS-1 close-shot assets passed: sources=13 source-complete-semantics=0:dunk,2:layup (2 families) native-numeric-identities=3 (numeric-1 pose-only; full trajectory incomplete) steps=48 poses=208 phases=0445C745 pose-sequence=BFDB4095\n");
        tecmo_gameplay_close_shots_destroy(&assets);
        tecmo_gameplay_close_shots_destroy(&assets);
        return 0;
    }

    return TECMO_CLI_NOT_HANDLED;
}

int tecmo_cli_run_gameplay_dunk_command(const TecmoCliContext *context)
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
    if (strcmp(command, "--gameplay-dunk-cutaway-test") == 0) {
        const char *pack_path = index < argc ? argv[index] : NULL;
        char message[192];
        if (!tecmo_gameplay_dunk_cutaway_self_test(
                pack_path, message, sizeof(message))) {
            printf("Dunk-cutaway asset test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    return TECMO_CLI_NOT_HANDLED;
}

static bool validate_jump_shot_flight_contract(
    TecmoGameplayJumpShotAssets *assets)
{
    uint16_t altitude = 0U;
    uint16_t velocity = 0x02E8U;
    bool landed = false;
    TecmoGameplayJumpShotFlightState flight;
    TecmoGameplayJumpShotMadeSettlement made;
        for (unsigned step = 1U; step <= 36U; ++step) {
            if (!tecmo_gameplay_jump_shots_step_q8(
                    assets, &altitude, &velocity, &landed) ||
                (step < 36U && landed) || (step == 18U &&
                    (altitude != 0x1998U || velocity != 0x0018U)) ||
                (step == 19U &&
                    (altitude != 0x1988U || velocity != 0xFFF0U))) {
                printf("Jump-shot asset test failed: Q8.8 step %u\n", step);
                return false;
            }
        }
        if (!landed || altitude != 0U || velocity != 0U) {
            printf("Jump-shot asset test failed: Q8.8 clamp contract\n");
            return false;
        }
        memset(&flight, 0xA5, sizeof(flight));
        if (!tecmo_gameplay_jump_shots_begin_distance_flight(
                assets, 0x0108U, 0x70U, 0x00A0U, 0x8FU,
                0x3900U, 0U, &flight) ||
            flight.world_x_q6 != 0x4200U ||
            flight.world_y_q6 != 0x1C00U ||
            flight.velocity_x_q6 != 0xFF92U ||
            flight.velocity_y_q6 != 0x0021U ||
            flight.altitude_velocity_q8 != 0x04ECU ||
            flight.remaining_updates != 60U ||
            flight.object_state != 5U ||
            !tecmo_gameplay_jump_shots_step_distance_flight(
                assets, &flight) ||
            flight.world_x_q6 != 0x4192U ||
            flight.world_y_q6 != 0x1C21U ||
            flight.altitude_q8 != 0x3DC4U ||
            flight.altitude_velocity_q8 != 0x04C4U ||
            flight.remaining_updates != 59U) {
            printf("Jump-shot asset test failed: distance flight contract\n");
            return false;
        }
        if (!tecmo_gameplay_jump_shots_begin_distance_flight(
                assets, 0x0100U, 0x8FU, 0x0110U, 0x8FU,
                0x4000U, 0U, &flight) ||
            flight.remaining_updates != 26U ||
            flight.velocity_x_q6 != 39U ||
            flight.velocity_y_q6 != 0U ||
            flight.altitude_velocity_q8 != 0x0208U ||
            !tecmo_gameplay_jump_shots_begin_distance_flight(
                assets, 0x0100U, 0x8FU, 0x0110U, 0x8FU,
                0x3900U, 3U, &flight) ||
            flight.remaining_updates != 26U ||
            flight.altitude_velocity_q8 != 0x03DCU) {
            printf("Jump-shot asset test failed: distance initializer goldens\n");
            return false;
        }
        if (!tecmo_gameplay_jump_shots_begin_distance_flight(
                assets, 0x0000U, 0x8FU, 0x0260U, 0x8FU,
                0x4000U, 0U, &flight) ||
            flight.remaining_updates != 60U ||
            flight.velocity_x_q6 != 0xFE45U) {
            printf("Jump-shot asset test failed: wide distance wrap golden\n");
            return false;
        }
        if (!tecmo_gameplay_jump_shots_begin_distance_flight(
                assets, 0x0100U, 0x8FU, 0x0110U, 0x8FU,
                0xFF00U, 0U, &flight) ||
            flight.remaining_updates != 26U ||
            flight.altitude_velocity_q8 != 0x19B2U) {
            printf("Jump-shot asset test failed: altitude factor wrap golden\n");
            return false;
        }
        memset(&flight, 0xA5, sizeof(flight));
        {
            TecmoGameplayJumpShotFlightState unchanged = flight;
            if (tecmo_gameplay_jump_shots_begin_distance_flight(
                    assets, 0x0100U, 0xFEU, 0x0101U, 0xFEU,
                    0x4000U, 0U, &flight) ||
                memcmp(&flight, &unchanged, sizeof(flight)) != 0) {
                printf("Jump-shot asset test failed: zero count accepted\n");
                return false;
            }
        }
        if (!tecmo_gameplay_jump_shots_begin_distance_flight(
                assets, 0x0100U, 0x8FU, 0x0110U, 0x8FU,
                0x3900U, 3U, &flight)) {
            return false;
        }
        flight.remaining_updates = 0U;
        flight.altitude_q8 = 0x3C00U;
        flight.altitude_velocity_q8 = 0U;
        if (!tecmo_gameplay_jump_shots_step_distance_flight(
                assets, &flight) ||
            flight.object_state != 5U || flight.altitude_q8 != 0x3BD8U ||
            !tecmo_gameplay_jump_shots_step_distance_flight(
                assets, &flight) ||
            flight.object_state != 7U || flight.altitude_q8 != 0x3BD8U) {
            printf("Jump-shot asset test failed: distance terminal branch\n");
            return false;
        }
        memset(&made, 0xA5, sizeof(made));
        if (!tecmo_gameplay_jump_shots_made_settlement_begin(
                assets, &made)) {
            printf("Jump-shot asset test failed: made settlement begin\n");
            return false;
        }
        for (unsigned step = 0U; step < 26U; ++step) {
            TecmoGameplayJumpShotMadeSettlement before = made;
            if (step == 3U &&
                (!tecmo_gameplay_jump_shots_made_settlement_step(
                     assets, &made, true) ||
                 memcmp(&made, &before, sizeof(made)) != 0)) {
                printf("Jump-shot asset test failed: made settlement stall\n");
                return false;
            }
            if (!tecmo_gameplay_jump_shots_made_settlement_step(
                    assets, &made, false) ||
                (step < 25U && made.complete)) {
                printf("Jump-shot asset test failed: made settlement step %u\n",
                       step + 1U);
                return false;
            }
        }
        if (!made.complete || made.state != 9U || made.stage != 12U ||
            made.updates != 26U) {
            printf("Jump-shot asset test failed: made settlement completion\n");
            return false;
        }
        if (!tecmo_gameplay_jump_shots_made_settlement_begin(
                assets, &made)) {
            return false;
        }
        ++made.stage;
        {
            TecmoGameplayJumpShotMadeSettlement corrupted = made;
            if (tecmo_gameplay_jump_shots_made_settlement_step(
                    assets, &made, false) ||
                memcmp(&made, &corrupted, sizeof(made)) != 0) {
                printf("Jump-shot asset test failed: corrupt settlement accepted\n");
                return false;
            }
        }

    return true;
}

static bool validate_jump_shot_dependencies(
    TecmoGameplayJumpShotAssets *assets, const char *pack_path)
{
    uint8_t *payload = NULL;
    uint8_t *gameplay_core = NULL;
    uint8_t *close_shots = NULL;
    uint8_t *payload_mutation = NULL;
    uint8_t *core_mutation = NULL;
    uint8_t *committed_storage = NULL;
    uint64_t payload_size = 0U;
    uint64_t gameplay_core_size = 0U;
    uint64_t close_shots_size = 0U;
    bool ok = false;
        if (tecmo_asset_pack_read_entry_exact(
                pack_path, "gameplay/jump-shots", 2776U,
                &payload, &payload_size) != 0 ||
            tecmo_asset_pack_read_entry_exact(
                pack_path, "gameplay/core", 23416U,
                &gameplay_core, &gameplay_core_size) != 0 ||
            tecmo_asset_pack_read_entry_exact(
                pack_path, "gameplay/close-shots", 3144U,
                &close_shots, &close_shots_size) != 0) {
            printf("Jump-shot asset test failed: dependencies unreadable\n");
            goto dependency_cleanup;
        }
        payload_mutation = (uint8_t *)malloc((size_t)payload_size);
        core_mutation = (uint8_t *)malloc((size_t)gameplay_core_size);
        if (payload_mutation == NULL || core_mutation == NULL) {
            printf("Jump-shot asset test failed: mutation allocation\n");
            goto dependency_cleanup;
        }
        committed_storage = (uint8_t *)malloc(assets->storage_size);
        if (committed_storage == NULL) {
            printf("Jump-shot asset test failed: committed snapshot allocation\n");
            goto dependency_cleanup;
        }
        memcpy(committed_storage, assets->storage, assets->storage_size);
        {
            TecmoGameplayJumpShotAssets committed = assets[0];
        memcpy(payload_mutation, payload, (size_t)payload_size);
        payload_mutation[88U] = 1U;
        if (tecmo_gameplay_jump_shots_parse(
                assets, payload_mutation, (size_t)payload_size,
                gameplay_core, (size_t)gameplay_core_size,
                close_shots, (size_t)close_shots_size) ||
            memcmp(assets, &committed, sizeof(committed)) != 0 ||
            memcmp(assets->storage, committed_storage,
                   assets->storage_size) != 0) {
            printf("Jump-shot asset test failed: reserved mutation accepted\n");
            goto dependency_cleanup;
        }
        }
        memcpy(payload_mutation, payload, (size_t)payload_size);
        payload_mutation[2672U] ^= 1U;
        if (tecmo_gameplay_jump_shots_parse(
                assets, payload_mutation, (size_t)payload_size,
                gameplay_core, (size_t)gameplay_core_size,
                close_shots, (size_t)close_shots_size)) {
            printf("Jump-shot asset test failed: constant mutation accepted\n");
            goto dependency_cleanup;
        }
        memcpy(payload_mutation, payload, (size_t)payload_size);
        payload_mutation[2712U] = 0xFFU;
        payload_mutation[2713U] = 0x7FU;
        if (tecmo_gameplay_jump_shots_parse(
                assets, payload_mutation, (size_t)payload_size,
                gameplay_core, (size_t)gameplay_core_size,
                close_shots, (size_t)close_shots_size)) {
            printf("Jump-shot asset test failed: pose mutation accepted\n");
            goto dependency_cleanup;
        }
        memcpy(core_mutation, gameplay_core, (size_t)gameplay_core_size);
        core_mutation[20632U] ^= 1U;
        if (tecmo_gameplay_jump_shots_parse(
                assets, payload, (size_t)payload_size,
                core_mutation, (size_t)gameplay_core_size,
                close_shots, (size_t)close_shots_size) ||
            tecmo_gameplay_jump_shots_parse(
                assets, payload, (size_t)payload_size - 1U,
                gameplay_core, (size_t)gameplay_core_size,
                close_shots, (size_t)close_shots_size) ||
            tecmo_gameplay_jump_shots_parse(
                assets, payload, (size_t)payload_size + 1U,
                gameplay_core, (size_t)gameplay_core_size,
                close_shots, (size_t)close_shots_size)) {
            printf("Jump-shot asset test failed: dependency/size mutation accepted\n");
            goto dependency_cleanup;
        }
        if (!tecmo_gameplay_jump_shots_parse(
                assets, payload, (size_t)payload_size,
                gameplay_core, (size_t)gameplay_core_size,
                close_shots, (size_t)close_shots_size)) {
            printf("Jump-shot asset test failed: canonical reparse: %s\n",
                   assets->status);
            goto dependency_cleanup;
        }
    ok = true;

dependency_cleanup:
    free(committed_storage);
    free(payload_mutation);
    free(core_mutation);
    tecmo_asset_pack_free(payload);
    tecmo_asset_pack_free(gameplay_core);
    tecmo_asset_pack_free(close_shots);
    return ok;
}

int tecmo_cli_run_gameplay_jump_shots_command(const TecmoCliContext *context)
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
    if (strcmp(command, "--gameplay-jump-shots-test") == 0) {
        const char *pack_path = index < argc ? argv[index] : NULL;
        TecmoGameplayJumpShotAssets assets;
        const TecmoGameplayJumpShotSourceSpan *settlement;
        uint32_t pose_hash = 2166136261U;
        uint16_t pointer = 0U;
        uint16_t altitude = 0U;
        uint16_t velocity = 0x02E8U;
        bool landed = false;
        bool ok = false;

        tecmo_gameplay_jump_shots_init(&assets);
        {
            TecmoGameplayJumpShotAssets fresh_parse;
            tecmo_gameplay_jump_shots_init(&fresh_parse);
            if (tecmo_gameplay_jump_shots_parse(
                    &fresh_parse, NULL, 0U, NULL, 0U, NULL, 0U) ||
                fresh_parse.available || fresh_parse.storage != NULL ||
                strcmp(fresh_parse.status,
                       "TGJS-2 header/size/reserved contract rejected") != 0) {
                printf("Jump-shot asset test failed: fresh parse diagnostic\n");
                tecmo_gameplay_jump_shots_destroy(&fresh_parse);
                tecmo_gameplay_jump_shots_destroy(&assets);
                return 1;
            }
            tecmo_gameplay_jump_shots_destroy(&fresh_parse);
        }
        if (tecmo_gameplay_jump_shots_find_source(
                &assets,
                TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_POST_SHOT_SETTLEMENT) != NULL ||
            tecmo_gameplay_jump_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_0,
                TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_0,
                TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_1, &pointer) ||
            tecmo_gameplay_jump_shots_step_q8(
                &assets, &altitude, &velocity, &landed)) {
            printf("Jump-shot asset test failed: unavailable helper accepted\n");
            goto jump_shot_test_cleanup;
        }
        if (pack_path == NULL ||
            !tecmo_gameplay_jump_shots_load(&assets, pack_path) ||
            !tecmo_gameplay_jump_shots_load(&assets, pack_path)) {
            printf("Jump-shot asset test failed: %s\n",
                   pack_path != NULL ? assets.status : "PACK path required");
            goto jump_shot_test_cleanup;
        }
        if (!validate_jump_parse_reload_rollback(&assets, pack_path) ||
            !validate_jump_load_reload_rollback(&assets)) {
            printf("Jump-shot asset test failed: valid-to-invalid parse/load rollback\n");
            goto jump_shot_test_cleanup;
        }
        settlement = tecmo_gameplay_jump_shots_find_source(
            &assets, TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_POST_SHOT_SETTLEMENT);
        if (settlement == NULL || settlement->bank != 5U ||
            settlement->fixed_bank || settlement->cpu_start != 0xBA65U ||
            settlement->cpu_end != 0xBAC0U ||
            settlement->byte_count != 92U ||
            settlement->fingerprint != 0x130C585CU ||
            tecmo_gameplay_jump_shots_find_source(
                &assets, (TecmoGameplayJumpShotSourceKind)0) != NULL ||
            tecmo_gameplay_jump_shots_find_source(
                &assets, (TecmoGameplayJumpShotSourceKind)13) != NULL ||
            assets.constants.nes_b_mask != 0x40U ||
            assets.constants.actor_state_gather != 0x1EU ||
            assets.constants.actor_state_prepared != 0x0BU ||
            assets.constants.actor_state_held != 0x0CU ||
            assets.constants.actor_state_airborne != 0x0DU ||
            assets.constants.actor_state_recovery != 0x0EU ||
            assets.constants.actor_state_neutral != 0U ||
            assets.constants.ball_state_route17 != 0x17U ||
            assets.constants.gravity_q8 != 0x0028U ||
            assets.constants.floor_wrap_clamp != 0xF6U ||
            assets.constants.bounce_decay_q8 != 0x0080U ||
            assets.constants.outcome_flag_mask != 0x80U ||
            assets.constants.crowd_sfx != 11U ||
            assets.constants.side_result_base != 12U ||
            assets.constants.made_update_count != 26U ||
            assets.constants.flight_count_limit != 60U ||
            assets.constants.flight_result_altitude_threshold != 60U ||
            assets.constants.made_complete_state != 9U) {
            printf("Jump-shot asset test failed: source/constants contract\n");
            goto jump_shot_test_cleanup;
        }
        for (unsigned family = 0U; family < 2U; ++family) {
            for (unsigned profile = 0U; profile < 2U; ++profile) {
                for (unsigned direction = 0U; direction < 8U; ++direction) {
                    uint16_t base;
                    if (!tecmo_gameplay_jump_shots_resolve_pose_pointer_index(
                            &assets, (TecmoGameplayJumpShotFamily)family,
                            (TecmoGameplayJumpShotProfile)profile,
                            (TecmoGameplayJumpShotDirection)direction,
                            &pointer)) {
                        printf("Jump-shot asset test failed: pose %u/%u/%u\n",
                               family, profile, direction);
                        goto jump_shot_test_cleanup;
                    }
                    base = pointer;
                    for (uint8_t phase = 0U; phase < 8U; ++phase) {
                        if (!tecmo_gameplay_jump_shots_resolve_phase_pose_pointer_index(
                                &assets,
                                (TecmoGameplayJumpShotFamily)family,
                                (TecmoGameplayJumpShotProfile)profile,
                                (TecmoGameplayJumpShotDirection)direction,
                                (uint8_t)(0x30U | phase), &pointer) ||
                            pointer != (uint16_t)(base + phase)) {
                            printf("Jump-shot asset test failed: sequence phase %u/%u/%u/%u\n",
                                   family, profile, direction, phase);
                            goto jump_shot_test_cleanup;
                        }
                    }
                    pointer = base;
                    pose_hash ^= (uint8_t)(pointer & 0xFFU);
                    pose_hash *= 16777619U;
                    pose_hash ^= (uint8_t)(pointer >> 8U);
                    pose_hash *= 16777619U;
                }
            }
        }
        if (pose_hash != 0xA057A625U ||
            !tecmo_gameplay_jump_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_0,
                TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_0,
                TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_1, &pointer) ||
            pointer != 213U ||
            tecmo_gameplay_jump_shots_resolve_pose_pointer_index(
                &assets, (TecmoGameplayJumpShotFamily)2,
                TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_0,
                TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_0, &pointer) ||
            tecmo_gameplay_jump_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_0,
                (TecmoGameplayJumpShotProfile)2,
                TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_0, &pointer) ||
            tecmo_gameplay_jump_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_0,
                TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_0,
                (TecmoGameplayJumpShotDirection)8, &pointer) ||
            tecmo_gameplay_jump_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_0,
                TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_0,
                TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_0, NULL)) {
            printf("Jump-shot asset test failed: pose matrix contract\n");
            goto jump_shot_test_cleanup;
        }

        if (!validate_jump_shot_flight_contract(&assets)) {
            goto jump_shot_test_cleanup;
        }

        if (!validate_jump_shot_dependencies(&assets, pack_path)) {
            goto jump_shot_test_cleanup;
        }
        for (unsigned direction = 0U; direction < 2U; ++direction) {
            uint16_t base;
            if (!tecmo_gameplay_jump_shots_resolve_pose_pointer_index(
                    &assets, TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_0,
                    TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_0,
                    (TecmoGameplayJumpShotDirection)direction, &base)) {
                goto jump_shot_test_cleanup;
            }
            for (uint8_t phase = 1U; phase <= 6U; ++phase) {
                if (!tecmo_gameplay_jump_shots_resolve_phase_pose_pointer_index(
                        &assets, TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_0,
                        TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_0,
                        (TecmoGameplayJumpShotDirection)direction, phase,
                        &pointer)) {
                    goto jump_shot_test_cleanup;
                }
                printf("TGJS_PHASE_TRACE family=0 profile=0 direction=%u base=%u phase=%u pose=%u\n",
                       direction, base, phase, pointer);
            }
        }
        ok = true;

jump_shot_test_cleanup:
        tecmo_gameplay_jump_shots_destroy(&assets);
        tecmo_gameplay_jump_shots_destroy(&assets);
        if (!ok) return 1;
        printf("TGJS-2 jump-shot assets passed: sources=12 poses=32 pose-matrix=A057A625 distance-flight=explicit-input made-settlement=26 dependencies=TGPL-1/TGCS-1\n");
        return 0;
    }

    return TECMO_CLI_NOT_HANDLED;
}
