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

static bool bytes_equal_pattern(const void *bytes,
                                size_t byte_count,
                                uint8_t pattern)
{
    const uint8_t *values = (const uint8_t *)bytes;
    for (size_t index = 0U; index < byte_count; ++index) {
        if (values[index] != pattern) return false;
    }
    return true;
}

typedef struct {
    const char *pack_path;
    TecmoGameplayCourt court;
    TecmoGameplayCourt unavailable;
    TecmoGameplayCourtWorld world;
    TecmoGameplayCourtWorld reloaded_world;
    TecmoGameplayCourtWorld corrupt_world;
    TecmoGameplayCourtViewport viewport;
    TecmoGameplayCourtViewport unchanged_viewport;
    TecmoGameplayCourtCoordinate coordinate;
    TecmoGameplayCourtCoordinate unchanged_coordinate;
    TecmoGameplayCourtCoordinateQ8 coordinate_q8;
    TecmoGameplayCourtCoordinateQ8 unchanged_coordinate_q8;
    bool passed;
} TecmoCliCourtViewportTest;

static bool validate_court_coordinate_contract(TecmoCliCourtViewportTest *test)
{
    TecmoGameplayCourtCoordinate *coordinate = &test->coordinate;
    TecmoGameplayCourtCoordinate *unchanged_coordinate =
        &test->unchanged_coordinate;
    TecmoGameplayCourtCoordinateQ8 *coordinate_q8 =
        &test->coordinate_q8;
    TecmoGameplayCourtCoordinateQ8 *unchanged_coordinate_q8 =
        &test->unchanged_coordinate_q8;
    bool passed = true;

    coordinate->x = TECMO_GAMEPLAY_COURT_WORLD_MAX_X;
    coordinate->y = TECMO_GAMEPLAY_COURT_WORLD_MAX_Y;
    coordinate_q8->x_q8 = -1;
    coordinate_q8->y_q8 = -1;
    if (tecmo_gameplay_court_coordinate_q8_valid(coordinate_q8) ||
        !tecmo_gameplay_court_coordinate_valid(coordinate) ||
        !tecmo_gameplay_court_coordinate_to_q8(
            coordinate, coordinate_q8) ||
        coordinate_q8->x_q8 !=
            TECMO_GAMEPLAY_COURT_WORLD_MAX_X *
                TECMO_GAMEPLAY_COURT_COORDINATE_Q8_SCALE ||
        coordinate_q8->y_q8 !=
            TECMO_GAMEPLAY_COURT_WORLD_MAX_Y *
                TECMO_GAMEPLAY_COURT_COORDINATE_Q8_SCALE) {
        passed = false;
    }
    coordinate_q8->x_q8 =
        TECMO_GAMEPLAY_COURT_COORDINATE_Q8_MAX_X;
    coordinate_q8->y_q8 =
        TECMO_GAMEPLAY_COURT_COORDINATE_Q8_MAX_Y;
    if (passed &&
        (!tecmo_gameplay_court_coordinate_q8_valid(coordinate_q8) ||
         !tecmo_gameplay_court_coordinate_q8_floor(
             coordinate_q8, coordinate) ||
         coordinate->x != TECMO_GAMEPLAY_COURT_WORLD_MAX_X ||
         coordinate->y != TECMO_GAMEPLAY_COURT_WORLD_MAX_Y)) {
        passed = false;
    }
    unchanged_coordinate->x = 123;
    unchanged_coordinate->y = 45;
    unchanged_coordinate_q8->x_q8 = 12345;
    unchanged_coordinate_q8->y_q8 = 23456;
    coordinate->x = -1;
    coordinate->y = 0;
    if (passed &&
        (tecmo_gameplay_court_coordinate_valid(coordinate) ||
         tecmo_gameplay_court_coordinate_to_q8(
             coordinate, unchanged_coordinate_q8) ||
         unchanged_coordinate_q8->x_q8 != 12345 ||
         unchanged_coordinate_q8->y_q8 != 23456)) {
        passed = false;
    }
    coordinate->x = 0;
    coordinate->y =
        (int16_t)(TECMO_GAMEPLAY_COURT_WORLD_MAX_Y + 1);
    if (passed &&
        (tecmo_gameplay_court_coordinate_valid(coordinate) ||
         tecmo_gameplay_court_coordinate_to_q8(
             coordinate, unchanged_coordinate_q8) ||
         unchanged_coordinate_q8->x_q8 != 12345 ||
         unchanged_coordinate_q8->y_q8 != 23456)) {
        passed = false;
    }
    coordinate_q8->x_q8 =
        TECMO_GAMEPLAY_COURT_COORDINATE_Q8_MAX_X + 1;
    coordinate_q8->y_q8 = 0;
    if (passed &&
        (tecmo_gameplay_court_coordinate_q8_valid(coordinate_q8) ||
         tecmo_gameplay_court_coordinate_q8_floor(
             coordinate_q8, unchanged_coordinate) ||
         unchanged_coordinate->x != 123 ||
         unchanged_coordinate->y != 45 ||
         tecmo_gameplay_court_coordinate_valid(NULL) ||
         tecmo_gameplay_court_coordinate_q8_valid(NULL) ||
         tecmo_gameplay_court_coordinate_to_q8(NULL, NULL) ||
         tecmo_gameplay_court_coordinate_q8_floor(NULL, NULL))) {
        passed = false;
    }
    coordinate_q8->x_q8 = 0;
    coordinate_q8->y_q8 =
        TECMO_GAMEPLAY_COURT_COORDINATE_Q8_MAX_Y + 1;
    if (passed &&
        (tecmo_gameplay_court_coordinate_q8_valid(coordinate_q8) ||
         tecmo_gameplay_court_coordinate_q8_floor(
             coordinate_q8, unchanged_coordinate) ||
         unchanged_coordinate->x != 123 ||
         unchanged_coordinate->y != 45)) {
        passed = false;
    }
    return passed;
}

