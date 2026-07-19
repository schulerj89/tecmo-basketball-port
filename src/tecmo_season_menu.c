#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_season_menu.h"

#include "tecmo_asset_pack.h"
#include "tecmo_nes_video.h"

#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <sys/stat.h>
#endif

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#endif

#define SEASON_ENTRY_ID "menu/season"
#define SEASON_PAYLOAD_SIZE 104732U
#define SEASON_PAYLOAD_FNV1A32 0x29C64F84U
#define SEASON_CHR_SIZE 262144U
#define SEASON_CHR_FNV1A32 0xF6F6E854U
#define SEASON_CHR_FNV1A64 0x96A64F53B240ABB4ULL
#define TEAM_DATA_ENTRY_ID "menu/team-data"
#define TEAM_DATA_PAYLOAD_SIZE 96372U
#define TEAM_DATA_PAYLOAD_FNV1A32 0x812628F0U
#define SEASON_HEADER_SIZE 256U
#define SEASON_CELLS_OFFSET 256U
#define SEASON_PALETTES_OFFSET 57856U
#define SEASON_FONT_OFFSET 57936U
#define SEASON_CURSOR_OFFSET 58408U
#define SEASON_SPRITE_PALETTE_OFFSET 58424U
#define SEASON_SCHEDULE_OFFSET 58440U
#define SEASON_CONTROL_CELLS_OFFSET 60654U
#define SEASON_LEADERS_OFFSET 60726U
#define SEASON_MENU_TEXT_OFFSET 60866U
#define SEASON_MENU_TEXT_SIZE 344U
#define SEASON_DIVISION_STARTS_OFFSET 61210U
#define SEASON_DIVISION_TEAMS_OFFSET 61214U
#define SEASON_LEADER_NAV_OFFSET 61241U
#define SEASON_LEADER_TEMPLATE_OFFSET 61269U
#define SEASON_LEADER_SCREEN_CELLS_OFFSET 61276U
#define SEASON_LEADER_PALETTES_OFFSET 101596U
#define SEASON_POPUP_DESCS_OFFSET 101708U
#define SEASON_POPUP_CELLS_OFFSET 101756U
#define SEASON_SAVE_SIZE 108U
#define SEASON_SAVE_HEADER_SIZE 24U
#define SEASON_SAVE_PAYLOAD_SIZE 84U
#define SEASON_DIRECTION_REPEAT 8U

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

static void write_u16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8U);
}

static void write_u32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8U);
    p[2] = (uint8_t)(value >> 16U);
    p[3] = (uint8_t)(value >> 24U);
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

static bool read_dependencies(const char *root,
                              uint8_t **payload,
                              uint64_t *payload_count,
                              uint8_t **chr,
                              uint64_t *chr_count,
                              uint8_t **team_data,
                              uint64_t *team_data_count)
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
        if (tecmo_asset_pack_read_entry_exact(
                paths[i], SEASON_ENTRY_ID, SEASON_PAYLOAD_SIZE,
                payload, payload_count) != 0)
            return false;
        if (tecmo_asset_pack_read_entry_exact(
                paths[i], "chr/all", SEASON_CHR_SIZE,
                chr, chr_count) != 0 ||
            tecmo_asset_pack_read_entry_exact(
                paths[i], TEAM_DATA_ENTRY_ID, TEAM_DATA_PAYLOAD_SIZE,
                team_data, team_data_count) != 0) {
            tecmo_asset_pack_free(*payload);
            tecmo_asset_pack_free(*chr);
            *payload = NULL;
            *chr = NULL;
            *payload_count = 0U;
            *chr_count = 0U;
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
           cell->chr_offset == bg_chr_offset(cell->tile_id, r0, r1) &&
           cell->chr_offset + 16U <= SEASON_CHR_SIZE;
}

static bool schedule_record_selected(uint8_t season_type,
                                     const uint8_t record[2]);

static bool parse_padded_text(char *destination,
                              size_t destination_size,
                              const uint8_t *source,
                              size_t source_size)
{
    size_t length = 0U;
    if (destination == NULL || source == NULL ||
        destination_size != source_size || source_size == 0U)
        return false;
    while (length < source_size && source[length] != 0U) {
        if (source[length] < 0x20U || source[length] > 0x7EU) return false;
        ++length;
    }
    if (length == source_size) return false;
    for (size_t i = length + 1U; i < source_size; ++i)
        if (source[i] != 0U) return false;
    memcpy(destination, source, source_size);
    return true;
}

static bool parse_payload(TecmoSeasonAsset *asset,
                          const uint8_t *bytes,
                          uint64_t count)
{
    static const uint8_t screen_ids[5] = {17U, 18U, 19U, 20U, 21U};
    static const uint8_t page_counts[5] = {2U, 1U, 2U, 1U, 2U};
    static const uint8_t r0[5] = {0xFAU, 0xDAU, 0xFAU, 0x76U, 0x76U};
    static const uint8_t r1[5] = {0xFAU, 0xFAU, 0xFAU, 0xFAU, 0xFAU};
    static const uint8_t control_tiles[4][3] = {
        {0x56U, 0x57U, 0x58U}, {0x54U, 0x55U, 0x51U},
        {0x51U, 0x52U, 0x53U}, {0x54U, 0x55U, 0x52U}
    };
    static const uint8_t leader_screen_ids[7] = {
        38U, 39U, 40U, 41U, 42U, 43U, 44U
    };
    static const uint8_t leader_selectors[14] = {
        0xFAU,0xFAU, 0xFAU,0x2EU, 0xFAU,0x2EU, 0xFAU,0x2EU,
        0xFAU,0x2EU, 0xFAU,0x2EU, 0xFAU,0x2EU
    };

    if (bytes == NULL || count != SEASON_PAYLOAD_SIZE ||
        fnv1a32(bytes, count) != SEASON_PAYLOAD_FNV1A32 ||
        memcmp(bytes, "TSNS", 4U) != 0 ||
        read_u16(bytes + 4U) != 1U ||
        read_u16(bytes + 6U) != SEASON_HEADER_SIZE ||
        read_u16(bytes + 8U) != 32U || read_u16(bytes + 10U) != 30U ||
        read_u16(bytes + 12U) != 5U || read_u16(bytes + 14U) != 1920U ||
        read_u16(bytes + 16U) != 59U || read_u16(bytes + 18U) != 1107U ||
        read_u32(bytes + 20U) != SEASON_CELLS_OFFSET ||
        read_u32(bytes + 24U) != SEASON_PALETTES_OFFSET ||
        read_u32(bytes + 28U) != SEASON_FONT_OFFSET ||
        read_u32(bytes + 32U) != SEASON_CURSOR_OFFSET ||
        read_u32(bytes + 36U) != SEASON_SPRITE_PALETTE_OFFSET ||
        read_u32(bytes + 40U) != SEASON_SCHEDULE_OFFSET ||
        read_u32(bytes + 44U) != SEASON_CONTROL_CELLS_OFFSET ||
        read_u32(bytes + 48U) != SEASON_LEADERS_OFFSET ||
        read_u32(bytes + 52U) != SEASON_PAYLOAD_SIZE ||
        read_u32(bytes + 56U) != SEASON_CHR_SIZE ||
        read_u32(bytes + 60U) != SEASON_CHR_FNV1A32 ||
        memcmp(bytes + 64U, page_counts, sizeof(page_counts)) != 0 ||
        memcmp(bytes + 69U, screen_ids, sizeof(screen_ids)) != 0 ||
        memcmp(bytes + 74U,
               "\xFA\xFA\xDA\xFA\xFA\xFA\x76\xFA\x76\xFA", 10U) != 0 ||
        bytes[84U] != 8U || bytes[85U] != 5U || bytes[86U] != 27U ||
        bytes[87U] != 4U || bytes[88U] != 4U || bytes[89U] != 2U ||
        bytes[90U] != 8U || bytes[91U] != 7U ||
        read_u16(bytes + 92U) != 1107U || read_u16(bytes + 94U) != 567U ||
        read_u16(bytes + 96U) != 351U || read_u16(bytes + 98U) != 1107U ||
        read_u16(bytes + 100U) != 0x8599U ||
        read_u16(bytes + 102U) != 0xB27FU ||
        read_u16(bytes + 104U) != 1U || bytes[105U] != 0U ||
        bytes[106U] != 40U || bytes[107U] != 120U || bytes[108U] != 200U ||
        bytes[109U] != 80U || bytes[110U] != 16U || bytes[111U] != 0U ||
        read_u32(bytes + 112U) != SEASON_MENU_TEXT_OFFSET ||
        read_u32(bytes + 116U) != SEASON_DIVISION_STARTS_OFFSET ||
        read_u32(bytes + 120U) != SEASON_DIVISION_TEAMS_OFFSET ||
        read_u32(bytes + 124U) != SEASON_LEADER_NAV_OFFSET ||
        read_u32(bytes + 128U) != SEASON_LEADER_TEMPLATE_OFFSET ||
        read_u32(bytes + 132U) != SEASON_LEADER_SCREEN_CELLS_OFFSET ||
        read_u32(bytes + 136U) != SEASON_LEADER_PALETTES_OFFSET ||
        read_u16(bytes + 140U) != TECMO_SEASON_LEADER_SCREEN_COUNT ||
        read_u16(bytes + 142U) != TECMO_SEASON_LEADER_SCREEN_CELLS ||
        read_u16(bytes + 144U) != SEASON_MENU_TEXT_SIZE ||
        bytes[146U] != 0U || bytes[147U] != 0U ||
        memcmp(bytes + 148U, leader_screen_ids,
               sizeof(leader_screen_ids)) != 0 ||
        memcmp(bytes + 155U, leader_selectors,
               sizeof(leader_selectors)) != 0)
        return false;
    if (bytes[169U] < 0x20U ||
        bytes[169U] >= 0x20U + TECMO_SEASON_FONT_COUNT ||
        bytes[170U] < 0x20U ||
        bytes[170U] >= 0x20U + TECMO_SEASON_FONT_COUNT ||
        bytes[171U] != 0U || bytes[172U] != 0U)
        return false;
    for (size_t box = 0U; box < 3U; ++box) {
        const uint8_t *record = bytes + 173U + box * 4U;
        if (record[0] == 0U || record[1] == 0U ||
            (unsigned)record[0] + record[2] > 32U ||
            (unsigned)record[1] + record[3] > 30U)
            return false;
    }
    if (read_u32(bytes + 185U) != SEASON_POPUP_DESCS_OFFSET ||
        read_u32(bytes + 189U) != SEASON_POPUP_CELLS_OFFSET ||
        read_u16(bytes + 193U) != TECMO_SEASON_POPUP_CELL_COUNT ||
        bytes[195U] != 3U || bytes[196U] != 16U)
        return false;
    {
        static const uint8_t option_counts[3] = {2U, 2U, 4U};
        size_t input = 197U;
        for (size_t menu = 0U; menu < 3U; ++menu)
            for (size_t option = 0U; option < option_counts[menu]; ++option) {
                uint8_t x = bytes[input++];
                uint8_t y = bytes[input++];
                if (x > 247U || y > 223U) return false;
                asset->popup_cursor_x[menu][option] = x;
                asset->popup_cursor_y[menu][option] = y;
            }
    }
    if (bytes[213U] != 0xFFU || bytes[219U] != 0x00U ||
        bytes[225U] != 0xF8U ||
        !parse_cell(&asset->zero_games_behind_cells[0], bytes + 213U,
                    0xFAU, 0xFAU) ||
        !parse_cell(&asset->zero_games_behind_cells[1], bytes + 219U,
                    0xFAU, 0xFAU) ||
        !parse_cell(&asset->half_game_cell, bytes + 225U, 0xFAU, 0xFAU))
        return false;
    for (size_t i = 231U; i < SEASON_HEADER_SIZE; ++i)
        if (bytes[i] != 0U) return false;

    for (size_t screen = 0U; screen < TECMO_SEASON_SCREEN_COUNT; ++screen) {
        for (size_t cell = 0U; cell < TECMO_SEASON_SCREEN_CELLS; ++cell)
            if (!parse_cell(
                    &asset->screens[screen][cell],
                    bytes + SEASON_CELLS_OFFSET +
                        (screen * TECMO_SEASON_SCREEN_CELLS + cell) * 6U,
                    r0[screen], r1[screen]))
                return false;
        for (size_t color = 0U; color < 16U; ++color) {
            uint8_t value = bytes[SEASON_PALETTES_OFFSET + screen * 16U + color];
            if (value > 0x3FU) return false;
            asset->palettes[screen][color] = value;
        }
    }
    for (size_t i = 0U; i < TECMO_SEASON_FONT_COUNT; ++i) {
        const uint8_t *record = bytes + SEASON_FONT_OFFSET + i * 8U;
        if (record[0] != 0x20U + i || record[2] != 0xFAU ||
            record[3] != 0xFAU ||
            read_u32(record + 4U) != bg_chr_offset(record[1], 0xFAU, 0xFAU))
            return false;
        asset->font[i].tile_id = record[1];
        asset->font[i].palette_index = 0U;
        asset->font[i].chr_offset = read_u32(record + 4U);
    }
    asset->cursor.dx = read_i16(bytes + SEASON_CURSOR_OFFSET);
    asset->cursor.dy = read_i16(bytes + SEASON_CURSOR_OFFSET + 2U);
    asset->cursor.top_chr_offset = read_u32(bytes + SEASON_CURSOR_OFFSET + 4U);
    asset->cursor.bottom_chr_offset = read_u32(bytes + SEASON_CURSOR_OFFSET + 8U);
    asset->cursor.raw_selector = bytes[SEASON_CURSOR_OFFSET + 12U];
    asset->cursor.raw_tile = bytes[SEASON_CURSOR_OFFSET + 13U];
    if (asset->cursor.dx != 0 || asset->cursor.dy != -4 ||
        asset->cursor.top_chr_offset != 0xC240U ||
        asset->cursor.bottom_chr_offset != 0xC250U ||
        asset->cursor.raw_selector != 0x30U ||
        asset->cursor.raw_tile != 0x24U ||
        bytes[SEASON_CURSOR_OFFSET + 14U] != 0U ||
        bytes[SEASON_CURSOR_OFFSET + 15U] != 0U)
        return false;
    for (size_t i = 0U; i < 16U; ++i) {
        uint8_t value = bytes[SEASON_SPRITE_PALETTE_OFFSET + i];
        if (value > 0x3FU) return false;
        asset->sprite_palette[i] = value;
    }
    for (size_t game = 0U; game < TECMO_SEASON_SCHEDULE_COUNT; ++game) {
        for (size_t side = 0U; side < 2U; ++side) {
            uint8_t value = bytes[SEASON_SCHEDULE_OFFSET + game * 2U + side];
            if ((value & 0x3FU) >= TECMO_SEASON_TEAM_COUNT ||
                (value & 0x40U) != 0U)
                return false;
            asset->schedule[game][side] = value;
        }
    }
    for (uint8_t type = 0U; type < 4U; ++type) {
        uint16_t expected = read_u16(bytes + 92U + (size_t)type * 2U);
        uint16_t selected = 0U;
        uint8_t games_by_team[TECMO_SEASON_TEAM_COUNT] = {0};
        unsigned target = (unsigned)expected * 2U / TECMO_SEASON_TEAM_COUNT;
        for (size_t raw = 0U; raw < TECMO_SEASON_SCHEDULE_COUNT; ++raw) {
            if (!schedule_record_selected(type, asset->schedule[raw])) continue;
            ++selected;
            ++games_by_team[asset->schedule[raw][0] & 0x3FU];
            ++games_by_team[asset->schedule[raw][1] & 0x3FU];
        }
        if (selected != expected) return false;
        for (size_t team = 0U; team < TECMO_SEASON_TEAM_COUNT; ++team)
            if (games_by_team[team] != target) return false;
    }
    for (size_t mode = 0U; mode < 4U; ++mode)
        for (size_t glyph = 0U; glyph < 3U; ++glyph) {
            const uint8_t *record = bytes + SEASON_CONTROL_CELLS_OFFSET +
                                    (mode * 3U + glyph) * 6U;
            if (record[0] != control_tiles[mode][glyph] ||
                !parse_cell(&asset->control_cells[mode][glyph], record,
                            0x76U, 0xFAU))
                return false;
        }
    for (size_t i = 0U; i < TECMO_SEASON_LEADER_COUNT; ++i) {
        const char *source = (const char *)bytes + SEASON_LEADERS_OFFSET + i * 20U;
        size_t length = 0U;
        while (length < 20U && source[length] != '\0') {
            if ((unsigned char)source[length] < 0x20U ||
                (unsigned char)source[length] > 0x7EU)
                return false;
            ++length;
        }
        if (length == 0U || length == 20U)
            return false;
        for (size_t j = length + 1U; j < 20U; ++j)
            if (source[j] != '\0') return false;
        memcpy(asset->leader_labels[i], source, 20U);
    }
    asset->overtime_text[0] = (char)bytes[169U];
    asset->overtime_text[1] = (char)bytes[170U];
    asset->overtime_text[2] = '\0';
    memcpy(asset->menu_boxes, bytes + 173U, sizeof(asset->menu_boxes));
    {
        const uint8_t *source = bytes + SEASON_MENU_TEXT_OFFSET;
        for (size_t i = 0U; i < 3U; ++i)
            if (!parse_padded_text(asset->schedule_text[i], 24U,
                                   source + i * 24U, 24U))
                return false;
        source += 3U * 24U;
        for (size_t i = 0U; i < 6U; ++i)
            if (!parse_padded_text(asset->reset_text[i], 32U,
                                   source + i * 32U, 32U))
                return false;
        source += 6U * 32U;
        for (size_t i = 0U; i < 5U; ++i)
            if (!parse_padded_text(asset->season_type_text[i], 16U,
                                   source + i * 16U, 16U))
                return false;
    }
    memcpy(asset->division_starts,
           bytes + SEASON_DIVISION_STARTS_OFFSET, 4U);
    if (asset->division_starts[0] != 0U) return false;
    for (size_t division = 1U; division < 4U; ++division)
        if (asset->division_starts[division] <=
                asset->division_starts[division - 1U] ||
            asset->division_starts[division] >= TECMO_SEASON_TEAM_COUNT)
            return false;
    {
        bool seen[TECMO_SEASON_TEAM_COUNT] = {false};
        for (size_t i = 0U; i < TECMO_SEASON_TEAM_COUNT; ++i) {
            uint8_t team = bytes[SEASON_DIVISION_TEAMS_OFFSET + i];
            if (team >= TECMO_SEASON_TEAM_COUNT || seen[team]) return false;
            seen[team] = true;
            asset->division_teams[i] = team;
        }
    }
    for (size_t i = 0U; i < TECMO_SEASON_LEADER_COUNT; ++i) {
        asset->leader_up[i] = bytes[SEASON_LEADER_NAV_OFFSET + i];
        asset->leader_down[i] = bytes[SEASON_LEADER_NAV_OFFSET + 7U + i];
        asset->leader_cursor_x[i] = bytes[SEASON_LEADER_NAV_OFFSET + 14U + i];
        asset->leader_cursor_y[i] = bytes[SEASON_LEADER_NAV_OFFSET + 21U + i];
        asset->leader_template[i] = bytes[SEASON_LEADER_TEMPLATE_OFFSET + i];
        if (asset->leader_up[i] >= TECMO_SEASON_LEADER_COUNT ||
            asset->leader_down[i] >= TECMO_SEASON_LEADER_COUNT ||
            asset->leader_template[i] < 39U ||
            asset->leader_template[i] > 44U)
            return false;
    }
    for (size_t screen = 0U;
         screen < TECMO_SEASON_LEADER_SCREEN_COUNT; ++screen) {
        uint8_t leader_r0 = bytes[155U + screen * 2U];
        uint8_t leader_r1 = bytes[156U + screen * 2U];
        for (size_t cell = 0U; cell < TECMO_SEASON_LEADER_SCREEN_CELLS; ++cell)
            if (!parse_cell(
                    &asset->leader_screens[screen][cell],
                    bytes + SEASON_LEADER_SCREEN_CELLS_OFFSET +
                        (screen * TECMO_SEASON_LEADER_SCREEN_CELLS + cell) * 6U,
                    leader_r0, leader_r1))
                return false;
        for (size_t color = 0U; color < 16U; ++color) {
            uint8_t value = bytes[SEASON_LEADER_PALETTES_OFFSET +
                                  screen * 16U + color];
            if (value > 0x3FU) return false;
            asset->leader_palettes[screen][color] = value;
        }
    }
    {
        size_t expected_start = 0U;
        for (size_t overlay = 0U; overlay < 3U; ++overlay) {
            const uint8_t *desc = bytes + SEASON_POPUP_DESCS_OFFSET +
                                  overlay * 16U;
            TecmoSeasonPopupOverlay *target = &asset->popup_overlays[overlay];
            target->cell_start = read_u16(desc);
            target->cell_count = read_u16(desc + 2U);
            target->width = read_u16(desc + 4U);
            target->height = read_u16(desc + 6U);
            target->origin_x = read_u16(desc + 8U);
            target->origin_y = read_u16(desc + 10U);
            if (target->cell_start != expected_start ||
                target->cell_count != target->width * target->height ||
                target->width != asset->menu_boxes[overlay][0] ||
                target->height != asset->menu_boxes[overlay][1] ||
                target->origin_x != asset->menu_boxes[overlay][2] ||
                target->origin_y != asset->menu_boxes[overlay][3] ||
                target->origin_x + target->width > 32U ||
                target->origin_y + target->height > 30U ||
                desc[12U] != overlay || desc[13U] != 0U ||
                desc[14U] != 0U || desc[15U] != 0U ||
                expected_start + target->cell_count >
                    TECMO_SEASON_POPUP_CELL_COUNT)
                return false;
            expected_start += target->cell_count;
        }
        if (expected_start != TECMO_SEASON_POPUP_CELL_COUNT) return false;
        for (size_t cell = 0U; cell < TECMO_SEASON_POPUP_CELL_COUNT; ++cell)
            if (!parse_cell(&asset->popup_cells[cell],
                            bytes + SEASON_POPUP_CELLS_OFFSET + cell * 6U,
                            0x76U, 0xFAU))
                return false;
    }
    for (size_t i = 0U; i < 4U; ++i)
        asset->game_counts[i] = read_u16(bytes + 92U + i * 2U);
    memcpy(asset->team_control_x, bytes + 106U, 3U);
    asset->team_control_y = bytes[109U];
    asset->team_control_stride = bytes[110U];
    asset->game_launch_boundary_cpu = read_u16(bytes + 100U);
    asset->game_launch_target_cpu = read_u16(bytes + 102U);
    asset->expected_chr_size = read_u32(bytes + 56U);
    asset->expected_chr_fingerprint32 = read_u32(bytes + 60U);
    return true;
}

