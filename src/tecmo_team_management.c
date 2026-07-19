#include "tecmo_team_management.h"

#include "tecmo_asset_pack.h"
#include "tecmo_nes_video.h"
#include "tecmo_team_data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define TEAM_MANAGEMENT_ENTRY_ID "menu/team-management"
#define TEAM_MANAGEMENT_PAYLOAD_SIZE 21061U
#define TEAM_MANAGEMENT_PAYLOAD_FNV1A32 0xD192EAC6U
#define TEAM_MANAGEMENT_TTDT_SIZE 96372U
#define TEAM_MANAGEMENT_TTDT_FNV1A32 0x812628F0U
#define TEAM_MANAGEMENT_CHR_SIZE 262144U
#define TEAM_MANAGEMENT_CHR_FNV1A32 0xF6F6E854U
#define TEAM_MANAGEMENT_CHR_FNV1A64 0x96A64F53B240ABB4ULL
#define TEAM_MANAGEMENT_STARTERS_OFFSET 256U
#define TEAM_MANAGEMENT_PLAYBOOK_OFFSET 6016U
#define TEAM_MANAGEMENT_PALETTES_OFFSET 17536U
#define TEAM_MANAGEMENT_DIAGRAMS_OFFSET 17568U
#define TEAM_MANAGEMENT_MARKER_OFFSET 20640U
#define TEAM_MANAGEMENT_NAMES_OFFSET 20656U
#define TEAM_MANAGEMENT_DEFAULTS_OFFSET 20800U

typedef enum TeamManagementButton {
    TEAM_MANAGEMENT_RIGHT = 0x01,
    TEAM_MANAGEMENT_LEFT = 0x02,
    TEAM_MANAGEMENT_DOWN = 0x04,
    TEAM_MANAGEMENT_UP = 0x08,
    TEAM_MANAGEMENT_START = 0x10,
    TEAM_MANAGEMENT_SELECT = 0x20,
    TEAM_MANAGEMENT_B = 0x40,
    TEAM_MANAGEMENT_A = 0x80
} TeamManagementButton;

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

static uint32_t bg_chr_offset(uint8_t tile)
{
    return 0xFAU * 1024U + (uint32_t)(tile & 0x7FU) * 16U;
}

static int make_path(char *path, size_t size, const char *root,
                     const char *suffix)
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

static bool read_dependencies(const char *root,
                              uint8_t **payload,
                              uint64_t *payload_count,
                              uint8_t **team_data,
                              uint64_t *team_data_count,
                              uint8_t **chr,
                              uint64_t *chr_count)
{
    const char *env = getenv("TECMO_ASSETPACK");
    const char *paths[4];
    char root_build[1024];
    char root_pack[1024];
    size_t path_count = 0U;
    if (env != NULL && env[0] != '\0') paths[path_count++] = env;
    else {
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
        if (tecmo_asset_pack_read_entry_exact(
                paths[i], TEAM_MANAGEMENT_ENTRY_ID,
                TEAM_MANAGEMENT_PAYLOAD_SIZE, payload, payload_count) != 0)
            return false;
        if (tecmo_asset_pack_read_entry_exact(
                paths[i], "menu/team-data", TEAM_MANAGEMENT_TTDT_SIZE,
                team_data, team_data_count) != 0 ||
            tecmo_asset_pack_read_entry_exact(
                paths[i], "chr/all", TEAM_MANAGEMENT_CHR_SIZE,
                chr, chr_count) != 0) {
            tecmo_asset_pack_free(*payload);
            tecmo_asset_pack_free(*team_data);
            tecmo_asset_pack_free(*chr);
            *payload = NULL;
            *team_data = NULL;
            *chr = NULL;
            *payload_count = 0U;
            *team_data_count = 0U;
            *chr_count = 0U;
            return false;
        }
        return true;
    }
    return false;
}

static bool parse_cell(TecmoStartGameMenuCell *cell, const uint8_t *source)
{
    cell->tile_id = source[0];
    cell->palette_index = source[1];
    cell->chr_offset = read_u32(source + 2U);
    return cell->palette_index <= 3U &&
           cell->chr_offset == bg_chr_offset(cell->tile_id);
}

static bool valid_play_name(const char name[18])
{
    bool ampersand = false;
    bool letter = false;
    if (name[17] != '\0') return false;
    for (size_t i = 0U; i < 17U; ++i) {
        char c = name[i];
        if (c >= 'A' && c <= 'Z') letter = true;
        else if (c == '&') {
            if (ampersand) return false;
            ampersand = true;
        } else if (c != ' ') return false;
    }
    return letter && ampersand;
}

