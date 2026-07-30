#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_gameplay_free_throw_projection_test.h"

#include "tecmo_gameplay_camera.h"
#include "tecmo_gameplay_free_throw_lineup.h"

#include <stdio.h>
#include <string.h>

static uint32_t fnv1a32(const uint8_t *bytes, size_t count)
{
    uint32_t hash = 2166136261U;
    for (size_t index = 0U; index < count; ++index) {
        hash ^= bytes[index];
        hash *= 16777619U;
    }
    return hash;
}

static bool states_equal(
    const TecmoGameplayCameraState *left,
    const TecmoGameplayCameraState *right)
{
    return left->camera_x == right->camera_x &&
           left->scroll_x == right->scroll_x &&
           left->scroll_aux == right->scroll_aux &&
           left->nametable_page == right->nametable_page &&
           left->aux == right->aux &&
           left->stream_direction == right->stream_direction &&
           left->layout_cursor == right->layout_cursor &&
           left->left_threshold == right->left_threshold &&
           left->right_threshold == right->right_threshold &&
           left->thresholds_valid == right->thresholds_valid &&
           left->endpoint_latched == right->endpoint_latched;
}

static bool is_settled_checkpoint(
    const TecmoGameplayCameraState *state)
{
    return state->camera_x == 0x0198U &&
           state->scroll_x == 0x98U &&
           state->scroll_aux == 0U &&
           state->nametable_page == 0U &&
           state->aux == 0U &&
           state->stream_direction == 0U &&
           state->layout_cursor == 0x34U &&
           state->left_threshold == 0x20U &&
           state->right_threshold == 0x04U &&
           state->thresholds_valid &&
           state->endpoint_latched;
}

