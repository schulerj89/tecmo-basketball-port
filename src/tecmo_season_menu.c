#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_season_menu.h"

#include "tecmo_asset_pack.h"
#include "tecmo_nes_video.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#endif

#define SEASON_ENTRY_ID "menu/season"
#define SEASON_PAYLOAD_SIZE 60866U
#define SEASON_PAYLOAD_FNV1A32 0xAC353D9FU
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
    static const char *leader_labels[7] = {
        "FIELD GOALS", "BLOCKED SHOTS", "REBOUNDS", "TOTAL POINTS",
        "STEALS", "3 POINT SHOTS", "FREE THROWS"
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
        bytes[109U] != 80U || bytes[110U] != 16U)
        return false;
    for (size_t i = 111U; i < SEASON_HEADER_SIZE; ++i)
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
        size_t expected_length = strlen(leader_labels[i]);
        if (expected_length >= 20U ||
            memcmp(source, leader_labels[i], expected_length + 1U) != 0)
            return false;
        for (size_t j = expected_length + 1U; j < 20U; ++j)
            if (source[j] != '\0') return false;
        memcpy(asset->leader_labels[i], source, 20U);
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
    if (read_u16(payload + 82U) >= season_game_count(payload[0])) return false;
    session->season_type = payload[0];
    memcpy(session->team_control, payload + 1U, TECMO_SEASON_TEAM_COUNT);
    memcpy(session->wins, payload + 28U, TECMO_SEASON_TEAM_COUNT);
    memcpy(session->losses, payload + 55U, TECMO_SEASON_TEAM_COUNT);
    session->schedule_index = read_u16(payload + 82U);
    session->dirty = false;
    return true;
}

void tecmo_season_session_init(TecmoSeasonSession *session,
                               const char *project_root)
{
    uint8_t bytes[SEASON_SAVE_SIZE];
    FILE *file;
    size_t got;
    int trailing;
    if (session == NULL) return;
    memset(session, 0, sizeof(*session));
    session_defaults(session);
    session->save_status = TECMO_SEASON_SAVE_ROM_DEFAULTS;
    (void)snprintf(session->status, sizeof(session->status),
                   "ROM clean defaults");
    if (make_path(session->save_path, sizeof(session->save_path), project_root,
                  "build\\tecmo-season.sav") != 0) {
        session->save_status = TECMO_SEASON_SAVE_IO_ERROR;
        (void)snprintf(session->status, sizeof(session->status),
                       "season save path unavailable");
        return;
    }
    file = fopen(session->save_path, "rb");
    if (file == NULL) return;
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
    (void)snprintf(session->status, sizeof(session->status),
                   "strict TSAV-1 loaded");
}

