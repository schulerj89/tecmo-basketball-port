#include "tecmo_intro_screen.h"

#include "tecmo_asset_pack.h"
#include "tecmo_nes_video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TISC_HEADER_SIZE 64U
#define TISC_CELL_STRIDE 6U
#define TISC_SPRITE_STRIDE 12U
#define TISC_CELLS_OFFSET TISC_HEADER_SIZE
#define TISC_PALETTES_OFFSET \
    (TISC_CELLS_OFFSET + TECMO_INTRO_SCREEN_CELL_COUNT * TISC_CELL_STRIDE)

static uint16_t read_u16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U));
}

static uint32_t read_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) | ((uint32_t)bytes[3] << 24U);
}

static int16_t read_i16(const uint8_t *bytes)
{
    return (int16_t)read_u16(bytes);
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

static bool range_valid(uint32_t offset, uint64_t size, uint64_t total)
{
    return (uint64_t)offset <= total && size <= total - offset;
}

static uint8_t fade_in_color(uint8_t target, unsigned level)
{
    uint8_t maximum;
    uint8_t high;
    if (target == 0x0FU) return 0x0FU;
    maximum = (uint8_t)(level << 4U);
    high = target & 0x30U;
    if (high > maximum) high = maximum;
    return (uint8_t)(high | (target & 0x0FU));
}

static uint8_t fade_out_color(uint8_t current)
{
    if (current == 0x0FU) return 0x0FU;
    return (current & 0x30U) >= 0x10U ? (uint8_t)(current - 0x10U) : 0x0FU;
}

static int make_path(char *path, size_t size, const char *root, const char *suffix)
{
    size_t length;
    int written;
    if (path == NULL || size == 0U || root == NULL || root[0] == '\0') return -1;
    length = strlen(root);
    written = snprintf(path, size, "%s%s%s", root,
                       root[length - 1U] == '\\' || root[length - 1U] == '/' ? "" : "\\",
                       suffix);
    return written >= 0 && (size_t)written < size ? 0 : -1;
}

static bool file_exists(const char *path)
{
    FILE *file = path != NULL ? fopen(path, "rb") : NULL;
    if (file == NULL) return false;
    fclose(file);
    return true;
}

static bool read_entry(const char *root,
                       const char *entry_id,
                       uint8_t **bytes,
                       uint64_t *size,
                       char *pack_path,
                       size_t pack_path_size)
{
    const char *env = getenv("TECMO_ASSETPACK");
    const char *paths[4];
    char root_build[1024];
    char root_pack[1024];
    size_t count = 0U;
    if (env != NULL && env[0] != '\0') {
        paths[count++] = env;
    } else {
        if (make_path(root_build, sizeof(root_build), root, "build\\tecmo.assetpack") == 0) {
            paths[count++] = root_build;
        }
        if (make_path(root_pack, sizeof(root_pack), root, "tecmo.assetpack") == 0) {
            paths[count++] = root_pack;
        }
        paths[count++] = "build\\tecmo.assetpack";
        paths[count++] = "tecmo.assetpack";
    }
    for (size_t i = 0U; i < count; ++i) {
        if (!file_exists(paths[i])) continue;
        if (tecmo_asset_pack_read_entry(paths[i], entry_id, bytes, size) != 0) return false;
        (void)snprintf(pack_path, pack_path_size, "%s", paths[i]);
        return true;
    }
    return false;
}

static bool parse_asset(TecmoIntroScreenAsset *asset,
                        const uint8_t *bytes,
                        uint64_t byte_count,
                        TecmoIntroScreenKind expected_kind)
{
    static const uint16_t presents_frames[] = {
        0U, 4U, 8U, 12U, 16U, 123U, 125U, 127U, 129U
    };
    static const uint16_t license_frames[] = {
        0U, 36U, 40U, 44U, 48U, 275U
    };
    const uint16_t *expected_frames;
    size_t expected_frame_count;
    uint16_t stage_count;
    uint16_t palette_stride;
    uint16_t sprite_count;
    uint16_t sprite_stride;
    uint32_t cells_offset;
    uint32_t palettes_offset;
    uint32_t frames_offset;
    uint32_t sprites_offset;
    uint64_t expected_size;
    size_t nonblank_cells = 0U;
    if (asset == NULL || bytes == NULL || byte_count < TISC_HEADER_SIZE ||
        memcmp(bytes, "TISC", 4U) != 0 || read_u16(bytes + 4U) != 1U ||
        read_u16(bytes + 6U) != TISC_HEADER_SIZE ||
        read_u16(bytes + 8U) != TECMO_INTRO_SCREEN_WIDTH ||
        read_u16(bytes + 10U) != TECMO_INTRO_SCREEN_HEIGHT ||
        read_u16(bytes + 12U) != TISC_CELL_STRIDE ||
        read_u16(bytes + 14U) != (uint16_t)expected_kind ||
        read_u32(bytes + 16U) != TECMO_INTRO_SCREEN_CELL_COUNT ||
        read_u32(bytes + 40U) != byte_count) {
        return false;
    }
    for (size_t i = 54U; i < TISC_HEADER_SIZE; ++i) if (bytes[i] != 0U) return false;
    stage_count = read_u16(bytes + 24U);
    palette_stride = read_u16(bytes + 26U);
    sprite_count = read_u16(bytes + 38U);
    sprite_stride = read_u16(bytes + 48U);
    cells_offset = read_u32(bytes + 20U);
    palettes_offset = read_u32(bytes + 28U);
    frames_offset = read_u32(bytes + 32U);
    sprites_offset = read_u32(bytes + 44U);
    expected_frames = expected_kind == TECMO_INTRO_SCREEN_PRESENTS
                          ? presents_frames
                          : license_frames;
    expected_frame_count = expected_kind == TECMO_INTRO_SCREEN_PRESENTS
                               ? sizeof(presents_frames) / sizeof(presents_frames[0])
                               : sizeof(license_frames) / sizeof(license_frames[0]);
    if (stage_count != expected_frame_count ||
        stage_count > TECMO_INTRO_SCREEN_MAX_PALETTE_STAGES ||
        palette_stride != (expected_kind == TECMO_INTRO_SCREEN_PRESENTS ? 32U : 16U) ||
        read_u16(bytes + 36U) !=
            (expected_kind == TECMO_INTRO_SCREEN_PRESENTS ? 133U : 277U) ||
        sprite_count !=
            (expected_kind == TECMO_INTRO_SCREEN_PRESENTS ? 20U : 0U) ||
        sprite_count > TECMO_INTRO_SCREEN_MAX_SPRITES ||
        sprite_stride != TISC_SPRITE_STRIDE ||
        read_u16(bytes + 50U) != 0U ||
        read_u16(bytes + 52U) !=
            (expected_kind == TECMO_INTRO_SCREEN_PRESENTS ? 131U : 0U) ||
        cells_offset != TISC_CELLS_OFFSET || palettes_offset != TISC_PALETTES_OFFSET ||
        frames_offset != palettes_offset + (uint32_t)stage_count * palette_stride ||
        sprites_offset != frames_offset + (uint32_t)stage_count * 2U) return false;
    expected_size = (uint64_t)sprites_offset +
                    (uint64_t)sprite_count * TISC_SPRITE_STRIDE;
    if (expected_size != byte_count ||
        !range_valid(cells_offset, TECMO_INTRO_SCREEN_CELL_COUNT * TISC_CELL_STRIDE, byte_count) ||
        !range_valid(palettes_offset, (uint64_t)stage_count * palette_stride, byte_count) ||
        !range_valid(frames_offset, (uint64_t)stage_count * 2U, byte_count) ||
        !range_valid(sprites_offset,
                     (uint64_t)sprite_count * TISC_SPRITE_STRIDE,
                     byte_count)) return false;

    asset->kind = expected_kind;
    asset->palette_stage_count = (uint8_t)stage_count;
    asset->palette_stride = (uint8_t)palette_stride;
    asset->duration_frames = read_u16(bytes + 36U);
    asset->sprite_count = sprite_count;
    asset->sprite_first_frame = read_u16(bytes + 50U);
    asset->sprite_hide_frame = read_u16(bytes + 52U);
    for (size_t stage = 0U; stage < stage_count; ++stage) {
        asset->palette_frames[stage] = read_u16(bytes + frames_offset + stage * 2U);
        if (asset->palette_frames[stage] != expected_frames[stage] ||
            asset->palette_frames[stage] >= asset->duration_frames) return false;
        memcpy(asset->palettes[stage],
               bytes + palettes_offset + stage * palette_stride,
               palette_stride);
        for (size_t color = 0U; color < palette_stride; ++color) {
            if (asset->palettes[stage][color] > 0x3FU) return false;
        }
    }
    for (size_t color = 0U; color < palette_stride; ++color) {
        uint8_t target = asset->palettes[4][color];
        if (asset->palettes[0][color] != 0x0FU ||
            asset->palettes[1][color] != fade_in_color(target, 0U) ||
            asset->palettes[2][color] != fade_in_color(target, 1U) ||
            asset->palettes[3][color] != fade_in_color(target, 2U)) return false;
        if (expected_kind == TECMO_INTRO_SCREEN_PRESENTS) {
            uint8_t current = target;
            for (size_t stage = 5U; stage < stage_count; ++stage) {
                current = fade_out_color(current);
                if (asset->palettes[stage][color] != current) return false;
            }
        } else if (asset->palettes[5][color] != 0x0FU) {
            return false;
        }
    }

    for (size_t i = 0U; i < TECMO_INTRO_SCREEN_CELL_COUNT; ++i) {
        const uint8_t *cell = bytes + cells_offset + i * TISC_CELL_STRIDE;
        asset->cells[i].tile_id = cell[0];
        asset->cells[i].palette_index = cell[1];
        asset->cells[i].chr_offset = read_u32(cell + 2U);
        if (cell[1] > 3U || (asset->cells[i].chr_offset & 0x0FU) != 0U) return false;
        if (cell[0] != 0xFFU) ++nonblank_cells;
    }
    if (nonblank_cells !=
        (expected_kind == TECMO_INTRO_SCREEN_PRESENTS ? 58U : 104U)) return false;
    for (size_t i = 0U; i < sprite_count; ++i) {
        const uint8_t *sprite = bytes + sprites_offset + i * TISC_SPRITE_STRIDE;
        asset->sprites[i].x = read_i16(sprite);
        asset->sprites[i].y = read_i16(sprite + 2U);
        asset->sprites[i].chr_offset = read_u32(sprite + 4U);
        asset->sprites[i].palette_index = sprite[8U];
        asset->sprites[i].flags = sprite[9U];
        if (asset->sprites[i].palette_index > 3U ||
            (asset->sprites[i].flags &
             ~(TECMO_INTRO_SCREEN_SPRITE_FLIP_HORIZONTAL |
               TECMO_INTRO_SCREEN_SPRITE_FLIP_VERTICAL)) != 0U ||
            (asset->sprites[i].chr_offset & 0x0FU) != 0U ||
            asset->sprites[i].x < 80 || asset->sprites[i].x > 104 ||
            asset->sprites[i].y < 64 || asset->sprites[i].y > 120 ||
            read_u16(sprite + 10U) != 0U) return false;
    }
    return true;
}

bool tecmo_intro_screen_load(TecmoIntroScreenAsset *asset,
                             const char *project_root,
                             const char *entry_id,
                             TecmoIntroScreenKind expected_kind)
{
    uint8_t *bytes = NULL;
    uint8_t *chr = NULL;
    uint64_t byte_count = 0U;
    uint64_t chr_count = 0U;
    char pack_path[1024] = {0};
    if (asset == NULL || entry_id == NULL) return false;
    memset(asset, 0, sizeof(*asset));
    if (!read_entry(project_root, entry_id, &bytes, &byte_count,
                    pack_path, sizeof(pack_path))) {
        (void)snprintf(asset->status, sizeof(asset->status), "TISC-1 asset unavailable");
        return false;
    }
    if (!parse_asset(asset, bytes, byte_count, expected_kind)) {
        tecmo_asset_pack_free(bytes);
        (void)snprintf(asset->status, sizeof(asset->status), "TISC-1 asset rejected");
        return false;
    }
    tecmo_asset_pack_free(bytes);
    if (tecmo_asset_pack_read_entry(pack_path, "chr/all", &chr, &chr_count) != 0 ||
        !tecmo_intro_screen_chr_available(asset, chr, chr_count)) {
        tecmo_asset_pack_free(chr);
        (void)snprintf(asset->status, sizeof(asset->status), "TISC-1 chr/all rejected");
        return false;
    }
    asset->chr_byte_count = chr_count;
    asset->chr_fingerprint = fnv1a64(chr, chr_count);
    tecmo_asset_pack_free(chr);
    asset->available = true;
    (void)snprintf(asset->status, sizeof(asset->status), "TISC-1 native assetpack");
    return true;
}

bool tecmo_intro_screen_chr_available(const TecmoIntroScreenAsset *asset,
                                      const uint8_t *chr_bytes,
                                      uint64_t chr_byte_count)
{
    if (asset == NULL || chr_bytes == NULL || chr_byte_count == 0U) return false;
    if (asset->chr_byte_count != 0U &&
        (asset->chr_byte_count != chr_byte_count ||
         asset->chr_fingerprint != fnv1a64(chr_bytes, chr_byte_count))) return false;
    for (size_t i = 0U; i < TECMO_INTRO_SCREEN_CELL_COUNT; ++i) {
        if ((uint64_t)asset->cells[i].chr_offset + 16U > chr_byte_count) return false;
    }
    for (size_t i = 0U; i < asset->sprite_count; ++i) {
        if ((uint64_t)asset->sprites[i].chr_offset + 16U > chr_byte_count) return false;
    }
    return true;
}

uint8_t tecmo_intro_screen_palette_stage(const TecmoIntroScreenAsset *asset,
                                         unsigned frame)
{
    uint8_t stage = 0U;
    if (asset == NULL) return 0U;
    for (uint8_t i = 1U; i < asset->palette_stage_count; ++i) {
        if (frame >= asset->palette_frames[i]) stage = i;
    }
    return stage;
}

bool tecmo_intro_screen_draw(TecmoFramebuffer *fb,
                             const TecmoIntroScreenAsset *asset,
                             const uint8_t *chr_bytes,
                             uint64_t chr_byte_count,
                             unsigned frame,
                             int origin_x,
                             int origin_y,
                             int scale)
{
    uint8_t stage;
    if (fb == NULL || asset == NULL || !asset->available || chr_bytes == NULL || scale <= 0) {
        return false;
    }
    stage = tecmo_intro_screen_palette_stage(asset, frame);
    for (size_t i = 0U; i < TECMO_INTRO_SCREEN_CELL_COUNT; ++i) {
        const TecmoIntroScreenCell *cell = &asset->cells[i];
        size_t base = (size_t)cell->palette_index * 4U;
        uint32_t rgba[4];
        unsigned row;
        unsigned col;
        if (cell->tile_id == 0xFFU) continue;
        if ((uint64_t)cell->chr_offset + 16U > chr_byte_count) return false;
        rgba[0] = tecmo_nes_2c02_rgba(asset->palettes[stage][0]);
        for (size_t color = 1U; color < 4U; ++color) {
            rgba[color] = tecmo_nes_2c02_rgba(asset->palettes[stage][base + color]);
        }
        row = (unsigned)(i / TECMO_INTRO_SCREEN_WIDTH);
        col = (unsigned)(i % TECMO_INTRO_SCREEN_WIDTH);
        tecmo_draw_chr_tile_at_offset_ex(fb, chr_bytes, chr_byte_count,
                                         cell->chr_offset,
                                         origin_x + (int)col * 8 * scale,
                                         origin_y + (int)row * 8 * scale,
                                         scale, rgba, false, false);
    }
    if (asset->sprite_count > 0U &&
        frame >= asset->sprite_first_frame &&
        frame < asset->sprite_hide_frame) {
        for (size_t remaining = asset->sprite_count; remaining > 0U; --remaining) {
            const TecmoIntroScreenSprite *sprite = &asset->sprites[remaining - 1U];
            size_t base = 16U + (size_t)sprite->palette_index * 4U;
            uint32_t rgba[4] = {0U, 0U, 0U, 0U};
            if (base + 3U >= asset->palette_stride ||
                (uint64_t)sprite->chr_offset + 16U > chr_byte_count) return false;
            for (size_t color = 1U; color < 4U; ++color) {
                rgba[color] = tecmo_nes_2c02_rgba(
                    asset->palettes[stage][base + color]);
            }
            tecmo_draw_chr_tile_at_offset_ex(
                fb,
                chr_bytes,
                chr_byte_count,
                sprite->chr_offset,
                origin_x + (int)sprite->x * scale,
                origin_y + ((int)sprite->y + 1) * scale,
                scale,
                rgba,
                (sprite->flags & TECMO_INTRO_SCREEN_SPRITE_FLIP_HORIZONTAL) != 0U,
                (sprite->flags & TECMO_INTRO_SCREEN_SPRITE_FLIP_VERTICAL) != 0U);
        }
    }
    return true;
}