static bool parse_payload(TecmoTeamManagementAsset *asset,
                          const uint8_t *bytes,
                          uint64_t count)
{
    if (bytes == NULL || count != TEAM_MANAGEMENT_PAYLOAD_SIZE ||
        fnv1a32(bytes, count) != TEAM_MANAGEMENT_PAYLOAD_FNV1A32 ||
        memcmp(bytes, "TTMG", 4U) != 0 || read_u16(bytes + 4U) != 1U ||
        read_u16(bytes + 6U) != 256U || read_u16(bytes + 8U) != 32U ||
        read_u16(bytes + 10U) != 30U || read_u16(bytes + 12U) != 1U ||
        read_u16(bytes + 14U) != 2U || read_u16(bytes + 16U) != 29U ||
        read_u16(bytes + 18U) != 12U ||
        read_u32(bytes + 20U) != TEAM_MANAGEMENT_STARTERS_OFFSET ||
        read_u32(bytes + 24U) != TEAM_MANAGEMENT_PLAYBOOK_OFFSET ||
        read_u32(bytes + 28U) != TEAM_MANAGEMENT_PALETTES_OFFSET ||
        read_u32(bytes + 32U) != TEAM_MANAGEMENT_DIAGRAMS_OFFSET ||
        read_u32(bytes + 36U) != TEAM_MANAGEMENT_MARKER_OFFSET ||
        read_u32(bytes + 40U) != TEAM_MANAGEMENT_NAMES_OFFSET ||
        read_u32(bytes + 44U) != TEAM_MANAGEMENT_DEFAULTS_OFFSET ||
        read_u32(bytes + 48U) != TEAM_MANAGEMENT_PAYLOAD_SIZE ||
        read_u32(bytes + 52U) != TEAM_MANAGEMENT_CHR_SIZE ||
        read_u32(bytes + 56U) != TEAM_MANAGEMENT_CHR_FNV1A32 ||
        read_u32(bytes + 60U) != TEAM_MANAGEMENT_TTDT_SIZE ||
        read_u32(bytes + 64U) != TEAM_MANAGEMENT_TTDT_FNV1A32 ||
        bytes[68U] != 8U || bytes[69U] != 8U || bytes[70U] != 8U ||
        bytes[71U] != 8U || bytes[72U] != 5U || bytes[73U] != 4U ||
        bytes[74U] != 6U || bytes[75U] != 7U || bytes[76U] != 8U ||
        bytes[77U] != 8U || bytes[78U] != 8U || bytes[79U] != 1U ||
        bytes[80U] != 1U || bytes[81U] != 2U || bytes[82U] != 15U ||
        bytes[83U] != 16U ||
        memcmp(bytes + 84U, "\xFA\xFA\xFA\xFA", 4U) != 0 ||
        bytes[88U] != 40U || bytes[89U] != 44U || bytes[90U] != 8U ||
        bytes[91U] != 40U || bytes[92U] != 64U || bytes[93U] != 136U ||
        bytes[94U] != 144U || bytes[95U] != 64U || bytes[96U] != 160U)
        return false;
    for (size_t i = 97U; i < 256U; ++i)
        if (bytes[i] != 0U) return false;

    for (size_t i = 0U; i < TECMO_TEAM_MANAGEMENT_SCREEN_CELLS; ++i)
        if (!parse_cell(&asset->starters_screen[i],
                        bytes + TEAM_MANAGEMENT_STARTERS_OFFSET + i * 6U))
            return false;
    for (size_t page = 0U; page < 2U; ++page)
        for (size_t i = 0U; i < TECMO_TEAM_MANAGEMENT_SCREEN_CELLS; ++i)
            if (!parse_cell(&asset->playbook_screens[page][i],
                            bytes + TEAM_MANAGEMENT_PLAYBOOK_OFFSET +
                                (page * TECMO_TEAM_MANAGEMENT_SCREEN_CELLS + i) * 6U))
                return false;
    for (size_t i = 0U; i < 32U; ++i) {
        uint8_t color = bytes[TEAM_MANAGEMENT_PALETTES_OFFSET + i];
        if (color > 0x3FU) return false;
        asset->palettes[i / 16U][i % 16U] = color;
    }
    for (size_t play = 0U; play < TECMO_TEAM_MANAGEMENT_PLAY_COUNT; ++play)
        for (size_t i = 0U; i < TECMO_TEAM_MANAGEMENT_DIAGRAM_CELLS; ++i)
            if (!parse_cell(&asset->diagrams[play][i],
                            bytes + TEAM_MANAGEMENT_DIAGRAMS_OFFSET +
                                (play * TECMO_TEAM_MANAGEMENT_DIAGRAM_CELLS + i) * 6U))
                return false;
    memcpy(asset->marker, bytes + TEAM_MANAGEMENT_MARKER_OFFSET,
           sizeof(asset->marker));
    if (asset->marker[10U] != 0x7DU || asset->marker[11U] != 0x10U)
        return false;
    for (size_t i = 0U; i < 10U; ++i)
        if ((uint64_t)bg_chr_offset(asset->marker[i]) + 16U >
            TEAM_MANAGEMENT_CHR_SIZE)
            return false;
    for (size_t play = 0U; play < TECMO_TEAM_MANAGEMENT_PLAY_COUNT; ++play) {
        memcpy(asset->play_names[play],
               bytes + TEAM_MANAGEMENT_NAMES_OFFSET + play * 18U, 18U);
        if (!valid_play_name(asset->play_names[play])) return false;
    }
    for (size_t team = 0U; team < TECMO_TEAM_MANAGEMENT_TEAM_COUNT; ++team) {
        const uint8_t *source = bytes + TEAM_MANAGEMENT_DEFAULTS_OFFSET + team * 9U;
        bool players[TECMO_TEAM_MANAGEMENT_PLAYER_COUNT] = {false};
        bool plays[TECMO_TEAM_MANAGEMENT_PLAY_COUNT] = {false};
        for (size_t i = 0U; i < TECMO_TEAM_MANAGEMENT_STARTER_COUNT; ++i) {
            uint8_t value = source[i];
            if (value >= TECMO_TEAM_MANAGEMENT_PLAYER_COUNT || players[value])
                return false;
            players[value] = true;
            asset->default_starters[team][i] = value;
        }
        for (size_t i = 0U; i < TECMO_TEAM_MANAGEMENT_PLAYBOOK_SLOT_COUNT; ++i) {
            uint8_t value = source[5U + i];
            if (value >= TECMO_TEAM_MANAGEMENT_PLAY_COUNT || plays[value])
                return false;
            plays[value] = true;
            asset->default_playbooks[team][i] = value;
        }
    }
    asset->starter_selector_count = bytes[74U];
    asset->bench_selector_count = bytes[75U];
    asset->held_repeat_frames = bytes[76U];
    asset->carousel_frames = bytes[77U];
    asset->carousel_pixels_per_frame = bytes[78U];
    asset->starters_cursor_x = bytes[88U];
    asset->starters_cursor_y = bytes[89U];
    asset->starters_cursor_stride = bytes[90U];
    asset->playbook_cursor_y = bytes[91U];
    asset->expected_chr_size = read_u32(bytes + 52U);
    asset->expected_chr_fingerprint32 = read_u32(bytes + 56U);
    asset->chr_fingerprint64 = TEAM_MANAGEMENT_CHR_FNV1A64;
    return true;
}