bool tecmo_season_asset_load(TecmoSeasonAsset *asset,
                             const char *project_root)
{
    uint8_t *payload = NULL;
    uint8_t *chr = NULL;
    uint8_t *team_data = NULL;
    uint64_t payload_count = 0U;
    uint64_t chr_count = 0U;
    uint64_t team_data_count = 0U;
    bool ok = false;
    if (asset == NULL) return false;
    memset(asset, 0, sizeof(*asset));
    if (!read_dependencies(project_root, &payload, &payload_count,
                           &chr, &chr_count, &team_data, &team_data_count)) {
        (void)snprintf(asset->status, sizeof(asset->status),
                       "TSNS-1/TTDT-1/chr runtime entries missing");
        goto cleanup;
    }
    if (chr_count != SEASON_CHR_SIZE ||
        fnv1a32(chr, chr_count) != SEASON_CHR_FNV1A32 ||
        fnv1a64(chr, chr_count) != SEASON_CHR_FNV1A64 ||
        team_data_count != TEAM_DATA_PAYLOAD_SIZE ||
        fnv1a32(team_data, team_data_count) != TEAM_DATA_PAYLOAD_FNV1A32 ||
        memcmp(team_data, "TTDT", 4U) != 0 ||
        !parse_payload(asset, payload, payload_count)) {
        (void)snprintf(asset->status, sizeof(asset->status),
                       "TSNS-1 strict pack contract rejected");
        goto cleanup;
    }
    asset->chr_fingerprint64 = fnv1a64(chr, chr_count);
    asset->available = true;
    (void)snprintf(asset->status, sizeof(asset->status),
                   "strict ROM TSNS-1 ready; gameplay boundary $8599 blocked");
    ok = true;

cleanup:
    tecmo_asset_pack_free(payload);
    tecmo_asset_pack_free(chr);
    tecmo_asset_pack_free(team_data);
    return ok;
}

bool tecmo_season_asset_chr_available(const TecmoSeasonAsset *asset,
                                      const uint8_t *chr_bytes,
                                      uint64_t chr_byte_count)
{
    return asset != NULL && asset->available && chr_bytes != NULL &&
           chr_byte_count == asset->expected_chr_size &&
           fnv1a32(chr_bytes, chr_byte_count) == asset->expected_chr_fingerprint32 &&
           fnv1a64(chr_bytes, chr_byte_count) == asset->chr_fingerprint64;
}

static void session_defaults(TecmoSeasonSession *session)
{
    session->season_type = TECMO_SEASON_REGULAR;
    memset(session->team_control, 0, sizeof(session->team_control));
    memset(session->wins, 0, sizeof(session->wins));
    memset(session->losses, 0, sizeof(session->losses));
    session->schedule_index = 0U;
    session->dirty = false;
}

static uint16_t season_game_count(uint8_t season_type)
{
    static const uint16_t counts[4] = {1107U, 567U, 351U, 1107U};
    return season_type < 4U ? counts[season_type] : 0U;
}

static bool parse_save(TecmoSeasonSession *session,
                       const uint8_t bytes[SEASON_SAVE_SIZE])
{
    const uint8_t *payload = bytes + SEASON_SAVE_HEADER_SIZE;
    if (memcmp(bytes, "TSAV", 4U) != 0 || read_u16(bytes + 4U) != 1U ||
        read_u16(bytes + 6U) != SEASON_SAVE_HEADER_SIZE ||
        read_u32(bytes + 8U) != SEASON_SAVE_SIZE ||
        read_u32(bytes + 12U) != fnv1a32(payload, SEASON_SAVE_PAYLOAD_SIZE))
        return false;
    for (size_t i = 16U; i < SEASON_SAVE_HEADER_SIZE; ++i)
        if (bytes[i] != 0U) return false;
    if (payload[0] >= 4U) return false;
    for (size_t team = 0U; team < TECMO_SEASON_TEAM_COUNT; ++team) {
        if (payload[1U + team] >= 4U || payload[28U + team] > 82U ||
            payload[55U + team] > 82U ||
            payload[28U + team] + payload[55U + team] > 82U)
            return false;
    }
    if (read_u16(payload + 82U) > season_game_count(payload[0])) return false;
    session->season_type = payload[0];
    memcpy(session->team_control, payload + 1U, TECMO_SEASON_TEAM_COUNT);
    memcpy(session->wins, payload + 28U, TECMO_SEASON_TEAM_COUNT);
    memcpy(session->losses, payload + 55U, TECMO_SEASON_TEAM_COUNT);
    session->schedule_index = read_u16(payload + 82U);
    session->dirty = false;
    return true;
}

typedef enum SeasonPathState {
    SEASON_PATH_MISSING = 0,
    SEASON_PATH_PRESENT,
    SEASON_PATH_ERROR
} SeasonPathState;

