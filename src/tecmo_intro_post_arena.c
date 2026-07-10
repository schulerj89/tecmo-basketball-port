#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_intro_post_arena.h"

#include "tecmo_asset_pack.h"
#include "tecmo_nes_video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define READY_ENTRY_ID "arena/intro/ready-screen"
#define WARRIORS_ENTRY_ID "arena/intro/warriors-transition"
#define READY_HEADER_SIZE 64U
#define READY_CELL_STRIDE 6U
#define READY_PALETTES_OFFSET 64U
#define READY_PALETTE_FRAMES_OFFSET 144U
#define READY_MASKS_OFFSET 149U
#define READY_CELLS_OFFSET 245U
#define READY_PAYLOAD_SIZE (READY_CELLS_OFFSET + TECMO_INTRO_READY_CELL_COUNT * READY_CELL_STRIDE)
#define WARRIORS_HEADER_SIZE 96U
#define WARRIORS_CELL_STRIDE 10U
#define WARRIORS_PIECE_STRIDE 16U
#define WARRIORS_PATCH_STRIDE 6U
#define WARRIORS_BG_PALETTE_OFFSET 96U
#define WARRIORS_SPRITE_PALETTE_OFFSET 112U
#define WARRIORS_PAGES_OFFSET 128U
#define WARRIORS_PIECES_OFFSET \
    (WARRIORS_PAGES_OFFSET + TECMO_INTRO_WARRIORS_PAGE_COUNT * \
                                  TECMO_INTRO_WARRIORS_TILES_PER_PAGE * WARRIORS_CELL_STRIDE)
#define WARRIORS_PATCHES_OFFSET \
    (WARRIORS_PIECES_OFFSET + TECMO_INTRO_WARRIORS_PIECE_COUNT * WARRIORS_PIECE_STRIDE)
#define WARRIORS_WORDMARK_OFFSET \
    (WARRIORS_PATCHES_OFFSET + TECMO_INTRO_WARRIORS_PATCH_COUNT * \
                                    TECMO_INTRO_WARRIORS_PATCH_TILE_COUNT * WARRIORS_PATCH_STRIDE)
#define WARRIORS_PAYLOAD_SIZE \
    (WARRIORS_WORDMARK_OFFSET + TECMO_INTRO_WARRIORS_WORDMARK_TILE_COUNT * \
                                    WARRIORS_PATCH_STRIDE)
#define CLIPPERS_ENTRY_ID "arena/intro/clippers-transition"
#define CLIPPERS_HEADER_SIZE 96U
#define CLIPPERS_CELL_STRIDE 10U
#define CLIPPERS_PALETTES_OFFSET 96U
#define CLIPPERS_CELLS_OFFSET 160U
#define CLIPPERS_WORDMARK_OFFSET \
    (CLIPPERS_CELLS_OFFSET + TECMO_INTRO_CLIPPERS_PAGE_COUNT * \
                                TECMO_INTRO_CLIPPERS_TILES_PER_PAGE * CLIPPERS_CELL_STRIDE)
