#include "tecmo_team_data.h"

#include "tecmo_asset_pack.h"
#include "tecmo_nes_video.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEAM_DATA_ENTRY_ID "menu/team-data"
#define TEAM_DATA_PAYLOAD_SIZE 96372U
#define TEAM_DATA_PAYLOAD_FNV1A32 0x812628F0U
#define TEAM_DATA_CHR_SIZE 262144U
#define TEAM_DATA_CHR_FNV1A32 0xF6F6E854U
#define TEAM_DATA_CHR_FNV1A64 0x96A64F53B240ABB4ULL
#define TEAM_DATA_CELLS_OFFSET 256U
#define TEAM_DATA_PALETTES_OFFSET 17536U
#define TEAM_DATA_SPRITE_PALETTE_OFFSET 17584U
#define TEAM_DATA_CURSORS_OFFSET 17600U
#define TEAM_DATA_SELECTORS_OFFSET 17632U
#define TEAM_DATA_FONT_OFFSET 17748U
#define TEAM_DATA_TEAMS_OFFSET 18220U
#define TEAM_DATA_LOGOS_OFFSET 19380U
#define TEAM_DATA_PLAYERS_OFFSET 32340U
#define TEAM_DATA_LOGO_CELL_STRIDE 8U
#define TEAM_DATA_PLAYER_STRIDE 184U
#define TEAM_DATA_PLAYER_PORTRAIT_OFFSET 40U
#define TEAM_DATA_PROFILE_PALETTES_OFFSET 128U

typedef enum TeamDataButton {
    TEAM_DATA_BUTTON_RIGHT = 0x01,
    TEAM_DATA_BUTTON_LEFT = 0x02,
    TEAM_DATA_BUTTON_DOWN = 0x04,
    TEAM_DATA_BUTTON_UP = 0x08,
    TEAM_DATA_BUTTON_B = 0x40,
    TEAM_DATA_BUTTON_A = 0x80
} TeamDataButton;

typedef enum TeamDataSelectionAction {
    TEAM_DATA_SELECTION_NONE,
    TEAM_DATA_SELECTION_ACCEPT,
    TEAM_DATA_SELECTION_CANCEL
} TeamDataSelectionAction;

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

static bool read_dependencies(const char *root,
                              uint8_t **payload,
                              uint64_t *payload_count,
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
        if (make_path(root_pack, sizeof(root_pack), root, "tecmo.assetpack") == 0)
            paths[path_count++] = root_pack;
        paths[path_count++] = "build\\tecmo.assetpack";
        paths[path_count++] = "tecmo.assetpack";
    }
    for (size_t i = 0U; i < path_count; ++i) {
        if (!file_exists(paths[i])) continue;
        if (tecmo_asset_pack_read_entry_exact(
                paths[i], TEAM_DATA_ENTRY_ID, TEAM_DATA_PAYLOAD_SIZE,
                payload, payload_count) != 0)
            return false;
        if (tecmo_asset_pack_read_entry_exact(
                paths[i], "chr/all", TEAM_DATA_CHR_SIZE,
                chr, chr_count) != 0) {
            tecmo_asset_pack_free(*payload);
            *payload = NULL;
            *payload_count = 0U;
            return false;
        }
        return true;
    }
    return false;
}

static bool parse_cell(TecmoStartGameMenuCell *cell,
                       const uint8_t *source,
                       uint8_t r0,
                       uint8_t r1)
{
    cell->tile_id = source[0];
    cell->palette_index = source[1];
    cell->chr_offset = read_u32(source + 2U);
    return cell->palette_index <= 3U &&
           cell->chr_offset == bg_chr_offset(cell->tile_id, r0, r1);
}

static bool valid_fixed_text(const uint8_t *text, size_t count)
{
    bool terminator_seen = false;
    bool character_seen = false;
    for (size_t i = 0U; i < count; ++i) {
        uint8_t c = text[i];
        if (c == 0U) {
            terminator_seen = true;
            continue;
        }
        if (terminator_seen ||
            !((c >= 'A' && c <= 'Z') || c == ' ' || c == '-' || c == '.'))
            return false;
        character_seen = true;
    }
    return character_seen && terminator_seen;
}