static bool load_court_viewport_world(TecmoCliCourtViewportTest *test)
{
    TecmoGameplayCourtWorld *world = &test->world;
    bool passed = true;

    memset(world, 0xA5, sizeof(*world));
    if (test->pack_path == NULL ||
        tecmo_gameplay_court_decode_world(NULL, world) ||
        !bytes_equal_pattern(world, sizeof(*world), 0xA5U) ||
        tecmo_gameplay_court_decode_world(
            &test->unavailable, world) ||
        !bytes_equal_pattern(world, sizeof(*world), 0xA5U) ||
        tecmo_gameplay_court_decode_world(
            &test->unavailable, NULL) ||
        !tecmo_gameplay_court_load(
            &test->court, test->pack_path) ||
        !tecmo_gameplay_court_decode_world(&test->court, world)) {
        passed = false;
    }
    if (passed &&
        (world->contract_tag != TECMO_GAMEPLAY_COURT_WORLD_CONTRACT_TAG ||
         world->width_tiles != TECMO_GAMEPLAY_COURT_WORLD_WIDTH_TILES ||
         world->height_tiles != TECMO_GAMEPLAY_COURT_WORLD_HEIGHT_TILES ||
         world->width_pixels != TECMO_GAMEPLAY_COURT_WORLD_WIDTH_PIXELS ||
         world->height_pixels != TECMO_GAMEPLAY_COURT_WORLD_HEIGHT_PIXELS ||
         world->minimum_macro_index != 0U ||
         world->maximum_macro_index != 360U ||
         world->unique_macro_count != 346U ||
         world->reserved != 0U ||
         world->tiles_fingerprint !=
             TECMO_GAMEPLAY_COURT_WORLD_TILES_FNV1A32 ||
         world->palette_indices_fingerprint !=
             TECMO_GAMEPLAY_COURT_WORLD_PALETTES_FNV1A32)) {
        passed = false;
    }
    return passed;
}

static bool validate_court_decode_contract(TecmoCliCourtViewportTest *test)
{
    size_t storage_offset = 0U;
    bool passed = true;

    memset(&test->reloaded_world, 0xC3, sizeof(test->reloaded_world));
    test->court.storage[storage_offset] ^= 1U;
    if (tecmo_gameplay_court_decode_world(
            &test->court, &test->reloaded_world) ||
        !bytes_equal_pattern(&test->reloaded_world,
                             sizeof(test->reloaded_world), 0xC3U)) {
        passed = false;
    }
    test->court.storage[storage_offset] ^= 1U;
    if (passed) {
        uint16_t saved_unique_macro_count =
            test->court.unique_macro_count;
        test->court.unique_macro_count = 129U;
        if (tecmo_gameplay_court_decode_world(
                &test->court, &test->reloaded_world) ||
            !bytes_equal_pattern(&test->reloaded_world,
                                 sizeof(test->reloaded_world), 0xC3U)) {
            passed = false;
        }
        test->court.unique_macro_count = saved_unique_macro_count;
    }
    if (passed &&
        tecmo_gameplay_court_decode_world(
            &test->court, NULL)) {
        passed = false;
    }
    return passed;
}

