#include "tecmo_start_game_menu.h"

#include "tecmo_asset_pack.h"
#include "tecmo_nes_video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define START_MENU_ENTRY_ID "menu/start-game"
#define START_MENU_HEADER_SIZE 160U
#define START_MENU_CELL_STRIDE 6U
#define START_MENU_CELLS_OFFSET 160U
#define START_MENU_PALETTES_OFFSET 11680U
#define START_MENU_EMBLEM_OFFSET 11968U
#define START_MENU_CURSOR_OFFSET 12752U
#define START_MENU_OVERLAY_DESCS_OFFSET 12768U
#define START_MENU_OVERLAY_CELLS_OFFSET 12816U
#define START_MENU_DIGITS_OFFSET 14052U
#define START_MENU_PAYLOAD_SIZE 14112U
#define START_MENU_SPRITE_STRIDE 16U
#define START_MENU_OVERLAY_DESC_STRIDE 16U
#define START_MENU_SEASON_CURSOR_X 103

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

static uint64_t fnv1a64(const uint8_t *bytes, uint64_t count)
{
    uint64_t hash = 14695981039346656037ULL;
    uint64_t i;
    for (i = 0U; i < count; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static uint32_t fnv1a32(const uint8_t *bytes, uint64_t count)
{
    uint32_t hash = 2166136261U;
    uint64_t i;
    for (i = 0U; i < count; ++i) {
        hash ^= bytes[i];
        hash *= 16777619U;
    }
    return hash;
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

static bool read_entry_from_selected_pack(const char *root,
                                          uint8_t **menu_bytes,
                                          uint64_t *menu_count,
                                          uint8_t **chr_bytes,
                                          uint64_t *chr_count)
{
    const char *env = getenv("TECMO_ASSETPACK");
    const char *paths[4];
    char root_build[1024];
    char root_pack[1024];
    size_t path_count = 0U;
    size_t i;

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

    for (i = 0U; i < path_count; ++i) {
        if (!file_exists(paths[i])) continue;
        if (tecmo_asset_pack_read_entry(paths[i], START_MENU_ENTRY_ID,
                                        menu_bytes, menu_count) != 0)
            return false;
        if (tecmo_asset_pack_read_entry(paths[i], "chr/all", chr_bytes, chr_count) != 0) {
            tecmo_asset_pack_free(*menu_bytes);
            *menu_bytes = NULL;
            *menu_count = 0U;
            return false;
        }
        return true;
    }
    return false;
}

static bool parse_cell(TecmoStartGameMenuCell *cell, const uint8_t *bytes)
{
    cell->tile_id = bytes[0];
    cell->palette_index = bytes[1];
    cell->chr_offset = read_u32(bytes + 2U);
    return cell->palette_index <= 3U && (cell->chr_offset & 15U) == 0U;
}

static bool parse_sprite(TecmoStartGameMenuSprite *sprite,
                         const uint8_t *bytes,
                         bool cursor)
{
    sprite->dx = read_i16(bytes);
    sprite->dy = read_i16(bytes + 2U);
    sprite->top_chr_offset = read_u32(bytes + 4U);
    sprite->bottom_chr_offset = read_u32(bytes + 8U);
    sprite->palette_index = bytes[12U];
    sprite->flags = bytes[13U];
    if (bytes[14U] != 0U || bytes[15U] != 0U ||
        sprite->palette_index > 3U || (sprite->flags & ~7U) != 0U ||
        (sprite->top_chr_offset & 15U) != 0U ||
        sprite->bottom_chr_offset != sprite->top_chr_offset + 16U ||
        sprite->dx < -256 || sprite->dx > 256 ||
        sprite->dy < -256 || sprite->dy > 256)
        return false;
    if (cursor && (sprite->dx != 31 || sprite->dy != 88 ||
                   sprite->palette_index != 0U || sprite->flags != 0U))
        return false;
    return true;
}

static bool parse_payload(TecmoStartGameMenuAsset *asset,
                          const uint8_t *bytes,
                          uint64_t count)
{
    static const uint16_t expected_stage_frames[9] = {
        0U, 2U, 4U, 6U, 8U, 20U, 24U, 28U, 32U
    };
    static const uint32_t overlay_starts[3] = {0U, 42U, 122U};
    static const uint16_t overlay_counts[3] = {42U, 80U, 84U};
    static const uint16_t overlay_widths[3] = {7U, 10U, 14U};
    static const uint16_t overlay_heights[3] = {6U, 8U, 6U};
    static const uint16_t overlay_x[3] = {5U, 5U, 5U};
    static const uint16_t overlay_y[3] = {23U, 19U, 21U};
    static const uint8_t expected_period_values[5] = {2U, 3U, 4U, 8U, 12U};
    size_t i;

    if (bytes == NULL || count != START_MENU_PAYLOAD_SIZE ||
        fnv1a32(bytes, count) != 0xA6C1E06BU ||
        memcmp(bytes, "TSGM", 4U) != 0 ||
        read_u16(bytes + 4U) != 1U ||
        read_u16(bytes + 6U) != START_MENU_HEADER_SIZE ||
        read_u16(bytes + 8U) != 32U || read_u16(bytes + 10U) != 30U ||
        read_u16(bytes + 12U) != 2U || read_u16(bytes + 14U) != 6U ||
        read_u32(bytes + 16U) != TECMO_START_GAME_MENU_CELL_COUNT ||
        read_u32(bytes + 20U) != START_MENU_CELLS_OFFSET ||
        read_u32(bytes + 24U) != START_MENU_PALETTES_OFFSET ||
        read_u16(bytes + 28U) != TECMO_START_GAME_MENU_PALETTE_STAGE_COUNT ||
        read_u16(bytes + 30U) != 32U ||
        read_u32(bytes + 32U) != START_MENU_EMBLEM_OFFSET ||
        read_u16(bytes + 36U) != TECMO_START_GAME_MENU_EMBLEM_COUNT ||
        read_u16(bytes + 38U) != START_MENU_SPRITE_STRIDE ||
        read_u32(bytes + 40U) != START_MENU_CURSOR_OFFSET ||
        read_u32(bytes + 44U) != START_MENU_PAYLOAD_SIZE ||
        read_u16(bytes + 48U) != 32U ||
        read_u16(bytes + 50U) != TECMO_START_GAME_MENU_ROOT_COUNT ||
        read_u16(bytes + 52U) != TECMO_START_GAME_MENU_SEASON_COUNT ||
        read_u16(bytes + 54U) != 8U || read_u16(bytes + 56U) != 32U ||
        read_u16(bytes + 58U) != 8U || read_u16(bytes + 60U) != 5U ||
        read_u16(bytes + 62U) != 31U || read_u16(bytes + 64U) != 88U ||
        read_u16(bytes + 66U) != 16U || read_u16(bytes + 68U) != 184U ||
        read_u16(bytes + 70U) != 60U ||
        read_u32(bytes + 72U) != START_MENU_OVERLAY_DESCS_OFFSET ||
        read_u16(bytes + 76U) != TECMO_START_GAME_MENU_OVERLAY_COUNT ||
        read_u16(bytes + 78U) != START_MENU_OVERLAY_DESC_STRIDE ||
        read_u32(bytes + 80U) != START_MENU_OVERLAY_CELLS_OFFSET ||
        read_u32(bytes + 84U) != TECMO_START_GAME_MENU_OVERLAY_CELL_COUNT ||
        read_u32(bytes + 88U) != START_MENU_DIGITS_OFFSET ||
        read_u16(bytes + 92U) != TECMO_START_GAME_MENU_DIGIT_COUNT ||
        read_u16(bytes + 94U) != START_MENU_CELL_STRIDE)
        return false;

    asset->stable_frame = read_u16(bytes + 48U);
    asset->repeat_frames = read_u16(bytes + 54U);
    asset->slide_frames = read_u16(bytes + 56U);
    asset->background_step = read_u16(bytes + 58U);
    asset->emblem_step = read_u16(bytes + 60U);
    asset->cursor_x = read_u16(bytes + 62U);
    asset->cursor_y = read_u16(bytes + 64U);
    asset->cursor_stride = read_u16(bytes + 66U);
    asset->emblem_anchor_x = read_u16(bytes + 68U);
    asset->emblem_anchor_y = read_u16(bytes + 70U);

    for (i = 0U; i < TECMO_START_GAME_MENU_PALETTE_STAGE_COUNT; ++i) {
        asset->stage_frames[i] = read_u16(bytes + 96U + i * 2U);
        if (asset->stage_frames[i] != expected_stage_frames[i]) return false;
    }
    for (i = 0U; i < TECMO_START_GAME_MENU_ROOT_COUNT; ++i) {
        asset->routes[i] = bytes[114U + i];
        if (asset->routes[i] != i + 1U) return false;
    }
    for (i = 0U; i < 5U; ++i) {
        asset->period_values[i] = bytes[121U + i];
        if (asset->period_values[i] != expected_period_values[i]) return false;
    }
    for (i = 126U; i < START_MENU_HEADER_SIZE; ++i)
        if (bytes[i] != 0U) return false;

    for (i = 0U; i < TECMO_START_GAME_MENU_CELL_COUNT; ++i)
        if (!parse_cell(&asset->cells[i], bytes + START_MENU_CELLS_OFFSET + i * 6U))
            return false;
    for (i = 0U; i < TECMO_START_GAME_MENU_PALETTE_STAGE_COUNT * 32U; ++i) {
        uint8_t value = bytes[START_MENU_PALETTES_OFFSET + i];
        if (value > 0x3FU) return false;
        asset->palettes[i / 32U][i % 32U] = value;
    }
    for (i = 0U; i < 32U; ++i)
        if (asset->palettes[4][i] != 0x0FU) return false;
    for (i = 0U; i < TECMO_START_GAME_MENU_EMBLEM_COUNT; ++i)
        if (!parse_sprite(&asset->emblem[i],
                          bytes + START_MENU_EMBLEM_OFFSET + i * 16U, false))
            return false;
    if (!parse_sprite(&asset->cursor, bytes + START_MENU_CURSOR_OFFSET, true) ||
        asset->cursor.top_chr_offset != 0xC240U ||
        asset->cursor.bottom_chr_offset != 0xC250U)
        return false;

    for (i = 0U; i < TECMO_START_GAME_MENU_OVERLAY_COUNT; ++i) {
        const uint8_t *source = bytes + START_MENU_OVERLAY_DESCS_OFFSET + i * 16U;
        TecmoStartGameMenuOverlay *overlay = &asset->overlays[i];
        overlay->cell_start = read_u32(source);
        overlay->cell_count = read_u16(source + 4U);
        overlay->width = read_u16(source + 6U);
        overlay->height = read_u16(source + 8U);
        overlay->x = read_u16(source + 10U);
        overlay->y = read_u16(source + 12U);
        overlay->type = source[14U];
        if (source[15U] != 0U || overlay->cell_start != overlay_starts[i] ||
            overlay->cell_count != overlay_counts[i] || overlay->type != i ||
            overlay->width != overlay_widths[i] || overlay->height != overlay_heights[i] ||
            overlay->x != overlay_x[i] || overlay->y != overlay_y[i] ||
            (uint32_t)overlay->width * overlay->height != overlay->cell_count ||
            overlay->x + overlay->width > 64U || overlay->y + overlay->height > 30U ||
            overlay->cell_start + overlay->cell_count > TECMO_START_GAME_MENU_OVERLAY_CELL_COUNT)
            return false;
    }
    for (i = 0U; i < TECMO_START_GAME_MENU_OVERLAY_CELL_COUNT; ++i)
        if (!parse_cell(&asset->overlay_cells[i],
                        bytes + START_MENU_OVERLAY_CELLS_OFFSET + i * 6U))
            return false;
    for (i = 0U; i < TECMO_START_GAME_MENU_DIGIT_COUNT; ++i)
        if (!parse_cell(&asset->digits[i], bytes + START_MENU_DIGITS_OFFSET + i * 6U))
            return false;
    return true;
}

bool tecmo_start_game_menu_asset_load(TecmoStartGameMenuAsset *asset,
                                      const char *project_root)
{
    uint8_t *menu = NULL;
    uint8_t *chr = NULL;
    uint64_t menu_count = 0U;
    uint64_t chr_count = 0U;
    bool ok;

    if (asset == NULL) return false;
    memset(asset, 0, sizeof(*asset));
    if (!read_entry_from_selected_pack(project_root, &menu, &menu_count, &chr, &chr_count)) {
        (void)snprintf(asset->status, sizeof(asset->status),
                       "TSGM-1 menu/start-game entry unavailable");
        return false;
    }
    ok = parse_payload(asset, menu, menu_count);
    if (ok) {
        asset->chr_byte_count = chr_count;
        asset->chr_fingerprint = fnv1a64(chr, chr_count);
        asset->available = true;
        ok = tecmo_start_game_menu_asset_chr_available(asset, chr, chr_count);
    }
    if (!ok) asset->available = false;
    tecmo_asset_pack_free(menu);
    tecmo_asset_pack_free(chr);
    (void)snprintf(asset->status, sizeof(asset->status), "%s",
                   ok ? "TSGM-1 native assetpack" : "TSGM-1 asset contract rejected");
    return ok;
}

bool tecmo_start_game_menu_asset_chr_available(const TecmoStartGameMenuAsset *asset,
                                               const uint8_t *chr_bytes,
                                               uint64_t chr_byte_count)
{
    size_t i;
    if (asset == NULL || chr_bytes == NULL || chr_byte_count == 0U ||
        (asset->chr_byte_count != 0U &&
         (asset->chr_byte_count != chr_byte_count ||
          asset->chr_fingerprint != fnv1a64(chr_bytes, chr_byte_count))))
        return false;
    for (i = 0U; i < TECMO_START_GAME_MENU_CELL_COUNT; ++i)
        if ((uint64_t)asset->cells[i].chr_offset + 16U > chr_byte_count) return false;
    for (i = 0U; i < TECMO_START_GAME_MENU_EMBLEM_COUNT; ++i)
        if ((uint64_t)asset->emblem[i].bottom_chr_offset + 16U > chr_byte_count)
            return false;
    if ((uint64_t)asset->cursor.bottom_chr_offset + 16U > chr_byte_count) return false;
    for (i = 0U; i < TECMO_START_GAME_MENU_OVERLAY_CELL_COUNT; ++i)
        if ((uint64_t)asset->overlay_cells[i].chr_offset + 16U > chr_byte_count)
            return false;
    for (i = 0U; i < TECMO_START_GAME_MENU_DIGIT_COUNT; ++i)
        if ((uint64_t)asset->digits[i].chr_offset + 16U > chr_byte_count) return false;
    return true;
}

void tecmo_start_game_menu_state_init(TecmoStartGameMenuState *state)
{
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->phase = TECMO_START_GAME_MENU_REVEAL;
    state->music_value = 1U;
    state->speed_value = 1U;
    state->period_index = 1U;
}

static void reset_repeat(TecmoStartGameMenuState *state)
{
    state->repeat_direction = 0;
    state->repeat_counter = 0U;
}

static bool vertical_repeat(TecmoStartGameMenuState *state,
                            const TecmoStartGameMenuAsset *asset,
                            const TecmoControlFrame *controls,
                            int *direction)
{
    int held_direction = 0;
    if (controls->held.up != controls->held.down)
        held_direction = controls->held.up ? -1 : 1;
    if (held_direction == 0) {
        reset_repeat(state);
        return false;
    }
    if (state->repeat_direction != held_direction) {
        state->repeat_direction = (int8_t)held_direction;
        state->repeat_counter = asset->repeat_frames;
        *direction = held_direction;
        return true;
    }
    if (state->repeat_counter > 1U) {
        --state->repeat_counter;
        return false;
    }
    state->repeat_counter = asset->repeat_frames;
    *direction = held_direction;
    return true;
}

static uint8_t wrap_selection(uint8_t selection, unsigned count, int direction)
{
    if (direction < 0) return selection == 0U ? (uint8_t)(count - 1U)
                                               : (uint8_t)(selection - 1U);
    return selection + 1U >= count ? 0U : (uint8_t)(selection + 1U);
}

static uint8_t clamp_selection(uint8_t selection, unsigned count, int direction)
{
    if (direction < 0) return selection == 0U ? 0U : (uint8_t)(selection - 1U);
    return selection + 1U >= count ? (uint8_t)(count - 1U)
                                   : (uint8_t)(selection + 1U);
}

typedef enum StartMenuSelectionAction {
    START_MENU_SELECTION_NONE,
    START_MENU_SELECTION_ACCEPT,
    START_MENU_SELECTION_CANCEL
} StartMenuSelectionAction;

static bool root_action_active(const TecmoControlFrame *controls)
{
    return controls->held.shoot || controls->held.cancel;
}

static StartMenuSelectionAction selection_action(const TecmoControlFrame *controls)
{
    if (controls->held.shoot) return START_MENU_SELECTION_ACCEPT;
    if (controls->held.cancel) return START_MENU_SELECTION_CANCEL;
    return START_MENU_SELECTION_NONE;
}

TecmoStartGameMenuAction tecmo_start_game_menu_update(
    TecmoStartGameMenuState *state,
    const TecmoStartGameMenuAsset *asset,
    const TecmoControlFrame *controls)
{
    int direction = 0;
    bool activate;
    StartMenuSelectionAction action;
    if (state == NULL || asset == NULL || controls == NULL) return TECMO_START_GAME_MENU_ACTION_NONE;
    ++state->frame;
    if (!asset->available) return TECMO_START_GAME_MENU_ACTION_NONE;
    if (state->phase == TECMO_START_GAME_MENU_REVEAL) {
        if (state->frame < asset->stable_frame) return TECMO_START_GAME_MENU_ACTION_NONE;
        state->phase = TECMO_START_GAME_MENU_ROOT;
        reset_repeat(state);
    }
    if (state->phase == TECMO_START_GAME_MENU_SEASON_SLIDE_IN) {
        if (state->slide_frame < asset->slide_frames) ++state->slide_frame;
        if (state->slide_frame >= asset->slide_frames) {
            state->slide_frame = asset->slide_frames;
            state->phase = TECMO_START_GAME_MENU_SEASON;
            reset_repeat(state);
        }
        return TECMO_START_GAME_MENU_ACTION_NONE;
    }
    if (state->phase == TECMO_START_GAME_MENU_SEASON_SLIDE_OUT) {
        if (state->slide_frame > 0U) --state->slide_frame;
        if (state->slide_frame == 0U) {
            state->phase = TECMO_START_GAME_MENU_ROOT;
            reset_repeat(state);
        }
        return TECMO_START_GAME_MENU_ACTION_NONE;
    }

    if (state->phase == TECMO_START_GAME_MENU_ROOT) {
        if (vertical_repeat(state, asset, controls, &direction))
            state->root_selection = wrap_selection(state->root_selection,
                                                   TECMO_START_GAME_MENU_ROOT_COUNT,
                                                   direction);
        activate = root_action_active(controls);
        if (!activate) return TECMO_START_GAME_MENU_ACTION_NONE;
        reset_repeat(state);
        switch (asset->routes[state->root_selection]) {
        case 1U: return TECMO_START_GAME_MENU_ACTION_PLAY_SETUP;
        case 2U:
            state->phase = TECMO_START_GAME_MENU_SEASON_SLIDE_IN;
            state->slide_frame = 0U;
            return TECMO_START_GAME_MENU_ACTION_NONE;
        case 3U: return TECMO_START_GAME_MENU_ACTION_PLAY_SETUP;
        case 4U: return TECMO_START_GAME_MENU_ACTION_ROSTERS;
        case 5U:
            state->phase = TECMO_START_GAME_MENU_SPEED;
            state->setting_selection = state->speed_value;
            return TECMO_START_GAME_MENU_ACTION_NONE;
        case 6U:
            state->phase = TECMO_START_GAME_MENU_PERIOD;
            state->setting_selection = state->period_index;
            return TECMO_START_GAME_MENU_ACTION_NONE;
        case 7U:
            state->phase = TECMO_START_GAME_MENU_MUSIC;
            state->setting_selection = state->music_value;
            return TECMO_START_GAME_MENU_ACTION_NONE;
        default: return TECMO_START_GAME_MENU_ACTION_NONE;
        }
    }

    if (state->phase == TECMO_START_GAME_MENU_SEASON) {
        action = selection_action(controls);
        if (action == START_MENU_SELECTION_CANCEL) {
            state->phase = TECMO_START_GAME_MENU_SEASON_SLIDE_OUT;
            reset_repeat(state);
            return TECMO_START_GAME_MENU_ACTION_NONE;
        }
        if (vertical_repeat(state, asset, controls, &direction))
            state->season_selection = wrap_selection(state->season_selection,
                                                     TECMO_START_GAME_MENU_SEASON_COUNT,
                                                     direction);
        if (action != START_MENU_SELECTION_ACCEPT) return TECMO_START_GAME_MENU_ACTION_NONE;
        if (state->season_selection == 2U) return TECMO_START_GAME_MENU_ACTION_PLAY_SETUP;
        if (state->season_selection == 5U) return TECMO_START_GAME_MENU_ACTION_ROSTERS;
        return TECMO_START_GAME_MENU_ACTION_NONE;
    }

    action = selection_action(controls);
    if (action == START_MENU_SELECTION_CANCEL) {
        state->phase = TECMO_START_GAME_MENU_ROOT;
        reset_repeat(state);
        return TECMO_START_GAME_MENU_ACTION_NONE;
    }
    if (vertical_repeat(state, asset, controls, &direction)) {
        if (state->phase == TECMO_START_GAME_MENU_MUSIC)
            state->setting_selection = wrap_selection(state->setting_selection, 2U, direction);
        else if (state->phase == TECMO_START_GAME_MENU_SPEED)
            state->setting_selection = wrap_selection(state->setting_selection, 3U, direction);
        else
            state->setting_selection = clamp_selection(state->setting_selection, 5U, -direction);
    }
    if (action == START_MENU_SELECTION_ACCEPT) {
        if (state->phase == TECMO_START_GAME_MENU_MUSIC)
            state->music_value = state->setting_selection;
        else if (state->phase == TECMO_START_GAME_MENU_SPEED)
            state->speed_value = state->setting_selection;
        else
            state->period_index = state->setting_selection;
        state->phase = TECMO_START_GAME_MENU_ROOT;
        reset_repeat(state);
    }
    return TECMO_START_GAME_MENU_ACTION_NONE;
}

static bool make_viewport(TecmoFramebuffer *view,
                          TecmoFramebuffer *framebuffer,
                          int origin_x,
                          int origin_y,
                          int scale)
{
    if (view == NULL || framebuffer == NULL || framebuffer->pixels == NULL || scale <= 0 ||
        origin_x < 0 || origin_y < 0 || origin_x + 256 * scale > framebuffer->width ||
        origin_y + 240 * scale > framebuffer->height)
        return false;
    view->pixels = framebuffer->pixels + (size_t)origin_y * (size_t)framebuffer->pitch_pixels +
                   (size_t)origin_x;
    view->width = 256 * scale;
    view->height = 240 * scale;
    view->pitch_pixels = framebuffer->pitch_pixels;
    return true;
}

static void fill_viewport(TecmoFramebuffer *view, uint32_t color)
{
    int y;
    for (y = 0; y < view->height; ++y) {
        uint32_t *row = view->pixels + (size_t)y * (size_t)view->pitch_pixels;
        int x;
        for (x = 0; x < view->width; ++x) row[x] = color;
    }
}

static void fill_viewport_rect(TecmoFramebuffer *view,
                               int x,
                               int y,
                               int width,
                               int height,
                               uint32_t color)
{
    int y0 = y < 0 ? 0 : y;
    int y1 = y + height > view->height ? view->height : y + height;
    int x0 = x < 0 ? 0 : x;
    int x1 = x + width > view->width ? view->width : x + width;
    int py;
    for (py = y0; py < y1; ++py) {
        uint32_t *row = view->pixels + (size_t)py * (size_t)view->pitch_pixels;
        int px;
        for (px = x0; px < x1; ++px) row[px] = color;
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
    size_t i;
    rgba[0] = tecmo_nes_2c02_rgba(palette[0]);
    for (i = 1U; i < 4U; ++i)
        rgba[i] = tecmo_nes_2c02_rgba(palette[(size_t)cell->palette_index * 4U + i]);
    tecmo_draw_chr_tile_at_offset_ex(view, chr_bytes, chr_byte_count,
                                     cell->chr_offset, x, y, scale, rgba, false, false);
}

static void draw_menu_cells(TecmoFramebuffer *view,
                            const TecmoStartGameMenuAsset *asset,
                            const uint8_t *chr_bytes,
                            uint64_t chr_byte_count,
                            const uint8_t palette[16],
                            int background_x,
                            int scale)
{
    size_t i;
    for (i = 0U; i < TECMO_START_GAME_MENU_CELL_COUNT; ++i) {
        int virtual_col = (int)(i % 960U % 32U) + (i >= 960U ? 32 : 0);
        int row = (int)(i % 960U / 32U);
        int x = background_x + virtual_col * 8;
        if (x <= -8 || x >= 256) continue;
        draw_cell(view, &asset->cells[i], palette, chr_bytes, chr_byte_count,
                  x * scale, row * 8 * scale, scale);
    }
}

static void draw_title_cells(TecmoFramebuffer *view,
                             const TecmoTitleAsset *title_asset,
                             const uint8_t *chr_bytes,
                             uint64_t chr_byte_count,
                             const uint8_t palette[16],
                             int scale)
{
    size_t i;
    for (i = 0U; i < TECMO_TITLE_CELL_COUNT; ++i) {
        TecmoStartGameMenuCell cell;
        cell.tile_id = title_asset->start_cells[i].tile_id;
        cell.palette_index = title_asset->start_cells[i].palette_index;
        cell.chr_offset = title_asset->start_cells[i].chr_offset;
        draw_cell(view, &cell, palette, chr_bytes, chr_byte_count,
                  (int)(i % 32U) * 8 * scale, (int)(i / 32U) * 8 * scale, scale);
    }
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
    size_t i;
    size_t base = (size_t)sprite->palette_index * 4U;
    for (i = 1U; i < 4U; ++i) rgba[i] = tecmo_nes_2c02_rgba(palette[base + i]);
    tecmo_draw_chr_tile_at_offset_ex(view, chr_bytes, chr_byte_count,
                                     sprite->top_chr_offset, x * scale, (y + 1) * scale,
                                     scale, rgba, (sprite->flags & 2U) != 0U,
                                     (sprite->flags & 4U) != 0U);
    tecmo_draw_chr_tile_at_offset_ex(view, chr_bytes, chr_byte_count,
                                     sprite->bottom_chr_offset, x * scale, (y + 9) * scale,
                                     scale, rgba, (sprite->flags & 2U) != 0U,
                                     (sprite->flags & 4U) != 0U);
}

static void draw_emblem(TecmoFramebuffer *view,
                        const TecmoStartGameMenuAsset *asset,
                        const uint8_t *chr_bytes,
                        uint64_t chr_byte_count,
                        const uint8_t sprite_palette[16],
                        int anchor_x,
                        int scale)
{
    size_t left;
    for (left = TECMO_START_GAME_MENU_EMBLEM_COUNT; left > 0U; --left) {
        const TecmoStartGameMenuSprite *piece = &asset->emblem[left - 1U];
        draw_sprite_piece(view, piece, sprite_palette, chr_bytes, chr_byte_count,
                          anchor_x + piece->dx, (int)asset->emblem_anchor_y + piece->dy,
                          scale);
    }
}

static void draw_cursor(TecmoFramebuffer *view,
                        const TecmoStartGameMenuAsset *asset,
                        const uint8_t *chr_bytes,
                        uint64_t chr_byte_count,
                        const uint8_t sprite_palette[16],
                        int x,
                        int y,
                        int scale)
{
    draw_sprite_piece(view, &asset->cursor, sprite_palette, chr_bytes,
                      chr_byte_count, x, y, scale);
}

static void draw_overlay(TecmoFramebuffer *view,
                         const TecmoStartGameMenuAsset *asset,
                         size_t index,
                         const uint8_t *chr_bytes,
                         uint64_t chr_byte_count,
                         const uint8_t palette[16],
                         int scale)
{
    const TecmoStartGameMenuOverlay *overlay = &asset->overlays[index];
    size_t i;
    fill_viewport_rect(view,
                       (int)overlay->x * 8 * scale,
                       (int)overlay->y * 8 * scale,
                       (int)overlay->width * 8 * scale,
                       (int)overlay->height * 8 * scale,
                       tecmo_nes_2c02_rgba(palette[0]));
    for (i = 0U; i < overlay->cell_count; ++i) {
        unsigned col = (unsigned)(i % overlay->width);
        unsigned row = (unsigned)(i / overlay->width);
        draw_cell(view, &asset->overlay_cells[overlay->cell_start + i], palette,
                  chr_bytes, chr_byte_count,
                  ((int)overlay->x + (int)col) * 8 * scale,
                  ((int)overlay->y + (int)row) * 8 * scale, scale);
    }
}

static void draw_period_value(TecmoFramebuffer *view,
                              const TecmoStartGameMenuAsset *asset,
                              const uint8_t *chr_bytes,
                              uint64_t chr_byte_count,
                              const uint8_t palette[16],
                              unsigned period_index,
                              int scale)
{
    unsigned value = asset->period_values[period_index < 5U ? period_index : 4U];
    const TecmoStartGameMenuOverlay *overlay = &asset->overlays[2];
    int row = ((int)overlay->y + 4) * 8 * scale;
    int col = (int)overlay->x + 5;
    if (value >= 10U) {
        draw_cell(view, &asset->digits[value / 10U], palette, chr_bytes, chr_byte_count,
                  (col - 1) * 8 * scale, row, scale);
    }
    draw_cell(view, &asset->digits[value % 10U], palette, chr_bytes, chr_byte_count,
              col * 8 * scale, row, scale);
}

bool tecmo_start_game_menu_draw(TecmoFramebuffer *framebuffer,
                                const TecmoStartGameMenuAsset *asset,
                                const TecmoStartGameMenuState *state,
                                const TecmoTitleAsset *title_asset,
                                const uint8_t *chr_bytes,
                                uint64_t chr_byte_count,
                                int origin_x,
                                int origin_y,
                                int scale)
{
    TecmoFramebuffer view;
    unsigned stage = 0U;
    int background_x = 0;
    int emblem_x;
    size_t i;
    if (asset == NULL || state == NULL || !asset->available ||
        !tecmo_start_game_menu_asset_chr_available(asset, chr_bytes, chr_byte_count) ||
        !make_viewport(&view, framebuffer, origin_x, origin_y, scale))
        return false;
    for (i = 1U; i < TECMO_START_GAME_MENU_PALETTE_STAGE_COUNT; ++i)
        if (state->frame >= asset->stage_frames[i]) stage = (unsigned)i;

    fill_viewport(&view, tecmo_nes_2c02_rgba(asset->palettes[stage][0]));
    if (state->frame < asset->stage_frames[4]) {
        if (title_asset == NULL || !title_asset->start_available ||
            !tecmo_title_asset_chr_available(title_asset, chr_bytes, chr_byte_count))
            return false;
        draw_title_cells(&view, title_asset, chr_bytes, chr_byte_count,
                         asset->palettes[stage], scale);
        return true;
    }
    if (state->frame < asset->stage_frames[5]) return true;

    if (state->phase == TECMO_START_GAME_MENU_SEASON_SLIDE_IN ||
        state->phase == TECMO_START_GAME_MENU_SEASON ||
        state->phase == TECMO_START_GAME_MENU_SEASON_SLIDE_OUT) {
        background_x = -(int)state->slide_frame * (int)asset->background_step;
    }
    emblem_x = (int)asset->emblem_anchor_x +
                background_x / (int)asset->background_step * (int)asset->emblem_step;
    draw_menu_cells(&view, asset, chr_bytes, chr_byte_count,
                    asset->palettes[stage], background_x, scale);
    draw_emblem(&view, asset, chr_bytes, chr_byte_count,
                asset->palettes[stage] + 16U, emblem_x, scale);

    if (state->phase == TECMO_START_GAME_MENU_MUSIC ||
        state->phase == TECMO_START_GAME_MENU_SPEED ||
        state->phase == TECMO_START_GAME_MENU_PERIOD) {
        size_t overlay_index = state->phase == TECMO_START_GAME_MENU_MUSIC ? 0U :
                               state->phase == TECMO_START_GAME_MENU_SPEED ? 1U : 2U;
        const TecmoStartGameMenuOverlay *overlay = &asset->overlays[overlay_index];
        draw_overlay(&view, asset, overlay_index, chr_bytes, chr_byte_count,
                     asset->palettes[stage], scale);
        if (overlay_index == 2U)
            draw_period_value(&view, asset, chr_bytes, chr_byte_count,
                              asset->palettes[stage], state->setting_selection, scale);
        draw_cursor(&view, asset, chr_bytes, chr_byte_count,
                    asset->palettes[stage] + 16U,
                    overlay_index == 2U ? 71 : (int)overlay->x * 8 - 9,
                    ((int)overlay->y + 2 + (overlay_index == 2U ? 2 :
                      2 * (int)state->setting_selection)) * 8,
                    scale);
    } else if (state->phase == TECMO_START_GAME_MENU_ROOT ||
               (state->phase == TECMO_START_GAME_MENU_REVEAL && state->frame >= 20U)) {
        draw_cursor(&view, asset, chr_bytes, chr_byte_count,
                    asset->palettes[stage] + 16U, asset->cursor_x,
                    (int)asset->cursor_y + (int)state->root_selection * asset->cursor_stride,
                    scale);
    } else if (state->phase == TECMO_START_GAME_MENU_SEASON) {
        draw_cursor(&view, asset, chr_bytes, chr_byte_count,
                    asset->palettes[stage] + 16U, START_MENU_SEASON_CURSOR_X,
                    (int)asset->cursor_y + (int)state->season_selection * asset->cursor_stride,
                    scale);
    }
    return true;
}

const char *tecmo_start_game_menu_phase_name(TecmoStartGameMenuPhase phase)
{
    switch (phase) {
    case TECMO_START_GAME_MENU_REVEAL: return "reveal";
    case TECMO_START_GAME_MENU_ROOT: return "root";
    case TECMO_START_GAME_MENU_SEASON_SLIDE_IN: return "season-slide-in";
    case TECMO_START_GAME_MENU_SEASON: return "season";
    case TECMO_START_GAME_MENU_SEASON_SLIDE_OUT: return "season-slide-out";
    case TECMO_START_GAME_MENU_MUSIC: return "music";
    case TECMO_START_GAME_MENU_SPEED: return "speed";
    case TECMO_START_GAME_MENU_PERIOD: return "period";
    default: return "unknown";
    }
}
