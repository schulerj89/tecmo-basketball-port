#include "tecmo_gameplay_assets.h"

#include <stdio.h>
#include <string.h>

#include "tecmo_cli_internal.h"

int tecmo_cli_run_gameplay_asset_contract_command(const TecmoCliContext *context)
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
    if (strcmp(command, "--gameplay-assets-test") == 0) {
        const char *pack_path = index < argc ? argv[index] : NULL;
        TecmoGameplayAssets assets;
        TecmoGameplayResolvedPose pose;
        TecmoGameplayPoseContext pose_context;
        TecmoGameplayLiveBackgroundContext live_context;
        const TecmoGameplaySourceSpan *foul;
        const TecmoGameplaySourceSpan *halftime;
        unsigned resolved = 0U;
        uint32_t orientation_visual_hash = 2166136261U;
        tecmo_gameplay_assets_init(&assets);
        if (pack_path == NULL ||
            !tecmo_gameplay_assets_load(&assets, pack_path)) {
            printf("Gameplay asset test failed: %s\n",
                   pack_path != NULL ? assets.status : "PACK path required");
            tecmo_gameplay_assets_destroy(&assets);
            return 1;
        }
        if (!tecmo_gameplay_assets_load(&assets, pack_path)) {
            printf("Gameplay asset test failed: reload contract: %s\n",
                   assets.status);
            tecmo_gameplay_assets_destroy(&assets);
            return 1;
        }
        foul = tecmo_gameplay_assets_find_source(
            &assets, TECMO_GAMEPLAY_SOURCE_FOUL_OVERLAY);
        halftime = tecmo_gameplay_assets_find_source(
            &assets, TECMO_GAMEPLAY_SOURCE_HALFTIME_BANNER);
        for (unsigned pointer = 0U;
             pointer < TECMO_GAMEPLAY_ASSET_POINTER_COUNT; ++pointer) {
            pose_context.actor_slot_base =
                (uint8_t)(1U + (pointer % 4U) * 0x40U);
            pose_context.actor_attributes = (uint8_t)(pointer % 4U);
            pose_context.palette_group = (uint8_t)(pointer % 2U);
            pose_context.uniform_color = (uint8_t)(pointer & 0x3FU);
            pose_context.apply_uniform_color = true;
            pose_context.mmc3_r2_r5[0] = 0x40U;
            pose_context.mmc3_r2_r5[1] = 0x41U;
            pose_context.mmc3_r2_r5[2] = 0x42U;
            pose_context.mmc3_r2_r5[3] = 0x43U;
            if (!tecmo_gameplay_assets_resolve_pose(
                    &assets, (uint16_t)pointer, &pose_context, &pose)) {
                printf("Gameplay asset test failed: pose pointer %u rejected\n",
                       pointer);
                tecmo_gameplay_assets_destroy(&assets);
                return 1;
            }
            ++resolved;
        }
        memset(&pose_context, 0, sizeof(pose_context));
        pose_context.actor_slot_base = 0x01U;
        pose_context.actor_attributes = 0x02U;
        pose_context.palette_group = 0U;
        pose_context.uniform_color = 0x2AU;
        pose_context.apply_uniform_color = true;
        pose_context.mmc3_r2_r5[0] = 0x40U;
        pose_context.mmc3_r2_r5[1] = 0x41U;
        pose_context.mmc3_r2_r5[2] = 0x42U;
        pose_context.mmc3_r2_r5[3] = 0x43U;
        if (!tecmo_gameplay_assets_resolve_pose(
                &assets, 16U, &pose_context, &pose) ||
            pose.columns != 2U || pose.rows != 3U || pose.piece_count != 5U ||
            pose.pieces[0].dx != -11 || pose.pieces[0].dy != -46 ||
            pose.pieces[0].cell_byte != 0x42U ||
            pose.pieces[0].tile_id != 0x03U ||
            pose.pieces[0].oam_attributes != 0x42U ||
            !pose.pieces[0].flip_horizontal ||
            pose.pieces[0].palette_index != 2U ||
            pose.pieces[0].top_chr_offset != 0x10020U ||
            pose.pieces[0].bottom_chr_offset != 0x10030U ||
            pose.uniform_color != 0x2AU ||
            !pose.uniform_color_applied ||
            memcmp(pose.palette,
                   "\x1B\x0F\x26\x36\x1B\x0F\x2A\x2A"
                   "\x1B\x2A\x26\x36\x1B\x3A\x37\x07", 16U) != 0 ||
            memcmp(pose.pieces[0].top_chr,
                   "\x00\x00\x00\x00\x00\x00\x80\x40"
                   "\x00\x00\x00\x00\x00\x00\x00\x80", 16U) != 0 ||
            memcmp(pose.pieces[0].palette,
                   "\x1B\x2A\x26\x36", 4U) != 0) {
            printf("Gameplay asset test failed: $D413 pose golden mismatch\n");
            tecmo_gameplay_assets_destroy(&assets);
            return 1;
        }
        pose_context.actor_slot_base = 0x02U;
        if (tecmo_gameplay_assets_resolve_pose(
                &assets, 16U, &pose_context, &pose)) {
            printf("Gameplay asset test failed: even actor slot accepted\n");
            tecmo_gameplay_assets_destroy(&assets);
            return 1;
        }
        pose_context.actor_slot_base = 0x03U;
        if (tecmo_gameplay_assets_resolve_pose(
                &assets, 16U, &pose_context, &pose)) {
            printf("Gameplay asset test failed: non-ROM odd actor slot accepted\n");
            tecmo_gameplay_assets_destroy(&assets);
            return 1;
        }
        pose_context.actor_slot_base = 0x01U;
        pose_context.actor_attributes = 0x04U;
        if (tecmo_gameplay_assets_resolve_pose(
                &assets, 16U, &pose_context, &pose)) {
            printf("Gameplay asset test failed: invalid actor attributes accepted\n");
            tecmo_gameplay_assets_destroy(&assets);
            return 1;
        }
        pose_context.actor_attributes = 0x02U;
        pose_context.uniform_color = 0x40U;
        if (tecmo_gameplay_assets_resolve_pose(
                &assets, 16U, &pose_context, &pose)) {
            printf("Gameplay asset test failed: invalid uniform color accepted\n");
            tecmo_gameplay_assets_destroy(&assets);
            return 1;
        }
        pose_context.uniform_color = 0x2AU;
        pose_context.apply_uniform_color = false;
        if (tecmo_gameplay_assets_resolve_pose(
                &assets, 16U, &pose_context, &pose)) {
            printf("Gameplay asset test failed: inactive uniform color accepted\n");
            tecmo_gameplay_assets_destroy(&assets);
            return 1;
        }
        if (tecmo_gameplay_assets_build_live_background_context(
                &assets, 0x3FU, &live_context) ||
            tecmo_gameplay_assets_build_live_background_context(
                &assets, 0x5BU, &live_context)) {
            printf("Gameplay asset test failed: invalid live selector accepted\n");
            tecmo_gameplay_assets_destroy(&assets);
            return 1;
        }
        if (tecmo_gameplay_assets_live_band_for_scanline(0U) != 0U ||
            tecmo_gameplay_assets_live_band_for_scanline(31U) != 0U ||
            tecmo_gameplay_assets_live_band_for_scanline(32U) != 1U ||
            tecmo_gameplay_assets_live_band_for_scanline(47U) != 1U ||
            tecmo_gameplay_assets_live_band_for_scanline(48U) != 2U ||
            tecmo_gameplay_assets_live_band_for_scanline(79U) != 2U ||
            tecmo_gameplay_assets_live_band_for_scanline(80U) != 3U ||
            tecmo_gameplay_assets_live_band_for_scanline(127U) != 3U ||
            tecmo_gameplay_assets_live_band_for_scanline(128U) != 4U ||
            tecmo_gameplay_assets_live_band_for_scanline(175U) != 4U ||
            tecmo_gameplay_assets_live_band_for_scanline(176U) != 5U ||
            tecmo_gameplay_assets_live_band_for_scanline(239U) != 5U) {
            printf("Gameplay asset test failed: live band boundaries changed\n");
            tecmo_gameplay_assets_destroy(&assets);
            return 1;
        }
        if (!tecmo_gameplay_assets_build_live_background_context(
                &assets, 0x43U, &live_context)) {
            printf("Gameplay asset test failed: live band context rejected\n");
            tecmo_gameplay_assets_destroy(&assets);
            return 1;
        }
        for (unsigned screen_index = 0U;
             screen_index < TECMO_GAMEPLAY_ASSET_SCREEN_COUNT; ++screen_index) {
          for (unsigned y = 32U; y < 240U; ++y) {
            for (unsigned x = 0U; x < 256U; ++x) {
                TecmoGameplayResolvedOrientationTile tile;
                uint8_t pattern_index;
                unsigned bit = 7U - (x & 7U);
                if (!tecmo_gameplay_assets_resolve_live_orientation_tile(
                        &assets, (uint8_t)screen_index, 0U,
                        (uint8_t)(y / 8U),
                        (uint8_t)(x / 8U), (uint8_t)y,
                        &live_context, &tile)) {
                    printf("Gameplay asset test failed: orientation-base tile rejected\n");
                    tecmo_gameplay_assets_destroy(&assets);
                    return 1;
                }
                pattern_index = (uint8_t)(
                    ((tile.chr[y & 7U] >> bit) & 1U) |
                    (((tile.chr[8U + (y & 7U)] >> bit) & 1U) << 1U));
                orientation_visual_hash ^= tile.palette[pattern_index];
                orientation_visual_hash *= 16777619U;
            }
          }
        }
        if (orientation_visual_hash != 0x37381BBDU) {
            printf("Gameplay asset test failed: orientation-base visual golden mismatch (got %08X)\n",
                   orientation_visual_hash);
            tecmo_gameplay_assets_destroy(&assets);
            return 1;
        }
        if (foul == NULL || foul->cpu_start != 0xB0F8U ||
            foul->cpu_end != 0xB2B0U || halftime == NULL ||
            halftime->cpu_start != 0xBC3CU || halftime->cpu_end != 0xBD10U) {
            printf("Gameplay asset test failed: event provenance missing\n");
            tecmo_gameplay_assets_destroy(&assets);
            return 1;
        }
        printf("TGPL-1 gameplay assets passed: orientation-bases=%u sources=%u pointers=%u chr=%08X base-visual=%08X\n",
               TECMO_GAMEPLAY_ASSET_SCREEN_COUNT,
               TECMO_GAMEPLAY_ASSET_SOURCE_COUNT, resolved,
               assets.chr_fingerprint32, orientation_visual_hash);
        tecmo_gameplay_assets_destroy(&assets);
        return 0;
    }

    return TECMO_CLI_NOT_HANDLED;
}