bool tecmo_team_management_asset_load(TecmoTeamManagementAsset *asset,
                                      const char *project_root)
{
    uint8_t *payload = NULL;
    uint8_t *team_data = NULL;
    uint8_t *chr = NULL;
    uint64_t payload_count = 0U;
    uint64_t team_data_count = 0U;
    uint64_t chr_count = 0U;
    bool valid;
    if (asset == NULL) return false;
    memset(asset, 0, sizeof(*asset));
    if (!read_dependencies(project_root, &payload, &payload_count,
                           &team_data, &team_data_count, &chr, &chr_count)) {
        (void)snprintf(asset->status, sizeof(asset->status),
                       "TTMG-1 or same-pack dependency missing");
        return false;
    }
    valid = team_data_count == TEAM_MANAGEMENT_TTDT_SIZE &&
            fnv1a32(team_data, team_data_count) == TEAM_MANAGEMENT_TTDT_FNV1A32 &&
            chr_count == TEAM_MANAGEMENT_CHR_SIZE &&
            fnv1a32(chr, chr_count) == TEAM_MANAGEMENT_CHR_FNV1A32 &&
            fnv1a64(chr, chr_count) == TEAM_MANAGEMENT_CHR_FNV1A64 &&
            parse_payload(asset, payload, payload_count);
    tecmo_asset_pack_free(payload);
    tecmo_asset_pack_free(team_data);
    tecmo_asset_pack_free(chr);
    if (!valid) {
        memset(asset, 0, sizeof(*asset));
        (void)snprintf(asset->status, sizeof(asset->status),
                       "TTMG-1 payload/dependency contract rejected");
        return false;
    }
    asset->available = true;
    (void)snprintf(asset->status, sizeof(asset->status),
                   "TTMG-1 ROM-only management ready");
    return true;
}

bool tecmo_team_management_asset_chr_available(
    const TecmoTeamManagementAsset *asset,
    const uint8_t *chr_bytes,
    uint64_t chr_byte_count)
{
    if (asset == NULL || !asset->available || chr_bytes == NULL ||
        chr_byte_count != asset->expected_chr_size ||
        fnv1a32(chr_bytes, chr_byte_count) != asset->expected_chr_fingerprint32 ||
        fnv1a64(chr_bytes, chr_byte_count) != asset->chr_fingerprint64)
        return false;
    return true;
}

bool tecmo_team_management_session_valid(
    const TecmoTeamManagementSession *session)
{
    if (session == NULL || !session->initialized) return false;
    for (size_t team = 0U; team < TECMO_TEAM_MANAGEMENT_TEAM_COUNT; ++team) {
        bool players[TECMO_TEAM_MANAGEMENT_PLAYER_COUNT] = {false};
        bool plays[TECMO_TEAM_MANAGEMENT_PLAY_COUNT] = {false};
        for (size_t i = 0U; i < TECMO_TEAM_MANAGEMENT_STARTER_COUNT; ++i) {
            uint8_t value = session->starters[team][i];
            if (value >= TECMO_TEAM_MANAGEMENT_PLAYER_COUNT || players[value])
                return false;
            players[value] = true;
        }
        for (size_t i = 0U; i < TECMO_TEAM_MANAGEMENT_PLAYBOOK_SLOT_COUNT; ++i) {
            uint8_t value = session->playbooks[team][i];
            if (value >= TECMO_TEAM_MANAGEMENT_PLAY_COUNT || plays[value])
                return false;
            plays[value] = true;
        }
    }
    return true;
}

bool tecmo_team_management_session_init(
    TecmoTeamManagementSession *session,
    const TecmoTeamManagementAsset *asset)
{
    if (session == NULL) return false;
    memset(session, 0, sizeof(*session));
    if (asset == NULL || !asset->available) return false;
    memcpy(session->starters, asset->default_starters,
           sizeof(session->starters));
    memcpy(session->playbooks, asset->default_playbooks,
           sizeof(session->playbooks));
    session->initialized = true;
    return tecmo_team_management_session_valid(session);
}

void tecmo_team_management_view_init_starters(
    TecmoTeamManagementViewState *state)
{
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->view = TECMO_TEAM_MANAGEMENT_VIEW_STARTERS;
}

void tecmo_team_management_view_init_playbook(
    TecmoTeamManagementViewState *state)
{
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->view = TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK;
}

static uint8_t controller_byte(const TecmoInput *input)
{
    uint8_t value = 0U;
    if (input->right) value |= TEAM_MANAGEMENT_RIGHT;
    if (input->left) value |= TEAM_MANAGEMENT_LEFT;
    if (input->down) value |= TEAM_MANAGEMENT_DOWN;
    if (input->up) value |= TEAM_MANAGEMENT_UP;
    if (input->confirm) value |= TEAM_MANAGEMENT_START;
    if (input->tab) value |= TEAM_MANAGEMENT_SELECT;
    if (input->cancel) value |= TEAM_MANAGEMENT_B;
    if (input->shoot) value |= TEAM_MANAGEMENT_A;
    return value;
}

static uint8_t released_button(const TecmoControlFrame *controls)
{
    if (controller_byte(&controls->held) != 0U) return 0U;
    return controller_byte(&controls->released);
}