static SeasonPathState season_path_state(const char *path)
{
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(path);
    if (attributes != INVALID_FILE_ATTRIBUTES) return SEASON_PATH_PRESENT;
    {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
            return SEASON_PATH_MISSING;
    }
    return SEASON_PATH_ERROR;
#else
    struct stat info;
    if (stat(path, &info) == 0) return SEASON_PATH_PRESENT;
    return errno == ENOENT ? SEASON_PATH_MISSING : SEASON_PATH_ERROR;
#endif
}

void tecmo_season_session_init(TecmoSeasonSession *session,
                               const char *project_root)
{
    uint8_t bytes[SEASON_SAVE_SIZE];
    char legacy_path[1024];
    FILE *file;
    size_t got;
    int trailing;
    bool migrated = false;
    SeasonPathState new_path_state;
    if (session == NULL) return;
    memset(session, 0, sizeof(*session));
    session_defaults(session);
    session->save_status = TECMO_SEASON_SAVE_ROM_DEFAULTS;
    (void)snprintf(session->status, sizeof(session->status),
                   "ROM clean defaults");
    if (make_path(session->save_path, sizeof(session->save_path), project_root,
                  "saves\\tecmo-season.sav") != 0 ||
        make_path(legacy_path, sizeof(legacy_path), project_root,
                  "build\\tecmo-season.sav") != 0) {
        session->save_status = TECMO_SEASON_SAVE_IO_ERROR;
        (void)snprintf(session->status, sizeof(session->status),
                       "season save path unavailable");
        return;
    }
    new_path_state = season_path_state(session->save_path);
    if (new_path_state == SEASON_PATH_PRESENT) {
        file = fopen(session->save_path, "rb");
        if (file == NULL) {
            session->save_status = TECMO_SEASON_SAVE_IO_ERROR;
            (void)snprintf(session->status, sizeof(session->status),
                           "existing TSAV-1 could not be read; ROM defaults active");
            return;
        }
    } else if (new_path_state == SEASON_PATH_ERROR) {
        session->save_status = TECMO_SEASON_SAVE_IO_ERROR;
        (void)snprintf(session->status, sizeof(session->status),
                       "TSAV-1 existence check failed; ROM defaults active");
        return;
    } else {
        SeasonPathState legacy_state = season_path_state(legacy_path);
        if (legacy_state == SEASON_PATH_MISSING) return;
        if (legacy_state == SEASON_PATH_ERROR ||
            (file = fopen(legacy_path, "rb")) == NULL) {
            session->save_status = TECMO_SEASON_SAVE_IO_ERROR;
            (void)snprintf(session->status, sizeof(session->status),
                           "legacy TSAV-1 could not be read; ROM defaults active");
            return;
        }
        migrated = true;
    }
    got = fread(bytes, 1U, sizeof(bytes), file);
    trailing = fgetc(file);
    fclose(file);
    if (got != sizeof(bytes) || trailing != EOF || !parse_save(session, bytes)) {
        session_defaults(session);
        session->save_status = TECMO_SEASON_SAVE_REJECTED;
        (void)snprintf(session->status, sizeof(session->status),
                       "malformed TSAV-1 rejected; ROM defaults active");
        return;
    }
    session->save_status = TECMO_SEASON_SAVE_LOADED;
    if (migrated) {
        session->dirty = true;
        if (!tecmo_season_session_save(session)) {
            session->save_status = TECMO_SEASON_SAVE_IO_ERROR;
            (void)snprintf(session->status, sizeof(session->status),
                           "legacy TSAV-1 loaded; migration failed");
            return;
        }
        session->save_status = TECMO_SEASON_SAVE_LOADED;
        (void)snprintf(session->status, sizeof(session->status),
                       "strict TSAV-1 loaded and migrated");
    } else {
        (void)snprintf(session->status, sizeof(session->status),
                       "strict TSAV-1 loaded");
    }
}