bool tecmo_gameplay_free_throw_projection_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size)
{
    static const uint16_t expected_x[10] = {
        0x00F8U,0x0242U,0x0259U,0x0231U,0x0120U,
        0x022EU,0x0206U,0x0245U,0x00DAU,0x0102U
    };
    static const uint8_t expected_y[10] = {
        0x96U,0x78U,0xB4U,0xB4U,0x5AU,
        0x78U,0x94U,0xB4U,0x64U,0xC8U
    };
    static const bool expected_visible[10] = {
        false,true,true,true,false,true,true,true,false,false
    };
    static const uint8_t expected_screen_x[10] = {
        0U,0xAAU,0xC1U,0x99U,0U,0x96U,0x6EU,0xADU,0U,0U
    };
    TecmoGameplayFreeThrowLineupAssets lineup_assets;
    TecmoGameplayCameraAssets camera_assets;
    TecmoGameplayFreeThrowLineup lineup;
    TecmoGameplayFreeThrowLineup lineup_unchanged;
    TecmoGameplayCameraState stepped;
    TecmoGameplayCameraState settled;
    TecmoGameplayCameraState unchanged_state;
    TecmoGameplayCameraFollowInput input;
    TecmoGameplayActorProjection projection;
    TecmoGameplayActorProjection unchanged_projection;
    uint32_t lineup_hash;
    uint32_t camera_hash;
    bool passed = false;

    tecmo_gameplay_free_throw_lineup_init(&lineup_assets);
    tecmo_gameplay_camera_assets_init(&camera_assets);
    if (asset_pack_path == NULL ||
        !tecmo_gameplay_free_throw_lineup_load(
            &lineup_assets, asset_pack_path) ||
        !tecmo_gameplay_camera_assets_load(
            &camera_assets, asset_pack_path)) {
        (void)snprintf(
            message, message_size, "%s",
            asset_pack_path == NULL ? "PACK path required" :
            (!lineup_assets.available ? lineup_assets.status
                                      : camera_assets.status));
        goto cleanup;
    }
    lineup_hash = fnv1a32(
        lineup_assets.storage, lineup_assets.storage_size);
    camera_hash = fnv1a32(
        camera_assets.storage, camera_assets.storage_size);

    memset(&lineup, 0xA5, sizeof(lineup));
    lineup_unchanged = lineup;
    if (tecmo_gameplay_free_throw_lineup_derive(
            &lineup_assets, 2U, 6U, 1U, &lineup) ||
        memcmp(&lineup, &lineup_unchanged, sizeof(lineup)) != 0 ||
        !tecmo_gameplay_free_throw_lineup_derive(
            &lineup_assets, 1U, 6U, 1U, &lineup) ||
        lineup.orientation != 1U || lineup.shooter_slot != 6U ||
        lineup.secondary_slot != 1U) {
        (void)snprintf(message, message_size,
                       "TGFL derivation or invalid-input contract failed");
        goto cleanup;
    }
    for (uint8_t slot = 0U; slot < 10U; ++slot) {
        if (lineup.actors[slot].raw_world_x != expected_x[slot] ||
            lineup.actors[slot].raw_world_y != expected_y[slot] ||
            lineup.actors[slot].shooter != (slot == 6U) ||
            lineup.actors[slot].secondary != (slot == 1U)) {
            (void)snprintf(
                message, message_size,
                "TGFL checkpoint mismatch at slot %u", (unsigned)slot);
            goto cleanup;
        }
    }

    if (!tecmo_gameplay_camera_state_initialize(
            &camera_assets, &stepped)) {
        (void)snprintf(message, message_size,
                       "TGCP initializer failed");
        goto cleanup;
    }
    stepped.layout_cursor = 0x21U;
    memset(&input, 0, sizeof(input));
    input.focus_world_x =
        lineup.actors[lineup.shooter_slot].raw_world_x;
    input.orientation = 1U;
    for (uint8_t update = 0U; update < 76U; ++update) {
        uint8_t previous_scroll = stepped.scroll_x;
        if (!tecmo_gameplay_camera_follow(
                &camera_assets, &stepped, &input) ||
            stepped.scroll_x == previous_scroll) {
            (void)snprintf(
                message, message_size,
                "TGCP stopped before moving update %u",
                (unsigned)(update + 1U));
            goto cleanup;
        }
    }
    if (!is_settled_checkpoint(&stepped)) {
        (void)snprintf(message, message_size,
                       "TGCP 76-update checkpoint failed");
        goto cleanup;
    }
    unchanged_state = stepped;
    if (!tecmo_gameplay_camera_follow(
            &camera_assets, &stepped, &input) ||
        !states_equal(&stepped, &unchanged_state)) {
        (void)snprintf(message, message_size,
                       "TGCP 77th update did not hold");
        goto cleanup;
    }

    if (!tecmo_gameplay_camera_state_initialize(
            &camera_assets, &settled)) {
        (void)snprintf(message, message_size,
                       "TGCP settle initializer failed");
        goto cleanup;
    }
    settled.layout_cursor = 0x21U;
    if (!tecmo_gameplay_camera_settle(
            &camera_assets, &settled, &input) ||
        !is_settled_checkpoint(&settled) ||
        !states_equal(&settled, &stepped)) {
        (void)snprintf(message, message_size,
                       "TGCP transactional settle checkpoint failed");
        goto cleanup;
    }

    for (uint8_t slot = 0U; slot < 10U; ++slot) {
        const TecmoGameplayFreeThrowLineupActor *actor =
            &lineup.actors[slot];
        if (!tecmo_gameplay_camera_project_actor(
                &camera_assets, &settled,
                actor->raw_world_x, actor->raw_world_y,
                actor->raw_aux_state, &projection) ||
            projection.visible != expected_visible[slot] ||
            projection.screen_x != expected_screen_x[slot] ||
            projection.screen_y !=
                (expected_visible[slot] ? expected_y[slot] : 0U)) {
            (void)snprintf(
                message, message_size,
                "TGFL->TGCP projection failed at slot %u",
                (unsigned)slot);
            goto cleanup;
        }
    }

    unchanged_state = settled;
    input.orientation = 2U;
    if (tecmo_gameplay_camera_follow(
            &camera_assets, &settled, &input) ||
        tecmo_gameplay_camera_settle(
            &camera_assets, &settled, &input) ||
        !states_equal(&settled, &unchanged_state)) {
        (void)snprintf(message, message_size,
                       "invalid camera input mutated state");
        goto cleanup;
    }
    memset(&projection, 0xA5, sizeof(projection));
    unchanged_projection = projection;
    unchanged_state.nametable_page = 2U;
    if (tecmo_gameplay_camera_project_actor(
            &camera_assets, &unchanged_state,
            0U, 0U, 0U, &projection) ||
        memcmp(&projection, &unchanged_projection,
               sizeof(projection)) != 0 ||
        fnv1a32(lineup_assets.storage,
                lineup_assets.storage_size) != lineup_hash ||
        fnv1a32(camera_assets.storage,
                camera_assets.storage_size) != camera_hash) {
        (void)snprintf(
            message, message_size,
            "invalid call mutated output or asset storage");
        goto cleanup;
    }

    (void)snprintf(
        message, message_size,
        "TGFL-1 -> TGCP-1 projection test passed: orientation=1 "
        "shooter=6 secondary=1 camera=0198 visible=6");
    passed = true;

cleanup:
    tecmo_gameplay_camera_assets_destroy(&camera_assets);
    tecmo_gameplay_free_throw_lineup_destroy(&lineup_assets);
    return passed;
}