#define CLIPPERS_PAYLOAD_SIZE \
    (CLIPPERS_WORDMARK_OFFSET + \
     TECMO_INTRO_CLIPPERS_WORDMARK_TILE_COUNT * CLIPPERS_CELL_STRIDE)

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
    int written;
    size_t root_length;

    if (path == NULL || path_size == 0U || root == NULL || root[0] == '\0') {
        return -1;
    }
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
                              const char *entry_id,
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
        if (make_pack_path(root_build, sizeof(root_build), project_root, "build\\tecmo.assetpack") == 0) {
            paths[path_count++] = root_build;
        }
        if (make_pack_path(root_pack, sizeof(root_pack), project_root, "tecmo.assetpack") == 0) {
            paths[path_count++] = root_pack;
        }
        paths[path_count++] = "build\\tecmo.assetpack";
        paths[path_count++] = "tecmo.assetpack";
        paths[path_count++] = "..\\build\\tecmo.assetpack";
    }

    for (size_t i = 0U; i < path_count; ++i) {
        if (!file_exists(paths[i])) continue;
        if (tecmo_asset_pack_read_entry(paths[i], entry_id, bytes_out, size_out) == 0) {
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
    return (uint64_t)offset <= chr_byte_count && (uint64_t)size <= chr_byte_count - offset;
}

static bool parse_ready(TecmoIntroReadyAsset *asset,
                        const uint8_t *bytes,
                        uint64_t byte_count)
{
    if (byte_count != READY_PAYLOAD_SIZE || memcmp(bytes, "TRDY", 4U) != 0 ||
        read_u16(bytes + 4U) != 1U || read_u16(bytes + 6U) != READY_HEADER_SIZE ||
        read_u16(bytes + 8U) != TECMO_INTRO_READY_WIDTH ||
        read_u16(bytes + 10U) != TECMO_INTRO_READY_HEIGHT ||
        read_u16(bytes + 12U) != READY_CELL_STRIDE ||
        read_u16(bytes + 14U) != TECMO_INTRO_READY_PALETTE_STAGE_COUNT ||
        read_u16(bytes + 16U) != TECMO_INTRO_READY_MASK_COUNT ||
        read_u16(bytes + 18U) != TECMO_INTRO_READY_MASK_SLOT_COUNT ||
        read_u32(bytes + 20U) != READY_PALETTES_OFFSET ||
        read_u32(bytes + 24U) != READY_PALETTE_FRAMES_OFFSET ||
        read_u32(bytes + 28U) != READY_MASKS_OFFSET ||
        read_u32(bytes + 32U) != READY_CELLS_OFFSET ||
        read_u32(bytes + 36U) != TECMO_INTRO_READY_CELL_COUNT ||
        read_u32(bytes + 40U) != TECMO_INTRO_READY_HANDOFF_FRAME ||
        read_u16(bytes + 44U) != 8U || read_u16(bytes + 46U) != 16U ||
        read_u16(bytes + 48U) != 16U || read_u16(bytes + 50U) != 4U) {
        return false;
    }

    memcpy(asset->palettes, bytes + READY_PALETTES_OFFSET, sizeof(asset->palettes));
    memcpy(asset->palette_frames,
           bytes + READY_PALETTE_FRAMES_OFFSET,
           sizeof(asset->palette_frames));
    memcpy(asset->masks, bytes + READY_MASKS_OFFSET, sizeof(asset->masks));
    if (asset->palette_frames[0] != 0U || asset->palette_frames[1] != 10U ||
        asset->palette_frames[2] != 14U || asset->palette_frames[3] != 18U ||
        asset->palette_frames[4] != 22U) {
        return false;
    }

    for (size_t i = 0U; i < TECMO_INTRO_READY_CELL_COUNT; ++i) {
        const uint8_t *cell = bytes + READY_CELLS_OFFSET + i * READY_CELL_STRIDE;
        asset->cells[i].tile_id = cell[0];
        asset->cells[i].palette_index = cell[1];
        asset->cells[i].chr_offset = read_u32(cell + 2U);
        if (asset->cells[i].palette_index > 3U) {
            return false;
        }
    }
    for (size_t record = 0U; record < TECMO_INTRO_READY_MASK_COUNT; ++record) {
        for (size_t slot = 0U; slot < TECMO_INTRO_READY_MASK_SLOT_COUNT; ++slot) {
            if (asset->masks[record][slot] > 3U) {
                return false;
            }
        }
    }
    return true;
}

static bool parse_warriors(TecmoIntroWarriorsAsset *asset,
                           const uint8_t *bytes,
                           uint64_t byte_count)
{
    if (byte_count != WARRIORS_PAYLOAD_SIZE || memcmp(bytes, "TWAR", 4U) != 0 ||
        read_u16(bytes + 4U) != 1U || read_u16(bytes + 6U) != WARRIORS_HEADER_SIZE ||
        read_u16(bytes + 8U) != 32U || read_u16(bytes + 10U) != 30U ||
        read_u16(bytes + 12U) != TECMO_INTRO_WARRIORS_PAGE_COUNT ||
        read_u16(bytes + 14U) != WARRIORS_CELL_STRIDE ||
        read_u16(bytes + 16U) != TECMO_INTRO_WARRIORS_PIECE_COUNT ||
        read_u16(bytes + 18U) != WARRIORS_PIECE_STRIDE ||
        read_u16(bytes + 20U) != TECMO_INTRO_WARRIORS_PATCH_COUNT ||
        read_u16(bytes + 22U) != TECMO_INTRO_WARRIORS_PATCH_TILE_COUNT ||
        read_u16(bytes + 24U) != TECMO_INTRO_WARRIORS_SPLIT_SCANLINE ||
        read_u32(bytes + 28U) != WARRIORS_BG_PALETTE_OFFSET ||
        read_u32(bytes + 32U) != WARRIORS_SPRITE_PALETTE_OFFSET ||
        read_u32(bytes + 36U) != WARRIORS_PAGES_OFFSET ||
        read_u32(bytes + 40U) != WARRIORS_PIECES_OFFSET ||
        read_u32(bytes + 44U) != WARRIORS_PATCHES_OFFSET ||
        read_u32(bytes + 48U) != TECMO_INTRO_WARRIORS_HANDOFF_FRAME ||
        bytes[52U] != TECMO_INTRO_WARRIORS_NEXT_SCREEN ||
        bytes[53U] != 4U || bytes[54U] != 7U || bytes[55U] != 11U ||
        bytes[56U] != 15U || bytes[57U] != 17U || bytes[58U] != 26U ||
        bytes[59U] != 74U || bytes[60U] != 193U || bytes[61U] != 200U ||
        bytes[62U] != 213U ||
        read_u32(bytes + 64U) != WARRIORS_WORDMARK_OFFSET ||
        read_u16(bytes + 68U) != TECMO_INTRO_WARRIORS_WORDMARK_GLYPH_COUNT ||
        read_u16(bytes + 70U) != 4U || read_u16(bytes + 72U) != 8U ||
        read_u16(bytes + 74U) != 26U) {
        return false;
    }

    memcpy(asset->background_palette, bytes + WARRIORS_BG_PALETTE_OFFSET, 16U);
    memcpy(asset->sprite_palette, bytes + WARRIORS_SPRITE_PALETTE_OFFSET, 16U);
    for (size_t page = 0U; page < TECMO_INTRO_WARRIORS_PAGE_COUNT; ++page) {
        for (size_t i = 0U; i < TECMO_INTRO_WARRIORS_TILES_PER_PAGE; ++i) {
            const uint8_t *cell = bytes + WARRIORS_PAGES_OFFSET +
                                  (page * TECMO_INTRO_WARRIORS_TILES_PER_PAGE + i) *
                                      WARRIORS_CELL_STRIDE;
            TecmoIntroWarriorsTile *dest = &asset->pages[page][i];
            dest->tile_id = cell[0];
            dest->palette_index = cell[1];
            dest->moving_chr_offset = read_u32(cell + 2U);
            dest->lower_chr_offset = read_u32(cell + 6U);
            if (dest->palette_index > 3U) {
                return false;
            }
        }
    }
    for (size_t i = 0U; i < TECMO_INTRO_WARRIORS_PIECE_COUNT; ++i) {
        const uint8_t *piece = bytes + WARRIORS_PIECES_OFFSET + i * WARRIORS_PIECE_STRIDE;
        TecmoIntroWarriorsPiece *dest = &asset->pieces[i];
        dest->dx = (int16_t)read_u16(piece + 0U);
        dest->dy = (int16_t)read_u16(piece + 2U);
        dest->top_chr_offset = read_u32(piece + 4U);
        dest->bottom_chr_offset = read_u32(piece + 8U);
        dest->palette_index = piece[12U];
        dest->flags = piece[13U];
        if (dest->palette_index > 3U || (dest->flags & 0xFCU) != 0U) {
            return false;
        }
    }
    for (size_t patch = 0U; patch < TECMO_INTRO_WARRIORS_PATCH_COUNT; ++patch) {
        for (size_t i = 0U; i < TECMO_INTRO_WARRIORS_PATCH_TILE_COUNT; ++i) {
            const uint8_t *cell = bytes + WARRIORS_PATCHES_OFFSET +
                                  (patch * TECMO_INTRO_WARRIORS_PATCH_TILE_COUNT + i) *
                                      WARRIORS_PATCH_STRIDE;
            TecmoIntroWarriorsPatchTile *dest = &asset->patches[patch][i];
            dest->tile_id = cell[0];
            dest->palette_index = cell[1];
            dest->chr_offset = read_u32(cell + 2U);
            if (dest->palette_index > 3U) {
                return false;
            }
        }
    }
    for (size_t i = 0U; i < TECMO_INTRO_WARRIORS_WORDMARK_TILE_COUNT; ++i) {
        const uint8_t *cell = bytes + WARRIORS_WORDMARK_OFFSET + i * WARRIORS_PATCH_STRIDE;
        TecmoIntroWarriorsPatchTile *dest = &asset->wordmark[i];
        dest->tile_id = cell[0];
        dest->palette_index = cell[1];
        dest->chr_offset = read_u32(cell + 2U);
        if (dest->tile_id < 0x80U || dest->palette_index > 3U) {
            return false;
        }
    }
    return true;
}

static bool parse_clippers(TecmoIntroClippersAsset *asset,
                           const uint8_t *bytes,
                           uint64_t byte_count)
{
    if (byte_count != CLIPPERS_PAYLOAD_SIZE || memcmp(bytes, "TCLP", 4U) != 0 ||
        read_u16(bytes + 4U) != 1U || read_u16(bytes + 6U) != CLIPPERS_HEADER_SIZE ||
        read_u16(bytes + 8U) != 32U || read_u16(bytes + 10U) != 30U ||
        read_u16(bytes + 12U) != TECMO_INTRO_CLIPPERS_PAGE_COUNT ||
        read_u16(bytes + 14U) != CLIPPERS_CELL_STRIDE ||
        read_u16(bytes + 16U) != TECMO_INTRO_CLIPPERS_PALETTE_STAGE_COUNT ||
        read_u32(bytes + 20U) != CLIPPERS_PALETTES_OFFSET ||
        read_u32(bytes + 24U) != CLIPPERS_CELLS_OFFSET ||
        read_u32(bytes + 28U) != TECMO_INTRO_CLIPPERS_PAGE_COUNT *
                                  TECMO_INTRO_CLIPPERS_TILES_PER_PAGE ||
        read_u32(bytes + 32U) != TECMO_INTRO_CLIPPERS_HANDOFF_FRAME ||
        read_u16(bytes + 36U) != TECMO_INTRO_CLIPPERS_NEXT_ROUTE ||
        read_u16(bytes + 38U) != TECMO_INTRO_CLIPPERS_LOWER_SPLIT_SCANLINE ||
        read_u16(bytes + 40U) != TECMO_INTRO_CLIPPERS_MOTION_FRAME ||
        read_u16(bytes + 42U) != TECMO_INTRO_CLIPPERS_MOTION_TICK_FRAMES ||
        read_u16(bytes + 44U) != TECMO_INTRO_CLIPPERS_MOTION_TICK_COUNT ||
        read_u16(bytes + 46U) != TECMO_INTRO_CLIPPERS_POSE_SWITCH_TICK ||
        bytes[48U] != 0x2CU || bytes[49U] != 0x2EU ||
        bytes[50U] != 0x2CU || bytes[51U] != 0xFAU ||
        read_u32(bytes + 56U) != CLIPPERS_WORDMARK_OFFSET ||
        read_u16(bytes + 60U) != TECMO_INTRO_CLIPPERS_WORDMARK_TILE_COUNT ||
        read_u16(bytes + 62U) != TECMO_INTRO_CLIPPERS_WORDMARK_FRAME) {
        return false;
    }

    memcpy(asset->palettes, bytes + CLIPPERS_PALETTES_OFFSET, sizeof(asset->palettes));
    for (size_t page = 0U; page < TECMO_INTRO_CLIPPERS_PAGE_COUNT; ++page) {
        for (size_t i = 0U; i < TECMO_INTRO_CLIPPERS_TILES_PER_PAGE; ++i) {
            const uint8_t *cell = bytes + CLIPPERS_CELLS_OFFSET +
                                  (page * TECMO_INTRO_CLIPPERS_TILES_PER_PAGE + i) *
                                      CLIPPERS_CELL_STRIDE;
            TecmoIntroClippersTile *dest = &asset->pages[page][i];
            dest->tile_id = cell[0];
            dest->palette_index = cell[1];
            dest->base_chr_offset = read_u32(cell + 2U);
            dest->lower_chr_offset = read_u32(cell + 6U);
            if (dest->palette_index > 3U) return false;
        }
    }
    for (size_t i = 0U; i < TECMO_INTRO_CLIPPERS_WORDMARK_TILE_COUNT; ++i) {
        const uint8_t *cell = bytes + CLIPPERS_WORDMARK_OFFSET + i * CLIPPERS_CELL_STRIDE;
        TecmoIntroClippersTile *dest = &asset->wordmark[i];
        dest->tile_id = cell[0];
        dest->palette_index = cell[1];
        dest->base_chr_offset = read_u32(cell + 2U);
        dest->lower_chr_offset = read_u32(cell + 6U);
        if (dest->tile_id < 0x80U || dest->palette_index > 3U) return false;
    }
    return true;
}

static bool palette_valid(const uint8_t *palette, size_t count)
{
    for (size_t i = 0U; i < count; ++i) {
        if (palette[i] > 0x3FU) return false;
    }
    return true;
}

static bool resolved_chr_valid(uint32_t offset, uint64_t chr_byte_count, uint32_t size)
{
    return (offset & 0x0FU) == 0U && chr_range_valid(offset, chr_byte_count, size);
}

static bool validate_ready_asset(const TecmoIntroReadyAsset *asset, uint64_t chr_byte_count)
{
    if (!palette_valid(&asset->palettes[0][0], sizeof(asset->palettes))) return false;
    for (size_t i = 0U; i < TECMO_INTRO_READY_CELL_COUNT; ++i) {
        if (!resolved_chr_valid(asset->cells[i].chr_offset, chr_byte_count, 16U)) return false;
    }
    return true;
}

static bool validate_warriors_asset(const TecmoIntroWarriorsAsset *asset,
                                    uint64_t chr_byte_count)
{
    if (!palette_valid(asset->background_palette, sizeof(asset->background_palette)) ||
        !palette_valid(asset->sprite_palette, sizeof(asset->sprite_palette))) return false;
    for (size_t page = 0U; page < TECMO_INTRO_WARRIORS_PAGE_COUNT; ++page) {
        for (size_t i = 0U; i < TECMO_INTRO_WARRIORS_TILES_PER_PAGE; ++i) {
            const TecmoIntroWarriorsTile *cell = &asset->pages[page][i];
            if (!resolved_chr_valid(cell->moving_chr_offset, chr_byte_count, 16U) ||
                !resolved_chr_valid(cell->lower_chr_offset, chr_byte_count, 16U)) return false;
        }
    }
    for (size_t i = 0U; i < TECMO_INTRO_WARRIORS_PIECE_COUNT; ++i) {
        if (!resolved_chr_valid(asset->pieces[i].top_chr_offset, chr_byte_count, 16U) ||
            !resolved_chr_valid(asset->pieces[i].bottom_chr_offset, chr_byte_count, 16U)) return false;
    }
    for (size_t patch = 0U; patch < TECMO_INTRO_WARRIORS_PATCH_COUNT; ++patch) {
        for (size_t i = 0U; i < TECMO_INTRO_WARRIORS_PATCH_TILE_COUNT; ++i) {
            if (!resolved_chr_valid(asset->patches[patch][i].chr_offset,
                                    chr_byte_count,
                                    16U)) return false;
        }
    }
    for (size_t i = 0U; i < TECMO_INTRO_WARRIORS_WORDMARK_TILE_COUNT; ++i) {
        if (!resolved_chr_valid(asset->wordmark[i].chr_offset, chr_byte_count, 16U)) return false;
    }
    return true;
}

static bool validate_clippers_asset(const TecmoIntroClippersAsset *asset,
                                    uint64_t chr_byte_count)
{
    if (!palette_valid(&asset->palettes[0][0], sizeof(asset->palettes))) return false;
    for (size_t page = 0U; page < TECMO_INTRO_CLIPPERS_PAGE_COUNT; ++page) {
        for (size_t i = 0U; i < TECMO_INTRO_CLIPPERS_TILES_PER_PAGE; ++i) {
            const TecmoIntroClippersTile *cell = &asset->pages[page][i];
            uint32_t tile_offset = (uint32_t)(cell->tile_id & 0x7FU) * 16U;
            uint32_t expected_base =
                (uint32_t)(cell->tile_id < 0x80U ? 0x2CU : 0x2EU) * 1024U + tile_offset;
            uint32_t expected_lower =
                (uint32_t)(cell->tile_id < 0x80U ? 0x2CU : 0xFAU) * 1024U + tile_offset;
            if (cell->base_chr_offset != expected_base ||
                cell->lower_chr_offset != expected_lower ||
                !resolved_chr_valid(cell->base_chr_offset, chr_byte_count, 16U) ||
                !resolved_chr_valid(cell->lower_chr_offset, chr_byte_count, 16U)) return false;
        }
    }
    for (size_t i = 0U; i < TECMO_INTRO_CLIPPERS_WORDMARK_TILE_COUNT; ++i) {
        const TecmoIntroClippersTile *cell = &asset->wordmark[i];
        uint32_t tile_offset = (uint32_t)(cell->tile_id & 0x7FU) * 16U;
        uint32_t expected_base = 0x2EU * 1024U + tile_offset;
        uint32_t expected_lower = 0xFAU * 1024U + tile_offset;
        if (cell->base_chr_offset != expected_base ||
            cell->lower_chr_offset != expected_lower ||
            !resolved_chr_valid(cell->base_chr_offset, chr_byte_count, 16U) ||
            !resolved_chr_valid(cell->lower_chr_offset, chr_byte_count, 16U)) return false;
    }
    return true;
}

bool tecmo_intro_ready_asset_load(TecmoIntroReadyAsset *asset, const char *project_root)
{
    uint8_t *bytes = NULL;
    uint8_t *chr_bytes = NULL;
    uint64_t byte_count = 0U;
    uint64_t chr_byte_count = 0U;
    char pack_path[1024];
    bool loaded;

    if (asset == NULL) {
        return false;
    }
    memset(asset, 0, sizeof(*asset));
    if (!read_native_entry(project_root,
                           READY_ENTRY_ID,
                           &bytes,
                           &byte_count,
                           pack_path,
                           sizeof(pack_path))) {
        set_status(asset->status, sizeof(asset->status), "TRDY-1 asset unavailable");
        return false;
    }
    loaded = parse_ready(asset, bytes, byte_count);
    tecmo_asset_pack_free(bytes);
    if (loaded && tecmo_asset_pack_read_entry(pack_path,
                                              "chr/all",
                                              &chr_bytes,
                                              &chr_byte_count) == 0) {
        loaded = validate_ready_asset(asset, chr_byte_count);
    } else {
        loaded = false;
    }
    tecmo_asset_pack_free(chr_bytes);
    if (!loaded) {
        set_status(asset->status, sizeof(asset->status), "TRDY-1 asset rejected");
        return false;
    }
    asset->available = true;
    (void)snprintf(asset->status, sizeof(asset->status), "TRDY-1 assetpack %s", pack_path);
    return true;
}

bool tecmo_intro_warriors_asset_load(TecmoIntroWarriorsAsset *asset,
                                     const char *project_root)
{
    uint8_t *bytes = NULL;
    uint8_t *chr_bytes = NULL;
    uint64_t byte_count = 0U;
    uint64_t chr_byte_count = 0U;
    char pack_path[1024];
    bool loaded;

    if (asset == NULL) {
        return false;
    }
    memset(asset, 0, sizeof(*asset));
    if (!read_native_entry(project_root,
                           WARRIORS_ENTRY_ID,
                           &bytes,
                           &byte_count,
                           pack_path,
                           sizeof(pack_path))) {
        set_status(asset->status, sizeof(asset->status), "TWAR-1 asset unavailable");
        return false;
    }
    loaded = parse_warriors(asset, bytes, byte_count);
    tecmo_asset_pack_free(bytes);
    if (loaded && tecmo_asset_pack_read_entry(pack_path,
                                              "chr/all",
                                              &chr_bytes,
                                              &chr_byte_count) == 0) {
        loaded = validate_warriors_asset(asset, chr_byte_count);
    } else {
        loaded = false;
    }
    tecmo_asset_pack_free(chr_bytes);
    if (!loaded) {
        set_status(asset->status, sizeof(asset->status), "TWAR-1 asset rejected");
        return false;
    }
    asset->available = true;
    (void)snprintf(asset->status, sizeof(asset->status), "TWAR-1 assetpack %s", pack_path);
    return true;
}

bool tecmo_intro_clippers_asset_load(TecmoIntroClippersAsset *asset,
                                     const char *project_root)
{
    uint8_t *bytes = NULL;
    uint8_t *chr_bytes = NULL;
    uint64_t byte_count = 0U;
    uint64_t chr_byte_count = 0U;
    char pack_path[1024];
    bool loaded;

    if (asset == NULL) return false;
    memset(asset, 0, sizeof(*asset));
    if (!read_native_entry(project_root,
                           CLIPPERS_ENTRY_ID,
                           &bytes,
                           &byte_count,
                           pack_path,
                           sizeof(pack_path))) {
        set_status(asset->status, sizeof(asset->status), "TCLP-1 asset unavailable");
        return false;
    }
    loaded = parse_clippers(asset, bytes, byte_count);
    tecmo_asset_pack_free(bytes);
    if (loaded && tecmo_asset_pack_read_entry(pack_path,
                                              "chr/all",
                                              &chr_bytes,
                                              &chr_byte_count) == 0) {
        loaded = validate_clippers_asset(asset, chr_byte_count);
    } else {
        loaded = false;
    }
    tecmo_asset_pack_free(chr_bytes);
    if (!loaded) {
        set_status(asset->status, sizeof(asset->status), "TCLP-1 asset rejected");
        return false;
    }
    asset->available = true;
    (void)snprintf(asset->status, sizeof(asset->status), "TCLP-1 assetpack %s", pack_path);
    return true;
}

void tecmo_intro_ready_state(unsigned frame, TecmoIntroReadyState *state)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->frame = frame;
    state->handoff = frame >= TECMO_INTRO_READY_HANDOFF_FRAME;
    state->black = frame >= 56U;
    if (frame >= 22U) state->palette_stage = 4U;
    else if (frame >= 18U) state->palette_stage = 3U;
    else if (frame >= 14U) state->palette_stage = 2U;
    else if (frame >= 10U) state->palette_stage = 1U;
    if (frame >= 24U) {
        unsigned record = (frame - 24U) / 2U;
        state->mask_index = (uint8_t)(record < TECMO_INTRO_READY_MASK_COUNT
                                          ? record
                                          : TECMO_INTRO_READY_MASK_COUNT - 1U);
    }
}

