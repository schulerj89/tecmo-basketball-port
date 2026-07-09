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
#define ARENA_SCREEN_NATIVE_COLUMNS 32
#define ARENA_SCREEN_NATIVE_ROWS \
    (ARENA_SCREEN_TOP_ROWS + ARENA_SCREEN_SMALL_CROWD_ROWS + ARENA_SCREEN_LARGE_CROWD_ROWS)
#define ARENA_SCREEN_NATIVE_TILE_SHEET_COLUMNS 16
#define ARENA_SCREEN_NATIVE_SLOT_TILE_ROWS 4

typedef struct ArenaCaptureBuild {
    uint8_t tiles[TECMO_INTRO_ARENA_PAGE_COUNT][TECMO_INTRO_ARENA_TILES_PER_PAGE];
    bool tile_present[TECMO_INTRO_ARENA_PAGE_COUNT][TECMO_INTRO_ARENA_TILES_PER_PAGE];
} ArenaCaptureBuild;

typedef struct ArenaNativeBand {
    unsigned source_page;
    int source_first_row;
    int row_count;
    int destination_first_row;
} ArenaNativeBand;

typedef struct ArenaNativeGoalTilePair {
    TecmoArenaGoalPart part;
    int offset_x;
    int offset_y;
    uint16_t top_tile;
} ArenaNativeGoalTilePair;

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

static const char *ARENA_NATIVE_ASSET_ENTRIES[] = {
    "arena/intro/background-layer",
    "arena/intro/palette-cycle",
};

static const ArenaNativeBand ARENA_NATIVE_BANDS[] = {
    {0U, 0, ARENA_SCREEN_TOP_ROWS, 0},
    {ARENA_SCREEN_SMALL_CROWD_PAGE,
     ARENA_SCREEN_SMALL_CROWD_FIRST_ROW,
     ARENA_SCREEN_SMALL_CROWD_ROWS,
     ARENA_SCREEN_TOP_ROWS},
    {ARENA_SCREEN_LARGE_CROWD_PAGE,
     ARENA_SCREEN_LARGE_CROWD_FIRST_ROW,
     ARENA_SCREEN_LARGE_CROWD_ROWS,
     ARENA_SCREEN_TOP_ROWS + ARENA_SCREEN_SMALL_CROWD_ROWS},
};

