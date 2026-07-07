#include "tecmo_intro_arena.h"
#include "tecmo_nes_video.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARENA_SCREEN_NAMETABLE_FRAME_FIRST 428
#define ARENA_SCREEN_NAMETABLE_FRAME_LAST 430
#define ARENA_SCREEN_ATTRIBUTE_FRAME_FIRST 427
#define ARENA_SCREEN_ATTRIBUTE_FRAME_LAST 430
#define ARENA_SCREEN_PALETTE_FRAME_FIRST 425
#define ARENA_SCREEN_PALETTE_FRAME_LAST 476
#define ARENA_SCREEN_DEFAULT_CHR_BANK 2U
#define ARENA_SCREEN_DEFAULT_LOWER_CHR_BANK 11U
#define ARENA_SCREEN_DEFAULT_SPRITE_CHR_BANK 1U
#define ARENA_SCREEN_OAM_FRAME_FIRST 461
#define ARENA_SCREEN_OAM_FRAME_LAST 520
#define ARENA_SCREEN_CHR_1KB_BYTES 1024ULL
#define ARENA_SCREEN_BG_UPPER_R0 0x14U
#define ARENA_SCREEN_BG_UPPER_R1 0x16U
#define ARENA_SCREEN_BG_LOWER_R0 0x5EU
#define ARENA_SCREEN_BG_LOWER_R1 0x60U
#define ARENA_SCREEN_BG_SPLIT_ROW 26
#define ARENA_SCREEN_TOP_ROWS 16
#define ARENA_SCREEN_SMALL_CROWD_PAGE 1U
#define ARENA_SCREEN_SMALL_CROWD_FIRST_ROW 1
#define ARENA_SCREEN_SMALL_CROWD_ROWS 13
#define ARENA_SCREEN_LARGE_CROWD_PAGE 0U
#define ARENA_SCREEN_LARGE_CROWD_FIRST_ROW 16
#define ARENA_SCREEN_LARGE_CROWD_ROWS 13

typedef struct ArenaCaptureBuild {
    uint8_t tiles[TECMO_INTRO_ARENA_PAGE_COUNT][TECMO_INTRO_ARENA_TILES_PER_PAGE];
    bool tile_present[TECMO_INTRO_ARENA_PAGE_COUNT][TECMO_INTRO_ARENA_TILES_PER_PAGE];
} ArenaCaptureBuild;

static const uint8_t ARENA_DEFAULT_PALETTE[16] = {
    0x0FU, 0x02U, 0x00U, 0x30U,
    0x0FU, 0x00U, 0x01U, 0x05U,
    0x0FU, 0x15U, 0x17U, 0x12U,
    0x0FU, 0x15U, 0x17U, 0x37U,
};

static bool arena_tile_position(uint16_t ppu, int *row, int *col)
{
    uint16_t page_offset;

    if (row == NULL || col == NULL || ppu < 0x2000U || ppu >= 0x3000U) {
        return false;
    }

    page_offset = (uint16_t)((ppu - 0x2000U) % 0x400U);
    if (page_offset >= 0x3C0U) {
        return false;
    }

    *row = (int)(page_offset / 32U);
    *col = (int)(page_offset % 32U);
    return true;
}

static uint8_t arena_attribute_byte(const uint8_t attributes[64], int row, int col)
{
    size_t index;

    if (attributes == NULL) {
        return 0U;
    }
    index = (size_t)(row / 4) * 8U + (size_t)(col / 4);
    if (index >= 64U) {
        return 0U;
    }
    return attributes[index];
}

static void arena_build_palette(uint32_t out_palette[4],
                                const uint8_t background_palette[16],
                                uint8_t palette_index)
{
    uint8_t base = (uint8_t)((palette_index & 0x03U) * 4U);

    out_palette[0] = 0x00000000U;
    if (background_palette == NULL) {
        out_palette[1] = 0xFFFFFFFFU;
        out_palette[2] = 0xFFFFFFFFU;
        out_palette[3] = 0xFFFFFFFFU;
        return;
    }
    for (size_t i = 1; i < 4U; ++i) {
        out_palette[i] = tecmo_nes_2c02_rgba(background_palette[(size_t)base + i]);
    }
}