void tecmo_intro_warriors_state(unsigned frame, TecmoIntroWarriorsState *state)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->frame = frame;
    state->next_screen = TECMO_INTRO_WARRIORS_NEXT_SCREEN;
    if (frame >= TECMO_INTRO_WARRIORS_HANDOFF_FRAME) {
        state->phase = TECMO_INTRO_WARRIORS_HANDOFF;
        state->handoff = true;
        state->black = true;
        return;
    }
    if (frame >= 213U) {
        state->phase = TECMO_INTRO_WARRIORS_BLACK;
        state->black = true;
        return;
    }
    if (frame >= 200U) {
        state->phase = TECMO_INTRO_WARRIORS_PATCH_TWO;
        state->patch_count = 2U;
    } else if (frame >= 193U) {
        state->phase = TECMO_INTRO_WARRIORS_PATCH_ONE;
        state->patch_count = 1U;
    } else if (frame >= 75U) {
        state->phase = TECMO_INTRO_WARRIORS_HOLD;
    } else if (frame >= 26U) {
        state->phase = TECMO_INTRO_WARRIORS_PAN;
    } else if (frame >= 17U) {
        state->phase = TECMO_INTRO_WARRIORS_WORDMARK;
    } else if (frame >= 4U) {
        state->phase = TECMO_INTRO_WARRIORS_FADE;
    } else {
        state->phase = TECMO_INTRO_WARRIORS_LOAD;
    }
    if (frame >= 15U) state->palette_stage = 3U;
    else if (frame >= 11U) state->palette_stage = 2U;
    else if (frame >= 7U) state->palette_stage = 1U;
    state->sprites_visible = frame >= 4U;
    state->wordmark_visible = frame >= 17U;
    if (frame >= 17U) {
        unsigned glyphs = frame - 16U;
        state->wordmark_glyph_count = (uint8_t)(glyphs < TECMO_INTRO_WARRIORS_WORDMARK_GLYPH_COUNT
                                                    ? glyphs
                                                    : TECMO_INTRO_WARRIORS_WORDMARK_GLYPH_COUNT);
    }
    if (frame >= 26U) {
        unsigned pan = (frame - 24U) / 2U;
        state->pan = (uint8_t)(pan < 25U ? pan : 25U);
    }
}

