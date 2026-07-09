#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_intro_arena.h"
#include "tecmo_asset_pack.h"
#include "tecmo_intro_arena_scene.h"
#include "tecmo_intro_stage.h"
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
#define ARENA_SCREEN_OAM_FRAME_LAST 811
#define ARENA_SCREEN_CAPTURE_FRAME_FIRST 464U
#define ARENA_SCREEN_NATIVE_REFERENCE_FRAME 320U
#define ARENA_SCREEN_CAPTURE_REFERENCE_FRAME 386U
#define ARENA_SCREEN_CHR_1KB_BYTES 1024ULL
#define ARENA_SCREEN_BG_UPPER_R0 0x14U
#define ARENA_SCREEN_BG_UPPER_R1 0x16U
#define ARENA_SCREEN_BG_LOWER_R0 0x5EU
#define ARENA_SCREEN_BG_LOWER_R1 0x60U
#define ARENA_SCREEN_BG_SPLIT_ROW 26
#define ARENA_SCREEN_TOP_ROWS 16
#define ARENA_SCREEN_SMALL_CROWD_PAGE 1U
#define ARENA_SCREEN_SMALL_CROWD_FIRST_ROW 1
#define ARENA_SCREEN_SMALL_CROWD_ROWS 22
#define ARENA_SCREEN_LARGE_CROWD_PAGE 0U
#define ARENA_SCREEN_LARGE_CROWD_FIRST_ROW 16
#define ARENA_SCREEN_LARGE_CROWD_ROWS 13
#define ARENA_SCREEN_BASKET_SPRITE_MIN_X 0x80U
#define ARENA_SCREEN_BASKET_SCREEN_X_OFFSET 0
#define ARENA_SCREEN_BASKET_SCREEN_Y_OFFSET 0
#define ARENA_SCREEN_BASKET_STAGE_FRAME_LAG 2
#define ARENA_SCREEN_FINAL_SCROLL_Y 0x64U
#define ARENA_SCREEN_SPRITE_Y_HARDWARE_OFFSET 1
#define ARENA_TILE_LAYER_ENTRY_ID "arena/intro/background-layer"
#define ARENA_TILE_LAYER_HEADER_SIZE 48U
#define ARENA_TILE_LAYER_CELL_STRIDE 6U
#define ARENA_TILE_LAYER_PALETTE_OFFSET 32U
#define ARENA_TILE_LAYER_CELLS_OFFSET 48U
#define ARENA_TILE_LAYER_VERSION 1U
#define ARENA_SPRITE_GROUPS_ENTRY_ID "arena/intro/sprite-groups"
#define ARENA_SPRITE_GROUPS_HEADER_SIZE 48U
#define ARENA_SPRITE_GROUPS_GROUP_STRIDE 20U
#define ARENA_SPRITE_GROUPS_PIECE_STRIDE 12U
#define ARENA_SPRITE_GROUPS_PALETTE_OFFSET 48U
#define ARENA_SPRITE_GROUPS_GROUPS_OFFSET 64U
#define ARENA_SPRITE_GROUPS_PIECES_OFFSET 104U
#define ARENA_SPRITE_GROUPS_VERSION 1U
#define ARENA_SPRITE_GROUPS_FLAGS 1U
#define ARENA_SPRITE_GROUPS_JUMBOTRON_PIECES 55U
#define ARENA_SPRITE_GROUPS_GOAL_PIECES 16U
#define ARENA_SPRITE_VIEWPORT_WIDTH 256
#define ARENA_SPRITE_VIEWPORT_HEIGHT 240
#define ARENA_SPRITE_WIDTH 8
#define ARENA_SPRITE_HEIGHT 16
#define ARENA_SPRITE_PIXEL_TOP_OFFSET 1

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

static const char *ARENA_CAPTURE_ASSET_ENTRIES[] = {
    "intro/arena/capture.ndjson",
    "intro/arena/capture",
    "intro/arena/intro_arena_capture.ndjson",
    "intro_arena_capture.ndjson",
    "intro/arena/emu_intro_memory_watch.ndjson",
    "emu_intro_memory_watch.ndjson",
    "intro/arena/emu_intro_arena_irq_watch.ndjson",
    "emu_intro_arena_irq_watch.ndjson",
};

unsigned tecmo_intro_arena_display_frame(unsigned native_frame)
{
    return (unsigned)(((uint64_t)native_frame * ARENA_SCREEN_CAPTURE_REFERENCE_FRAME +
                       (ARENA_SCREEN_NATIVE_REFERENCE_FRAME / 2U)) /
                      ARENA_SCREEN_NATIVE_REFERENCE_FRAME);
}

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