static uint8_t wrap(uint8_t value, uint8_t count, int direction)
{
    if (direction < 0) return value == 0U ? (uint8_t)(count - 1U)
                                          : (uint8_t)(value - 1U);
    return value + 1U >= count ? 0U : (uint8_t)(value + 1U);
}

static size_t bench_candidates(uint8_t destination[7],
                               const TecmoTeamManagementSession *session,
                               const TecmoTeamDataAsset *team_data,
                               uint8_t team_id)
{
    size_t count = 0U;
    for (uint8_t player = 0U; player < TECMO_TEAM_MANAGEMENT_PLAYER_COUNT;
         ++player) {
        bool starter = false;
        for (size_t i = 0U; i < TECMO_TEAM_MANAGEMENT_STARTER_COUNT; ++i)
            if (session->starters[team_id][i] == player) starter = true;
        if (!starter &&
            team_data->players[team_id][player].condition_seed != 0U &&
            count < 7U)
            destination[count++] = player;
    }
    return count;
}

static size_t unused_plays(uint8_t destination[4],
                           const TecmoTeamManagementSession *session,
                           uint8_t team_id)
{
    size_t count = 0U;
    for (int play = 7; play >= 0; --play) {
        bool used = false;
        for (size_t i = 0U; i < TECMO_TEAM_MANAGEMENT_PLAYBOOK_SLOT_COUNT; ++i)
            if (session->playbooks[team_id][i] == (uint8_t)play) used = true;
        if (!used && count < 4U) destination[count++] = (uint8_t)play;
    }
    return count;
}

static void restore_default_starters(TecmoTeamManagementSession *session,
                                     const TecmoTeamManagementAsset *asset,
                                     uint8_t team_id)
{
    memcpy(session->starters[team_id], asset->default_starters[team_id],
           TECMO_TEAM_MANAGEMENT_STARTER_COUNT);
}

static void restore_default_playbook(TecmoTeamManagementSession *session,
                                     const TecmoTeamManagementAsset *asset,
                                     uint8_t team_id)
{
    memcpy(session->playbooks[team_id], asset->default_playbooks[team_id],
           TECMO_TEAM_MANAGEMENT_PLAYBOOK_SLOT_COUNT);
}

static bool view_state_valid(const TecmoTeamManagementAsset *asset,
                             const TecmoTeamManagementViewState *state)
{
    if (asset == NULL || state == NULL ||
        state->view < TECMO_TEAM_MANAGEMENT_VIEW_STARTERS ||
        state->view > TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK_RESET ||
        state->secondary_selection >= TECMO_TEAM_MANAGEMENT_BENCH_COUNT ||
        state->carousel_origin >= TECMO_TEAM_MANAGEMENT_PLAYBOOK_SLOT_COUNT ||
        state->carousel_pending_origin >=
            TECMO_TEAM_MANAGEMENT_PLAYBOOK_SLOT_COUNT ||
        state->carousel_frame > asset->carousel_frames ||
        state->carousel_direction < -1 || state->carousel_direction > 1 ||
        state->direction_cooldown > asset->held_repeat_frames ||
        (state->carousel_direction != 0 &&
         state->view != TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK_REPLACE))
        return false;
    switch (state->view) {
    case TECMO_TEAM_MANAGEMENT_VIEW_STARTERS:
        return state->selection < asset->starter_selector_count;
    case TECMO_TEAM_MANAGEMENT_VIEW_STARTER_RESET:
        return state->selection == 0U && state->secondary_selection < 2U;
    case TECMO_TEAM_MANAGEMENT_VIEW_STARTER_BENCH:
        return state->selection > 0U &&
               state->selection < asset->starter_selector_count &&
               state->secondary_selection < asset->bench_selector_count;
    case TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK:
    case TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK_REPLACE:
    case TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK_RESET:
        return state->selection < TECMO_TEAM_MANAGEMENT_PLAYBOOK_SLOT_COUNT;
    default:
        return false;
    }
}