void tecmo_intro_clippers_state(unsigned frame, TecmoIntroClippersState *state)
{
    unsigned tick = 0U;
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->frame = frame;
    state->next_route = TECMO_INTRO_CLIPPERS_NEXT_ROUTE;
    state->handoff = frame >= TECMO_INTRO_CLIPPERS_HANDOFF_FRAME;
    if (frame >= 18U) state->palette_stage = 3U;
    else if (frame >= 14U) state->palette_stage = 2U;
    else if (frame >= 10U) state->palette_stage = 1U;
    if (frame >= TECMO_INTRO_CLIPPERS_MOTION_FRAME) {
        tick = (frame - TECMO_INTRO_CLIPPERS_MOTION_FRAME) /
               TECMO_INTRO_CLIPPERS_MOTION_TICK_FRAMES;
        if (tick > TECMO_INTRO_CLIPPERS_MOTION_TICK_COUNT) {
            tick = TECMO_INTRO_CLIPPERS_MOTION_TICK_COUNT;
        }
    }
    state->motion = (uint8_t)tick;
    state->scroll_x = tick >= TECMO_INTRO_CLIPPERS_POSE_SWITCH_TICK ? 0xFFU : 0U;
    state->pose_page = state->scroll_x != 0U ? 1U : 0U;
    state->wordmark_visible = frame >= TECMO_INTRO_CLIPPERS_WORDMARK_FRAME;
}