static uint64_t arena_mmc3_bg_tile_offset(const TecmoIntroArenaCapture *capture,
                                          unsigned page,
                                          int row,
                                          uint8_t tile)
{
    uint8_t r0;
    uint8_t r1;
    uint8_t slot;
    uint8_t local_tile = (uint8_t)(tile & 0x3FU);

    if (capture == NULL) {
        r0 = ARENA_SCREEN_BG_UPPER_R0;
        r1 = ARENA_SCREEN_BG_UPPER_R1;
    } else if (page == 0U && row < capture->bg_split_row) {
        r0 = capture->bg_upper_r0;
        r1 = capture->bg_upper_r1;
    } else {
        r0 = capture->bg_lower_r0;
        r1 = capture->bg_lower_r1;
    }

    switch (tile >> 6U) {
    case 0U:
        slot = (uint8_t)(r0 & 0xFEU);
        break;
    case 1U:
        slot = (uint8_t)((r0 & 0xFEU) + 1U);
        break;
    case 2U:
        slot = (uint8_t)(r1 & 0xFEU);
        break;
    default:
        slot = (uint8_t)((r1 & 0xFEU) + 1U);
        break;
    }

    return (uint64_t)slot * ARENA_SCREEN_CHR_1KB_BYTES + (uint64_t)local_tile * 16ULL;
}

static void arena_status(TecmoIntroArenaCapture *capture, const char *text)
{
    if (capture == NULL) {
        return;
    }
    if (text == NULL) {
        text = "";
    }
    (void)snprintf(capture->status, sizeof(capture->status), "%s", text);
}