TecmoTeamManagementAction tecmo_team_management_update(
    TecmoTeamManagementViewState *state,
    TecmoTeamManagementSession *session,
    const TecmoTeamManagementAsset *asset,
    const TecmoTeamDataAsset *team_data,
    uint8_t team_id,
    const TecmoControlFrame *controls,
    uint8_t *player_detail_index)
{
    uint8_t released;
    if (state == NULL || !tecmo_team_management_session_valid(session) ||
        asset == NULL || !asset->available || team_data == NULL ||
        !team_data->available || team_id >= TECMO_TEAM_MANAGEMENT_TEAM_COUNT ||
        controls == NULL || !view_state_valid(asset, state))
        return TECMO_TEAM_MANAGEMENT_ACTION_NONE;

    if (state->carousel_direction != 0) {
        if (state->carousel_frame < asset->carousel_frames)
            ++state->carousel_frame;
        if (state->carousel_frame >= asset->carousel_frames) {
            state->carousel_origin = state->carousel_pending_origin;
            state->carousel_direction = 0;
        }
        return TECMO_TEAM_MANAGEMENT_ACTION_NONE;
    }
    released = released_button(controls);
    if (state->view == TECMO_TEAM_MANAGEMENT_VIEW_STARTERS) {
        uint8_t held = controller_byte(&controls->held) &
                       (TEAM_MANAGEMENT_UP | TEAM_MANAGEMENT_DOWN);
        if ((released & TEAM_MANAGEMENT_B) != 0U)
            return TECMO_TEAM_MANAGEMENT_ACTION_BACK_TO_PROFILE;
        if ((released & TEAM_MANAGEMENT_START) != 0U && state->selection > 0U) {
            if (player_detail_index != NULL)
                *player_detail_index = session->starters[team_id]
                    [state->selection - 1U];
            return TECMO_TEAM_MANAGEMENT_ACTION_PLAYER_DETAIL;
        }
        if ((released & TEAM_MANAGEMENT_A) != 0U) {
            if (state->selection == 0U) {
                state->view = TECMO_TEAM_MANAGEMENT_VIEW_STARTER_RESET;
                state->secondary_selection = 0U;
            } else {
                state->view = TECMO_TEAM_MANAGEMENT_VIEW_STARTER_BENCH;
                state->secondary_selection = 0U;
            }
            return TECMO_TEAM_MANAGEMENT_ACTION_NONE;
        }
        if (state->direction_cooldown == 0U &&
            (held == TEAM_MANAGEMENT_UP || held == TEAM_MANAGEMENT_DOWN)) {
            state->selection = wrap(state->selection, 6U,
                held == TEAM_MANAGEMENT_UP ? -1 : 1);
            state->direction_cooldown = asset->held_repeat_frames;
        }
        if (state->direction_cooldown > 0U) --state->direction_cooldown;
    } else if (state->view == TECMO_TEAM_MANAGEMENT_VIEW_STARTER_RESET) {
        if ((released & TEAM_MANAGEMENT_B) != 0U) {
            state->view = TECMO_TEAM_MANAGEMENT_VIEW_STARTERS;
        } else if ((released & TEAM_MANAGEMENT_A) != 0U) {
            if (state->secondary_selection == 1U)
                restore_default_starters(session, asset, team_id);
            state->view = TECMO_TEAM_MANAGEMENT_VIEW_STARTERS;
        } else if ((released & (TEAM_MANAGEMENT_UP | TEAM_MANAGEMENT_DOWN)) != 0U) {
            state->secondary_selection ^= 1U;
        }
    } else if (state->view == TECMO_TEAM_MANAGEMENT_VIEW_STARTER_BENCH) {
        uint8_t candidates[7];
        size_t count = bench_candidates(candidates, session, team_data, team_id);
        if ((released & TEAM_MANAGEMENT_B) != 0U) {
            state->view = TECMO_TEAM_MANAGEMENT_VIEW_STARTERS;
        } else if ((released & TEAM_MANAGEMENT_A) != 0U && count == 7U) {
            session->starters[team_id][state->selection - 1U] =
                candidates[state->secondary_selection];
            state->view = TECMO_TEAM_MANAGEMENT_VIEW_STARTERS;
        } else {
            uint8_t held = controller_byte(&controls->held) &
                           (TEAM_MANAGEMENT_UP | TEAM_MANAGEMENT_DOWN);
            if (state->direction_cooldown == 0U && count == 7U &&
                (held == TEAM_MANAGEMENT_UP || held == TEAM_MANAGEMENT_DOWN)) {
                state->secondary_selection = wrap(
                    state->secondary_selection, 7U,
                    held == TEAM_MANAGEMENT_UP ? -1 : 1);
                state->direction_cooldown = asset->held_repeat_frames;
            }
            if (state->direction_cooldown > 0U) --state->direction_cooldown;
        }
    } else if (state->view == TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK) {
        if ((released & TEAM_MANAGEMENT_B) != 0U)
            return TECMO_TEAM_MANAGEMENT_ACTION_BACK_TO_PROFILE;
        if ((released & TEAM_MANAGEMENT_LEFT) != 0U)
            state->selection = wrap(state->selection, 4U, -1);
        else if ((released & TEAM_MANAGEMENT_RIGHT) != 0U)
            state->selection = wrap(state->selection, 4U, 1);
        else if ((released & TEAM_MANAGEMENT_A) != 0U) {
            state->view = TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK_REPLACE;
            state->carousel_origin = 0U;
            state->carousel_pending_origin = 0U;
            state->carousel_frame = 0U;
        } else if ((released & TEAM_MANAGEMENT_UP) != 0U) {
            state->view = TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK_RESET;
        }
    } else if (state->view == TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK_REPLACE) {
        uint8_t unused[4];
        if (unused_plays(unused, session, team_id) != 4U)
            return TECMO_TEAM_MANAGEMENT_ACTION_NONE;
        if ((released & TEAM_MANAGEMENT_B) != 0U) {
            state->view = TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK;
        } else if ((released & TEAM_MANAGEMENT_A) != 0U) {
            size_t centered = (state->carousel_origin + 2U) % 4U;
            session->playbooks[team_id][state->selection] = unused[centered];
            state->view = TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK;
        } else if ((released & TEAM_MANAGEMENT_LEFT) != 0U ||
                   (released & TEAM_MANAGEMENT_RIGHT) != 0U) {
            state->carousel_direction =
                (released & TEAM_MANAGEMENT_LEFT) != 0U ? -1 : 1;
            state->carousel_pending_origin = wrap(
                state->carousel_origin, 4U, state->carousel_direction);
            state->carousel_frame = 0U;
        }
    } else if (state->view == TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK_RESET) {
        if ((released & TEAM_MANAGEMENT_A) != 0U) {
            restore_default_playbook(session, asset, team_id);
            state->view = TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK;
        } else if ((released & (TEAM_MANAGEMENT_B | TEAM_MANAGEMENT_DOWN)) != 0U) {
            state->view = TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK;
        }
    }
    return TECMO_TEAM_MANAGEMENT_ACTION_NONE;
}