const char *tecmo_intro_warriors_phase_name(TecmoIntroWarriorsPhase phase)
{
    static const char *names[] = {
        "load", "fade", "wordmark", "pan", "hold",
        "patch-one", "patch-two", "black", "handoff"
    };
    return (unsigned)phase < sizeof(names) / sizeof(names[0]) ? names[phase] : "unknown";
}

static void fill_rect(TecmoFramebuffer *fb, int x, int y, int w, int h, uint32_t color)
{
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > fb->width ? fb->width : x + w;
    int y1 = y + h > fb->height ? fb->height : y + h;
    if (fb == NULL || fb->pixels == NULL || x0 >= x1 || y0 >= y1) return;
    for (int row = y0; row < y1; ++row) {
        uint32_t *dest = fb->pixels + (size_t)row * (size_t)fb->pitch_pixels + (size_t)x0;
        for (int col = x0; col < x1; ++col) *dest++ = color;
    }
}

static void make_bg_palette(uint32_t rgba[4], const uint8_t palette[16], uint8_t index)
{
    size_t base = (size_t)(index & 3U) * 4U;
    rgba[0] = tecmo_nes_2c02_rgba(palette[0]);
    for (size_t i = 1U; i < 4U; ++i) rgba[i] = tecmo_nes_2c02_rgba(palette[base + i]);
}

