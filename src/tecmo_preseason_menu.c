#include "tecmo_preseason_menu.h"

#include "tecmo_asset_pack.h"
#include "tecmo_nes_video.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PRESEASON_ENTRY_ID "menu/preseason"
#define PRESEASON_PAYLOAD_SIZE 26736U
#define PRESEASON_PAYLOAD_FNV1A32 0xD9EE49F4U
#define PRESEASON_TSGM_SIZE 14112U
#define PRESEASON_TSGM_FNV1A32 0xDF89006BU
#define PRESEASON_CHR_SIZE 262144U
#define PRESEASON_CHR_FNV1A32 0xF6F6E854U
#define PRESEASON_CHR_FNV1A64 0x96A64F53B240ABB4ULL
#define PRESEASON_OVERLAY_DESCS_OFFSET 256U
#define PRESEASON_OVERLAY_CELLS_OFFSET 304U
#define PRESEASON_TEAM_CELLS_OFFSET 3364U
#define PRESEASON_TEAM_PALETTES_OFFSET 26404U
#define PRESEASON_MARKERS_OFFSET 26468U
#define PRESEASON_COORDS_OFFSET 26628U
#define PRESEASON_DIVISION_OFFSETS_OFFSET 26682U
#define PRESEASON_DIVISION_COUNTS_OFFSET 26686U
#define PRESEASON_TEAM_IDS_OFFSET 26690U
#define PRESEASON_OWNERSHIP_OFFSET 26717U
#define PRESEASON_CONFIGS_OFFSET 26729U
#define PRESEASON_MARKER_SELECTOR 0x30U

typedef enum PreseasonButton {
    PRESEASON_BUTTON_RIGHT = 0x01,
    PRESEASON_BUTTON_LEFT = 0x02,
    PRESEASON_BUTTON_DOWN = 0x04,
    PRESEASON_BUTTON_UP = 0x08,
    PRESEASON_BUTTON_START = 0x10,
    PRESEASON_BUTTON_SELECT = 0x20,
    PRESEASON_BUTTON_B = 0x40,
    PRESEASON_BUTTON_A = 0x80
} PreseasonButton;

typedef enum PreseasonSelectionAction {
    PRESEASON_SELECTION_NONE,
    PRESEASON_SELECTION_ACCEPT,
    PRESEASON_SELECTION_CANCEL
} PreseasonSelectionAction;

static uint16_t read_u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8U));
}

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8U) |
           ((uint32_t)p[2] << 16U) | ((uint32_t)p[3] << 24U);
}

static int16_t read_i16(const uint8_t *p)
{
    return (int16_t)read_u16(p);
}

