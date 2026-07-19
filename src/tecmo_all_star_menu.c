#include "tecmo_all_star_menu.h"

#include "tecmo_asset_pack.h"
#include "tecmo_nes_video.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALL_STAR_ENTRY_ID "menu/all-star"
#define ALL_STAR_PAYLOAD_SIZE 524U
#define ALL_STAR_PAYLOAD_FNV1A32 0xF02A2A45U
#define ALL_STAR_TPRE_SIZE 26736U
#define ALL_STAR_TPRE_FNV1A32 0xD9EE49F4U
#define ALL_STAR_TSGM_SIZE 14112U
#define ALL_STAR_TSGM_FNV1A32 0xDF89006BU
#define ALL_STAR_CHR_SIZE 262144U
#define ALL_STAR_CHR_FNV1A32 0xF6F6E854U
#define ALL_STAR_CHR_FNV1A64 0x96A64F53B240ABB4ULL
#define ALL_STAR_OVERLAY_DESCS_OFFSET 256U
#define ALL_STAR_OVERLAY_CELLS_OFFSET 272U
#define ALL_STAR_CONTROL_HEIGHT 14U
#define ALL_STAR_DIFFICULTY_HEIGHT 8U
#define ALL_STAR_TEAM_HEIGHT 6U
#define ALL_STAR_WEST_TEAM 0x1BU
#define ALL_STAR_EAST_TEAM 0x1CU

typedef enum AllStarButton {
    ALL_STAR_BUTTON_RIGHT = 0x01,
    ALL_STAR_BUTTON_LEFT = 0x02,
    ALL_STAR_BUTTON_DOWN = 0x04,
    ALL_STAR_BUTTON_UP = 0x08,
    ALL_STAR_BUTTON_START = 0x10,
    ALL_STAR_BUTTON_SELECT = 0x20,
    ALL_STAR_BUTTON_B = 0x40,
    ALL_STAR_BUTTON_A = 0x80
} AllStarButton;

typedef enum AllStarSelectionAction {
    ALL_STAR_SELECTION_NONE,
    ALL_STAR_SELECTION_ACCEPT,
    ALL_STAR_SELECTION_CANCEL
} AllStarSelectionAction;

static uint16_t read_u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8U));
}

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8U) |
           ((uint32_t)p[2] << 16U) | ((uint32_t)p[3] << 24U);
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