static void make_sprite_palette(uint32_t rgba[4], const uint8_t palette[16], uint8_t index)
{
    size_t base = (size_t)(index & 3U) * 4U;
    rgba[0] = 0U;
    for (size_t i = 1U; i < 4U; ++i) rgba[i] = tecmo_nes_2c02_rgba(palette[base + i]);
}

static uint8_t fade_color(uint8_t color, uint8_t stage)
{
    uint8_t reduction;
    if (stage == 0U) return 0x0FU;
    if (stage >= 3U || color == 0x0FU) return color;
    reduction = stage == 1U ? 0x20U : 0x10U;
    return (color & 0x30U) >= reduction ? (uint8_t)(color - reduction) : 0x0FU;
}

bool tecmo_intro_post_arena_draw_ready(TecmoFramebuffer *fb,
                                       const TecmoIntroReadyAsset *asset,
                                       const uint8_t *chr_bytes,
                                       uint64_t chr_byte_count,
                                       unsigned frame,
                                       int origin_x,
                                       int origin_y,
                                       int scale)
{
    TecmoIntroReadyState state;
    if (fb == NULL || asset == NULL || !asset->available || chr_bytes == NULL || scale <= 0) {
        return false;
    }
    tecmo_intro_ready_state(frame, &state);
    fill_rect(fb, origin_x, origin_y, 256 * scale, 240 * scale, tecmo_nes_2c02_rgba(0x0FU));
    if (state.black) return true;

    for (size_t i = 0U; i < TECMO_INTRO_READY_CELL_COUNT; ++i) {
        const TecmoIntroNativeTile *cell = &asset->cells[i];
        unsigned row = (unsigned)(i / TECMO_INTRO_READY_WIDTH);
        unsigned col = (unsigned)(i % TECMO_INTRO_READY_WIDTH);
        uint8_t palette_index = cell->palette_index;
        uint32_t rgba[4];
        if (!chr_range_valid(cell->chr_offset, chr_byte_count, 16U)) return false;
        if (row >= 16U && row < 18U && col >= 8U && col < 24U) {
            palette_index = asset->masks[state.mask_index][(col - 8U) / 2U];
        } else if (row >= 18U && row < 20U && col >= 8U && col < 24U) {
            palette_index = 0U;
        }
        make_bg_palette(rgba, asset->palettes[state.palette_stage], palette_index);
        tecmo_draw_chr_tile_at_offset_ex(fb,
                                         chr_bytes,
                                         chr_byte_count,
                                         cell->chr_offset,
                                         origin_x + (int)col * 8 * scale,
                                         origin_y + (int)row * 8 * scale,
                                         scale,
                                         rgba,
                                         false,
                                         false);
    }
    return true;
}