bool tecmo_season_session_save(TecmoSeasonSession *session)
{
    uint8_t bytes[SEASON_SAVE_SIZE] = {0};
    uint8_t *payload = bytes + SEASON_SAVE_HEADER_SIZE;
    char temp_path[1060];
    char parent_path[1024];
    FILE *file;
    int written;
    if (session == NULL || session->save_path[0] == '\0' ||
        session->season_type >= 4U ||
        session->schedule_index > season_game_count(session->season_type))
        return false;
    for (size_t team = 0U; team < TECMO_SEASON_TEAM_COUNT; ++team)
        if (session->team_control[team] >= 4U || session->wins[team] > 82U ||
            session->losses[team] > 82U ||
            session->wins[team] + session->losses[team] > 82U)
            return false;
    memcpy(bytes, "TSAV", 4U);
    write_u16(bytes + 4U, 1U);
    write_u16(bytes + 6U, SEASON_SAVE_HEADER_SIZE);
    write_u32(bytes + 8U, SEASON_SAVE_SIZE);
    payload[0] = session->season_type;
    memcpy(payload + 1U, session->team_control, TECMO_SEASON_TEAM_COUNT);
    memcpy(payload + 28U, session->wins, TECMO_SEASON_TEAM_COUNT);
    memcpy(payload + 55U, session->losses, TECMO_SEASON_TEAM_COUNT);
    write_u16(payload + 82U, session->schedule_index);
    write_u32(bytes + 12U, fnv1a32(payload, SEASON_SAVE_PAYLOAD_SIZE));
    (void)snprintf(parent_path, sizeof(parent_path), "%s", session->save_path);
    {
        char *slash = strrchr(parent_path, '\\');
        if (slash != NULL) {
            *slash = '\0';
#ifdef _WIN32
            (void)_mkdir(parent_path);
#endif
        }
    }
    written = snprintf(temp_path, sizeof(temp_path), "%s.tmp", session->save_path);
    if (written < 0 || (size_t)written >= sizeof(temp_path)) return false;
    file = fopen(temp_path, "wb");
    if (file == NULL) {
        session->save_status = TECMO_SEASON_SAVE_IO_ERROR;
        (void)snprintf(session->status, sizeof(session->status),
                       "could not write TSAV-1");
        return false;
    }
    if (fwrite(bytes, 1U, sizeof(bytes), file) != sizeof(bytes) ||
        fflush(file) != 0) {
        (void)fclose(file);
        (void)remove(temp_path);
        session->save_status = TECMO_SEASON_SAVE_IO_ERROR;
        (void)snprintf(session->status, sizeof(session->status),
                       "could not write TSAV-1");
        return false;
    }
    if (fclose(file) != 0) {
        (void)remove(temp_path);
        session->save_status = TECMO_SEASON_SAVE_IO_ERROR;
        (void)snprintf(session->status, sizeof(session->status),
                       "could not write TSAV-1");
        return false;
    }
#ifdef _WIN32
    if (!MoveFileExA(temp_path, session->save_path,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
#else
    if (rename(temp_path, session->save_path) != 0) {
#endif
        (void)remove(temp_path);
        session->save_status = TECMO_SEASON_SAVE_IO_ERROR;
        (void)snprintf(session->status, sizeof(session->status),
                       "could not install TSAV-1");
        return false;
    }
    session->dirty = false;
    session->save_status = TECMO_SEASON_SAVE_SAVED;
    (void)snprintf(session->status, sizeof(session->status),
                   "strict TSAV-1 saved");
    return true;
}

void tecmo_season_state_init(TecmoSeasonState *state,
                             TecmoSeasonRoute route,
                             const TecmoSeasonSession *session)
{
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    switch (route) {
    case TECMO_SEASON_ROUTE_TEAM_CONTROL:
        state->phase = TECMO_SEASON_TEAM_CONTROL;
        break;
    case TECMO_SEASON_ROUTE_SCHEDULE:
        state->phase = TECMO_SEASON_SCHEDULE;
        break;
    case TECMO_SEASON_ROUTE_GAME_START:
        state->phase = TECMO_SEASON_GAME_START;
        break;
    case TECMO_SEASON_ROUTE_STANDINGS:
        state->phase = session != NULL &&
                       session->season_type == TECMO_SEASON_PROGRAMMED
                           ? TECMO_SEASON_PROGRAMMED_EDITOR
                           : TECMO_SEASON_STANDINGS;
        break;
    case TECMO_SEASON_ROUTE_LEADERS:
        state->phase = TECMO_SEASON_LEADERS;
        break;
    default:
        state->phase = TECMO_SEASON_TEAM_CONTROL;
        break;
    }
    if (session != NULL) {
        uint16_t count = season_game_count(session->season_type);
        state->schedule_selection = session->schedule_index < count
                                        ? session->schedule_index
                                        : (count == 0U ? 0U
                                                       : (uint16_t)(count - 1U));
        state->season_type_selection = session->season_type;
        state->season_complete = session->schedule_index == count;
        state->game_prepare_pending = route == TECMO_SEASON_ROUTE_GAME_START;
        state->game_launch_blocked = route == TECMO_SEASON_ROUTE_GAME_START;
    }
}

static bool controls_neutral(const TecmoInput *held)
{
    return !held->up && !held->down && !held->left && !held->right &&
           !held->confirm && !held->cancel && !held->shoot && !held->tab;
}

static bool accept_released(const TecmoControlFrame *controls)
{
    return controls_neutral(&controls->held) &&
           !controls->released.up && !controls->released.down &&
           !controls->released.left && !controls->released.right &&
           controls->released.shoot;
}

static bool cancel_released(const TecmoControlFrame *controls)
{
    return controls_neutral(&controls->held) &&
           !controls->released.up && !controls->released.down &&
           !controls->released.left && !controls->released.right &&
           controls->released.cancel;
}

typedef enum TecmoSeasonDirection {
    TECMO_SEASON_DIRECTION_NONE,
    TECMO_SEASON_DIRECTION_INVALID,
    TECMO_SEASON_DIRECTION_UP,
    TECMO_SEASON_DIRECTION_DOWN,
    TECMO_SEASON_DIRECTION_LEFT,
    TECMO_SEASON_DIRECTION_RIGHT
} TecmoSeasonDirection;

static TecmoSeasonDirection gated_direction(
    TecmoSeasonState *state, const TecmoControlFrame *controls)
{
    unsigned held = (controls->held.up ? 0x08U : 0U) |
                    (controls->held.down ? 0x04U : 0U) |
                    (controls->held.left ? 0x02U : 0U) |
                    (controls->held.right ? 0x01U : 0U);
    if (held == 0U || state->direction_cooldown != 0U)
        return TECMO_SEASON_DIRECTION_NONE;
    state->direction_cooldown = SEASON_DIRECTION_REPEAT;
    switch (held) {
    case 0x08U: return TECMO_SEASON_DIRECTION_UP;
    case 0x04U: return TECMO_SEASON_DIRECTION_DOWN;
    case 0x02U: return TECMO_SEASON_DIRECTION_LEFT;
    case 0x01U: return TECMO_SEASON_DIRECTION_RIGHT;
    default: return TECMO_SEASON_DIRECTION_INVALID;
    }
}

static TecmoSeasonAction finish_update(TecmoSeasonState *state,
                                       TecmoSeasonAction action,
                                       bool button_event)
{
    if (!button_event && state->direction_cooldown != 0U)
        --state->direction_cooldown;
    return action;
}

static uint8_t wrap_u8(uint8_t value, uint8_t count, int direction)
{
    if (direction < 0) return value == 0U ? (uint8_t)(count - 1U)
                                          : (uint8_t)(value - 1U);
    return value + 1U >= count ? 0U : (uint8_t)(value + 1U);
}

static uint16_t wrap_u16(uint16_t value, uint16_t count, int direction)
{
    if (direction < 0) return value == 0U ? (uint16_t)(count - 1U)
                                          : (uint16_t)(value - 1U);
    return value + 1U >= count ? 0U : (uint16_t)(value + 1U);
}

static void persist_change(TecmoSeasonSession *session)
{
    session->dirty = true;
    (void)tecmo_season_session_save(session);
}

static void reset_for_type(TecmoSeasonSession *session, uint8_t type)
{
    session_defaults(session);
    session->season_type = type;
    persist_change(session);
}

static bool valid_runtime_state(const TecmoSeasonAsset *asset,
                                const TecmoSeasonSession *session,
                                const TecmoSeasonState *state);

static size_t season_division_start(const TecmoSeasonAsset *asset,
                                    size_t division)
{
    return division < 4U ? asset->division_starts[division]
                         : TECMO_SEASON_TEAM_COUNT;
}

static size_t season_division_count(const TecmoSeasonAsset *asset,
                                    size_t division)
{
    return season_division_start(asset, division + 1U) -
           season_division_start(asset, division);
}

static uint8_t season_division_team(const TecmoSeasonAsset *asset,
                                    size_t division,
                                    size_t row)
{
    size_t end = season_division_start(asset, division + 1U);
    return asset->division_teams[end - 1U - row];
}

static uint8_t season_page_team_count(const TecmoSeasonAsset *asset,
                                      size_t page)
{
    return (uint8_t)(season_division_count(asset, page * 2U) +
                     season_division_count(asset, page * 2U + 1U));
}

static uint8_t season_page_team(const TecmoSeasonAsset *asset,
                                size_t page,
                                uint8_t index)
{
    size_t first_division = page * 2U;
    size_t first_count = season_division_count(asset, first_division);
    if (index < first_count)
        return season_division_team(asset, first_division, index);
    return season_division_team(asset, first_division + 1U,
                                (size_t)index - first_count);
}

static bool schedule_record_selected(uint8_t season_type,
                                     const uint8_t record[2])
{
    if (season_type == TECMO_SEASON_SHORT)
        return (record[0] & 0x80U) != 0U && (record[1] & 0x80U) != 0U;
    if (season_type == TECMO_SEASON_REDUCED)
        return (record[0] & 0x80U) != 0U;
    return season_type == TECMO_SEASON_REGULAR ||
           season_type == TECMO_SEASON_PROGRAMMED;
}

bool tecmo_season_schedule_raw_index(const TecmoSeasonAsset *asset,
                                     uint8_t season_type,
                                     uint16_t ordinal,
                                     uint16_t *raw_index)
{
    uint16_t selected = 0U;
    if (asset == NULL || raw_index == NULL || season_type >= 4U ||
        ordinal >= asset->game_counts[season_type])
        return false;
    for (uint16_t raw = 0U; raw < TECMO_SEASON_SCHEDULE_COUNT; ++raw) {
        if (!schedule_record_selected(season_type, asset->schedule[raw]))
            continue;
        if (selected == ordinal) {
            *raw_index = raw;
            return true;
        }
        ++selected;
    }
    return false;
}

static void prepare_game_start_boundary(TecmoSeasonState *state,
                                        const TecmoSeasonAsset *asset,
                                        const TecmoSeasonSession *session)
{
    uint16_t count = asset->game_counts[session->season_type];
    uint16_t raw;
    state->game_result_count = 0U;
    state->game_result_visible_rows = 0U;
    state->game_prepare_pending = false;
    state->game_result_pending = false;
    state->game_launch_blocked = true;
    state->season_complete = session->schedule_index >= count;
    memset(&state->pending_game, 0, sizeof(state->pending_game));
    if (state->season_complete ||
        !tecmo_season_schedule_raw_index(asset, session->season_type,
                                         session->schedule_index, &raw))
        return;
    state->pending_game.game_index = session->schedule_index;
    state->pending_game.away_team = asset->schedule[raw][0] & 0x3FU;
    state->pending_game.home_team = asset->schedule[raw][1] & 0x3FU;
    state->game_result_pending = true;
}

bool tecmo_season_commit_game_result(TecmoSeasonState *state,
                                     const TecmoSeasonAsset *asset,
                                     TecmoSeasonSession *session,
                                     const TecmoSeasonGameResult *result)
{
    uint8_t away_wins;
    uint8_t away_losses;
    uint8_t home_wins;
    uint8_t home_losses;
    uint8_t scheduled_away;
    uint8_t scheduled_home;
    uint16_t raw_index;
    uint16_t schedule_index;
    bool dirty;
    if (state == NULL || asset == NULL || session == NULL || result == NULL ||
        !asset->available || state->phase != TECMO_SEASON_GAME_START ||
        session->season_type >= 4U ||
        session->schedule_index >= asset->game_counts[session->season_type] ||
        !tecmo_season_schedule_raw_index(asset, session->season_type,
                                         session->schedule_index, &raw_index))
        return false;
    scheduled_away = asset->schedule[raw_index][0] & 0x3FU;
    scheduled_home = asset->schedule[raw_index][1] & 0x3FU;
    if (scheduled_away >= TECMO_SEASON_TEAM_COUNT ||
        scheduled_home >= TECMO_SEASON_TEAM_COUNT ||
        scheduled_away == scheduled_home ||
        !state->game_result_pending || state->game_prepare_pending ||
        result->game_index != state->pending_game.game_index ||
        state->pending_game.away_team != scheduled_away ||
        state->pending_game.home_team != scheduled_home ||
        result->away_team != scheduled_away ||
        result->home_team != scheduled_home ||
        result->away_team >= TECMO_SEASON_TEAM_COUNT ||
        result->home_team >= TECMO_SEASON_TEAM_COUNT ||
        result->away_team == result->home_team ||
        result->away_score > 999U || result->home_score > 999U ||
        result->away_score == result->home_score ||
        session->schedule_index != state->pending_game.game_index ||
        (unsigned)session->wins[result->away_team] +
                session->losses[result->away_team] >= 82U ||
        (unsigned)session->wins[result->home_team] +
                session->losses[result->home_team] >= 82U)
        return false;

    away_wins = session->wins[result->away_team];
    away_losses = session->losses[result->away_team];
    home_wins = session->wins[result->home_team];
    home_losses = session->losses[result->home_team];
    schedule_index = session->schedule_index;
    dirty = session->dirty;
    if (result->away_score > result->home_score) {
        ++session->wins[result->away_team];
        ++session->losses[result->home_team];
    } else {
        ++session->losses[result->away_team];
        ++session->wins[result->home_team];
    }
    ++session->schedule_index;
    session->dirty = true;
    if (!tecmo_season_session_save(session)) {
        session->wins[result->away_team] = away_wins;
        session->losses[result->away_team] = away_losses;
        session->wins[result->home_team] = home_wins;
        session->losses[result->home_team] = home_losses;
        session->schedule_index = schedule_index;
        session->dirty = dirty;
        return false;
    }

    state->game_results[0] = *result;
    state->game_result_count = 1U;
    state->game_result_visible_rows = 1U;
    state->game_result_pending = false;
    state->game_launch_blocked = false;
    state->schedule_selection = session->schedule_index <
                                    asset->game_counts[session->season_type]
                                    ? session->schedule_index
                                    : (uint16_t)(asset->game_counts[
                                          session->season_type] - 1U);
    state->season_complete = session->schedule_index ==
                             asset->game_counts[session->season_type];
    return true;
}

static int popup_box_index(TecmoSeasonPhase phase)
{
    switch (phase) {
    case TECMO_SEASON_SCHEDULE_POPUP: return 0;
    case TECMO_SEASON_RESET_CONFIRM: return 1;
    case TECMO_SEASON_TYPE_SELECT: return 2;
    default: return -1;
    }
}

static void begin_popup(TecmoSeasonState *state, TecmoSeasonPhase phase)
{
    state->phase = phase;
    state->popup_rows_visible = 0U;
    state->popup_animation_tick = 0U;
    state->popup_open_sync_frames = 1U;
    state->popup_closing = false;
}

static void close_popup(TecmoSeasonState *state, TecmoSeasonPhase target)
{
    state->popup_target_phase = target;
    state->popup_animation_tick = 0U;
    state->popup_open_sync_frames = 0U;
    state->popup_closing = true;
}

static bool update_popup_animation(TecmoSeasonState *state,
                                   const TecmoSeasonAsset *asset)
{
    int box_index = popup_box_index(state->phase);
    uint8_t height;
    if (box_index < 0) return false;
    height = asset->menu_boxes[box_index][1];
    if (!state->popup_closing && state->popup_rows_visible >= height)
        return false;
    if (!state->popup_closing && state->popup_open_sync_frames != 0U) {
        --state->popup_open_sync_frames;
        return true;
    }
    ++state->popup_animation_tick;
    if (state->popup_animation_tick < 10U) return true;
    state->popup_animation_tick = 0U;
    if (state->popup_closing) {
        if (state->popup_rows_visible != 0U)
            --state->popup_rows_visible;
        if (state->popup_rows_visible == 0U) {
            TecmoSeasonPhase target = state->popup_target_phase;
            state->popup_closing = false;
            if (target == TECMO_SEASON_SCHEDULE_POPUP) {
                state->phase = target;
                state->popup_rows_visible = asset->menu_boxes[0][1];
                state->popup_animation_tick = 0U;
                state->popup_open_sync_frames = 0U;
            } else if (popup_box_index(target) >= 0) {
                begin_popup(state, target);
            } else {
                state->phase = target;
            }
        }
    } else {
        ++state->popup_rows_visible;
    }
    return true;
}

TecmoSeasonAction tecmo_season_update(TecmoSeasonState *state,
                                      const TecmoSeasonAsset *asset,
                                      TecmoSeasonSession *session,
                                      const TecmoControlFrame *controls)
{
    TecmoSeasonDirection direction;
    bool accept;
    bool cancel;
    bool direction_consumed;
    if (state == NULL || asset == NULL || session == NULL || controls == NULL)
        return TECMO_SEASON_ACTION_NONE;
    ++state->frame;
    if (!asset->available || !valid_runtime_state(asset, session, state))
        return TECMO_SEASON_ACTION_NONE;
    if (update_popup_animation(state, asset))
        return finish_update(state, TECMO_SEASON_ACTION_NONE, false);
    direction = state->phase == TECMO_SEASON_LEADERS ||
                        state->phase == TECMO_SEASON_GAME_START
                    ? TECMO_SEASON_DIRECTION_NONE
                    : gated_direction(state, controls);
    direction_consumed = direction != TECMO_SEASON_DIRECTION_NONE;
    accept = !direction_consumed &&
             state->phase != TECMO_SEASON_PROGRAMMED_EDITOR &&
             state->phase != TECMO_SEASON_LEADERS &&
             accept_released(controls);
    cancel = !direction_consumed &&
             state->phase != TECMO_SEASON_PROGRAMMED_EDITOR &&
             state->phase != TECMO_SEASON_LEADERS &&
             cancel_released(controls);
    if (accept || cancel) state->direction_cooldown = 5U;

    switch (state->phase) {
    case TECMO_SEASON_TEAM_CONTROL:
        if (accept) {
            session->team_control[state->team_selection] =
                (uint8_t)((session->team_control[state->team_selection] + 1U) & 3U);
            persist_change(session);
            return finish_update(state, TECMO_SEASON_ACTION_NONE,
                                 accept || cancel);
        }
        if (cancel)
            return finish_update(state,
                                 TECMO_SEASON_ACTION_BACK_TO_START_MENU,
                                 accept || cancel);
        if (direction == TECMO_SEASON_DIRECTION_UP ||
            direction == TECMO_SEASON_DIRECTION_DOWN) {
            uint8_t column = (uint8_t)(state->team_selection / 9U);
            uint8_t row = (uint8_t)(state->team_selection % 9U);
            row = wrap_u8(row, 9U,
                          direction == TECMO_SEASON_DIRECTION_UP ? -1 : 1);
            state->team_selection = (uint8_t)(column * 9U + row);
        } else if (direction == TECMO_SEASON_DIRECTION_LEFT ||
                   direction == TECMO_SEASON_DIRECTION_RIGHT) {
            uint8_t column = (uint8_t)(state->team_selection / 9U);
            uint8_t row = (uint8_t)(state->team_selection % 9U);
            column = wrap_u8(column, 3U,
                             direction == TECMO_SEASON_DIRECTION_LEFT ? -1 : 1);
            state->team_selection = (uint8_t)(column * 9U + row);
        }
        break;

    case TECMO_SEASON_SCHEDULE:
        if (accept) {
            begin_popup(state, TECMO_SEASON_SCHEDULE_POPUP);
            state->popup_selection = 0U;
            break;
        }
        if (cancel)
            return finish_update(state,
                                 TECMO_SEASON_ACTION_BACK_TO_START_MENU,
                                 accept || cancel);
        if (direction == TECMO_SEASON_DIRECTION_UP ||
            direction == TECMO_SEASON_DIRECTION_DOWN) {
            uint16_t count = asset->game_counts[session->season_type];
            state->schedule_selection = wrap_u16(state->schedule_selection,
                count, direction == TECMO_SEASON_DIRECTION_UP ? -1 : 1);
        }
        break;

    case TECMO_SEASON_SCHEDULE_POPUP:
        if (accept) {
            if (state->popup_selection == 0U) {
                state->phase = TECMO_SEASON_PLAYOFF;
                state->playoff_scroll = 0U;
                state->popup_rows_visible = 0U;
                state->popup_animation_tick = 0U;
                state->popup_open_sync_frames = 0U;
                state->popup_closing = false;
            } else {
                state->popup_selection = 0U;
                begin_popup(state, TECMO_SEASON_RESET_CONFIRM);
            }
            break;
        }
        if (cancel) {
            close_popup(state, TECMO_SEASON_SCHEDULE);
            break;
        }
        if (direction == TECMO_SEASON_DIRECTION_UP ||
            direction == TECMO_SEASON_DIRECTION_DOWN)
            state->popup_selection = wrap_u8(
                state->popup_selection, 2U,
                direction == TECMO_SEASON_DIRECTION_UP ? -1 : 1);
        break;

    case TECMO_SEASON_PLAYOFF:
        if (controls->held.left != controls->held.right) {
            if (controls->held.left)
                state->playoff_scroll = state->playoff_scroll < 4U
                                            ? 0U
                                            : (uint16_t)(state->playoff_scroll - 4U);
            else
                state->playoff_scroll = state->playoff_scroll > 248U
                                            ? 252U
                                            : (uint16_t)(state->playoff_scroll + 4U);
        }
        if (cancel && !accept) {
            state->phase = TECMO_SEASON_SCHEDULE_POPUP;
            state->popup_rows_visible = asset->menu_boxes[0][1];
            state->popup_animation_tick = 0U;
            state->popup_open_sync_frames = 0U;
            state->popup_closing = false;
            state->popup_selection = 0U;
        }
        break;

    case TECMO_SEASON_RESET_CONFIRM:
        if (accept) {
            if (state->popup_selection == 0U) {
                state->popup_selection = 1U;
                close_popup(state, TECMO_SEASON_SCHEDULE_POPUP);
            } else {
                state->season_type_selection = session->season_type;
                state->popup_selection = 0U;
                close_popup(state, TECMO_SEASON_TYPE_SELECT);
            }
            break;
        }
        if (cancel) {
            state->popup_selection = 1U;
            close_popup(state, TECMO_SEASON_SCHEDULE_POPUP);
            break;
        }
        if (direction == TECMO_SEASON_DIRECTION_UP ||
            direction == TECMO_SEASON_DIRECTION_DOWN)
            state->popup_selection = wrap_u8(
                state->popup_selection, 2U,
                direction == TECMO_SEASON_DIRECTION_UP ? -1 : 1);
        break;

    case TECMO_SEASON_TYPE_SELECT:
        if (accept || cancel) {
            reset_for_type(session, state->season_type_selection);
            state->schedule_selection = 0U;
            state->editor_panel = 0U;
            state->editor_team = 0U;
            state->programmed_return_to_schedule =
                session->season_type == TECMO_SEASON_PROGRAMMED;
            state->popup_rows_visible = 0U;
            state->popup_animation_tick = 0U;
            state->popup_open_sync_frames = 0U;
            state->popup_closing = false;
            state->phase = session->season_type == TECMO_SEASON_PROGRAMMED
                               ? TECMO_SEASON_PROGRAMMED_EDITOR
                               : TECMO_SEASON_SCHEDULE;
            break;
        }
        if (direction == TECMO_SEASON_DIRECTION_UP ||
            direction == TECMO_SEASON_DIRECTION_DOWN)
            state->season_type_selection = wrap_u8(
                state->season_type_selection, 4U,
                direction == TECMO_SEASON_DIRECTION_UP ? -1 : 1);
        break;

    case TECMO_SEASON_STANDINGS:
        if (state->standings_slide != 0U) {
            state->standings_slide = state->standings_slide > 248U
                                         ? 256U
                                         : (uint16_t)(state->standings_slide + 8U);
            if (state->standings_slide == 256U) {
                state->standings_page = state->standings_target_page;
                state->standings_slide = 0U;
                state->standings_slide_direction = 0;
            }
            break;
        }
        if (cancel && !accept)
            return finish_update(state,
                                 TECMO_SEASON_ACTION_BACK_TO_START_MENU,
                                 accept || cancel);
        if (direction == TECMO_SEASON_DIRECTION_LEFT ||
            direction == TECMO_SEASON_DIRECTION_RIGHT) {
            state->standings_target_page = state->standings_page ^ 1U;
            state->standings_slide_direction =
                direction == TECMO_SEASON_DIRECTION_LEFT ? -1 : 1;
            state->standings_slide = 8U;
        }
        break;

    case TECMO_SEASON_PROGRAMMED_EDITOR:
        if (state->standings_slide != 0U) {
            state->standings_slide = state->standings_slide > 248U
                                         ? 256U
                                         : (uint16_t)(state->standings_slide + 8U);
            if (state->standings_slide == 256U) {
                state->editor_panel = state->editor_target_panel;
                state->standings_page = state->standings_target_page;
                state->editor_team = 0U;
                state->standings_slide = 0U;
                state->standings_slide_direction = 0;
            }
            break;
        }
        if (controls->pressed.confirm || controls->pressed.tab ||
            controls->released.confirm || controls->released.tab) {
            if (state->programmed_return_to_schedule) {
                state->phase = TECMO_SEASON_SCHEDULE;
                state->schedule_selection = 0U;
                break;
            }
            return finish_update(state,
                                 TECMO_SEASON_ACTION_BACK_TO_START_MENU,
                                 accept || cancel);
        }
        if (direction == TECMO_SEASON_DIRECTION_UP ||
            direction == TECMO_SEASON_DIRECTION_DOWN) {
            uint8_t count = season_page_team_count(
                asset,
                state->editor_panel >= 2U ? 1U : 0U);
            state->editor_team = wrap_u8(
                state->editor_team, count,
                direction == TECMO_SEASON_DIRECTION_UP ? -1 : 1);
            break;
        }
        if (direction == TECMO_SEASON_DIRECTION_LEFT ||
            direction == TECMO_SEASON_DIRECTION_RIGHT) {
            uint8_t target_panel = wrap_u8(
                state->editor_panel, 4U,
                direction == TECMO_SEASON_DIRECTION_LEFT ? -1 : 1);
            uint8_t target_page = target_panel >= 2U ? 1U : 0U;
            if (target_page != state->standings_page) {
                state->editor_target_panel = target_panel;
                state->standings_target_page = target_page;
                state->standings_slide_direction =
                    direction == TECMO_SEASON_DIRECTION_RIGHT ? 1 : -1;
                state->standings_slide = 8U;
            } else {
                state->editor_panel = target_panel;
                state->editor_team = 0U;
            }
            break;
        }
        if (!controls->held.up && !controls->held.down &&
            !controls->held.left && !controls->held.right &&
            (controls->held.shoot || controls->held.cancel) &&
            state->direction_cooldown == 0U) {
            uint8_t team = season_page_team(
                asset,
                state->editor_panel >= 2U ? 1U : 0U,
                state->editor_team);
            uint8_t *value = (state->editor_panel & 1U) == 0U
                                 ? &session->wins[team]
                                 : &session->losses[team];
            uint8_t other = (state->editor_panel & 1U) == 0U
                                ? session->losses[team]
                                : session->wins[team];
            state->direction_cooldown = 5U;
            if (controls->held.shoot != controls->held.cancel &&
                controls->held.shoot) {
                *value = *value >= 82U ? 0U : (uint8_t)(*value + 1U);
                if ((unsigned)*value + other > 82U) *value = 0U;
                persist_change(session);
            } else if (controls->held.shoot != controls->held.cancel) {
                *value = *value == 0U ? (uint8_t)(82U - other)
                                      : (uint8_t)(*value - 1U);
                persist_change(session);
            }
            return finish_update(state, TECMO_SEASON_ACTION_NONE, true);
        }
        break;

    case TECMO_SEASON_LEADERS:
        {
            if (state->leaders_results) {
                if (controls->pressed.cancel) {
                    state->leaders_results = false;
                    state->leader_page = 0U;
                }
                break;
            }
            if (controls->pressed.left && state->leader_category != 0U)
                --state->leader_category;
            if (controls->pressed.right &&
                state->leader_category + 1U < TECMO_SEASON_LEADER_COUNT)
                ++state->leader_category;
            if (controls->pressed.down)
                state->leader_category =
                    asset->leader_down[state->leader_category];
            if (controls->pressed.up)
                state->leader_category =
                    asset->leader_up[state->leader_category];
            if (controls->pressed.cancel)
                return TECMO_SEASON_ACTION_BACK_TO_START_MENU;
            if (controls->pressed.shoot) {
                state->leaders_results = true;
                state->leader_page = 0U;
            }
        }
        break;

    case TECMO_SEASON_GAME_START:
        if (state->game_prepare_pending) {
            prepare_game_start_boundary(state, asset, session);
            return finish_update(state, TECMO_SEASON_ACTION_NONE, false);
        }
        if (state->game_result_visible_rows <
                (uint8_t)(state->game_result_count * 2U)) {
            ++state->game_result_visible_rows;
            return finish_update(state, TECMO_SEASON_ACTION_NONE, false);
        }
        if (cancel) {
            return finish_update(state,
                                 TECMO_SEASON_ACTION_BACK_TO_START_MENU,
                                 true);
        }
        break;
    }
    return finish_update(state, TECMO_SEASON_ACTION_NONE, accept || cancel);
}

static bool make_viewport(TecmoFramebuffer *view,
                          TecmoFramebuffer *framebuffer,
                          int origin_x,
                          int origin_y,
                          int scale)
{
    int scaled_width;
    int scaled_height;
    size_t pitch;
    size_t origin_offset;
    if (view == NULL || framebuffer == NULL || framebuffer->pixels == NULL ||
        framebuffer->width <= 0 || framebuffer->height <= 0 ||
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
    if ((size_t)origin_y > (SIZE_MAX - (size_t)origin_x) / pitch) return false;
    origin_offset = (size_t)origin_y * pitch + (size_t)origin_x;
    view->pixels = framebuffer->pixels + origin_offset;
    view->width = scaled_width;
    view->height = scaled_height;
    view->pitch_pixels = framebuffer->pitch_pixels;
    return true;
}

static void fill_rect(TecmoFramebuffer *view, int x, int y, int width,
                      int height, int scale, uint32_t color)
{
    int x0 = x * scale;
    int y0 = y * scale;
    int x1 = (x + width) * scale;
    int y1 = (y + height) * scale;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > view->width) x1 = view->width;
    if (y1 > view->height) y1 = view->height;
    for (int py = y0; py < y1; ++py) {
        uint32_t *row = view->pixels + (size_t)py * (size_t)view->pitch_pixels;
        for (int px = x0; px < x1; ++px) row[px] = color;
    }
}

static void draw_cell(TecmoFramebuffer *view,
                      const TecmoStartGameMenuCell *cell,
                      const uint8_t palette[16],
                      const uint8_t *chr,
                      uint64_t chr_count,
                      int x,
                      int y,
                      int scale,
                      int palette_override)
{
    uint32_t rgba[4];
    uint8_t index = palette_override >= 0
                        ? (uint8_t)palette_override
                        : cell->palette_index;
    size_t base = (size_t)index * 4U;
    rgba[0] = tecmo_nes_2c02_rgba(palette[0]);
    for (size_t i = 1U; i < 4U; ++i)
        rgba[i] = tecmo_nes_2c02_rgba(palette[base + i]);
    tecmo_draw_chr_tile_at_offset_ex(view, chr, chr_count, cell->chr_offset,
                                     x * scale, y * scale, scale, rgba,
                                     false, false);
}

static void draw_screen(TecmoFramebuffer *view,
                        const TecmoSeasonAsset *asset,
                        size_t screen,
                        size_t page,
                        const uint8_t *chr,
                        uint64_t chr_count,
                        int scale)
{
    const uint8_t *palette = asset->palettes[screen];
    fill_rect(view, 0, 0, 256, 240, scale,
              tecmo_nes_2c02_rgba(palette[0]));
    for (size_t cell = 0U; cell < 960U; ++cell)
        draw_cell(view, &asset->screens[screen][page * 960U + cell], palette,
                  chr, chr_count, (int)(cell % 32U) * 8,
                  (int)(cell / 32U) * 8, scale, -1);
}

static uint8_t palette_at(const TecmoSeasonAsset *asset, size_t screen,
                          size_t page, int x, int y)
{
    int col = x / 8;
    int row = y / 8;
    if (col < 0 || col >= 32 || row < 0 || row >= 30) return 0U;
    return asset->screens[screen][page * 960U +
                                  (size_t)row * 32U + (size_t)col].palette_index;
}

static void draw_text(TecmoFramebuffer *view,
                      const TecmoSeasonAsset *asset,
                      size_t screen,
                      size_t page,
                      const char *text,
                      int x,
                      int y,
                      int scale,
                      int forced_palette,
                      const uint8_t *chr,
                      uint64_t chr_count)
{
    if (text == NULL) return;
    for (size_t i = 0U; text[i] != '\0'; ++i) {
        unsigned c = (unsigned char)text[i];
        int px = x + (int)i * 8;
        if (c < 0x20U || c >= 0x20U + TECMO_SEASON_FONT_COUNT ||
            px < 0 || px >= 256)
            continue;
        draw_cell(view, &asset->font[c - 0x20U], asset->palettes[screen],
                  chr, chr_count, px, y, scale,
                  forced_palette >= 0 ? forced_palette
                                      : palette_at(asset, screen, page, px, y));
    }
}

static void draw_leader_screen(TecmoFramebuffer *view,
                               const TecmoSeasonAsset *asset,
                               size_t screen,
                               const uint8_t *chr,
                               uint64_t chr_count,
                               int scale)
{
    const uint8_t *palette = asset->leader_palettes[screen];
    fill_rect(view, 0, 0, 256, 240, scale,
              tecmo_nes_2c02_rgba(palette[0]));
    for (size_t cell = 0U; cell < TECMO_SEASON_LEADER_SCREEN_CELLS; ++cell)
        draw_cell(view, &asset->leader_screens[screen][cell], palette,
                  chr, chr_count, (int)(cell % 32U) * 8,
                  (int)(cell / 32U) * 8, scale, -1);
}

static void draw_leader_text(TecmoFramebuffer *view,
                             const TecmoSeasonAsset *asset,
                             size_t screen,
                             const char *value,
                             int x,
                             int y,
                             const uint8_t *chr,
                             uint64_t chr_count,
                             int scale)
{
    if (value == NULL) return;
    for (size_t i = 0U; value[i] != '\0'; ++i) {
        unsigned c = (unsigned char)value[i];
        int px = x + (int)i * 8;
        if (c < 0x20U || c >= 0x20U + TECMO_SEASON_FONT_COUNT ||
            px < 0 || px >= 256)
            continue;
        draw_cell(view, &asset->font[c - 0x20U],
                  asset->leader_palettes[screen], chr, chr_count,
                  px, y, scale, 2);
    }
}

static void draw_cursor(TecmoFramebuffer *view,
                        const TecmoSeasonAsset *asset,
                        const uint8_t *chr,
                        uint64_t chr_count,
                        int x,
                        int y,
                        int scale)
{
    uint32_t rgba[4] = {0U, 0U, 0U, 0U};
    for (size_t i = 1U; i < 4U; ++i)
        rgba[i] = tecmo_nes_2c02_rgba(asset->sprite_palette[i]);
    tecmo_draw_chr_tile_at_offset_ex(
        view, chr, chr_count, asset->cursor.top_chr_offset,
        (x + asset->cursor.dx) * scale,
        (y + asset->cursor.dy + 1) * scale, scale, rgba, false, false);
    tecmo_draw_chr_tile_at_offset_ex(
        view, chr, chr_count, asset->cursor.bottom_chr_offset,
        (x + asset->cursor.dx) * scale,
        (y + asset->cursor.dy + 9) * scale, scale, rgba, false, false);
}

static void team_label(char output[17], const TecmoTeamDataAsset *team_data,
                       uint8_t team)
{
    if (team_data != NULL && team_data->available && team < 27U)
        (void)snprintf(output, 17U, "%.16s", team_data->teams[team].nickname);
    else
        (void)snprintf(output, 17U, "TEAM %02u", (unsigned)team + 1U);
}

static void team_city_label(char output[17],
                            const TecmoTeamDataAsset *team_data,
                            uint8_t team)
{
    if (team_data != NULL && team_data->available && team < 27U)
        (void)snprintf(output, 17U, "%.16s", team_data->teams[team].city);
    else
        (void)snprintf(output, 17U, "TEAM %02u", (unsigned)team + 1U);
}

static void draw_team_control(TecmoFramebuffer *view,
                              const TecmoSeasonAsset *asset,
                              const TecmoSeasonSession *session,
                              const TecmoSeasonState *state,
                              const uint8_t *chr,
                              uint64_t chr_count,
                              int scale)
{
    draw_screen(view, asset, 3U, 0U, chr, chr_count, scale);
    for (size_t team = 0U; team < 27U; ++team) {
        size_t column = team / 9U;
        size_t row = team % 9U;
        uint8_t mode = session->team_control[team];
        for (size_t glyph = 0U; glyph < 3U; ++glyph)
            draw_cell(view, &asset->control_cells[mode][glyph],
                      asset->palettes[3], chr, chr_count,
                      asset->team_control_x[column] + (int)glyph * 8,
                      asset->team_control_y + (int)row * asset->team_control_stride,
                      scale, -1);
    }
    draw_cursor(view, asset, chr, chr_count,
                asset->team_control_x[state->team_selection / 9U] - 16,
                asset->team_control_y +
                    (state->team_selection % 9U) * asset->team_control_stride,
                scale);
}

static void draw_schedule_rows(TecmoFramebuffer *view,
                               const TecmoSeasonAsset *asset,
                               const TecmoSeasonSession *session,
                               const TecmoSeasonState *state,
                               const TecmoTeamDataAsset *team_data,
                               const uint8_t *chr,
                               uint64_t chr_count,
                               int scale)
{
    uint16_t count = asset->game_counts[session->season_type];
    uint16_t start = state->schedule_selection > 3U
                         ? (uint16_t)(state->schedule_selection - 3U)
                         : 0U;
    char line[34];
    for (size_t row = 0U; row < 8U; ++row) {
        uint16_t game = (uint16_t)(start + row);
        uint16_t raw;
        char away[17];
        char home[17];
        if (game >= count) break;
        if (!tecmo_season_schedule_raw_index(asset, session->season_type,
                                             game, &raw))
            break;
        team_label(away, team_data, asset->schedule[raw][0] & 0x3FU);
        team_label(home, team_data, asset->schedule[raw][1] & 0x3FU);
        (void)snprintf(line, sizeof(line), " %03u %.8s - %.8s",
                       (unsigned)game + 1U, away, home);
        draw_text(view, asset, 4U, 0U, line, 16, 64 + (int)row * 16,
                  scale, 2, chr, chr_count);
        if (game == state->schedule_selection)
            draw_cursor(view, asset, chr, chr_count, 0,
                        64 + (int)row * 16, scale);
    }
}

static void draw_record_popup(TecmoFramebuffer *view,
                              const TecmoSeasonAsset *asset,
                              size_t screen,
                              size_t box_index,
                              size_t selection,
                              uint8_t visible_rows,
                              const uint8_t *chr,
                              uint64_t chr_count,
                              int scale)
{
    const TecmoSeasonPopupOverlay *overlay = &asset->popup_overlays[box_index];
    if (visible_rows > overlay->height) visible_rows = (uint8_t)overlay->height;
    for (size_t y = 0U; y < visible_rows; ++y)
        for (size_t x = 0U; x < overlay->width; ++x) {
            size_t cell = overlay->cell_start + y * overlay->width + x;
            draw_cell(view, &asset->popup_cells[cell], asset->palettes[screen],
                      chr, chr_count,
                      ((int)overlay->origin_x + (int)x) * 8,
                      ((int)overlay->origin_y + (int)y) * 8,
                      scale, -1);
        }
    if (visible_rows == overlay->height)
        draw_cursor(view, asset, chr, chr_count,
                    asset->popup_cursor_x[box_index][selection],
                    asset->popup_cursor_y[box_index][selection], scale);
}

static void draw_playoff(TecmoFramebuffer *view,
                         const TecmoSeasonAsset *asset,
                         uint16_t scroll,
                         const uint8_t *chr,
                         uint64_t chr_count,
                         int scale)
{
    fill_rect(view, 0, 0, 256, 240, scale,
              tecmo_nes_2c02_rgba(asset->palettes[0][0]));
    for (size_t page = 0U; page < 2U; ++page) {
        for (size_t cell = 0U; cell < 960U; ++cell) {
            size_t row = cell / 32U;
            size_t column = cell % 32U;
            int world_x = (int)(page * 256U + column * 8U);
            int x = (world_x - (int)scroll) % 512;
            if (x < 0) x += 512;
            if (x >= 256) x -= 512;
            if (x <= -8 || x >= 256)
                continue;
            draw_cell(view, &asset->screens[0][page * 960U + cell],
                      asset->palettes[0], chr, chr_count, x,
                      (int)row * 8, scale, -1);
        }
    }
}

static void draw_standings_page(TecmoFramebuffer *view,
                                const TecmoSeasonAsset *asset,
                                const TecmoSeasonSession *session,
                                const TecmoSeasonState *state,
                                const TecmoTeamDataAsset *team_data,
                                size_t page,
                                int x_offset,
                                bool editor,
                                bool editor_cursor,
                                const uint8_t *chr,
                                uint64_t chr_count,
                                int scale)
{
    for (size_t cell = 0U; cell < 960U; ++cell)
        draw_cell(view, &asset->screens[2U][page * 960U + cell],
                  asset->palettes[2U], chr, chr_count,
                  x_offset + (int)(cell % 32U) * 8,
                  (int)(cell / 32U) * 8, scale, -1);
    for (size_t half = 0U; half < 2U; ++half) {
        size_t division = page * 2U + half;
        size_t count = season_division_count(asset, division);
        uint8_t order[7];
        int y0 = half == 0U ? 72 : 160;
        for (size_t row = 0U; row < count; ++row)
            order[row] = season_division_team(asset, division, row);
        if (!editor) {
            for (size_t row = 1U; row < count; ++row) {
                uint8_t candidate = order[row];
                size_t insert = row;
                while (insert > 0U) {
                    uint8_t prior = order[insert - 1U];
                    unsigned candidate_games = session->wins[candidate] +
                                               session->losses[candidate];
                    unsigned prior_games = session->wins[prior] +
                                           session->losses[prior];
                    unsigned candidate_pct = session->wins[candidate] * 1000U /
                        (candidate_games == 0U ? 1U : candidate_games);
                    unsigned prior_pct = session->wins[prior] * 1000U /
                        (prior_games == 0U ? 1U : prior_games);
                    bool ranks_higher = candidate_pct > prior_pct ||
                        (candidate_pct == prior_pct &&
                         session->wins[candidate] > session->wins[prior]);
                    if (!ranks_higher) break;
                    order[insert] = prior;
                    --insert;
                }
                order[insert] = candidate;
            }
        }
        for (size_t row = 0U; row < count; ++row) {
            uint8_t team = order[row];
            uint8_t leader = order[0];
            unsigned games = session->wins[team] + session->losses[team];
            unsigned pct = games == 0U ? 0U
                                       : session->wins[team] * 1000U / games;
            int games_difference = (int)games -
                ((int)session->wins[leader] + session->losses[leader]);
            int half_games = games_difference >= 0
                                 ? games_difference / 2
                                 : -((-games_difference + 1) / 2);
            int games_behind = (int)session->wins[leader] -
                               (int)session->wins[team] + half_games;
            bool has_half_game =
                ((games_difference < 0 ? -games_difference
                                       : games_difference) % 2) != 0;
            char name[17];
            char value[16];
            int y = y0 + (int)row * 8;
            team_city_label(name, team_data, team);
            (void)snprintf(value, sizeof(value), "%.13s", name);
            draw_text(view, asset, 2U, page, value, x_offset + 8, y,
                      scale, 2, chr, chr_count);
            (void)snprintf(value, sizeof(value), "%02u", session->wins[team]);
            draw_text(view, asset, 2U, page, value, x_offset + 120, y,
                      scale, 2, chr, chr_count);
            (void)snprintf(value, sizeof(value), "%02u", session->losses[team]);
            draw_text(view, asset, 2U, page, value, x_offset + 144, y,
                      scale, 2, chr, chr_count);
            if (games != 0U && pct >= 1000U)
                (void)snprintf(value, sizeof(value), "1.000");
            else
                (void)snprintf(value, sizeof(value), " .%03u", pct);
            draw_text(view, asset, 2U, page, value, x_offset + 168, y,
                      scale, 2, chr, chr_count);
            if (games_behind < 0) games_behind = 0;
            if (games_behind == 0) {
                draw_cell(view, &asset->zero_games_behind_cells[0],
                          asset->palettes[2U], chr, chr_count,
                          x_offset + 216, y, scale, 2);
                draw_cell(view,
                          has_half_game ? &asset->half_game_cell
                                        : &asset->zero_games_behind_cells[1],
                          asset->palettes[2U], chr, chr_count,
                          x_offset + 224, y, scale, 2);
                continue;
            }
            if (!has_half_game)
                (void)snprintf(value, sizeof(value), "%2u",
                               (unsigned)games_behind);
            else
                (void)snprintf(value, sizeof(value), "%u",
                               (unsigned)games_behind);
            draw_text(view, asset, 2U, page, value, x_offset + 216, y,
                      scale, 2, chr, chr_count);
            if (has_half_game)
                draw_cell(view, &asset->half_game_cell, asset->palettes[2U],
                          chr, chr_count,
                          x_offset + 216 + (int)strlen(value) * 8,
                          y, scale, 2);
        }
    }
    if (editor_cursor) {
        uint8_t team = season_page_team(asset, page, state->editor_team);
        size_t division = page * 2U;
        size_t row = 0U;
        for (; division < page * 2U + 2U; ++division) {
            for (row = 0U; row < season_division_count(asset, division); ++row)
                if (season_division_team(asset, division, row) == team)
                    goto cursor_found;
        }
cursor_found:
        if (x_offset > -256 && x_offset < 256)
            draw_cursor(view, asset, chr, chr_count,
                        x_offset + ((state->editor_panel & 1U) == 0U
                                        ? 112 : 136),
                        (division & 1U) == 0U ? 75 + (int)row * 8
                                              : 163 + (int)row * 8,
                        scale);
    }
}

static void draw_standings(TecmoFramebuffer *view,
                           const TecmoSeasonAsset *asset,
                           const TecmoSeasonSession *session,
                           const TecmoSeasonState *state,
                           const TecmoTeamDataAsset *team_data,
                           const uint8_t *chr,
                           uint64_t chr_count,
                           int scale)
{
    bool editor = state->phase == TECMO_SEASON_PROGRAMMED_EDITOR;
    size_t page = editor ? (state->editor_panel >= 2U ? 1U : 0U)
                         : state->standings_page;
    fill_rect(view, 0, 0, 256, 240, scale,
              tecmo_nes_2c02_rgba(asset->palettes[2U][0]));
    if (state->standings_slide != 0U) {
        int direction = state->standings_slide_direction;
        int source_x = -direction * (int)state->standings_slide;
        int target_x = source_x + direction * 256;
        draw_standings_page(view, asset, session, state, team_data,
                            page, source_x, editor, editor,
                            chr, chr_count, scale);
        draw_standings_page(view, asset, session, state, team_data,
                            state->standings_target_page, target_x, editor,
                            false,
                            chr, chr_count, scale);
    } else {
        draw_standings_page(view, asset, session, state, team_data,
                            page, 0, editor, editor, chr, chr_count, scale);
    }
}

static bool valid_runtime_state(const TecmoSeasonAsset *asset,
                                const TecmoSeasonSession *session,
                                const TecmoSeasonState *state)
{
    uint16_t game_count;
    int box_index;
    if (asset == NULL || session == NULL || state == NULL ||
        session->season_type >= 4U ||
        state->phase > TECMO_SEASON_GAME_START)
        return false;
    game_count = asset->game_counts[session->season_type];
    if (game_count == 0U || game_count > TECMO_SEASON_SCHEDULE_COUNT ||
        session->schedule_index > game_count ||
        state->schedule_selection >= game_count ||
        state->team_selection >= TECMO_SEASON_TEAM_COUNT ||
        state->popup_selection >= 2U ||
        state->season_type_selection >= 4U || state->standings_page >= 2U ||
        state->playoff_scroll > 252U || state->standings_slide > 256U ||
        state->standings_target_page >= 2U ||
        state->standings_slide_direction < -1 ||
        state->standings_slide_direction > 1 ||
        state->editor_panel >= 4U || state->editor_target_panel >= 4U ||
        state->leader_category >= 7U ||
        (state->leader_page != 0U && state->leader_page != 6U &&
         state->leader_page != 12U) ||
        state->game_result_count > 4U ||
        state->game_result_visible_rows > state->game_result_count * 2U ||
        state->editor_team >= season_page_team_count(
            asset,
            state->editor_panel >= 2U ? 1U : 0U))
        return false;
    if ((state->game_prepare_pending &&
         state->phase != TECMO_SEASON_GAME_START) ||
        (state->game_result_pending &&
        (state->phase != TECMO_SEASON_GAME_START ||
         state->game_prepare_pending || state->season_complete ||
         state->pending_game.game_index != session->schedule_index ||
         state->pending_game.game_index >= game_count ||
         state->pending_game.away_team >= TECMO_SEASON_TEAM_COUNT ||
         state->pending_game.home_team >= TECMO_SEASON_TEAM_COUNT ||
         state->pending_game.away_team == state->pending_game.home_team)))
        return false;
    box_index = popup_box_index(state->phase);
    if (state->popup_target_phase > TECMO_SEASON_GAME_START ||
        state->popup_animation_tick >= 10U ||
        state->popup_open_sync_frames > 1U ||
        (state->popup_closing && state->popup_open_sync_frames != 0U) ||
        (box_index >= 0 && state->popup_rows_visible >
                               asset->menu_boxes[box_index][1]) ||
        (box_index < 0 &&
         (state->popup_rows_visible != 0U || state->popup_closing ||
          state->popup_open_sync_frames != 0U)))
        return false;
    if ((state->standings_slide == 0U) !=
            (state->standings_slide_direction == 0) ||
        (state->standings_slide != 0U &&
         state->standings_target_page == state->standings_page))
        return false;
    for (size_t result = 0U; result < state->game_result_count; ++result)
        if (state->game_results[result].game_index >=
                TECMO_SEASON_SCHEDULE_COUNT ||
            state->game_results[result].away_team >= TECMO_SEASON_TEAM_COUNT ||
            state->game_results[result].home_team >= TECMO_SEASON_TEAM_COUNT ||
            state->game_results[result].away_score > 999U ||
            state->game_results[result].home_score > 999U)
            return false;
    for (size_t team = 0U; team < TECMO_SEASON_TEAM_COUNT; ++team)
        if (session->team_control[team] >= 4U || session->wins[team] > 82U ||
            session->losses[team] > 82U ||
            session->wins[team] + session->losses[team] > 82U)
            return false;
    return true;
}

bool tecmo_season_draw(TecmoFramebuffer *framebuffer,
                       const TecmoSeasonAsset *asset,
                       const TecmoSeasonSession *session,
                       const TecmoSeasonState *state,
                       const TecmoTeamDataAsset *team_data,
                       const uint8_t *chr_bytes,
                       uint64_t chr_byte_count,
                       int origin_x,
                       int origin_y,
                       int scale)
{
    TecmoFramebuffer view;
    if (asset == NULL || session == NULL || state == NULL ||
        !asset->available || team_data == NULL || !team_data->available ||
        !valid_runtime_state(asset, session, state) ||
        !tecmo_season_asset_chr_available(asset, chr_bytes, chr_byte_count) ||
        !make_viewport(&view, framebuffer, origin_x, origin_y, scale))
        return false;
    switch (state->phase) {
    case TECMO_SEASON_TEAM_CONTROL:
        draw_team_control(&view, asset, session, state,
                          chr_bytes, chr_byte_count, scale);
        break;
    case TECMO_SEASON_SCHEDULE:
    case TECMO_SEASON_SCHEDULE_POPUP:
    case TECMO_SEASON_RESET_CONFIRM:
    case TECMO_SEASON_TYPE_SELECT:
        draw_screen(&view, asset, 4U, 0U, chr_bytes, chr_byte_count, scale);
        draw_text(&view, asset, 4U, 0U, asset->schedule_text[0], 96, 24,
                  scale, 2, chr_bytes, chr_byte_count);
        draw_text(&view, asset, 4U, 0U,
                  asset->season_type_text[session->season_type + 1U], 88, 40,
                  scale, 2, chr_bytes, chr_byte_count);
        draw_schedule_rows(&view, asset, session, state, team_data,
                           chr_bytes, chr_byte_count, scale);
        if (state->phase == TECMO_SEASON_RESET_CONFIRM ||
            state->phase == TECMO_SEASON_TYPE_SELECT)
            draw_record_popup(&view, asset, 4U, 0U, 1U,
                              (uint8_t)asset->popup_overlays[0].height,
                              chr_bytes, chr_byte_count, scale);
        if (state->phase == TECMO_SEASON_SCHEDULE_POPUP)
            draw_record_popup(&view, asset, 4U, 0U, state->popup_selection,
                              state->popup_rows_visible, chr_bytes,
                              chr_byte_count, scale);
        else if (state->phase == TECMO_SEASON_RESET_CONFIRM)
            draw_record_popup(&view, asset, 4U, 1U, state->popup_selection,
                              state->popup_rows_visible, chr_bytes,
                              chr_byte_count, scale);
        else if (state->phase == TECMO_SEASON_TYPE_SELECT)
            draw_record_popup(&view, asset, 4U, 2U,
                              state->season_type_selection,
                              state->popup_rows_visible, chr_bytes,
                              chr_byte_count, scale);
        break;
    case TECMO_SEASON_PLAYOFF:
        draw_playoff(&view, asset, state->playoff_scroll,
                     chr_bytes, chr_byte_count, scale);
        break;
    case TECMO_SEASON_STANDINGS:
    case TECMO_SEASON_PROGRAMMED_EDITOR:
        draw_standings(&view, asset, session, state, team_data,
                       chr_bytes, chr_byte_count, scale);
        break;
    case TECMO_SEASON_LEADERS:
        if (state->leaders_results) {
            draw_leader_screen(&view, asset, 0U,
                               chr_bytes, chr_byte_count, scale);
            draw_leader_text(&view, asset, 0U,
                             "PLAYER RESULTS UNAVAILABLE", 24, 208,
                             chr_bytes, chr_byte_count, scale);
        } else {
            int marker_x = ((int)asset->leader_cursor_x[
                                state->leader_category] / 8) * 8 - 8;
            int marker_y = ((int)asset->leader_cursor_y[
                                state->leader_category] / 8) * 8;
            if (marker_x < 0) marker_x = 0;
            draw_leader_screen(&view, asset, 0U,
                               chr_bytes, chr_byte_count, scale);
            /* Bank00 $2D10 routes these coordinates through $AC88/$AC5E.
             * TSNS does not yet carry those priority metasprites, so use the
             * imported ROM font as a non-overlapping boundary marker instead
             * of the unrelated Bank01 $8031 cursor. */
            draw_leader_text(&view, asset, 0U, ">", marker_x, marker_y,
                             chr_bytes, chr_byte_count, scale);
        }
        break;
    case TECMO_SEASON_GAME_START: {
        if (state->game_launch_blocked) {
            fill_rect(&view, 0, 0, 256, 240, scale,
                      tecmo_nes_2c02_rgba(0x0FU));
            break;
        }
        draw_screen(&view, asset, 1U, 0U, chr_bytes, chr_byte_count, scale);
        for (size_t row = 0U; row < state->game_result_visible_rows; ++row) {
            const TecmoSeasonGameResult *result =
                &state->game_results[row / 2U];
            bool home_row = (row & 1U) != 0U;
            uint8_t team = home_row ? result->home_team : result->away_team;
            uint16_t score = home_row ? result->home_score : result->away_score;
            int y = 96 + (int)(row / 2U) * 32 + (home_row ? 8 : 0);
            char city[17];
            char score_text[4];
            team_city_label(city, team_data, team);
            city[12] = '\0';
            draw_text(&view, asset, 1U, 0U, city, 32, y, scale, 2,
                      chr_bytes, chr_byte_count);
            (void)snprintf(score_text, sizeof(score_text), "%3u",
                           (unsigned)score);
            draw_text(&view, asset, 1U, 0U, score_text, 152, y, scale, 2,
                      chr_bytes, chr_byte_count);
            if (result->overtime && home_row)
                draw_text(&view, asset, 1U, 0U, asset->overtime_text,
                          184, y, scale, 2,
                          chr_bytes, chr_byte_count);
        }
        break;
    }
    }
    return true;
}

const char *tecmo_season_phase_name(TecmoSeasonPhase phase)
{
    switch (phase) {
    case TECMO_SEASON_TEAM_CONTROL: return "team-control";
    case TECMO_SEASON_SCHEDULE: return "schedule";
    case TECMO_SEASON_SCHEDULE_POPUP: return "schedule-popup";
    case TECMO_SEASON_PLAYOFF: return "playoff";
    case TECMO_SEASON_RESET_CONFIRM: return "reset-confirm";
    case TECMO_SEASON_TYPE_SELECT: return "season-type";
    case TECMO_SEASON_STANDINGS: return "standings";
    case TECMO_SEASON_PROGRAMMED_EDITOR: return "programmed-editor";
    case TECMO_SEASON_LEADERS: return "leaders";
    case TECMO_SEASON_GAME_START: return "game-start-prelaunch";
    default: return "unknown";
    }
}

const char *tecmo_season_type_name(uint8_t season_type)
{
    static const char *names[4] = {
        "REGULAR", "REDUCED", "SHORT", "PROGRAMMED"
    };
    return season_type < 4U ? names[season_type] : "INVALID";
}

const char *tecmo_season_control_name(uint8_t control)
{
    static const char *names[4] = {"SKIP", "MAN", "COM", "COA"};
    return control < 4U ? names[control] : "INVALID";
}

static void season_test_neutral_frames(TecmoSeasonState *state,
                                       const TecmoSeasonAsset *asset,
                                       TecmoSeasonSession *session,
                                       size_t count)
{
    TecmoControlFrame controls;
    memset(&controls, 0, sizeof(controls));
    for (size_t frame = 0U; frame < count; ++frame)
        (void)tecmo_season_update(state, asset, session, &controls);
}

bool tecmo_season_self_test(char *message, size_t message_size)
{
    TecmoSeasonAsset asset;
    TecmoSeasonSession session;
    TecmoSeasonSession loaded;
    TecmoSeasonState state;
    TecmoSeasonGameResult completed;
    TecmoControlFrame controls;
    uint8_t bytes[SEASON_SAVE_SIZE] = {0};
    uint8_t *payload = bytes + SEASON_SAVE_HEADER_SIZE;
    const char *temp_root;
    char temp_save[1024] = {0};
    FILE *file;
    size_t got;
    int trailing;
    uint8_t first_editor_team;
    memset(&asset, 0, sizeof(asset));
    asset.available = true;
    asset.game_counts[0] = 1107U;
    asset.game_counts[1] = 567U;
    asset.game_counts[2] = 351U;
    asset.game_counts[3] = 1107U;
    asset.menu_boxes[0][0] = 13U;
    asset.menu_boxes[0][1] = 6U;
    asset.menu_boxes[1][0] = 24U;
    asset.menu_boxes[1][1] = 12U;
    asset.menu_boxes[2][0] = 13U;
    asset.menu_boxes[2][1] = 10U;
    asset.division_starts[0] = 0U;
    asset.division_starts[1] = 7U;
    asset.division_starts[2] = 14U;
    asset.division_starts[3] = 20U;
    for (size_t team = 0U; team < TECMO_SEASON_TEAM_COUNT; ++team)
        asset.division_teams[team] = (uint8_t)team;
    for (size_t leader = 0U; leader < TECMO_SEASON_LEADER_COUNT; ++leader) {
        asset.leader_up[leader] = (uint8_t)leader;
        asset.leader_down[leader] = (uint8_t)leader;
        asset.leader_template[leader] = 39U;
    }
    for (size_t game = 0U; game < TECMO_SEASON_SCHEDULE_COUNT; ++game) {
        asset.schedule[game][0] = (uint8_t)(game % TECMO_SEASON_TEAM_COUNT);
        asset.schedule[game][1] =
            (uint8_t)((game + 1U) % TECMO_SEASON_TEAM_COUNT);
    }
    first_editor_team = season_page_team(&asset, 0U, 0U);
    memset(&session, 0, sizeof(session));
    session_defaults(&session);
    memcpy(bytes, "TSAV", 4U);
    write_u16(bytes + 4U, 1U);
    write_u16(bytes + 6U, SEASON_SAVE_HEADER_SIZE);
    write_u32(bytes + 8U, SEASON_SAVE_SIZE);
    payload[0] = TECMO_SEASON_PROGRAMMED;
    payload[1U] = 3U;
    payload[28U] = 41U;
    payload[55U] = 41U;
    write_u16(payload + 82U, 1106U);
    write_u32(bytes + 12U, fnv1a32(payload, SEASON_SAVE_PAYLOAD_SIZE));
    if (!parse_save(&session, bytes) ||
        session.season_type != TECMO_SEASON_PROGRAMMED ||
        session.team_control[0] != 3U || session.wins[0] != 41U ||
        session.losses[0] != 41U || session.schedule_index != 1106U) {
        if (message != NULL && message_size > 0U)
            (void)snprintf(message, message_size,
                           "Season TSAV-1 parser self-test failed.");
        return false;
    }
    tecmo_season_state_init(&state, TECMO_SEASON_ROUTE_GAME_START, &session);
    if (state.phase != TECMO_SEASON_GAME_START ||
        state.schedule_selection != 1106U ||
        strcmp(tecmo_season_control_name(3U), "COA") != 0 ||
        strcmp(tecmo_season_type_name(3U), "PROGRAMMED") != 0) {
        if (message != NULL && message_size > 0U)
            (void)snprintf(message, message_size,
                           "Season state self-test failed.");
        return false;
    }

    session_defaults(&session);
    tecmo_season_state_init(&state, TECMO_SEASON_ROUTE_TEAM_CONTROL, &session);
    memset(&controls, 0, sizeof(controls));
    controls.released.shoot = true;
    controls.released.cancel = true;
    if (tecmo_season_update(&state, &asset, &session, &controls) !=
            TECMO_SEASON_ACTION_NONE ||
        session.team_control[0] != 1U || state.direction_cooldown != 5U) {
        if (message != NULL && message_size > 0U)
            (void)snprintf(message, message_size,
                           "Season A+B priority self-test failed.");
        return false;
    }
    memset(&controls, 0, sizeof(controls));
    controls.released.down = true;
    controls.released.shoot = true;
    if (tecmo_season_update(&state, &asset, &session, &controls) !=
            TECMO_SEASON_ACTION_NONE ||
        session.team_control[0] != 1U) {
        if (message != NULL && message_size > 0U)
            (void)snprintf(message, message_size,
                           "Season direction-release suppression self-test failed.");
        return false;
    }
    tecmo_season_state_init(&state, TECMO_SEASON_ROUTE_TEAM_CONTROL, &session);
    memset(&controls, 0, sizeof(controls));
    controls.held.down = true;
    controls.pressed.down = true;
    controls.released.shoot = true;
    if (tecmo_season_update(&state, &asset, &session, &controls) !=
            TECMO_SEASON_ACTION_NONE ||
        state.team_selection != 1U || session.team_control[0] != 1U ||
        state.direction_cooldown != 7U)
        goto state_failure;
    tecmo_season_state_init(&state, TECMO_SEASON_ROUTE_TEAM_CONTROL, &session);
    memset(&controls, 0, sizeof(controls));
    controls.held.down = true;
    controls.held.right = true;
    controls.pressed.down = true;
    controls.pressed.right = true;
    controls.released.shoot = true;
    controls.released.cancel = true;
    if (tecmo_season_update(&state, &asset, &session, &controls) !=
            TECMO_SEASON_ACTION_NONE ||
        state.team_selection != 0U || session.team_control[0] != 1U ||
        state.direction_cooldown != 7U)
        goto state_failure;
    tecmo_season_state_init(&state, TECMO_SEASON_ROUTE_TEAM_CONTROL, &session);
    memset(&controls, 0, sizeof(controls));
    controls.held.down = true;
    controls.pressed.down = true;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    memset(&controls, 0, sizeof(controls));
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    controls.held.right = true;
    controls.pressed.right = true;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    if (state.team_selection != 1U || state.direction_cooldown != 5U)
        goto state_failure;
    tecmo_season_state_init(&state, TECMO_SEASON_ROUTE_TEAM_CONTROL, &session);
    memset(&controls, 0, sizeof(controls));
    controls.held.down = true;
    controls.pressed.down = true;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    if (state.team_selection != 1U || state.direction_cooldown != 7U)
        goto state_failure;
    controls.pressed.down = false;
    for (size_t held_frame = 0U; held_frame < 7U; ++held_frame)
        (void)tecmo_season_update(&state, &asset, &session, &controls);
    if (state.team_selection != 1U || state.direction_cooldown != 0U)
        goto state_failure;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    if (state.team_selection != 2U || state.direction_cooldown != 7U)
        goto state_failure;
    tecmo_season_state_init(&state, TECMO_SEASON_ROUTE_TEAM_CONTROL, &session);
    memset(&controls, 0, sizeof(controls));
    controls.held.down = true;
    controls.held.right = true;
    controls.pressed.down = true;
    controls.pressed.right = true;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    if (state.team_selection != 0U || state.direction_cooldown != 7U)
        goto state_failure;
    memset(&controls, 0, sizeof(controls));
    controls.released.cancel = true;
    if (tecmo_season_update(&state, &asset, &session, &controls) !=
        TECMO_SEASON_ACTION_BACK_TO_START_MENU) {
        if (message != NULL && message_size > 0U)
            (void)snprintf(message, message_size,
                           "Season TEAM CONTROL return self-test failed.");
        return false;
    }

    tecmo_season_state_init(&state, TECMO_SEASON_ROUTE_STANDINGS, &session);
    memset(&controls, 0, sizeof(controls));
    controls.held.left = true;
    controls.held.down = true;
    controls.pressed.left = true;
    controls.pressed.down = true;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    if (state.standings_slide != 0U || state.standings_page != 0U ||
        state.direction_cooldown != 7U)
        goto state_failure;
    memset(&controls, 0, sizeof(controls));
    for (size_t gate_frame = 0U; gate_frame < 7U; ++gate_frame)
        (void)tecmo_season_update(&state, &asset, &session, &controls);
    controls.held.right = true;
    controls.pressed.right = true;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    if (state.standings_slide != 8U ||
        state.standings_slide_direction != 1 ||
        state.standings_target_page != 1U)
        goto state_failure;
    memset(&controls, 0, sizeof(controls));
    for (size_t slide_frame = 0U; slide_frame < 31U; ++slide_frame)
        (void)tecmo_season_update(&state, &asset, &session, &controls);
    if (state.standings_slide != 0U || state.standings_page != 1U ||
        state.standings_slide_direction != 0)
        goto state_failure;
    tecmo_season_state_init(&state, TECMO_SEASON_ROUTE_STANDINGS, &session);
    memset(&controls, 0, sizeof(controls));
    controls.released.shoot = true;
    controls.released.cancel = true;
    if (tecmo_season_update(&state, &asset, &session, &controls) !=
            TECMO_SEASON_ACTION_NONE ||
        state.phase != TECMO_SEASON_STANDINGS)
        goto state_failure;

    tecmo_season_state_init(&state, TECMO_SEASON_ROUTE_SCHEDULE, &session);
    memset(&controls, 0, sizeof(controls));
    controls.released.shoot = true;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    if (state.phase != TECMO_SEASON_SCHEDULE_POPUP) goto state_failure;
    season_test_neutral_frames(&state, &asset, &session, 1U);
    if (state.popup_rows_visible != 0U ||
        state.popup_open_sync_frames != 0U)
        goto state_failure;
    season_test_neutral_frames(&state, &asset, &session, 59U);
    if (state.popup_rows_visible != 5U) goto state_failure;
    season_test_neutral_frames(&state, &asset, &session, 1U);
    if (state.popup_rows_visible != 6U) goto state_failure;
    memset(&controls, 0, sizeof(controls));
    controls.held.down = true;
    controls.pressed.down = true;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    memset(&controls, 0, sizeof(controls));
    controls.released.shoot = true;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    if (state.phase != TECMO_SEASON_RESET_CONFIRM ||
        state.popup_rows_visible != 0U)
        goto state_failure;
    season_test_neutral_frames(&state, &asset, &session, 1U);
    if (state.popup_rows_visible != 0U ||
        state.popup_open_sync_frames != 0U)
        goto state_failure;
    season_test_neutral_frames(&state, &asset, &session, 119U);
    if (state.popup_rows_visible != 11U) goto state_failure;
    season_test_neutral_frames(&state, &asset, &session, 1U);
    if (state.popup_rows_visible != 12U) goto state_failure;
    memset(&controls, 0, sizeof(controls));
    controls.held.down = true;
    controls.pressed.down = true;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    memset(&controls, 0, sizeof(controls));
    controls.released.shoot = true;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    season_test_neutral_frames(&state, &asset, &session, 119U);
    if (state.phase != TECMO_SEASON_RESET_CONFIRM ||
        state.popup_rows_visible != 1U)
        goto state_failure;
    season_test_neutral_frames(&state, &asset, &session, 1U);
    if (state.phase != TECMO_SEASON_TYPE_SELECT ||
        state.popup_rows_visible != 0U)
        goto state_failure;
    season_test_neutral_frames(&state, &asset, &session, 1U);
    if (state.popup_rows_visible != 0U ||
        state.popup_open_sync_frames != 0U)
        goto state_failure;
    season_test_neutral_frames(&state, &asset, &session, 99U);
    if (state.popup_rows_visible != 9U) goto state_failure;
    season_test_neutral_frames(&state, &asset, &session, 1U);
    state.season_type_selection = TECMO_SEASON_PROGRAMMED;
    memset(&controls, 0, sizeof(controls));
    controls.released.cancel = true;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    if (state.phase != TECMO_SEASON_PROGRAMMED_EDITOR ||
        session.season_type != TECMO_SEASON_PROGRAMMED)
        goto state_failure;
    memset(&controls, 0, sizeof(controls));
    for (size_t gate_frame = 0U; gate_frame < 5U; ++gate_frame)
        (void)tecmo_season_update(&state, &asset, &session, &controls);
    controls.held.shoot = true;
    controls.held.cancel = true;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    if (session.wins[first_editor_team] != 0U ||
        state.direction_cooldown != 5U)
        goto state_failure;
    memset(&controls, 0, sizeof(controls));
    for (size_t gate_frame = 0U; gate_frame < 5U; ++gate_frame)
        (void)tecmo_season_update(&state, &asset, &session, &controls);
    controls.held.shoot = true;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    if (session.wins[first_editor_team] != 1U) goto state_failure;
    memset(&controls, 0, sizeof(controls));
    for (size_t gate_frame = 0U; gate_frame < 5U; ++gate_frame)
        (void)tecmo_season_update(&state, &asset, &session, &controls);
    controls.held.cancel = true;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    if (session.wins[first_editor_team] != 0U) goto state_failure;
    memset(&controls, 0, sizeof(controls));
    controls.pressed.confirm = true;
    if (tecmo_season_update(&state, &asset, &session, &controls) !=
            TECMO_SEASON_ACTION_NONE ||
        state.phase != TECMO_SEASON_SCHEDULE)
        goto state_failure;
    tecmo_season_state_init(&state, TECMO_SEASON_ROUTE_STANDINGS, &session);
    memset(&controls, 0, sizeof(controls));
    controls.pressed.confirm = true;
    if (tecmo_season_update(&state, &asset, &session, &controls) !=
        TECMO_SEASON_ACTION_BACK_TO_START_MENU)
        goto state_failure;

    tecmo_season_state_init(&state, TECMO_SEASON_ROUTE_SCHEDULE, &session);
    state.phase = TECMO_SEASON_PLAYOFF;
    memset(&controls, 0, sizeof(controls));
    controls.held.right = true;
    for (size_t pan_frame = 0U; pan_frame < 70U; ++pan_frame)
        (void)tecmo_season_update(&state, &asset, &session, &controls);
    if (state.playoff_scroll != 252U) goto state_failure;
    memset(&controls, 0, sizeof(controls));
    controls.released.shoot = true;
    controls.released.cancel = true;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    if (state.phase != TECMO_SEASON_PLAYOFF) goto state_failure;
    memset(&controls, 0, sizeof(controls));
    controls.released.cancel = true;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    if (state.phase != TECMO_SEASON_SCHEDULE_POPUP ||
        state.popup_selection != 0U || state.popup_rows_visible != 6U)
        goto state_failure;
    state.popup_selection = 1U;
    memset(&controls, 0, sizeof(controls));
    controls.released.shoot = true;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    if (state.phase != TECMO_SEASON_RESET_CONFIRM) goto state_failure;
    season_test_neutral_frames(&state, &asset, &session, 121U);
    memset(&controls, 0, sizeof(controls));
    controls.released.cancel = true;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    season_test_neutral_frames(&state, &asset, &session, 120U);
    if (state.phase != TECMO_SEASON_SCHEDULE_POPUP ||
        state.popup_rows_visible != 6U || state.popup_selection != 1U)
        goto state_failure;

    session_defaults(&session);
    tecmo_season_state_init(&state, TECMO_SEASON_ROUTE_GAME_START, &session);
    if (state.phase != TECMO_SEASON_GAME_START ||
        !state.game_prepare_pending || !state.game_launch_blocked ||
        state.game_result_pending || session.schedule_index != 0U)
        goto state_failure;
    memset(&controls, 0, sizeof(controls));
    if (tecmo_season_update(&state, &asset, &session, &controls) !=
            TECMO_SEASON_ACTION_NONE ||
        state.phase != TECMO_SEASON_GAME_START || !state.game_launch_blocked ||
        !state.game_result_pending || state.game_prepare_pending ||
        state.game_result_count != 0U || state.game_result_visible_rows != 0U ||
        session.schedule_index != 0U || session.wins[0] != 0U ||
        session.losses[0] != 0U)
        goto state_failure;
    memset(&controls, 0, sizeof(controls));
    controls.released.shoot = true;
    if (tecmo_season_update(&state, &asset, &session, &controls) !=
            TECMO_SEASON_ACTION_NONE || session.schedule_index != 0U ||
        !state.game_result_pending || state.game_result_count != 0U)
        goto state_failure;

    temp_root = getenv("TEMP");
    if (temp_root == NULL || temp_root[0] == '\0') temp_root = ".";
    {
        int path_length = snprintf(temp_save, sizeof(temp_save),
                                   "%s\\tecmo-season-self-test-%p.sav",
                                   temp_root, (void *)&session);
        if (path_length < 0 || (size_t)path_length >= sizeof(temp_save))
            goto state_failure;
    }
    (void)snprintf(session.save_path, sizeof(session.save_path), "%s", temp_save);
    memset(&completed, 0, sizeof(completed));
    completed.game_index = state.pending_game.game_index;
    completed.away_team = state.pending_game.away_team;
    completed.home_team = state.pending_game.home_team;
    completed.away_score = 101U;
    completed.home_score = 99U;
    {
        uint8_t original_away = state.pending_game.away_team;
        state.pending_game.away_team = (uint8_t)((original_away + 1U) %
                                                 TECMO_SEASON_TEAM_COUNT);
        if (state.pending_game.away_team == state.pending_game.home_team)
            state.pending_game.away_team = (uint8_t)((state.pending_game.away_team + 1U) %
                                                     TECMO_SEASON_TEAM_COUNT);
        completed.away_team = state.pending_game.away_team;
        if (tecmo_season_commit_game_result(&state, &asset, &session,
                                            &completed) ||
            session.schedule_index != 0U || !state.game_result_pending)
            goto state_failure;
        state.pending_game.away_team = original_away;
        completed.away_team = original_away;
    }
    ++completed.game_index;
    if (tecmo_season_commit_game_result(&state, &asset, &session,
                                        &completed) ||
        session.schedule_index != 0U || !state.game_result_pending)
        goto state_failure;
    --completed.game_index;
    if (!tecmo_season_commit_game_result(&state, &asset, &session,
                                         &completed) ||
        session.schedule_index != 1U || state.game_result_pending ||
        state.game_result_count != 1U || state.game_result_visible_rows != 1U ||
        session.wins[completed.away_team] != 1U ||
        session.losses[completed.home_team] != 1U)
        goto state_failure;
    session.team_control[4] = 3U;
    session.wins[4] = 50U;
    session.losses[4] = 32U;
    if (!tecmo_season_session_save(&session)) goto state_failure;
    file = fopen(temp_save, "rb");
    if (file == NULL) goto state_failure;
    got = fread(bytes, 1U, sizeof(bytes), file);
    trailing = fgetc(file);
    fclose(file);
    (void)remove(temp_save);
    memset(&loaded, 0, sizeof(loaded));
    if (got != sizeof(bytes) || trailing != EOF || !parse_save(&loaded, bytes) ||
        loaded.team_control[4] != 3U || loaded.wins[4] != 50U ||
        loaded.losses[4] != 32U || loaded.schedule_index != 1U)
        goto state_failure;

    bytes[SEASON_SAVE_SIZE - 1U] ^= 1U;
    if (parse_save(&session, bytes)) {
        if (message != NULL && message_size > 0U)
            (void)snprintf(message, message_size,
                           "Season malformed save was accepted.");
        return false;
    }
    if (message != NULL && message_size > 0U)
        (void)snprintf(message, message_size,
                       "Season management self-test passed.");
    return true;

state_failure:
    if (temp_save[0] != '\0') (void)remove(temp_save);
    if (message != NULL && message_size > 0U)
        (void)snprintf(message, message_size,
                       "Season route/persistence self-test failed.");
    return false;
}