static bool make_viewport(TecmoFramebuffer *view,
                          TecmoFramebuffer *framebuffer,
                          int origin_x,
                          int origin_y,
                          int scale)
{
    int width;
    int height;
    size_t offset;
    if (view == NULL || framebuffer == NULL || framebuffer->pixels == NULL ||
        framebuffer->width <= 0 || framebuffer->height <= 0 ||
        framebuffer->pitch_pixels < framebuffer->width || scale <= 0 ||
        scale > INT_MAX / 256 || scale > INT_MAX / 240 ||
        origin_x < 0 || origin_y < 0)
        return false;
    width = 256 * scale;
    height = 240 * scale;
    if (origin_x > framebuffer->width - width ||
        origin_y > framebuffer->height - height)
        return false;
    offset = (size_t)origin_y * (size_t)framebuffer->pitch_pixels +
             (size_t)origin_x;
    view->pixels = framebuffer->pixels + offset;
    view->width = width;
    view->height = height;
    view->pitch_pixels = framebuffer->pitch_pixels;
    return true;
}

static void draw_cell(TecmoFramebuffer *view,
                      const TecmoStartGameMenuCell *cell,
                      const uint8_t palette[16],
                      const uint8_t *chr,
                      uint64_t chr_count,
                      int x,
                      int y,
                      int scale)
{
    uint32_t colors[4];
    size_t base = (size_t)cell->palette_index * 4U;
    for (size_t i = 0U; i < 4U; ++i)
        colors[i] = tecmo_nes_2c02_rgba(palette[base + i]);
    tecmo_draw_chr_tile_at_offset_ex(view, chr, chr_count, cell->chr_offset,
                                     x, y, scale, colors, false, false);
}

static void draw_screen(TecmoFramebuffer *view,
                        const TecmoStartGameMenuCell *cells,
                        const uint8_t palette[16],
                        const uint8_t *chr,
                        uint64_t chr_count,
                        int scale)
{
    for (size_t i = 0U; i < TECMO_TEAM_MANAGEMENT_SCREEN_CELLS; ++i)
        draw_cell(view, &cells[i], palette, chr, chr_count,
                  (int)(i % 32U) * 8 * scale,
                  (int)(i / 32U) * 8 * scale, scale);
}

static const TecmoStartGameMenuCell *font_cell(const TecmoTeamDataAsset *data,
                                                char c)
{
    unsigned value = (unsigned char)c;
    if (value < 0x20U || value >= 0x20U + TECMO_TEAM_DATA_FONT_COUNT)
        value = 0x20U;
    return &data->font[value - 0x20U];
}

static void draw_text(TecmoFramebuffer *view,
                      const TecmoTeamDataAsset *data,
                      const uint8_t palette[16],
                      uint8_t palette_index,
                      const char *text,
                      int x,
                      int y,
                      int scale,
                      const uint8_t *chr,
                      uint64_t chr_count)
{
    for (size_t i = 0U; text[i] != '\0'; ++i) {
        TecmoStartGameMenuCell cell = *font_cell(data, text[i]);
        cell.palette_index = palette_index;
        draw_cell(view, &cell, palette, chr, chr_count,
                  (x + (int)i * 8) * scale, y * scale, scale);
    }
}

static void draw_cursor(TecmoFramebuffer *view,
                        const TecmoTeamDataAsset *data,
                        const uint8_t *chr,
                        uint64_t chr_count,
                        int x,
                        int y,
                        int scale)
{
    uint32_t colors[4];
    for (size_t i = 0U; i < 4U; ++i)
        colors[i] = tecmo_nes_2c02_rgba(data->sprite_palette[i]);
    colors[0] = 0U;
    tecmo_draw_chr_tile_at_offset_ex(
        view, chr, chr_count, data->cursors[1].top_chr_offset,
        x * scale, y * scale, scale, colors, false, false);
    tecmo_draw_chr_tile_at_offset_ex(
        view, chr, chr_count, data->cursors[1].bottom_chr_offset,
        x * scale, (y + 8) * scale, scale, colors, false, false);
}

static void trim_copy(char *destination, size_t size,
                      const char *begin, const char *end)
{
    size_t count;
    while (begin < end && *begin == ' ') ++begin;
    while (end > begin && end[-1] == ' ') --end;
    count = (size_t)(end - begin);
    if (count >= size) count = size - 1U;
    memcpy(destination, begin, count);
    destination[count] = '\0';
}

static void draw_play(TecmoFramebuffer *view,
                      const TecmoTeamManagementAsset *asset,
                      const TecmoTeamDataAsset *data,
                      uint8_t play,
                      int x,
                      int scale,
                      const uint8_t *chr,
                      uint64_t chr_count)
{
    const char *amp = strchr(asset->play_names[play], '&');
    char first[18];
    char second[18];
    for (size_t i = 0U; i < 64U; ++i)
        draw_cell(view, &asset->diagrams[play][i], asset->palettes[1],
                  chr, chr_count, (x + (int)(i % 8U) * 8) * scale,
                  (64 + (int)(i / 8U) * 8) * scale, scale);
    if (amp == NULL) return;
    trim_copy(first, sizeof(first), asset->play_names[play], amp);
    trim_copy(second, sizeof(second), amp + 1, asset->play_names[play] + 17U);
    draw_text(view, data, asset->palettes[1], 0U, first,
              x + (64 - (int)strlen(first) * 8) / 2, 136, scale,
              chr, chr_count);
    draw_text(view, data, asset->palettes[1], 0U, second,
              x + (64 - (int)strlen(second) * 8) / 2, 144, scale,
              chr, chr_count);
}