static bool parse_payload(TecmoTeamDataAsset *asset,
                          const uint8_t *bytes,
                          uint64_t count)
{
    static const uint8_t r0[3] = {0xFAU, 0xFAU, 0xCCU};
    static const uint8_t r1[3] = {0xFAU, 0xFAU, 0xFAU};
    static const uint8_t all_star_source_teams[2][12] = {
        {12U, 21U, 8U, 25U, 23U, 25U, 8U, 20U, 20U, 12U, 9U, 6U},
        {7U, 3U, 3U, 19U, 17U, 4U, 26U, 7U, 7U, 1U, 0U, 4U}
    };
    static const uint8_t all_star_source_players[2][12] = {
        {1U, 1U, 2U, 3U, 4U, 0U, 0U, 1U, 6U, 2U, 4U, 4U},
        {0U, 1U, 2U, 3U, 4U, 0U, 0U, 1U, 2U, 2U, 3U, 4U}
    };
    bool seen_teams[TECMO_TEAM_DATA_TEAM_COUNT] = {false};

    if (bytes == NULL || count != TEAM_DATA_PAYLOAD_SIZE ||
        fnv1a32(bytes, count) != TEAM_DATA_PAYLOAD_FNV1A32 ||
        memcmp(bytes, "TTDT", 4U) != 0 ||
        read_u16(bytes + 4U) != 1U || read_u16(bytes + 6U) != 256U ||
        read_u16(bytes + 8U) != 32U || read_u16(bytes + 10U) != 30U ||
        read_u16(bytes + 12U) != 3U || read_u16(bytes + 14U) != 29U ||
        read_u16(bytes + 16U) != 29U || read_u16(bytes + 18U) != 12U ||
        read_u32(bytes + 20U) != TEAM_DATA_CELLS_OFFSET ||
        read_u32(bytes + 24U) != TEAM_DATA_PALETTES_OFFSET ||
        read_u32(bytes + 28U) != TEAM_DATA_SPRITE_PALETTE_OFFSET ||
        read_u32(bytes + 32U) != TEAM_DATA_CURSORS_OFFSET ||
        read_u32(bytes + 36U) != TEAM_DATA_SELECTORS_OFFSET ||
        read_u32(bytes + 40U) != TEAM_DATA_FONT_OFFSET ||
        read_u32(bytes + 44U) != TEAM_DATA_TEAMS_OFFSET ||
        read_u32(bytes + 48U) != TEAM_DATA_LOGOS_OFFSET ||
        read_u32(bytes + 52U) != TEAM_DATA_PLAYERS_OFFSET ||
        read_u32(bytes + 56U) != TEAM_DATA_PAYLOAD_SIZE ||
        read_u32(bytes + 60U) != TEAM_DATA_CHR_SIZE ||
        read_u32(bytes + 64U) != TEAM_DATA_CHR_FNV1A32 ||
        bytes[68U] != 16U || bytes[69U] != 5U ||
        bytes[70U] != 5U || bytes[71U] != 8U ||
        bytes[72U] != 32U || bytes[73U] != 8U ||
        bytes[74U] != 135U || bytes[75U] != 80U || bytes[76U] != 8U ||
        bytes[77U] != 40U || bytes[78U] != 143U || bytes[79U] != 8U ||
        memcmp(bytes + 80U, "\xFA\xFA\xCC\xFA\xFA\xFA", 6U) != 0 ||
        bytes[86U] != 0x0CU || bytes[87U] != 0x0DU || bytes[88U] != 0x0EU ||
        bytes[89U] != 0x20U || bytes[90U] != 59U || bytes[91U] != 3U ||
        bytes[92U] != 6U || bytes[93U] != 2U ||
        bytes[94U] != 6U || bytes[95U] != 4U ||
        bytes[96U] != 32U || bytes[97U] != 32U ||
        bytes[98U] != 0x67U || bytes[99U] != 0x5FU ||
        bytes[100U] != 0x64U || bytes[101U] != 48U ||
        bytes[102U] != 8U || bytes[103U] != 10U ||
        bytes[104U] != 16U || bytes[105U] != 19U ||
        bytes[106U] != 4U || bytes[107U] != 32U ||
        bytes[108U] != 8U || bytes[109U] != 10U ||
        bytes[110U] != 15U || bytes[111U] != 18U ||
        bytes[112U] != 4U || bytes[113U] != 31U ||
        bytes[114U] != 4U || bytes[115U] != 7U ||
        bytes[116U] != 4U || bytes[117U] != 20U)
        return false;
    for (size_t i = 118U; i < 128U; ++i)
        if (bytes[i] != 0U) return false;
    for (size_t group = 0U;
         group < TECMO_TEAM_DATA_PROFILE_PALETTE_COUNT; ++group) {
        for (size_t color = 0U; color < 16U; ++color) {
            uint8_t value = bytes[TEAM_DATA_PROFILE_PALETTES_OFFSET +
                                  group * 16U + color];
            if (value > 0x3FU) return false;
            asset->profile_palettes[group][color] = value;
        }
    }
    for (size_t i = 192U; i < 256U; ++i)
        if (bytes[i] != 0U) return false;

    for (size_t screen = 0U; screen < 3U; ++screen) {
        for (size_t cell = 0U; cell < TECMO_TEAM_DATA_SCREEN_CELLS; ++cell)
            if (!parse_cell(&asset->screens[screen][cell],
                            bytes + TEAM_DATA_CELLS_OFFSET +
                                (screen * TECMO_TEAM_DATA_SCREEN_CELLS + cell) * 6U,
                            r0[screen], r1[screen]))
                return false;
        for (size_t i = 0U; i < 16U; ++i) {
            uint8_t color = bytes[TEAM_DATA_PALETTES_OFFSET + screen * 16U + i];
            if (color > 0x3FU) return false;
            asset->palettes[screen][i] = color;
        }
    }
    for (size_t i = 0U; i < 16U; ++i) {
        uint8_t color = bytes[TEAM_DATA_SPRITE_PALETTE_OFFSET + i];
        if (color > 0x3FU) return false;
        asset->sprite_palette[i] = color;
    }
    for (size_t i = 0U; i < 2U; ++i) {
        const uint8_t *source = bytes + TEAM_DATA_CURSORS_OFFSET + i * 16U;
        TecmoTeamDataCursor *cursor = &asset->cursors[i];
        cursor->dx = read_i16(source);
        cursor->dy = read_i16(source + 2U);
        cursor->top_chr_offset = read_u32(source + 4U);
        cursor->bottom_chr_offset = read_u32(source + 8U);
        cursor->selector = source[12U];
        cursor->raw_tile = source[13U];
        if (cursor->top_chr_offset != 0xC240U ||
            cursor->bottom_chr_offset != 0xC250U ||
            cursor->selector != 0x30U || cursor->raw_tile != 0x24U ||
            source[14U] != 0U || source[15U] != 0U ||
            (i == 0U && (cursor->dx != -1 || cursor->dy != 0)) ||
            (i == 1U && (cursor->dx != 0 || cursor->dy != -4)))
            return false;
    }
    for (size_t i = 0U; i < TECMO_TEAM_DATA_SELECTOR_COUNT; ++i) {
        const uint8_t *source = bytes + TEAM_DATA_SELECTORS_OFFSET + i * 4U;
        TecmoTeamDataSelector *selector = &asset->selectors[i];
        size_t column = i < 11U ? 0U : (i < 20U ? 1U : 2U);
        size_t row = i < 11U ? i : (i < 20U ? i - 11U : i - 20U);
        uint8_t expected_x = (uint8_t)(16U + column * 80U);
        uint8_t expected_y = column == 0U && row < 2U
            ? (uint8_t)(32U + row * 16U)
            : (uint8_t)(72U + (row - (column == 0U ? 2U : 0U)) * 16U);
        selector->x = source[0];
        selector->y = source[1];
        selector->team_id = source[2];
        if (source[3] != 0U || selector->x != expected_x ||
            selector->y != expected_y ||
            selector->team_id >= TECMO_TEAM_DATA_TEAM_COUNT ||
            seen_teams[selector->team_id])
            return false;
        seen_teams[selector->team_id] = true;
    }
    if (asset->selectors[0].team_id != 28U ||
        asset->selectors[1].team_id != 27U)
        return false;

    for (size_t i = 0U; i < TECMO_TEAM_DATA_FONT_COUNT; ++i) {
        const uint8_t *source = bytes + TEAM_DATA_FONT_OFFSET + i * 8U;
        TecmoStartGameMenuCell *font = &asset->font[i];
        font->tile_id = source[1];
        font->palette_index = 0U;
        font->chr_offset = read_u32(source + 4U);
        if (source[0] != (uint8_t)(0x20U + i) ||
            source[2] != 0xFAU || source[3] != 0xFAU ||
            font->chr_offset != bg_chr_offset(font->tile_id, 0xFAU, 0xFAU))
            return false;
    }

    for (size_t team = 0U; team < TECMO_TEAM_DATA_TEAM_COUNT; ++team) {
        const uint8_t *source = bytes + TEAM_DATA_TEAMS_OFFSET + team * 40U;
        TecmoTeamDataTeam *dest = &asset->teams[team];
        if (!valid_fixed_text(source, 16U) ||
            !valid_fixed_text(source + 16U, 16U))
            return false;
        memcpy(dest->city, source, 16U);
        memcpy(dest->nickname, source + 16U, 16U);
        dest->conference = source[32U];
        dest->division = source[33U];
        dest->logo_width = source[34U];
        dest->logo_height = source[35U];
        dest->logo_selector = source[36U];
        dest->logo_tile_high = source[37U];
        dest->logo_count = source[38U];
        dest->logo_x = (uint8_t)((source[39U] & 0x0FU) * 16U);
        dest->profile_palette_group = (uint8_t)(source[39U] >> 4U);
        if (dest->conference > 1U) return false;
        if (dest->profile_palette_group >=
                TECMO_TEAM_DATA_PROFILE_PALETTE_COUNT ||
            (dest->logo_x != 16U && dest->logo_x != 32U))
            return false;
        if (team < TECMO_TEAM_DATA_REAL_TEAM_COUNT) {
            if (dest->division > 3U || dest->logo_width == 0U ||
                dest->logo_height == 0U ||
                (size_t)dest->logo_width * dest->logo_height != dest->logo_count ||
                dest->logo_count > TECMO_TEAM_DATA_LOGO_CELL_LIMIT ||
                (dest->logo_selector & 1U) != 0U ||
                (dest->logo_tile_high != 0U && dest->logo_tile_high != 0x80U))
                return false;
        } else if (dest->division != 0xFFU || dest->logo_width != 0U ||
                   dest->logo_height != 0U || dest->logo_count != 0U) {
            return false;
        }
    }
    for (size_t team = 0U; team < TECMO_TEAM_DATA_REAL_TEAM_COUNT; ++team) {
        const TecmoTeamDataTeam *team_info = &asset->teams[team];
        for (size_t i = 0U; i < TECMO_TEAM_DATA_LOGO_CELL_LIMIT; ++i) {
            const uint8_t *source = bytes + TEAM_DATA_LOGOS_OFFSET +
                                    (team * TECMO_TEAM_DATA_LOGO_CELL_LIMIT + i) *
                                        TEAM_DATA_LOGO_CELL_STRIDE;
            TecmoStartGameMenuCell *cell = &asset->logos[team][i];
            cell->tile_id = source[0];
            cell->palette_index = source[1];
            cell->chr_offset = read_u32(source + 4U);
            if (source[2U] != 0U || source[3U] != 0U) return false;
            if (i < team_info->logo_count) {
                if (cell->palette_index > 3U ||
                    cell->chr_offset !=
                        bg_chr_offset(cell->tile_id,
                                      team_info->logo_selector,
                                      0xFAU))
                    return false;
            } else if (cell->tile_id != 0U || cell->palette_index != 0U ||
                       cell->chr_offset != 0U) {
                return false;
            }
        }
    }

    for (size_t team = 0U; team < TECMO_TEAM_DATA_TEAM_COUNT; ++team) {
        for (size_t player = 0U; player < TECMO_TEAM_DATA_PLAYERS_PER_TEAM;
             ++player) {
            const uint8_t *source = bytes + TEAM_DATA_PLAYERS_OFFSET +
                (team * TECMO_TEAM_DATA_PLAYERS_PER_TEAM + player) *
                    TEAM_DATA_PLAYER_STRIDE;
            TecmoTeamDataPlayer *dest = &asset->players[team][player];
            bool name_seen = false;
            bool zero_seen = false;
            memset(dest->name, 0, sizeof(dest->name));
            for (size_t i = 0U; i < 20U; ++i) {
                uint8_t c = source[i];
                if (c == 0U) {
                    zero_seen = true;
                    continue;
                }
                if (zero_seen || c < 0x20U || c > 0x5AU) return false;
                dest->name[i] = (char)c;
                name_seen = true;
            }
            if (!name_seen || source[20U] < 0x80U ||
                (source[20U] & 0x07U) > 4U || (source[27U] >> 4U) > 2U ||
                source[33U] >= 27U ||
                source[34U] >= TECMO_TEAM_DATA_PLAYERS_PER_TEAM ||
                source[35U] != 0xCCU || source[36U] != 0xFAU ||
                source[37U] != 0x64U || source[38U] != 0U ||
                source[39U] != 0U)
                return false;
            memcpy(dest->attributes, source + 20U, 7U);
            memcpy(dest->profile, source + 27U, 6U);
            dest->source_team = source[33U];
            dest->source_player = source[34U];
            dest->portrait_r0 = source[35U];
            dest->portrait_r1 = source[36U];
            dest->condition_seed = source[37U];
            for (size_t i = 0U; i < TECMO_TEAM_DATA_PORTRAIT_CELL_COUNT; ++i)
                if (!parse_cell(
                        &dest->portrait[i],
                        source + TEAM_DATA_PLAYER_PORTRAIT_OFFSET + i * 6U,
                        dest->portrait_r0, dest->portrait_r1))
                    return false;
            if (team < 27U &&
                (dest->source_team != team || dest->source_player != player))
                return false;
            if (team >= 27U) {
                size_t all_star = team - 27U;
                if (dest->source_team != all_star_source_teams[all_star][player] ||
                    dest->source_player !=
                        all_star_source_players[all_star][player])
                    return false;
            }
        }
    }

    asset->selector_initial_cooldown = bytes[68U];
    asset->selector_repeat_frames = bytes[69U];
    asset->generic_initial_cooldown = bytes[70U];
    asset->generic_repeat_frames = bytes[71U];
    asset->slide_frames = bytes[72U];
    asset->slide_pixels_per_frame = bytes[73U];
    asset->profile_cursor_x = bytes[74U];
    asset->profile_cursor_y = bytes[75U];
    asset->profile_cursor_stride = bytes[76U];
    asset->roster_cursor_x = bytes[77U];
    asset->roster_cursor_y = bytes[78U];
    asset->roster_cursor_stride = bytes[79U];
    asset->logo_y = bytes[101U];
    asset->selector_transition_black_frame = bytes[102U];
    asset->selector_transition_render_off_frame = bytes[103U];
    asset->selector_transition_render_on_frame = bytes[104U];
    asset->selector_transition_first_visible_frame = bytes[105U];
    asset->selector_transition_palette_step_frames = bytes[106U];
    asset->selector_transition_stable_frame = bytes[107U];
    asset->detail_transition_black_frame = bytes[108U];
    asset->detail_transition_render_off_frame = bytes[109U];
    asset->detail_transition_render_on_frame = bytes[110U];
    asset->detail_transition_first_visible_frame = bytes[111U];
    asset->detail_transition_palette_step_frames = bytes[112U];
    asset->detail_transition_stable_frame = bytes[113U];
    asset->entry_transition_render_on_frame = bytes[114U];
    asset->entry_transition_first_visible_frame = bytes[115U];
    asset->entry_transition_palette_step_frames = bytes[116U];
    asset->entry_transition_stable_frame = bytes[117U];
    asset->expected_chr_size = read_u32(bytes + 60U);
    asset->expected_chr_fingerprint32 = read_u32(bytes + 64U);
    return true;
}