static char *read_text_file(const char *path, size_t *size_out)
{
    FILE *file = fopen(path, "rb");
    long size;
    char *buffer;

    if (size_out != NULL) {
        *size_out = 0;
    }
    if (file == NULL) {
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    size = ftell(file);
    if (size < 0) {
        fclose(file);
        return NULL;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    buffer = (char *)malloc((size_t)size + 1U);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }
    if (size > 0 && fread(buffer, 1U, (size_t)size, file) != (size_t)size) {
        free(buffer);
        fclose(file);
        return NULL;
    }
    fclose(file);

    buffer[size] = '\0';
    if (size_out != NULL) {
        *size_out = (size_t)size;
    }
    return buffer;
}

static const char *skip_json_spaces(const char *cursor, const char *end)
{
    while (cursor < end && isspace((unsigned char)*cursor)) {
        ++cursor;
    }
    return cursor;
}

static const char *find_json_value(const char *start, const char *end, const char *key)
{
    char pattern[96];
    const char *cursor = start;
    size_t key_len = strlen(key);

    if (key_len + 3U >= sizeof(pattern)) {
        return NULL;
    }
    (void)snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    while ((cursor = strstr(cursor, pattern)) != NULL && cursor < end) {
        const char *colon = strchr(cursor + strlen(pattern), ':');
        if (colon == NULL || colon >= end) {
            return NULL;
        }
        return skip_json_spaces(colon + 1, end);
    }
    return NULL;
}

static bool json_int_field(const char *start, const char *end, const char *key, int *out)
{
    const char *value = find_json_value(start, end, key);
    char *parse_end;
    long parsed;

    if (value == NULL || value >= end) {
        return false;
    }
    parsed = strtol(value, &parse_end, 10);
    if (parse_end == value || parse_end > end) {
        return false;
    }
    *out = (int)parsed;
    return true;
}

static bool json_string_span(const char *start,
                             const char *end,
                             const char *key,
                             const char **value_start,
                             const char **value_end)
{
    const char *value = find_json_value(start, end, key);
    const char *close;

    if (value_start == NULL || value_end == NULL) {
        return false;
    }
    *value_start = NULL;
    *value_end = NULL;
    if (value == NULL || value >= end || *value != '"') {
        return false;
    }
    close = strchr(value + 1, '"');
    if (close == NULL || close >= end) {
        return false;
    }
    *value_start = value + 1;
    *value_end = close;
    return true;
}

static bool json_string_field_equals(const char *start,
                                     const char *end,
                                     const char *key,
                                     const char *expected)
{
    const char *value_start;
    const char *value_end;
    size_t expected_len;

    if (!json_string_span(start, end, key, &value_start, &value_end) || expected == NULL) {
        return false;
    }
    expected_len = strlen(expected);
    return (size_t)(value_end - value_start) == expected_len &&
           strncmp(value_start, expected, expected_len) == 0;
}

static bool parse_hex_pair(const char **cursor_io,
                           const char *end,
                           unsigned *address_out,
                           unsigned *value_out)
{
    const char *cursor;
    char *parse_end;
    unsigned long address;
    unsigned long value;

    if (cursor_io == NULL || *cursor_io == NULL || address_out == NULL || value_out == NULL) {
        return false;
    }
    cursor = *cursor_io;
    while (cursor < end && (*cursor == ',' || isspace((unsigned char)*cursor))) {
        ++cursor;
    }
    if (cursor >= end) {
        *cursor_io = cursor;
        return false;
    }

    address = strtoul(cursor, &parse_end, 16);
    if (parse_end == cursor || parse_end >= end || *parse_end != '=') {
        *cursor_io = parse_end > cursor ? parse_end : cursor + 1;
        return false;
    }
    cursor = parse_end + 1;
    value = strtoul(cursor, &parse_end, 16);
    if (parse_end == cursor) {
        *cursor_io = cursor + 1;
        return false;
    }

    *address_out = (unsigned)address;
    *value_out = (unsigned)value;
    cursor = parse_end;
    while (cursor < end && *cursor != ',') {
        ++cursor;
    }
    if (cursor < end && *cursor == ',') {
        ++cursor;
    }
    *cursor_io = cursor;
    return true;
}

static void store_nametable_write(ArenaCaptureBuild *build, unsigned address, unsigned value)
{
    unsigned page;
    unsigned offset;

    if (build == NULL || value > 0xFFU || address < 0x2000U || address >= 0x2800U) {
        return;
    }

    page = address >= 0x2400U ? 1U : 0U;
    offset = address - (page == 0U ? 0x2000U : 0x2400U);
    if (offset >= TECMO_INTRO_ARENA_TILES_PER_PAGE) {
        return;
    }

    build->tiles[page][offset] = (uint8_t)value;
    build->tile_present[page][offset] = true;
}

static void store_attribute_write(TecmoIntroArenaCapture *capture, unsigned address, unsigned value)
{
    unsigned page;
    unsigned index;

    if (capture == NULL || value > 0xFFU) {
        return;
    }
    if (address >= 0x23C0U && address < 0x2400U) {
        page = 0U;
        index = address - 0x23C0U;
    } else if (address >= 0x27C0U && address < 0x2800U) {
        page = 1U;
        index = address - 0x27C0U;
    } else {
        return;
    }

    capture->attributes[page][index] = (uint8_t)value;
}

static void parse_nametable_pairs(ArenaCaptureBuild *build, const char *start, const char *end)
{
    const char *cursor = start;
    while (cursor < end) {
        unsigned address;
        unsigned value;
        if (parse_hex_pair(&cursor, end, &address, &value)) {
            store_nametable_write(build, address, value);
        }
    }
}

static void parse_attribute_pairs(TecmoIntroArenaCapture *capture, const char *start, const char *end)
{
    const char *cursor = start;
    while (cursor < end) {
        unsigned address;
        unsigned value;
        if (parse_hex_pair(&cursor, end, &address, &value)) {
            store_attribute_write(capture, address, value);
        }
    }
}

static void parse_palette_stage(TecmoIntroArenaCapture *capture,
                                int frame,
                                const char *start,
                                const char *end)
{
    uint8_t stage[16];
    uint8_t sprite_stage[16];
    const char *cursor = start;

    if (capture == NULL ||
        capture->palette_stage_count >= TECMO_INTRO_ARENA_PALETTE_STAGE_COUNT) {
        return;
    }

    if (capture->palette_stage_count > 0U) {
        memcpy(stage,
               capture->palette_stages[capture->palette_stage_count - 1U],
               sizeof(stage));
        memcpy(sprite_stage,
               capture->sprite_palette_stages[capture->palette_stage_count - 1U],
               sizeof(sprite_stage));
    } else {
        for (size_t i = 0; i < sizeof(stage); ++i) {
            stage[i] = 0x0FU;
            sprite_stage[i] = 0x0FU;
        }
        capture->first_capture_frame = (unsigned)frame;
    }

    while (cursor < end) {
        unsigned address;
        unsigned value;
        if (parse_hex_pair(&cursor, end, &address, &value)) {
            if (address >= 0x3F00U && address < 0x3F10U && value <= 0xFFU) {
                stage[address - 0x3F00U] = (uint8_t)value;
            } else if (address >= 0x3F10U && address < 0x3F20U && value <= 0xFFU) {
                sprite_stage[address - 0x3F10U] = (uint8_t)value;
            }
        }
    }

    memcpy(capture->palette_stages[capture->palette_stage_count], stage, sizeof(stage));
    memcpy(capture->sprite_palette_stages[capture->palette_stage_count], sprite_stage, sizeof(sprite_stage));
    capture->palette_stage_offsets[capture->palette_stage_count] =
        (unsigned)frame - capture->first_capture_frame;
    ++capture->palette_stage_count;
    capture->palette_available = true;
}

static bool parse_oam_quad(const char **cursor_io,
                           const char *end,
                           TecmoIntroArenaSprite *sprite_out)
{
    const char *cursor;
    char *parse_end;
    unsigned long ignored_slot;
    unsigned long y;
    unsigned long tile;
    unsigned long attributes;
    unsigned long x;

    if (cursor_io == NULL || *cursor_io == NULL || sprite_out == NULL) {
        return false;
    }

    cursor = *cursor_io;
    while (cursor < end && (*cursor == ';' || isspace((unsigned char)*cursor))) {
        ++cursor;
    }
    if (cursor >= end) {
        *cursor_io = cursor;
        return false;
    }

    ignored_slot = strtoul(cursor, &parse_end, 10);
    (void)ignored_slot;
    if (parse_end == cursor || parse_end >= end || *parse_end != ':') {
        *cursor_io = parse_end > cursor ? parse_end : cursor + 1;
        return false;
    }
    cursor = parse_end + 1;

    y = strtoul(cursor, &parse_end, 16);
    if (parse_end == cursor || parse_end >= end || *parse_end != ',') {
        *cursor_io = parse_end > cursor ? parse_end : cursor + 1;
        return false;
    }
    cursor = parse_end + 1;
    tile = strtoul(cursor, &parse_end, 16);
    if (parse_end == cursor || parse_end >= end || *parse_end != ',') {
        *cursor_io = parse_end > cursor ? parse_end : cursor + 1;
        return false;
    }
    cursor = parse_end + 1;
    attributes = strtoul(cursor, &parse_end, 16);
    if (parse_end == cursor || parse_end >= end || *parse_end != ',') {
        *cursor_io = parse_end > cursor ? parse_end : cursor + 1;
        return false;
    }
    cursor = parse_end + 1;
    x = strtoul(cursor, &parse_end, 16);
    if (parse_end == cursor || y > 0xFFUL || tile > 0xFFUL ||
        attributes > 0xFFUL || x > 0xFFUL) {
        *cursor_io = parse_end > cursor ? parse_end : cursor + 1;
        return false;
    }

    sprite_out->y = (uint8_t)y;
    sprite_out->tile = (uint8_t)tile;
    sprite_out->attributes = (uint8_t)attributes;
    sprite_out->x = (uint8_t)x;

    cursor = parse_end;
    while (cursor < end && *cursor != ';') {
        ++cursor;
    }
    if (cursor < end && *cursor == ';') {
        ++cursor;
    }
    *cursor_io = cursor;
    return true;
}

static void parse_visible_oam(TecmoIntroArenaCapture *capture, const char *start, const char *end)
{
    const char *cursor = start;
    size_t count = 0;

    if (capture == NULL) {
        return;
    }

    while (cursor < end && count < TECMO_INTRO_ARENA_MAX_SPRITES) {
        TecmoIntroArenaSprite sprite;
        if (parse_oam_quad(&cursor, end, &sprite)) {
            capture->sprites[count++] = sprite;
        }
    }
    capture->sprite_count = count;
}

static void finalize_tile_lists(TecmoIntroArenaCapture *capture, const ArenaCaptureBuild *build)
{
    if (capture == NULL || build == NULL) {
        return;
    }

    for (unsigned page = 0; page < TECMO_INTRO_ARENA_PAGE_COUNT; ++page) {
        size_t count = 0;
        uint16_t base = (uint16_t)(page == 0U ? 0x2000U : 0x2400U);
        for (unsigned offset = 0; offset < TECMO_INTRO_ARENA_TILES_PER_PAGE; ++offset) {
            if (!build->tile_present[page][offset]) {
                continue;
            }
            capture->tiles[page][count].ppu = (uint16_t)(base + offset);
            capture->tiles[page][count].tile = build->tiles[page][offset];
            ++count;
        }
        capture->tile_count[page] = count;
    }
}

static void append_path(char *dest, size_t dest_size, const char *root, const char *relative)
{
    size_t root_len;

    if (dest == NULL || dest_size == 0U) {
        return;
    }
    dest[0] = '\0';
    if (root == NULL || root[0] == '\0') {
        return;
    }

    (void)snprintf(dest, dest_size, "%s", root);
    root_len = strlen(dest);
    if (root_len > 0U && root_len + 1U < dest_size &&
        dest[root_len - 1U] != '\\' && dest[root_len - 1U] != '/') {
        dest[root_len++] = '\\';
        dest[root_len] = '\0';
    }
    if (root_len < dest_size) {
        (void)snprintf(dest + root_len, dest_size - root_len, "%s", relative);
    }
}

static const char *arena_capture_env_path(char *buffer, size_t buffer_size)
{
#ifdef _WIN32
    char *value = NULL;
    size_t value_size = 0;

    if (buffer == NULL || buffer_size == 0U) {
        return NULL;
    }
    buffer[0] = '\0';
    if (_dupenv_s(&value, &value_size, "TECMO_INTRO_CAPTURE") != 0 ||
        value == NULL || value[0] == '\0') {
        free(value);
        return NULL;
    }
    (void)snprintf(buffer, buffer_size, "%s", value);
    free(value);
    return buffer;
#else
    const char *value;
    (void)buffer;
    (void)buffer_size;
    value = getenv("TECMO_INTRO_CAPTURE");
    return value != NULL && value[0] != '\0' ? value : NULL;
#endif
}

static bool arena_env_uint(const char *name, unsigned *out)
{
    char text[64];
    char *parse_end;
    unsigned long parsed;

    if (name == NULL || out == NULL) {
        return false;
    }

#ifdef _WIN32
    {
        char *value = NULL;
        size_t value_size = 0;
        if (_dupenv_s(&value, &value_size, name) != 0 ||
            value == NULL || value[0] == '\0') {
            free(value);
            return false;
        }
        (void)snprintf(text, sizeof(text), "%s", value);
        free(value);
    }
#else
    {
        const char *value = getenv(name);
        if (value == NULL || value[0] == '\0') {
            return false;
        }
        (void)snprintf(text, sizeof(text), "%s", value);
    }
#endif

    parsed = strtoul(text, &parse_end, 0);
    if (parse_end == text || *parse_end != '\0' || parsed > 255UL) {
        return false;
    }
    *out = (unsigned)parsed;
    return true;
}

static bool load_arena_capture_file(TecmoIntroArenaCapture *capture, const char *path)
{
    ArenaCaptureBuild build;
    char *json;
    size_t json_size = 0;
    const char *cursor;
    const char *json_end;

    json = read_text_file(path, &json_size);
    if (json == NULL) {
        return false;
    }

    memset(&build, 0, sizeof(build));
    cursor = json;
    json_end = json + json_size;

    while (cursor < json_end) {
        const char *line_end = strchr(cursor, '\n');
        int frame = 0;

        if (line_end == NULL || line_end > json_end) {
            line_end = json_end;
        }

        if (json_int_field(cursor, line_end, "frame", &frame)) {
            const char *pair_start;
            const char *pair_end;

            if (frame >= ARENA_SCREEN_NAMETABLE_FRAME_FIRST &&
                frame <= ARENA_SCREEN_NAMETABLE_FRAME_LAST &&
                json_string_field_equals(cursor, line_end, "kind", "ppu_nametable_write_batch") &&
                json_string_span(cursor, line_end, "nametable_writes", &pair_start, &pair_end)) {
                parse_nametable_pairs(&build, pair_start, pair_end);
                capture->last_capture_frame = (unsigned)frame;
            } else if (frame >= ARENA_SCREEN_ATTRIBUTE_FRAME_FIRST &&
                       frame <= ARENA_SCREEN_ATTRIBUTE_FRAME_LAST &&
                       json_string_field_equals(cursor, line_end, "kind", "ppu_attribute_write_batch") &&
                       json_string_span(cursor, line_end, "attribute_writes", &pair_start, &pair_end)) {
                parse_attribute_pairs(capture, pair_start, pair_end);
                capture->last_capture_frame = (unsigned)frame;
            } else if (frame >= ARENA_SCREEN_PALETTE_FRAME_FIRST &&
                       frame <= ARENA_SCREEN_PALETTE_FRAME_LAST &&
                       json_string_field_equals(cursor, line_end, "kind", "ppu_palette_write_batch") &&
                       json_string_span(cursor, line_end, "palette_writes", &pair_start, &pair_end)) {
                parse_palette_stage(capture, frame, pair_start, pair_end);
            } else if (frame >= ARENA_SCREEN_OAM_FRAME_FIRST &&
                       frame <= ARENA_SCREEN_OAM_FRAME_LAST &&
                       json_string_field_equals(cursor, line_end, "kind", "oam_frame_diff") &&
                       json_string_span(cursor, line_end, "visible_oam", &pair_start, &pair_end)) {
                parse_visible_oam(capture, pair_start, pair_end);
            }
        }

        cursor = line_end;
        if (cursor < json_end && *cursor == '\n') {
            ++cursor;
        }
    }

    finalize_tile_lists(capture, &build);
    capture->available = capture->tile_count[0] > 0U;
    if (capture->available) {
        (void)snprintf(capture->status,
                       sizeof(capture->status),
                       "ARENA TRACE %s  P0 %u/CHR%02u P1 %u/CHR%02u  PAL %u",
                       path,
                       (unsigned)capture->tile_count[0],
                       (unsigned)capture->page_chr_bank[0],
                       (unsigned)capture->tile_count[1],
                       (unsigned)capture->page_chr_bank[1],
                       (unsigned)capture->palette_stage_count);
    }

    free(json);
    return capture->available;
}

bool tecmo_intro_arena_capture_load(TecmoIntroArenaCapture *capture, const char *project_root)
{
    char project_path[260];
    char env_path_buffer[260];
    const char *env_path;
    const char *paths[5];

    if (capture == NULL) {
        return false;
    }

    memset(capture, 0, sizeof(*capture));
    capture->chr_bank = ARENA_SCREEN_DEFAULT_CHR_BANK;
    capture->page_chr_bank[0] = ARENA_SCREEN_DEFAULT_CHR_BANK;
    capture->page_chr_bank[1] = ARENA_SCREEN_DEFAULT_LOWER_CHR_BANK;
    capture->bg_upper_r0 = ARENA_SCREEN_BG_UPPER_R0;
    capture->bg_upper_r1 = ARENA_SCREEN_BG_UPPER_R1;
    capture->bg_lower_r0 = ARENA_SCREEN_BG_LOWER_R0;
    capture->bg_lower_r1 = ARENA_SCREEN_BG_LOWER_R1;
    capture->bg_split_row = ARENA_SCREEN_BG_SPLIT_ROW;
    capture->sprite_chr_bank = ARENA_SCREEN_DEFAULT_SPRITE_CHR_BANK;
    {
        unsigned override_bank = 0;
        if (arena_env_uint("TECMO_ARENA_CHR_BANK", &override_bank)) {
            capture->chr_bank = override_bank;
            capture->page_chr_bank[0] = override_bank;
            capture->page_chr_bank[1] = override_bank;
        }
        if (arena_env_uint("TECMO_ARENA_PAGE0_CHR_BANK", &override_bank)) {
            capture->page_chr_bank[0] = override_bank;
            capture->chr_bank = override_bank;
        }
        if (arena_env_uint("TECMO_ARENA_PAGE1_CHR_BANK", &override_bank)) {
            capture->page_chr_bank[1] = override_bank;
        }
        if (arena_env_uint("TECMO_ARENA_UPPER_R0", &override_bank)) {
            capture->bg_upper_r0 = (uint8_t)override_bank;
        }
        if (arena_env_uint("TECMO_ARENA_UPPER_R1", &override_bank)) {
            capture->bg_upper_r1 = (uint8_t)override_bank;
        }
        if (arena_env_uint("TECMO_ARENA_LOWER_R0", &override_bank)) {
            capture->bg_lower_r0 = (uint8_t)override_bank;
        }
        if (arena_env_uint("TECMO_ARENA_LOWER_R1", &override_bank)) {
            capture->bg_lower_r1 = (uint8_t)override_bank;
        }
        if (arena_env_uint("TECMO_ARENA_SPLIT_ROW", &override_bank)) {
            capture->bg_split_row = override_bank > 30U ? 30 : (int)override_bank;
        }
    }
    arena_status(capture, "RUN FCEUX LUA WATCH TO CAPTURE INTRO ARENA SCREEN");

    append_path(project_path,
                sizeof(project_path),
                project_root,
                "build\\emu_intro_memory_watch.ndjson");
    env_path = arena_capture_env_path(env_path_buffer, sizeof(env_path_buffer));

    paths[0] = env_path;
    paths[1] = project_path[0] != '\0' ? project_path : NULL;
    paths[2] = "build\\emu_intro_memory_watch.ndjson";
    paths[3] = "emu_intro_memory_watch.ndjson";
    paths[4] = "..\\build\\emu_intro_memory_watch.ndjson";

    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        if (paths[i] != NULL && paths[i][0] != '\0' &&
            load_arena_capture_file(capture, paths[i])) {
            return true;
        }
    }

    return false;
}