bool tecmo_team_management_draw(
    TecmoFramebuffer *framebuffer,
    const TecmoTeamManagementAsset *asset,
    const TecmoTeamDataAsset *team_data,
    const TecmoTeamManagementSession *session,
    const TecmoTeamManagementViewState *state,
    uint8_t team_id,
    const uint8_t *chr_bytes,
    uint64_t chr_byte_count,
    int origin_x,
    int origin_y,
    int scale)
{
    TecmoFramebuffer view;
    if (asset == NULL || !asset->available || team_data == NULL ||
        !team_data->available || !tecmo_team_management_session_valid(session) ||
        state == NULL || team_id >= TECMO_TEAM_MANAGEMENT_TEAM_COUNT ||
        !view_state_valid(asset, state) ||
        !tecmo_team_management_asset_chr_available(asset, chr_bytes,
                                                   chr_byte_count) ||
        !make_viewport(&view, framebuffer, origin_x, origin_y, scale))
        return false;
    if (state->view == TECMO_TEAM_MANAGEMENT_VIEW_STARTERS ||
        state->view == TECMO_TEAM_MANAGEMENT_VIEW_STARTER_RESET ||
        state->view == TECMO_TEAM_MANAGEMENT_VIEW_STARTER_BENCH) {
        char team_title[34];
        draw_screen(&view, asset->starters_screen, asset->palettes[0],
                    chr_bytes, chr_byte_count, scale);
        (void)snprintf(team_title, sizeof(team_title), "%s %s",
                       team_data->teams[team_id].city,
                       team_data->teams[team_id].nickname);
        draw_text(&view, team_data, asset->palettes[0], 2U, team_title,
                  16, 8, scale, chr_bytes, chr_byte_count);
        draw_text(&view, team_data, asset->palettes[0], 2U, "RESET", 56, 48,
                  scale, chr_bytes, chr_byte_count);
        for (size_t i = 0U; i < 5U; ++i) {
            uint8_t player = session->starters[team_id][i];
            char lineup_name[12];
            const char *full_name = team_data->players[team_id][player].name;
            const char *last_name = strrchr(full_name, ' ');
            if (last_name != NULL) ++last_name;
            else last_name = full_name;
            (void)snprintf(lineup_name, sizeof(lineup_name), "%.11s", last_name);
            draw_text(&view, team_data, asset->palettes[0], 2U,
                      team_data->players[team_id][player].name,
                      56, 56 + (int)i * 8, scale,
                      chr_bytes, chr_byte_count);
            draw_text(&view, team_data, asset->palettes[0], 2U,
                      lineup_name, 32, 132 + (int)i * 8, scale,
                      chr_bytes, chr_byte_count);
        }
        draw_cursor(&view, team_data, chr_bytes, chr_byte_count,
                    asset->starters_cursor_x,
                    asset->starters_cursor_y +
                        (int)state->selection * asset->starters_cursor_stride,
                    scale);
        if (state->view == TECMO_TEAM_MANAGEMENT_VIEW_STARTER_RESET) {
            draw_text(&view, team_data, asset->palettes[0], 2U, "STARTER RESET",
                      64, 160, scale, chr_bytes, chr_byte_count);
            draw_text(&view, team_data, asset->palettes[0], 2U, "NO", 104, 176,
                      scale, chr_bytes, chr_byte_count);
            draw_text(&view, team_data, asset->palettes[0], 2U, "YES", 104, 192,
                      scale, chr_bytes, chr_byte_count);
            draw_cursor(&view, team_data, chr_bytes, chr_byte_count,
                        88, 172 + (int)state->secondary_selection * 16, scale);
        } else if (state->view == TECMO_TEAM_MANAGEMENT_VIEW_STARTER_BENCH) {
            uint8_t candidates[7];
            size_t count = bench_candidates(candidates, session, team_data,
                                            team_id);
            for (size_t i = 0U; i < count; ++i)
                draw_text(&view, team_data, asset->palettes[0], 2U,
                          team_data->players[team_id][candidates[i]].name,
                          144, 48 + (int)i * 16, scale,
                          chr_bytes, chr_byte_count);
            draw_cursor(&view, team_data, chr_bytes, chr_byte_count,
                        128, 44 + (int)state->secondary_selection * 16, scale);
        }
        return true;
    }
    draw_screen(&view,
                asset->playbook_screens[
                    state->view == TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK ? 0U : 1U],
                asset->palettes[1], chr_bytes, chr_byte_count, scale);
    if (state->view == TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK ||
        state->view == TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK_RESET) {
        for (size_t slot = 0U; slot < 4U; ++slot)
            draw_play(&view, asset, team_data,
                      session->playbooks[team_id][slot], (int)slot * 64,
                      scale, chr_bytes, chr_byte_count);
        draw_cursor(&view, team_data, chr_bytes, chr_byte_count,
                    24 + (int)state->selection * 64, 40, scale);
        if (state->view == TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK_RESET)
            draw_text(&view, team_data, asset->palettes[1], 0U,
                      "A RESET   B CANCEL", 56, 184, scale,
                      chr_bytes, chr_byte_count);
    } else {
        uint8_t unused[4];
        int offset = state->carousel_direction == 0 ? 0 :
            state->carousel_direction * (int)state->carousel_frame *
                asset->carousel_pixels_per_frame;
        if (unused_plays(unused, session, team_id) != 4U) return false;
        for (int slot = -1; slot <= 4; ++slot) {
            uint8_t index = (uint8_t)((state->carousel_origin + slot + 4) % 4);
            draw_play(&view, asset, team_data, unused[index],
                      slot * 64 - offset, scale, chr_bytes, chr_byte_count);
        }
        draw_cursor(&view, team_data, chr_bytes, chr_byte_count,
                    152, 40, scale);
    }
    return true;
}

const char *tecmo_team_management_view_name(TecmoTeamManagementView view)
{
    switch (view) {
    case TECMO_TEAM_MANAGEMENT_VIEW_STARTERS: return "STARTERS";
    case TECMO_TEAM_MANAGEMENT_VIEW_STARTER_RESET: return "STARTER RESET";
    case TECMO_TEAM_MANAGEMENT_VIEW_STARTER_BENCH: return "STARTER BENCH";
    case TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK: return "PLAYBOOK";
    case TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK_REPLACE: return "PLAYBOOK REPLACE";
    case TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK_RESET: return "PLAYBOOK RESET";
    default: return "NONE";
    }
}