static uint32_t fnv1a32(const uint8_t *bytes, uint64_t count)
{
    uint32_t hash = 2166136261U;
    for (uint64_t i = 0U; i < count; ++i) {
        hash ^= bytes[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint64_t fnv1a64(const uint8_t *bytes, uint64_t count)
{
    uint64_t hash = 14695981039346656037ULL;
    for (uint64_t i = 0U; i < count; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static uint32_t bg_chr_offset(uint8_t tile, uint8_t r0, uint8_t r1)
{
    uint8_t selector = tile < 128U ? r0 : r1;
    return (uint32_t)selector * 1024U + (uint32_t)(tile & 0x7FU) * 16U;
}

static int make_path(char *path, size_t size, const char *root, const char *suffix)
{
    size_t n;
    int written;
    if (path == NULL || size == 0U || root == NULL || root[0] == '\0') return -1;
    n = strlen(root);
    written = snprintf(path, size, "%s%s%s", root,
                       root[n - 1U] == '\\' || root[n - 1U] == '/' ? "" : "\\",
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

static bool read_dependencies_from_selected_pack(const char *root,
                                                 uint8_t **preseason,
                                                 uint64_t *preseason_count,
                                                 uint8_t **start_menu,
                                                 uint64_t *start_menu_count,
                                                 uint8_t **chr,
                                                 uint64_t *chr_count)
{
    const char *env = getenv("TECMO_ASSETPACK");
    const char *paths[4];
    char root_build[1024];
    char root_pack[1024];
    size_t path_count = 0U;

    if (env != NULL && env[0] != '\0') {
        paths[path_count++] = env;
    } else {
        if (make_path(root_build, sizeof(root_build), root, "build\\tecmo.assetpack") == 0)
            paths[path_count++] = root_build;
        if (make_path(root_pack, sizeof(root_pack), root, "tecmo.assetpack") == 0)
            paths[path_count++] = root_pack;
        paths[path_count++] = "build\\tecmo.assetpack";
        paths[path_count++] = "tecmo.assetpack";
    }

    for (size_t i = 0U; i < path_count; ++i) {
        if (!file_exists(paths[i])) continue;
        if (tecmo_asset_pack_read_entry_exact(paths[i], PRESEASON_ENTRY_ID,
                                              PRESEASON_PAYLOAD_SIZE,
                                              preseason, preseason_count) != 0)
            return false;
        if (tecmo_asset_pack_read_entry_exact(paths[i], "menu/start-game",
                                              PRESEASON_TSGM_SIZE,
                                              start_menu, start_menu_count) != 0) {
            tecmo_asset_pack_free(*preseason);
            *preseason = NULL;
            *preseason_count = 0U;
            return false;
        }
        if (tecmo_asset_pack_read_entry_exact(paths[i], "chr/all",
                                              PRESEASON_CHR_SIZE,
                                              chr, chr_count) != 0) {
            tecmo_asset_pack_free(*preseason);
            tecmo_asset_pack_free(*start_menu);
            *preseason = NULL;
            *start_menu = NULL;
            *preseason_count = 0U;
            *start_menu_count = 0U;
            return false;
        }
        return true;
    }
    return false;
}

static bool parse_cell(TecmoStartGameMenuCell *cell,
                       const uint8_t *bytes,
                       uint8_t r0,
                       uint8_t r1)
{
    cell->tile_id = bytes[0];
    cell->palette_index = bytes[1];
    cell->chr_offset = read_u32(bytes + 2U);
    return cell->palette_index <= 3U &&
           cell->chr_offset == bg_chr_offset(cell->tile_id, r0, r1);
}

static bool parse_sprite(TecmoStartGameMenuSprite *sprite,
                         const uint8_t *bytes,
                         uint8_t selector)
{
    sprite->dx = read_i16(bytes);
    sprite->dy = read_i16(bytes + 2U);
    sprite->top_chr_offset = read_u32(bytes + 4U);
    sprite->bottom_chr_offset = read_u32(bytes + 8U);
    sprite->palette_index = bytes[12U];
    sprite->flags = bytes[13U];
    return sprite->bottom_chr_offset == sprite->top_chr_offset + 16U &&
           sprite->top_chr_offset / 1024U == selector &&
           (sprite->top_chr_offset % 1024U) % 32U == 0U &&
           sprite->palette_index == 0U && sprite->flags == 0U &&
           bytes[14U] == 0U && bytes[15U] == 0U;
}

static bool parse_payload(TecmoPreseasonAsset *asset,
                          const uint8_t *bytes,
                          uint64_t count)
{
    static const uint32_t overlay_starts[3] = {0U, 210U, 406U};
    static const uint16_t overlay_counts[3] = {210U, 196U, 104U};
    static const uint16_t overlay_widths[3] = {15U, 14U, 13U};
    static const uint16_t overlay_heights[3] = {14U, 14U, 8U};
    static const uint16_t overlay_x[3] = {5U, 7U, 7U};
    static const uint16_t overlay_y[3] = {11U, 13U, 11U};
    static const uint8_t team_r0[4] = {0xE0U, 0xE4U, 0xE8U, 0xECU};
    static const uint8_t team_r1[4] = {0xE2U, 0xE6U, 0xEAU, 0xEEU};
    bool seen_ids[27] = {false};

    if (bytes == NULL || count != PRESEASON_PAYLOAD_SIZE ||
        fnv1a32(bytes, count) != PRESEASON_PAYLOAD_FNV1A32 ||
        memcmp(bytes, "TPRE", 4U) != 0 ||
        read_u16(bytes + 4U) != 1U || read_u16(bytes + 6U) != 256U ||
        read_u16(bytes + 8U) != 32U || read_u16(bytes + 10U) != 30U ||
        read_u16(bytes + 12U) != 3U || read_u16(bytes + 14U) != 4U ||
        read_u32(bytes + 16U) != TECMO_PRESEASON_OVERLAY_CELL_COUNT ||
        read_u32(bytes + 20U) != TECMO_PRESEASON_TEAM_CELL_COUNT ||
        read_u32(bytes + 24U) != PRESEASON_OVERLAY_DESCS_OFFSET ||
        read_u32(bytes + 28U) != PRESEASON_OVERLAY_CELLS_OFFSET ||
        read_u32(bytes + 32U) != PRESEASON_TEAM_CELLS_OFFSET ||
        read_u32(bytes + 36U) != PRESEASON_TEAM_PALETTES_OFFSET ||
        read_u32(bytes + 40U) != PRESEASON_MARKERS_OFFSET ||
        read_u32(bytes + 44U) != PRESEASON_COORDS_OFFSET ||
        read_u32(bytes + 48U) != PRESEASON_DIVISION_OFFSETS_OFFSET ||
        read_u32(bytes + 52U) != PRESEASON_DIVISION_COUNTS_OFFSET ||
        read_u32(bytes + 56U) != PRESEASON_TEAM_IDS_OFFSET ||
        read_u32(bytes + 60U) != PRESEASON_OWNERSHIP_OFFSET ||
        read_u32(bytes + 64U) != PRESEASON_CONFIGS_OFFSET ||
        read_u32(bytes + 68U) != PRESEASON_PAYLOAD_SIZE ||
        read_u32(bytes + 72U) != PRESEASON_TSGM_SIZE ||
        read_u32(bytes + 76U) != PRESEASON_TSGM_FNV1A32 ||
        read_u32(bytes + 80U) != PRESEASON_CHR_SIZE ||
        read_u32(bytes + 84U) != PRESEASON_CHR_FNV1A32 ||
        read_u16(bytes + 88U) != 5U || read_u16(bytes + 90U) != 16U ||
        read_u16(bytes + 92U) != 27U || bytes[94U] != 7U ||
        bytes[95U] != 3U || bytes[96U] != 4U || bytes[97U] != 1U ||
        bytes[98U] != 1U || bytes[99U] != 1U || bytes[100U] != 1U ||
        bytes[101U] != 5U || bytes[102U] != 8U || bytes[103U] != 0U ||
        read_u16(bytes + 104U) > 255U || read_u16(bytes + 106U) > 239U ||
        read_u16(bytes + 108U) == 0U || read_u16(bytes + 110U) > 255U ||
        read_u16(bytes + 116U) != 16U || read_u16(bytes + 118U) != 32U ||
        read_u32(bytes + 120U) != 0xC240U ||
        read_u32(bytes + 124U) != 0xC250U ||
        bytes[128U] != PRESEASON_MARKER_SELECTOR ||
        bytes[129U] != 0x03U || bytes[130U] != 0xC0U ||
        bytes[131U] != 0U || bytes[132U] != 31U ||
        read_u16(bytes + 133U) > 255U || read_u16(bytes + 135U) > 239U ||
        read_u16(bytes + 137U) == 0U ||
        bytes[139U] == 0U || bytes[140U] == 0U ||
        bytes[141U] <= bytes[139U] ||
        (uint8_t)(bytes[141U] - bytes[139U]) !=
            (uint8_t)(3U * bytes[140U]) ||
        bytes[142U] == 0U || bytes[143U] == 0U ||
        bytes[144U] <= bytes[142U] ||
        (uint8_t)(bytes[144U] - bytes[142U]) !=
            (uint8_t)(3U * bytes[143U]) ||
        bytes[145U] == 0U || bytes[146U] <= bytes[145U] ||
        bytes[147U] <= bytes[146U] || bytes[148U] <= bytes[147U] ||
        bytes[149U] == 0U || bytes[150U] <= bytes[149U] ||
        bytes[151U] <= bytes[150U] || bytes[152U] <= bytes[151U] ||
        bytes[148U] >= read_u16(bytes + 116U) ||
        bytes[152U] >= read_u16(bytes + 118U))
        return false;
    for (size_t i = 153U; i < 256U; ++i)
        if (bytes[i] != 0U) return false;
    for (size_t i = 0U; i < 4U; ++i)
        if (bytes[112U + i] > 239U ||
            (i > 0U && bytes[112U + i] <= bytes[111U + i]))
            return false;

    for (size_t i = 0U; i < 3U; ++i) {
        const uint8_t *source = bytes + PRESEASON_OVERLAY_DESCS_OFFSET + i * 16U;
        TecmoStartGameMenuOverlay *overlay = &asset->overlays[i];
        overlay->cell_start = read_u32(source);
        overlay->cell_count = read_u16(source + 4U);
        overlay->width = read_u16(source + 6U);
        overlay->height = read_u16(source + 8U);
        overlay->x = read_u16(source + 10U);
        overlay->y = read_u16(source + 12U);
        overlay->type = source[14U];
        if (source[15U] != 0U || overlay->cell_start != overlay_starts[i] ||
            overlay->cell_count != overlay_counts[i] ||
            overlay->width != overlay_widths[i] || overlay->height != overlay_heights[i] ||
            overlay->x != overlay_x[i] || overlay->y != overlay_y[i] ||
            overlay->type != i ||
            overlay->cell_start + overlay->cell_count >
                TECMO_PRESEASON_OVERLAY_CELL_COUNT)
            return false;
    }
    for (size_t i = 0U; i < TECMO_PRESEASON_OVERLAY_CELL_COUNT; ++i)
        if (!parse_cell(&asset->overlay_cells[i],
                        bytes + PRESEASON_OVERLAY_CELLS_OFFSET + i * 6U,
                        0xFAU, 0xFAU))
            return false;
    for (size_t screen = 0U; screen < 4U; ++screen) {
        for (size_t i = 0U; i < 960U; ++i)
            if (!parse_cell(&asset->team_cells[screen * 960U + i],
                            bytes + PRESEASON_TEAM_CELLS_OFFSET +
                                (screen * 960U + i) * 6U,
                            team_r0[screen], team_r1[screen]))
                return false;
        for (size_t i = 0U; i < 16U; ++i) {
            uint8_t color = bytes[PRESEASON_TEAM_PALETTES_OFFSET + screen * 16U + i];
            if (color > 0x3FU) return false;
            asset->team_palettes[screen][i] = color;
        }
    }
    for (size_t player = 0U; player < 2U; ++player)
        for (size_t piece = 0U; piece < 5U; ++piece) {
            if (!parse_sprite(&asset->markers[player][piece],
                              bytes + PRESEASON_MARKERS_OFFSET +
                                  (player * 5U + piece) * 16U,
                              bytes[128U]))
                return false;
        }
    for (size_t player = 0U; player < 2U; ++player) {
        const TecmoStartGameMenuSprite *marker = asset->markers[player];
        if (marker[1].dx != marker[0].dx + 8 ||
            marker[2].dx != marker[1].dx + 8 ||
            marker[3].dx != marker[0].dx || marker[4].dx != marker[1].dx ||
            marker[1].dy != marker[0].dy || marker[2].dy != marker[0].dy ||
            marker[3].dy != marker[0].dy + 16 ||
            marker[4].dy != marker[3].dy)
            return false;
    }
    for (size_t piece = 0U; piece < 5U; ++piece)
        if (asset->markers[0][piece].dx != asset->markers[1][piece].dx ||
            asset->markers[0][piece].dy != asset->markers[1][piece].dy)
            return false;
    for (size_t piece = 2U; piece < 5U; ++piece)
        if (asset->markers[0][piece].top_chr_offset !=
            asset->markers[1][piece].top_chr_offset)
            return false;
    if (asset->markers[0][0].top_chr_offset == asset->markers[1][0].top_chr_offset ||
        asset->markers[0][1].top_chr_offset == asset->markers[1][1].top_chr_offset)
        return false;
    for (size_t i = 0U; i < 27U; ++i) {
        asset->team_x[i] = bytes[PRESEASON_COORDS_OFFSET + i * 2U];
        asset->team_y[i] = bytes[PRESEASON_COORDS_OFFSET + i * 2U + 1U];
        asset->team_ids[i] = bytes[PRESEASON_TEAM_IDS_OFFSET + i];
        if (asset->team_x[i] < 8U || asset->team_x[i] > 0xE8U ||
            asset->team_y[i] < 16U || asset->team_y[i] > 0xE8U ||
            asset->team_ids[i] >= 27U ||
            seen_ids[asset->team_ids[i]])
            return false;
        seen_ids[asset->team_ids[i]] = true;
    }
    memcpy(asset->division_offsets, bytes + PRESEASON_DIVISION_OFFSETS_OFFSET, 4U);
    memcpy(asset->division_counts, bytes + PRESEASON_DIVISION_COUNTS_OFFSET, 4U);
    memcpy(asset->ownership, bytes + PRESEASON_OWNERSHIP_OFFSET, 12U);
    memcpy(asset->configs, bytes + PRESEASON_CONFIGS_OFFSET, 7U);
    if (asset->division_offsets[0] != 0U || asset->configs[0] == 0U ||
        asset->configs[1] == 0U || asset->configs[2] == 0U ||
        asset->configs[0] == asset->configs[1] ||
        asset->configs[0] == asset->configs[2] ||
        asset->configs[1] == asset->configs[2])
        return false;
    for (size_t i = 3U; i < 7U; ++i)
        if ((i > 3U && asset->configs[i] != asset->configs[i - 1U] + 1U) ||
            asset->configs[i] == 0U)
            return false;
    for (size_t i = 0U; i < 4U; ++i) {
        uint8_t end = i + 1U < 4U ? asset->division_offsets[i + 1U] : 27U;
        if (asset->division_offsets[i] >= end || end > 27U ||
            asset->division_counts[i] != (uint8_t)(end - asset->division_offsets[i]))
            return false;
    }
    for (size_t i = 0U; i < 12U; ++i)
        if (((const uint8_t *)asset->ownership)[i] > 2U) return false;
    for (size_t selection = 0U; selection < 6U; ++selection) {
        bool player_two_owned = asset->ownership[0][selection] == 0U &&
                                asset->ownership[1][selection] == 0U;
        if (player_two_owned != (selection == 1U)) return false;
    }

    asset->expected_start_menu_size = read_u32(bytes + 72U);
    asset->expected_start_menu_fingerprint32 = read_u32(bytes + 76U);
    asset->expected_chr_size = read_u32(bytes + 80U);
    asset->expected_chr_fingerprint32 = read_u32(bytes + 84U);
    asset->row_cadence = bytes[97U];
    asset->cursor_commit_delay_frames = bytes[100U];
    asset->accepted_input_seed = bytes[101U];
    asset->repeat_frames = bytes[102U];
    asset->control_cursor_x = read_u16(bytes + 104U);
    asset->control_cursor_y = read_u16(bytes + 106U);
    asset->control_cursor_stride = read_u16(bytes + 108U);
    asset->division_cursor_x = read_u16(bytes + 110U);
    memcpy(asset->division_cursor_y, bytes + 112U, 4U);
    asset->team_input_ready_frames = read_u16(bytes + 116U);
    asset->team_exit_frames = read_u16(bytes + 118U);
    asset->team_palette_full_frames = bytes[132U];
    asset->difficulty_cursor_x = read_u16(bytes + 133U);
    asset->difficulty_cursor_y = read_u16(bytes + 135U);
    asset->difficulty_cursor_stride = read_u16(bytes + 137U);
    asset->team_first_visible_frame = bytes[139U];
    asset->team_palette_step_frames = bytes[140U];
    asset->team_visible_full_frame = bytes[141U];
    asset->division_return_first_visible_frame = bytes[142U];
    asset->division_return_palette_step_frames = bytes[143U];
    asset->division_return_full_frame = bytes[144U];
    asset->team_entry_stage7_frame = bytes[145U];
    asset->team_entry_stage6_frame = bytes[146U];
    asset->team_entry_stage5_frame = bytes[147U];
    asset->team_entry_black_frame = bytes[148U];
    asset->team_exit_cap2_frame = bytes[149U];
    asset->team_exit_cap1_frame = bytes[150U];
    asset->team_exit_cap0_frame = bytes[151U];
    asset->team_exit_black_frame = bytes[152U];
    return true;
}

bool tecmo_preseason_asset_load(TecmoPreseasonAsset *asset,
                                const char *project_root)
{
    uint8_t *preseason = NULL;
    uint8_t *start_menu = NULL;
    uint8_t *chr = NULL;
    uint64_t preseason_count = 0U;
    uint64_t start_menu_count = 0U;
    uint64_t chr_count = 0U;
    bool ok;

    if (asset == NULL) return false;
    memset(asset, 0, sizeof(*asset));
    if (!read_dependencies_from_selected_pack(project_root,
                                              &preseason, &preseason_count,
                                              &start_menu, &start_menu_count,
                                              &chr, &chr_count)) {
        (void)snprintf(asset->status, sizeof(asset->status),
                       "TPRE-1 menu/preseason entry or dependency unavailable");
        return false;
    }
    ok = parse_payload(asset, preseason, preseason_count) &&
         start_menu_count == asset->expected_start_menu_size &&
         fnv1a32(start_menu, start_menu_count) ==
             asset->expected_start_menu_fingerprint32 &&
         chr_count == asset->expected_chr_size &&
         fnv1a32(chr, chr_count) == asset->expected_chr_fingerprint32 &&
         fnv1a64(chr, chr_count) == PRESEASON_CHR_FNV1A64;
    if (ok) {
        asset->chr_byte_count = chr_count;
        asset->chr_fingerprint = fnv1a64(chr, chr_count);
        asset->available = true;
        ok = tecmo_preseason_asset_chr_available(asset, chr, chr_count);
    }
    if (!ok) asset->available = false;
    tecmo_asset_pack_free(preseason);
    tecmo_asset_pack_free(start_menu);
    tecmo_asset_pack_free(chr);
    (void)snprintf(asset->status, sizeof(asset->status), "%s",
                   ok ? "TPRE-1 native assetpack"
                      : "TPRE-1 asset contract rejected");
    return ok;
}

bool tecmo_preseason_asset_chr_available(const TecmoPreseasonAsset *asset,
                                         const uint8_t *chr_bytes,
                                         uint64_t chr_byte_count)
{
    if (asset == NULL || chr_bytes == NULL || !asset->available ||
        asset->expected_chr_size != PRESEASON_CHR_SIZE ||
        asset->expected_chr_fingerprint32 != PRESEASON_CHR_FNV1A32 ||
        chr_byte_count != asset->expected_chr_size ||
        asset->chr_byte_count != chr_byte_count ||
        asset->chr_fingerprint != PRESEASON_CHR_FNV1A64 ||
        fnv1a32(chr_bytes, chr_byte_count) != asset->expected_chr_fingerprint32 ||
        fnv1a64(chr_bytes, chr_byte_count) != asset->chr_fingerprint)
        return false;
    for (size_t i = 0U; i < TECMO_PRESEASON_OVERLAY_CELL_COUNT; ++i)
        if ((uint64_t)asset->overlay_cells[i].chr_offset + 16U > chr_byte_count)
            return false;
    for (size_t i = 0U; i < TECMO_PRESEASON_TEAM_CELL_COUNT; ++i)
        if ((uint64_t)asset->team_cells[i].chr_offset + 16U > chr_byte_count)
            return false;
    for (size_t player = 0U; player < 2U; ++player)
        for (size_t piece = 0U; piece < 5U; ++piece)
            if ((uint64_t)asset->markers[player][piece].bottom_chr_offset + 16U >
                chr_byte_count)
                return false;
    return true;
}

void tecmo_preseason_state_init(TecmoPreseasonState *state)
{
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->phase = TECMO_PRESEASON_CONTROL_SETUP;
}

static bool preseason_state_valid(const TecmoPreseasonAsset *asset,
                                  const TecmoPreseasonState *state)
{
    uint16_t transition_limit;
    uint8_t cooldown_limit;
    if (asset == NULL || state == NULL || !asset->available ||
        (int)state->phase < 0 || state->phase > TECMO_PRESEASON_TEAM_EXIT ||
        (int)state->team_exit_target < 0 ||
        state->team_exit_target > TECMO_PRESEASON_TEAM_EXIT_P2_DIVISION ||
        state->active_player > 1U || state->control_selection >= 7U ||
        state->difficulty_selection >= 3U || state->committed_difficulty >= 3U ||
        state->division_selection[0] >= 4U ||
        state->division_selection[1] >= 4U ||
        asset->division_counts[state->division_selection[0]] == 0U ||
        asset->division_counts[state->division_selection[1]] == 0U ||
        state->team_palette_frame > asset->team_palette_full_frames ||
        state->division_return_fade_frame >
            asset->division_return_full_frame ||
        state->cursor_delay > asset->cursor_commit_delay_frames)
        return false;
    if (state->phase >= TECMO_PRESEASON_TEAM_ENTRY &&
        (state->team_selection[state->active_player] >=
             asset->division_counts[state->division_selection[state->active_player]] ||
         (state->active_player == 1U &&
          state->division_selection[0] == state->division_selection[1] &&
          state->team_selection[0] >=
              asset->division_counts[state->division_selection[0]])))
        return false;
    cooldown_limit = asset->repeat_frames > asset->accepted_input_seed
        ? asset->repeat_frames : asset->accepted_input_seed;
    if (state->direction_cooldown > cooldown_limit) return false;
    switch (state->phase) {
    case TECMO_PRESEASON_CONTROL_SETUP:
    case TECMO_PRESEASON_CONTROL:
    case TECMO_PRESEASON_CONTROL_TEARDOWN_ROOT:
        transition_limit = asset->overlays[0].height;
        break;
    case TECMO_PRESEASON_DIFFICULTY_SETUP:
    case TECMO_PRESEASON_DIFFICULTY:
    case TECMO_PRESEASON_DIFFICULTY_TEARDOWN:
        transition_limit = asset->overlays[2].height;
        break;
    case TECMO_PRESEASON_DIVISION_SETUP:
    case TECMO_PRESEASON_DIVISION:
    case TECMO_PRESEASON_DIVISION_TEARDOWN_CONTROL:
        transition_limit = asset->overlays[1].height;
        break;
    case TECMO_PRESEASON_TEAM_ENTRY:
    case TECMO_PRESEASON_TEAM:
        transition_limit = asset->team_input_ready_frames;
        break;
    case TECMO_PRESEASON_TEAM_EXIT:
        transition_limit = asset->team_exit_frames;
        break;
    default:
        return false;
    }
    if (state->transition_frame > transition_limit) return false;
    if (state->division_return_fade_active &&
        state->phase != TECMO_PRESEASON_DIVISION_SETUP &&
        state->phase != TECMO_PRESEASON_DIVISION)
        return false;
    return true;
}

static uint8_t controller_byte(const TecmoInput *input)
{
    uint8_t value = 0U;
    if (input->right) value |= PRESEASON_BUTTON_RIGHT;
    if (input->left) value |= PRESEASON_BUTTON_LEFT;
    if (input->down) value |= PRESEASON_BUTTON_DOWN;
    if (input->up) value |= PRESEASON_BUTTON_UP;
    if (input->confirm) value |= PRESEASON_BUTTON_START;
    if (input->tab) value |= PRESEASON_BUTTON_SELECT;
    if (input->cancel) value |= PRESEASON_BUTTON_B;
    if (input->shoot) value |= PRESEASON_BUTTON_A;
    return value;
}

static PreseasonSelectionAction selection_action(const TecmoControlFrame *controls)
{
    uint8_t released;
    if (controller_byte(&controls->held) != 0U) return PRESEASON_SELECTION_NONE;
    released = controller_byte(&controls->released) &
               (PRESEASON_BUTTON_A | PRESEASON_BUTTON_B);
    if ((released & PRESEASON_BUTTON_A) != 0U) return PRESEASON_SELECTION_ACCEPT;
    if ((released & PRESEASON_BUTTON_B) != 0U) return PRESEASON_SELECTION_CANCEL;
    return PRESEASON_SELECTION_NONE;
}

static bool direction_action(const TecmoControlFrame *controls,
                             uint8_t direction_mask,
                             int *direction)
{
    uint8_t raw = controller_byte(&controls->held) & direction_mask;
    *direction = 0;
    if (raw == 0U) return false;
    if (raw == PRESEASON_BUTTON_UP || raw == PRESEASON_BUTTON_LEFT)
        *direction = -1;
    else if (raw == PRESEASON_BUTTON_DOWN || raw == PRESEASON_BUTTON_RIGHT)
        *direction = 1;
    return true;
}

static uint8_t wrap_selection(uint8_t selection, uint8_t count, int direction)
{
    if (direction < 0) return selection == 0U ? (uint8_t)(count - 1U)
                                               : (uint8_t)(selection - 1U);
    return selection + 1U >= count ? 0U : (uint8_t)(selection + 1U);
}

static void tick_cooldown(TecmoPreseasonState *state)
{
    if (state->direction_cooldown > 0U) --state->direction_cooldown;
}

static void begin_setup(TecmoPreseasonState *state, TecmoPreseasonPhase phase)
{
    state->phase = phase;
    state->transition_frame = 0U;
    state->cursor_delay = 0U;
}

static void begin_team_entry(TecmoPreseasonState *state, uint8_t player)
{
    state->active_player = player;
    state->phase = TECMO_PRESEASON_TEAM_ENTRY;
    state->transition_frame = 0U;
    state->team_palette_frame = 0U;
    state->division_return_fade_active = false;
    state->division_return_fade_frame = 0U;
    state->cursor_delay = 0U;
}

static void begin_team_exit(TecmoPreseasonState *state,
                            TecmoPreseasonTeamExitTarget target)
{
    state->phase = TECMO_PRESEASON_TEAM_EXIT;
    state->team_exit_target = target;
    state->transition_frame = 0U;
    state->cursor_delay = 0U;
}

static bool active_selection_uses_player_two(const TecmoPreseasonAsset *asset,
                                             const TecmoPreseasonState *state)
{
    size_t ownership_index;
    if (state->active_player == 0U || state->control_selection == 0U ||
        state->control_selection > 6U)
        return false;
    ownership_index = (size_t)state->control_selection - 1U;
    return asset->ownership[0][ownership_index] == 0U &&
           asset->ownership[1][ownership_index] == 0U;
}

static uint8_t next_team_selection(const TecmoPreseasonAsset *asset,
                                   const TecmoPreseasonState *state,
                                   uint8_t player,
                                   int direction)
{
    uint8_t division = state->division_selection[player];
    uint8_t count = asset->division_counts[division];
    uint8_t selection = state->team_selection[player];
    for (uint8_t attempts = 0U; attempts < count; ++attempts) {
        selection = wrap_selection(selection, count, direction);
        if (player == 0U || division != state->division_selection[0] ||
            selection != state->team_selection[0])
            break;
    }
    return selection;
}

static uint8_t first_available_p2_team(const TecmoPreseasonAsset *asset,
                                       const TecmoPreseasonState *state)
{
    uint8_t division = state->division_selection[1];
    uint8_t count = asset->division_counts[division];
    uint8_t selection = 0U;
    if (division == state->division_selection[0] &&
        selection == state->team_selection[0] && count > 1U)
        selection = 1U;
    return selection;
}

TecmoPreseasonAction tecmo_preseason_update(TecmoPreseasonState *state,
                                            const TecmoPreseasonAsset *asset,
                                            const TecmoControlFrame *player_one,
                                            const TecmoControlFrame *player_two)
{
    const TecmoControlFrame *controls;
    PreseasonSelectionAction action;
    int direction;
    uint16_t phase_frames;

    if (player_one == NULL || player_two == NULL ||
        !preseason_state_valid(asset, state))
        return TECMO_PRESEASON_ACTION_NONE;
    ++state->frame;
    if (state->cursor_delay > 0U) --state->cursor_delay;
    controls = active_selection_uses_player_two(asset, state)
        ? player_two : player_one;

    if (state->phase == TECMO_PRESEASON_CONTROL_SETUP ||
        state->phase == TECMO_PRESEASON_DIFFICULTY_SETUP ||
        state->phase == TECMO_PRESEASON_DIVISION_SETUP) {
        size_t overlay = state->phase == TECMO_PRESEASON_CONTROL_SETUP ? 0U :
                         state->phase == TECMO_PRESEASON_DIFFICULTY_SETUP ? 2U : 1U;
        phase_frames = (uint16_t)(asset->overlays[overlay].height * asset->row_cadence);
        if (state->transition_frame < phase_frames) ++state->transition_frame;
        if (state->transition_frame >= phase_frames) {
            state->phase = state->phase == TECMO_PRESEASON_CONTROL_SETUP
                ? TECMO_PRESEASON_CONTROL
                : state->phase == TECMO_PRESEASON_DIFFICULTY_SETUP
                    ? TECMO_PRESEASON_DIFFICULTY : TECMO_PRESEASON_DIVISION;
            if (state->phase == TECMO_PRESEASON_DIVISION &&
                state->division_return_fade_active)
                state->division_return_fade_frame = 0U;
            state->direction_cooldown = asset->accepted_input_seed;
            state->cursor_delay = asset->cursor_commit_delay_frames;
        }
        return TECMO_PRESEASON_ACTION_NONE;
    }
    if (state->phase == TECMO_PRESEASON_DIFFICULTY_TEARDOWN ||
        state->phase == TECMO_PRESEASON_CONTROL_TEARDOWN_ROOT ||
        state->phase == TECMO_PRESEASON_DIVISION_TEARDOWN_CONTROL) {
        size_t overlay = state->phase == TECMO_PRESEASON_DIFFICULTY_TEARDOWN ? 2U :
                         state->phase == TECMO_PRESEASON_CONTROL_TEARDOWN_ROOT ? 0U : 1U;
        phase_frames = (uint16_t)(asset->overlays[overlay].height * asset->row_cadence);
        if (state->transition_frame < phase_frames) ++state->transition_frame;
        if (state->transition_frame >= phase_frames) {
            if (state->phase == TECMO_PRESEASON_CONTROL_TEARDOWN_ROOT)
                return TECMO_PRESEASON_ACTION_BACK_TO_START_MENU;
            state->phase = TECMO_PRESEASON_CONTROL;
            state->active_player = 0U;
            state->division_selection[0] = 0U;
            state->division_selection[1] = 0U;
            state->direction_cooldown = asset->accepted_input_seed;
            state->cursor_delay = asset->cursor_commit_delay_frames;
        }
        return TECMO_PRESEASON_ACTION_NONE;
    }
    if (state->phase == TECMO_PRESEASON_TEAM_ENTRY) {
        if (state->transition_frame < asset->team_input_ready_frames)
            ++state->transition_frame;
        if (state->transition_frame >= asset->team_input_ready_frames) {
            state->phase = TECMO_PRESEASON_TEAM;
            state->team_palette_frame = (uint8_t)asset->team_input_ready_frames;
            state->direction_cooldown = asset->accepted_input_seed;
            state->cursor_delay = asset->cursor_commit_delay_frames;
        }
        return TECMO_PRESEASON_ACTION_NONE;
    }
    if (state->phase == TECMO_PRESEASON_TEAM_EXIT) {
        if (state->transition_frame < asset->team_exit_frames)
            ++state->transition_frame;
        if (state->transition_frame >= asset->team_exit_frames) {
            if (state->team_exit_target == TECMO_PRESEASON_TEAM_EXIT_P2_DIVISION) {
                state->active_player = 1U;
                state->division_selection[1] = state->division_selection[0];
                state->team_selection[1] = first_available_p2_team(asset, state);
            }
            state->division_return_fade_active = true;
            state->division_return_fade_frame = 0U;
            begin_setup(state, TECMO_PRESEASON_DIVISION_SETUP);
        }
        return TECMO_PRESEASON_ACTION_NONE;
    }

    if (state->phase == TECMO_PRESEASON_CONTROL) {
        action = selection_action(player_one);
        if (action == PRESEASON_SELECTION_CANCEL) {
            state->direction_cooldown = asset->accepted_input_seed;
            begin_setup(state, TECMO_PRESEASON_CONTROL_TEARDOWN_ROOT);
            return TECMO_PRESEASON_ACTION_NONE;
        }
        if (action == PRESEASON_SELECTION_ACCEPT) {
            state->direction_cooldown = asset->accepted_input_seed;
            if (state->control_selection == 0U) {
                state->difficulty_selection = state->committed_difficulty;
                begin_setup(state, TECMO_PRESEASON_DIFFICULTY_SETUP);
            } else {
                state->active_player = 0U;
                state->division_return_fade_active = false;
                begin_setup(state, TECMO_PRESEASON_DIVISION_SETUP);
            }
            return TECMO_PRESEASON_ACTION_NONE;
        }
        if (direction_action(player_one,
                             PRESEASON_BUTTON_UP | PRESEASON_BUTTON_DOWN,
                             &direction) && state->direction_cooldown == 0U) {
            if (direction != 0)
                state->control_selection = wrap_selection(state->control_selection, 7U,
                                                          direction);
            state->direction_cooldown = asset->repeat_frames;
        }
        tick_cooldown(state);
        return TECMO_PRESEASON_ACTION_NONE;
    }

    if (state->phase == TECMO_PRESEASON_DIFFICULTY) {
        action = selection_action(player_one);
        if (action != PRESEASON_SELECTION_NONE) {
            if (action == PRESEASON_SELECTION_ACCEPT) {
                state->committed_difficulty = state->difficulty_selection;
                state->control_selection = 1U;
            } else {
                state->difficulty_selection = state->committed_difficulty;
            }
            state->direction_cooldown = asset->accepted_input_seed;
            begin_setup(state, TECMO_PRESEASON_DIFFICULTY_TEARDOWN);
            return TECMO_PRESEASON_ACTION_NONE;
        }
        if (direction_action(player_one,
                             PRESEASON_BUTTON_UP | PRESEASON_BUTTON_DOWN,
                             &direction) && state->direction_cooldown == 0U) {
            if (direction != 0)
                state->difficulty_selection = wrap_selection(
                    state->difficulty_selection, 3U, direction);
            state->direction_cooldown = asset->repeat_frames;
        }
        tick_cooldown(state);
        return TECMO_PRESEASON_ACTION_NONE;
    }

    if (state->phase == TECMO_PRESEASON_DIVISION) {
        if (state->division_return_fade_active &&
            state->division_return_fade_frame <
                asset->division_return_full_frame)
            ++state->division_return_fade_frame;
        if (state->division_return_fade_frame >=
            asset->division_return_full_frame)
            state->division_return_fade_active = false;
        action = selection_action(controls);
        if (action == PRESEASON_SELECTION_CANCEL) {
            state->direction_cooldown = asset->accepted_input_seed;
            state->active_player = 0U;
            state->division_selection[0] = 0U;
            state->division_selection[1] = 0U;
            state->division_return_fade_active = false;
            state->division_return_fade_frame = 0U;
            begin_setup(state, TECMO_PRESEASON_DIVISION_TEARDOWN_CONTROL);
            return TECMO_PRESEASON_ACTION_NONE;
        }
        if (action == PRESEASON_SELECTION_ACCEPT) {
            state->direction_cooldown = asset->accepted_input_seed;
            state->team_selection[state->active_player] = 0U;
            if (state->active_player == 1U)
                state->team_selection[1] = first_available_p2_team(asset, state);
            begin_team_entry(state, state->active_player);
            return TECMO_PRESEASON_ACTION_NONE;
        }
        if (direction_action(controls,
                             PRESEASON_BUTTON_UP | PRESEASON_BUTTON_DOWN,
                             &direction) && state->direction_cooldown == 0U) {
            if (direction != 0)
                state->division_selection[state->active_player] = wrap_selection(
                    state->division_selection[state->active_player], 4U, direction);
            state->direction_cooldown = asset->repeat_frames;
        }
        tick_cooldown(state);
        return TECMO_PRESEASON_ACTION_NONE;
    }

    if (state->phase == TECMO_PRESEASON_TEAM) {
        if (state->team_palette_frame < asset->team_palette_full_frames)
            ++state->team_palette_frame;
        action = selection_action(controls);
        if (action == PRESEASON_SELECTION_CANCEL) {
            state->direction_cooldown = asset->accepted_input_seed;
            begin_team_exit(state, TECMO_PRESEASON_TEAM_EXIT_DIVISION);
            return TECMO_PRESEASON_ACTION_NONE;
        }
        if (action == PRESEASON_SELECTION_ACCEPT) {
            if (state->active_player == 0U) {
                state->direction_cooldown = asset->accepted_input_seed;
                begin_team_exit(state, TECMO_PRESEASON_TEAM_EXIT_P2_DIVISION);
            } else {
                state->direction_cooldown = asset->accepted_input_seed;
                return TECMO_PRESEASON_ACTION_LAUNCH_GAME;
            }
            return TECMO_PRESEASON_ACTION_NONE;
        }
        if (direction_action(controls,
                             PRESEASON_BUTTON_LEFT | PRESEASON_BUTTON_RIGHT,
                             &direction) && state->direction_cooldown == 0U) {
            if (direction != 0)
                state->team_selection[state->active_player] = next_team_selection(
                    asset, state, state->active_player, direction);
            state->direction_cooldown = asset->repeat_frames;
        }
        tick_cooldown(state);
    }
    return TECMO_PRESEASON_ACTION_NONE;
}

unsigned tecmo_preseason_overlay_visible_rows(const TecmoPreseasonAsset *asset,
                                              const TecmoPreseasonState *state,
                                              size_t overlay_index)
{
    unsigned height;
    unsigned elapsed;
    if (asset == NULL || state == NULL || !asset->available ||
        overlay_index >= TECMO_PRESEASON_OVERLAY_COUNT)
        return 0U;
    height = asset->overlays[overlay_index].height;
    elapsed = asset->row_cadence == 0U ? 0U :
              state->transition_frame / asset->row_cadence;
    if (elapsed > height) elapsed = height;
    if (overlay_index == 0U) {
        if (state->phase == TECMO_PRESEASON_CONTROL_SETUP)
            return elapsed < height ? elapsed + 1U : height;
        if (state->phase == TECMO_PRESEASON_CONTROL_TEARDOWN_ROOT)
            return height - elapsed;
        if (state->phase == TECMO_PRESEASON_TEAM_ENTRY &&
            state->transition_frame >= asset->team_entry_black_frame)
            return 0U;
        if (state->phase == TECMO_PRESEASON_TEAM ||
            state->phase == TECMO_PRESEASON_TEAM_EXIT)
            return 0U;
        return height;
    }
    if (overlay_index == 1U) {
        if (state->phase == TECMO_PRESEASON_DIVISION_SETUP)
            return elapsed < height ? elapsed + 1U : height;
        if (state->phase == TECMO_PRESEASON_DIVISION_TEARDOWN_CONTROL)
            return height - elapsed;
        if (state->phase == TECMO_PRESEASON_DIVISION ||
            (state->phase == TECMO_PRESEASON_TEAM_ENTRY &&
             state->transition_frame < asset->team_entry_black_frame))
            return height;
        return 0U;
    }
    if (state->phase == TECMO_PRESEASON_DIFFICULTY_SETUP)
        return elapsed < height ? elapsed + 1U : height;
    if (state->phase == TECMO_PRESEASON_DIFFICULTY_TEARDOWN)
        return height - elapsed;
    return state->phase == TECMO_PRESEASON_DIFFICULTY ? height : 0U;
}

const char *tecmo_preseason_phase_name(TecmoPreseasonPhase phase)
{
    switch (phase) {
    case TECMO_PRESEASON_CONTROL_SETUP: return "CONTROL SETUP";
    case TECMO_PRESEASON_CONTROL: return "CONTROL";
    case TECMO_PRESEASON_DIFFICULTY_SETUP: return "DIFFICULTY SETUP";
    case TECMO_PRESEASON_DIFFICULTY: return "DIFFICULTY";
    case TECMO_PRESEASON_DIFFICULTY_TEARDOWN: return "DIFFICULTY TEARDOWN";
    case TECMO_PRESEASON_CONTROL_TEARDOWN_ROOT: return "CONTROL TEARDOWN";
    case TECMO_PRESEASON_DIVISION_SETUP: return "DIVISION SETUP";
    case TECMO_PRESEASON_DIVISION: return "DIVISION";
    case TECMO_PRESEASON_DIVISION_TEARDOWN_CONTROL: return "DIVISION TEARDOWN";
    case TECMO_PRESEASON_TEAM_ENTRY: return "TEAM ENTRY";
    case TECMO_PRESEASON_TEAM: return "TEAM";
    case TECMO_PRESEASON_TEAM_EXIT: return "TEAM EXIT";
    default: return "UNKNOWN";
    }
}

static bool make_viewport(TecmoFramebuffer *view,
                          TecmoFramebuffer *framebuffer,
                          int origin_x,
                          int origin_y,
                          int scale)
{
    int scaled_height;
    int scaled_width;
    size_t origin_offset;
    size_t pitch;
    size_t total_last_offset;
    if (view == NULL || framebuffer == NULL || framebuffer->pixels == NULL ||
        framebuffer->width <= 0 || framebuffer->height <= 0 ||
        framebuffer->pitch_pixels <= 0 ||
        framebuffer->pitch_pixels < framebuffer->width ||
        scale <= 0 || scale > INT_MAX / 256 || scale > INT_MAX / 240 ||
        origin_x < 0 || origin_y < 0)
        return false;
    scaled_width = 256 * scale;
    scaled_height = 240 * scale;
    if (scaled_width > framebuffer->width || scaled_height > framebuffer->height ||
        origin_x > framebuffer->width - scaled_width ||
        origin_y > framebuffer->height - scaled_height)
        return false;
    pitch = (size_t)framebuffer->pitch_pixels;
    if ((size_t)(framebuffer->height - 1) >
            (SIZE_MAX - (size_t)(framebuffer->width - 1)) / pitch)
        return false;
    total_last_offset = (size_t)(framebuffer->height - 1) * pitch +
                        (size_t)(framebuffer->width - 1);
    if (total_last_offset > SIZE_MAX / sizeof(*framebuffer->pixels) ||
        (size_t)origin_y > (SIZE_MAX - (size_t)origin_x) / pitch)
        return false;
    origin_offset = (size_t)origin_y * pitch + (size_t)origin_x;
    if (origin_offset > SIZE_MAX / sizeof(*framebuffer->pixels)) return false;
    view->pixels = framebuffer->pixels + origin_offset;
    view->width = scaled_width;
    view->height = scaled_height;
    view->pitch_pixels = framebuffer->pitch_pixels;
    return true;
}

static void fill_viewport(TecmoFramebuffer *view, uint32_t color)
{
    for (int y = 0; y < view->height; ++y) {
        uint32_t *row = view->pixels + (size_t)y * (size_t)view->pitch_pixels;
        for (int x = 0; x < view->width; ++x) row[x] = color;
    }
}

static void fill_viewport_rect(TecmoFramebuffer *view,
                               int x,
                               int y,
                               int width,
                               int height,
                               uint32_t color)
{
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + width > view->width ? view->width : x + width;
    int y1 = y + height > view->height ? view->height : y + height;
    for (int py = y0; py < y1; ++py) {
        uint32_t *row = view->pixels + (size_t)py * (size_t)view->pitch_pixels;
        for (int px = x0; px < x1; ++px) row[px] = color;
    }
}

static void draw_cell(TecmoFramebuffer *view,
                      const TecmoStartGameMenuCell *cell,
                      const uint8_t palette[16],
                      const uint8_t *chr_bytes,
                      uint64_t chr_byte_count,
                      int x,
                      int y,
                      int scale)
{
    uint32_t rgba[4];
    size_t base = (size_t)cell->palette_index * 4U;
    rgba[0] = tecmo_nes_2c02_rgba(palette[0]);
    for (size_t i = 1U; i < 4U; ++i)
        rgba[i] = tecmo_nes_2c02_rgba(palette[base + i]);
    tecmo_draw_chr_tile_at_offset_ex(view, chr_bytes, chr_byte_count,
                                     cell->chr_offset, x, y, scale, rgba,
                                     false, false);
}

static void draw_sprite_piece(TecmoFramebuffer *view,
                              const TecmoStartGameMenuSprite *sprite,
                              const uint8_t palette[16],
                              const uint8_t *chr_bytes,
                              uint64_t chr_byte_count,
                              int x,
                              int y,
                              int scale)
{
    uint32_t rgba[4] = {0U, 0U, 0U, 0U};
    size_t base = (size_t)sprite->palette_index * 4U;
    for (size_t i = 1U; i < 4U; ++i)
        rgba[i] = tecmo_nes_2c02_rgba(palette[base + i]);
    tecmo_draw_chr_tile_at_offset_ex(view, chr_bytes, chr_byte_count,
                                     sprite->top_chr_offset,
                                     x * scale, (y + 1) * scale, scale, rgba,
                                     (sprite->flags & 2U) != 0U,
                                     (sprite->flags & 4U) != 0U);
    tecmo_draw_chr_tile_at_offset_ex(view, chr_bytes, chr_byte_count,
                                     sprite->bottom_chr_offset,
                                     x * scale, (y + 9) * scale, scale, rgba,
                                     (sprite->flags & 2U) != 0U,
                                     (sprite->flags & 4U) != 0U);
}

static void draw_overlay(TecmoFramebuffer *view,
                         const TecmoPreseasonAsset *asset,
                         size_t overlay_index,
                         unsigned visible_rows,
                         const uint8_t palette[16],
                         const uint8_t *chr_bytes,
                         uint64_t chr_byte_count,
                         int scale)
{
    const TecmoStartGameMenuOverlay *overlay = &asset->overlays[overlay_index];
    size_t visible_cells;
    if (visible_rows > overlay->height) visible_rows = overlay->height;
    visible_cells = (size_t)visible_rows * overlay->width;
    fill_viewport_rect(view,
                       (int)overlay->x * 8 * scale,
                       (int)overlay->y * 8 * scale,
                       (int)overlay->width * 8 * scale,
                       (int)visible_rows * 8 * scale,
                       tecmo_nes_2c02_rgba(palette[0]));
    for (size_t i = 0U; i < visible_cells; ++i) {
        unsigned col = (unsigned)(i % overlay->width);
        unsigned row = (unsigned)(i / overlay->width);
        draw_cell(view, &asset->overlay_cells[overlay->cell_start + i],
                  palette, chr_bytes, chr_byte_count,
                  ((int)overlay->x + (int)col) * 8 * scale,
                  ((int)overlay->y + (int)row) * 8 * scale, scale);
    }
}

static uint8_t brightness_cap(uint8_t color, uint8_t cap)
{
    uint8_t brightness;
    if (color == 0x0FU) return color;
    brightness = (uint8_t)(color & 0x30U);
    if (brightness > (uint8_t)(cap << 4U))
        color = (uint8_t)((color & 0x0FU) | (cap << 4U));
    return color;
}

static bool draw_start_base(TecmoFramebuffer *framebuffer,
                            const TecmoStartGameMenuAsset *start_asset,
                            const uint8_t *chr_bytes,
                            uint64_t chr_byte_count,
                            int origin_x,
                            int origin_y,
                            int scale,
                            unsigned palette_stage)
{
    TecmoStartGameMenuState base_state;
    memset(&base_state, 0, sizeof(base_state));
    base_state.frame = start_asset->stable_frame;
    base_state.cursor_delay = 1U;
    if (palette_stage >= 8U) {
        base_state.phase = TECMO_START_GAME_MENU_ROOT;
    } else {
        unsigned fade_step = 8U - palette_stage;
        base_state.phase = TECMO_START_GAME_MENU_EXIT;
        base_state.transition_frame = (uint16_t)(fade_step *
            start_asset->exit_palette_step_frames);
    }
    return tecmo_start_game_menu_draw(framebuffer, start_asset, &base_state,
                                      NULL, chr_bytes, chr_byte_count,
                                      origin_x, origin_y, scale);
}

static void draw_cursor(TecmoFramebuffer *view,
                        const TecmoStartGameMenuAsset *start_asset,
                        const uint8_t *chr_bytes,
                        uint64_t chr_byte_count,
                        const uint8_t palette[16],
                        int x,
                        int y,
                        int scale)
{
    draw_sprite_piece(view, &start_asset->cursor, palette, chr_bytes,
                      chr_byte_count, x, y, scale);
}

static void make_capped_palette(uint8_t dest[16],
                                const uint8_t source[16],
                                int cap)
{
    if (cap < 0) {
        memset(dest, 0x0F, 16U);
        return;
    }
    for (size_t i = 0U; i < 16U; ++i)
        dest[i] = brightness_cap(source[i], (uint8_t)cap);
}

static void draw_team_marker(TecmoFramebuffer *view,
                             const TecmoPreseasonAsset *asset,
                             unsigned player,
                             unsigned global_team,
                             const uint8_t sprite_palette[16],
                             const uint8_t *chr_bytes,
                             uint64_t chr_byte_count,
                             int scale)
{
    int anchor_x = asset->team_x[global_team];
    int anchor_y = asset->team_y[global_team];
    for (size_t piece = 0U; piece < TECMO_PRESEASON_MARKER_PIECES; ++piece) {
        const TecmoStartGameMenuSprite *sprite = &asset->markers[player][piece];
        draw_sprite_piece(view, sprite, sprite_palette, chr_bytes, chr_byte_count,
                          anchor_x + sprite->dx, anchor_y + sprite->dy, scale);
    }
}

static void draw_team_screen(TecmoFramebuffer *view,
                             const TecmoPreseasonAsset *asset,
                             const TecmoPreseasonState *state,
                             const TecmoStartGameMenuAsset *start_asset,
                             const uint8_t *chr_bytes,
                             uint64_t chr_byte_count,
                             int scale)
{
    uint8_t bg_palette[16];
    uint8_t sprite_palette[16];
    uint8_t division = state->division_selection[state->active_player];
    int cap = 3;
    if (state->phase == TECMO_PRESEASON_TEAM) {
        if (state->team_palette_frame < asset->team_first_visible_frame) cap = -1;
        else if (state->team_palette_frame <
                 asset->team_first_visible_frame +
                     asset->team_palette_step_frames) cap = 0;
        else if (state->team_palette_frame <
                 asset->team_first_visible_frame +
                     2U * asset->team_palette_step_frames) cap = 1;
        else if (state->team_palette_frame < asset->team_visible_full_frame) cap = 2;
    } else if (state->phase == TECMO_PRESEASON_TEAM_EXIT) {
        if (state->transition_frame >= asset->team_exit_black_frame) cap = -1;
        else if (state->transition_frame >= asset->team_exit_cap0_frame) cap = 0;
        else if (state->transition_frame >= asset->team_exit_cap1_frame) cap = 1;
        else if (state->transition_frame >= asset->team_exit_cap2_frame) cap = 2;
    }
    make_capped_palette(bg_palette, asset->team_palettes[division], cap);
    make_capped_palette(sprite_palette, start_asset->palettes[8] + 16U, cap);
    fill_viewport(view, tecmo_nes_2c02_rgba(bg_palette[0]));
    for (size_t i = 0U; i < 960U; ++i)
        draw_cell(view, &asset->team_cells[(size_t)division * 960U + i],
                  bg_palette, chr_bytes, chr_byte_count,
                  (int)(i % 32U) * 8 * scale,
                  (int)(i / 32U) * 8 * scale, scale);
    if (state->active_player == 1U) {
        unsigned p2_global = asset->division_offsets[division] +
                             state->team_selection[1];
        draw_team_marker(view, asset, 1U, p2_global, sprite_palette,
                         chr_bytes, chr_byte_count, scale);
        if (state->division_selection[0] == division) {
            unsigned p1_global = asset->division_offsets[division] +
                                 state->team_selection[0];
            draw_team_marker(view, asset, 0U, p1_global, sprite_palette,
                             chr_bytes, chr_byte_count, scale);
        }
    } else {
        unsigned p1_global = asset->division_offsets[division] +
                             state->team_selection[0];
        draw_team_marker(view, asset, 0U, p1_global, sprite_palette,
                         chr_bytes, chr_byte_count, scale);
    }
}

bool tecmo_preseason_draw(TecmoFramebuffer *framebuffer,
                          const TecmoPreseasonAsset *asset,
                          const TecmoPreseasonState *state,
                          const TecmoStartGameMenuAsset *start_asset,
                          const uint8_t *chr_bytes,
                          uint64_t chr_byte_count,
                          int origin_x,
                          int origin_y,
                          int scale)
{
    TecmoFramebuffer view;
    unsigned stage = 8U;
    unsigned control_rows;
    unsigned division_rows;
    unsigned difficulty_rows;
    bool team_visible;

    if (start_asset == NULL || !start_asset->available ||
        !preseason_state_valid(asset, state) ||
        !tecmo_preseason_asset_chr_available(asset, chr_bytes, chr_byte_count) ||
        !tecmo_start_game_menu_asset_chr_available(start_asset, chr_bytes,
                                                   chr_byte_count) ||
        !make_viewport(&view, framebuffer, origin_x, origin_y, scale))
        return false;

    team_visible = state->phase == TECMO_PRESEASON_TEAM ||
                   state->phase == TECMO_PRESEASON_TEAM_EXIT;
    if (team_visible) {
        draw_team_screen(&view, asset, state, start_asset, chr_bytes,
                         chr_byte_count, scale);
        return true;
    }
    if (state->phase == TECMO_PRESEASON_TEAM_ENTRY &&
        state->transition_frame >= asset->team_entry_black_frame) {
        fill_viewport(&view, tecmo_nes_2c02_rgba(0x0FU));
        return true;
    }
    if (state->division_return_fade_active &&
        state->phase == TECMO_PRESEASON_DIVISION_SETUP) {
        fill_viewport(&view, tecmo_nes_2c02_rgba(0x0FU));
        return true;
    }
    if (state->division_return_fade_active &&
        state->phase == TECMO_PRESEASON_DIVISION) {
        if (state->division_return_fade_frame <
            asset->division_return_first_visible_frame) {
            fill_viewport(&view, tecmo_nes_2c02_rgba(0x0FU));
            return true;
        }
        if (state->division_return_fade_frame <
            asset->division_return_first_visible_frame +
                asset->division_return_palette_step_frames) stage = 5U;
        else if (state->division_return_fade_frame <
                 asset->division_return_first_visible_frame +
                     2U * asset->division_return_palette_step_frames) stage = 6U;
        else if (state->division_return_fade_frame <
                 asset->division_return_full_frame) stage = 7U;
    }
    if (state->phase == TECMO_PRESEASON_TEAM_ENTRY) {
        if (state->transition_frame >= asset->team_entry_stage5_frame) stage = 5U;
        else if (state->transition_frame >= asset->team_entry_stage6_frame) stage = 6U;
        else if (state->transition_frame >= asset->team_entry_stage7_frame) stage = 7U;
    }
    if (!draw_start_base(framebuffer, start_asset, chr_bytes, chr_byte_count,
                         origin_x, origin_y, scale, stage))
        return false;

    control_rows = tecmo_preseason_overlay_visible_rows(asset, state, 0U);
    division_rows = tecmo_preseason_overlay_visible_rows(asset, state, 1U);
    difficulty_rows = tecmo_preseason_overlay_visible_rows(asset, state, 2U);
    if (control_rows > 0U)
        draw_overlay(&view, asset, 0U, control_rows,
                     start_asset->palettes[stage], chr_bytes, chr_byte_count,
                     scale);
    if (division_rows > 0U) {
        draw_overlay(&view, asset, 1U, division_rows,
                     start_asset->palettes[stage], chr_bytes,
                     chr_byte_count, scale);
    }
    if (difficulty_rows > 0U)
        draw_overlay(&view, asset, 2U, difficulty_rows,
                     start_asset->palettes[stage], chr_bytes, chr_byte_count,
                     scale);

    if (state->cursor_delay == 0U) {
        if (state->phase == TECMO_PRESEASON_CONTROL) {
            draw_cursor(&view, start_asset, chr_bytes, chr_byte_count,
                        start_asset->palettes[stage] + 16U,
                        asset->control_cursor_x,
                        asset->control_cursor_y +
                            state->control_selection * asset->control_cursor_stride,
                        scale);
        } else if (state->phase == TECMO_PRESEASON_DIFFICULTY) {
            draw_cursor(&view, start_asset, chr_bytes, chr_byte_count,
                        start_asset->palettes[stage] + 16U,
                        asset->difficulty_cursor_x,
                        asset->difficulty_cursor_y +
                            state->difficulty_selection *
                                asset->difficulty_cursor_stride,
                        scale);
        } else if (state->phase == TECMO_PRESEASON_DIVISION) {
            draw_cursor(&view, start_asset, chr_bytes, chr_byte_count,
                        start_asset->palettes[stage] + 16U,
                        asset->division_cursor_x,
                        asset->division_cursor_y[
                            state->division_selection[state->active_player]],
                        scale);
        }
    }
    return true;
}