const uint8_t *tecmo_intro_arena_palette_for_frame(const TecmoIntroArenaCapture *capture,
                                                   unsigned frame)
{
    size_t selected = 0;

    if (capture == NULL || capture->palette_stage_count == 0U) {
        return ARENA_DEFAULT_PALETTE;
    }

    for (size_t i = 0; i < capture->palette_stage_count; ++i) {
        if (frame >= capture->palette_stage_offsets[i]) {
            selected = i;
        }
    }
    return capture->palette_stages[selected];
}

static const uint8_t *arena_sprite_palette_for_frame(const TecmoIntroArenaCapture *capture,
                                                     unsigned frame)
{
    size_t selected = 0;

    if (capture == NULL || capture->palette_stage_count == 0U) {
        return ARENA_DEFAULT_PALETTE;
    }

    for (size_t i = 0; i < capture->palette_stage_count; ++i) {
        if (frame >= capture->palette_stage_offsets[i]) {
            selected = i;
        }
    }
    return capture->sprite_palette_stages[selected];
}

static bool arena_draw_tile_entry(TecmoFramebuffer *fb,
                                  const TecmoIntroArenaCapture *capture,
                                  const uint8_t *chr_bytes,
                                  uint64_t chr_byte_count,
                                  const uint8_t *background_palette,
                                  unsigned page,
                                  const TecmoNametableTile *entry,
                                  int display_row,
                                  int origin_x,
                                  int origin_y,
                                  int scale)
{
    uint8_t attribute;
    uint8_t palette_index;
    uint32_t palette[4];
    uint64_t tile_offset;
    int row;
    int col;

    if (entry == NULL || !arena_tile_position(entry->ppu, &row, &col)) {
        return false;
    }

    attribute = arena_attribute_byte(capture->attributes[page], row, col);
    palette_index = tecmo_nes_attribute_palette_index(attribute, row, col);
    arena_build_palette(palette, background_palette, palette_index);
    tile_offset = arena_mmc3_bg_tile_offset(capture, page, row, entry->tile);

    tecmo_draw_chr_tile_at_offset_ex(fb,
                                     chr_bytes,
                                     chr_byte_count,
                                     tile_offset,
                                     origin_x + col * 8 * scale,
                                     origin_y + display_row * 8 * scale,
                                     scale,
                                     palette,
                                     false,
                                     false);
    return true;
}