static void release_button(TecmoControlFrame *frame, uint8_t button)
{
    memset(frame, 0, sizeof(*frame));
    if (button == TEAM_MANAGEMENT_A) frame->released.shoot = true;
    else if (button == TEAM_MANAGEMENT_B) frame->released.cancel = true;
    else if (button == TEAM_MANAGEMENT_START) frame->released.confirm = true;
    else if (button == TEAM_MANAGEMENT_UP) frame->released.up = true;
    else if (button == TEAM_MANAGEMENT_DOWN) frame->released.down = true;
    else if (button == TEAM_MANAGEMENT_LEFT) frame->released.left = true;
    else if (button == TEAM_MANAGEMENT_RIGHT) frame->released.right = true;
}

bool tecmo_team_management_self_test(const char *project_root,
                                     char *message,
                                     size_t message_size)
{
    TecmoTeamManagementAsset asset;
    TecmoTeamDataAsset data;
    TecmoTeamManagementSession session;
    TecmoTeamManagementViewState state;
    TecmoControlFrame controls;
    uint8_t player = 0xFFU;
    if (!tecmo_team_management_asset_load(&asset, project_root) ||
        !tecmo_team_data_asset_load(&data, project_root) ||
        !tecmo_team_management_session_init(&session, &asset)) {
        (void)snprintf(message, message_size,
                       "TTMG-1 assets/session were unavailable");
        return false;
    }
    tecmo_team_management_view_init_starters(&state);
    state.selection = 1U;
    release_button(&controls, TEAM_MANAGEMENT_A);
    (void)tecmo_team_management_update(&state, &session, &asset, &data, 0U,
                                       &controls, &player);
    if (state.view != TECMO_TEAM_MANAGEMENT_VIEW_STARTER_BENCH) goto fail;
    state.secondary_selection = 0U;
    release_button(&controls, TEAM_MANAGEMENT_A);
    (void)tecmo_team_management_update(&state, &session, &asset, &data, 0U,
                                       &controls, &player);
    if (session.starters[0][0] != 5U ||
        !tecmo_team_management_session_valid(&session)) goto fail;
    tecmo_team_management_view_init_starters(&state);
    state.selection = 1U;
    release_button(&controls, TEAM_MANAGEMENT_START);
    if (tecmo_team_management_update(&state, &session, &asset, &data, 0U,
                                     &controls, &player) !=
            TECMO_TEAM_MANAGEMENT_ACTION_PLAYER_DETAIL || player != 5U)
        goto fail;
    tecmo_team_management_view_init_playbook(&state);
    release_button(&controls, TEAM_MANAGEMENT_A);
    (void)tecmo_team_management_update(&state, &session, &asset, &data, 0U,
                                       &controls, &player);
    if (state.view != TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK_REPLACE) goto fail;
    release_button(&controls, TEAM_MANAGEMENT_RIGHT);
    (void)tecmo_team_management_update(&state, &session, &asset, &data, 0U,
                                       &controls, &player);
    if (state.carousel_frame != 0U || state.carousel_direction != 1) goto fail;
    memset(&controls, 0, sizeof(controls));
    for (uint8_t frame = 1U; frame <= 8U; ++frame) {
        (void)tecmo_team_management_update(&state, &session, &asset, &data, 0U,
                                           &controls, &player);
        if (state.carousel_frame != frame) goto fail;
    }
    release_button(&controls, TEAM_MANAGEMENT_A);
    (void)tecmo_team_management_update(&state, &session, &asset, &data, 0U,
                                       &controls, &player);
    if (state.view != TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK ||
        session.playbooks[0][0] == 0U ||
        !tecmo_team_management_session_valid(&session)) goto fail;
    release_button(&controls, TEAM_MANAGEMENT_UP);
    (void)tecmo_team_management_update(&state, &session, &asset, &data, 0U,
                                       &controls, &player);
    if (state.view != TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK_RESET) goto fail;
    release_button(&controls, TEAM_MANAGEMENT_A);
    (void)tecmo_team_management_update(&state, &session, &asset, &data, 0U,
                                       &controls, &player);
    if (state.view != TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK ||
        session.playbooks[0][0] != asset.default_playbooks[0][0]) goto fail;
    tecmo_team_management_view_init_starters(&state);
    state.selection = 0U;
    release_button(&controls, TEAM_MANAGEMENT_A);
    (void)tecmo_team_management_update(&state, &session, &asset, &data, 0U,
                                       &controls, &player);
    release_button(&controls, TEAM_MANAGEMENT_DOWN);
    (void)tecmo_team_management_update(&state, &session, &asset, &data, 0U,
                                       &controls, &player);
    release_button(&controls, TEAM_MANAGEMENT_A);
    (void)tecmo_team_management_update(&state, &session, &asset, &data, 0U,
                                       &controls, &player);
    if (state.view != TECMO_TEAM_MANAGEMENT_VIEW_STARTERS ||
        session.starters[0][0] != asset.default_starters[0][0]) goto fail;
    {
        TecmoTeamManagementViewState invalid = state;
        TecmoTeamManagementSession before = session;
        invalid.selection = asset.starter_selector_count;
        memset(&controls, 0, sizeof(controls));
        if (tecmo_team_management_update(
                &invalid, &session, &asset, &data, 0U, &controls, &player) !=
                TECMO_TEAM_MANAGEMENT_ACTION_NONE ||
            memcmp(&before, &session, sizeof(before)) != 0)
            goto fail;
    }
    (void)snprintf(message, message_size,
                   "TTMG-1 parser, persistence, substitution, reset, detail return anchor, bounds, and carousel tests passed");
    return true;
fail:
    (void)snprintf(message, message_size,
                   "TTMG-1 management state/input self-test failed");
    return false;
}
