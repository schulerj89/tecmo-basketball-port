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
#define FINALE_SEMANTIC_HEADER_OFFSET 116U
#define FINALE_SEMANTIC_MAGIC "TFM1"
#define FINALE_SEMANTIC_ROUTE_DURATION_OFFSET 120U
#define FINALE_SEMANTIC_ROUTE_GATE_OFFSET 130U
#define FINALE_SEMANTIC_ROUTE_PULSE_OFFSET 135U
#define FINALE_SEMANTIC_ROUTE_TAIL_OFFSET 140U
#define FINALE_SEMANTIC_CAPTION_COUNT_OFFSET 145U
#define FINALE_SEMANTIC_CAPTION_COLUMN_OFFSET 146U
#define FINALE_SEMANTIC_CAPTION_ROW_OFFSET 147U
#define FINALE_SEMANTIC_CAPTION_INTERVAL_OFFSET 148U
#define FINALE_SEMANTIC_CAPTION_ROUTE_OFFSET 149U
#define FINALE_SEMANTIC_CAPTION_FIRST_FRAME_OFFSET 152U
#define FINALE_SEMANTIC_CAPTION_PALETTE_OFFSET 155U
#define FINALE_SEMANTIC_CAPTION_GLYPH_COUNT_OFFSET 158U
#define FINALE_SEMANTIC_CAPTION_REF_OFFSET 161U
#define FINALE_SEMANTIC_EXTRA_GLYPH_OFFSET 176U
#define FINALE_SEMANTIC_STAGED_TEAM_COLOR_OFFSET 180U
#define FINALE_SEMANTIC_RESERVED_OFFSET 181U

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

_Static_assert(FINALE_SCREENS_OFFSET == FINALE_HEADER_SIZE,
               "TFIN screens must begin after the complete 192-byte header");

#define FINALE_OPENING_DURATION TECMO_INTRO_FINALE_OPENING_DURATION_FRAMES
#define FINALE_SHORT_LOOP_DURATION TECMO_INTRO_FINALE_SHORT_LOOP_DURATION_FRAMES
#define FINALE_SELECTOR_DURATION TECMO_INTRO_FINALE_SELECTOR_DURATION_FRAMES
#define FINALE_STAGED_DURATION TECMO_INTRO_FINALE_STAGED_DURATION_FRAMES
#define FINALE_TITLE_DURATION TECMO_INTRO_FINALE_TITLE_DURATION_FRAMES
#define FINALE_TITLE_HOLD_FRAME TECMO_INTRO_FINALE_HOLD_FRAME

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

