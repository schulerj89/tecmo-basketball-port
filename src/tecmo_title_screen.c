#include "tecmo_title_screen.h"

#include "tecmo_asset_pack.h"
#include "tecmo_nes_video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TITLE_HEADER_SIZE 64U
#define TITLE_CELL_STRIDE 6U
#define TITLE_ATTRACT_SIZE (64U + 960U * 6U + 48U + 49U * 16U + 32U)
#define TITLE_START_SIZE (64U + 960U * 6U + 16U + 20U)
#define TITLE_CHR_SIZE 262144U

static uint16_t read_u16(const uint8_t *p) { return (uint16_t)(p[0] | ((uint16_t)p[1] << 8U)); }
static uint32_t read_u32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8U) | ((uint32_t)p[2] << 16U) | ((uint32_t)p[3] << 24U); }
static int16_t read_i16(const uint8_t *p) { return (int16_t)read_u16(p); }

static uint64_t fnv1a64(const uint8_t *bytes, uint64_t count)
{
    uint64_t hash = 14695981039346656037ULL;
    for (uint64_t i = 0U; i < count; ++i) { hash ^= bytes[i]; hash *= 1099511628211ULL; }
    return hash;
}

static int make_path(char *path, size_t size, const char *root, const char *suffix)
{
    size_t n;
    int written;
    if (root == NULL || root[0] == '\0') return -1;
    n = strlen(root);
    written = snprintf(path, size, "%s%s%s", root,
                       root[n - 1U] == '\\' || root[n - 1U] == '/' ? "" : "\\", suffix);
    return written >= 0 && (size_t)written < size ? 0 : -1;
}

static bool file_exists(const char *path)
{
    FILE *f = path != NULL ? fopen(path, "rb") : NULL;
    if (f == NULL) return false;
    fclose(f);
    return true;
}

static bool read_entry(const char *root, const char *id, uint64_t expected_count,
                       uint8_t **bytes,
                       uint64_t *count, char *pack, size_t pack_size)
{
    const char *env = getenv("TECMO_ASSETPACK");
    const char *paths[4];
    char root_build[1024], root_pack[1024];
    size_t n = 0U;
    if (env != NULL && env[0] != '\0') paths[n++] = env;
    else {
        if (make_path(root_build, sizeof(root_build), root, "build\\tecmo.assetpack") == 0) paths[n++] = root_build;
        if (make_path(root_pack, sizeof(root_pack), root, "tecmo.assetpack") == 0) paths[n++] = root_pack;
        paths[n++] = "build\\tecmo.assetpack";
        paths[n++] = "tecmo.assetpack";
    }
    for (size_t i = 0U; i < n; ++i) {
        if (!file_exists(paths[i])) continue;
        if (tecmo_asset_pack_read_entry_exact(paths[i], id, expected_count,
                                              bytes, count) != 0) return false;
        (void)snprintf(pack, pack_size, "%s", paths[i]);
        return true;
    }
    return false;
}

static bool parse_cells(TecmoTitleCell cells[TECMO_TITLE_CELL_COUNT],
                        const uint8_t *bytes, uint64_t count)
{
    (void)count;
    if (read_u32(bytes + 16U) != TECMO_TITLE_CELL_COUNT || read_u32(bytes + 20U) != 64U) return false;
    for (size_t i = 0U; i < TECMO_TITLE_CELL_COUNT; ++i) {
        const uint8_t *cell = bytes + 64U + i * 6U;
        cells[i].tile_id = cell[0];
        cells[i].palette_index = cell[1];
        cells[i].chr_offset = read_u32(cell + 2U);
        if (cells[i].palette_index > 3U || (cells[i].chr_offset & 15U) != 0U ||
            cells[i].chr_offset > 0x00100000U) return false;
    }
    return true;
}

static bool parse_attract(TecmoTitleAsset *asset, const uint8_t *bytes, uint64_t count)
{
    if (count != TITLE_ATTRACT_SIZE || memcmp(bytes, "TATR", 4U) != 0 ||
        read_u16(bytes + 4U) != 2U || read_u16(bytes + 6U) != 64U ||
        read_u16(bytes + 8U) != 32U || read_u16(bytes + 10U) != 30U ||
        read_u16(bytes + 12U) != 6U || read_u16(bytes + 14U) != 0U ||
        read_u32(bytes + 24U) != 5824U || read_u32(bytes + 28U) != count ||
        read_u32(bytes + 32U) != 5872U || read_u16(bytes + 36U) != 49U ||
        read_u16(bytes + 38U) != 16U || read_u32(bytes + 40U) != 6656U ||
        read_u16(bytes + 44U) != 642U || read_u16(bytes + 46U) != 621U) return false;
    for (size_t i = 48U; i < 64U; ++i) if (bytes[i] != 0U) return false;
    if (!parse_cells(asset->attract_cells, bytes, count)) return false;
    memcpy(asset->attract_palette, bytes + 5824U, 48U);
    memcpy(asset->attract_attributes, bytes + 6656U, 32U);
    for (size_t i = 0U; i < 49U; ++i) {
        const uint8_t *p = bytes + 5872U + i * 16U;
        asset->sprites[i].dx = read_i16(p);
        asset->sprites[i].dy = read_i16(p + 2U);
        asset->sprites[i].top_chr_offset = read_u32(p + 4U);
        asset->sprites[i].bottom_chr_offset = read_u32(p + 8U);
        asset->sprites[i].palette_index = p[12U];
        asset->sprites[i].flags = p[13U];
        if (p[14U] != 0U || p[15U] != 0U || asset->sprites[i].palette_index > 3U ||
            (asset->sprites[i].flags & ~7U) != 0U) return false;
    }
    return true;
}