bool tecmo_team_data_asset_load(TecmoTeamDataAsset *asset,
                                const char *project_root)
{
    uint8_t *payload = NULL;
    uint8_t *chr = NULL;
    uint64_t payload_count = 0U;
    uint64_t chr_count = 0U;
    bool ok;
    if (asset == NULL) return false;
    memset(asset, 0, sizeof(*asset));
    if (!read_dependencies(project_root, &payload, &payload_count,
                           &chr, &chr_count)) {
        (void)snprintf(asset->status, sizeof(asset->status),
                       "TTDT-1 menu/team-data entry or CHR dependency unavailable");
        return false;
    }
    ok = parse_payload(asset, payload, payload_count) &&
         chr_count == asset->expected_chr_size &&
         fnv1a32(chr, chr_count) == asset->expected_chr_fingerprint32 &&
         fnv1a64(chr, chr_count) == TEAM_DATA_CHR_FNV1A64;
    if (ok) {
        asset->chr_fingerprint64 = fnv1a64(chr, chr_count);
        asset->available = true;
        ok = tecmo_team_data_asset_chr_available(asset, chr, chr_count);
    }
    if (!ok) asset->available = false;
    tecmo_asset_pack_free(payload);
    tecmo_asset_pack_free(chr);
    (void)snprintf(asset->status, sizeof(asset->status), "%s",
                   ok ? "TTDT-1 native assetpack"
                      : "TTDT-1 asset contract rejected");
    return ok;
}

bool tecmo_team_data_asset_chr_available(const TecmoTeamDataAsset *asset,
                                         const uint8_t *chr_bytes,
                                         uint64_t chr_byte_count)
{
    if (asset == NULL || chr_bytes == NULL || !asset->available ||
        asset->expected_chr_size != TEAM_DATA_CHR_SIZE ||
        asset->expected_chr_fingerprint32 != TEAM_DATA_CHR_FNV1A32 ||
        chr_byte_count != asset->expected_chr_size ||
        asset->chr_fingerprint64 != TEAM_DATA_CHR_FNV1A64 ||
        fnv1a32(chr_bytes, chr_byte_count) != asset->expected_chr_fingerprint32 ||
        fnv1a64(chr_bytes, chr_byte_count) != asset->chr_fingerprint64)
        return false;
    for (size_t screen = 0U; screen < TECMO_TEAM_DATA_SCREEN_COUNT; ++screen)
        for (size_t cell = 0U; cell < TECMO_TEAM_DATA_SCREEN_CELLS; ++cell)
            if ((uint64_t)asset->screens[screen][cell].chr_offset + 16U >
                chr_byte_count)
                return false;
    for (size_t i = 0U; i < TECMO_TEAM_DATA_FONT_COUNT; ++i)
        if ((uint64_t)asset->font[i].chr_offset + 16U > chr_byte_count)
            return false;
    for (size_t team = 0U; team < TECMO_TEAM_DATA_REAL_TEAM_COUNT; ++team)
        for (size_t i = 0U; i < asset->teams[team].logo_count; ++i)
            if ((uint64_t)asset->logos[team][i].chr_offset + 16U > chr_byte_count)
                return false;
    for (size_t team = 0U; team < TECMO_TEAM_DATA_TEAM_COUNT; ++team)
        for (size_t player = 0U; player < TECMO_TEAM_DATA_PLAYERS_PER_TEAM;
             ++player)
            for (size_t cell = 0U;
                 cell < TECMO_TEAM_DATA_PORTRAIT_CELL_COUNT; ++cell)
                if ((uint64_t)asset->players[team][player]
                            .portrait[cell].chr_offset + 16U > chr_byte_count)
                    return false;
    for (size_t i = 0U; i < 2U; ++i)
        if ((uint64_t)asset->cursors[i].bottom_chr_offset + 16U > chr_byte_count)
            return false;
    return true;
}

void tecmo_team_data_state_init(TecmoTeamDataState *state)
{
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->phase = TECMO_TEAM_DATA_TEAM_SELECT;
    state->team_id = 28U;
}