static int make_path(char *path, size_t size, const char *root,
                     const char *suffix)
{
    size_t n;
    int written;
    if (path == NULL || size == 0U || root == NULL || root[0] == '\0')
        return -1;
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

static void free_dependencies(uint8_t **all_star,
                              uint8_t **preseason,
                              uint8_t **start_menu,
                              uint8_t **chr)
{
    tecmo_asset_pack_free(*all_star);
    tecmo_asset_pack_free(*preseason);
    tecmo_asset_pack_free(*start_menu);
    tecmo_asset_pack_free(*chr);
    *all_star = NULL;
    *preseason = NULL;
    *start_menu = NULL;
    *chr = NULL;
}

static bool read_dependencies_from_selected_pack(const char *root,
                                                 uint8_t **all_star,
                                                 uint64_t *all_star_count,
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
        if (make_path(root_build, sizeof(root_build), root,
                      "build\\tecmo.assetpack") == 0)
            paths[path_count++] = root_build;
        if (make_path(root_pack, sizeof(root_pack), root,
                      "tecmo.assetpack") == 0)
            paths[path_count++] = root_pack;
        paths[path_count++] = "build\\tecmo.assetpack";
        paths[path_count++] = "tecmo.assetpack";
    }

    for (size_t i = 0U; i < path_count; ++i) {
        if (!file_exists(paths[i])) continue;
        if (tecmo_asset_pack_read_entry_exact(paths[i], ALL_STAR_ENTRY_ID,
                                              ALL_STAR_PAYLOAD_SIZE,
                                              all_star, all_star_count) != 0)
            return false;
        if (tecmo_asset_pack_read_entry_exact(paths[i], "menu/preseason",
                                              ALL_STAR_TPRE_SIZE,
                                              preseason, preseason_count) != 0 ||
            tecmo_asset_pack_read_entry_exact(paths[i], "menu/start-game",
                                              ALL_STAR_TSGM_SIZE,
                                              start_menu, start_menu_count) != 0 ||
            tecmo_asset_pack_read_entry_exact(paths[i], "chr/all",
                                              ALL_STAR_CHR_SIZE,
                                              chr, chr_count) != 0) {
            free_dependencies(all_star, preseason, start_menu, chr);
            *all_star_count = 0U;
            *preseason_count = 0U;
            *start_menu_count = 0U;
            *chr_count = 0U;
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
    return cell->palette_index <= 3U &&
           cell->chr_offset == bg_chr_offset(cell->tile_id, 0xFAU, 0xFAU);
}

static bool parse_payload(TecmoAllStarAsset *asset,
                          const uint8_t *bytes,
                          uint64_t count)
{
    static const uint8_t expected_ownership[12] = {
        0U, 0U, 0U, 2U, 2U, 1U,
        1U, 0U, 2U, 1U, 2U, 1U
    };
    static const uint8_t expected_configs[3] = {0x14U, 0x13U, 0x1CU};
    static const uint16_t expected_cursor[3][3] = {
        {47U, 120U, 16U},
        {64U, 104U, 16U},
        {63U, 152U, 16U}
    };
    const uint8_t *desc;

    if (bytes == NULL || count != ALL_STAR_PAYLOAD_SIZE ||
        fnv1a32(bytes, count) != ALL_STAR_PAYLOAD_FNV1A32 ||
        memcmp(bytes, "TALL", 4U) != 0 ||
        read_u16(bytes + 4U) != 1U || read_u16(bytes + 6U) != 256U ||
        read_u16(bytes + 8U) != 32U || read_u16(bytes + 10U) != 30U ||
        read_u16(bytes + 12U) != 1U || read_u16(bytes + 14U) != 42U ||
        read_u32(bytes + 16U) != ALL_STAR_OVERLAY_DESCS_OFFSET ||
        read_u32(bytes + 20U) != ALL_STAR_OVERLAY_CELLS_OFFSET ||
        read_u32(bytes + 24U) != ALL_STAR_PAYLOAD_SIZE ||
        read_u32(bytes + 28U) != ALL_STAR_TPRE_SIZE ||
        read_u32(bytes + 32U) != ALL_STAR_TPRE_FNV1A32 ||
        read_u32(bytes + 36U) != ALL_STAR_TSGM_SIZE ||
        read_u32(bytes + 40U) != ALL_STAR_TSGM_FNV1A32 ||
        read_u32(bytes + 44U) != ALL_STAR_CHR_SIZE ||
        read_u32(bytes + 48U) != ALL_STAR_CHR_FNV1A32 ||
        bytes[52U] != 1U || bytes[53U] != 1U || bytes[54U] != 5U ||
        bytes[55U] != 8U || bytes[56U] != TECMO_ALL_STAR_CONTROL_COUNT ||
        bytes[57U] != TECMO_ALL_STAR_DIFFICULTY_COUNT ||
        bytes[58U] != TECMO_ALL_STAR_TEAM_SIDE_COUNT ||
        bytes[59U] != ALL_STAR_WEST_TEAM || bytes[60U] != ALL_STAR_EAST_TEAM ||
        bytes[61U] != (ALL_STAR_BUTTON_A | ALL_STAR_BUTTON_B) ||
        bytes[62U] != (ALL_STAR_BUTTON_UP | ALL_STAR_BUTTON_DOWN) ||
        bytes[63U] != 1U || bytes[64U] != 1U || bytes[65U] != 4U ||
        memcmp(bytes + 66U, expected_ownership,
               sizeof(expected_ownership)) != 0 ||
        memcmp(bytes + 78U, expected_configs, sizeof(expected_configs)) != 0 ||
        bytes[81U] != 0U || bytes[100U] != 0U || bytes[101U] != 1U ||
        bytes[102U] != 1U || bytes[103U] != 0U)
        return false;
    for (size_t i = 0U; i < 3U; ++i) {
        if (read_u16(bytes + 82U + i * 6U) != expected_cursor[i][0] ||
            read_u16(bytes + 84U + i * 6U) != expected_cursor[i][1] ||
            read_u16(bytes + 86U + i * 6U) != expected_cursor[i][2])
            return false;
    }
    for (size_t i = 104U; i < ALL_STAR_OVERLAY_DESCS_OFFSET; ++i)
        if (bytes[i] != 0U) return false;

    desc = bytes + ALL_STAR_OVERLAY_DESCS_OFFSET;
    asset->team_overlay.cell_start = read_u32(desc);
    asset->team_overlay.cell_count = read_u16(desc + 4U);
    asset->team_overlay.width = read_u16(desc + 6U);
    asset->team_overlay.height = read_u16(desc + 8U);
    asset->team_overlay.x = read_u16(desc + 10U);
    asset->team_overlay.y = read_u16(desc + 12U);
    asset->team_overlay.type = desc[14U];
    if (desc[15U] != 0U || asset->team_overlay.cell_start != 0U ||
        asset->team_overlay.cell_count != TECMO_ALL_STAR_TEAM_CELL_COUNT ||
        asset->team_overlay.width != 7U ||
        asset->team_overlay.height != ALL_STAR_TEAM_HEIGHT ||
        asset->team_overlay.x != 7U || asset->team_overlay.y != 17U ||
        asset->team_overlay.type != 0U)
        return false;
    for (size_t i = 0U; i < TECMO_ALL_STAR_TEAM_CELL_COUNT; ++i)
        if (!parse_cell(&asset->team_cells[i],
                        bytes + ALL_STAR_OVERLAY_CELLS_OFFSET + i * 6U))
            return false;

    asset->row_cadence = bytes[52U];
    asset->cursor_commit_delay_frames = bytes[53U];
    asset->accepted_input_seed = bytes[54U];
    asset->repeat_frames = bytes[55U];
    asset->west_team_code = bytes[59U];
    asset->east_team_code = bytes[60U];
    asset->release_input_mask = bytes[61U];
    asset->direction_input_mask = bytes[62U];
    asset->team_mode_rows[0] = bytes[64U];
    asset->team_mode_rows[1] = bytes[65U];
    memcpy(asset->ownership, bytes + 66U, 12U);
    memcpy(asset->configs, bytes + 78U, 3U);
    for (size_t i = 0U; i < 3U; ++i) {
        asset->cursor_x[i] = read_u16(bytes + 82U + i * 6U);
        asset->cursor_y[i] = read_u16(bytes + 84U + i * 6U);
        asset->cursor_stride[i] = read_u16(bytes + 86U + i * 6U);
    }
    asset->expected_preseason_size = read_u32(bytes + 28U);
    asset->expected_preseason_fingerprint32 = read_u32(bytes + 32U);
    asset->expected_start_menu_size = read_u32(bytes + 36U);
    asset->expected_start_menu_fingerprint32 = read_u32(bytes + 40U);
    asset->expected_chr_size = read_u32(bytes + 44U);
    asset->expected_chr_fingerprint32 = read_u32(bytes + 48U);
    return true;
}

bool tecmo_all_star_asset_load(TecmoAllStarAsset *asset,
                               const char *project_root)
{
    uint8_t *all_star = NULL;
    uint8_t *preseason = NULL;
    uint8_t *start_menu = NULL;
    uint8_t *chr = NULL;
    uint64_t all_star_count = 0U;
    uint64_t preseason_count = 0U;
    uint64_t start_menu_count = 0U;
    uint64_t chr_count = 0U;
    bool ok;

    if (asset == NULL) return false;
    memset(asset, 0, sizeof(*asset));
    if (!read_dependencies_from_selected_pack(
            project_root, &all_star, &all_star_count,
            &preseason, &preseason_count, &start_menu, &start_menu_count,
            &chr, &chr_count)) {
        (void)snprintf(asset->status, sizeof(asset->status),
                       "TALL-1 menu/all-star entry or dependency unavailable");
        return false;
    }
    ok = parse_payload(asset, all_star, all_star_count) &&
         preseason_count == asset->expected_preseason_size &&
         fnv1a32(preseason, preseason_count) ==
             asset->expected_preseason_fingerprint32 &&
         start_menu_count == asset->expected_start_menu_size &&
         fnv1a32(start_menu, start_menu_count) ==
             asset->expected_start_menu_fingerprint32 &&
         chr_count == asset->expected_chr_size &&
         fnv1a32(chr, chr_count) == asset->expected_chr_fingerprint32 &&
         fnv1a64(chr, chr_count) == ALL_STAR_CHR_FNV1A64;
    if (ok) {
        asset->chr_byte_count = chr_count;
        asset->chr_fingerprint = fnv1a64(chr, chr_count);
        asset->available = true;
        ok = tecmo_all_star_asset_chr_available(asset, chr, chr_count);
    }
    if (!ok) asset->available = false;
    free_dependencies(&all_star, &preseason, &start_menu, &chr);
    (void)snprintf(asset->status, sizeof(asset->status), "%s",
                   ok ? "TALL-1 native assetpack"
                      : "TALL-1 asset contract rejected");
    return ok;
}

bool tecmo_all_star_asset_chr_available(const TecmoAllStarAsset *asset,
                                        const uint8_t *chr_bytes,
                                        uint64_t chr_byte_count)
{
    if (asset == NULL || chr_bytes == NULL || !asset->available ||
        asset->expected_chr_size != ALL_STAR_CHR_SIZE ||
        asset->expected_chr_fingerprint32 != ALL_STAR_CHR_FNV1A32 ||
        chr_byte_count != asset->expected_chr_size ||
        asset->chr_byte_count != chr_byte_count ||
        asset->chr_fingerprint != ALL_STAR_CHR_FNV1A64 ||
        fnv1a32(chr_bytes, chr_byte_count) != asset->expected_chr_fingerprint32 ||
        fnv1a64(chr_bytes, chr_byte_count) != asset->chr_fingerprint)
        return false;
    for (size_t i = 0U; i < TECMO_ALL_STAR_TEAM_CELL_COUNT; ++i)
        if ((uint64_t)asset->team_cells[i].chr_offset + 16U > chr_byte_count)
            return false;
    return true;
}

void tecmo_all_star_state_init(TecmoAllStarState *state)
{
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->phase = TECMO_ALL_STAR_CONTROL_SETUP;
    state->west_team = ALL_STAR_WEST_TEAM;
    state->east_team = ALL_STAR_EAST_TEAM;
}

static bool all_star_state_valid(const TecmoAllStarAsset *asset,
                                 const TecmoAllStarState *state)
{
    uint16_t transition_limit;
    uint8_t cooldown_limit;
    if (asset == NULL || state == NULL || !asset->available ||
        (int)state->phase < 0 ||
        state->phase > TECMO_ALL_STAR_CONTROL_TEARDOWN_ROOT ||
        state->control_selection >= TECMO_ALL_STAR_CONTROL_COUNT ||
        state->difficulty_selection >= TECMO_ALL_STAR_DIFFICULTY_COUNT ||
        state->committed_difficulty >= TECMO_ALL_STAR_DIFFICULTY_COUNT ||
        state->team_selection >= TECMO_ALL_STAR_TEAM_SIDE_COUNT ||
        state->west_owner > 2U || state->east_owner > 2U ||
        state->west_team != asset->west_team_code ||
        state->east_team != asset->east_team_code ||
        state->cursor_delay > asset->cursor_commit_delay_frames ||
        (state->terminal_from_team &&
         (!state->terminal_commit || state->phase != TECMO_ALL_STAR_TEAM)) ||
        (state->terminal_commit && state->phase != TECMO_ALL_STAR_CONTROL &&
         state->phase != TECMO_ALL_STAR_TEAM))
        return false;
    cooldown_limit = asset->repeat_frames > asset->accepted_input_seed
        ? asset->repeat_frames : asset->accepted_input_seed;
    if (state->direction_cooldown > cooldown_limit) return false;
    switch (state->phase) {
    case TECMO_ALL_STAR_CONTROL_SETUP:
    case TECMO_ALL_STAR_CONTROL:
    case TECMO_ALL_STAR_CONTROL_TEARDOWN_ROOT:
        transition_limit = ALL_STAR_CONTROL_HEIGHT;
        break;
    case TECMO_ALL_STAR_DIFFICULTY_SETUP:
    case TECMO_ALL_STAR_DIFFICULTY:
    case TECMO_ALL_STAR_DIFFICULTY_TEARDOWN:
        transition_limit = ALL_STAR_DIFFICULTY_HEIGHT;
        break;
    case TECMO_ALL_STAR_TEAM_SETUP:
    case TECMO_ALL_STAR_TEAM:
    case TECMO_ALL_STAR_TEAM_TEARDOWN:
        transition_limit = ALL_STAR_TEAM_HEIGHT;
        break;
    default:
        return false;
    }
    return state->transition_frame <= transition_limit;
}

static uint8_t controller_byte(const TecmoInput *input)
{
    uint8_t value = 0U;
    if (input->right) value |= ALL_STAR_BUTTON_RIGHT;
    if (input->left) value |= ALL_STAR_BUTTON_LEFT;
    if (input->down) value |= ALL_STAR_BUTTON_DOWN;
    if (input->up) value |= ALL_STAR_BUTTON_UP;
    if (input->confirm) value |= ALL_STAR_BUTTON_START;
    if (input->tab) value |= ALL_STAR_BUTTON_SELECT;
    if (input->cancel) value |= ALL_STAR_BUTTON_B;
    if (input->shoot) value |= ALL_STAR_BUTTON_A;
    return value;
}

static AllStarSelectionAction selection_action(const TecmoAllStarAsset *asset,
                                               const TecmoControlFrame *controls)
{
    uint8_t released;
    if (controller_byte(&controls->held) != 0U) return ALL_STAR_SELECTION_NONE;
    released = controller_byte(&controls->released) & asset->release_input_mask;
    if ((released & ALL_STAR_BUTTON_A) != 0U) return ALL_STAR_SELECTION_ACCEPT;
    if ((released & ALL_STAR_BUTTON_B) != 0U) return ALL_STAR_SELECTION_CANCEL;
    return ALL_STAR_SELECTION_NONE;
}

static bool direction_action(const TecmoControlFrame *controls,
                             uint8_t direction_mask,
                             int *direction)
{
    uint8_t raw = controller_byte(&controls->held) & direction_mask;
    *direction = 0;
    if (raw == 0U) return false;
    if (raw == ALL_STAR_BUTTON_UP || raw == ALL_STAR_BUTTON_LEFT)
        *direction = -1;
    else if (raw == ALL_STAR_BUTTON_DOWN || raw == ALL_STAR_BUTTON_RIGHT)
        *direction = 1;
    return true;
}

static uint8_t wrap_selection(uint8_t selection, uint8_t count, int direction)
{
    if (direction < 0)
        return selection == 0U ? (uint8_t)(count - 1U)
                               : (uint8_t)(selection - 1U);
    return selection + 1U >= count ? 0U : (uint8_t)(selection + 1U);
}

static void tick_cooldown(TecmoAllStarState *state)
{
    if (state->direction_cooldown > 0U) --state->direction_cooldown;
}

static void begin_phase(TecmoAllStarState *state, TecmoAllStarPhase phase)
{
    state->phase = phase;
    state->transition_frame = 0U;
    state->cursor_delay = 0U;
}

static void commit_ownership(TecmoAllStarState *state,
                             const TecmoAllStarAsset *asset,
                             bool use_team_choice)
{
    size_t mode = (size_t)state->control_selection - 1U;
    uint8_t west = asset->ownership[0][mode];
    uint8_t east = asset->ownership[1][mode];
    if (use_team_choice && state->team_selection == 0U) {
        uint8_t swap = west;
        west = east;
        east = swap;
    }
    state->west_owner = west;
    state->east_owner = east;
    state->west_team = asset->west_team_code;
    state->east_team = asset->east_team_code;
    state->terminal_commit = true;
    state->terminal_from_team = use_team_choice;
}

TecmoAllStarAction tecmo_all_star_update(TecmoAllStarState *state,
                                         const TecmoAllStarAsset *asset,
                                         const TecmoControlFrame *controls)
{
    AllStarSelectionAction action;
    uint16_t phase_frames;
    int direction;

    if (controls == NULL || !all_star_state_valid(asset, state))
        return TECMO_ALL_STAR_ACTION_NONE;
    ++state->frame;
    if (state->cursor_delay > 0U) --state->cursor_delay;

    if (state->phase == TECMO_ALL_STAR_CONTROL_SETUP ||
        state->phase == TECMO_ALL_STAR_DIFFICULTY_SETUP ||
        state->phase == TECMO_ALL_STAR_TEAM_SETUP) {
        uint16_t height = state->phase == TECMO_ALL_STAR_CONTROL_SETUP
            ? ALL_STAR_CONTROL_HEIGHT
            : state->phase == TECMO_ALL_STAR_DIFFICULTY_SETUP
                ? ALL_STAR_DIFFICULTY_HEIGHT : ALL_STAR_TEAM_HEIGHT;
        phase_frames = (uint16_t)(height * asset->row_cadence);
        if (state->transition_frame < phase_frames) ++state->transition_frame;
        if (state->transition_frame >= phase_frames) {
            state->phase = state->phase == TECMO_ALL_STAR_CONTROL_SETUP
                ? TECMO_ALL_STAR_CONTROL
                : state->phase == TECMO_ALL_STAR_DIFFICULTY_SETUP
                    ? TECMO_ALL_STAR_DIFFICULTY : TECMO_ALL_STAR_TEAM;
            state->direction_cooldown = asset->accepted_input_seed;
            state->cursor_delay = asset->cursor_commit_delay_frames;
        }
        return TECMO_ALL_STAR_ACTION_NONE;
    }
    if (state->phase == TECMO_ALL_STAR_DIFFICULTY_TEARDOWN ||
        state->phase == TECMO_ALL_STAR_TEAM_TEARDOWN ||
        state->phase == TECMO_ALL_STAR_CONTROL_TEARDOWN_ROOT) {
        uint16_t height = state->phase == TECMO_ALL_STAR_DIFFICULTY_TEARDOWN
            ? ALL_STAR_DIFFICULTY_HEIGHT
            : state->phase == TECMO_ALL_STAR_TEAM_TEARDOWN
                ? ALL_STAR_TEAM_HEIGHT : ALL_STAR_CONTROL_HEIGHT;
        phase_frames = (uint16_t)(height * asset->row_cadence);
        if (state->transition_frame < phase_frames) ++state->transition_frame;
        if (state->transition_frame >= phase_frames) {
            if (state->phase == TECMO_ALL_STAR_CONTROL_TEARDOWN_ROOT)
                return TECMO_ALL_STAR_ACTION_BACK_TO_START_MENU;
            state->phase = TECMO_ALL_STAR_CONTROL;
            state->direction_cooldown = asset->accepted_input_seed;
            state->cursor_delay = asset->cursor_commit_delay_frames;
        }
        return TECMO_ALL_STAR_ACTION_NONE;
    }

    action = selection_action(asset, controls);
    if (state->terminal_commit) {
        if (action == ALL_STAR_SELECTION_CANCEL) {
            state->terminal_commit = false;
            if (state->terminal_from_team) {
                state->terminal_from_team = false;
                begin_phase(state, TECMO_ALL_STAR_TEAM_TEARDOWN);
            } else {
                state->direction_cooldown = asset->accepted_input_seed;
                state->cursor_delay = asset->cursor_commit_delay_frames;
            }
        }
        return TECMO_ALL_STAR_ACTION_NONE;
    }

    if (state->phase == TECMO_ALL_STAR_CONTROL) {
        if (action == ALL_STAR_SELECTION_CANCEL) {
            state->direction_cooldown = asset->accepted_input_seed;
            begin_phase(state, TECMO_ALL_STAR_CONTROL_TEARDOWN_ROOT);
            return TECMO_ALL_STAR_ACTION_NONE;
        }
        if (action == ALL_STAR_SELECTION_ACCEPT) {
            state->direction_cooldown = asset->accepted_input_seed;
            if (state->control_selection == 0U) {
                state->difficulty_selection = state->committed_difficulty;
                begin_phase(state, TECMO_ALL_STAR_DIFFICULTY_SETUP);
            } else if (state->control_selection == asset->team_mode_rows[0] ||
                       state->control_selection == asset->team_mode_rows[1]) {
                state->team_selection = 0U;
                begin_phase(state, TECMO_ALL_STAR_TEAM_SETUP);
            } else {
                commit_ownership(state, asset, false);
            }
            return TECMO_ALL_STAR_ACTION_NONE;
        }
        if (direction_action(controls, asset->direction_input_mask,
                             &direction) && state->direction_cooldown == 0U) {
            if (direction != 0)
                state->control_selection = wrap_selection(
                    state->control_selection, TECMO_ALL_STAR_CONTROL_COUNT,
                    direction);
            state->direction_cooldown = asset->repeat_frames;
        }
        tick_cooldown(state);
        return TECMO_ALL_STAR_ACTION_NONE;
    }

    if (state->phase == TECMO_ALL_STAR_DIFFICULTY) {
        if (action != ALL_STAR_SELECTION_NONE) {
            if (action == ALL_STAR_SELECTION_ACCEPT) {
                state->committed_difficulty = state->difficulty_selection;
                state->control_selection = 1U;
            } else {
                state->difficulty_selection = state->committed_difficulty;
            }
            state->direction_cooldown = asset->accepted_input_seed;
            begin_phase(state, TECMO_ALL_STAR_DIFFICULTY_TEARDOWN);
            return TECMO_ALL_STAR_ACTION_NONE;
        }
        if (direction_action(controls, asset->direction_input_mask,
                             &direction) && state->direction_cooldown == 0U) {
            if (direction != 0)
                state->difficulty_selection = wrap_selection(
                    state->difficulty_selection,
                    TECMO_ALL_STAR_DIFFICULTY_COUNT, direction);
            state->direction_cooldown = asset->repeat_frames;
        }
        tick_cooldown(state);
        return TECMO_ALL_STAR_ACTION_NONE;
    }

    if (state->phase == TECMO_ALL_STAR_TEAM) {
        if (action == ALL_STAR_SELECTION_CANCEL) {
            state->direction_cooldown = asset->accepted_input_seed;
            begin_phase(state, TECMO_ALL_STAR_TEAM_TEARDOWN);
            return TECMO_ALL_STAR_ACTION_NONE;
        }
        if (action == ALL_STAR_SELECTION_ACCEPT) {
            commit_ownership(state, asset, true);
            state->direction_cooldown = asset->accepted_input_seed;
            return TECMO_ALL_STAR_ACTION_NONE;
        }
        if (direction_action(controls, asset->direction_input_mask,
                             &direction) && state->direction_cooldown == 0U) {
            if (direction != 0)
                state->team_selection = wrap_selection(
                    state->team_selection, TECMO_ALL_STAR_TEAM_SIDE_COUNT,
                    direction);
            state->direction_cooldown = asset->repeat_frames;
        }
        tick_cooldown(state);
    }
    return TECMO_ALL_STAR_ACTION_NONE;
}

unsigned tecmo_all_star_overlay_visible_rows(const TecmoAllStarAsset *asset,
                                             const TecmoAllStarState *state,
                                             size_t overlay_index)
{
    unsigned height;
    unsigned elapsed;
    if (asset == NULL || state == NULL || !asset->available || overlay_index > 2U)
        return 0U;
    height = overlay_index == 0U ? ALL_STAR_CONTROL_HEIGHT
           : overlay_index == 1U ? ALL_STAR_DIFFICULTY_HEIGHT
                                 : ALL_STAR_TEAM_HEIGHT;
    elapsed = asset->row_cadence == 0U ? 0U
        : state->transition_frame / asset->row_cadence;
    if (elapsed > height) elapsed = height;
    if (overlay_index == 0U) {
        if (state->phase == TECMO_ALL_STAR_CONTROL_SETUP)
            return elapsed < height ? elapsed + 1U : height;
        if (state->phase == TECMO_ALL_STAR_CONTROL_TEARDOWN_ROOT)
            return height - elapsed;
        return height;
    }
    if (overlay_index == 1U) {
        if (state->phase == TECMO_ALL_STAR_DIFFICULTY_SETUP)
            return elapsed < height ? elapsed + 1U : height;
        if (state->phase == TECMO_ALL_STAR_DIFFICULTY_TEARDOWN)
            return height - elapsed;
        return state->phase == TECMO_ALL_STAR_DIFFICULTY ? height : 0U;
    }
    if (state->phase == TECMO_ALL_STAR_TEAM_SETUP)
        return elapsed < height ? elapsed + 1U : height;
    if (state->phase == TECMO_ALL_STAR_TEAM_TEARDOWN)
        return height - elapsed;
    return state->phase == TECMO_ALL_STAR_TEAM ? height : 0U;
}

const char *tecmo_all_star_phase_name(TecmoAllStarPhase phase)
{
    switch (phase) {
    case TECMO_ALL_STAR_CONTROL_SETUP: return "CONTROL SETUP";
    case TECMO_ALL_STAR_CONTROL: return "CONTROL";
    case TECMO_ALL_STAR_DIFFICULTY_SETUP: return "DIFFICULTY SETUP";
    case TECMO_ALL_STAR_DIFFICULTY: return "DIFFICULTY";
    case TECMO_ALL_STAR_DIFFICULTY_TEARDOWN: return "DIFFICULTY TEARDOWN";
    case TECMO_ALL_STAR_TEAM_SETUP: return "TEAM SETUP";
    case TECMO_ALL_STAR_TEAM: return "TEAM";
    case TECMO_ALL_STAR_TEAM_TEARDOWN: return "TEAM TEARDOWN";
    case TECMO_ALL_STAR_CONTROL_TEARDOWN_ROOT: return "CONTROL TEARDOWN";
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
                         const TecmoStartGameMenuOverlay *overlay,
                         const TecmoStartGameMenuCell *cells,
                         unsigned visible_rows,
                         const uint8_t palette[16],
                         const uint8_t *chr_bytes,
                         uint64_t chr_byte_count,
                         int scale)
{
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
        draw_cell(view, &cells[overlay->cell_start + i], palette,
                  chr_bytes, chr_byte_count,
                  ((int)overlay->x + (int)col) * 8 * scale,
                  ((int)overlay->y + (int)row) * 8 * scale, scale);
    }
}

static bool draw_start_base(TecmoFramebuffer *framebuffer,
                            const TecmoStartGameMenuAsset *start_asset,
                            const uint8_t *chr_bytes,
                            uint64_t chr_byte_count,
                            int origin_x,
                            int origin_y,
                            int scale)
{
    TecmoStartGameMenuState base_state;
    memset(&base_state, 0, sizeof(base_state));
    base_state.frame = start_asset->stable_frame;
    base_state.phase = TECMO_START_GAME_MENU_ROOT;
    base_state.cursor_delay = 1U;
    return tecmo_start_game_menu_draw(framebuffer, start_asset, &base_state,
                                      NULL, chr_bytes, chr_byte_count,
                                      origin_x, origin_y, scale);
}

bool tecmo_all_star_draw(TecmoFramebuffer *framebuffer,
                         const TecmoAllStarAsset *asset,
                         const TecmoAllStarState *state,
                         const TecmoPreseasonAsset *preseason_asset,
                         const TecmoStartGameMenuAsset *start_asset,
                         const uint8_t *chr_bytes,
                         uint64_t chr_byte_count,
                         int origin_x,
                         int origin_y,
                         int scale)
{
    TecmoFramebuffer view;
    unsigned control_rows;
    unsigned difficulty_rows;
    unsigned team_rows;
    const uint8_t *bg_palette;
    const uint8_t *sprite_palette;

    if (preseason_asset == NULL || !preseason_asset->available ||
        start_asset == NULL || !start_asset->available ||
        !all_star_state_valid(asset, state) ||
        asset->expected_preseason_size != ALL_STAR_TPRE_SIZE ||
        asset->expected_preseason_fingerprint32 != ALL_STAR_TPRE_FNV1A32 ||
        preseason_asset->expected_start_menu_size !=
            asset->expected_start_menu_size ||
        preseason_asset->expected_start_menu_fingerprint32 !=
            asset->expected_start_menu_fingerprint32 ||
        preseason_asset->expected_chr_size != asset->expected_chr_size ||
        preseason_asset->expected_chr_fingerprint32 !=
            asset->expected_chr_fingerprint32 ||
        !tecmo_all_star_asset_chr_available(asset, chr_bytes, chr_byte_count) ||
        !tecmo_preseason_asset_chr_available(preseason_asset, chr_bytes,
                                             chr_byte_count) ||
        !tecmo_start_game_menu_asset_chr_available(start_asset, chr_bytes,
                                                   chr_byte_count) ||
        !make_viewport(&view, framebuffer, origin_x, origin_y, scale))
        return false;
    if (preseason_asset->overlays[0].height != ALL_STAR_CONTROL_HEIGHT ||
        preseason_asset->overlays[2].height != ALL_STAR_DIFFICULTY_HEIGHT)
        return false;
    if (!draw_start_base(framebuffer, start_asset, chr_bytes, chr_byte_count,
                         origin_x, origin_y, scale))
        return false;

    bg_palette = start_asset->palettes[8U];
    sprite_palette = bg_palette + 16U;
    control_rows = tecmo_all_star_overlay_visible_rows(asset, state, 0U);
    difficulty_rows = tecmo_all_star_overlay_visible_rows(asset, state, 1U);
    team_rows = tecmo_all_star_overlay_visible_rows(asset, state, 2U);
    if (control_rows > 0U)
        draw_overlay(&view, &preseason_asset->overlays[0],
                     preseason_asset->overlay_cells, control_rows,
                     bg_palette, chr_bytes, chr_byte_count, scale);
    if (difficulty_rows > 0U)
        draw_overlay(&view, &preseason_asset->overlays[2],
                     preseason_asset->overlay_cells, difficulty_rows,
                     bg_palette, chr_bytes, chr_byte_count, scale);
    if (team_rows > 0U)
        draw_overlay(&view, &asset->team_overlay, asset->team_cells,
                     team_rows, bg_palette, chr_bytes, chr_byte_count, scale);

    if (state->cursor_delay == 0U) {
        size_t cursor = state->phase == TECMO_ALL_STAR_CONTROL ? 0U
                      : state->phase == TECMO_ALL_STAR_DIFFICULTY ? 1U
                      : state->phase == TECMO_ALL_STAR_TEAM ? 2U : 3U;
        if (cursor < 3U) {
            uint8_t selection = cursor == 0U ? state->control_selection
                              : cursor == 1U ? state->difficulty_selection
                                             : state->team_selection;
            draw_sprite_piece(&view, &start_asset->cursor, sprite_palette,
                              chr_bytes, chr_byte_count,
                              asset->cursor_x[cursor],
                              asset->cursor_y[cursor] +
                                  selection * asset->cursor_stride[cursor],
                              scale);
        }
    }
    return true;
}