static uint64_t arena_mmc3_bg_tile_offset_for_registers(uint8_t r0,
                                                        uint8_t r1,
                                                        uint8_t tile)
{
    uint8_t slot;
    uint8_t local_tile = (uint8_t)(tile & 0x3FU);

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

static uint64_t arena_mmc3_bg_tile_offset(const TecmoIntroArenaCapture *capture,
                                          unsigned page,
                                          int row,
                                          uint8_t tile)
{
    uint8_t r0;
    uint8_t r1;

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

    return arena_mmc3_bg_tile_offset_for_registers(r0, r1, tile);
}

static bool arena_chr_tile_offset_available(uint64_t chr_byte_count, uint64_t tile_offset)
{
    return tile_offset + 15ULL < chr_byte_count;
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

static bool arena_file_exists(const char *path)
{
    FILE *file;

    if (path == NULL || path[0] == '\0') {
        return false;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }
    fclose(file);
    return true;
}

static char *read_asset_pack_text_entry(const char *pack_path,
                                        const char *entry_id,
                                        size_t *size_out)
{
    uint8_t *entry_bytes = NULL;
    uint64_t entry_size = 0;
    char *buffer;

    if (size_out != NULL) {
        *size_out = 0U;
    }
    if (pack_path == NULL || entry_id == NULL ||
        tecmo_asset_pack_read_entry(pack_path, entry_id, &entry_bytes, &entry_size) != 0) {
        return NULL;
    }
    if (entry_size > (uint64_t)((size_t)-1) - 1ULL) {
        tecmo_asset_pack_free(entry_bytes);
        return NULL;
    }

    buffer = (char *)malloc((size_t)entry_size + 1U);
    if (buffer == NULL) {
        tecmo_asset_pack_free(entry_bytes);
        return NULL;
    }
    if (entry_size > 0U) {
        memcpy(buffer, entry_bytes, (size_t)entry_size);
    }
    buffer[(size_t)entry_size] = '\0';
    if (size_out != NULL) {
        *size_out = (size_t)entry_size;
    }

    tecmo_asset_pack_free(entry_bytes);
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

static int hex_nibble(int value)
{
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

static bool parse_hex_stream_entry(const char **cursor_io,
                                   const char *end,
                                   unsigned *address_out,
                                   const char **bytes_start_out,
                                   const char **bytes_end_out)
{
    const char *cursor;
    char *parse_end;
    unsigned long address;
    const char *bytes_start;
    const char *bytes_end;

    if (cursor_io == NULL || *cursor_io == NULL || address_out == NULL ||
        bytes_start_out == NULL || bytes_end_out == NULL) {
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

    address = strtoul(cursor, &parse_end, 16);
    if (parse_end == cursor || parse_end >= end || *parse_end != ':') {
        *cursor_io = parse_end > cursor ? parse_end : cursor + 1;
        return false;
    }

    bytes_start = parse_end + 1;
    bytes_end = bytes_start;
    while (bytes_end < end && isxdigit((unsigned char)*bytes_end)) {
        ++bytes_end;
    }

    cursor = bytes_end;
    while (cursor < end && *cursor != ';') {
        ++cursor;
    }
    if (cursor < end && *cursor == ';') {
        ++cursor;
    }

    *address_out = (unsigned)address;
    *bytes_start_out = bytes_start;
    *bytes_end_out = bytes_end;
    *cursor_io = cursor;
    return bytes_end > bytes_start;
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

static void parse_nametable_snapshot_rows(ArenaCaptureBuild *build, const char *start, const char *end)
{
    const char *cursor = start;

    while (cursor < end) {
        unsigned address;
        const char *bytes_start;
        const char *bytes_end;
        const char *byte_cursor;

        if (!parse_hex_stream_entry(&cursor, end, &address, &bytes_start, &bytes_end)) {
            continue;
        }

        byte_cursor = bytes_start;
        while (byte_cursor + 1 < bytes_end) {
            int high = hex_nibble((unsigned char)byte_cursor[0]);
            int low = hex_nibble((unsigned char)byte_cursor[1]);
            if (high < 0 || low < 0) {
                break;
            }
            store_nametable_write(build, address, (unsigned)((high << 4) | low));
            ++address;
            byte_cursor += 2;
        }
    }
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

static bool init_palette_stage(TecmoIntroArenaCapture *capture,
                               int frame,
                               uint8_t background[16],
                               uint8_t sprites[16])
{
    (void)frame;

    if (capture == NULL || background == NULL || sprites == NULL ||
        capture->palette_stage_count >= TECMO_INTRO_ARENA_PALETTE_STAGE_COUNT) {
        return false;
    }

    if (capture->palette_stage_count > 0U) {
        memcpy(background,
               capture->palette_stages[capture->palette_stage_count - 1U],
               16U);
        memcpy(sprites,
               capture->sprite_palette_stages[capture->palette_stage_count - 1U],
               16U);
    } else {
        for (size_t i = 0; i < 16U; ++i) {
            background[i] = 0x0FU;
            sprites[i] = 0x0FU;
        }
        if (capture->first_capture_frame == 0U) {
            capture->first_capture_frame = ARENA_SCREEN_CAPTURE_FRAME_FIRST;
        }
    }

    return true;
}

static void append_palette_stage(TecmoIntroArenaCapture *capture,
                                 int frame,
                                 const uint8_t background[16],
                                 const uint8_t sprites[16])
{
    if (capture == NULL || background == NULL || sprites == NULL ||
        capture->palette_stage_count >= TECMO_INTRO_ARENA_PALETTE_STAGE_COUNT) {
        return;
    }

    memcpy(capture->palette_stages[capture->palette_stage_count], background, 16U);
    memcpy(capture->sprite_palette_stages[capture->palette_stage_count], sprites, 16U);
    capture->palette_stage_offsets[capture->palette_stage_count] =
        (unsigned)frame > capture->first_capture_frame ?
            (unsigned)frame - capture->first_capture_frame :
            0U;
    ++capture->palette_stage_count;
    capture->palette_available = true;
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

static void parse_snapshot_ranges(TecmoIntroArenaCapture *capture,
                                  int frame,
                                  const char *start,
                                  const char *end,
                                  bool parse_palette)
{
    const char *cursor = start;
    uint8_t background[16];
    uint8_t sprites[16];
    bool palette_touched = false;

    if (capture == NULL) {
        return;
    }

    if (parse_palette && !init_palette_stage(capture, frame, background, sprites)) {
        parse_palette = false;
    }

    while (cursor < end) {
        unsigned address;
        const char *bytes_start;
        const char *bytes_end;
        const char *byte_cursor;

        if (!parse_hex_stream_entry(&cursor, end, &address, &bytes_start, &bytes_end)) {
            continue;
        }

        byte_cursor = bytes_start;
        while (byte_cursor + 1 < bytes_end) {
            int high = hex_nibble((unsigned char)byte_cursor[0]);
            int low = hex_nibble((unsigned char)byte_cursor[1]);
            unsigned value;

            if (high < 0 || low < 0) {
                break;
            }

            value = (unsigned)((high << 4) | low);
            if ((address >= 0x23C0U && address < 0x2400U) ||
                (address >= 0x27C0U && address < 0x2800U)) {
                store_attribute_write(capture, address, value);
            } else if (parse_palette && address >= 0x3F00U && address < 0x3F20U) {
                if (address < 0x3F10U) {
                    background[address - 0x3F00U] = (uint8_t)value;
                } else {
                    sprites[address - 0x3F10U] = (uint8_t)value;
                }
                palette_touched = true;
            }

            ++address;
            byte_cursor += 2;
        }
    }

    if (palette_touched) {
        append_palette_stage(capture, frame, background, sprites);
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

    if (!init_palette_stage(capture, frame, stage, sprite_stage)) {
        return;
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

    append_palette_stage(capture, frame, stage, sprite_stage);
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

static void parse_visible_oam_stage(TecmoIntroArenaCapture *capture,
                                    int frame,
                                    const char *start,
                                    const char *end)
{
    const char *cursor = start;
    TecmoIntroArenaSpriteStage *stage;
    size_t count = 0;

    if (capture == NULL ||
        capture->sprite_stage_count >= TECMO_INTRO_ARENA_MAX_SPRITE_STAGES) {
        return;
    }

    stage = &capture->sprite_stages[capture->sprite_stage_count];
    if (capture->first_capture_frame == 0U) {
        capture->first_capture_frame = ARENA_SCREEN_CAPTURE_FRAME_FIRST;
    }
    stage->capture_frame = (unsigned)frame;
    while (cursor < end && count < TECMO_INTRO_ARENA_MAX_SPRITES) {
        TecmoIntroArenaSprite sprite;
        if (parse_oam_quad(&cursor, end, &sprite)) {
            stage->sprites[count++] = sprite;
        }
    }
    stage->sprite_count = count;
    ++capture->sprite_stage_count;

    memcpy(capture->sprites, stage->sprites, count * sizeof(stage->sprites[0]));
    capture->sprite_count = count;
    capture->last_capture_frame = (unsigned)frame;
}

static bool oam_span_has_basket_sprite(const char *start, const char *end)
{
    const char *cursor = start;

    while (cursor < end) {
        TecmoIntroArenaSprite sprite;
        if (parse_oam_quad(&cursor, end, &sprite) &&
            sprite.x >= ARENA_SCREEN_BASKET_SPRITE_MIN_X) {
            return true;
        }
    }

    return false;
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

static const char *arena_capture_env_path(void)
{
    const char *value = getenv("TECMO_INTRO_CAPTURE");
    return value != NULL && value[0] != '\0' ? value : NULL;
}

static const char *asset_pack_env_path(void)
{
    const char *value = getenv("TECMO_ASSETPACK");
    return value != NULL && value[0] != '\0' ? value : NULL;
}

static bool arena_env_enabled(const char *name)
{
    char text[16];

    if (name == NULL) {
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
    return strcmp(text, "0") != 0 &&
           strcmp(text, "false") != 0 &&
           strcmp(text, "FALSE") != 0;
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

static bool load_arena_capture_text(TecmoIntroArenaCapture *capture,
                                    const char *source_label,
                                    const char *json,
                                    size_t json_size)
{
    ArenaCaptureBuild build;
    const char *cursor;
    const char *json_end;

    if (capture == NULL || json == NULL) {
        return false;
    }
    if (source_label == NULL) {
        source_label = "";
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
                parse_visible_oam_stage(capture, frame, pair_start, pair_end);
            } else if (json_string_field_equals(cursor, line_end, "kind", "ppu_arena_rows_snapshot")) {
                bool parsed_snapshot = false;

                if (json_string_span(cursor, line_end, "rows", &pair_start, &pair_end)) {
                    parse_nametable_snapshot_rows(&build, pair_start, pair_end);
                    parsed_snapshot = true;
                }
                if (json_string_span(cursor, line_end, "ranges", &pair_start, &pair_end)) {
                    parse_snapshot_ranges(capture,
                                          frame,
                                          pair_start,
                                          pair_end,
                                          capture->palette_stage_count == 0U);
                    parsed_snapshot = true;
                }
                if (json_string_span(cursor, line_end, "oam", &pair_start, &pair_end)) {
                    const char *first = pair_start;
                    while (first + 6 <= pair_end && strncmp(first, "first=", 6) != 0) {
                        ++first;
                    }
                    if (first + 6 <= pair_end &&
                        oam_span_has_basket_sprite(first + 6, pair_end)) {
                        parse_visible_oam_stage(capture, frame, first + 6, pair_end);
                    }
                }
                if (parsed_snapshot) {
                    capture->last_capture_frame = (unsigned)frame;
                }
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
                       "ARENA TRACE %s  P0 %u/CHR%02u P1 %u/CHR%02u  PAL %u OAM %u",
                       source_label,
                       (unsigned)capture->tile_count[0],
                       (unsigned)capture->page_chr_bank[0],
                       (unsigned)capture->tile_count[1],
                       (unsigned)capture->page_chr_bank[1],
                       (unsigned)capture->palette_stage_count,
                       (unsigned)capture->sprite_stage_count);
    }

    return capture->available;
}

static void arena_asset_pack_status(TecmoIntroArenaCapture *capture,
                                    const char *pack_path,
                                    const char *entry_id)
{
    if (capture == NULL) {
        return;
    }
    if (pack_path == NULL) {
        pack_path = "";
    }
    if (entry_id == NULL) {
        entry_id = "";
    }
    (void)snprintf(capture->status,
                   sizeof(capture->status),
                   "ARENA TRACE assetpack entry %s pack %s  P0 %u/CHR%02u P1 %u/CHR%02u  PAL %u OAM %u",
                   entry_id,
                   pack_path,
                   (unsigned)capture->tile_count[0],
                   (unsigned)capture->page_chr_bank[0],
                   (unsigned)capture->tile_count[1],
                   (unsigned)capture->page_chr_bank[1],
                   (unsigned)capture->palette_stage_count,
                   (unsigned)capture->sprite_stage_count);
}

static bool load_arena_capture_file(TecmoIntroArenaCapture *capture, const char *path)
{
    char *json;
    size_t json_size = 0;
    bool loaded;

    json = read_text_file(path, &json_size);
    if (json == NULL) {
        return false;
    }

    loaded = load_arena_capture_text(capture, path, json, json_size);
    free(json);
    return loaded;
}

static bool load_arena_capture_asset_pack_entry(TecmoIntroArenaCapture *capture,
                                                const char *pack_path,
                                                const char *entry_id)
{
    char *json;
    size_t json_size = 0;
    TecmoIntroArenaCapture candidate;
    bool loaded;

    if (capture == NULL || pack_path == NULL || pack_path[0] == '\0' ||
        entry_id == NULL || entry_id[0] == '\0') {
        return false;
    }

    json = read_asset_pack_text_entry(pack_path, entry_id, &json_size);
    if (json == NULL) {
        return false;
    }

    candidate = *capture;
    loaded = load_arena_capture_text(&candidate, entry_id, json, json_size);
    free(json);
    if (loaded) {
        arena_asset_pack_status(&candidate, pack_path, entry_id);
        *capture = candidate;
    }
    return loaded;
}

static bool load_arena_capture_asset_pack(TecmoIntroArenaCapture *capture,
                                          const char *project_root)
{
    char project_pack_path[260];
    const char *env_path;
    const char *pack_paths[4];

    append_path(project_pack_path,
                sizeof(project_pack_path),
                project_root,
                "build\\tecmo.assetpack");
    env_path = asset_pack_env_path();

    pack_paths[0] = env_path;
    pack_paths[1] = project_pack_path[0] != '\0' ? project_pack_path : NULL;
    pack_paths[2] = "build\\tecmo.assetpack";
    pack_paths[3] = "..\\build\\tecmo.assetpack";

    for (size_t i = 0; i < sizeof(pack_paths) / sizeof(pack_paths[0]); ++i) {
        if (pack_paths[i] == NULL || pack_paths[i][0] == '\0') {
            continue;
        }
        for (size_t entry = 0;
             entry < sizeof(ARENA_CAPTURE_ASSET_ENTRIES) / sizeof(ARENA_CAPTURE_ASSET_ENTRIES[0]);
             ++entry) {
            if (load_arena_capture_asset_pack_entry(capture,
                                                    pack_paths[i],
                                                    ARENA_CAPTURE_ASSET_ENTRIES[entry])) {
                return true;
            }
        }
    }

    return false;
}

static uint16_t arena_read_le_u16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U));
}

static int16_t arena_read_le_i16(const uint8_t *bytes)
{
    return (int16_t)arena_read_le_u16(bytes);
}

static uint32_t arena_read_le_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) |
           ((uint32_t)bytes[3] << 24U);
}

static void arena_tile_layer_status(TecmoArenaTileLayer *layer, const char *text)
{
    if (layer == NULL) {
        return;
    }
    (void)snprintf(layer->status, sizeof(layer->status), "%s", text != NULL ? text : "");
}

static bool arena_decode_tile_layer(TecmoArenaTileLayer *layer,
                                    const uint8_t *bytes,
                                    uint64_t byte_count)
{
    const uint32_t expected_cells = TECMO_ARENA_TILE_LAYER_CELL_COUNT;
    const uint64_t expected_size = ARENA_TILE_LAYER_CELLS_OFFSET +
        (uint64_t)expected_cells * ARENA_TILE_LAYER_CELL_STRIDE;
    uint32_t cell_count;
    uint32_t cells_offset;
    uint32_t palette_offset;

    if (layer == NULL || bytes == NULL || byte_count != expected_size ||
        memcmp(bytes, "TATL", 4U) != 0 ||
        arena_read_le_u16(bytes + 4U) != ARENA_TILE_LAYER_VERSION ||
        arena_read_le_u16(bytes + 6U) != ARENA_TILE_LAYER_HEADER_SIZE ||
        arena_read_le_u16(bytes + 8U) != TECMO_ARENA_TILE_LAYER_WIDTH ||
        arena_read_le_u16(bytes + 10U) != TECMO_ARENA_TILE_LAYER_HEIGHT ||
        arena_read_le_u16(bytes + 12U) != 32U ||
        arena_read_le_u16(bytes + 14U) != 30U ||
        arena_read_le_u16(bytes + 16U) != ARENA_TILE_LAYER_CELL_STRIDE ||
        arena_read_le_u16(bytes + 18U) != 0U) {
        return false;
    }

    cell_count = arena_read_le_u32(bytes + 20U);
    cells_offset = arena_read_le_u32(bytes + 24U);
    palette_offset = arena_read_le_u32(bytes + 28U);
    if (cell_count != expected_cells ||
        cells_offset != ARENA_TILE_LAYER_CELLS_OFFSET ||
        palette_offset != ARENA_TILE_LAYER_PALETTE_OFFSET) {
        return false;
    }

    memset(layer, 0, sizeof(*layer));
    layer->width = TECMO_ARENA_TILE_LAYER_WIDTH;
    layer->height = TECMO_ARENA_TILE_LAYER_HEIGHT;
    layer->viewport_width = 32U;
    layer->viewport_height = 30U;
    layer->cell_count = cell_count;
    memcpy(layer->palette, bytes + palette_offset, sizeof(layer->palette));

    for (size_t i = 0; i < layer->cell_count; ++i) {
        const uint8_t *cell_bytes = bytes + cells_offset + i * ARENA_TILE_LAYER_CELL_STRIDE;
        TecmoArenaTileCell *cell = &layer->cells[i];

        cell->tile_id = cell_bytes[0];
        cell->palette_index = cell_bytes[1];
        cell->chr_byte_offset = arena_read_le_u32(cell_bytes + 2U);
        if (cell->palette_index > 3U || (cell->chr_byte_offset & 0x0FU) != 0U) {
            memset(layer, 0, sizeof(*layer));
            return false;
        }
    }

    layer->available = true;
    arena_tile_layer_status(layer, "ROM-ONLY EXACT ARENA TILE LAYER LOADED");
    return true;
}

static bool load_arena_tile_layer_entry(TecmoArenaTileLayer *layer, const char *pack_path)
{
    uint8_t *bytes = NULL;
    uint64_t byte_count = 0U;
    bool loaded;

    if (pack_path == NULL || pack_path[0] == '\0' ||
        tecmo_asset_pack_read_entry(pack_path,
                                    ARENA_TILE_LAYER_ENTRY_ID,
                                    &bytes,
                                    &byte_count) != 0) {
        return false;
    }

    loaded = arena_decode_tile_layer(layer, bytes, byte_count);
    tecmo_asset_pack_free(bytes);
    return loaded;
}

bool tecmo_intro_arena_tile_layer_load(TecmoArenaTileLayer *layer, const char *project_root)
{
    char project_pack_path[260];
    const char *env_path;
    const char *pack_paths[3];

    if (layer == NULL) {
        return false;
    }
    memset(layer, 0, sizeof(*layer));
    arena_tile_layer_status(layer, "ROM-ONLY EXACT ARENA TILE LAYER MISSING");

    append_path(project_pack_path,
                sizeof(project_pack_path),
                project_root,
                "build\\tecmo.assetpack");
    env_path = asset_pack_env_path();
    if (env_path != NULL) {
        return load_arena_tile_layer_entry(layer, env_path);
    }
    pack_paths[0] = project_pack_path[0] != '\0' ? project_pack_path : NULL;
    pack_paths[1] = "tecmo.assetpack";
    pack_paths[2] = "build\\tecmo.assetpack";
    for (size_t i = 0; i < sizeof(pack_paths) / sizeof(pack_paths[0]); ++i) {
        if (arena_file_exists(pack_paths[i])) {
            return load_arena_tile_layer_entry(layer, pack_paths[i]);
        }
    }
    return false;
}

static void arena_sprite_groups_status(TecmoArenaNativeSpriteGroups *sprite_groups,
                                       const char *text)
{
    if (sprite_groups == NULL) {
        return;
    }
    (void)snprintf(sprite_groups->status,
                   sizeof(sprite_groups->status),
                   "%s",
                   text != NULL ? text : "");
}

static bool arena_sprite_group_contract_valid(const TecmoArenaNativeSpriteGroup *group)
{
    if (group == NULL || group->camera_x_multiplier != 0 ||
        group->camera_y_multiplier != 2) {
        return false;
    }
    if (group->kind == TECMO_ARENA_NATIVE_SPRITE_GROUP_JUMBOTRON) {
        return group->draw_order == 1U &&
               group->piece_count == ARENA_SPRITE_GROUPS_JUMBOTRON_PIECES &&
               group->anchor_x == 0 && group->anchor_y == 0;
    }
    if (group->kind == TECMO_ARENA_NATIVE_SPRITE_GROUP_GOAL) {
        return group->draw_order == 0U &&
               group->piece_count == ARENA_SPRITE_GROUPS_GOAL_PIECES &&
               group->anchor_x == 165 && group->anchor_y == 350;
    }
    return false;
}

static bool arena_decode_sprite_groups(TecmoArenaNativeSpriteGroups *sprite_groups,
                                       const uint8_t *bytes,
                                       uint64_t byte_count)
{
    const uint64_t expected_size = ARENA_SPRITE_GROUPS_PIECES_OFFSET +
        (uint64_t)TECMO_INTRO_ARENA_NATIVE_SPRITE_PIECE_COUNT *
            ARENA_SPRITE_GROUPS_PIECE_STRIDE;
    TecmoArenaNativeSpriteGroups decoded;
    bool covered[TECMO_INTRO_ARENA_NATIVE_SPRITE_PIECE_COUNT] = {false};
    bool saw_jumbotron = false;
    bool saw_goal = false;

    if (sprite_groups == NULL || bytes == NULL || byte_count != expected_size ||
        memcmp(bytes, "TASG", 4U) != 0 ||
        arena_read_le_u16(bytes + 4U) != ARENA_SPRITE_GROUPS_VERSION ||
        arena_read_le_u16(bytes + 6U) != ARENA_SPRITE_GROUPS_HEADER_SIZE ||
        arena_read_le_u16(bytes + 8U) != TECMO_INTRO_ARENA_NATIVE_SPRITE_GROUP_COUNT ||
        arena_read_le_u16(bytes + 10U) != ARENA_SPRITE_GROUPS_GROUP_STRIDE ||
        arena_read_le_u32(bytes + 12U) != TECMO_INTRO_ARENA_NATIVE_SPRITE_PIECE_COUNT ||
        arena_read_le_u16(bytes + 16U) != ARENA_SPRITE_GROUPS_PIECE_STRIDE ||
        arena_read_le_u16(bytes + 18U) != ARENA_SPRITE_GROUPS_FLAGS ||
        arena_read_le_u32(bytes + 20U) != ARENA_SPRITE_GROUPS_PALETTE_OFFSET ||
        arena_read_le_u32(bytes + 24U) != ARENA_SPRITE_GROUPS_GROUPS_OFFSET ||
        arena_read_le_u32(bytes + 28U) != ARENA_SPRITE_GROUPS_PIECES_OFFSET) {
        return false;
    }
    for (size_t i = 32U; i < ARENA_SPRITE_GROUPS_HEADER_SIZE; ++i) {
        if (bytes[i] != 0U) {
            return false;
        }
    }

    memset(&decoded, 0, sizeof(decoded));
    decoded.group_count = TECMO_INTRO_ARENA_NATIVE_SPRITE_GROUP_COUNT;
    decoded.piece_count = TECMO_INTRO_ARENA_NATIVE_SPRITE_PIECE_COUNT;
    memcpy(decoded.palette,
           bytes + ARENA_SPRITE_GROUPS_PALETTE_OFFSET,
           sizeof(decoded.palette));
    for (size_t i = 0; i < sizeof(decoded.palette); ++i) {
        if (decoded.palette[i] > 0x3FU) {
            return false;
        }
    }

    for (size_t i = 0; i < decoded.group_count; ++i) {
        const uint8_t *group_bytes = bytes + ARENA_SPRITE_GROUPS_GROUPS_OFFSET +
            i * ARENA_SPRITE_GROUPS_GROUP_STRIDE;
        TecmoArenaNativeSpriteGroup *group = &decoded.groups[i];
        uint32_t first_piece = arena_read_le_u32(group_bytes + 4U);
        uint32_t piece_count = arena_read_le_u32(group_bytes + 8U);

        group->kind = (TecmoArenaNativeSpriteGroupKind)arena_read_le_u16(group_bytes);
        group->draw_order = arena_read_le_u16(group_bytes + 2U);
        group->first_piece = first_piece;
        group->piece_count = piece_count;
        group->anchor_x = arena_read_le_i16(group_bytes + 12U);
        group->anchor_y = arena_read_le_i16(group_bytes + 14U);
        group->camera_x_multiplier = arena_read_le_i16(group_bytes + 16U);
        group->camera_y_multiplier = arena_read_le_i16(group_bytes + 18U);

        if (!arena_sprite_group_contract_valid(group) ||
            first_piece > decoded.piece_count ||
            piece_count > decoded.piece_count - first_piece) {
            return false;
        }
        if (group->kind == TECMO_ARENA_NATIVE_SPRITE_GROUP_JUMBOTRON) {
            if (saw_jumbotron) {
                return false;
            }
            saw_jumbotron = true;
        } else if (group->kind == TECMO_ARENA_NATIVE_SPRITE_GROUP_GOAL) {
            if (saw_goal) {
                return false;
            }
            saw_goal = true;
        }
        for (size_t piece = group->first_piece;
             piece < group->first_piece + group->piece_count;
             ++piece) {
            if (covered[piece]) {
                return false;
            }
            covered[piece] = true;
        }
    }
    if (!saw_jumbotron || !saw_goal) {
        return false;
    }
    for (size_t i = 0; i < decoded.piece_count; ++i) {
        if (!covered[i]) {
            return false;
        }
    }

    for (size_t i = 0; i < decoded.piece_count; ++i) {
        const uint8_t *piece_bytes = bytes + ARENA_SPRITE_GROUPS_PIECES_OFFSET +
            i * ARENA_SPRITE_GROUPS_PIECE_STRIDE;
        TecmoArenaNativeSpritePiece *piece = &decoded.pieces[i];

        piece->dx = arena_read_le_i16(piece_bytes);
        piece->dy = arena_read_le_i16(piece_bytes + 2U);
        piece->top_chr_offset = arena_read_le_u32(piece_bytes + 4U);
        piece->palette_index = piece_bytes[8U];
        piece->flags = piece_bytes[9U];
        if ((piece->top_chr_offset & 0x0FU) != 0U ||
            piece->palette_index > 3U ||
            (piece->flags & ~(uint8_t)(TECMO_ARENA_NATIVE_SPRITE_FLIP_HORIZONTAL |
                                      TECMO_ARENA_NATIVE_SPRITE_FLIP_VERTICAL |
                                      TECMO_ARENA_NATIVE_SPRITE_PRIORITY)) != 0U ||
            arena_read_le_u16(piece_bytes + 10U) != 0U) {
            return false;
        }
    }

    decoded.available = true;
    arena_sprite_groups_status(&decoded, "ROM-ONLY EXACT ARENA SPRITE GROUPS LOADED");
    *sprite_groups = decoded;
    return true;
}

static bool load_arena_sprite_groups_entry(TecmoArenaNativeSpriteGroups *sprite_groups,
                                           const char *pack_path)
{
    uint8_t *bytes = NULL;
    uint64_t byte_count = 0U;
    bool loaded;

    if (sprite_groups == NULL || pack_path == NULL || pack_path[0] == '\0' ||
        tecmo_asset_pack_read_entry(pack_path,
                                    ARENA_SPRITE_GROUPS_ENTRY_ID,
                                    &bytes,
                                    &byte_count) != 0) {
        return false;
    }
    loaded = arena_decode_sprite_groups(sprite_groups, bytes, byte_count);
    tecmo_asset_pack_free(bytes);
    if (!loaded) {
        arena_sprite_groups_status(sprite_groups,
                                   "ROM-ONLY EXACT ARENA SPRITE GROUPS INVALID (TASG-1)");
    }
    return loaded;
}

bool tecmo_intro_arena_sprite_groups_load(TecmoArenaNativeSpriteGroups *sprite_groups,
                                          const char *project_root)
{
    char project_pack_path[260];
    const char *env_path;
    const char *pack_paths[3];

    if (sprite_groups == NULL) {
        return false;
    }
    memset(sprite_groups, 0, sizeof(*sprite_groups));
    arena_sprite_groups_status(sprite_groups,
                               "ROM-ONLY EXACT ARENA SPRITE GROUPS MISSING: "
                               ARENA_SPRITE_GROUPS_ENTRY_ID);

    append_path(project_pack_path,
                sizeof(project_pack_path),
                project_root,
                "build\\tecmo.assetpack");
    env_path = asset_pack_env_path();
    if (env_path != NULL) {
        return load_arena_sprite_groups_entry(sprite_groups, env_path);
    }
    pack_paths[0] = project_pack_path[0] != '\0' ? project_pack_path : NULL;
    pack_paths[1] = "tecmo.assetpack";
    pack_paths[2] = "build\\tecmo.assetpack";
    for (size_t i = 0; i < sizeof(pack_paths) / sizeof(pack_paths[0]); ++i) {
        if (arena_file_exists(pack_paths[i])) {
            return load_arena_sprite_groups_entry(sprite_groups, pack_paths[i]);
        }
    }
    return false;
}

static bool arena_native_asset_entry_present(const char *pack_path)
{
    TecmoArenaTileLayer layer;
    return load_arena_tile_layer_entry(&layer, pack_path);
}

static bool load_arena_native_asset_pack_status(TecmoIntroArenaCapture *capture,
                                                const char *project_root)
{
    char project_pack_path[260];
    const char *env_path;
    const char *pack_paths[4];

    append_path(project_pack_path,
                sizeof(project_pack_path),
                project_root,
                "build\\tecmo.assetpack");
    env_path = asset_pack_env_path();

    pack_paths[0] = env_path != NULL
                        ? env_path
                        : (project_pack_path[0] != '\0' ? project_pack_path : NULL);
    pack_paths[1] = env_path == NULL ? "tecmo.assetpack" : NULL;
    pack_paths[2] = env_path == NULL ? "build\\tecmo.assetpack" : NULL;
    pack_paths[3] = env_path == NULL ? "..\\build\\tecmo.assetpack" : NULL;

    for (size_t i = 0; i < sizeof(pack_paths) / sizeof(pack_paths[0]); ++i) {
        if (pack_paths[i] == NULL || pack_paths[i][0] == '\0') {
            continue;
        }
        if (arena_file_exists(pack_paths[i])) {
            if (arena_native_asset_entry_present(pack_paths[i])) {
                arena_status(capture,
                             "ROM-ONLY EXACT ARENA TILE LAYER PRESENT; CAPTURE DISABLED");
                return true;
            }
            return false;
        }
    }

    return false;
}

bool tecmo_intro_arena_capture_load(TecmoIntroArenaCapture *capture, const char *project_root)
{
    char project_compact_path[260];
    char project_memory_path[260];
    char project_arena_path[260];
    const char *env_path;
    const char *paths[12];

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
    arena_status(capture, "IMPORT INTRO ARENA CAPTURE OR RUN FCEUX LUA WATCH");

    if (load_arena_native_asset_pack_status(capture, project_root) &&
        !arena_env_enabled("TECMO_ALLOW_LOOSE_INTRO_CAPTURE")) {
        return false;
    }

    env_path = arena_capture_env_path();
    if (env_path != NULL && env_path[0] != '\0') {
        TecmoIntroArenaCapture candidate = *capture;
        if (load_arena_capture_file(&candidate, env_path)) {
            *capture = candidate;
            return true;
        }
    }

    if (load_arena_capture_asset_pack(capture, project_root)) {
        return true;
    }

    append_path(project_compact_path,
                sizeof(project_compact_path),
                project_root,
                "build\\intro_arena_capture.ndjson");
    append_path(project_memory_path,
                sizeof(project_memory_path),
                project_root,
                "build\\emu_intro_memory_watch.ndjson");
    append_path(project_arena_path,
                sizeof(project_arena_path),
                project_root,
                "build\\emu_intro_arena_irq_watch.ndjson");

    paths[0] = NULL;
    paths[1] = project_compact_path[0] != '\0' ? project_compact_path : NULL;
    paths[2] = "build\\intro_arena_capture.ndjson";
    paths[3] = "intro_arena_capture.ndjson";
    paths[4] = project_memory_path[0] != '\0' ? project_memory_path : NULL;
    paths[5] = "build\\emu_intro_memory_watch.ndjson";
    paths[6] = "emu_intro_memory_watch.ndjson";
    paths[7] = project_arena_path[0] != '\0' ? project_arena_path : NULL;
    paths[8] = "build\\emu_intro_arena_irq_watch.ndjson";
    paths[9] = "emu_intro_arena_irq_watch.ndjson";
    paths[10] = "..\\build\\intro_arena_capture.ndjson";
    paths[11] = "..\\build\\emu_intro_arena_irq_watch.ndjson";

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

static const TecmoIntroArenaSpriteStage *arena_sprite_stage_for_frame(const TecmoIntroArenaCapture *capture,
                                                                      unsigned frame,
                                                                      int frame_delta)
{
    const TecmoIntroArenaSpriteStage *selected = NULL;
    unsigned base_capture_frame;
    unsigned target_capture_frame;

    if (capture == NULL || capture->sprite_stage_count == 0U) {
        return NULL;
    }

    base_capture_frame = capture->first_capture_frame != 0U ?
        capture->first_capture_frame + frame :
        frame;
    if (frame_delta < 0) {
        unsigned lag = (unsigned)(-frame_delta);
        target_capture_frame = base_capture_frame > lag ? base_capture_frame - lag : 0U;
    } else {
        target_capture_frame = base_capture_frame + (unsigned)frame_delta;
    }
    for (size_t i = 0; i < capture->sprite_stage_count; ++i) {
        if (capture->sprite_stages[i].capture_frame <= target_capture_frame &&
            (selected == NULL ||
             capture->sprite_stages[i].capture_frame >= selected->capture_frame)) {
            selected = &capture->sprite_stages[i];
        }
    }
    return selected;
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

bool tecmo_intro_arena_tile_layer_chr_available(const TecmoArenaTileLayer *layer,
                                                const uint8_t *chr_bytes,
                                                uint64_t chr_byte_count)
{
    if (layer == NULL || !layer->available || chr_bytes == NULL ||
        layer->cell_count != TECMO_ARENA_TILE_LAYER_CELL_COUNT) {
        return false;
    }

    for (size_t i = 0; i < layer->cell_count; ++i) {
        if (!arena_chr_tile_offset_available(chr_byte_count,
                                             layer->cells[i].chr_byte_offset)) {
            return false;
        }
    }
    return true;
}

bool tecmo_intro_arena_draw_native_chr(TecmoFramebuffer *fb,
                                       const TecmoArenaTileLayer *layer,
                                       const uint8_t *chr_bytes,
                                       uint64_t chr_byte_count,
                                       unsigned frame,
                                       int origin_x,
                                       int origin_y,
                                       int scale)
{
    (void)frame;
    if (fb == NULL || scale <= 0 ||
        !tecmo_intro_arena_tile_layer_chr_available(layer, chr_bytes, chr_byte_count)) {
        return false;
    }

    for (size_t i = 0; i < layer->cell_count; ++i) {
        const TecmoArenaTileCell *cell = &layer->cells[i];
        int row = (int)(i / layer->width);
        int col = (int)(i % layer->width);
        uint32_t palette[4];

        arena_build_palette(palette, layer->palette, cell->palette_index);
        tecmo_draw_chr_tile_at_offset_ex(fb,
                                         chr_bytes,
                                         chr_byte_count,
                                         cell->chr_byte_offset,
                                         origin_x + col * 8 * scale,
                                         origin_y + row * 8 * scale,
                                         scale,
                                         palette,
                                         false,
                                         false);
    }
    return true;
}

size_t tecmo_intro_arena_native_sprite_group_count(
    const TecmoArenaNativeSpriteGroups *sprite_groups)
{
    return sprite_groups != NULL && sprite_groups->available ? sprite_groups->group_count : 0U;
}

const TecmoArenaNativeSpriteGroup *tecmo_intro_arena_native_sprite_group(
    const TecmoArenaNativeSpriteGroups *sprite_groups,
    TecmoArenaNativeSpriteGroupKind kind)
{
    if (sprite_groups == NULL || !sprite_groups->available) {
        return NULL;
    }
    for (size_t i = 0; i < sprite_groups->group_count; ++i) {
        if (sprite_groups->groups[i].kind == kind) {
            return &sprite_groups->groups[i];
        }
    }
    return NULL;
}

size_t tecmo_intro_arena_native_sprite_piece_count(
    const TecmoArenaNativeSpriteGroups *sprite_groups,
    TecmoArenaNativeSpriteGroupKind kind)
{
    const TecmoArenaNativeSpriteGroup *group =
        tecmo_intro_arena_native_sprite_group(sprite_groups, kind);
    return group != NULL ? group->piece_count : 0U;
}

bool tecmo_intro_arena_native_sprite_chr_available(
    const TecmoArenaNativeSpriteGroups *sprite_groups,
    const uint8_t *chr_bytes,
    uint64_t chr_byte_count)
{
    if (sprite_groups == NULL || !sprite_groups->available || chr_bytes == NULL ||
        sprite_groups->piece_count != TECMO_INTRO_ARENA_NATIVE_SPRITE_PIECE_COUNT) {
        return false;
    }
    for (size_t i = 0; i < sprite_groups->piece_count; ++i) {
        uint64_t top_offset = sprite_groups->pieces[i].top_chr_offset;
        if ((top_offset & 0x0FULL) != 0U || top_offset + 31ULL >= chr_byte_count) {
            return false;
        }
    }
    return true;
}

static bool arena_native_sprite_piece_position(
    const TecmoArenaNativeSpriteGroup *group,
    const TecmoArenaNativeSpritePiece *piece,
    const TecmoIntroArenaTransitionState *state,
    int *x,
    int *pixel_top)
{
    int piece_x;
    int oam_y;
    int piece_pixel_top;

    if (group == NULL || piece == NULL || state == NULL) {
        return false;
    }
    piece_x = group->anchor_x + piece->dx;
    oam_y = group->anchor_y + piece->dy -
        group->camera_y_multiplier * (int)state->scroll_y_0301;
    piece_pixel_top = oam_y + ARENA_SPRITE_PIXEL_TOP_OFFSET;
    if (x != NULL) {
        *x = piece_x;
    }
    if (pixel_top != NULL) {
        *pixel_top = piece_pixel_top;
    }
    return piece_x + ARENA_SPRITE_WIDTH > 0 &&
           piece_x < ARENA_SPRITE_VIEWPORT_WIDTH &&
           piece_pixel_top + ARENA_SPRITE_HEIGHT > 0 &&
           piece_pixel_top < ARENA_SPRITE_VIEWPORT_HEIGHT;
}

static void arena_native_sprite_count_visible(
    TecmoArenaNativeSpriteVisibleCounts *counts,
    TecmoArenaNativeSpriteGroupKind kind)
{
    if (counts == NULL) {
        return;
    }
    if (kind == TECMO_ARENA_NATIVE_SPRITE_GROUP_JUMBOTRON) {
        ++counts->jumbotron;
    } else if (kind == TECMO_ARENA_NATIVE_SPRITE_GROUP_GOAL) {
        ++counts->goal;
    }
}

TecmoArenaNativeSpriteVisibleCounts tecmo_intro_arena_native_sprite_visible_counts(
    const TecmoArenaNativeSpriteGroups *sprite_groups,
    const TecmoIntroArenaTransitionState *state)
{
    TecmoArenaNativeSpriteVisibleCounts counts = {0U, 0U};

    if (sprite_groups == NULL || !sprite_groups->available || state == NULL) {
        return counts;
    }
    for (size_t group_index = 0; group_index < sprite_groups->group_count; ++group_index) {
        const TecmoArenaNativeSpriteGroup *group = &sprite_groups->groups[group_index];
        for (size_t piece_index = group->first_piece;
             piece_index < group->first_piece + group->piece_count;
             ++piece_index) {
            if (arena_native_sprite_piece_position(group,
                                                   &sprite_groups->pieces[piece_index],
                                                   state,
                                                   NULL,
                                                   NULL)) {
                arena_native_sprite_count_visible(&counts, group->kind);
            }
        }
    }
    return counts;
}

static const TecmoArenaNativeSpriteGroup *arena_native_sprite_group_for_draw_order(
    const TecmoArenaNativeSpriteGroups *sprite_groups,
    unsigned draw_order)
{
    if (sprite_groups == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < sprite_groups->group_count; ++i) {
        if (sprite_groups->groups[i].draw_order == draw_order) {
            return &sprite_groups->groups[i];
        }
    }
    return NULL;
}

static void arena_draw_native_sprite_piece(TecmoFramebuffer *fb,
                                           const TecmoArenaNativeSpriteGroups *sprite_groups,
                                           const TecmoArenaNativeSpritePiece *piece,
                                           const uint8_t *chr_bytes,
                                           uint64_t chr_byte_count,
                                           int x,
                                           int y,
                                           int scale)
{
    uint32_t palette[4];
    uint64_t first_offset = piece->top_chr_offset;
    uint64_t second_offset = first_offset + 16ULL;
    bool flip_horizontal =
        (piece->flags & TECMO_ARENA_NATIVE_SPRITE_FLIP_HORIZONTAL) != 0U;
    bool flip_vertical =
        (piece->flags & TECMO_ARENA_NATIVE_SPRITE_FLIP_VERTICAL) != 0U;

    if (flip_vertical) {
        first_offset += 16ULL;
        second_offset -= 16ULL;
    }
    arena_build_palette(palette, sprite_groups->palette, piece->palette_index);
    tecmo_draw_chr_tile_at_offset_ex(fb,
                                     chr_bytes,
                                     chr_byte_count,
                                     first_offset,
                                     x,
                                     y,
                                     scale,
                                     palette,
                                     flip_horizontal,
                                     flip_vertical);
    tecmo_draw_chr_tile_at_offset_ex(fb,
                                     chr_bytes,
                                     chr_byte_count,
                                     second_offset,
                                     x,
                                     y + 8 * scale,
                                     scale,
                                     palette,
                                     flip_horizontal,
                                     flip_vertical);
}

TecmoArenaNativeSpriteVisibleCounts tecmo_intro_arena_draw_native_sprite_groups(
    TecmoFramebuffer *fb,
    const TecmoArenaNativeSpriteGroups *sprite_groups,
    const uint8_t *chr_bytes,
    uint64_t chr_byte_count,
    const TecmoIntroArenaTransitionState *state,
    int origin_x,
    int origin_y,
    int scale)
{
    TecmoArenaNativeSpriteVisibleCounts counts = {0U, 0U};

    if (fb == NULL || state == NULL || scale <= 0 ||
        !tecmo_intro_arena_native_sprite_chr_available(sprite_groups,
                                                       chr_bytes,
                                                       chr_byte_count)) {
        return counts;
    }
    for (unsigned draw_order = 0U;
         draw_order < TECMO_INTRO_ARENA_NATIVE_SPRITE_GROUP_COUNT;
         ++draw_order) {
        const TecmoArenaNativeSpriteGroup *group =
            arena_native_sprite_group_for_draw_order(sprite_groups, draw_order);
        if (group == NULL) {
            return (TecmoArenaNativeSpriteVisibleCounts){0U, 0U};
        }
        for (size_t piece_index = group->first_piece;
             piece_index < group->first_piece + group->piece_count;
             ++piece_index) {
            const TecmoArenaNativeSpritePiece *piece = &sprite_groups->pieces[piece_index];
            int x;
            int pixel_top;

            if (!arena_native_sprite_piece_position(group, piece, state, &x, &pixel_top)) {
                continue;
            }
            arena_draw_native_sprite_piece(fb,
                                           sprite_groups,
                                           piece,
                                           chr_bytes,
                                           chr_byte_count,
                                           origin_x + x * scale,
                                           origin_y + pixel_top * scale,
                                           scale);
            arena_native_sprite_count_visible(&counts, group->kind);
        }
    }
    return counts;
}

static void arena_draw_sprite_entry(TecmoFramebuffer *fb,
                                    const TecmoIntroArenaCapture *capture,
                                    const uint8_t *chr_bytes,
                                    uint64_t chr_byte_count,
                                    const uint8_t *sprite_palette,
                                    const TecmoIntroArenaSprite *sprite,
                                    int origin_x,
                                    int origin_y,
                                    int scale)
{
    uint8_t palette_index;
    bool flip_horizontal;
    bool flip_vertical;
    uint16_t top_tile;
    uint16_t bottom_tile;
    uint16_t first_tile;
    uint16_t second_tile;
    uint8_t palette_base;
    uint32_t rgba_palette[4];
    int x;
    int y;

    if (sprite == NULL) {
        return;
    }

    palette_index = (uint8_t)(sprite->attributes & 0x03U);
    flip_horizontal = (sprite->attributes & 0x40U) != 0U;
    flip_vertical = (sprite->attributes & 0x80U) != 0U;
    top_tile = (uint16_t)(sprite->tile & 0xFEU);
    bottom_tile = (uint16_t)(top_tile + 1U);
    first_tile = flip_vertical ? bottom_tile : top_tile;
    second_tile = flip_vertical ? top_tile : bottom_tile;
    palette_base = (uint8_t)(palette_index * 4U);
    x = origin_x + (int)sprite->x * scale;
    y = origin_y + ((int)sprite->y + ARENA_SCREEN_SPRITE_Y_HARDWARE_OFFSET) * scale;

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

static size_t arena_draw_sprite_stage_filtered(TecmoFramebuffer *fb,
                                               const TecmoIntroArenaCapture *capture,
                                               const uint8_t *chr_bytes,
                                               uint64_t chr_byte_count,
                                               const uint8_t *sprite_palette,
                                               const TecmoIntroArenaSpriteStage *stage,
                                               bool draw_basket_sprites,
                                               int origin_x,
                                               int origin_y,
                                               int scale)
{
    size_t drawn = 0;

    if (stage == NULL) {
        return 0U;
    }

    for (size_t i = 0; i < stage->sprite_count; ++i) {
        const TecmoIntroArenaSprite *sprite = &stage->sprites[i];
        bool is_basket_sprite = sprite->x >= ARENA_SCREEN_BASKET_SPRITE_MIN_X;

        if (is_basket_sprite != draw_basket_sprites) {
            continue;
        }
        arena_draw_sprite_entry(fb,
                                capture,
                                chr_bytes,
                                chr_byte_count,
                                sprite_palette,
                                sprite,
                                origin_x,
                                origin_y,
                                scale);
        ++drawn;
    }

    return drawn;
}

size_t tecmo_intro_arena_draw_sprites(TecmoFramebuffer *fb,
                                      const TecmoIntroArenaCapture *capture,
                                      const uint8_t *chr_bytes,
                                      uint64_t chr_byte_count,
                                      unsigned frame,
                                      int origin_x,
                                      int origin_y,
                                      int scale)
{
    const uint8_t *sprite_palette;
    const TecmoIntroArenaSpriteStage *stage;
    const TecmoIntroArenaSpriteStage *basket_stage;
    TecmoIntroArenaTransitionState transition_state;
    TecmoIntroArenaSpriteStage fallback_stage;
    int basket_origin_y;
    size_t drawn = 0;

    if (fb == NULL || capture == NULL || !capture->available ||
        chr_bytes == NULL || chr_byte_count == 0U || scale <= 0) {
        return 0U;
    }

    stage = arena_sprite_stage_for_frame(capture, frame, 0U);
    basket_stage = arena_sprite_stage_for_frame(capture,
                                                frame,
                                                -ARENA_SCREEN_BASKET_STAGE_FRAME_LAG);
    if (stage == NULL && capture->sprite_stage_count == 0U) {
        memset(&fallback_stage, 0, sizeof(fallback_stage));
        memcpy(fallback_stage.sprites,
               capture->sprites,
               capture->sprite_count * sizeof(capture->sprites[0]));
        fallback_stage.sprite_count = capture->sprite_count;
        stage = &fallback_stage;
        basket_stage = &fallback_stage;
    } else if (stage == NULL) {
        return 0U;
    }
    if (basket_stage == NULL) {
        basket_stage = stage;
    }
    tecmo_intro_arena_transition_state(frame, &transition_state);
    basket_origin_y = origin_y + (int)transition_state.scroll_y_0301 * scale;
    sprite_palette = arena_sprite_palette_for_frame(capture, frame);
    if (stage != NULL) {
        drawn += arena_draw_sprite_stage_filtered(fb,
                                                 capture,
                                                 chr_bytes,
                                                 chr_byte_count,
                                                 sprite_palette,
                                                 stage,
                                                 false,
                                                 origin_x,
                                                 origin_y,
                                                 scale);
    }
    drawn += arena_draw_sprite_stage_filtered(fb,
                                             capture,
                                             chr_bytes,
                                             chr_byte_count,
                                             sprite_palette,
                                             basket_stage,
                                             true,
                                             origin_x + ARENA_SCREEN_BASKET_SCREEN_X_OFFSET,
                                             basket_origin_y + ARENA_SCREEN_BASKET_SCREEN_Y_OFFSET,
                                             scale);

    return drawn;
}