static bool state_valid(const TecmoTeamDataAsset *asset,
                        const TecmoTeamDataState *state)
{
    uint16_t cooldown_limit;
    if (asset == NULL || state == NULL || !asset->available ||
        (int)state->phase < 0 || state->phase > TECMO_TEAM_DATA_PLAYER_DETAIL ||
        state->selector_index >= TECMO_TEAM_DATA_SELECTOR_COUNT ||
        state->team_id >= TECMO_TEAM_DATA_TEAM_COUNT ||
        state->profile_selection >= 3U || state->roster_page >= 2U ||
        state->roster_row >= 6U || state->player_index >= 12U ||
        state->slide_frame > asset->slide_frames ||
        state->slide_from_page >= 2U || state->slide_to_page >= 2U ||
        state->slide_direction < -1 || state->slide_direction > 1 ||
        state->cursor_delay > 1U ||
        (int)state->transition < 0 ||
        state->transition > TECMO_TEAM_DATA_TRANSITION_DETAIL_TO_ROSTER)
        return false;
    cooldown_limit = asset->selector_initial_cooldown;
    if (cooldown_limit < asset->generic_initial_cooldown)
        cooldown_limit = asset->generic_initial_cooldown;
    if (cooldown_limit < asset->generic_repeat_frames)
        cooldown_limit = asset->generic_repeat_frames;
    if (state->direction_cooldown > cooldown_limit) return false;
    if (state->slide_direction == 0 && state->slide_frame != 0U) return false;
    if (state->slide_direction != 0 && state->phase != TECMO_TEAM_DATA_ROSTER)
        return false;
    if (state->transition == TECMO_TEAM_DATA_TRANSITION_NONE) {
        if (state->transition_frame != 0U) return false;
    } else {
        uint8_t stable = state->transition ==
                TECMO_TEAM_DATA_TRANSITION_ENTRY_TO_SELECTOR
            ? asset->entry_transition_stable_frame
            : (state->transition ==
                       TECMO_TEAM_DATA_TRANSITION_ROSTER_TO_DETAIL
                   ? asset->detail_transition_stable_frame
                   : asset->selector_transition_stable_frame);
        bool phase_matches =
            (state->transition == TECMO_TEAM_DATA_TRANSITION_ENTRY_TO_SELECTOR &&
             state->phase == TECMO_TEAM_DATA_TEAM_SELECT) ||
            (state->transition == TECMO_TEAM_DATA_TRANSITION_SELECTOR_TO_PROFILE &&
             state->phase == TECMO_TEAM_DATA_TEAM_SELECT) ||
            (state->transition == TECMO_TEAM_DATA_TRANSITION_PROFILE_TO_SELECTOR &&
             state->phase == TECMO_TEAM_DATA_PROFILE) ||
            (state->transition == TECMO_TEAM_DATA_TRANSITION_ROSTER_TO_DETAIL &&
             state->phase == TECMO_TEAM_DATA_ROSTER) ||
            (state->transition == TECMO_TEAM_DATA_TRANSITION_DETAIL_TO_ROSTER &&
             state->phase == TECMO_TEAM_DATA_PLAYER_DETAIL);
        if (!phase_matches || state->transition_frame > stable ||
            state->slide_direction != 0)
            return false;
    }
    return true;
}

static uint8_t transition_stable_frame(const TecmoTeamDataAsset *asset,
                                       TecmoTeamDataTransition transition)
{
    if (transition == TECMO_TEAM_DATA_TRANSITION_ENTRY_TO_SELECTOR)
        return asset->entry_transition_stable_frame;
    return transition == TECMO_TEAM_DATA_TRANSITION_ROSTER_TO_DETAIL
        ? asset->detail_transition_stable_frame
        : asset->selector_transition_stable_frame;
}

static void begin_transition(TecmoTeamDataState *state,
                             TecmoTeamDataTransition transition)
{
    state->transition = transition;
    state->transition_frame = 0U;
    state->cursor_delay = 0U;
}

static void finish_transition(TecmoTeamDataState *state,
                              const TecmoTeamDataAsset *asset)
{
    switch (state->transition) {
    case TECMO_TEAM_DATA_TRANSITION_ENTRY_TO_SELECTOR:
        state->phase = TECMO_TEAM_DATA_TEAM_SELECT;
        break;
    case TECMO_TEAM_DATA_TRANSITION_SELECTOR_TO_PROFILE:
        state->phase = TECMO_TEAM_DATA_PROFILE;
        state->profile_selection = 0U;
        break;
    case TECMO_TEAM_DATA_TRANSITION_PROFILE_TO_SELECTOR:
        state->phase = TECMO_TEAM_DATA_TEAM_SELECT;
        break;
    case TECMO_TEAM_DATA_TRANSITION_ROSTER_TO_DETAIL:
        state->phase = TECMO_TEAM_DATA_PLAYER_DETAIL;
        break;
    case TECMO_TEAM_DATA_TRANSITION_DETAIL_TO_ROSTER:
        state->phase = TECMO_TEAM_DATA_ROSTER;
        state->roster_page = (uint8_t)(state->player_index / 6U);
        state->roster_row = (uint8_t)(state->player_index % 6U);
        break;
    default: return;
    }
    state->transition = TECMO_TEAM_DATA_TRANSITION_NONE;
    state->transition_frame = 0U;
    state->cursor_delay = 1U;
    state->direction_cooldown =
        state->phase == TECMO_TEAM_DATA_TEAM_SELECT
            ? asset->selector_initial_cooldown
            : asset->generic_initial_cooldown;
}

static uint8_t controller_byte(const TecmoInput *input)
{
    uint8_t value = 0U;
    if (input->right) value |= TEAM_DATA_BUTTON_RIGHT;
    if (input->left) value |= TEAM_DATA_BUTTON_LEFT;
    if (input->down) value |= TEAM_DATA_BUTTON_DOWN;
    if (input->up) value |= TEAM_DATA_BUTTON_UP;
    if (input->cancel) value |= TEAM_DATA_BUTTON_B;
    if (input->shoot) value |= TEAM_DATA_BUTTON_A;
    return value;
}

static TeamDataSelectionAction selection_action(const TecmoControlFrame *controls)
{
    uint8_t released;
    if (controller_byte(&controls->held) != 0U) return TEAM_DATA_SELECTION_NONE;
    released = controller_byte(&controls->released) &
               (TEAM_DATA_BUTTON_A | TEAM_DATA_BUTTON_B);
    if ((released & TEAM_DATA_BUTTON_A) != 0U) return TEAM_DATA_SELECTION_ACCEPT;
    if ((released & TEAM_DATA_BUTTON_B) != 0U) return TEAM_DATA_SELECTION_CANCEL;
    return TEAM_DATA_SELECTION_NONE;
}

static uint8_t held_direction(const TecmoControlFrame *controls)
{
    return controller_byte(&controls->held) & 0x0FU;
}

static uint8_t wrap(uint8_t value, uint8_t count, int direction)
{
    if (direction < 0) return value == 0U ? (uint8_t)(count - 1U)
                                          : (uint8_t)(value - 1U);
    return value + 1U >= count ? 0U : (uint8_t)(value + 1U);
}

static void selector_column_row(uint8_t selection,
                                uint8_t *column,
                                uint8_t *row,
                                uint8_t *count)
{
    if (selection < 11U) {
        *column = 0U;
        *row = selection;
        *count = 11U;
    } else if (selection < 20U) {
        *column = 1U;
        *row = (uint8_t)(selection - 11U);
        *count = 9U;
    } else {
        *column = 2U;
        *row = (uint8_t)(selection - 20U);
        *count = 9U;
    }
}

static uint8_t selector_base(uint8_t column)
{
    return column == 0U ? 0U : (column == 1U ? 11U : 20U);
}

static uint8_t nearest_selector_in_column(const TecmoTeamDataAsset *asset,
                                          uint8_t selection,
                                          int direction)
{
    uint8_t column;
    uint8_t row;
    uint8_t count;
    uint8_t target_column;
    uint8_t target_base;
    uint8_t target_count;
    uint8_t best = 0U;
    unsigned best_distance = UINT_MAX;
    selector_column_row(selection, &column, &row, &count);
    (void)row;
    (void)count;
    target_column = direction < 0 ? (column == 0U ? 2U : (uint8_t)(column - 1U))
                                  : (column == 2U ? 0U : (uint8_t)(column + 1U));
    target_base = selector_base(target_column);
    target_count = target_column == 0U ? 11U : 9U;
    for (uint8_t i = 0U; i < target_count; ++i) {
        uint8_t candidate = (uint8_t)(target_base + i);
        unsigned a = asset->selectors[selection].y;
        unsigned b = asset->selectors[candidate].y;
        unsigned distance = a > b ? a - b : b - a;
        if (distance < best_distance) {
            best_distance = distance;
            best = candidate;
        }
    }
    return best;
}