typedef struct TecmoCliCourtViewportCheckpoint {
    uint16_t camera_x;
    uint16_t first_tile_x;
    uint8_t fine_scroll_x;
    uint8_t column_count;
    uint32_t tiles_fingerprint;
    uint32_t palettes_fingerprint;
} TecmoCliCourtViewportCheckpoint;

static const TecmoCliCourtViewportCheckpoint court_viewport_checkpoints[] = {
        {0U, 0U, 0U, 32U, 0x761E3E3AU, 0xA5392D9DU},
        {1U, 0U, 1U, 33U, 0xCC377E43U, 0x90E0E4D1U},
        {7U, 0U, 7U, 33U, 0xCC377E43U, 0x90E0E4D1U},
        {8U, 1U, 0U, 32U, 0x47FE78F1U, 0x76873401U},
        {255U, 31U, 7U, 33U, 0x6C6E3A2EU, 0x31996DC1U},
        {256U, 32U, 0U, 32U, 0xD3122A2CU, 0x71C4B6FDU},
        {257U, 32U, 1U, 33U, 0x2648F8EFU, 0xEBC10A89U},
        {511U, 63U, 7U, 33U, 0xE8A99EA4U, 0xB9D18F41U},
        {512U, 64U, 0U, 32U, 0x531D6A15U, 0x1F1CE3ADU}
};

static bool validate_court_viewport_checkpoints(
    TecmoCliCourtViewportTest *test)
{
    TecmoGameplayCourtWorld *world = &test->world;
    TecmoGameplayCourtViewport *viewport = &test->viewport;

    for (size_t checkpoint = 0U;
         checkpoint < sizeof(court_viewport_checkpoints) /
                          sizeof(court_viewport_checkpoints[0]);
         ++checkpoint) {
        const uint8_t fine_scroll_x =
            court_viewport_checkpoints[checkpoint].fine_scroll_x;
        if (!tecmo_gameplay_court_slice_viewport(
                world, court_viewport_checkpoints[checkpoint].camera_x,
                viewport) ||
            viewport->camera_x != court_viewport_checkpoints[checkpoint].camera_x ||
            viewport->first_tile_x !=
                court_viewport_checkpoints[checkpoint].first_tile_x ||
            viewport->fine_scroll_x != fine_scroll_x ||
            viewport->column_count !=
                court_viewport_checkpoints[checkpoint].column_count ||
            viewport->tile_stride !=
                TECMO_GAMEPLAY_COURT_VIEWPORT_TILE_STRIDE ||
            viewport->height_tiles !=
                TECMO_GAMEPLAY_COURT_WORLD_HEIGHT_TILES ||
            viewport->tiles_fingerprint !=
                court_viewport_checkpoints[checkpoint].tiles_fingerprint ||
            viewport->palette_indices_fingerprint !=
                court_viewport_checkpoints[checkpoint].palettes_fingerprint) {
            return false;
        }
        if (fine_scroll_x == 0U) {
            for (size_t row = 0U;
                 row < TECMO_GAMEPLAY_COURT_WORLD_HEIGHT_TILES; ++row) {
                size_t unused =
                    row * TECMO_GAMEPLAY_COURT_VIEWPORT_TILE_STRIDE + 32U;
                if (viewport->tiles[unused] != 0U ||
                    viewport->palette_indices[unused] != 0U) {
                    return false;
                }
            }
        }
    }
    return true;
}

