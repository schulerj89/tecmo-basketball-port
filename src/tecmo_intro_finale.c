#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_intro_finale.h"

#include "tecmo_asset_pack.h"
#include "tecmo_nes_video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FINALE_ENTRY_ID "intro/finale-sequence"
#define FINALE_HEADER_SIZE 192U
#define FINALE_CELL_STRIDE 6U
#define FINALE_GROUP_STRIDE 16U
#define FINALE_PIECE_STRIDE 16U
#define FINALE_ROUTE_STRIDE 8U
#define FINALE_ANCHOR_STRIDE 2U
#define FINALE_REVERSE_META_SIZE 16U
#define FINALE_TITLE_META_SIZE 32U
#define FINALE_TITLE_SLOT_STRIDE 32U
#define FINALE_BAND_STRIDE 16U
#define FINALE_ROUTE_COUNT 5U
#define FINALE_ANCHOR_COUNT 9U
#define FINALE_REVERSE_PALETTE_FRAME_STRIDE 2U
#define FINALE_REVERSE_FIRST_SCROLL_ADJUST 8U
#define FINALE_REVERSE_FIRST_VIRTUAL_PAGE 1U

#define FINALE_SCREENS_OFFSET FINALE_HEADER_SIZE
#define FINALE_BACKGROUND_PALETTES_OFFSET \
    (FINALE_SCREENS_OFFSET + TECMO_INTRO_FINALE_SCREEN_COUNT * \
        TECMO_INTRO_FINALE_CELL_COUNT * FINALE_CELL_STRIDE)
#define FINALE_REVERSE_PALETTES_OFFSET \
    (FINALE_BACKGROUND_PALETTES_OFFSET + TECMO_INTRO_FINALE_SCREEN_COUNT * 16U)
#define FINALE_REVERSE_PALETTE_FRAMES_OFFSET \
    (FINALE_REVERSE_PALETTES_OFFSET + TECMO_INTRO_FINALE_PALETTE_STAGE_COUNT * 16U)
#define FINALE_GROUPS_OFFSET \
    ((FINALE_REVERSE_PALETTE_FRAMES_OFFSET + \
      TECMO_INTRO_FINALE_PALETTE_STAGE_COUNT * \
          FINALE_REVERSE_PALETTE_FRAME_STRIDE + 3U) & ~3U)
#define FINALE_SPRITE_PALETTES_OFFSET \
    (FINALE_GROUPS_OFFSET + \
     TECMO_INTRO_FINALE_SPRITE_PALETTE_COUNT * FINALE_GROUP_STRIDE)
#define FINALE_PIECES_OFFSET \
    (FINALE_SPRITE_PALETTES_OFFSET + \
     TECMO_INTRO_FINALE_SPRITE_PALETTE_COUNT * 16U)
#define FINALE_ROUTES_OFFSET \
    (FINALE_PIECES_OFFSET + TECMO_INTRO_FINALE_PIECE_COUNT * FINALE_PIECE_STRIDE)
#define FINALE_ANCHORS_OFFSET \
    (FINALE_ROUTES_OFFSET + FINALE_ROUTE_COUNT * FINALE_ROUTE_STRIDE)
#define FINALE_REVERSE_META_OFFSET \
    ((FINALE_ANCHORS_OFFSET + FINALE_ANCHOR_COUNT * FINALE_ANCHOR_STRIDE + 3U) & \
     ~3U)
#define FINALE_TITLE_META_OFFSET \
    (FINALE_REVERSE_META_OFFSET + FINALE_REVERSE_META_SIZE)
#define FINALE_BANDS_OFFSET \
    (FINALE_TITLE_META_OFFSET + FINALE_TITLE_META_SIZE)
#define FINALE_TITLE_SLOTS_OFFSET \
    (FINALE_BANDS_OFFSET + TECMO_INTRO_FINALE_TITLE_BAND_COUNT * FINALE_BAND_STRIDE)
#define FINALE_BYTE_COUNT \
    (FINALE_TITLE_SLOTS_OFFSET + \
     TECMO_INTRO_FINALE_TITLE_SLOT_COUNT * FINALE_TITLE_SLOT_STRIDE)

#define FINALE_OPENING_DURATION \
    (TECMO_INTRO_FINALE_LOAD_BOUNDARY_FRAMES + \
     TECMO_INTRO_FINALE_OPENING_WAIT_FRAMES)
#define FINALE_SHORT_LOOP_DURATION \
    (TECMO_INTRO_FINALE_LOAD_BOUNDARY_FRAMES + \
     TECMO_INTRO_FINALE_SHORT_LOOP_STEPS * \
         TECMO_INTRO_FINALE_SHORT_LOOP_STEP_FRAMES + \
     TECMO_INTRO_FINALE_SHORT_LOOP_WAIT_FRAMES)
/* 45 imported core frames + 1 load boundary + 6 native black/fade frames. */
#define FINALE_SELECTOR_DURATION \
    (TECMO_INTRO_FINALE_TRANSITION_IN_FRAMES + \
     TECMO_INTRO_FINALE_TRANSITION_SWAP_FRAMES + \
     TECMO_INTRO_FINALE_TRANSITION_OUT_FRAMES + \
     TECMO_INTRO_FINALE_LOAD_BOUNDARY_FRAMES + \
     TECMO_INTRO_FINALE_SELECTOR_NORMALIZATION_FRAMES)
#define FINALE_STAGED_DURATION \
    (TECMO_INTRO_FINALE_LOAD_BOUNDARY_FRAMES + \
     TECMO_INTRO_FINALE_STAGED_WAIT_FRAMES + \
     TECMO_INTRO_FINALE_STAGED_DISPATCH_WAIT_FRAMES)
#define FINALE_TITLE_HOLD_FRAME \
    (FINALE_OPENING_DURATION + FINALE_SHORT_LOOP_DURATION + \
     FINALE_SELECTOR_DURATION + FINALE_STAGED_DURATION + \
     TECMO_INTRO_FINALE_LOAD_BOUNDARY_FRAMES + \
     TECMO_INTRO_FINALE_TITLE_PREROLL_FRAMES + \
     TECMO_INTRO_FINALE_TITLE_WRITE_FRAMES + \
     TECMO_INTRO_FINALE_TITLE_TAIL_FRAMES + \
     TECMO_INTRO_FINALE_TITLE_DISPATCH_WAIT_FRAMES)

static uint16_t read_u16(const uint8_t *bytes)
{
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U);
}

static uint32_t read_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) |
           ((uint32_t)bytes[3] << 24U);
}