TecmoTeamDataAction tecmo_team_data_update(
    TecmoTeamDataState *state,
    const TecmoTeamDataAsset *asset,
    const TecmoControlFrame *controls)
{
    TeamDataSelectionAction action;
    uint8_t direction;
    if (!state_valid(asset, state) || controls == NULL)
        return TECMO_TEAM_DATA_ACTION_NONE;
    ++state->frame;
    if (state->cursor_delay < 1U) ++state->cursor_delay;

    if (state->transition != TECMO_TEAM_DATA_TRANSITION_NONE) {
        uint8_t stable = transition_stable_frame(asset, state->transition);
        if (state->transition_frame < stable) ++state->transition_frame;
        if (state->transition_frame >= stable)
            finish_transition(state, asset);
        return TECMO_TEAM_DATA_ACTION_NONE;
    }

    if (state->slide_direction != 0) {
        if (state->slide_frame < asset->slide_frames) ++state->slide_frame;
        if (state->slide_frame >= asset->slide_frames) {
            state->roster_page = state->slide_to_page;
            state->slide_frame = 0U;
            state->slide_direction = 0;
            state->direction_cooldown = asset->generic_initial_cooldown;
        }
        return TECMO_TEAM_DATA_ACTION_NONE;
    }

    action = selection_action(controls);
    if (state->phase == TECMO_TEAM_DATA_TEAM_SELECT) {
        if (action == TEAM_DATA_SELECTION_CANCEL)
            return TECMO_TEAM_DATA_ACTION_BACK_TO_START_MENU;
        if (action == TEAM_DATA_SELECTION_ACCEPT) {
            state->team_id = asset->selectors[state->selector_index].team_id;
            begin_transition(
                state, TECMO_TEAM_DATA_TRANSITION_SELECTOR_TO_PROFILE);
            return TECMO_TEAM_DATA_ACTION_NONE;
        }
        direction = held_direction(controls);
        if (state->direction_cooldown == 0U) {
            uint8_t column;
            uint8_t row;
            uint8_t column_count;
            selector_column_row(state->selector_index, &column, &row,
                                &column_count);
            if (direction == TEAM_DATA_BUTTON_UP ||
                direction == TEAM_DATA_BUTTON_DOWN) {
                row = wrap(row, column_count,
                           direction == TEAM_DATA_BUTTON_UP ? -1 : 1);
                state->selector_index = (uint8_t)(selector_base(column) + row);
                state->direction_cooldown = asset->selector_repeat_frames;
            } else if (direction == TEAM_DATA_BUTTON_LEFT ||
                       direction == TEAM_DATA_BUTTON_RIGHT) {
                state->selector_index = nearest_selector_in_column(
                    asset, state->selector_index,
                    direction == TEAM_DATA_BUTTON_LEFT ? -1 : 1);
                state->direction_cooldown = asset->selector_repeat_frames;
            }
        }
    } else if (state->phase == TECMO_TEAM_DATA_PROFILE) {
        if (action == TEAM_DATA_SELECTION_CANCEL) {
            state->selector_index = 0U;
            for (uint8_t i = 0U; i < TECMO_TEAM_DATA_SELECTOR_COUNT; ++i)
                if (asset->selectors[i].team_id == state->team_id) {
                    state->selector_index = i;
                    break;
                }
            begin_transition(
                state, TECMO_TEAM_DATA_TRANSITION_PROFILE_TO_SELECTOR);
            return TECMO_TEAM_DATA_ACTION_NONE;
        }
        if (action == TEAM_DATA_SELECTION_ACCEPT &&
            state->profile_selection == 0U) {
            state->phase = TECMO_TEAM_DATA_ROSTER;
            state->roster_page = 0U;
            state->roster_row = 0U;
            state->direction_cooldown = asset->generic_initial_cooldown;
            state->cursor_delay = 0U;
            return TECMO_TEAM_DATA_ACTION_NONE;
        }
        direction = held_direction(controls);
        if (state->direction_cooldown == 0U &&
            (direction == TEAM_DATA_BUTTON_UP ||
             direction == TEAM_DATA_BUTTON_DOWN)) {
            state->profile_selection = wrap(
                state->profile_selection, 3U,
                direction == TEAM_DATA_BUTTON_UP ? -1 : 1);
            state->direction_cooldown = asset->generic_repeat_frames;
        }
    } else if (state->phase == TECMO_TEAM_DATA_ROSTER) {
        if (action == TEAM_DATA_SELECTION_CANCEL) {
            state->phase = TECMO_TEAM_DATA_PROFILE;
            state->profile_selection = 0U;
            state->direction_cooldown = asset->generic_initial_cooldown;
            state->cursor_delay = 0U;
            return TECMO_TEAM_DATA_ACTION_NONE;
        }
        if (action == TEAM_DATA_SELECTION_ACCEPT) {
            state->player_index = (uint8_t)(state->roster_page * 6U +
                                             state->roster_row);
            begin_transition(
                state, TECMO_TEAM_DATA_TRANSITION_ROSTER_TO_DETAIL);
            return TECMO_TEAM_DATA_ACTION_NONE;
        }
        direction = held_direction(controls);
        if (state->direction_cooldown == 0U) {
            if (direction == TEAM_DATA_BUTTON_UP ||
                direction == TEAM_DATA_BUTTON_DOWN) {
                state->roster_row = wrap(
                    state->roster_row, 6U,
                    direction == TEAM_DATA_BUTTON_UP ? -1 : 1);
                state->direction_cooldown = asset->generic_repeat_frames;
            } else if (direction == TEAM_DATA_BUTTON_LEFT ||
                       direction == TEAM_DATA_BUTTON_RIGHT) {
                state->slide_from_page = state->roster_page;
                state->slide_to_page = (uint8_t)(state->roster_page ^ 1U);
                state->slide_direction =
                    direction == TEAM_DATA_BUTTON_LEFT ? -1 : 1;
                state->slide_frame = 0U;
                state->cursor_delay = 0U;
                return TECMO_TEAM_DATA_ACTION_NONE;
            }
        }
    } else if (action == TEAM_DATA_SELECTION_CANCEL) {
        begin_transition(state, TECMO_TEAM_DATA_TRANSITION_DETAIL_TO_ROSTER);
        return TECMO_TEAM_DATA_ACTION_NONE;
    }

    if (state->direction_cooldown > 0U) --state->direction_cooldown;
    return TECMO_TEAM_DATA_ACTION_NONE;
}