static const TecmoIntroWarriorsTile *warriors_tile(const TecmoIntroWarriorsAsset *asset,
                                                    unsigned page,
                                                    unsigned row,
                                                    unsigned col,
                                                    uint8_t patch_count,
                                                    TecmoIntroWarriorsPatchTile const **patch_out)
{
    *patch_out = NULL;
    if (page == 0U && patch_count > 0U && row >= 10U && row < 18U && col >= 12U && col < 20U) {
        unsigned patch = patch_count - 1U;
        *patch_out = &asset->patches[patch][(row - 10U) * 8U + col - 12U];
    }
    return &asset->pages[page][row * 32U + col];
}

static bool draw_warriors_page_rows(TecmoFramebuffer *fb,
                                    const TecmoIntroWarriorsAsset *asset,
                                    const uint8_t *chr_bytes,
                                    uint64_t chr_byte_count,
                                    const uint8_t palette[16],
                                    unsigned page,
                                    unsigned row_first,
                                    unsigned row_last,
                                    uint8_t patch_count,
                                    bool lower,
                                    int page_x,
                                    int origin_y,
                                    int scale)
{
    for (unsigned row = row_first; row < row_last; ++row) {
        for (unsigned col = 0U; col < 32U; ++col) {
            const TecmoIntroWarriorsPatchTile *patch;
            const TecmoIntroWarriorsTile *cell = warriors_tile(asset,
                                                                page,
                                                                row,
                                                                col,
                                                                patch_count,
                                                                &patch);
            uint32_t offset = patch != NULL ? patch->chr_offset
                                            : (lower ? cell->lower_chr_offset
                                                     : cell->moving_chr_offset);
            uint8_t palette_index = patch != NULL ? patch->palette_index : cell->palette_index;
            uint32_t rgba[4];
            if (!chr_range_valid(offset, chr_byte_count, 16U)) return false;
            make_bg_palette(rgba, palette, palette_index);
            tecmo_draw_chr_tile_at_offset_ex(fb,
                                             chr_bytes,
                                             chr_byte_count,
                                             offset,
                                             page_x + (int)col * 8 * scale,
                                             origin_y + (int)row * 8 * scale,
                                             scale,
                                             rgba,
                                             false,
                                             false);
        }
    }
    return true;
}