static bool parse_start(TecmoTitleAsset *asset, const uint8_t *bytes, uint64_t count)
{
    static const uint8_t visible_prompt[10] = {
        0x9EU, 0x9FU, 0x8CU, 0x9DU, 0x9FU, 0xFFU, 0x92U, 0x8CU, 0x98U, 0x90U
    };
    if (count != TITLE_START_SIZE || memcmp(bytes, "TTLE", 4U) != 0 ||
        read_u16(bytes + 4U) != 1U || read_u16(bytes + 6U) != 64U ||
        read_u16(bytes + 8U) != 32U || read_u16(bytes + 10U) != 30U ||
        read_u16(bytes + 12U) != 6U || read_u16(bytes + 14U) != 1U ||
        read_u32(bytes + 24U) != 5824U || read_u32(bytes + 28U) != count ||
        read_u32(bytes + 32U) != 5840U || read_u16(bytes + 36U) != 10U ||
        read_u16(bytes + 38U) != 1U || read_u32(bytes + 40U) != 0U ||
        read_u16(bytes + 44U) != 127U || read_u16(bytes + 46U) != 126U) return false;
    for (size_t i = 48U; i < 64U; ++i) if (bytes[i] != 0U) return false;
    if (!parse_cells(asset->start_cells, bytes, count)) return false;
    memcpy(asset->start_palette, bytes + 5824U, 16U);
    memcpy(asset->prompt_rows, bytes + 5840U, 20U);
    for (size_t i = 0U; i < 10U; ++i) if (asset->prompt_rows[0][i] != 0xFFU) return false;
    if (memcmp(asset->prompt_rows[1], visible_prompt, sizeof(visible_prompt)) != 0) return false;
    return true;
}

bool tecmo_title_asset_load(TecmoTitleAsset *asset, const char *root)
{
    uint8_t *attract = NULL, *start = NULL, *chr = NULL;
    uint64_t attract_count = 0U, start_count = 0U, chr_count = 0U;
    char pack[1024] = {0};
    if (asset == NULL) return false;
    memset(asset, 0, sizeof(*asset));
    if (!read_entry(root, "title/attract-continuation", TITLE_ATTRACT_SIZE,
                    &attract, &attract_count, pack, sizeof(pack)) ||
        !read_entry(root, "title/start-screen", TITLE_START_SIZE,
                    &start, &start_count, pack, sizeof(pack)) ||
        !parse_attract(asset, attract, attract_count) || !parse_start(asset, start, start_count) ||
        tecmo_asset_pack_read_entry_exact(pack, "chr/all", TITLE_CHR_SIZE,
                                          &chr, &chr_count) != 0) {
        tecmo_asset_pack_free(attract); tecmo_asset_pack_free(start); tecmo_asset_pack_free(chr);
        (void)snprintf(asset->status, sizeof(asset->status), "TATR-2/TTLE-1 assets unavailable or rejected");
        return false;
    }
    asset->chr_byte_count = chr_count;
    asset->chr_fingerprint = fnv1a64(chr, chr_count);
    asset->attract_available = true;
    asset->start_available = true;
    if (!tecmo_title_asset_chr_available(asset, chr, chr_count)) {
        asset->attract_available = asset->start_available = false;
    }
    tecmo_asset_pack_free(attract); tecmo_asset_pack_free(start); tecmo_asset_pack_free(chr);
    (void)snprintf(asset->status, sizeof(asset->status), "%s",
                   asset->start_available ? "TATR-2/TTLE-1 native assetpack" : "title chr/all rejected");
    return asset->start_available;
}

bool tecmo_title_asset_chr_available(const TecmoTitleAsset *asset, const uint8_t *chr, uint64_t count)
{
    if (asset == NULL || chr == NULL || count == 0U ||
        (asset->chr_byte_count != 0U && (asset->chr_byte_count != count || asset->chr_fingerprint != fnv1a64(chr, count)))) return false;
    for (size_t i = 0U; i < 960U; ++i)
        if ((uint64_t)asset->attract_cells[i].chr_offset + 16U > count ||
            (uint64_t)asset->start_cells[i].chr_offset + 16U > count) return false;
    for (size_t i = 0U; i < 49U; ++i)
        if ((uint64_t)asset->sprites[i].bottom_chr_offset + 16U > count) return false;
    return true;
}