bool tecmo_intro_arena_draw_page(TecmoFramebuffer *fb,
                                 const TecmoIntroArenaCapture *capture,
                                 const uint8_t *chr_bytes,
                                 uint64_t chr_byte_count,
                                 unsigned page,
                                 unsigned frame,
                                 int origin_x,
                                 int origin_y,
                                 int scale)
{
    const uint8_t *background_palette;

    if (capture == NULL || !capture->available ||
        page >= TECMO_INTRO_ARENA_PAGE_COUNT ||
        capture->tile_count[page] == 0U) {
        return false;
    }
    if (fb == NULL || chr_bytes == NULL || chr_byte_count == 0U || scale <= 0) {
        return false;
    }

    background_palette = tecmo_intro_arena_palette_for_frame(capture, frame);
    for (size_t i = 0; i < capture->tile_count[page]; ++i) {
        const TecmoNametableTile *entry = &capture->tiles[page][i];
        int row;
        int col;

        if (!arena_tile_position(entry->ppu, &row, &col)) {
            continue;
        }
        (void)col;

        (void)arena_draw_tile_entry(fb,
                                    capture,
                                    chr_bytes,
                                    chr_byte_count,
                                    background_palette,
                                    page,
                                    entry,
                                    row,
                                    origin_x,
                                    origin_y,
                                    scale);
    }

    return true;
}