bool tecmo_intro_post_arena_draw_warriors(TecmoFramebuffer *fb,
                                          const TecmoIntroWarriorsAsset *asset,
                                          const uint8_t *chr_bytes,
                                          uint64_t chr_byte_count,
                                          unsigned frame,
                                          int origin_x,
                                          int origin_y,
                                          int scale)
{
    TecmoIntroWarriorsState state;
    uint8_t palette[16];
    uint8_t sprite_palette[16];
    unsigned scroll;

    if (fb == NULL || asset == NULL || !asset->available || chr_bytes == NULL || scale <= 0) {
        return false;
    }
    tecmo_intro_warriors_state(frame, &state);
    fill_rect(fb, origin_x, origin_y, 256 * scale, 240 * scale, tecmo_nes_2c02_rgba(0x0FU));
    if (state.black || state.phase == TECMO_INTRO_WARRIORS_LOAD) return true;

    for (size_t i = 0U; i < 16U; ++i) {
        palette[i] = fade_color(asset->background_palette[i], state.palette_stage);
        sprite_palette[i] = fade_color(asset->sprite_palette[i], state.palette_stage);
    }
    scroll = state.pan == 0U ? 0U : 512U - state.pan;
    for (int copy = -1; copy <= 1; ++copy) {
        for (unsigned page = 0U; page < 2U; ++page) {
            int page_x = origin_x + ((int)page * 256 + copy * 512 - (int)scroll) * scale;
            if (!draw_warriors_page_rows(fb,
                                         asset,
                                         chr_bytes,
                                         chr_byte_count,
                                         palette,
                                         page,
                                         0U,
                                         21U,
                                         state.patch_count,
                                         false,
                                         page_x,
                                         origin_y,
                                         scale)) return false;
        }
    }
    fill_rect(fb,
              origin_x,
              origin_y + (int)TECMO_INTRO_WARRIORS_SPLIT_SCANLINE * scale,
              256 * scale,
              (240 - (int)TECMO_INTRO_WARRIORS_SPLIT_SCANLINE) * scale,
              tecmo_nes_2c02_rgba(palette[0]));
    if (!draw_warriors_page_rows(fb,
                                 asset,
                                 chr_bytes,
                                 chr_byte_count,
                                 palette,
                                 0U,
                                 21U,
                                 30U,
                                 0U,
                                 true,
                                 origin_x,
                                 origin_y,
                                 scale)) return false;

    for (size_t glyph = 0U; glyph < state.wordmark_glyph_count; ++glyph) {
        for (size_t tile_index = 0U; tile_index < 4U; ++tile_index) {
            const TecmoIntroWarriorsPatchTile *cell = &asset->wordmark[glyph * 4U + tile_index];
            unsigned row = 26U + (unsigned)(tile_index / 2U);
            unsigned col = 8U + (unsigned)glyph * 2U + (unsigned)(tile_index % 2U);
            uint32_t rgba[4];
            if (!chr_range_valid(cell->chr_offset, chr_byte_count, 16U)) return false;
            make_bg_palette(rgba, palette, cell->palette_index);
            tecmo_draw_chr_tile_at_offset_ex(fb,
                                             chr_bytes,
                                             chr_byte_count,
                                             cell->chr_offset,
                                             origin_x + (int)col * 8 * scale,
                                             origin_y + (int)row * 8 * scale,
                                             scale,
                                             rgba,
                                             false,
                                             false);
        }
    }

    if (state.sprites_visible) {
        for (size_t i = 0U; i < TECMO_INTRO_WARRIORS_PIECE_COUNT; ++i) {
            const TecmoIntroWarriorsPiece *piece = &asset->pieces[i];
            bool flip_x = (piece->flags & 1U) != 0U;
            bool flip_y = (piece->flags & 2U) != 0U;
            uint32_t rgba[4];
            uint32_t top = flip_y ? piece->bottom_chr_offset : piece->top_chr_offset;
            uint32_t bottom = flip_y ? piece->top_chr_offset : piece->bottom_chr_offset;
            int x = origin_x + (98 - (int)state.pan + piece->dx) * scale;
            int y = origin_y + (92 + piece->dy) * scale;
            if (!chr_range_valid(top, chr_byte_count, 16U) ||
                !chr_range_valid(bottom, chr_byte_count, 16U)) return false;
            make_sprite_palette(rgba, sprite_palette, piece->palette_index);
            tecmo_draw_chr_tile_at_offset_ex(fb, chr_bytes, chr_byte_count, top,
                                             x, y, scale, rgba, flip_x, flip_y);
            tecmo_draw_chr_tile_at_offset_ex(fb, chr_bytes, chr_byte_count, bottom,
                                             x, y + 8 * scale, scale, rgba, flip_x, flip_y);
        }
    }
    return true;
}

bool tecmo_intro_post_arena_draw_clippers(TecmoFramebuffer *fb,
                                          const TecmoIntroClippersAsset *asset,
                                          const uint8_t *chr_bytes,
                                          uint64_t chr_byte_count,
                                          unsigned frame,
                                          int origin_x,
                                          int origin_y,
                                          int scale)
{
    TecmoIntroClippersState state;
    const uint8_t *palette;

    if (fb == NULL || asset == NULL || !asset->available || chr_bytes == NULL || scale <= 0) {
        return false;
    }
    tecmo_intro_clippers_state(frame, &state);
    palette = asset->palettes[state.palette_stage];
    fill_rect(fb,
              origin_x,
              origin_y,
              256 * scale,
              240 * scale,
              tecmo_nes_2c02_rgba(palette[0]));

    for (unsigned scanline = 0U; scanline < 240U; ++scanline) {
        unsigned row = scanline / 8U;
        unsigned tile_scanline = scanline % 8U;
        unsigned scroll = scanline < TECMO_INTRO_CLIPPERS_LOWER_SPLIT_SCANLINE
                              ? state.scroll_x
                              : 0U;
        for (unsigned x = 0U; x < 256U; ++x) {
            unsigned source_x = x + scroll;
            unsigned page = (source_x >> 8U) & 1U;
            unsigned page_x = source_x & 0xFFU;
            unsigned col = page_x / 8U;
            unsigned tile_col = page_x % 8U;
            const TecmoIntroClippersTile *cell = &asset->pages[page][row * 32U + col];
            uint32_t rgba[4];
            uint32_t offset;
            uint8_t plane0;
            uint8_t plane1;
            uint8_t bit;
            uint8_t value;

            if (state.wordmark_visible && page == 0U && row >= 26U && row < 28U &&
                col >= 8U && col < 24U) {
                unsigned glyph = (col - 8U) / 2U;
                unsigned tile_index = (row - 26U) * 2U + (col - 8U) % 2U;
                cell = &asset->wordmark[glyph * 4U + tile_index];
            }
            offset = scanline >= TECMO_INTRO_CLIPPERS_LOWER_SPLIT_SCANLINE
                         ? cell->lower_chr_offset
                         : cell->base_chr_offset;
            if (!chr_range_valid(offset, chr_byte_count, 16U)) return false;
            make_bg_palette(rgba, palette, cell->palette_index);
            plane0 = chr_bytes[offset + tile_scanline];
            plane1 = chr_bytes[offset + tile_scanline + 8U];
            bit = (uint8_t)(7U - tile_col);
            value = (uint8_t)(((plane0 >> bit) & 1U) |
                              (((plane1 >> bit) & 1U) << 1U));
            if (value != 0U) {
                fill_rect(fb,
                          origin_x + (int)x * scale,
                          origin_y + (int)scanline * scale,
                          scale,
                          scale,
                          rgba[value]);
            }
        }
    }
    return true;
}