static uint64_t fnv1a64(const uint8_t *bytes, uint64_t byte_count)
{
    uint64_t hash = 14695981039346656037ULL;
    for (uint64_t i = 0U; i < byte_count; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static void set_status(char *dest, size_t size, const char *text)
{
    if (dest != NULL && size > 0U) {
        (void)snprintf(dest, size, "%s", text != NULL ? text : "");
    }
}

static int make_pack_path(char *path,
                          size_t path_size,
                          const char *root,
                          const char *suffix)
{
    size_t root_length;
    int written;
    if (path == NULL || path_size == 0U || root == NULL || root[0] == '\0') return -1;
    root_length = strlen(root);
    written = snprintf(path,
                       path_size,
                       "%s%s%s",
                       root,
                       root[root_length - 1U] == '\\' || root[root_length - 1U] == '/' ? "" : "\\",
                       suffix);
    return written >= 0 && (size_t)written < path_size ? 0 : -1;
}

static bool file_exists(const char *path)
{
    FILE *file;
    if (path == NULL || path[0] == '\0') return false;
    file = fopen(path, "rb");
    if (file == NULL) return false;
    fclose(file);
    return true;
}

static bool read_native_entry(const char *project_root,
                              uint8_t **bytes_out,
                              uint64_t *size_out,
                              char *pack_path_out,
                              size_t pack_path_size)
{
    const char *env_path = getenv("TECMO_ASSETPACK");
    const char *paths[5] = {0};
    char root_build[1024];
    char root_pack[1024];
    size_t path_count = 0U;

    if (env_path != NULL && env_path[0] != '\0') {
        paths[path_count++] = env_path;
    } else {
        if (make_pack_path(root_build, sizeof(root_build), project_root,
                           "build\\tecmo.assetpack") == 0) {
            paths[path_count++] = root_build;
        }
        if (make_pack_path(root_pack, sizeof(root_pack), project_root,
                           "tecmo.assetpack") == 0) {
            paths[path_count++] = root_pack;
        }
        paths[path_count++] = "build\\tecmo.assetpack";
        paths[path_count++] = "tecmo.assetpack";
        paths[path_count++] = "..\\build\\tecmo.assetpack";
    }

    for (size_t i = 0U; i < path_count; ++i) {
        if (!file_exists(paths[i])) continue;
        if (tecmo_asset_pack_read_entry(paths[i], FINALE_ENTRY_ID,
                                        bytes_out, size_out) == 0) {
            if (pack_path_out != NULL && pack_path_size > 0U) {
                (void)snprintf(pack_path_out, pack_path_size, "%s", paths[i]);
            }
            return true;
        }
        return false;
    }
    return false;
}

static bool chr_range_valid(uint32_t offset, uint64_t chr_byte_count, uint32_t size)
{
    return (uint64_t)offset <= chr_byte_count &&
           (uint64_t)size <= chr_byte_count - offset;
}

static bool range_valid(uint32_t offset, uint64_t size, uint64_t byte_count)
{
    return (uint64_t)offset <= byte_count && size <= byte_count - offset;
}

static bool palette_valid(const uint8_t *palette, size_t size)
{
    for (size_t i = 0U; i < size; ++i) if (palette[i] > 0x3FU) return false;
    return true;
}

static bool parse_asset(TecmoIntroFinaleAsset *asset,
                        const uint8_t *bytes,
                        uint64_t byte_count)
{
    uint32_t screens_offset;
    uint32_t background_palettes_offset;
    uint32_t reverse_palettes_offset;
    uint32_t reverse_palette_frames_offset;
    uint32_t groups_offset;
    uint32_t pieces_offset;
    uint32_t routes_offset;
    uint32_t anchors_offset;
    uint32_t reverse_meta_offset;
    uint32_t title_meta_offset;
    uint32_t title_slots_offset;
    uint32_t bands_offset;
    const uint8_t *reverse_meta;
    const uint8_t *title_meta;

    if (bytes == NULL || asset == NULL || byte_count < FINALE_HEADER_SIZE ||
        memcmp(bytes, "TFIN", 4U) != 0 || read_u16(bytes + 4U) != 1U ||
        read_u16(bytes + 6U) != FINALE_HEADER_SIZE ||
        read_u16(bytes + 8U) != TECMO_INTRO_FINALE_SCREEN_COUNT ||
        read_u16(bytes + 10U) != TECMO_INTRO_FINALE_WIDTH ||
        read_u16(bytes + 12U) != TECMO_INTRO_FINALE_HEIGHT ||
        read_u16(bytes + 14U) != TECMO_INTRO_FINALE_PAGE_COUNT ||
        read_u16(bytes + 16U) != FINALE_CELL_STRIDE ||
        read_u16(bytes + 18U) != TECMO_INTRO_FINALE_SCREEN_COUNT ||
        read_u16(bytes + 28U) != TECMO_INTRO_FINALE_PALETTE_STAGE_COUNT ||
        read_u16(bytes + 30U) != FINALE_REVERSE_PALETTE_FRAME_STRIDE ||
        read_u16(bytes + 40U) != TECMO_INTRO_FINALE_SPRITE_PALETTE_COUNT ||
        read_u16(bytes + 42U) != FINALE_GROUP_STRIDE ||
        read_u16(bytes + 48U) != 16U ||
        read_u16(bytes + 50U) != FINALE_PIECE_STRIDE ||
        read_u16(bytes + 52U) != TECMO_INTRO_FINALE_PIECE_COUNT ||
        read_u16(bytes + 54U) != 0U ||
        read_u16(bytes + 60U) != FINALE_ROUTE_COUNT ||
        read_u16(bytes + 62U) != FINALE_ROUTE_STRIDE ||
        read_u16(bytes + 68U) != FINALE_ANCHOR_COUNT ||
        read_u16(bytes + 70U) != FINALE_ANCHOR_STRIDE ||
        read_u16(bytes + 76U) != FINALE_REVERSE_META_SIZE ||
        read_u16(bytes + 78U) != FINALE_TITLE_META_SIZE ||
        read_u16(bytes + 88U) != TECMO_INTRO_FINALE_TITLE_SLOT_COUNT ||
        read_u16(bytes + 90U) != TECMO_INTRO_FINALE_TITLE_TEXT_SLOT_COUNT ||
        read_u16(bytes + 92U) != FINALE_TITLE_SLOT_STRIDE ||
        read_u16(bytes + 94U) != TECMO_INTRO_FINALE_TITLE_CELL_COUNT ||
        read_u16(bytes + 100U) != TECMO_INTRO_FINALE_TITLE_BAND_COUNT ||
        read_u16(bytes + 102U) != FINALE_BAND_STRIDE ||
        read_u16(bytes + 108U) != TECMO_INTRO_FINALE_LOAD_BOUNDARY_FRAMES ||
        read_u16(bytes + 110U) != 1U || read_u32(bytes + 112U) != byte_count) {
        return false;
    }
    for (size_t pad = 116U; pad < FINALE_HEADER_SIZE; ++pad) {
        if (bytes[pad] != 0U) return false;
    }

    screens_offset = read_u32(bytes + 20U);
    background_palettes_offset = read_u32(bytes + 24U);
    reverse_palettes_offset = read_u32(bytes + 32U);
    reverse_palette_frames_offset = read_u32(bytes + 36U);
    groups_offset = read_u32(bytes + 44U);
    pieces_offset = read_u32(bytes + 56U);
    routes_offset = read_u32(bytes + 64U);
    anchors_offset = read_u32(bytes + 72U);
    reverse_meta_offset = read_u32(bytes + 80U);
    title_meta_offset = read_u32(bytes + 84U);
    title_slots_offset = read_u32(bytes + 96U);
    bands_offset = read_u32(bytes + 104U);

    if (screens_offset != FINALE_SCREENS_OFFSET ||
        background_palettes_offset != FINALE_BACKGROUND_PALETTES_OFFSET ||
        reverse_palettes_offset != FINALE_REVERSE_PALETTES_OFFSET ||
        reverse_palette_frames_offset != FINALE_REVERSE_PALETTE_FRAMES_OFFSET ||
        groups_offset != FINALE_GROUPS_OFFSET || pieces_offset != FINALE_PIECES_OFFSET ||
        routes_offset != FINALE_ROUTES_OFFSET || anchors_offset != FINALE_ANCHORS_OFFSET ||
        reverse_meta_offset != FINALE_REVERSE_META_OFFSET ||
        title_meta_offset != FINALE_TITLE_META_OFFSET ||
        bands_offset != FINALE_BANDS_OFFSET ||
        title_slots_offset != FINALE_TITLE_SLOTS_OFFSET || byte_count != FINALE_BYTE_COUNT) {
        return false;
    }
    for (uint32_t pad = FINALE_REVERSE_PALETTE_FRAMES_OFFSET +
                        TECMO_INTRO_FINALE_PALETTE_STAGE_COUNT *
                            FINALE_REVERSE_PALETTE_FRAME_STRIDE;
         pad < FINALE_GROUPS_OFFSET;
         ++pad) {
        if (bytes[pad] != 0U) return false;
    }
    for (uint32_t pad = FINALE_ANCHORS_OFFSET +
                        FINALE_ANCHOR_COUNT * FINALE_ANCHOR_STRIDE;
         pad < FINALE_REVERSE_META_OFFSET;
         ++pad) {
        if (bytes[pad] != 0U) return false;
    }

    if (!range_valid(screens_offset,
                     (uint64_t)TECMO_INTRO_FINALE_SCREEN_COUNT *
                         TECMO_INTRO_FINALE_CELL_COUNT * FINALE_CELL_STRIDE,
                     byte_count) ||
        !range_valid(background_palettes_offset,
                     TECMO_INTRO_FINALE_SCREEN_COUNT * 16U,
                     byte_count) ||
        !range_valid(reverse_palettes_offset,
                     TECMO_INTRO_FINALE_PALETTE_STAGE_COUNT * 16U,
                     byte_count) ||
        !range_valid(reverse_palette_frames_offset,
                     TECMO_INTRO_FINALE_PALETTE_STAGE_COUNT *
                         FINALE_REVERSE_PALETTE_FRAME_STRIDE,
                     byte_count) ||
        !range_valid(groups_offset,
                     TECMO_INTRO_FINALE_SPRITE_PALETTE_COUNT * FINALE_GROUP_STRIDE,
                     byte_count) ||
        !range_valid(pieces_offset,
                     TECMO_INTRO_FINALE_PIECE_COUNT * FINALE_PIECE_STRIDE,
                     byte_count) ||
        !range_valid(routes_offset, FINALE_ROUTE_COUNT * FINALE_ROUTE_STRIDE, byte_count) ||
        !range_valid(anchors_offset,
                     FINALE_ANCHOR_COUNT * FINALE_ANCHOR_STRIDE,
                     byte_count) ||
        !range_valid(reverse_meta_offset, FINALE_REVERSE_META_SIZE, byte_count) ||
        !range_valid(title_meta_offset, FINALE_TITLE_META_SIZE, byte_count) ||
        !range_valid(title_slots_offset,
                     TECMO_INTRO_FINALE_TITLE_SLOT_COUNT * FINALE_TITLE_SLOT_STRIDE,
                     byte_count) ||
        !range_valid(bands_offset,
                     TECMO_INTRO_FINALE_TITLE_BAND_COUNT * FINALE_BAND_STRIDE,
                     byte_count)) {
        return false;
    }

    for (size_t screen = 0U; screen < TECMO_INTRO_FINALE_SCREEN_COUNT; ++screen) {
        const uint8_t *palette = bytes + background_palettes_offset + screen * 16U;
        if (!palette_valid(palette, 16U)) return false;
        for (size_t stage = 0U; stage < TECMO_INTRO_FINALE_PALETTE_STAGE_COUNT; ++stage) {
            memcpy(asset->palettes[screen][stage], palette, 16U);
        }
        asset->palette_stage_count[screen] = 1U;
        for (size_t i = 0U; i < TECMO_INTRO_FINALE_CELL_COUNT; ++i) {
            const uint8_t *cell = bytes + screens_offset +
                                  (screen * TECMO_INTRO_FINALE_CELL_COUNT + i) *
                                      FINALE_CELL_STRIDE;
            asset->screens[screen][i].tile_id = cell[0];
            asset->screens[screen][i].palette_index = cell[1];
            asset->screens[screen][i].chr_offset = read_u32(cell + 2U);
            if (cell[1] > 3U || (asset->screens[screen][i].chr_offset & 0x0FU) != 0U) {
                return false;
            }
        }
        if (screen == 0U || screen == 3U) {
            for (size_t i = 0U; i < TECMO_INTRO_FINALE_TILES_PER_PAGE; ++i) {
                const TecmoIntroFinaleCell *first = &asset->screens[screen][i];
                const TecmoIntroFinaleCell *second =
                    &asset->screens[screen][TECMO_INTRO_FINALE_TILES_PER_PAGE + i];
                if (first->tile_id != second->tile_id ||
                    first->palette_index != second->palette_index ||
                    first->chr_offset != second->chr_offset) return false;
            }
        }
    }
    if (!palette_valid(bytes + reverse_palettes_offset,
                       TECMO_INTRO_FINALE_PALETTE_STAGE_COUNT * 16U)) return false;
    memcpy(asset->palettes[2], bytes + reverse_palettes_offset,
           TECMO_INTRO_FINALE_PALETTE_STAGE_COUNT * 16U);
    asset->palette_stage_count[2] = TECMO_INTRO_FINALE_PALETTE_STAGE_COUNT;
    for (size_t stage = 0U; stage < TECMO_INTRO_FINALE_PALETTE_STAGE_COUNT; ++stage) {
        asset->reverse_palette_frames[stage] =
            read_u16(bytes + reverse_palette_frames_offset +
                     stage * FINALE_REVERSE_PALETTE_FRAME_STRIDE);
        if ((stage > 0U && asset->reverse_palette_frames[stage] <=
                              asset->reverse_palette_frames[stage - 1U]) ||
            asset->reverse_palette_frames[stage] >= FINALE_SELECTOR_DURATION) return false;
    }
    if (asset->reverse_palette_frames[0] != 10U ||
        asset->reverse_palette_frames[1] != 14U ||
        asset->reverse_palette_frames[2] != 18U ||
        asset->reverse_palette_frames[3] != 22U ||
        asset->reverse_palette_frames[4] != 27U) return false;

    for (size_t variant = 0U; variant < TECMO_INTRO_FINALE_SPRITE_PALETTE_COUNT;
         ++variant) {
        static const uint16_t expected_usage[TECMO_INTRO_FINALE_SPRITE_PALETTE_COUNT] = {
            5U, 2U
        };
        const uint8_t *group = bytes + groups_offset + variant * FINALE_GROUP_STRIDE;
        uint32_t group_pieces_offset = read_u32(group + 4U);
        uint32_t palette_offset = read_u32(group + 8U);
        if (group[0] != variant || group[1] != variant ||
            read_u16(group + 2U) != TECMO_INTRO_FINALE_PIECE_COUNT ||
            group_pieces_offset != pieces_offset ||
            palette_offset != FINALE_SPRITE_PALETTES_OFFSET + variant * 16U ||
            !range_valid(palette_offset, 16U, byte_count) ||
            read_u16(group + 12U) != expected_usage[variant] ||
            read_u16(group + 14U) != 0U ||
            !palette_valid(bytes + palette_offset, 16U)) return false;
        memcpy(asset->sprite_palettes[variant], bytes + palette_offset, 16U);
    }

    for (size_t i = 0U; i < TECMO_INTRO_FINALE_PIECE_COUNT; ++i) {
        const uint8_t *piece = bytes + pieces_offset + i * FINALE_PIECE_STRIDE;
        asset->pieces[i].dx = (int16_t)read_u16(piece + 0U);
        asset->pieces[i].dy = (int16_t)read_u16(piece + 2U);
        asset->pieces[i].top_chr_offset = read_u32(piece + 4U);
        asset->pieces[i].bottom_chr_offset = read_u32(piece + 8U);
        asset->pieces[i].palette_index = piece[12U];
        asset->pieces[i].flags = piece[13U];
        if (piece[12U] > 3U || (piece[13U] & ~3U) != 0U ||
            piece[14U] != 0U || piece[15U] != 0U ||
            (asset->pieces[i].top_chr_offset & 0x0FU) != 0U ||
            (asset->pieces[i].bottom_chr_offset & 0x0FU) != 0U ||
            asset->pieces[i].top_chr_offset > UINT32_MAX - 16U ||
            asset->pieces[i].bottom_chr_offset !=
                asset->pieces[i].top_chr_offset + 16U) return false;
    }

    {
        static const uint8_t expected_variants[FINALE_ROUTE_COUNT] = {
            0xFFU, 0U, 1U, 0U, 0xFFU
        };
        static const uint8_t expected_flags[FINALE_ROUTE_COUNT] = {
            0U, 0U, 0U, 0U, 1U
        };
        static const uint16_t expected_internal_frames[FINALE_ROUTE_COUNT] = {
            0U, 16U, 45U, 80U, 601U
        };
        static const uint16_t expected_dispatch_waits[FINALE_ROUTE_COUNT] = {
            50U, 30U, 0U, 75U, 1U
        };
        for (size_t route_index = 0U; route_index < FINALE_ROUTE_COUNT; ++route_index) {
            const uint8_t *route = bytes + routes_offset + route_index * FINALE_ROUTE_STRIDE;
            if (route[0] != route_index ||
                route[1] != expected_variants[route_index] || route[2] != route_index ||
                route[3] != expected_flags[route_index] ||
                read_u16(route + 4U) != expected_internal_frames[route_index] ||
                read_u16(route + 6U) != expected_dispatch_waits[route_index]) return false;
        }
    }

    for (size_t i = 0U; i < TECMO_INTRO_FINALE_SHORT_LOOP_STEPS; ++i) {
        const uint8_t *anchor = bytes + anchors_offset + i * FINALE_ANCHOR_STRIDE;
        asset->short_anchor_x[i] = anchor[0];
        asset->short_anchor_y[i] = anchor[1];
    }
    asset->staged_anchor_x =
        bytes[anchors_offset + TECMO_INTRO_FINALE_SHORT_LOOP_STEPS * FINALE_ANCHOR_STRIDE];
    asset->staged_anchor_y =
        bytes[anchors_offset + TECMO_INTRO_FINALE_SHORT_LOOP_STEPS * FINALE_ANCHOR_STRIDE + 1U];

    reverse_meta = bytes + reverse_meta_offset;
    asset->reverse_initial_x = reverse_meta[0];
    asset->reverse_second_x = reverse_meta[1];
    asset->reverse_delta_x = (int8_t)reverse_meta[2];
    asset->reverse_anchor_y = reverse_meta[3];
    if (asset->reverse_initial_x != 0x78U || asset->reverse_second_x != 0xD8U ||
        asset->reverse_delta_x != -8 || asset->reverse_anchor_y != 0x54U ||
        read_u16(reverse_meta + 4U) != TECMO_INTRO_FINALE_TRANSITION_IN_FRAMES ||
        read_u16(reverse_meta + 6U) != TECMO_INTRO_FINALE_TRANSITION_SWAP_FRAMES ||
        read_u16(reverse_meta + 8U) != TECMO_INTRO_FINALE_TRANSITION_OUT_FRAMES ||
        read_u16(reverse_meta + 10U) != TECMO_INTRO_FINALE_PALETTE_STAGE_COUNT ||
        read_u32(reverse_meta + 12U) != 0U) return false;

    title_meta = bytes + title_meta_offset;
    if (read_u16(title_meta + 0U) != TECMO_INTRO_FINALE_TITLE_PREROLL_FRAMES ||
        read_u16(title_meta + 2U) != TECMO_INTRO_FINALE_TITLE_SLOT_COUNT ||
        read_u16(title_meta + 4U) != 1U || read_u16(title_meta + 6U) != 7U ||
        read_u16(title_meta + 8U) != 301U ||
        read_u16(title_meta + 10U) != TECMO_INTRO_FINALE_TITLE_WRITE_FRAMES ||
        read_u16(title_meta + 12U) != TECMO_INTRO_FINALE_TITLE_TAIL_FRAMES ||
        read_u16(title_meta + 14U) != TECMO_INTRO_FINALE_TITLE_DISPATCH_WAIT_FRAMES ||
        read_u16(title_meta + 16U) != 2U || read_u16(title_meta + 18U) != 2U ||
        read_u16(title_meta + 20U) != 8U || read_u16(title_meta + 22U) != 1U ||
        read_u16(title_meta + 24U) != 16U || read_u16(title_meta + 26U) != 2U ||
        read_u16(title_meta + 28U) != 2U || read_u16(title_meta + 30U) != 0U) {
        return false;
    }

    for (size_t i = 0U; i < TECMO_INTRO_FINALE_TITLE_SLOT_COUNT; ++i) {
        const uint8_t *slot = bytes + title_slots_offset + i * FINALE_TITLE_SLOT_STRIDE;
        TecmoIntroFinaleTitleSlot *dest = &asset->title_slots[i];
        dest->page = slot[0];
        dest->column = slot[1];
        dest->row = slot[2];
        {
            unsigned render_x = (unsigned)((i + 16U) & 31U);
            uint8_t expected_page = render_x >= 16U ? 1U : 0U;
            uint8_t expected_column =
                (uint8_t)((render_x >= 16U ? render_x - 16U : render_x) * 2U);
            if (dest->page != expected_page || dest->column != expected_column ||
                dest->row != 16U) return false;
        }
        if (slot[3] != 0U || dest->page >= TECMO_INTRO_FINALE_PAGE_COUNT ||
            dest->column > TECMO_INTRO_FINALE_WIDTH - 2U ||
            dest->row > TECMO_INTRO_FINALE_HEIGHT - 2U) return false;
        for (size_t tile = 0U; tile < TECMO_INTRO_FINALE_TITLE_CELL_COUNT; ++tile) {
            const uint8_t *cell = slot + 4U + tile * FINALE_CELL_STRIDE;
            dest->cells[tile].tile_id = cell[0];
            dest->cells[tile].palette_index = cell[1];
            dest->cells[tile].chr_offset = read_u32(cell + 2U);
            if (cell[1] > 3U || (dest->cells[tile].chr_offset & 0x0FU) != 0U) return false;
        }
        if (read_u32(slot + 28U) != 0U) return false;
    }

    {
        static const uint16_t expected_starts[TECMO_INTRO_FINALE_TITLE_BAND_COUNT] = {
            0U, 200U, 223U
        };
        static const uint16_t expected_ends[TECMO_INTRO_FINALE_TITLE_BAND_COUNT] = {
            200U, 223U, 240U
        };
        for (size_t band_index = 0U; band_index < TECMO_INTRO_FINALE_TITLE_BAND_COUNT;
             ++band_index) {
            const uint8_t *band = bytes + bands_offset + band_index * FINALE_BAND_STRIDE;
            TecmoIntroFinaleTitleBand *dest = &asset->title_bands[band_index];
            dest->start_scanline = read_u16(band + 0U);
            dest->end_scanline = read_u16(band + 2U);
            dest->scroll_channel = band[4U];
            dest->page_channel = band[5U];
            dest->low_chr_base = read_u32(band + 8U);
            dest->high_chr_base = read_u32(band + 12U);
            if (dest->start_scanline != expected_starts[band_index] ||
                dest->end_scanline != expected_ends[band_index] ||
                dest->scroll_channel != band_index || dest->page_channel != band_index ||
                read_u16(band + 6U) != 0U ||
                (dest->low_chr_base & 0x3FFU) != 0U ||
                (dest->high_chr_base & 0x3FFU) != 0U) return false;
        }
    }
    return true;
}

bool tecmo_intro_finale_asset_load(TecmoIntroFinaleAsset *asset,
                                   const char *project_root)
{
    uint8_t *bytes = NULL;
    uint8_t *chr_bytes = NULL;
    uint64_t byte_count = 0U;
    uint64_t chr_byte_count = 0U;
    char pack_path[1024];

    if (asset == NULL) return false;
    memset(asset, 0, sizeof(*asset));
    pack_path[0] = '\0';
    if (!read_native_entry(project_root, &bytes, &byte_count,
                           pack_path, sizeof(pack_path))) {
        set_status(asset->status, sizeof(asset->status), "TFIN-1 asset unavailable");
        return false;
    }
    if (!parse_asset(asset, bytes, byte_count)) {
        free(bytes);
        set_status(asset->status, sizeof(asset->status), "TFIN-1 asset rejected");
        return false;
    }
    free(bytes);
    if (tecmo_asset_pack_read_entry(pack_path,
                                    "chr/all",
                                    &chr_bytes,
                                    &chr_byte_count) != 0 ||
        !tecmo_intro_finale_asset_chr_available(asset, chr_bytes, chr_byte_count)) {
        free(chr_bytes);
        set_status(asset->status, sizeof(asset->status),
                   "TFIN-1 chr/all contract rejected");
        return false;
    }
    asset->chr_byte_count = chr_byte_count;
    asset->chr_fingerprint = fnv1a64(chr_bytes, chr_byte_count);
    free(chr_bytes);
    asset->available = true;
    (void)snprintf(asset->status, sizeof(asset->status),
                   "TFIN-1 assetpack %s", pack_path);
    return true;
}

bool tecmo_intro_finale_asset_chr_available(const TecmoIntroFinaleAsset *asset,
                                            const uint8_t *chr_bytes,
                                            uint64_t chr_byte_count)
{
    if (asset == NULL || chr_bytes == NULL || chr_byte_count == 0U) {
        return false;
    }
    if (asset->chr_byte_count != 0U &&
        (asset->chr_byte_count != chr_byte_count ||
         asset->chr_fingerprint != fnv1a64(chr_bytes, chr_byte_count))) {
        return false;
    }
    for (size_t screen = 0U; screen < TECMO_INTRO_FINALE_SCREEN_COUNT; ++screen) {
        for (size_t i = 0U; i < TECMO_INTRO_FINALE_CELL_COUNT; ++i) {
            if (!chr_range_valid(asset->screens[screen][i].chr_offset,
                                 chr_byte_count,
                                 16U)) return false;
        }
    }
    for (size_t i = 0U; i < TECMO_INTRO_FINALE_PIECE_COUNT; ++i) {
        if (!chr_range_valid(asset->pieces[i].top_chr_offset, chr_byte_count, 16U) ||
            !chr_range_valid(asset->pieces[i].bottom_chr_offset, chr_byte_count, 16U)) {
            return false;
        }
    }
    for (size_t i = 0U; i < TECMO_INTRO_FINALE_TITLE_SLOT_COUNT; ++i) {
        for (size_t tile = 0U; tile < TECMO_INTRO_FINALE_TITLE_CELL_COUNT; ++tile) {
            if (!chr_range_valid(asset->title_slots[i].cells[tile].chr_offset,
                                 chr_byte_count,
                                 16U)) return false;
        }
    }
    for (size_t i = 0U; i < TECMO_INTRO_FINALE_TITLE_BAND_COUNT; ++i) {
        if (!chr_range_valid(asset->title_bands[i].low_chr_base,
                             chr_byte_count,
                             128U * 16U) ||
            !chr_range_valid(asset->title_bands[i].high_chr_base,
                             chr_byte_count,
                             128U * 16U)) return false;
    }
    return true;
}

unsigned tecmo_intro_finale_hold_frame(void)
{
    return FINALE_TITLE_HOLD_FRAME;
}

unsigned tecmo_intro_finale_scene_start_frame(TecmoIntroFinaleScene scene)
{
    switch (scene) {
        case TECMO_INTRO_FINALE_OPENING_SCREEN:
            return 0U;
        case TECMO_INTRO_FINALE_SHORT_SPRITE_LOOP:
            return FINALE_OPENING_DURATION;
        case TECMO_INTRO_FINALE_SELECTOR_TRANSITION:
            return FINALE_OPENING_DURATION + FINALE_SHORT_LOOP_DURATION;
        case TECMO_INTRO_FINALE_STAGED_GROUP:
            return FINALE_OPENING_DURATION + FINALE_SHORT_LOOP_DURATION +
                   FINALE_SELECTOR_DURATION;
        case TECMO_INTRO_FINALE_TITLE:
            return FINALE_OPENING_DURATION + FINALE_SHORT_LOOP_DURATION +
                   FINALE_SELECTOR_DURATION + FINALE_STAGED_DURATION;
        case TECMO_INTRO_FINALE_TERMINATOR_HOLD:
            return FINALE_TITLE_HOLD_FRAME;
        default:
            return FINALE_TITLE_HOLD_FRAME;
    }
}

unsigned tecmo_intro_finale_scene_duration(TecmoIntroFinaleScene scene)
{
    switch (scene) {
        case TECMO_INTRO_FINALE_OPENING_SCREEN:
            return FINALE_OPENING_DURATION;
        case TECMO_INTRO_FINALE_SHORT_SPRITE_LOOP:
            return FINALE_SHORT_LOOP_DURATION;
        case TECMO_INTRO_FINALE_SELECTOR_TRANSITION:
            return FINALE_SELECTOR_DURATION;
        case TECMO_INTRO_FINALE_STAGED_GROUP:
            return FINALE_STAGED_DURATION;
        case TECMO_INTRO_FINALE_TITLE:
            return FINALE_TITLE_HOLD_FRAME -
                   tecmo_intro_finale_scene_start_frame(TECMO_INTRO_FINALE_TITLE);
        case TECMO_INTRO_FINALE_TERMINATOR_HOLD:
        default:
            return 0U;
    }
}

static void set_scene_state(TecmoIntroFinaleState *state,
                            TecmoIntroFinaleScene scene,
                            uint8_t screen_index,
                            unsigned scene_frame)
{
    state->scene = scene;
    state->screen_index = screen_index;
    state->scene_frame = scene_frame;
}

void tecmo_intro_finale_state(const TecmoIntroFinaleAsset *asset,
                              unsigned frame,
                              TecmoIntroFinaleState *state)
{
    unsigned cursor = frame;
    unsigned elapsed;
    unsigned iterations;
    unsigned secondary_iterations = 0U;

    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->frame = frame;

    if (cursor < FINALE_OPENING_DURATION) {
        set_scene_state(state, TECMO_INTRO_FINALE_OPENING_SCREEN, 0U, cursor);
        state->phase = cursor < TECMO_INTRO_FINALE_LOAD_BOUNDARY_FRAMES
                           ? TECMO_INTRO_FINALE_PHASE_LOAD
                           : TECMO_INTRO_FINALE_PHASE_DISPATCH_WAIT;
        return;
    }
    cursor -= FINALE_OPENING_DURATION;

    if (cursor < FINALE_SHORT_LOOP_DURATION) {
        set_scene_state(state, TECMO_INTRO_FINALE_SHORT_SPRITE_LOOP, 1U, cursor);
        state->sprite_variant_index = 0U;
        if (cursor < TECMO_INTRO_FINALE_LOAD_BOUNDARY_FRAMES) {
            state->phase = TECMO_INTRO_FINALE_PHASE_LOAD;
            return;
        }
        elapsed = cursor - TECMO_INTRO_FINALE_LOAD_BOUNDARY_FRAMES;
        if (elapsed < TECMO_INTRO_FINALE_SHORT_LOOP_STEPS *
                          TECMO_INTRO_FINALE_SHORT_LOOP_STEP_FRAMES) {
            state->phase = TECMO_INTRO_FINALE_PHASE_SHORT_LOOP;
            state->short_loop_step =
                (uint8_t)(elapsed / TECMO_INTRO_FINALE_SHORT_LOOP_STEP_FRAMES);
            if (asset != NULL) {
                state->player_x = asset->short_anchor_x[state->short_loop_step];
                state->player_y = asset->short_anchor_y[state->short_loop_step];
            }
            state->sprites_visible = true;
        } else {
            state->phase = TECMO_INTRO_FINALE_PHASE_DISPATCH_WAIT;
            state->short_loop_step = TECMO_INTRO_FINALE_SHORT_LOOP_STEPS - 1U;
            if (asset != NULL) {
                state->player_x = asset->short_anchor_x[state->short_loop_step];
                state->player_y = asset->short_anchor_y[state->short_loop_step];
            }
            state->sprites_visible = true;
        }
        return;
    }
    cursor -= FINALE_SHORT_LOOP_DURATION;

    if (cursor < FINALE_SELECTOR_DURATION) {
        set_scene_state(state, TECMO_INTRO_FINALE_SELECTOR_TRANSITION, 2U, cursor);
        state->sprite_variant_index = 1U;
        if (asset != NULL) {
            state->player_y = asset->reverse_anchor_y;
            for (size_t stage = 1U; stage < TECMO_INTRO_FINALE_PALETTE_STAGE_COUNT;
                 ++stage) {
                if (cursor >= asset->reverse_palette_frames[stage]) {
                    state->palette_stage = (uint8_t)stage;
                }
            }
        }
        if (cursor == 0U) {
            state->phase = TECMO_INTRO_FINALE_PHASE_LOAD;
            state->black = true;
            return;
        }
        if (cursor < 8U) {
            state->phase = TECMO_INTRO_FINALE_PHASE_BLACK;
            state->black = true;
        } else if (cursor <= 25U) {
            uint8_t move_count = (uint8_t)(cursor - 7U);
            state->phase = TECMO_INTRO_FINALE_PHASE_FIRST_MOVE;
            if (asset != NULL) {
                state->player_x = (uint8_t)(asset->reverse_initial_x +
                                             move_count * asset->reverse_delta_x);
            }
            state->black = cursor < 10U;
            state->sprites_visible = !state->black;
        } else if (cursor == 26U) {
            state->phase = TECMO_INTRO_FINALE_PHASE_HOLD;
            if (asset != NULL) {
                state->player_x = (uint8_t)(asset->reverse_initial_x +
                                             18U * asset->reverse_delta_x);
            }
            state->sprites_visible = true;
        } else {
            uint8_t move_count = (uint8_t)(cursor - 26U);
            state->phase = TECMO_INTRO_FINALE_PHASE_SECOND_MOVE;
            if (asset != NULL) {
                state->player_x = (uint8_t)(asset->reverse_second_x +
                                             move_count * asset->reverse_delta_x);
                state->scroll_x = (uint8_t)(move_count * asset->reverse_delta_x);
            }
            state->sprites_visible = true;
        }
        return;
    }
    cursor -= FINALE_SELECTOR_DURATION;

    if (cursor < FINALE_STAGED_DURATION) {
        set_scene_state(state, TECMO_INTRO_FINALE_STAGED_GROUP, 3U, cursor);
        state->sprite_variant_index = 0U;
        if (asset != NULL) {
            state->player_x = asset->staged_anchor_x;
            state->player_y = asset->staged_anchor_y;
        }
        if (cursor < TECMO_INTRO_FINALE_LOAD_BOUNDARY_FRAMES) {
            state->phase = TECMO_INTRO_FINALE_PHASE_LOAD;
        } else if (cursor < TECMO_INTRO_FINALE_LOAD_BOUNDARY_FRAMES +
                            TECMO_INTRO_FINALE_STAGED_WAIT_FRAMES) {
            state->phase = TECMO_INTRO_FINALE_PHASE_STAGED_WAIT;
            state->sprites_visible = true;
        } else {
            state->phase = TECMO_INTRO_FINALE_PHASE_DISPATCH_WAIT;
            state->sprites_visible = true;
        }
        return;
    }
    cursor -= FINALE_STAGED_DURATION;
    set_scene_state(state, TECMO_INTRO_FINALE_TITLE, 4U, cursor);
    if (cursor < TECMO_INTRO_FINALE_LOAD_BOUNDARY_FRAMES) {
        state->phase = TECMO_INTRO_FINALE_PHASE_LOAD;
        return;
    }
    cursor -= TECMO_INTRO_FINALE_LOAD_BOUNDARY_FRAMES;
    if (cursor < TECMO_INTRO_FINALE_TITLE_PREROLL_FRAMES) {
        state->phase = TECMO_INTRO_FINALE_PHASE_TITLE_PREROLL;
        secondary_iterations = cursor;
        state->secondary_scroll_x = (uint8_t)(secondary_iterations * 2U);
        state->secondary_scroll_page =
            (uint8_t)(((secondary_iterations * 2U) >> 8U) & 1U);
        return;
    }
    cursor -= TECMO_INTRO_FINALE_TITLE_PREROLL_FRAMES;
    secondary_iterations = TECMO_INTRO_FINALE_TITLE_PREROLL_FRAMES;
    if (cursor < TECMO_INTRO_FINALE_TITLE_WRITE_FRAMES) {
        state->phase = TECMO_INTRO_FINALE_PHASE_TITLE_WRITE;
        state->title_slots_written =
            (uint8_t)(cursor / TECMO_INTRO_FINALE_TITLE_SLOT_INTERVAL_FRAMES + 1U);
        iterations = cursor + 1U;
    } else {
        state->title_slots_written = TECMO_INTRO_FINALE_TITLE_SLOT_COUNT;
        iterations = TECMO_INTRO_FINALE_TITLE_WRITE_FRAMES;
        cursor -= TECMO_INTRO_FINALE_TITLE_WRITE_FRAMES;
        if (cursor < TECMO_INTRO_FINALE_TITLE_TAIL_FRAMES) {
            state->phase = TECMO_INTRO_FINALE_PHASE_TITLE_TAIL;
            secondary_iterations += cursor;
        } else {
            cursor -= TECMO_INTRO_FINALE_TITLE_TAIL_FRAMES;
            secondary_iterations += TECMO_INTRO_FINALE_TITLE_TAIL_FRAMES;
            if (cursor < TECMO_INTRO_FINALE_TITLE_DISPATCH_WAIT_FRAMES) {
                state->phase = TECMO_INTRO_FINALE_PHASE_DISPATCH_WAIT;
            } else {
                state->scene = TECMO_INTRO_FINALE_TERMINATOR_HOLD;
                state->scene_frame = frame - FINALE_TITLE_HOLD_FRAME;
                state->phase = TECMO_INTRO_FINALE_PHASE_TERMINATOR_HOLD;
                state->persistent_hold = true;
            }
        }
    }
    state->scroll_x = (uint8_t)(iterations * 2U);
    state->scroll_page = (uint8_t)(((iterations * 2U) >> 8U) & 1U);
    state->secondary_scroll_x = (uint8_t)(secondary_iterations * 2U);
    state->secondary_scroll_page =
        (uint8_t)(((secondary_iterations * 2U) >> 8U) & 1U);
}

const char *tecmo_intro_finale_scene_name(TecmoIntroFinaleScene scene)
{
    switch (scene) {
        case TECMO_INTRO_FINALE_OPENING_SCREEN: return "opening-screen";
        case TECMO_INTRO_FINALE_SHORT_SPRITE_LOOP: return "short-sprite-loop";
        case TECMO_INTRO_FINALE_SELECTOR_TRANSITION: return "selector-transition";
        case TECMO_INTRO_FINALE_STAGED_GROUP: return "staged-group";
        case TECMO_INTRO_FINALE_TITLE: return "title";
        case TECMO_INTRO_FINALE_TERMINATOR_HOLD: return "terminator-hold";
        default: return "unknown";
    }
}

const char *tecmo_intro_finale_phase_name(TecmoIntroFinalePhase phase)
{
    switch (phase) {
        case TECMO_INTRO_FINALE_PHASE_LOAD: return "load";
        case TECMO_INTRO_FINALE_PHASE_DISPATCH_WAIT: return "dispatch-wait";
        case TECMO_INTRO_FINALE_PHASE_SHORT_LOOP: return "short-loop";
        case TECMO_INTRO_FINALE_PHASE_BLACK: return "black";
        case TECMO_INTRO_FINALE_PHASE_FIRST_MOVE: return "first-move";
        case TECMO_INTRO_FINALE_PHASE_HOLD: return "hold";
        case TECMO_INTRO_FINALE_PHASE_SECOND_MOVE: return "second-move";
        case TECMO_INTRO_FINALE_PHASE_STAGED_WAIT: return "staged-wait";
        case TECMO_INTRO_FINALE_PHASE_TITLE_PREROLL: return "title-preroll";
        case TECMO_INTRO_FINALE_PHASE_TITLE_WRITE: return "title-write";
        case TECMO_INTRO_FINALE_PHASE_TITLE_TAIL: return "title-tail";
        case TECMO_INTRO_FINALE_PHASE_TERMINATOR_HOLD: return "terminator-hold";
        default: return "unknown";
    }
}

static void fill_rect(TecmoFramebuffer *fb, int x, int y, int w, int h, uint32_t color)
{
    int x0 = x;
    int y0 = y;
    int x1 = x + w;
    int y1 = y + h;
    if (fb == NULL || fb->pixels == NULL || w <= 0 || h <= 0) return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > fb->width) x1 = fb->width;
    if (y1 > fb->height) y1 = fb->height;
    if (x0 >= x1 || y0 >= y1) return;
    for (int row = y0; row < y1; ++row) {
        uint32_t *pixel = fb->pixels + (size_t)row * (size_t)fb->pitch_pixels +
                          (size_t)x0;
        for (int col = x0; col < x1; ++col) *pixel++ = color;
    }
}

static void make_bg_palette(uint32_t rgba[4], const uint8_t palette[16], uint8_t index)
{
    uint8_t base = (uint8_t)((index & 3U) * 4U);
    rgba[0] = tecmo_nes_2c02_rgba(palette[0]);
    for (size_t i = 1U; i < 4U; ++i) {
        rgba[i] = tecmo_nes_2c02_rgba(palette[base + i]);
    }
}

static void make_sprite_palette(uint32_t rgba[4], const uint8_t palette[16], uint8_t index)
{
    uint8_t base = (uint8_t)((index & 3U) * 4U);
    rgba[0] = 0U;
    for (size_t i = 1U; i < 4U; ++i) rgba[i] = tecmo_nes_2c02_rgba(palette[base + i]);
}

static const TecmoIntroFinaleCell *title_overlay_cell(
    const TecmoIntroFinaleAsset *asset,
    uint8_t slots_written,
    unsigned page,
    unsigned row,
    unsigned col)
{
    const TecmoIntroFinaleCell *found = NULL;
    for (size_t slot_index = 0U; slot_index < slots_written; ++slot_index) {
        const TecmoIntroFinaleTitleSlot *slot = &asset->title_slots[slot_index];
        if (slot->page != page || row < slot->row || row >= slot->row + 2U ||
            col < slot->column || col >= slot->column + 2U) continue;
        found = &slot->cells[(row - slot->row) * 2U + col - slot->column];
    }
    return found;
}

static const TecmoIntroFinaleTitleBand *title_band_for_scanline(
    const TecmoIntroFinaleAsset *asset,
    unsigned scanline)
{
    for (size_t i = 0U; i < TECMO_INTRO_FINALE_TITLE_BAND_COUNT; ++i) {
        const TecmoIntroFinaleTitleBand *band = &asset->title_bands[i];
        if (scanline >= band->start_scanline && scanline < band->end_scanline) {
            return band;
        }
    }
    return NULL;
}

static bool draw_background(TecmoFramebuffer *fb,
                            const TecmoIntroFinaleAsset *asset,
                            const uint8_t *chr_bytes,
                            uint64_t chr_byte_count,
                            const TecmoIntroFinaleState *state,
                            int origin_x,
                            int origin_y,
                            int scale)
{
    const uint8_t *palette = asset->palettes[state->screen_index][state->palette_stage];
    fill_rect(fb, origin_x, origin_y, 256 * scale, 240 * scale,
              tecmo_nes_2c02_rgba(state->black ? 0x0FU : palette[0]));
    if (state->black) return true;

    for (unsigned scanline = 0U; scanline < 240U; ++scanline) {
        const TecmoIntroFinaleTitleBand *title_band =
            state->screen_index == 4U ? title_band_for_scanline(asset, scanline) : NULL;
        unsigned scroll_x = title_band == NULL || title_band->scroll_channel == 0U
                                ? state->scroll_x
                                : (title_band->scroll_channel == 1U
                                       ? state->secondary_scroll_x
                                       : 0U);
        unsigned scroll_page = title_band == NULL || title_band->page_channel == 0U
                                   ? state->scroll_page
                                   : (title_band->page_channel == 1U
                                          ? state->secondary_scroll_page
                                          : 0U);
        bool first_reverse_scroll = state->scene == TECMO_INTRO_FINALE_SELECTOR_TRANSITION &&
                                    state->scene_frame == 27U;
        unsigned row = scanline / 8U;
        unsigned tile_scanline = scanline & 7U;
        for (unsigned x = 0U; x < 256U; ++x) {
            unsigned source_x = x + scroll_x +
                                (first_reverse_scroll
                                     ? FINALE_REVERSE_FIRST_SCROLL_ADJUST
                                     : 0U);
            unsigned page = first_reverse_scroll
                                ? FINALE_REVERSE_FIRST_VIRTUAL_PAGE
                                : ((source_x >> 8U) + scroll_page) & 1U;
            unsigned page_x = source_x & 0xFFU;
            unsigned col = page_x / 8U;
            unsigned tile_col = page_x & 7U;
            const TecmoIntroFinaleCell *cell =
                &asset->screens[state->screen_index]
                               [page * TECMO_INTRO_FINALE_TILES_PER_PAGE +
                                row * TECMO_INTRO_FINALE_WIDTH + col];
            uint32_t rgba[4];
            uint8_t plane0;
            uint8_t plane1;
            uint8_t bit;
            uint8_t value;

            if (state->screen_index == 4U && state->title_slots_written > 0U) {
                const TecmoIntroFinaleCell *overlay =
                    title_overlay_cell(asset, state->title_slots_written, page, row, col);
                if (overlay != NULL) cell = overlay;
            }
            {
                uint32_t chr_offset = cell->chr_offset;
                if (title_band != NULL) {
                    chr_offset = (cell->tile_id < 0x80U ? title_band->low_chr_base
                                                        : title_band->high_chr_base) +
                                 (uint32_t)(cell->tile_id & 0x7FU) * 16U;
                }
                if (!chr_range_valid(chr_offset, chr_byte_count, 16U)) return false;
                plane0 = chr_bytes[chr_offset + tile_scanline];
                plane1 = chr_bytes[chr_offset + tile_scanline + 8U];
            }
            make_bg_palette(rgba, palette, cell->palette_index);
            bit = (uint8_t)(7U - tile_col);
            value = (uint8_t)(((plane0 >> bit) & 1U) |
                              (((plane1 >> bit) & 1U) << 1U));
            fill_rect(fb,
                      origin_x + (int)x * scale,
                      origin_y + (int)scanline * scale,
                      scale,
                      scale,
                      rgba[value]);
        }
    }
    return true;
}

static bool draw_pieces(TecmoFramebuffer *fb,
                        const TecmoIntroFinaleAsset *asset,
                        const uint8_t *chr_bytes,
                        uint64_t chr_byte_count,
                        const TecmoIntroFinaleState *state,
                        int origin_x,
                        int origin_y,
                        int scale)
{
    for (size_t i = 0U; i < TECMO_INTRO_FINALE_PIECE_COUNT; ++i) {
        const TecmoIntroFinalePiece *piece = &asset->pieces[i];
        bool flip_x = (piece->flags & 1U) != 0U;
        bool flip_y = (piece->flags & 2U) != 0U;
        uint32_t top = flip_y ? piece->bottom_chr_offset : piece->top_chr_offset;
        uint32_t bottom = flip_y ? piece->top_chr_offset : piece->bottom_chr_offset;
        uint32_t rgba[4];
        int x = origin_x + ((int)state->player_x + piece->dx) * scale;
        int y = origin_y + ((int)state->player_y + piece->dy) * scale;
        if (!chr_range_valid(top, chr_byte_count, 16U) ||
            !chr_range_valid(bottom, chr_byte_count, 16U)) return false;
        make_sprite_palette(rgba,
                            asset->sprite_palettes[state->sprite_variant_index],
                            piece->palette_index);
        tecmo_draw_chr_tile_at_offset_ex(fb, chr_bytes, chr_byte_count, top,
                                         x, y, scale, rgba, flip_x, flip_y);
        tecmo_draw_chr_tile_at_offset_ex(fb, chr_bytes, chr_byte_count, bottom,
                                         x, y + 8 * scale, scale, rgba, flip_x, flip_y);
    }
    return true;
}

bool tecmo_intro_finale_draw(TecmoFramebuffer *fb,
                             const TecmoIntroFinaleAsset *asset,
                             const uint8_t *chr_bytes,
                             uint64_t chr_byte_count,
                             unsigned frame,
                             int origin_x,
                             int origin_y,
                             int scale)
{
    TecmoIntroFinaleState state;
    if (fb == NULL || asset == NULL || !asset->available || chr_bytes == NULL ||
        scale <= 0) return false;
    tecmo_intro_finale_state(asset, frame, &state);
    if (!draw_background(fb, asset, chr_bytes, chr_byte_count,
                         &state, origin_x, origin_y, scale)) return false;
    if (state.sprites_visible &&
        !draw_pieces(fb, asset, chr_bytes, chr_byte_count,
                     &state, origin_x, origin_y, scale)) return false;
    return true;
}