static bool parse_semantic_header(TecmoIntroFinaleAsset *asset,
                                  const uint8_t *bytes)
{
    static const uint16_t expected_durations[FINALE_ROUTE_COUNT] = {
        84U, 59U, 52U, 189U, 617U
    };
    static const uint8_t expected_gates[FINALE_ROUTE_COUNT] = {
        7U, 6U, 8U, 7U, 15U
    };
    static const uint8_t expected_pulses[FINALE_ROUTE_COUNT] = {
        0xFFU, 0xFFU, 24U, 0xFFU, 0xFFU
    };
    static const uint8_t expected_tails[FINALE_ROUTE_COUNT] = {
        83U, 58U, 51U, 0xFFU, 0xFFU
    };
    static const uint8_t expected_caption_routes[TECMO_INTRO_FINALE_CAPTION_COUNT] = {
        0U, 1U, 3U
    };
    static const uint8_t expected_caption_first_frames[TECMO_INTRO_FINALE_CAPTION_COUNT] = {
        29U, 7U, 29U
    };
    static const uint8_t expected_caption_counts[TECMO_INTRO_FINALE_CAPTION_COUNT] = {
        4U, 5U, 5U
    };
    static const uint8_t expected_caption_refs[TECMO_INTRO_FINALE_CAPTION_COUNT]
                                               [TECMO_INTRO_FINALE_CAPTION_MAX_GLYPHS] = {
        {6U, 7U, TECMO_INTRO_FINALE_CAPTION_GLYPH_SENTINEL, 6U,
         TECMO_INTRO_FINALE_CAPTION_GLYPH_SENTINEL},
        {6U, 8U, 7U, 10U, 6U},
        {16U, 7U, 24U, 24U, 6U}
    };
    static const uint8_t expected_extra_glyph[TECMO_INTRO_FINALE_TITLE_CELL_COUNT] = {
        0xD2U, 0xD6U, 0xD4U, 0xD7U
    };

    if (asset == NULL || bytes == NULL ||
        memcmp(bytes + FINALE_SEMANTIC_HEADER_OFFSET,
               FINALE_SEMANTIC_MAGIC, 4U) != 0) {
        return false;
    }
    for (size_t route = 0U; route < FINALE_ROUTE_COUNT; ++route) {
        asset->route_duration_frames[route] =
            read_u16(bytes + FINALE_SEMANTIC_ROUTE_DURATION_OFFSET + route * 2U);
        asset->route_black_gate_frames[route] =
            bytes[FINALE_SEMANTIC_ROUTE_GATE_OFFSET + route];
        asset->route_black_pulse_frames[route] =
            bytes[FINALE_SEMANTIC_ROUTE_PULSE_OFFSET + route];
        asset->route_black_tail_frames[route] =
            bytes[FINALE_SEMANTIC_ROUTE_TAIL_OFFSET + route];
        if (asset->route_duration_frames[route] != expected_durations[route] ||
            asset->route_black_gate_frames[route] != expected_gates[route] ||
            asset->route_black_pulse_frames[route] != expected_pulses[route] ||
            asset->route_black_tail_frames[route] != expected_tails[route]) {
            return false;
        }
    }
    if (bytes[FINALE_SEMANTIC_CAPTION_COUNT_OFFSET] !=
            TECMO_INTRO_FINALE_CAPTION_COUNT ||
        bytes[FINALE_SEMANTIC_CAPTION_COLUMN_OFFSET] != 12U ||
        bytes[FINALE_SEMANTIC_CAPTION_ROW_OFFSET] != 26U ||
        bytes[FINALE_SEMANTIC_CAPTION_INTERVAL_OFFSET] != 1U) {
        return false;
    }
    for (size_t caption = 0U;
         caption < TECMO_INTRO_FINALE_CAPTION_COUNT; ++caption) {
        TecmoIntroFinaleCaption *dest = &asset->captions[caption];
        dest->route_index = bytes[FINALE_SEMANTIC_CAPTION_ROUTE_OFFSET + caption];
        dest->first_frame =
            bytes[FINALE_SEMANTIC_CAPTION_FIRST_FRAME_OFFSET + caption];
        dest->reveal_interval = bytes[FINALE_SEMANTIC_CAPTION_INTERVAL_OFFSET];
        dest->palette_index =
            bytes[FINALE_SEMANTIC_CAPTION_PALETTE_OFFSET + caption];
        dest->column = bytes[FINALE_SEMANTIC_CAPTION_COLUMN_OFFSET];
        dest->row = bytes[FINALE_SEMANTIC_CAPTION_ROW_OFFSET];
        dest->glyph_count =
            bytes[FINALE_SEMANTIC_CAPTION_GLYPH_COUNT_OFFSET + caption];
        if (dest->route_index != expected_caption_routes[caption] ||
            dest->first_frame != expected_caption_first_frames[caption] ||
            dest->glyph_count != expected_caption_counts[caption] ||
            dest->palette_index > 3U || dest->column > 30U || dest->row > 28U) {
            return false;
        }
        for (size_t glyph = 0U;
             glyph < TECMO_INTRO_FINALE_CAPTION_MAX_GLYPHS; ++glyph) {
            uint8_t ref = bytes[FINALE_SEMANTIC_CAPTION_REF_OFFSET +
                                caption * TECMO_INTRO_FINALE_CAPTION_MAX_GLYPHS + glyph];
            dest->glyph_refs[glyph] = ref;
            if (ref != expected_caption_refs[caption][glyph]) return false;
            if (glyph < dest->glyph_count) {
                if (ref != TECMO_INTRO_FINALE_CAPTION_GLYPH_SENTINEL &&
                    ref >= TECMO_INTRO_FINALE_TITLE_SLOT_COUNT) {
                    return false;
                }
            } else if (ref != TECMO_INTRO_FINALE_CAPTION_GLYPH_SENTINEL) {
                return false;
            }
        }
    }
    memcpy(asset->caption_extra_glyph_tiles,
           bytes + FINALE_SEMANTIC_EXTRA_GLYPH_OFFSET,
           TECMO_INTRO_FINALE_TITLE_CELL_COUNT);
    if (memcmp(asset->caption_extra_glyph_tiles, expected_extra_glyph,
               TECMO_INTRO_FINALE_TITLE_CELL_COUNT) != 0) return false;
    if (bytes[FINALE_SEMANTIC_STAGED_TEAM_COLOR_OFFSET] != 0x15U) return false;
    asset->staged_team_color = bytes[FINALE_SEMANTIC_STAGED_TEAM_COLOR_OFFSET];
    for (size_t pad = FINALE_SEMANTIC_RESERVED_OFFSET;
         pad < FINALE_HEADER_SIZE; ++pad) {
        if (bytes[pad] != 0U) return false;
    }
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
    if (!parse_semantic_header(asset, bytes)) return false;

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
    /* Bank04 screen $2D supplies the ordinary two-page title band: page 0
       is black at row 18 and page 1 supplies the two-line baseline. */
    for (size_t col = 0U; col < TECMO_INTRO_FINALE_WIDTH; ++col) {
        const TecmoIntroFinaleCell *page0 =
            &asset->screens[4U][18U * TECMO_INTRO_FINALE_WIDTH + col];
        const TecmoIntroFinaleCell *page1 =
            &asset->screens[4U][TECMO_INTRO_FINALE_TILES_PER_PAGE +
                                18U * TECMO_INTRO_FINALE_WIDTH + col];
        if (page0->tile_id != 0xFFU || page0->palette_index != 2U ||
            page1->tile_id != 0xF1U || page1->palette_index != 2U) {
            return false;
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
    if (asset->reverse_palette_frames[0] != 8U ||
        asset->reverse_palette_frames[1] != 12U ||
        asset->reverse_palette_frames[2] != 16U ||
        asset->reverse_palette_frames[3] != 20U ||
        asset->reverse_palette_frames[4] != 25U) return false;

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
    asset->title_secondary_initial_page = (uint8_t)read_u16(title_meta + 30U);

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
            0U, 144U, 152U
        };
        static const uint16_t expected_ends[TECMO_INTRO_FINALE_TITLE_BAND_COUNT] = {
            144U, 152U, 240U
        };
        static const uint8_t expected_channels[TECMO_INTRO_FINALE_TITLE_BAND_COUNT] = {
            0U, 1U, 0U
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
                dest->scroll_channel != expected_channels[band_index] ||
                dest->page_channel != expected_channels[band_index] ||
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
    for (size_t col = 0U; col < TECMO_INTRO_FINALE_WIDTH; ++col) {
        const TecmoIntroFinaleCell *page0 =
            &asset->screens[4U][18U * TECMO_INTRO_FINALE_WIDTH + col];
        const TecmoIntroFinaleCell *page1 =
            &asset->screens[4U][TECMO_INTRO_FINALE_TILES_PER_PAGE +
                                18U * TECMO_INTRO_FINALE_WIDTH + col];
        if (page0->tile_id != 0xFFU || page0->palette_index != 2U ||
            page1->tile_id != 0xF1U || page1->palette_index != 2U ||
            !chr_range_valid(page0->chr_offset, chr_byte_count, 16U) ||
            !chr_range_valid(page1->chr_offset, chr_byte_count, 16U) ||
            chr_bytes[page0->chr_offset + 3U] != 0U ||
            chr_bytes[page0->chr_offset + 4U] != 0U ||
            chr_bytes[page0->chr_offset + 11U] != 0U ||
            chr_bytes[page0->chr_offset + 12U] != 0U ||
            chr_bytes[page1->chr_offset + 3U] != 0xFFU ||
            chr_bytes[page1->chr_offset + 4U] != 0xFFU ||
            chr_bytes[page1->chr_offset + 11U] != 0xFFU ||
            chr_bytes[page1->chr_offset + 12U] != 0xFFU) {
            return false;
        }
    }
    for (size_t i = 0U; i < TECMO_INTRO_FINALE_TITLE_CELL_COUNT; ++i) {
        uint8_t tile = asset->caption_extra_glyph_tiles[i];
        uint32_t chr_offset = (tile < 0x80U
                                   ? asset->title_bands[0].low_chr_base
                                   : asset->title_bands[0].high_chr_base) +
                              (uint32_t)(tile & 0x7FU) * 16U;
        if (!chr_range_valid(chr_offset, chr_byte_count, 16U)) return false;
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

static uint8_t finale_route_gate(const TecmoIntroFinaleAsset *asset,
                                 unsigned route)
{
    static const uint8_t defaults[FINALE_ROUTE_COUNT] = {7U, 6U, 8U, 7U, 15U};
    if (asset != NULL && route < FINALE_ROUTE_COUNT &&
        asset->route_black_gate_frames[route] != 0U) {
        return asset->route_black_gate_frames[route];
    }
    return defaults[route < FINALE_ROUTE_COUNT ? route : 0U];
}

static uint8_t finale_route_pulse(const TecmoIntroFinaleAsset *asset,
                                  unsigned route)
{
    static const uint8_t defaults[FINALE_ROUTE_COUNT] = {
        0xFFU, 0xFFU, 24U, 0xFFU, 0xFFU
    };
    if (asset != NULL && route < FINALE_ROUTE_COUNT &&
        asset->route_black_pulse_frames[route] != 0U) {
        return asset->route_black_pulse_frames[route];
    }
    return defaults[route < FINALE_ROUTE_COUNT ? route : 0U];
}

static uint8_t finale_route_tail(const TecmoIntroFinaleAsset *asset,
                                 unsigned route)
{
    static const uint8_t defaults[FINALE_ROUTE_COUNT] = {
        83U, 58U, 51U, 0xFFU, 0xFFU
    };
    if (asset != NULL && route < FINALE_ROUTE_COUNT &&
        asset->route_black_tail_frames[route] != 0U) {
        return asset->route_black_tail_frames[route];
    }
    return defaults[route < FINALE_ROUTE_COUNT ? route : 0U];
}

static bool finale_route_is_black(const TecmoIntroFinaleAsset *asset,
                                  unsigned route,
                                  unsigned scene_frame)
{
    uint8_t pulse = finale_route_pulse(asset, route);
    uint8_t tail = finale_route_tail(asset, route);
    return scene_frame < finale_route_gate(asset, route) ||
           (pulse != 0xFFU && scene_frame == pulse) ||
           (tail != 0xFFU && scene_frame == tail);
}

static uint8_t finale_title_initial_page(const TecmoIntroFinaleAsset *asset)
{
    if (asset != NULL && asset->title_secondary_initial_page < 2U) {
        return asset->title_secondary_initial_page;
    }
    return 0U;
}

static void finale_set_title_scroll(TecmoIntroFinaleState *state,
                                    unsigned primary_iterations,
                                    unsigned secondary_iterations,
                                    uint8_t secondary_initial_page)
{
    unsigned primary_scroll = primary_iterations * 2U;
    unsigned secondary_scroll = secondary_iterations * 2U;
    state->scroll_x = (uint8_t)primary_scroll;
    state->scroll_page = (uint8_t)((primary_scroll >> 8U) & 1U);
    state->secondary_scroll_x = (uint8_t)secondary_scroll;
    state->secondary_scroll_page = (uint8_t)(secondary_initial_page ^
                                             ((secondary_scroll >> 8U) & 1U));
}

void tecmo_intro_finale_state(const TecmoIntroFinaleAsset *asset,
                              unsigned frame,
                              TecmoIntroFinaleState *state)
{
    unsigned cursor = frame;
    unsigned elapsed;
    unsigned iterations;
    unsigned secondary_iterations;
    uint8_t black_gate;

    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->frame = frame;

    if (frame >= FINALE_TITLE_HOLD_FRAME) {
        state->scene = TECMO_INTRO_FINALE_TERMINATOR_HOLD;
        state->screen_index = 4U;
        state->scene_frame = frame - FINALE_TITLE_HOLD_FRAME;
        state->phase = TECMO_INTRO_FINALE_PHASE_TERMINATOR_HOLD;
        state->title_slots_written = TECMO_INTRO_FINALE_TITLE_SLOT_COUNT;
        state->persistent_hold = true;
        finale_set_title_scroll(state, TECMO_INTRO_FINALE_TITLE_WRITE_FRAMES,
                                TECMO_INTRO_FINALE_TITLE_PREROLL_FRAMES +
                                    TECMO_INTRO_FINALE_TITLE_TAIL_FRAMES,
                                finale_title_initial_page(asset));
        return;
    }

    if (cursor < FINALE_OPENING_DURATION) {
        set_scene_state(state, TECMO_INTRO_FINALE_OPENING_SCREEN, 0U, cursor);
        black_gate = finale_route_gate(asset, 0U);
        state->phase = cursor == 0U ? TECMO_INTRO_FINALE_PHASE_LOAD
                                    : (cursor < black_gate
                                           ? TECMO_INTRO_FINALE_PHASE_BLACK
                                           : TECMO_INTRO_FINALE_PHASE_DISPATCH_WAIT);
        state->black = finale_route_is_black(asset, 0U, cursor);
        return;
    }
    cursor -= FINALE_OPENING_DURATION;

    if (cursor < FINALE_SHORT_LOOP_DURATION) {
        set_scene_state(state, TECMO_INTRO_FINALE_SHORT_SPRITE_LOOP, 1U, cursor);
        state->sprite_variant_index = 0U;
        /* Bank04 $83EA renders the decoded screen-$20 page-1 nametable. */
        state->scroll_page = 1U;
        black_gate = finale_route_gate(asset, 1U);
        state->black = finale_route_is_black(asset, 1U, cursor);
        if (cursor == 0U) {
            state->phase = TECMO_INTRO_FINALE_PHASE_LOAD;
            return;
        }
        if (cursor < black_gate) {
            state->phase = TECMO_INTRO_FINALE_PHASE_BLACK;
            return;
        }
        elapsed = cursor - black_gate;
        if (elapsed < 8U) {
            state->phase = TECMO_INTRO_FINALE_PHASE_SHORT_LOOP;
            state->short_loop_step = 0U;
            if (asset != NULL) {
                state->player_x = asset->short_anchor_x[state->short_loop_step];
                state->player_y = asset->short_anchor_y[state->short_loop_step];
            }
        } else if (elapsed < 22U) {
            state->phase = TECMO_INTRO_FINALE_PHASE_SHORT_LOOP;
            /* The setup consumes four C000 calls; the 14-frame displayed
               animation then advances its duplicated two-frame anchor
               entries through local 27. */
            state->short_loop_step = (uint8_t)((elapsed - 8U) / 2U);
            if (asset != NULL) {
                state->player_x = asset->short_anchor_x[state->short_loop_step];
                state->player_y = asset->short_anchor_y[state->short_loop_step];
            }
        } else {
            state->phase = TECMO_INTRO_FINALE_PHASE_DISPATCH_WAIT;
            state->short_loop_step = TECMO_INTRO_FINALE_SHORT_LOOP_STEPS - 1U;
            if (asset != NULL) {
                state->player_x = asset->short_anchor_x[state->short_loop_step];
                state->player_y = asset->short_anchor_y[state->short_loop_step];
            }
        }
        state->sprites_visible = !state->black && elapsed >= 8U && elapsed < 22U;
        return;
    }
    cursor -= FINALE_SHORT_LOOP_DURATION;

    if (cursor < FINALE_SELECTOR_DURATION) {
        set_scene_state(state, TECMO_INTRO_FINALE_SELECTOR_TRANSITION, 2U, cursor);
        state->sprite_variant_index = 1U;
        state->black = finale_route_is_black(asset, 2U, cursor);
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
            return;
        }
        if (cursor < 7U) {
            state->phase = TECMO_INTRO_FINALE_PHASE_BLACK;
        } else if (cursor < 24U) {
            /* The setup frame at local 7 has count one in RAM but remains
               hidden by the eight-frame route gate.  Visible frames 8..23
               are counts 2..17. */
            uint8_t move_count = (uint8_t)(cursor - 6U);
            state->phase = TECMO_INTRO_FINALE_PHASE_FIRST_MOVE;
            if (asset != NULL) {
                state->player_x = (uint8_t)(asset->reverse_initial_x +
                                             move_count * asset->reverse_delta_x);
            }
        } else if (cursor == 24U) {
            state->phase = TECMO_INTRO_FINALE_PHASE_BLACK;
            if (asset != NULL) {
                state->player_x = (uint8_t)(asset->reverse_initial_x +
                                             18U * asset->reverse_delta_x);
            }
        } else if (cursor == 25U) {
            /* The page swap/hold is a single visible frame.  It retains
               x=$E8 and selects repeated water page 1 with scroll zero. */
            state->phase = TECMO_INTRO_FINALE_PHASE_HOLD;
            state->scroll_page = 1U;
            if (asset != NULL) {
                state->player_x = (uint8_t)(asset->reverse_initial_x +
                                             18U * asset->reverse_delta_x);
            }
        } else if (cursor <= 51U) {
            /* L85E6 updates $0300/$07D7 before each rendered frame.  The
               first second-move frame therefore observes decrement one:
               $F8 scroll and $D0 sprite state, with counts 1..26 at
               locals 26..51.  Keep page 1 selected while the 8-bit scroll
               wraps; local 51 is the black tail. */
            uint8_t move_count = (uint8_t)(cursor - 25U);
            state->phase = TECMO_INTRO_FINALE_PHASE_SECOND_MOVE;
            if (asset != NULL) {
                state->player_x = (uint8_t)(asset->reverse_second_x +
                                             move_count * asset->reverse_delta_x);
                state->scroll_x = (uint8_t)(move_count * asset->reverse_delta_x);
            }
            state->scroll_page = 1U;
        } else {
            state->phase = TECMO_INTRO_FINALE_PHASE_BLACK;
        }
        state->sprites_visible = !state->black;
        return;
    }
    cursor -= FINALE_SELECTOR_DURATION;

    if (cursor < FINALE_STAGED_DURATION) {
        set_scene_state(state, TECMO_INTRO_FINALE_STAGED_GROUP, 3U, cursor);
        state->sprite_variant_index = 0U;
        state->black = finale_route_is_black(asset, 3U, cursor);
        if (asset != NULL) {
            state->player_x = asset->staged_anchor_x;
            state->player_y = asset->staged_anchor_y;
        }
        black_gate = finale_route_gate(asset, 3U);
        if (cursor == 0U) {
            state->phase = TECMO_INTRO_FINALE_PHASE_LOAD;
        } else if (cursor < black_gate) {
            state->phase = TECMO_INTRO_FINALE_PHASE_BLACK;
        } else if (cursor < black_gate +
                            TECMO_INTRO_FINALE_STAGED_WAIT_FRAMES) {
            state->phase = TECMO_INTRO_FINALE_PHASE_STAGED_WAIT;
        } else {
            state->phase = TECMO_INTRO_FINALE_PHASE_DISPATCH_WAIT;
        }
        state->sprites_visible = !state->black && cursor >= black_gate;
        return;
    }
    cursor -= FINALE_STAGED_DURATION;
    set_scene_state(state, TECMO_INTRO_FINALE_TITLE, 4U, cursor);
    state->black = cursor < 15U;
    if (cursor < 15U) {
        state->phase = cursor == 0U ? TECMO_INTRO_FINALE_PHASE_LOAD
                                    : TECMO_INTRO_FINALE_PHASE_BLACK;
        return;
    }
    state->black = false;
    cursor -= 15U;
    if (cursor < TECMO_INTRO_FINALE_TITLE_PREROLL_FRAMES) {
        state->phase = TECMO_INTRO_FINALE_PHASE_TITLE_PREROLL;
        /* The first $8385/C000 call is already rendered at local 15:
           secondary iteration one exposes the opposite title page at the
           right edge. */
        secondary_iterations = cursor + 1U;
        finale_set_title_scroll(state, 0U, secondary_iterations,
                                finale_title_initial_page(asset));
        return;
    }
    cursor -= TECMO_INTRO_FINALE_TITLE_PREROLL_FRAMES;
    secondary_iterations = TECMO_INTRO_FINALE_TITLE_PREROLL_FRAMES;
    if (cursor < TECMO_INTRO_FINALE_TITLE_DISPATCH_WAIT_FRAMES) {
        state->phase = TECMO_INTRO_FINALE_PHASE_DISPATCH_WAIT;
        finale_set_title_scroll(state, 0U, secondary_iterations,
                                finale_title_initial_page(asset));
        return;
    }
    cursor -= TECMO_INTRO_FINALE_TITLE_DISPATCH_WAIT_FRAMES;
    if (cursor < TECMO_INTRO_FINALE_TITLE_WRITE_FRAMES) {
        state->phase = TECMO_INTRO_FINALE_PHASE_TITLE_WRITE;
        state->title_slots_written =
            (uint8_t)(cursor / TECMO_INTRO_FINALE_TITLE_SLOT_INTERVAL_FRAMES + 1U);
        iterations = cursor + 1U;
        finale_set_title_scroll(state, iterations, secondary_iterations,
                                finale_title_initial_page(asset));
    } else {
        state->title_slots_written = TECMO_INTRO_FINALE_TITLE_SLOT_COUNT;
        iterations = TECMO_INTRO_FINALE_TITLE_WRITE_FRAMES;
        cursor -= TECMO_INTRO_FINALE_TITLE_WRITE_FRAMES;
        if (cursor < TECMO_INTRO_FINALE_TITLE_TAIL_FRAMES) {
            state->phase = TECMO_INTRO_FINALE_PHASE_TITLE_TAIL;
            /* The first C000 call after slot 44 is already a rendered tail
               frame.  Its secondary retreat count is therefore one, not
               zero (Bank04 $8303-$836A / $8385). */
            secondary_iterations += cursor + 1U;
            finale_set_title_scroll(state, iterations, secondary_iterations,
                                    finale_title_initial_page(asset));
            return;
        }
        /* The 617-frame title route ends with one rendered driver wait.  The
           terminal hold begins at the next global frame (1001), where the
           outer handoff gate above takes over. */
        state->phase = TECMO_INTRO_FINALE_PHASE_DISPATCH_WAIT;
        finale_set_title_scroll(state, iterations,
                                TECMO_INTRO_FINALE_TITLE_PREROLL_FRAMES +
                                    TECMO_INTRO_FINALE_TITLE_TAIL_FRAMES,
                                finale_title_initial_page(asset));
        return;
    }
    finale_set_title_scroll(state, iterations, secondary_iterations,
                            finale_title_initial_page(asset));
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

static uint8_t finale_brightness_cap(uint8_t color, uint8_t cap)
{
    uint8_t brightness;
    if (cap >= 4U || color == 0x0FU) return color;
    brightness = (uint8_t)(color & 0x30U);
    if (brightness > (uint8_t)(cap << 4U)) {
        color = (uint8_t)((color & 0x0FU) | (cap << 4U));
    }
    return color;
}

static uint8_t finale_scene_brightness_cap(const TecmoIntroFinaleState *state)
{
    unsigned stage;
    if (state == NULL ||
        (state->scene != TECMO_INTRO_FINALE_OPENING_SCREEN &&
         state->scene != TECMO_INTRO_FINALE_STAGED_GROUP)) {
        return 4U;
    }
    if (state->scene_frame < 7U) return 4U;
    stage = (state->scene_frame - 7U) / 4U;
    return (uint8_t)(stage > 3U ? 3U : stage);
}

static void finale_scene_palette(const TecmoIntroFinaleAsset *asset,
                                 const TecmoIntroFinaleState *state,
                                 uint8_t palette[16])
{
    uint8_t cap;
    memcpy(palette, asset->palettes[state->screen_index][state->palette_stage],
           16U);
    if (state->scene == TECMO_INTRO_FINALE_STAGED_GROUP) {
        /* Bank04 $8A6F mirrors the C036 team color into both the background
           and sprite palette work areas at absolute slot 9. */
        palette[9U] = asset->staged_team_color;
    }
    cap = finale_scene_brightness_cap(state);
    for (size_t i = 0U; i < 16U; ++i) {
        palette[i] = finale_brightness_cap(palette[i], cap);
    }
}

static void make_sprite_palette(uint32_t rgba[4],
                                const uint8_t palette[16],
                                uint8_t index,
                                uint8_t brightness_cap)
{
    uint8_t base = (uint8_t)((index & 3U) * 4U);
    rgba[0] = 0U;
    for (size_t i = 1U; i < 4U; ++i) {
        rgba[i] = tecmo_nes_2c02_rgba(
            finale_brightness_cap(palette[base + i], brightness_cap));
    }
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
    uint8_t palette[16];
    finale_scene_palette(asset, state, palette);
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
        unsigned row = scanline / 8U;
        unsigned tile_scanline = scanline & 7U;
        for (unsigned x = 0U; x < 256U; ++x) {
            unsigned source_x = x + scroll_x;
            /* Bank04's page-1 swap inherits the eight-pixel nametable
               phase established by the short route.  The RAM scroll values
               remain the evidence-backed $00/$F8..$30 values; this fixed
               phase belongs to the source page presentation. */
            if (state->scene == TECMO_INTRO_FINALE_SELECTOR_TRANSITION &&
                state->scroll_page == 1U) {
                source_x -= 8U;
            }
            /* The selector's second move repeats water page 1 rather than
               switching to page 0 when the eight-bit scroll crosses 256. */
            unsigned page = state->scene == TECMO_INTRO_FINALE_SELECTOR_TRANSITION &&
                                    state->scroll_page == 1U
                                ? 1U
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

static bool draw_caption_glyph(TecmoFramebuffer *fb,
                               const TecmoIntroFinaleAsset *asset,
                               const uint8_t *chr_bytes,
                               uint64_t chr_byte_count,
                               const TecmoIntroFinaleState *state,
                               const TecmoIntroFinaleCaption *caption,
                               unsigned glyph_index,
                               uint8_t glyph_ref,
                               int origin_x,
                               int origin_y,
                               int scale)
{
    uint32_t rgba[4];
    uint32_t chr_offset;
    uint8_t palette[16];
    uint8_t tile_ids[TECMO_INTRO_FINALE_TITLE_CELL_COUNT];
    const TecmoIntroFinaleTitleSlot *slot = NULL;

    if (glyph_ref != TECMO_INTRO_FINALE_CAPTION_GLYPH_SENTINEL) {
        if (glyph_ref >= TECMO_INTRO_FINALE_TITLE_SLOT_COUNT) return false;
        slot = &asset->title_slots[glyph_ref];
        for (size_t tile = 0U; tile < TECMO_INTRO_FINALE_TITLE_CELL_COUNT; ++tile) {
            tile_ids[tile] = slot->cells[tile].tile_id;
        }
        chr_offset = slot->cells[0].chr_offset;
    } else {
        for (size_t tile = 0U; tile < TECMO_INTRO_FINALE_TITLE_CELL_COUNT; ++tile) {
            tile_ids[tile] = asset->caption_extra_glyph_tiles[tile];
        }
        chr_offset = (tile_ids[0] < 0x80U
                          ? asset->title_bands[0].low_chr_base
                          : asset->title_bands[0].high_chr_base) +
                     (uint32_t)(tile_ids[0] & 0x7FU) * 16U;
    }
    finale_scene_palette(asset, state, palette);
    for (size_t tile = 0U; tile < TECMO_INTRO_FINALE_TITLE_CELL_COUNT; ++tile) {
        if (glyph_ref != TECMO_INTRO_FINALE_CAPTION_GLYPH_SENTINEL) {
            chr_offset = slot->cells[tile].chr_offset;
        } else {
            chr_offset = (tile_ids[tile] < 0x80U
                              ? asset->title_bands[0].low_chr_base
                              : asset->title_bands[0].high_chr_base) +
                         (uint32_t)(tile_ids[tile] & 0x7FU) * 16U;
        }
        if (!chr_range_valid(chr_offset, chr_byte_count, 16U)) return false;
        for (unsigned scanline = 0U; scanline < 8U; ++scanline) {
            uint8_t plane0 = chr_bytes[chr_offset + scanline];
            uint8_t plane1 = chr_bytes[chr_offset + scanline + 8U];
            for (unsigned column = 0U; column < 8U; ++column) {
                uint8_t bit = (uint8_t)(7U - column);
                uint8_t value = (uint8_t)(((plane0 >> bit) & 1U) |
                                           (((plane1 >> bit) & 1U) << 1U));
                if (value == 0U) continue;
                make_bg_palette(rgba,
                                palette,
                                caption->palette_index);
                fill_rect(fb,
                          origin_x + ((int)caption->column + (int)glyph_index * 2 +
                                      (int)(tile % 2U)) * 8 * scale +
                                      (int)column * scale,
                          origin_y + ((int)caption->row + (int)(tile / 2U)) * 8 * scale +
                                      (int)scanline * scale,
                          scale,
                          scale,
                          rgba[value]);
            }
        }
    }
    return true;
}

static bool draw_captions(TecmoFramebuffer *fb,
                          const TecmoIntroFinaleAsset *asset,
                          const uint8_t *chr_bytes,
                          uint64_t chr_byte_count,
                          const TecmoIntroFinaleState *state,
                          int origin_x,
                          int origin_y,
                          int scale)
{
    if (state->black) return true;
    for (size_t caption_index = 0U;
         caption_index < TECMO_INTRO_FINALE_CAPTION_COUNT; ++caption_index) {
        const TecmoIntroFinaleCaption *caption = &asset->captions[caption_index];
        unsigned revealed;
        if (caption->route_index != state->screen_index ||
            state->scene_frame < caption->first_frame) continue;
        revealed = (state->scene_frame - caption->first_frame) /
                   (caption->reveal_interval == 0U ? 1U : caption->reveal_interval) + 1U;
        if (revealed > caption->glyph_count) revealed = caption->glyph_count;
        for (unsigned glyph = 0U; glyph < revealed; ++glyph) {
            if (!draw_caption_glyph(fb, asset, chr_bytes, chr_byte_count, state,
                                    caption, glyph, caption->glyph_refs[glyph],
                                    origin_x, origin_y, scale)) return false;
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
        uint8_t sprite_palette[16];
        uint8_t brightness_cap = 4U;
        int x = origin_x + ((int)state->player_x + piece->dx) * scale;
        /* NES OAM Y is the stored sprite top minus one in the CPU-facing
           tables; the PPU displays it one scanline below that value. */
        int y = origin_y + ((int)state->player_y + piece->dy + 1) * scale;
        if (!chr_range_valid(top, chr_byte_count, 16U) ||
            !chr_range_valid(bottom, chr_byte_count, 16U)) return false;
        if (state->scene == TECMO_INTRO_FINALE_SELECTOR_TRANSITION &&
            state->palette_stage < 4U) {
            brightness_cap = state->palette_stage;
        } else if (state->scene == TECMO_INTRO_FINALE_STAGED_GROUP) {
            brightness_cap = finale_scene_brightness_cap(state);
        }
        memcpy(sprite_palette,
               asset->sprite_palettes[state->sprite_variant_index],
               sizeof(sprite_palette));
        if (state->scene == TECMO_INTRO_FINALE_STAGED_GROUP) {
            /* The same $8A6F write is visible to the sprite palette work
               area, even when a particular piece does not use slot 9. */
            sprite_palette[9U] = asset->staged_team_color;
        }
        make_sprite_palette(rgba,
                            sprite_palette,
                            piece->palette_index,
                            brightness_cap);
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
    if (!draw_captions(fb, asset, chr_bytes, chr_byte_count,
                       &state, origin_x, origin_y, scale)) return false;
    if (state.sprites_visible &&
        !draw_pieces(fb, asset, chr_bytes, chr_byte_count,
                     &state, origin_x, origin_y, scale)) return false;
    return true;
}