static bool validate_legacy_center_viewport(
    TecmoCliCourtViewportTest *test)
{
    const uint8_t *legacy_nametable;
    size_t legacy_size;

    legacy_nametable =
        tecmo_gameplay_court_nametable(
            &test->court, &legacy_size);
    if (!tecmo_gameplay_court_slice_viewport(
            &test->world, 256U, &test->viewport) ||
        legacy_nametable == NULL ||
        legacy_size != TECMO_GAMEPLAY_COURT_NAMETABLE_SIZE) {
        return false;
    }
    for (size_t row = 0U;
         row < TECMO_GAMEPLAY_COURT_HEIGHT; ++row) {
        for (size_t column = 0U;
             column < TECMO_GAMEPLAY_COURT_WIDTH; ++column) {
            size_t viewport_offset =
                row * TECMO_GAMEPLAY_COURT_VIEWPORT_TILE_STRIDE + column;
            size_t legacy_offset =
                row * TECMO_GAMEPLAY_COURT_WIDTH + column;
            size_t attribute_offset =
                960U + (row / 4U) * 8U + column / 4U;
            unsigned shift = ((row & 2U) != 0U ? 4U : 0U) +
                             ((column & 2U) != 0U ? 2U : 0U);
            uint8_t legacy_palette = (uint8_t)(
                (legacy_nametable[attribute_offset] >> shift) & 3U);
            if (test->viewport.tiles[viewport_offset] !=
                    legacy_nametable[legacy_offset] ||
                test->viewport.palette_indices[viewport_offset] !=
                    legacy_palette) {
                return false;
            }
        }
    }
    return true;
}

static bool validate_invalid_court_viewports(
    TecmoCliCourtViewportTest *test)
{
    TecmoGameplayCourtWorld *world = &test->world;
    TecmoGameplayCourtViewport *unchanged =
        &test->unchanged_viewport;
    TecmoGameplayCourtWorld *corrupt = &test->corrupt_world;

    memset(unchanged, 0x5A, sizeof(*unchanged));
    if (tecmo_gameplay_court_slice_viewport(
            world, 513U, unchanged) ||
        !bytes_equal_pattern(unchanged, sizeof(*unchanged), 0x5AU) ||
        tecmo_gameplay_court_slice_viewport(NULL, 0U, unchanged) ||
        !bytes_equal_pattern(unchanged, sizeof(*unchanged), 0x5AU) ||
        tecmo_gameplay_court_slice_viewport(world, 0U, NULL)) {
        return false;
    }
    *corrupt = *world;
    corrupt->contract_tag ^= 1U;
    if (tecmo_gameplay_court_slice_viewport(corrupt, 0U, unchanged) ||
        !bytes_equal_pattern(unchanged, sizeof(*unchanged), 0x5AU)) {
        return false;
    }
    *corrupt = *world;
    corrupt->tiles[0U] ^= 1U;
    if (tecmo_gameplay_court_slice_viewport(corrupt, 0U, unchanged) ||
        !bytes_equal_pattern(unchanged, sizeof(*unchanged), 0x5AU)) {
        return false;
    }
    *corrupt = *world;
    corrupt->palette_indices[
        TECMO_GAMEPLAY_COURT_WORLD_TILE_COUNT - 1U] ^= 1U;
    if (tecmo_gameplay_court_slice_viewport(corrupt, 0U, unchanged) ||
        !bytes_equal_pattern(unchanged, sizeof(*unchanged), 0x5AU)) {
        return false;
    }
    *corrupt = *world;
    corrupt->unique_macro_count = 345U;
    if (tecmo_gameplay_court_slice_viewport(corrupt, 0U, unchanged) ||
        !bytes_equal_pattern(unchanged, sizeof(*unchanged), 0x5AU)) {
        return false;
    }
    return true;
}

static bool reload_court_viewport_world(
    TecmoCliCourtViewportTest *test)
{
    return tecmo_gameplay_court_load(
               &test->court, test->pack_path) &&
           tecmo_gameplay_court_decode_world(
               &test->court, &test->reloaded_world) &&
           memcmp(&test->world, &test->reloaded_world,
                  sizeof(test->world)) == 0;
}