static uint8_t attract_palette_index(const TecmoTitleAsset *asset, unsigned frame,
                                     unsigned row, unsigned col, uint8_t normal)
{
    const uint8_t *attrs = NULL;
    unsigned attr_index = (row / 4U) * 8U + col / 4U;
    if (frame >= 376U && frame < 420U) attrs = asset->attract_attributes[((frame - 376U) / 2U) & 1U];
    else if (frame >= 420U && frame < 440U) attrs = asset->attract_attributes[0];
    else if (frame >= 440U) attrs = asset->attract_attributes[1];
    if (attrs == NULL || attr_index >= 16U) return normal;
    return tecmo_nes_attribute_palette_index(attrs[attr_index], (int)row, (int)col);
}

static void draw_cells(TecmoFramebuffer *fb, const TecmoTitleCell *cells,
                       const uint8_t *palette, const uint8_t *chr, uint64_t chr_count,
                       int ox, int oy, int scale, const TecmoTitleAsset *asset,
                       unsigned frame, bool attract)
{
    for (size_t i = 0U; i < 960U; ++i) {
        unsigned row = (unsigned)(i / 32U), col = (unsigned)(i % 32U);
        uint8_t pi = attract ? attract_palette_index(asset, frame, row, col, cells[i].palette_index)
                             : cells[i].palette_index;
        uint32_t offset = cells[i].chr_offset;
        uint32_t rgba[4];
        rgba[0] = tecmo_nes_2c02_rgba(palette[0]);
        for (size_t c = 1U; c < 4U; ++c) rgba[c] = tecmo_nes_2c02_rgba(palette[(size_t)pi * 4U + c]);
        tecmo_draw_chr_tile_at_offset_ex(fb, chr, chr_count, offset,
                                         ox + (int)col * 8 * scale, oy + (int)row * 8 * scale,
                                         scale, rgba, false, false);
    }
}

bool tecmo_title_attract_draw(TecmoFramebuffer *fb, const TecmoTitleAsset *asset,
                              const uint8_t *chr, uint64_t count, unsigned frame,
                              int ox, int oy, int scale)
{
    const int anchor_x = 105;
    const int anchor_y = 97;
    if (fb == NULL || asset == NULL || !asset->attract_available || chr == NULL || frame < 6U) return false;
    draw_cells(fb, asset->attract_cells, asset->attract_palette, chr, count,
               ox, oy, scale, asset, frame, true);
    if (frame < 126U) return true;
    for (size_t left = 49U; left > 0U; --left) {
        const TecmoTitleSprite *s = &asset->sprites[left - 1U];
        uint32_t rgba[4] = {0U};
        size_t base = (frame < 376U ? 16U : 32U) + (size_t)s->palette_index * 4U;
        for (size_t c = 1U; c < 4U; ++c) rgba[c] = tecmo_nes_2c02_rgba(asset->attract_palette[base + c]);
        tecmo_draw_chr_tile_at_offset_ex(fb, chr, count, s->top_chr_offset,
                                         ox + (anchor_x + s->dx) * scale, oy + (anchor_y + s->dy + 1) * scale,
                                         scale, rgba, (s->flags & 2U) != 0U, (s->flags & 4U) != 0U);
        tecmo_draw_chr_tile_at_offset_ex(fb, chr, count, s->bottom_chr_offset,
                                         ox + (anchor_x + s->dx) * scale, oy + (anchor_y + s->dy + 9) * scale,
                                         scale, rgba, (s->flags & 2U) != 0U, (s->flags & 4U) != 0U);
    }
    return true;
}

bool tecmo_title_start_draw(TecmoFramebuffer *fb, const TecmoTitleAsset *asset,
                            const uint8_t *chr, uint64_t count, bool confirming,
                            unsigned frame, int ox, int oy, int scale)
{
    TecmoTitleCell cells[TECMO_TITLE_CELL_COUNT];
    bool visible = !confirming || frame == 0U || (((frame - 1U) / 7U) & 1U) != 0U;
    if (fb == NULL || asset == NULL || !asset->start_available || chr == NULL) return false;
    memcpy(cells, asset->start_cells, sizeof(cells));
    if (confirming && frame > 0U) {
        const uint8_t *row = asset->prompt_rows[visible ? 1U : 0U];
        for (size_t i = 0U; i < 10U; ++i) {
            uint8_t tile = row[i];
            size_t index = 17U * 32U + 11U + i;
            uint8_t selector = tile < 0x80U ? 0xFEU : 0xFAU;
            cells[index].tile_id = tile;
            cells[index].chr_offset = (uint32_t)selector * 1024U + (uint32_t)(tile & 0x7FU) * 16U;
        }
    }
    draw_cells(fb, cells, asset->start_palette, chr, count, ox, oy, scale,
               asset, frame, false);
    return true;
}

unsigned tecmo_title_attract_reset_frame(void) { return 642U; }
bool tecmo_title_attract_natural_complete(unsigned frame) { return frame >= 621U; }
unsigned tecmo_title_confirmation_handoff_frame(void) { return 127U; }