static const ArenaNativeGoalTilePair ARENA_NATIVE_GOAL_TILE_PAIRS[] = {
    {TECMO_ARENA_GOAL_PART_BACKBOARD, -32, -40, 0x98U},
    {TECMO_ARENA_GOAL_PART_BACKBOARD, -24, -40, 0x9AU},
    {TECMO_ARENA_GOAL_PART_BACKBOARD, -16, -40, 0x9CU},
    {TECMO_ARENA_GOAL_PART_BACKBOARD, -8, -40, 0x9EU},
    {TECMO_ARENA_GOAL_PART_RIM, -13, -8, 0xA0U},
    {TECMO_ARENA_GOAL_PART_RIM, -5, -8, 0xA2U},
    {TECMO_ARENA_GOAL_PART_NET, -10, 0, 0xA4U},
    {TECMO_ARENA_GOAL_PART_NET, -2, 0, 0xA6U},
    {TECMO_ARENA_GOAL_PART_SUPPORT, 22, -28, 0x96U},
    {TECMO_ARENA_GOAL_PART_SUPPORT, 30, -28, 0x9EU},
    {TECMO_ARENA_GOAL_PART_POST, 48, -24, 0x96U},
    {TECMO_ARENA_GOAL_PART_POST, 48, -8, 0x96U},
    {TECMO_ARENA_GOAL_PART_POST, 48, 8, 0x96U},
    {TECMO_ARENA_GOAL_PART_POST, 48, 24, 0x96U},
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

static uint64_t arena_native_bg_tile_offset_for_source(unsigned source_page,
                                                       int source_row,
                                                       uint8_t tile)
{
    if (source_page == 0U && source_row < ARENA_SCREEN_BG_SPLIT_ROW) {
        return arena_mmc3_bg_tile_offset_for_registers(ARENA_SCREEN_BG_UPPER_R0,
                                                       ARENA_SCREEN_BG_UPPER_R1,
                                                       tile);
    }
    return arena_mmc3_bg_tile_offset_for_registers(ARENA_SCREEN_BG_LOWER_R0,
                                                   ARENA_SCREEN_BG_LOWER_R1,
                                                   tile);
}

static bool arena_chr_tile_offset_available(uint64_t chr_byte_count, uint64_t tile_offset)
{
    return tile_offset + 15ULL < chr_byte_count;
}

static bool arena_chr_bank_tile_available(uint64_t chr_byte_count,
                                          uint32_t chr_bank,
                                          uint16_t tile)
{
    uint64_t tile_offset = (uint64_t)chr_bank * 8192ULL + (uint64_t)tile * 16ULL;

    return tile <= 0x1FFU && arena_chr_tile_offset_available(chr_byte_count, tile_offset);
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

static bool arena_native_asset_entry_present(const char *pack_path, const char *entry_id)
{
    char *json;
    size_t json_size = 0U;
    bool present = false;

    if (pack_path == NULL || pack_path[0] == '\0' || entry_id == NULL || entry_id[0] == '\0') {
        return false;
    }

    json = read_asset_pack_text_entry(pack_path, entry_id, &json_size);
    if (json == NULL) {
        return false;
    }
    present = json_size > 0U &&
              strstr(json, "\"input_contract\":\"ines-only\"") != NULL;
    free(json);
    return present;
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

    pack_paths[0] = env_path;
    pack_paths[1] = project_pack_path[0] != '\0' ? project_pack_path : NULL;
    pack_paths[2] = "build\\tecmo.assetpack";
    pack_paths[3] = "..\\build\\tecmo.assetpack";

    for (size_t i = 0; i < sizeof(pack_paths) / sizeof(pack_paths[0]); ++i) {
        bool present = true;

        if (pack_paths[i] == NULL || pack_paths[i][0] == '\0') {
            continue;
        }
        for (size_t entry = 0;
             entry < sizeof(ARENA_NATIVE_ASSET_ENTRIES) / sizeof(ARENA_NATIVE_ASSET_ENTRIES[0]);
             ++entry) {
            if (!arena_native_asset_entry_present(pack_paths[i], ARENA_NATIVE_ASSET_ENTRIES[entry])) {
                present = false;
                break;
            }
        }
        if (present) {
            arena_status(capture,
                         "ROM-ONLY ARENA NATIVE ENTRIES PRESENT; TILE/PALETTE STATE EXTRACTOR PENDING");
            return true;
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

static uint8_t arena_native_tile_from_sheet_row(int sheet_row, int col)
{
    uint8_t slot_group = (uint8_t)((sheet_row / ARENA_SCREEN_NATIVE_SLOT_TILE_ROWS) & 0x03);
    uint8_t local_row = (uint8_t)(sheet_row % ARENA_SCREEN_NATIVE_SLOT_TILE_ROWS);
    uint8_t local_col = (uint8_t)(col % ARENA_SCREEN_NATIVE_TILE_SHEET_COLUMNS);

    return (uint8_t)((slot_group << 6U) |
                     (uint8_t)(local_row * ARENA_SCREEN_NATIVE_TILE_SHEET_COLUMNS + local_col));
}

static bool arena_native_source_for_display_row(int display_row,
                                                unsigned *source_page_out,
                                                int *source_row_out,
                                                int *band_row_out)
{
    for (size_t i = 0; i < sizeof(ARENA_NATIVE_BANDS) / sizeof(ARENA_NATIVE_BANDS[0]); ++i) {
        const ArenaNativeBand *band = &ARENA_NATIVE_BANDS[i];
        int band_row = display_row - band->destination_first_row;

        if (band_row < 0 || band_row >= band->row_count) {
            continue;
        }
        if (source_page_out != NULL) {
            *source_page_out = band->source_page;
        }
        if (source_row_out != NULL) {
            *source_row_out = band->source_first_row + band_row;
        }
        if (band_row_out != NULL) {
            *band_row_out = band_row;
        }
        return true;
    }

    return false;
}

static int arena_native_sheet_row_for_source(unsigned source_page,
                                             int source_row,
                                             int band_row)
{
    if (source_page == ARENA_SCREEN_SMALL_CROWD_PAGE) {
        return band_row % ARENA_SCREEN_NATIVE_SLOT_TILE_ROWS;
    }
    if (source_row >= ARENA_SCREEN_BG_SPLIT_ROW) {
        return (band_row + 2) % ARENA_SCREEN_NATIVE_SLOT_TILE_ROWS;
    }
    return source_row % (ARENA_SCREEN_NATIVE_SLOT_TILE_ROWS * 4);
}

static uint8_t arena_native_palette_index_for_cell(int display_row, int col)
{
    if (display_row < 6) {
        return 0U;
    }
    if (display_row < ARENA_SCREEN_TOP_ROWS) {
        return (uint8_t)(1U + (((display_row + col) >> 2) & 0x01U));
    }
    if (display_row < ARENA_SCREEN_TOP_ROWS + ARENA_SCREEN_SMALL_CROWD_ROWS) {
        return (uint8_t)(1U + (((display_row ^ col) >> 1) & 0x01U));
    }
    return (uint8_t)(2U + (((display_row + col) >> 2) & 0x01U));
}

static bool arena_native_background_chr_available(uint64_t chr_byte_count)
{
    for (int row = 0; row < ARENA_SCREEN_NATIVE_ROWS; ++row) {
        unsigned source_page = 0U;
        int source_row = 0;
        int band_row = 0;
        int sheet_row;

        if (!arena_native_source_for_display_row(row, &source_page, &source_row, &band_row)) {
            return false;
        }

        sheet_row = arena_native_sheet_row_for_source(source_page, source_row, band_row);
        for (int col = 0; col < ARENA_SCREEN_NATIVE_COLUMNS; ++col) {
            uint8_t tile = arena_native_tile_from_sheet_row(sheet_row, col);
            uint64_t tile_offset = arena_native_bg_tile_offset_for_source(source_page,
                                                                         source_row,
                                                                         tile);
            if (!arena_chr_tile_offset_available(chr_byte_count, tile_offset)) {
                return false;
            }
        }
    }

    return true;
}

bool tecmo_intro_arena_native_chr_available(const uint8_t *chr_bytes,
                                            uint64_t chr_byte_count)
{
    if (chr_bytes == NULL || chr_byte_count == 0U) {
        return false;
    }
    return arena_native_background_chr_available(chr_byte_count);
}

bool tecmo_intro_arena_draw_native_chr(TecmoFramebuffer *fb,
                                       const uint8_t *chr_bytes,
                                       uint64_t chr_byte_count,
                                       unsigned frame,
                                       int origin_x,
                                       int origin_y,
                                       int scale)
{
    const uint8_t *background_palette;
    bool drew_any = false;

    if (fb == NULL || chr_bytes == NULL || chr_byte_count == 0U || scale <= 0) {
        return false;
    }
    if (!tecmo_intro_arena_native_chr_available(chr_bytes, chr_byte_count)) {
        return false;
    }

    background_palette = tecmo_intro_arena_palette_for_frame(NULL, frame);
    for (int row = 0; row < ARENA_SCREEN_NATIVE_ROWS; ++row) {
        unsigned source_page = 0U;
        int source_row = 0;
        int band_row = 0;
        int sheet_row;

        if (!arena_native_source_for_display_row(row, &source_page, &source_row, &band_row)) {
            return false;
        }
        sheet_row = arena_native_sheet_row_for_source(source_page, source_row, band_row);

        for (int col = 0; col < ARENA_SCREEN_NATIVE_COLUMNS; ++col) {
            uint8_t tile = arena_native_tile_from_sheet_row(sheet_row, col);
            uint8_t palette_index = arena_native_palette_index_for_cell(row, col);
            uint32_t palette[4];
            uint64_t tile_offset = arena_native_bg_tile_offset_for_source(source_page,
                                                                         source_row,
                                                                         tile);

            arena_build_palette(palette, background_palette, palette_index);
            tecmo_draw_chr_tile_at_offset_ex(fb,
                                             chr_bytes,
                                             chr_byte_count,
                                             tile_offset,
                                             origin_x + col * 8 * scale,
                                             origin_y + row * 8 * scale,
                                             scale,
                                             palette,
                                             false,
                                             false);
            drew_any = true;
        }
    }

    return drew_any;
}

static bool arena_native_goal_chr_available(uint64_t chr_byte_count)
{
    for (size_t i = 0; i < sizeof(ARENA_NATIVE_GOAL_TILE_PAIRS) /
                               sizeof(ARENA_NATIVE_GOAL_TILE_PAIRS[0]); ++i) {
        uint16_t top_tile = ARENA_NATIVE_GOAL_TILE_PAIRS[i].top_tile;
        if (!arena_chr_bank_tile_available(chr_byte_count,
                                           ARENA_SCREEN_DEFAULT_SPRITE_CHR_BANK,
                                           top_tile) ||
            !arena_chr_bank_tile_available(chr_byte_count,
                                           ARENA_SCREEN_DEFAULT_SPRITE_CHR_BANK,
                                           (uint16_t)(top_tile + 1U))) {
            return false;
        }
    }

    return true;
}

size_t tecmo_intro_arena_native_goal_chr_pair_count(const uint8_t *chr_bytes,
                                                    uint64_t chr_byte_count)
{
    if (chr_bytes == NULL || chr_byte_count == 0U || !arena_native_goal_chr_available(chr_byte_count)) {
        return 0U;
    }
    return sizeof(ARENA_NATIVE_GOAL_TILE_PAIRS) / sizeof(ARENA_NATIVE_GOAL_TILE_PAIRS[0]);
}

static void arena_native_goal_palette(uint32_t out_palette[4])
{
    out_palette[0] = 0x00000000U;
    out_palette[1] = tecmo_nes_2c02_rgba(0x02U);
    out_palette[2] = tecmo_nes_2c02_rgba(0x05U);
    out_palette[3] = tecmo_nes_2c02_rgba(0x30U);
}

static void arena_draw_native_goal_pair(TecmoFramebuffer *fb,
                                        const uint8_t *chr_bytes,
                                        uint64_t chr_byte_count,
                                        uint16_t top_tile,
                                        int x,
                                        int y,
                                        int scale,
                                        const uint32_t palette[4])
{
    tecmo_draw_chr_tile_ex(fb,
                           chr_bytes,
                           chr_byte_count,
                           ARENA_SCREEN_DEFAULT_SPRITE_CHR_BANK,
                           top_tile,
                           x,
                           y,
                           scale,
                           palette,
                           false,
                           false);
    tecmo_draw_chr_tile_ex(fb,
                           chr_bytes,
                           chr_byte_count,
                           ARENA_SCREEN_DEFAULT_SPRITE_CHR_BANK,
                           (uint16_t)(top_tile + 1U),
                           x,
                           y + 8 * scale,
                           scale,
                           palette,
                           false,
                           false);
}

size_t tecmo_intro_arena_draw_native_goal_chr(TecmoFramebuffer *fb,
                                              const uint8_t *chr_bytes,
                                              uint64_t chr_byte_count,
                                              unsigned frame,
                                              int origin_x,
                                              int origin_y,
                                              int scale)
{
    uint32_t palette[4];
    int goal_x;
    int goal_y;
    size_t drawn = 0U;

    (void)frame;
    if (fb == NULL || chr_bytes == NULL || chr_byte_count == 0U || scale <= 0) {
        return 0U;
    }
    if (tecmo_intro_arena_native_goal_chr_pair_count(chr_bytes, chr_byte_count) == 0U) {
        return 0U;
    }

    arena_native_goal_palette(palette);
    goal_x = origin_x + TECMO_ARENA_INTRO_GOAL_ANCHOR_X * scale;
    goal_y = origin_y + TECMO_ARENA_INTRO_GOAL_ANCHOR_Y * scale;

    for (size_t i = 0; i < sizeof(ARENA_NATIVE_GOAL_TILE_PAIRS) /
                               sizeof(ARENA_NATIVE_GOAL_TILE_PAIRS[0]); ++i) {
        const ArenaNativeGoalTilePair *pair = &ARENA_NATIVE_GOAL_TILE_PAIRS[i];
        arena_draw_native_goal_pair(fb,
                                    chr_bytes,
                                    chr_byte_count,
                                    pair->top_tile,
                                    goal_x + pair->offset_x * scale,
                                    goal_y + pair->offset_y * scale,
                                    scale,
                                    palette);
        ++drawn;
    }

    return drawn;
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