static int run_gameplay_court_viewport_test(const char *pack_path)
{
    TecmoCliCourtViewportTest test;

    memset(&test, 0, sizeof(test));
    test.pack_path = pack_path;
    test.passed = true;
    tecmo_gameplay_court_init(&test.court);
    tecmo_gameplay_court_init(&test.unavailable);

    if (!validate_court_coordinate_contract(&test)) {
        test.passed = false;
    }
    if (!load_court_viewport_world(&test)) {
        test.passed = false;
    }
    if (test.passed && !validate_court_decode_contract(&test)) {
        test.passed = false;
    }
    if (test.passed && !validate_court_viewport_checkpoints(&test)) {
        test.passed = false;
    }
    if (test.passed && !validate_legacy_center_viewport(&test)) {
        test.passed = false;
    }
    if (test.passed && !validate_invalid_court_viewports(&test)) {
        test.passed = false;
    }
    if (test.passed && !reload_court_viewport_world(&test)) {
        test.passed = false;
    }

    tecmo_gameplay_court_destroy(&test.unavailable);
    tecmo_gameplay_court_destroy(&test.court);
    if (!test.passed) {
        printf("TGCT-1 full court viewport test failed\n");
        return 1;
    }
    printf("TGCT-1 full court viewport passed: world=%ux%u tiles=%08X palettes=%08X cameras=%u max-x=%u\n",
           (unsigned)test.world.width_tiles,
           (unsigned)test.world.height_tiles,
           test.world.tiles_fingerprint,
           test.world.palette_indices_fingerprint,
            (unsigned)(sizeof(court_viewport_checkpoints) /
                       sizeof(court_viewport_checkpoints[0])),
           (unsigned)TECMO_GAMEPLAY_COURT_MAX_CAMERA_X);
    return 0;
}
int tecmo_cli_run_gameplay_court_commands(const TecmoCliContext *context)
{
    const char *command;
    const char *root;
    int argc;
    char **argv;
    int index;

    if (context == NULL) return TECMO_CLI_NOT_HANDLED;
    command = context->command;
    root = context->root;
    argc = context->argc;
    argv = context->argv;
    index = context->index;
    if (strcmp(command, "--gameplay-court-test") == 0) {
        const char *pack_path = index < argc ? argv[index] : NULL;
        TecmoGameplayCourt court;
        const uint8_t *nametable;
        const uint8_t *palette;
        size_t nametable_size;
        size_t palette_size;
        tecmo_gameplay_court_init(&court);
        if (tecmo_gameplay_court_nametable(&court, &nametable_size) != NULL ||
            nametable_size != 0U ||
            tecmo_gameplay_court_palette(&court, &palette_size) != NULL ||
            palette_size != 0U || pack_path == NULL ||
            !tecmo_gameplay_court_load(&court, pack_path)) {
            printf("Gameplay court test failed: %s\n",
                   pack_path != NULL ? court.status : "PACK path required");
            tecmo_gameplay_court_destroy(&court);
            return 1;
        }
        if (!tecmo_gameplay_court_load(&court, pack_path)) {
            printf("Gameplay court test failed: reload contract: %s\n",
                   court.status);
            tecmo_gameplay_court_destroy(&court);
            return 1;
        }
        nametable = tecmo_gameplay_court_nametable(
            &court, &nametable_size);
        palette = tecmo_gameplay_court_palette(&court, &palette_size);
        if (nametable == NULL || palette == NULL ||
            nametable_size != TECMO_GAMEPLAY_COURT_NAMETABLE_SIZE ||
            palette_size != TECMO_GAMEPLAY_COURT_PALETTE_SIZE ||
            court.minimum_macro_index != 0U ||
            court.maximum_macro_index != 360U ||
            court.unique_macro_count != 130U ||
            court.nametable_fingerprint != 0x0CF54A0EU ||
            court.palette_fingerprint != 0xB20C1E11U ||
            court.chr_fingerprint32 != 0xF6F6E854U ||
            court.chr_fingerprint64 != 0x96A64F53B240ABB4ULL) {
            printf("Gameplay court test failed: TGCT-1 golden mismatch\n");
            tecmo_gameplay_court_destroy(&court);
            return 1;
        }
        printf("TGCT-1 gameplay court passed: size=%u palette=%u min=%u max=%u unique=%u nametable=%08X palette-fnv=%08X\n",
               (unsigned)nametable_size, (unsigned)palette_size,
               (unsigned)court.minimum_macro_index,
               (unsigned)court.maximum_macro_index,
               (unsigned)court.unique_macro_count,
               court.nametable_fingerprint, court.palette_fingerprint);
        tecmo_gameplay_court_destroy(&court);
        return 0;
    }

    if (strcmp(command, "--gameplay-court-viewport-test") == 0) {
        const char *pack_path = index < argc ? argv[index] : NULL;
        return run_gameplay_court_viewport_test(pack_path);
    }
    return TECMO_CLI_NOT_HANDLED;
}