bool tecmo_season_session_save(TecmoSeasonSession *session)
{
    uint8_t bytes[SEASON_SAVE_SIZE] = {0};
    uint8_t *payload = bytes + SEASON_SAVE_HEADER_SIZE;
    char temp_path[1060];
    char build_path[1024];
    FILE *file;
    int written;
    if (session == NULL || session->save_path[0] == '\0' ||
        session->season_type >= 4U ||
        session->schedule_index >= season_game_count(session->season_type))
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
    (void)snprintf(build_path, sizeof(build_path), "%s", session->save_path);
    {
        char *slash = strrchr(build_path, '\\');
        if (slash != NULL) {
            *slash = '\0';
#ifdef _WIN32
            (void)_mkdir(build_path);
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
    (void)remove(session->save_path);
    if (rename(temp_path, session->save_path) != 0) {
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
        state->schedule_selection = session->schedule_index;
        state->season_type_selection = session->season_type;
    }
}

static bool controls_neutral(const TecmoInput *held)
{
    return !held->up && !held->down && !held->left && !held->right &&
           !held->confirm && !held->cancel && !held->shoot && !held->tab;
}

static bool accept_released(const TecmoControlFrame *controls)
{
    return controls_neutral(&controls->held) && controls->released.shoot;
}

static bool cancel_released(const TecmoControlFrame *controls)
{
    return controls_neutral(&controls->held) && controls->released.cancel;
}

static int repeated_direction(TecmoSeasonState *state,
                              bool negative_held,
                              bool positive_held,
                              bool negative_pressed,
                              bool positive_pressed)
{
    int direction = 0;
    if (negative_held != positive_held)
        direction = negative_held ? -1 : 1;
    if (direction == 0) {
        state->direction_cooldown = 0U;
        return 0;
    }
    if (negative_pressed || positive_pressed || state->direction_cooldown == 0U) {
        state->direction_cooldown = SEASON_DIRECTION_REPEAT;
        return direction;
    }
    --state->direction_cooldown;
    return 0;
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

TecmoSeasonAction tecmo_season_update(TecmoSeasonState *state,
                                      const TecmoSeasonAsset *asset,
                                      TecmoSeasonSession *session,
                                      const TecmoControlFrame *controls)
{
    int direction;
    if (state == NULL || asset == NULL || session == NULL || controls == NULL)
        return TECMO_SEASON_ACTION_NONE;
    ++state->frame;
    if (!asset->available || !valid_runtime_state(asset, session, state))
        return TECMO_SEASON_ACTION_NONE;

    switch (state->phase) {
    case TECMO_SEASON_TEAM_CONTROL:
        if (cancel_released(controls))
            return TECMO_SEASON_ACTION_BACK_TO_START_MENU;
        if (accept_released(controls)) {
            session->team_control[state->team_selection] =
                (uint8_t)((session->team_control[state->team_selection] + 1U) & 3U);
            persist_change(session);
            return TECMO_SEASON_ACTION_NONE;
        }
        direction = repeated_direction(state, controls->held.up,
                                       controls->held.down,
                                       controls->pressed.up,
                                       controls->pressed.down);
        if (direction != 0) {
            uint8_t column = (uint8_t)(state->team_selection / 9U);
            uint8_t row = (uint8_t)(state->team_selection % 9U);
            row = wrap_u8(row, 9U, direction);
            state->team_selection = (uint8_t)(column * 9U + row);
            return TECMO_SEASON_ACTION_NONE;
        }
        direction = repeated_direction(state, controls->held.left,
                                       controls->held.right,
                                       controls->pressed.left,
                                       controls->pressed.right);
        if (direction != 0) {
            uint8_t column = (uint8_t)(state->team_selection / 9U);
            uint8_t row = (uint8_t)(state->team_selection % 9U);
            column = wrap_u8(column, 3U, direction);
            state->team_selection = (uint8_t)(column * 9U + row);
        }
        break;

    case TECMO_SEASON_SCHEDULE:
        if (cancel_released(controls))
            return TECMO_SEASON_ACTION_BACK_TO_START_MENU;
        if (accept_released(controls)) {
            state->phase = TECMO_SEASON_SCHEDULE_POPUP;
            state->popup_selection = 0U;
            state->direction_cooldown = 0U;
            break;
        }
        direction = repeated_direction(state, controls->held.up,
                                       controls->held.down,
                                       controls->pressed.up,
                                       controls->pressed.down);
        if (direction != 0) {
            uint16_t count = asset->game_counts[session->season_type];
            state->schedule_selection = wrap_u16(state->schedule_selection,
                                                 count, direction);
            session->schedule_index = state->schedule_selection;
            persist_change(session);
        }
        break;

    case TECMO_SEASON_SCHEDULE_POPUP:
        if (cancel_released(controls)) {
            state->phase = TECMO_SEASON_SCHEDULE;
            break;
        }
        if (accept_released(controls)) {
            state->phase = state->popup_selection == 0U
                               ? TECMO_SEASON_PLAYOFF
                               : TECMO_SEASON_RESET_CONFIRM;
            state->popup_selection = 0U;
            break;
        }
        direction = repeated_direction(state, controls->held.up,
                                       controls->held.down,
                                       controls->pressed.up,
                                       controls->pressed.down);
        if (direction != 0)
            state->popup_selection = wrap_u8(state->popup_selection, 2U,
                                             direction);
        break;

    case TECMO_SEASON_PLAYOFF:
        if (cancel_released(controls) || accept_released(controls))
            state->phase = TECMO_SEASON_SCHEDULE;
        break;

    case TECMO_SEASON_RESET_CONFIRM:
        if (cancel_released(controls)) {
            state->phase = TECMO_SEASON_SCHEDULE_POPUP;
            state->popup_selection = 1U;
            break;
        }
        if (accept_released(controls)) {
            if (state->popup_selection == 0U) {
                state->phase = TECMO_SEASON_SCHEDULE_POPUP;
                state->popup_selection = 1U;
            } else {
                state->phase = TECMO_SEASON_TYPE_SELECT;
                state->season_type_selection = session->season_type;
                state->popup_selection = 0U;
            }
            break;
        }
        direction = repeated_direction(state, controls->held.up,
                                       controls->held.down,
                                       controls->pressed.up,
                                       controls->pressed.down);
        if (direction != 0)
            state->popup_selection = wrap_u8(state->popup_selection, 2U,
                                             direction);
        break;

    case TECMO_SEASON_TYPE_SELECT:
        if (cancel_released(controls)) {
            state->phase = TECMO_SEASON_SCHEDULE;
            break;
        }
        if (accept_released(controls)) {
            reset_for_type(session, state->season_type_selection);
            state->schedule_selection = 0U;
            state->phase = session->season_type == TECMO_SEASON_PROGRAMMED
                               ? TECMO_SEASON_PROGRAMMED_EDITOR
                               : TECMO_SEASON_SCHEDULE;
            state->editor_panel = 0U;
            state->editor_team = 0U;
            break;
        }
        direction = repeated_direction(state, controls->held.up,
                                       controls->held.down,
                                       controls->pressed.up,
                                       controls->pressed.down);
        if (direction != 0)
            state->season_type_selection = wrap_u8(
                state->season_type_selection, 4U, direction);
        break;

    case TECMO_SEASON_STANDINGS:
        if (cancel_released(controls))
            return TECMO_SEASON_ACTION_BACK_TO_START_MENU;
        direction = repeated_direction(state, controls->held.left,
                                       controls->held.right,
                                       controls->pressed.left,
                                       controls->pressed.right);
        if (direction != 0) state->standings_page ^= 1U;
        break;

    case TECMO_SEASON_PROGRAMMED_EDITOR:
        if (controls->pressed.confirm || controls->pressed.tab ||
            controls->released.confirm || controls->released.tab)
            return TECMO_SEASON_ACTION_BACK_TO_START_MENU;
        direction = repeated_direction(state, controls->held.up,
                                       controls->held.down,
                                       controls->pressed.up,
                                       controls->pressed.down);
        if (direction != 0) {
            uint8_t count = state->editor_panel < 2U ? 13U : 14U;
            state->editor_team = wrap_u8(state->editor_team, count, direction);
            break;
        }
        direction = repeated_direction(state, controls->held.left,
                                       controls->held.right,
                                       controls->pressed.left,
                                       controls->pressed.right);
        if (direction != 0) {
            state->editor_panel = wrap_u8(state->editor_panel, 4U, direction);
            state->editor_team = 0U;
            state->standings_page = state->editor_panel >= 2U ? 1U : 0U;
            break;
        }
        if (accept_released(controls) || cancel_released(controls)) {
            uint8_t team = state->editor_panel < 2U
                               ? state->editor_team
                               : (uint8_t)(13U + state->editor_team);
            uint8_t *value = (state->editor_panel & 1U) == 0U
                                 ? &session->wins[team]
                                 : &session->losses[team];
            uint8_t other = (state->editor_panel & 1U) == 0U
                                ? session->losses[team]
                                : session->wins[team];
            if (accept_released(controls)) {
                *value = *value >= 82U ? 0U : (uint8_t)(*value + 1U);
                if ((unsigned)*value + other > 82U) *value = 0U;
            } else {
                *value = *value == 0U ? (uint8_t)(82U - other)
                                      : (uint8_t)(*value - 1U);
            }
            persist_change(session);
        }
        break;

    case TECMO_SEASON_LEADERS:
        if (cancel_released(controls))
            return TECMO_SEASON_ACTION_BACK_TO_START_MENU;
        direction = repeated_direction(
            state, controls->held.left || controls->held.up,
            controls->held.right || controls->held.down,
            controls->pressed.left || controls->pressed.up,
            controls->pressed.right || controls->pressed.down);
        if (direction != 0)
            state->leader_category = wrap_u8(state->leader_category,
                                             TECMO_SEASON_LEADER_COUNT,
                                             direction);
        break;

    case TECMO_SEASON_GAME_START:
        if (cancel_released(controls))
            return TECMO_SEASON_ACTION_BACK_TO_START_MENU;
        if (accept_released(controls)) state->game_launch_blocked = true;
        direction = repeated_direction(state, controls->held.up,
                                       controls->held.down,
                                       controls->pressed.up,
                                       controls->pressed.down);
        if (direction != 0) {
            uint16_t count = asset->game_counts[session->season_type];
            state->schedule_selection = wrap_u16(state->schedule_selection,
                                                 count, direction);
        }
        break;
    }
    return TECMO_SEASON_ACTION_NONE;
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
        char away[17];
        char home[17];
        if (game >= count) break;
        team_label(away, team_data, asset->schedule[game][0] & 0x3FU);
        team_label(home, team_data, asset->schedule[game][1] & 0x3FU);
        (void)snprintf(line, sizeof(line), " %03u %.8s - %.8s",
                       (unsigned)game + 1U, away, home);
        draw_text(view, asset, 4U, 0U, line, 16, 64 + (int)row * 16,
                  scale, 2, chr, chr_count);
        if (game == state->schedule_selection)
            draw_cursor(view, asset, chr, chr_count, 0,
                        64 + (int)row * 16, scale);
    }
}

static void draw_popup(TecmoFramebuffer *view,
                       const TecmoSeasonAsset *asset,
                       size_t screen,
                       const char *title,
                       const char *const *options,
                       size_t option_count,
                       size_t selection,
                       const uint8_t *chr,
                       uint64_t chr_count,
                       int scale)
{
    int height = 40 + (int)option_count * 16;
    int y = (240 - height) / 2;
    fill_rect(view, 32, y, 192, height, scale,
              tecmo_nes_2c02_rgba(0x0FU));
    draw_text(view, asset, screen, 0U, title, 48, y + 8, scale, 2,
              chr, chr_count);
    for (size_t i = 0U; i < option_count; ++i) {
        char line[24];
        (void)snprintf(line, sizeof(line), "  %s", options[i]);
        draw_text(view, asset, screen, 0U, line, 56,
                  y + 32 + (int)i * 16, scale, 2, chr, chr_count);
        if (i == selection)
            draw_cursor(view, asset, chr, chr_count, 40,
                        y + 32 + (int)i * 16, scale);
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
    static const uint8_t division_start[4] = {0U, 6U, 13U, 20U};
    static const uint8_t division_count[4] = {6U, 7U, 7U, 7U};
    static const char *division_name[4] = {
        "ATLANTIC", "CENTRAL", "MIDWEST", "PACIFIC"
    };
    size_t page = state->phase == TECMO_SEASON_PROGRAMMED_EDITOR
                      ? (state->editor_panel >= 2U ? 1U : 0U)
                      : state->standings_page;
    draw_screen(view, asset, 2U, page, chr, chr_count, scale);
    for (size_t column = 0U; column < 2U; ++column) {
        size_t division = page * 2U + column;
        int x = column == 0U ? 8 : 136;
        draw_text(view, asset, 2U, page, division_name[division], x, 40,
                  scale, 2, chr, chr_count);
        for (size_t row = 0U; row < division_count[division]; ++row) {
            uint8_t team = (uint8_t)(division_start[division] + row);
            char name[17];
            char line[18];
            team_label(name, team_data, team);
            (void)snprintf(line, sizeof(line), "%.8s %02u %02u", name,
                           session->wins[team], session->losses[team]);
            draw_text(view, asset, 2U, page, line, x, 64 + (int)row * 16,
                      scale, 2, chr, chr_count);
        }
    }
    if (state->phase == TECMO_SEASON_PROGRAMMED_EDITOR) {
        uint8_t team = state->editor_panel < 2U
                           ? state->editor_team
                           : (uint8_t)(13U + state->editor_team);
        size_t division = team < 6U ? 0U : team < 13U ? 1U
                                : team < 20U ? 2U : 3U;
        int x = (division & 1U) == 0U ? 0 : 128;
        int row = (int)(team - division_start[division]);
        draw_cursor(view, asset, chr, chr_count, x,
                    64 + row * 16, scale);
        draw_text(view, asset, 2U, page,
                  (state->editor_panel & 1U) == 0U
                      ? "A/B EDIT WINS  SPACE/TAB EXIT"
                      : "A/B EDIT LOSSES SPACE/TAB EXIT",
                  8, 216, scale, 2, chr, chr_count);
    } else {
        draw_text(view, asset, 2U, page,
                  page == 0U ? "< EASTERN >" : "< WESTERN >",
                  80, 216, scale, 2, chr, chr_count);
    }
}

static bool valid_runtime_state(const TecmoSeasonAsset *asset,
                                const TecmoSeasonSession *session,
                                const TecmoSeasonState *state)
{
    uint16_t game_count;
    if (asset == NULL || session == NULL || state == NULL ||
        session->season_type >= 4U ||
        state->phase > TECMO_SEASON_GAME_START)
        return false;
    game_count = asset->game_counts[session->season_type];
    if (game_count == 0U || game_count > TECMO_SEASON_SCHEDULE_COUNT ||
        session->schedule_index >= game_count ||
        state->schedule_selection >= game_count ||
        state->team_selection >= TECMO_SEASON_TEAM_COUNT ||
        state->popup_selection >= 2U ||
        state->season_type_selection >= 4U || state->standings_page >= 2U ||
        state->editor_panel >= 4U || state->leader_category >= 7U ||
        state->editor_team >= (state->editor_panel < 2U ? 13U : 14U))
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
    static const char *schedule_options[2] = {"PLAYOFF", "RESET"};
    static const char *reset_options[2] = {"NO", "YES"};
    static const char *season_options[4] = {
        "REGULAR", "REDUCED", "SHORT", "PROGRAMMED"
    };
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
        draw_text(&view, asset, 4U, 0U, "SCHEDULE", 96, 24,
                  scale, 2, chr_bytes, chr_byte_count);
        draw_text(&view, asset, 4U, 0U,
                  tecmo_season_type_name(session->season_type), 88, 40,
                  scale, 2, chr_bytes, chr_byte_count);
        draw_schedule_rows(&view, asset, session, state, team_data,
                           chr_bytes, chr_byte_count, scale);
        if (state->phase == TECMO_SEASON_SCHEDULE_POPUP)
            draw_popup(&view, asset, 4U, "SCHEDULE", schedule_options, 2U,
                       state->popup_selection, chr_bytes, chr_byte_count, scale);
        else if (state->phase == TECMO_SEASON_RESET_CONFIRM)
            draw_popup(&view, asset, 4U, "RESET SEASON?", reset_options, 2U,
                       state->popup_selection, chr_bytes, chr_byte_count, scale);
        else if (state->phase == TECMO_SEASON_TYPE_SELECT)
            draw_popup(&view, asset, 4U, "SEASON TYPE", season_options, 4U,
                       state->season_type_selection, chr_bytes, chr_byte_count,
                       scale);
        break;
    case TECMO_SEASON_PLAYOFF:
        draw_screen(&view, asset, 0U, 0U, chr_bytes, chr_byte_count, scale);
        break;
    case TECMO_SEASON_STANDINGS:
    case TECMO_SEASON_PROGRAMMED_EDITOR:
        draw_standings(&view, asset, session, state, team_data,
                       chr_bytes, chr_byte_count, scale);
        break;
    case TECMO_SEASON_LEADERS:
        draw_screen(&view, asset, 1U, 0U, chr_bytes, chr_byte_count, scale);
        draw_text(&view, asset, 1U, 0U,
                  asset->leader_labels[state->leader_category], 48, 88,
                  scale, 2, chr_bytes, chr_byte_count);
        draw_text(&view, asset, 1U, 0U, "NO STATISTICS RECORDED", 40, 120,
                  scale, 2, chr_bytes, chr_byte_count);
        draw_text(&view, asset, 1U, 0U, "< CHANGE CATEGORY >", 48, 200,
                  scale, 2, chr_bytes, chr_byte_count);
        break;
    case TECMO_SEASON_GAME_START: {
        uint16_t game = state->schedule_selection;
        char away[17];
        char home[17];
        char line[34];
        draw_screen(&view, asset, 4U, 0U, chr_bytes, chr_byte_count, scale);
        team_label(away, team_data, asset->schedule[game][0] & 0x3FU);
        team_label(home, team_data, asset->schedule[game][1] & 0x3FU);
        (void)snprintf(line, sizeof(line), "GAME %03u", (unsigned)game + 1U);
        draw_text(&view, asset, 4U, 0U, line, 88, 64, scale, 2,
                  chr_bytes, chr_byte_count);
        (void)snprintf(line, sizeof(line), "%.12s", away);
        draw_text(&view, asset, 4U, 0U, line, 80, 96, scale, 2,
                  chr_bytes, chr_byte_count);
        draw_text(&view, asset, 4U, 0U, "AT", 120, 120, scale, 2,
                  chr_bytes, chr_byte_count);
        (void)snprintf(line, sizeof(line), "%.12s", home);
        draw_text(&view, asset, 4U, 0U, line, 80, 144, scale, 2,
                  chr_bytes, chr_byte_count);
        draw_text(&view, asset, 4U, 0U,
                  state->game_launch_blocked
                      ? "GAME LAUNCH NOT YET PORTED"
                      : "PRESS A TO START  B TO RETURN",
                  16, 200, scale, 2, chr_bytes, chr_byte_count);
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

bool tecmo_season_self_test(char *message, size_t message_size)
{
    TecmoSeasonAsset asset;
    TecmoSeasonSession session;
    TecmoSeasonSession loaded;
    TecmoSeasonState state;
    TecmoControlFrame controls;
    uint8_t bytes[SEASON_SAVE_SIZE] = {0};
    uint8_t *payload = bytes + SEASON_SAVE_HEADER_SIZE;
    const char *temp_root;
    char temp_save[1024] = {0};
    FILE *file;
    size_t got;
    int trailing;
    memset(&asset, 0, sizeof(asset));
    asset.available = true;
    asset.game_counts[0] = 1107U;
    asset.game_counts[1] = 567U;
    asset.game_counts[2] = 351U;
    asset.game_counts[3] = 1107U;
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
    if (tecmo_season_update(&state, &asset, &session, &controls) !=
            TECMO_SEASON_ACTION_NONE ||
        session.team_control[0] != 1U) {
        if (message != NULL && message_size > 0U)
            (void)snprintf(message, message_size,
                           "Season TEAM CONTROL update self-test failed.");
        return false;
    }
    memset(&controls, 0, sizeof(controls));
    controls.released.cancel = true;
    if (tecmo_season_update(&state, &asset, &session, &controls) !=
        TECMO_SEASON_ACTION_BACK_TO_START_MENU) {
        if (message != NULL && message_size > 0U)
            (void)snprintf(message, message_size,
                           "Season TEAM CONTROL return self-test failed.");
        return false;
    }

    tecmo_season_state_init(&state, TECMO_SEASON_ROUTE_SCHEDULE, &session);
    memset(&controls, 0, sizeof(controls));
    controls.released.shoot = true;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    if (state.phase != TECMO_SEASON_SCHEDULE_POPUP) goto state_failure;
    memset(&controls, 0, sizeof(controls));
    controls.held.down = true;
    controls.pressed.down = true;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    memset(&controls, 0, sizeof(controls));
    controls.released.shoot = true;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    if (state.phase != TECMO_SEASON_RESET_CONFIRM) goto state_failure;
    memset(&controls, 0, sizeof(controls));
    controls.held.down = true;
    controls.pressed.down = true;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    memset(&controls, 0, sizeof(controls));
    controls.released.shoot = true;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    if (state.phase != TECMO_SEASON_TYPE_SELECT) goto state_failure;
    state.season_type_selection = TECMO_SEASON_PROGRAMMED;
    memset(&controls, 0, sizeof(controls));
    controls.released.shoot = true;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    if (state.phase != TECMO_SEASON_PROGRAMMED_EDITOR ||
        session.season_type != TECMO_SEASON_PROGRAMMED)
        goto state_failure;
    memset(&controls, 0, sizeof(controls));
    controls.released.shoot = true;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    if (session.wins[0] != 1U) goto state_failure;
    memset(&controls, 0, sizeof(controls));
    controls.released.cancel = true;
    (void)tecmo_season_update(&state, &asset, &session, &controls);
    if (session.wins[0] != 0U) goto state_failure;
    memset(&controls, 0, sizeof(controls));
    controls.pressed.confirm = true;
    if (tecmo_season_update(&state, &asset, &session, &controls) !=
        TECMO_SEASON_ACTION_BACK_TO_START_MENU)
        goto state_failure;

    tecmo_season_state_init(&state, TECMO_SEASON_ROUTE_GAME_START, &session);
    memset(&controls, 0, sizeof(controls));
    controls.released.shoot = true;
    if (tecmo_season_update(&state, &asset, &session, &controls) !=
            TECMO_SEASON_ACTION_NONE ||
        state.phase != TECMO_SEASON_GAME_START || !state.game_launch_blocked ||
        session.schedule_index != 0U)
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
        loaded.losses[4] != 32U)
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