bool tecmo_intro_arena_draw_composite(TecmoFramebuffer *fb,
                                      const TecmoIntroArenaCapture *capture,
                                      const uint8_t *chr_bytes,
                                      uint64_t chr_byte_count,
                                      unsigned frame,
                                      int origin_x,
                                      int origin_y,
                                      int scale)
{
    const uint8_t *background_palette;
    bool drew_any = false;

    if (capture == NULL || !capture->available ||
        fb == NULL || chr_bytes == NULL || chr_byte_count == 0U || scale <= 0) {
        return false;
    }

    background_palette = tecmo_intro_arena_palette_for_frame(capture, frame);

    for (unsigned pass = 0; pass < 3U; ++pass) {
        unsigned page = 0U;
        int source_first_row = 0;
        int source_last_row = 0;
        int destination_first_row = 0;

        if (pass == 0U) {
            page = 0U;
            source_first_row = 0;
            source_last_row = ARENA_SCREEN_TOP_ROWS - 1;
            destination_first_row = 0;
        } else if (pass == 1U) {
            page = ARENA_SCREEN_SMALL_CROWD_PAGE;
            source_first_row = ARENA_SCREEN_SMALL_CROWD_FIRST_ROW;
            source_last_row = source_first_row + ARENA_SCREEN_SMALL_CROWD_ROWS - 1;
            destination_first_row = ARENA_SCREEN_TOP_ROWS;
        } else {
            page = ARENA_SCREEN_LARGE_CROWD_PAGE;
            source_first_row = ARENA_SCREEN_LARGE_CROWD_FIRST_ROW;
            source_last_row = source_first_row + ARENA_SCREEN_LARGE_CROWD_ROWS - 1;
            destination_first_row = ARENA_SCREEN_TOP_ROWS + ARENA_SCREEN_SMALL_CROWD_ROWS;
        }

        if (page >= TECMO_INTRO_ARENA_PAGE_COUNT) {
            continue;
        }

        for (size_t i = 0; i < capture->tile_count[page]; ++i) {
            const TecmoNametableTile *entry = &capture->tiles[page][i];
            int row;
            int col;
            int display_row;

            if (!arena_tile_position(entry->ppu, &row, &col) ||
                row < source_first_row ||
                row > source_last_row) {
                continue;
            }
            (void)col;

            display_row = destination_first_row + (row - source_first_row);
            if (arena_draw_tile_entry(fb,
                                      capture,
                                      chr_bytes,
                                      chr_byte_count,
                                      background_palette,
                                      page,
                                      entry,
                                      display_row,
                                      origin_x,
                                      origin_y,
                                      scale)) {
                drew_any = true;
            }
        }
    }

    return drew_any;
}