const char *tecmo_team_data_phase_name(TecmoTeamDataPhase phase)
{
    switch (phase) {
    case TECMO_TEAM_DATA_TEAM_SELECT: return "TEAM SELECT";
    case TECMO_TEAM_DATA_PROFILE: return "TEAM PROFILE";
    case TECMO_TEAM_DATA_ROSTER: return "PLAYERS DATA";
    case TECMO_TEAM_DATA_PLAYER_DETAIL: return "PLAYER DETAIL";
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
        framebuffer->pitch_pixels < framebuffer->width || scale <= 0 ||
        scale > INT_MAX / 256 || scale > INT_MAX / 240 ||
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

static void draw_cell(TecmoFramebuffer *view,
                      const TecmoStartGameMenuCell *cell,
                      const uint8_t palette[16],
                      const uint8_t *chr_bytes,
                      uint64_t chr_byte_count,
                      int x,
                      int y,
                      int scale,
                      int palette_override)
{
    uint32_t rgba[4];
    uint8_t palette_index = palette_override >= 0
        ? (uint8_t)palette_override : cell->palette_index;
    size_t base = (size_t)palette_index * 4U;
    rgba[0] = tecmo_nes_2c02_rgba(palette[0]);
    for (size_t i = 1U; i < 4U; ++i)
        rgba[i] = tecmo_nes_2c02_rgba(palette[base + i]);
    tecmo_draw_chr_tile_at_offset_ex(view, chr_bytes, chr_byte_count,
                                     cell->chr_offset, x, y, scale, rgba,
                                     false, false);
}

static void draw_screen(TecmoFramebuffer *view,
                        const TecmoTeamDataAsset *asset,
                        size_t screen,
                        const uint8_t palette[16],
                        const uint8_t *chr_bytes,
                        uint64_t chr_byte_count,
                        int scale)
{
    fill_viewport(view, tecmo_nes_2c02_rgba(palette[0]));
    for (size_t i = 0U; i < TECMO_TEAM_DATA_SCREEN_CELLS; ++i)
        draw_cell(view, &asset->screens[screen][i], palette,
                  chr_bytes, chr_byte_count,
                  (int)(i % 32U) * 8 * scale,
                  (int)(i / 32U) * 8 * scale, scale, -1);
}

static void draw_cursor(TecmoFramebuffer *view,
                        const TecmoTeamDataCursor *cursor,
                        const uint8_t sprite_palette[16],
                        const uint8_t *chr_bytes,
                        uint64_t chr_byte_count,
                        int x,
                        int y,
                        int scale)
{
    uint32_t rgba[4] = {0U, 0U, 0U, 0U};
    for (size_t i = 1U; i < 4U; ++i)
        rgba[i] = tecmo_nes_2c02_rgba(sprite_palette[i]);
    tecmo_draw_chr_tile_at_offset_ex(
        view, chr_bytes, chr_byte_count, cursor->top_chr_offset,
        (x + cursor->dx) * scale, (y + cursor->dy + 1) * scale,
        scale, rgba, false, false);
    tecmo_draw_chr_tile_at_offset_ex(
        view, chr_bytes, chr_byte_count, cursor->bottom_chr_offset,
        (x + cursor->dx) * scale, (y + cursor->dy + 9) * scale,
        scale, rgba, false, false);
}

static uint8_t palette_at(const TecmoTeamDataAsset *asset,
                          size_t screen,
                          int x,
                          int y)
{
    int col = x / 8;
    int row = y / 8;
    if (col < 0 || col >= 32 || row < 0 || row >= 30) return 0U;
    return asset->screens[screen][(size_t)row * 32U + (size_t)col].palette_index;
}

static void draw_text(TecmoFramebuffer *view,
                      const TecmoTeamDataAsset *asset,
                      size_t screen,
                      const uint8_t palette[16],
                      const char *text,
                      int x,
                      int y,
                      int scale,
                      int clip_x0,
                      int clip_x1,
                      const uint8_t *chr_bytes,
                      uint64_t chr_byte_count)
{
    if (text == NULL) return;
    for (size_t i = 0U; text[i] != '\0'; ++i) {
        unsigned c = (unsigned char)text[i];
        int px = x + (int)i * 8;
        if (c < 0x20U || c >= 0x20U + TECMO_TEAM_DATA_FONT_COUNT) continue;
        if (px + 8 <= clip_x0 || px >= clip_x1) continue;
        draw_cell(view, &asset->font[c - 0x20U], palette,
                  chr_bytes, chr_byte_count, px * scale, y * scale, scale,
                  palette_at(asset, screen, px, y));
    }
}

static void draw_text_forced(TecmoFramebuffer *view,
                             const TecmoTeamDataAsset *asset,
                             const uint8_t palette[16],
                             const char *text,
                             int x,
                             int y,
                             int scale,
                             int palette_index,
                             const uint8_t *chr_bytes,
                             uint64_t chr_byte_count)
{
    if (text == NULL || palette_index < 0 || palette_index > 3) return;
    for (size_t i = 0U; text[i] != '\0'; ++i) {
        unsigned c = (unsigned char)text[i];
        if (c < 0x20U || c >= 0x20U + TECMO_TEAM_DATA_FONT_COUNT) continue;
        draw_cell(view, &asset->font[c - 0x20U], palette,
                  chr_bytes, chr_byte_count,
                  (x + (int)i * 8) * scale, y * scale, scale,
                  palette_index);
    }
}

static void make_team_title(char dest[34], const TecmoTeamDataTeam *team)
{
    (void)snprintf(dest, 34U, "%s %s", team->city, team->nickname);
}

static void draw_team_profile_text(TecmoFramebuffer *view,
                                   const TecmoTeamDataAsset *asset,
                                   const TecmoTeamDataState *state,
                                   const uint8_t palette[16],
                                   const uint8_t *chr_bytes,
                                   uint64_t chr_byte_count,
                                   int scale)
{
    static const char *conference[2] = {"EASTERN", "WESTERN"};
    static const char *division[4] = {
        "ATLANTIC", "CENTRAL", "MIDWEST", "PACIFIC"
    };
    const TecmoTeamDataTeam *team = &asset->teams[state->team_id];
    char title[34];
    make_team_title(title, team);
    draw_text_forced(view, asset, palette, title, 16, 8, scale, 2,
                     chr_bytes, chr_byte_count);
    draw_text_forced(view, asset, palette, conference[team->conference], 184, 32,
                     scale, 2, chr_bytes, chr_byte_count);
    if (team->division < 4U)
        draw_text_forced(view, asset, palette, division[team->division], 184, 40,
                         scale, 2, chr_bytes, chr_byte_count);
    if (state->team_id < TECMO_TEAM_DATA_REAL_TEAM_COUNT) {
        for (size_t i = 0U; i < team->logo_count; ++i) {
            unsigned col = (unsigned)(i % team->logo_width);
            unsigned row = (unsigned)(i / team->logo_width);
            draw_cell(view, &asset->logos[state->team_id][i], palette,
                      chr_bytes, chr_byte_count,
                      (team->logo_x + (int)col * 8) * scale,
                      (asset->logo_y + (int)row * 8) * scale, scale, -1);
        }
    }
}

static unsigned bcd_byte(uint8_t value);

static void draw_roster_page(TecmoFramebuffer *view,
                             const TecmoTeamDataAsset *asset,
                             const TecmoTeamDataState *state,
                             const uint8_t palette[16],
                             uint8_t page,
                             int offset_x,
                             const uint8_t *chr_bytes,
                             uint64_t chr_byte_count,
                             int scale)
{
    for (size_t row = 0U; row < 6U; ++row) {
        const TecmoTeamDataPlayer *player =
            &asset->players[state->team_id][page * 6U + row];
        char number[3];
        (void)snprintf(number, sizeof(number), "%02u",
                       bcd_byte(player->attributes[1]));
        draw_text_forced(view, asset, palette, number, 48 + offset_x,
                         144 + (int)row * 8, scale, 2,
                         chr_bytes, chr_byte_count);
        draw_text_forced(view, asset, palette, player->name, 64 + offset_x,
                         144 + (int)row * 8, scale, 2,
                         chr_bytes, chr_byte_count);
    }
}

static unsigned bcd_byte(uint8_t value)
{
    return (unsigned)(value >> 4U) * 10U + (unsigned)(value & 0x0FU);
}

const char *tecmo_team_data_position_name(uint8_t roster_code)
{
    switch (roster_code & 0x07U) {
    case 0U:
    case 1U: return "GUARD";
    case 2U:
    case 3U: return "FORWARD";
    case 4U: return "CENTER";
    default: return "";
    }
}

const char *tecmo_team_data_condition_name(uint8_t condition_value)
{
    if (condition_value >= 0x5EU) return "EXCELLENT";
    if (condition_value >= 0x4CU) return "GOOD";
    if (condition_value >= 0x38U) return "AVERAGE";
    if (condition_value >= 0x1FU) return "POOR";
    if (condition_value >= 0x01U) return "BAD";
    return "INJURED";
}

uint8_t tecmo_team_data_meter_fill_length(const uint8_t profile[6],
                                          size_t meter_index)
{
    static const uint8_t grade_bases[3] = {0U, 40U, 80U};
    uint8_t value;
    uint8_t delta;
    if (profile == NULL || meter_index >= 6U) return 0U;
    switch (meter_index) {
    case 0U:
        if ((profile[0] >> 4U) >= 3U) return 0U;
        value = (uint8_t)(grade_bases[profile[0] >> 4U] +
                          (profile[0] & 0x0FU) * 4U);
        break;
    case 1U:
        delta = (uint8_t)(profile[1] - 0x90U);
        value = (uint8_t)(delta + (delta >> 1U));
        break;
    case 2U: value = (uint8_t)((profile[2] & 0x0FU) * 10U); break;
    case 3U: value = profile[3]; break;
    case 4U: value = (uint8_t)(profile[4] >> 1U); break;
    default: value = (uint8_t)((profile[5] & 0x0FU) * 8U); break;
    }
    return (uint8_t)(value + 4U);
}

static void draw_player_portrait(TecmoFramebuffer *view,
                                 const TecmoTeamDataPlayer *player,
                                 const uint8_t palette[16],
                                 const uint8_t *chr_bytes,
                                 uint64_t chr_byte_count,
                                 int scale)
{
    for (size_t i = 0U; i < TECMO_TEAM_DATA_PORTRAIT_CELL_COUNT; ++i) {
        draw_cell(view, &player->portrait[i], palette, chr_bytes,
                  chr_byte_count, (32 + (int)(i % 6U) * 8) * scale,
                  (32 + (int)(i / 6U) * 8) * scale, scale, -1);
    }
}

static void draw_player_meters(TecmoFramebuffer *view,
                               const TecmoTeamDataAsset *asset,
                               const TecmoTeamDataPlayer *player,
                               const uint8_t palette[16],
                               const uint8_t *chr_bytes,
                               uint64_t chr_byte_count,
                               int scale)
{
    static const int y_positions[6] = {160, 168, 192, 200, 176, 184};
    for (size_t meter = 0U; meter < 6U; ++meter) {
        uint8_t length = tecmo_team_data_meter_fill_length(player->profile,
                                                           meter);
        size_t full_tiles = length >> 3U;
        uint8_t tail = (uint8_t)(length & 0x07U);
        size_t cell_count = full_tiles + (tail != 0U ? 1U : 0U);
        for (size_t i = 0U; i < cell_count; ++i) {
            TecmoStartGameMenuCell cell;
            int x = 128 + (int)i * 8;
            cell.tile_id = i < full_tiles ? 0x67U : (uint8_t)(0x5FU + tail);
            cell.palette_index = palette_at(asset, 2U, x, y_positions[meter]);
            cell.chr_offset = bg_chr_offset(cell.tile_id, 0xCCU, 0xFAU);
            draw_cell(view, &cell, palette, chr_bytes,
                      chr_byte_count, x * scale, y_positions[meter] * scale,
                      scale, -1);
        }
    }
}

static void draw_player_detail(TecmoFramebuffer *view,
                               const TecmoTeamDataAsset *asset,
                               const TecmoTeamDataState *state,
                               const uint8_t palette[16],
                               const uint8_t *chr_bytes,
                               uint64_t chr_byte_count,
                               int scale)
{
    const TecmoTeamDataTeam *team = &asset->teams[state->team_id];
    const TecmoTeamDataPlayer *player =
        &asset->players[state->team_id][state->player_index];
    char title[34];
    char line[40];
    char first_name[21];
    const char *last_name;
    const char *space;
    unsigned number = bcd_byte(player->attributes[1]);
    unsigned height_feet = player->attributes[2] >> 4U;
    unsigned height_inches = player->attributes[2] & 0x0FU;
    unsigned weight = (unsigned)player->attributes[3] * 4U;
    make_team_title(title, team);
    draw_player_portrait(view, player, palette,
                         chr_bytes, chr_byte_count, scale);
    draw_player_meters(view, asset, player, palette,
                       chr_bytes, chr_byte_count, scale);
    draw_text(view, asset, 2U, palette, title, 16, 8, scale, 0, 256,
              chr_bytes, chr_byte_count);
    space = strchr(player->name, ' ');
    last_name = space != NULL ? space + 1 : "";
    memset(first_name, 0, sizeof(first_name));
    if (space != NULL) {
        size_t length = (size_t)(space - player->name);
        if (length > sizeof(first_name) - 1U) length = sizeof(first_name) - 1U;
        memcpy(first_name, player->name, length);
    } else {
        (void)snprintf(first_name, sizeof(first_name), "%s", player->name);
    }
    (void)snprintf(line, sizeof(line), "%02u-%s", number, first_name);
    draw_text_forced(view, asset, palette, line, 120, 24, scale, 0,
                     chr_bytes, chr_byte_count);
    draw_text_forced(view, asset, palette, last_name, 144, 32, scale, 0,
                     chr_bytes, chr_byte_count);
    (void)snprintf(line, sizeof(line), "%u-%u", height_feet, height_inches);
    draw_text_forced(view, asset, palette, line, 192, 48, scale, 0,
                     chr_bytes, chr_byte_count);
    (void)snprintf(line, sizeof(line), "%u", weight);
    draw_text_forced(view, asset, palette, line, 192, 56, scale, 0,
                     chr_bytes, chr_byte_count);
    draw_text_forced(view, asset, palette,
                     tecmo_team_data_position_name(player->attributes[0]),
                     192, 64, scale, 0,
                     chr_bytes, chr_byte_count);
    draw_text_forced(view, asset, palette,
                     tecmo_team_data_condition_name(player->condition_seed),
                     176, 72, scale, 0,
                     chr_bytes, chr_byte_count);
    (void)snprintf(line, sizeof(line), ".%03u",
                   (unsigned)player->attributes[4] * 4U);
    draw_text_forced(view, asset, palette, line, 8, 112, scale, 0,
                     chr_bytes, chr_byte_count);
    (void)snprintf(line, sizeof(line), ".%03u",
                   (unsigned)player->attributes[5] * 4U);
    draw_text_forced(view, asset, palette, line, 48, 112, scale, 0,
                     chr_bytes, chr_byte_count);
    (void)snprintf(line, sizeof(line), ".%03u",
                   (unsigned)player->attributes[6] * 4U);
    draw_text_forced(view, asset, palette, line, 88, 112, scale, 0,
                     chr_bytes, chr_byte_count);
    draw_text_forced(view, asset, palette, "0", 136, 112, scale, 0,
                     chr_bytes, chr_byte_count);
    draw_text_forced(view, asset, palette, "0", 168, 112, scale, 0,
                     chr_bytes, chr_byte_count);
    draw_text_forced(view, asset, palette, "0", 200, 112, scale, 0,
                     chr_bytes, chr_byte_count);
    draw_text_forced(view, asset, palette, ".0", 240, 112, scale, 0,
                     chr_bytes, chr_byte_count);
}

static bool detail_timing(TecmoTeamDataTransition transition)
{
    return transition == TECMO_TEAM_DATA_TRANSITION_ROSTER_TO_DETAIL;
}

static uint8_t transition_black_frame(const TecmoTeamDataAsset *asset,
                                      TecmoTeamDataTransition transition)
{
    return detail_timing(transition)
        ? asset->detail_transition_black_frame
        : asset->selector_transition_black_frame;
}

static uint8_t transition_render_off_frame(
    const TecmoTeamDataAsset *asset,
    TecmoTeamDataTransition transition)
{
    if (transition == TECMO_TEAM_DATA_TRANSITION_ENTRY_TO_SELECTOR) return 0U;
    return detail_timing(transition)
        ? asset->detail_transition_render_off_frame
        : asset->selector_transition_render_off_frame;
}

static uint8_t transition_render_on_frame(
    const TecmoTeamDataAsset *asset,
    TecmoTeamDataTransition transition)
{
    if (transition == TECMO_TEAM_DATA_TRANSITION_ENTRY_TO_SELECTOR)
        return asset->entry_transition_render_on_frame;
    return detail_timing(transition)
        ? asset->detail_transition_render_on_frame
        : asset->selector_transition_render_on_frame;
}

static uint8_t transition_first_visible_frame(
    const TecmoTeamDataAsset *asset,
    TecmoTeamDataTransition transition)
{
    if (transition == TECMO_TEAM_DATA_TRANSITION_ENTRY_TO_SELECTOR)
        return asset->entry_transition_first_visible_frame;
    return detail_timing(transition)
        ? asset->detail_transition_first_visible_frame
        : asset->selector_transition_first_visible_frame;
}

static uint8_t transition_palette_step_frames(
    const TecmoTeamDataAsset *asset,
    TecmoTeamDataTransition transition)
{
    if (transition == TECMO_TEAM_DATA_TRANSITION_ENTRY_TO_SELECTOR)
        return asset->entry_transition_palette_step_frames;
    return detail_timing(transition)
        ? asset->detail_transition_palette_step_frames
        : asset->selector_transition_palette_step_frames;
}

unsigned tecmo_team_data_transition_palette_stage(
    const TecmoTeamDataAsset *asset,
    const TecmoTeamDataState *state)
{
    uint8_t black;
    uint8_t first_visible;
    uint8_t step;
    unsigned elapsed;
    if (asset == NULL || state == NULL ||
        state->transition == TECMO_TEAM_DATA_TRANSITION_NONE)
        return 3U;
    if (state->transition == TECMO_TEAM_DATA_TRANSITION_ENTRY_TO_SELECTOR) {
        first_visible = asset->entry_transition_first_visible_frame;
        step = asset->entry_transition_palette_step_frames;
        if (step == 0U || state->transition_frame < first_visible) return 4U;
        elapsed = (unsigned)(state->transition_frame - first_visible) / step;
        return elapsed > 3U ? 3U : elapsed;
    }
    black = transition_black_frame(asset, state->transition);
    first_visible = transition_first_visible_frame(asset, state->transition);
    step = transition_palette_step_frames(asset, state->transition);
    if (black == 0U || step == 0U) return 4U;
    if (state->transition_frame < black) {
        unsigned fade_step = state->transition_frame / (black / 4U);
        return fade_step >= 4U ? 4U : 3U - fade_step;
    }
    if (state->transition_frame < first_visible) return 4U;
    elapsed = (unsigned)(state->transition_frame - first_visible) / step;
    return elapsed > 3U ? 3U : elapsed;
}

bool tecmo_team_data_transition_render_enabled(
    const TecmoTeamDataAsset *asset,
    const TecmoTeamDataState *state)
{
    uint8_t off;
    uint8_t on;
    if (asset == NULL || state == NULL ||
        state->transition == TECMO_TEAM_DATA_TRANSITION_NONE)
        return true;
    off = transition_render_off_frame(asset, state->transition);
    on = transition_render_on_frame(asset, state->transition);
    return state->transition_frame < off || state->transition_frame >= on;
}

static TecmoTeamDataPhase transition_display_phase(
    const TecmoTeamDataAsset *asset,
    const TecmoTeamDataState *state)
{
    if (state->transition == TECMO_TEAM_DATA_TRANSITION_NONE ||
        state->transition_frame <
            transition_render_on_frame(asset, state->transition))
        return state->phase;
    switch (state->transition) {
    case TECMO_TEAM_DATA_TRANSITION_SELECTOR_TO_PROFILE:
        return TECMO_TEAM_DATA_PROFILE;
    case TECMO_TEAM_DATA_TRANSITION_PROFILE_TO_SELECTOR:
        return TECMO_TEAM_DATA_TEAM_SELECT;
    case TECMO_TEAM_DATA_TRANSITION_ROSTER_TO_DETAIL:
        return TECMO_TEAM_DATA_PLAYER_DETAIL;
    case TECMO_TEAM_DATA_TRANSITION_DETAIL_TO_ROSTER:
        return TECMO_TEAM_DATA_ROSTER;
    default: return state->phase;
    }
}

static uint8_t brightness_cap(uint8_t color, unsigned cap)
{
    uint8_t brightness;
    if (color == 0x0FU) return color;
    brightness = (uint8_t)(color & 0x30U);
    if (brightness > (uint8_t)(cap << 4U))
        color = (uint8_t)((color & 0x0FU) | (cap << 4U));
    return color;
}

static void cap_palette(uint8_t destination[16],
                        const uint8_t source[16],
                        unsigned cap)
{
    for (size_t i = 0U; i < 16U; ++i)
        destination[i] = brightness_cap(source[i], cap);
}

bool tecmo_team_data_draw(TecmoFramebuffer *framebuffer,
                          const TecmoTeamDataAsset *asset,
                          const TecmoTeamDataState *state,
                          const uint8_t *chr_bytes,
                          uint64_t chr_byte_count,
                          int origin_x,
                          int origin_y,
                          int scale)
{
    TecmoFramebuffer view;
    TecmoTeamDataState display_state;
    TecmoTeamDataPhase phase;
    const uint8_t *stable_palette;
    uint8_t palette[16];
    uint8_t sprite_palette[16];
    unsigned palette_stage;
    size_t screen;
    if (!state_valid(asset, state) ||
        !tecmo_team_data_asset_chr_available(asset, chr_bytes, chr_byte_count) ||
        !make_viewport(&view, framebuffer, origin_x, origin_y, scale))
        return false;
    if (!tecmo_team_data_transition_render_enabled(asset, state)) {
        fill_viewport(&view, tecmo_nes_2c02_rgba(0x0FU));
        return true;
    }
    phase = transition_display_phase(asset, state);
    display_state = *state;
    display_state.phase = phase;
    screen = phase == TECMO_TEAM_DATA_TEAM_SELECT ? 0U :
             (phase == TECMO_TEAM_DATA_PLAYER_DETAIL ? 2U : 1U);
    stable_palette = screen == 1U
        ? asset->profile_palettes[
              asset->teams[state->team_id].profile_palette_group]
        : asset->palettes[screen];
    palette_stage = tecmo_team_data_transition_palette_stage(asset, state);
    if (palette_stage >= 4U) {
        fill_viewport(&view, tecmo_nes_2c02_rgba(0x0FU));
        return true;
    }
    cap_palette(palette, stable_palette, palette_stage);
    cap_palette(sprite_palette, asset->sprite_palette, palette_stage);
    draw_screen(&view, asset, screen, palette,
                chr_bytes, chr_byte_count, scale);
    if (phase == TECMO_TEAM_DATA_TEAM_SELECT) {
        const TecmoTeamDataSelector *selector =
            &asset->selectors[state->selector_index];
        if (state->transition == TECMO_TEAM_DATA_TRANSITION_NONE &&
            state->cursor_delay > 0U)
            draw_cursor(&view, &asset->cursors[0], sprite_palette, chr_bytes,
                        chr_byte_count, selector->x, selector->y, scale);
        return true;
    }
    if (phase == TECMO_TEAM_DATA_PLAYER_DETAIL) {
        draw_player_detail(&view, asset, &display_state, palette,
                           chr_bytes, chr_byte_count, scale);
        return true;
    }
    draw_team_profile_text(&view, asset, &display_state, palette,
                           chr_bytes, chr_byte_count, scale);
    if (phase == TECMO_TEAM_DATA_PROFILE) {
        draw_roster_page(&view, asset, &display_state, palette, 0U, 0, chr_bytes,
                         chr_byte_count, scale);
        if (state->transition == TECMO_TEAM_DATA_TRANSITION_NONE &&
            state->cursor_delay > 0U)
            draw_cursor(&view, &asset->cursors[1], sprite_palette, chr_bytes,
                        chr_byte_count, asset->profile_cursor_x,
                        asset->profile_cursor_y +
                            state->profile_selection * asset->profile_cursor_stride,
                        scale);
        return true;
    }
    if (state->slide_direction == 0) {
        draw_roster_page(&view, asset, &display_state, palette,
                         state->roster_page, 0,
                         chr_bytes, chr_byte_count, scale);
        if (state->transition == TECMO_TEAM_DATA_TRANSITION_NONE &&
            state->cursor_delay > 0U)
            draw_cursor(&view, &asset->cursors[1], sprite_palette, chr_bytes,
                        chr_byte_count, asset->roster_cursor_x,
                        asset->roster_cursor_y +
                            state->roster_row * asset->roster_cursor_stride,
                        scale);
    } else {
        int distance = (int)state->slide_frame * asset->slide_pixels_per_frame;
        int outgoing = state->slide_direction > 0 ? -distance : distance;
        int incoming = state->slide_direction > 0 ? 256 - distance : -256 + distance;
        draw_roster_page(&view, asset, &display_state, palette,
                         state->slide_from_page, outgoing,
                         chr_bytes, chr_byte_count, scale);
        draw_roster_page(&view, asset, &display_state, palette,
                         state->slide_to_page, incoming,
                         chr_bytes, chr_byte_count, scale);
    }
    return true;
}