bool tecmo_intro_arena_draw_sprites(TecmoFramebuffer *fb,
                                    const TecmoIntroArenaCapture *capture,
                                    const uint8_t *chr_bytes,
                                    uint64_t chr_byte_count,
                                    unsigned frame,
                                    int origin_x,
                                    int origin_y,
                                    int scale)
{
    const uint8_t *sprite_palette;

    if (fb == NULL || capture == NULL || !capture->available ||
        chr_bytes == NULL || chr_byte_count == 0U || scale <= 0) {
        return false;
    }

    sprite_palette = arena_sprite_palette_for_frame(capture, frame);
    for (size_t i = 0; i < capture->sprite_count; ++i) {
        const TecmoIntroArenaSprite *sprite = &capture->sprites[i];
        uint8_t palette_index = (uint8_t)(sprite->attributes & 0x03U);
        bool flip_horizontal = (sprite->attributes & 0x40U) != 0U;
        bool flip_vertical = (sprite->attributes & 0x80U) != 0U;
        uint16_t top_tile = (uint16_t)(sprite->tile & 0xFEU);
        uint16_t bottom_tile = (uint16_t)(top_tile + 1U);
        uint16_t first_tile = flip_vertical ? bottom_tile : top_tile;
        uint16_t second_tile = flip_vertical ? top_tile : bottom_tile;
        uint8_t palette_base = (uint8_t)(palette_index * 4U);
        uint32_t rgba_palette[4];
        int x = origin_x + (int)sprite->x * scale;
        int y = origin_y + (int)sprite->y * scale;

        rgba_palette[0] = 0x00000000U;
        rgba_palette[1] = tecmo_nes_2c02_rgba(sprite_palette[(size_t)palette_base + 1U]);
        rgba_palette[2] = tecmo_nes_2c02_rgba(sprite_palette[(size_t)palette_base + 2U]);
        rgba_palette[3] = tecmo_nes_2c02_rgba(sprite_palette[(size_t)palette_base + 3U]);

        tecmo_draw_chr_tile_ex(fb,
                               chr_bytes,
                               chr_byte_count,
                               capture->sprite_chr_bank,
                               first_tile,
                               x,
                               y,
                               scale,
                               rgba_palette,
                               flip_horizontal,
                               flip_vertical);
        tecmo_draw_chr_tile_ex(fb,
                               chr_bytes,
                               chr_byte_count,
                               capture->sprite_chr_bank,
                               second_tile,
                               x,
                               y + 8 * scale,
                               scale,
                               rgba_palette,
                               flip_horizontal,
                               flip_vertical);
    }

    return capture->sprite_count > 0U;
}
