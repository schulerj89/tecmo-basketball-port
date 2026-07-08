#include "tecmo_intro_post_arena.h"
#include "tecmo_nes_video.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define POST_CHR_1KB_BYTES 1024ULL

typedef struct PostTileCell {
    bool present;
    uint8_t tile;
    TecmoIntroPostMapperState mapper;
} PostTileCell;

static const uint8_t POST_BLACK_PALETTE[16] = {
    0x0FU, 0x0FU, 0x0FU, 0x0FU,
    0x0FU, 0x0FU, 0x0FU, 0x0FU,
    0x0FU, 0x0FU, 0x0FU, 0x0FU,
    0x0FU, 0x0FU, 0x0FU, 0x0FU,
};

static const TecmoIntroPostMapperState POST_READY_DEFAULT_MAPPER = {
    {0xF0U, 0xF2U, 0x08U, 0x09U, 0x00U, 0x00U, 0x08U, 0x09U}
};

static const TecmoIntroPostMapperState POST_WARRIORS_DEFAULT_MAPPER = {
    {0x0AU, 0xFAU, 0x91U, 0x93U, 0x95U, 0x97U, 0x08U, 0x09U}
};

static void post_rect(TecmoFramebuffer *fb, int x, int y, int w, int h, uint32_t color)
{
    int x0 = x;
    int y0 = y;
    int x1 = x + w;
    int y1 = y + h;

    if (fb == NULL || fb->pixels == NULL || w <= 0 || h <= 0) {
        return;
    }
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > fb->width) x1 = fb->width;
    if (y1 > fb->height) y1 = fb->height;
    if (x0 >= x1 || y0 >= y1) {
        return;
    }

    for (int row = y0; row < y1; ++row) {
        uint32_t *dst = fb->pixels + (size_t)row * (size_t)fb->pitch_pixels + (size_t)x0;
        for (int col = x0; col < x1; ++col) {
            *dst++ = color;
        }
    }
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

    if (value == NULL || value >= end || out == NULL) {
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

    if (cursor_io == NULL || *cursor_io == NULL ||
        address_out == NULL || value_out == NULL) {
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

static void post_capture_status(TecmoIntroPostArenaCapture *capture, const char *text)
{
    if (capture == NULL) {
        return;
    }
    if (text == NULL) {
        text = "";
    }
    (void)snprintf(capture->status, sizeof(capture->status), "%s", text);
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

static const char *post_capture_env_path(char *buffer, size_t buffer_size)
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

static void parse_mapper_batch(TecmoIntroPostMapperState *state, const char *start, const char *end)
{
    const char *cursor = start;
    int selected = 0;

    if (state == NULL) {
        return;
    }

    while (cursor < end) {
        while (cursor < end && (*cursor == ',' || isspace((unsigned char)*cursor))) {
            ++cursor;
        }
        if (cursor + 5 <= end && strncmp(cursor, "8000=", 5) == 0) {
            char *parse_end;
            unsigned long value = strtoul(cursor + 5, &parse_end, 16);
            if (parse_end != cursor + 5) {
                selected = (int)(value & 0x07UL);
                cursor = parse_end;
                continue;
            }
        } else if (cursor + 5 <= end && strncmp(cursor, "8001", 4) == 0) {
            const char *value_start = strchr(cursor, '=');
            char *parse_end;
            int reg = selected;
            if (value_start != NULL && value_start < end) {
                if (cursor[4] == '[' && cursor + 7 < value_start) {
                    reg = (int)strtoul(cursor + 5, NULL, 16);
                }
                if (reg >= 0 && reg < 8) {
                    unsigned long value = strtoul(value_start + 1, &parse_end, 16);
                    if (parse_end != value_start + 1 && value <= 0xFFUL) {
                        state->regs[reg] = (uint8_t)value;
                        cursor = parse_end;
                        continue;
                    }
                }
            }
        }

        while (cursor < end && *cursor != ',') {
            ++cursor;
        }
        if (cursor < end && *cursor == ',') {
            ++cursor;
        }
    }
}

static void append_tile_event(TecmoIntroPostArenaCapture *capture,
                              unsigned frame,
                              unsigned address,
                              unsigned value,
                              const TecmoIntroPostMapperState *mapper)
{
    TecmoIntroPostTileEvent *event;

    if (capture == NULL || mapper == NULL || value > 0xFFU ||
        address < 0x2000U || address >= 0x2800U) {
        return;
    }
    if (((address - 0x2000U) % 0x400U) >= TECMO_INTRO_POST_TILES_PER_PAGE) {
        return;
    }
    if (capture->tile_event_count >= TECMO_INTRO_POST_MAX_TILE_EVENTS) {
        capture->truncated = true;
        return;
    }

    event = &capture->tile_events[capture->tile_event_count++];
    event->frame = frame;
    event->ppu = (uint16_t)address;
    event->tile = (uint8_t)value;
    event->mapper = *mapper;
    capture->last_capture_frame = frame;
}

static void append_attribute_event(TecmoIntroPostArenaCapture *capture,
                                   unsigned frame,
                                   unsigned address,
                                   unsigned value)
{
    TecmoIntroPostAttributeEvent *event;

    if (capture == NULL || value > 0xFFU) {
        return;
    }
    if (!((address >= 0x23C0U && address < 0x2400U) ||
          (address >= 0x27C0U && address < 0x2800U))) {
        return;
    }
    if (capture->attribute_event_count >= TECMO_INTRO_POST_MAX_ATTRIBUTE_EVENTS) {
        capture->truncated = true;
        return;
    }

    event = &capture->attribute_events[capture->attribute_event_count++];
    event->frame = frame;
    event->ppu = (uint16_t)address;
    event->value = (uint8_t)value;
    capture->last_capture_frame = frame;
}

static void parse_nametable_pairs(TecmoIntroPostArenaCapture *capture,
                                  unsigned frame,
                                  const char *start,
                                  const char *end,
                                  const TecmoIntroPostMapperState *mapper)
{
    const char *cursor = start;

    while (cursor < end) {
        unsigned address;
        unsigned value;
        if (parse_hex_pair(&cursor, end, &address, &value)) {
            append_tile_event(capture, frame, address, value, mapper);
        }
    }
}

static void parse_attribute_pairs(TecmoIntroPostArenaCapture *capture,
                                  unsigned frame,
                                  const char *start,
                                  const char *end)
{
    const char *cursor = start;

    while (cursor < end) {
        unsigned address;
        unsigned value;
        if (parse_hex_pair(&cursor, end, &address, &value)) {
            append_attribute_event(capture, frame, address, value);
        }
    }
}

static void parse_palette_stage(TecmoIntroPostArenaCapture *capture,
                                unsigned frame,
                                const char *start,
                                const char *end)
{
    TecmoIntroPostPaletteStage *stage;
    const char *cursor = start;

    if (capture == NULL ||
        capture->palette_stage_count >= TECMO_INTRO_POST_MAX_PALETTE_STAGES) {
        if (capture != NULL) {
            capture->truncated = true;
        }
        return;
    }

    stage = &capture->palette_stages[capture->palette_stage_count];
    stage->frame = frame;
    if (capture->palette_stage_count > 0U) {
        const TecmoIntroPostPaletteStage *previous =
            &capture->palette_stages[capture->palette_stage_count - 1U];
        memcpy(stage->background, previous->background, sizeof(stage->background));
        memcpy(stage->sprites, previous->sprites, sizeof(stage->sprites));
    } else {
        memcpy(stage->background, POST_BLACK_PALETTE, sizeof(stage->background));
        memcpy(stage->sprites, POST_BLACK_PALETTE, sizeof(stage->sprites));
    }

    while (cursor < end) {
        unsigned address;
        unsigned value;
        if (parse_hex_pair(&cursor, end, &address, &value)) {
            if (address >= 0x3F00U && address < 0x3F10U && value <= 0xFFU) {
                stage->background[address - 0x3F00U] = (uint8_t)value;
            } else if (address >= 0x3F10U && address < 0x3F20U && value <= 0xFFU) {
                stage->sprites[address - 0x3F10U] = (uint8_t)value;
            }
        }
    }

    ++capture->palette_stage_count;
    capture->last_capture_frame = frame;
}

static bool parse_oam_quad(const char **cursor_io,
                           const char *end,
                           TecmoIntroPostSprite *sprite_out)
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

static void parse_visible_oam_stage(TecmoIntroPostArenaCapture *capture,
                                    unsigned frame,
                                    const char *start,
                                    const char *end,
                                    const TecmoIntroPostMapperState *mapper)
{
    const char *cursor = start;
    TecmoIntroPostSpriteStage *stage;
    size_t count = 0;

    if (capture == NULL || mapper == NULL ||
        capture->sprite_stage_count >= TECMO_INTRO_POST_MAX_SPRITE_STAGES) {
        if (capture != NULL) {
            capture->truncated = true;
        }
        return;
    }

    stage = &capture->sprite_stages[capture->sprite_stage_count];
    stage->frame = frame;
    stage->mapper = *mapper;
    while (cursor < end && count < TECMO_INTRO_POST_MAX_SPRITES) {
        TecmoIntroPostSprite sprite;
        if (parse_oam_quad(&cursor, end, &sprite)) {
            stage->sprites[count++] = sprite;
        }
    }
    stage->sprite_count = count;
    ++capture->sprite_stage_count;
    capture->last_capture_frame = frame;
}

static void parse_scroll_stage(TecmoIntroPostArenaCapture *capture,
                               unsigned frame,
                               const char *start,
                               const char *end)
{
    const char *cursor = start;
    int selected_x = 0;
    bool found = false;
    TecmoIntroPostScrollStage *stage;

    if (capture == NULL) {
        return;
    }

    while (cursor < end) {
        const char *match = strstr(cursor, "2005X=");
        char *parse_end;
        unsigned long value;
        if (match == NULL || match >= end) {
            break;
        }
        value = strtoul(match + 6, &parse_end, 16);
        if (parse_end != match + 6 && value <= 0xFFUL) {
            int signed_value = value >= 0x80UL ? (int)value - 0x100 : (int)value;
            if (signed_value != 0 || !found) {
                selected_x = signed_value;
                found = true;
            }
            cursor = parse_end;
        } else {
            cursor = match + 6;
        }
    }

    if (!found || capture->scroll_stage_count >= TECMO_INTRO_POST_MAX_SPRITE_STAGES) {
        return;
    }

    stage = &capture->scroll_stages[capture->scroll_stage_count++];
    stage->frame = frame;
    stage->scroll_x = selected_x;
}

static bool load_post_capture_file(TecmoIntroPostArenaCapture *capture, const char *path)
{
    char *json;
    size_t json_size = 0;
    const char *cursor;
    const char *json_end;
    TecmoIntroPostMapperState mapper = POST_READY_DEFAULT_MAPPER;

    json = read_text_file(path, &json_size);
    if (json == NULL) {
        return false;
    }

    cursor = json;
    json_end = json + json_size;

    while (cursor < json_end) {
        const char *line_end = strchr(cursor, '\n');
        int frame_value = 0;
        unsigned frame;

        if (line_end == NULL || line_end > json_end) {
            line_end = json_end;
        }

        if (json_int_field(cursor, line_end, "frame", &frame_value) &&
            frame_value >= (int)TECMO_INTRO_READY_CAPTURE_FIRST &&
            frame_value <= (int)TECMO_INTRO_WARRIORS_CAPTURE_LAST) {
            const char *pair_start;
            const char *pair_end;
            frame = (unsigned)frame_value;
            if (capture->first_capture_frame == 0U) {
                capture->first_capture_frame = frame;
            }

            if (json_string_field_equals(cursor, line_end, "kind", "mapper_write_batch") &&
                json_string_span(cursor, line_end, "mapper_writes", &pair_start, &pair_end)) {
                parse_mapper_batch(&mapper, pair_start, pair_end);
            } else if (json_string_field_equals(cursor, line_end, "kind", "ppu_nametable_write_batch") &&
                       json_string_span(cursor, line_end, "nametable_writes", &pair_start, &pair_end)) {
                parse_nametable_pairs(capture, frame, pair_start, pair_end, &mapper);
            } else if (json_string_field_equals(cursor, line_end, "kind", "ppu_attribute_write_batch") &&
                       json_string_span(cursor, line_end, "attribute_writes", &pair_start, &pair_end)) {
                parse_attribute_pairs(capture, frame, pair_start, pair_end);
            } else if (json_string_field_equals(cursor, line_end, "kind", "ppu_palette_write_batch") &&
                       json_string_span(cursor, line_end, "palette_writes", &pair_start, &pair_end)) {
                parse_palette_stage(capture, frame, pair_start, pair_end);
            } else if (json_string_field_equals(cursor, line_end, "kind", "oam_frame_diff") &&
                       json_string_span(cursor, line_end, "visible_oam", &pair_start, &pair_end)) {
                TecmoIntroPostMapperState sprite_mapper = mapper;
                if (frame >= TECMO_INTRO_WARRIORS_CAPTURE_FIRST) {
                    sprite_mapper = POST_WARRIORS_DEFAULT_MAPPER;
                }
                parse_visible_oam_stage(capture, frame, pair_start, pair_end, &sprite_mapper);
            } else if (json_string_field_equals(cursor, line_end, "kind", "ppu_scroll_write_batch") &&
                       json_string_span(cursor, line_end, "ppu_scroll_writes", &pair_start, &pair_end)) {
                parse_scroll_stage(capture, frame, pair_start, pair_end);
            }
        }

        cursor = line_end;
        if (cursor < json_end && *cursor == '\n') {
            ++cursor;
        }
    }

    capture->available = capture->tile_event_count > 0U || capture->sprite_stage_count > 0U;
    if (capture->available) {
        (void)snprintf(capture->status,
                       sizeof(capture->status),
                       "POST ARENA TRACE %s  NT %u ATTR %u PAL %u OAM %u%s",
                       path,
                       (unsigned)capture->tile_event_count,
                       (unsigned)capture->attribute_event_count,
                       (unsigned)capture->palette_stage_count,
                       (unsigned)capture->sprite_stage_count,
                       capture->truncated ? " TRUNCATED" : "");
    }

    free(json);
    return capture->available;
}

bool tecmo_intro_post_arena_capture_load(TecmoIntroPostArenaCapture *capture,
                                         const char *project_root)
{
    char project_path[260];
    char env_path_buffer[260];
    const char *env_path;
    const char *paths[5];

    if (capture == NULL) {
        return false;
    }

    memset(capture, 0, sizeof(*capture));
    post_capture_status(capture, "RUN FCEUX LUA WATCH THROUGH READY/WARRIORS INTRO");

    append_path(project_path,
                sizeof(project_path),
                project_root,
                "build\\emu_intro_memory_watch.ndjson");
    env_path = post_capture_env_path(env_path_buffer, sizeof(env_path_buffer));

    paths[0] = env_path;
    paths[1] = project_path[0] != '\0' ? project_path : NULL;
    paths[2] = "build\\emu_intro_memory_watch.ndjson";
    paths[3] = "emu_intro_memory_watch.ndjson";
    paths[4] = "..\\build\\emu_intro_memory_watch.ndjson";

    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        if (paths[i] != NULL && paths[i][0] != '\0' &&
            load_post_capture_file(capture, paths[i])) {
            return true;
        }
    }

    return false;
}

unsigned tecmo_intro_ready_capture_frame(unsigned native_frame)
{
    unsigned frame = TECMO_INTRO_READY_CAPTURE_FIRST + native_frame;
    if (frame > TECMO_INTRO_READY_CAPTURE_LAST) {
        frame = TECMO_INTRO_READY_CAPTURE_LAST;
    }
    return frame;
}

unsigned tecmo_intro_warriors_capture_frame(unsigned native_frame)
{
    unsigned frame = TECMO_INTRO_WARRIORS_CAPTURE_FIRST + native_frame;
    if (frame > TECMO_INTRO_WARRIORS_CAPTURE_LAST) {
        frame = TECMO_INTRO_WARRIORS_CAPTURE_LAST;
    }
    return frame;
}

static bool ppu_tile_position(uint16_t ppu, unsigned *page_out, unsigned *offset_out)
{
    unsigned page;
    unsigned offset;

    if (ppu < 0x2000U || ppu >= 0x2800U || page_out == NULL || offset_out == NULL) {
        return false;
    }
    page = ppu >= 0x2400U ? 1U : 0U;
    offset = ppu - (page == 0U ? 0x2000U : 0x2400U);
    if (offset >= TECMO_INTRO_POST_TILES_PER_PAGE) {
        return false;
    }
    *page_out = page;
    *offset_out = offset;
    return true;
}

static bool ppu_attribute_position(uint16_t ppu, unsigned *page_out, unsigned *index_out)
{
    if (page_out == NULL || index_out == NULL) {
        return false;
    }
    if (ppu >= 0x23C0U && ppu < 0x2400U) {
        *page_out = 0U;
        *index_out = ppu - 0x23C0U;
        return true;
    }
    if (ppu >= 0x27C0U && ppu < 0x2800U) {
        *page_out = 1U;
        *index_out = ppu - 0x27C0U;
        return true;
    }
    return false;
}

static uint8_t attribute_byte(const uint8_t attributes[TECMO_INTRO_POST_PAGE_COUNT][64],
                              unsigned page,
                              int row,
                              int col)
{
    size_t index;

    if (page >= TECMO_INTRO_POST_PAGE_COUNT) {
        return 0U;
    }
    index = (size_t)(row / 4) * 8U + (size_t)(col / 4);
    if (index >= 64U) {
        return 0U;
    }
    return attributes[page][index];
}

static void build_palette(uint32_t out_palette[4],
                          const uint8_t nes_palette[16],
                          uint8_t palette_index)
{
    uint8_t base = (uint8_t)((palette_index & 0x03U) * 4U);

    out_palette[0] = tecmo_nes_2c02_rgba(nes_palette[0]);
    for (size_t i = 1; i < 4U; ++i) {
        out_palette[i] = tecmo_nes_2c02_rgba(nes_palette[(size_t)base + i]);
    }
}

static void build_sprite_palette(uint32_t out_palette[4],
                                 const uint8_t nes_palette[16],
                                 uint8_t palette_index)
{
    uint8_t base = (uint8_t)((palette_index & 0x03U) * 4U);

    out_palette[0] = 0x00000000U;
    for (size_t i = 1; i < 4U; ++i) {
        out_palette[i] = tecmo_nes_2c02_rgba(nes_palette[(size_t)base + i]);
    }
}

static uint64_t mmc3_ppu_offset(const TecmoIntroPostMapperState *mapper, uint16_t ppu_address)
{
    unsigned slot = (unsigned)(ppu_address >> 10U) & 0x07U;
    unsigned local = (unsigned)(ppu_address & 0x03FFU);
    uint8_t bank;

    if (mapper == NULL) {
        mapper = &POST_WARRIORS_DEFAULT_MAPPER;
    }

    switch (slot) {
    case 0U:
        bank = (uint8_t)(mapper->regs[0] & 0xFEU);
        break;
    case 1U:
        bank = (uint8_t)((mapper->regs[0] & 0xFEU) + 1U);
        break;
    case 2U:
        bank = (uint8_t)(mapper->regs[1] & 0xFEU);
        break;
    case 3U:
        bank = (uint8_t)((mapper->regs[1] & 0xFEU) + 1U);
        break;
    case 4U:
        bank = mapper->regs[2];
        break;
    case 5U:
        bank = mapper->regs[3];
        break;
    case 6U:
        bank = mapper->regs[4];
        break;
    default:
        bank = mapper->regs[5];
        break;
    }

    return (uint64_t)bank * POST_CHR_1KB_BYTES + (uint64_t)local;
}

static uint64_t bg_tile_offset(const TecmoIntroPostMapperState *mapper, uint8_t tile)
{
    return mmc3_ppu_offset(mapper, (uint16_t)tile * 16U);
}

static uint64_t sprite_tile_offset(const TecmoIntroPostMapperState *mapper, uint8_t tile, bool bottom)
{
    uint16_t base = (tile & 1U) != 0U ? 0x1000U : 0x0000U;
    uint16_t even_tile = (uint16_t)(tile & 0xFEU);
    uint16_t address = (uint16_t)(base + even_tile * 16U + (bottom ? 16U : 0U));
    return mmc3_ppu_offset(mapper, address);
}

static const TecmoIntroPostPaletteStage *palette_for_frame(const TecmoIntroPostArenaCapture *capture,
                                                           unsigned frame)
{
    const TecmoIntroPostPaletteStage *selected = NULL;

    if (capture == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < capture->palette_stage_count; ++i) {
        if (capture->palette_stages[i].frame <= frame) {
            selected = &capture->palette_stages[i];
        }
    }
    return selected;
}

static const TecmoIntroPostSpriteStage *sprite_stage_for_frame(const TecmoIntroPostArenaCapture *capture,
                                                               unsigned frame)
{
    const TecmoIntroPostSpriteStage *selected = NULL;

    if (capture == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < capture->sprite_stage_count; ++i) {
        const TecmoIntroPostSpriteStage *stage = &capture->sprite_stages[i];
        if (stage->frame >= TECMO_INTRO_WARRIORS_CAPTURE_FIRST && stage->frame <= frame) {
            selected = stage;
        }
    }
    return selected;
}

static int scroll_x_for_frame(const TecmoIntroPostArenaCapture *capture, unsigned frame)
{
    int selected = 0;

    if (capture == NULL) {
        return 0;
    }
    for (size_t i = 0; i < capture->scroll_stage_count; ++i) {
        if (capture->scroll_stages[i].frame >= TECMO_INTRO_WARRIORS_CAPTURE_FIRST &&
            capture->scroll_stages[i].frame <= frame) {
            selected = capture->scroll_stages[i].scroll_x;
        }
    }
    return selected;
}

static void build_scene_cells(const TecmoIntroPostArenaCapture *capture,
                              unsigned first_frame,
                              unsigned target_frame,
                              PostTileCell tiles[TECMO_INTRO_POST_PAGE_COUNT][TECMO_INTRO_POST_TILES_PER_PAGE],
                              uint8_t attributes[TECMO_INTRO_POST_PAGE_COUNT][64])
{
    if (tiles == NULL || attributes == NULL) {
        return;
    }
    memset(tiles, 0, sizeof(PostTileCell) * TECMO_INTRO_POST_PAGE_COUNT * TECMO_INTRO_POST_TILES_PER_PAGE);
    memset(attributes, 0, 64U * TECMO_INTRO_POST_PAGE_COUNT);

    if (capture == NULL) {
        return;
    }

    for (size_t i = 0; i < capture->attribute_event_count; ++i) {
        const TecmoIntroPostAttributeEvent *event = &capture->attribute_events[i];
        unsigned page;
        unsigned index;
        if (event->frame < first_frame || event->frame > target_frame) {
            continue;
        }
        if (ppu_attribute_position(event->ppu, &page, &index)) {
            attributes[page][index] = event->value;
        }
    }

    for (size_t i = 0; i < capture->tile_event_count; ++i) {
        const TecmoIntroPostTileEvent *event = &capture->tile_events[i];
        unsigned page;
        unsigned offset;
        if (event->frame < first_frame || event->frame > target_frame) {
            continue;
        }
        if (ppu_tile_position(event->ppu, &page, &offset)) {
            tiles[page][offset].present = true;
            tiles[page][offset].tile = event->tile;
            tiles[page][offset].mapper = event->mapper;
        }
    }
}

static void draw_scene_tiles(TecmoFramebuffer *fb,
                             const uint8_t *chr_bytes,
                             uint64_t chr_byte_count,
                             const PostTileCell tiles[TECMO_INTRO_POST_PAGE_COUNT][TECMO_INTRO_POST_TILES_PER_PAGE],
                             const uint8_t attributes[TECMO_INTRO_POST_PAGE_COUNT][64],
                             const uint8_t palette[16],
                             int origin_x,
                             int origin_y,
                             int scale,
                             int scroll_x)
{
    for (unsigned page = 0; page < TECMO_INTRO_POST_PAGE_COUNT; ++page) {
        int page_x = origin_x + ((int)page * 256 + scroll_x) * scale;
        for (unsigned offset = 0; offset < TECMO_INTRO_POST_TILES_PER_PAGE; ++offset) {
            const PostTileCell *cell = &tiles[page][offset];
            int row;
            int col;
            uint8_t attribute;
            uint8_t palette_index;
            uint32_t rgba[4];
            uint64_t tile_offset;

            if (!cell->present) {
                continue;
            }

            row = (int)(offset / 32U);
            col = (int)(offset % 32U);
            attribute = attribute_byte(attributes, page, row, col);
            palette_index = tecmo_nes_attribute_palette_index(attribute, row, col);
            build_palette(rgba, palette, palette_index);
            tile_offset = bg_tile_offset(&cell->mapper, cell->tile);

            tecmo_draw_chr_tile_at_offset_ex(fb,
                                             chr_bytes,
                                             chr_byte_count,
                                             tile_offset,
                                             page_x + col * 8 * scale,
                                             origin_y + row * 8 * scale,
                                             scale,
                                             rgba,
                                             false,
                                             false);
        }
    }
}

static void draw_sprite_stage(TecmoFramebuffer *fb,
                              const uint8_t *chr_bytes,
                              uint64_t chr_byte_count,
                              const TecmoIntroPostSpriteStage *stage,
                              const uint8_t sprite_palette[16],
                              int origin_x,
                              int origin_y,
                              int scale)
{
    if (stage == NULL) {
        return;
    }

    for (size_t i = 0; i < stage->sprite_count; ++i) {
        const TecmoIntroPostSprite *sprite = &stage->sprites[i];
        uint8_t palette_index = (uint8_t)(sprite->attributes & 0x03U);
        bool flip_horizontal = (sprite->attributes & 0x40U) != 0U;
        bool flip_vertical = (sprite->attributes & 0x80U) != 0U;
        uint32_t rgba[4];
        uint64_t top_offset;
        uint64_t bottom_offset;
        int x = origin_x + (int)sprite->x * scale;
        int y = origin_y + (int)sprite->y * scale;

        build_sprite_palette(rgba, sprite_palette, palette_index);
        if (flip_vertical) {
            top_offset = sprite_tile_offset(&stage->mapper, sprite->tile, true);
            bottom_offset = sprite_tile_offset(&stage->mapper, sprite->tile, false);
        } else {
            top_offset = sprite_tile_offset(&stage->mapper, sprite->tile, false);
            bottom_offset = sprite_tile_offset(&stage->mapper, sprite->tile, true);
        }

        tecmo_draw_chr_tile_at_offset_ex(fb,
                                         chr_bytes,
                                         chr_byte_count,
                                         top_offset,
                                         x,
                                         y,
                                         scale,
                                         rgba,
                                         flip_horizontal,
                                         flip_vertical);
        tecmo_draw_chr_tile_at_offset_ex(fb,
                                         chr_bytes,
                                         chr_byte_count,
                                         bottom_offset,
                                         x,
                                         y + 8 * scale,
                                         scale,
                                         rgba,
                                         flip_horizontal,
                                         flip_vertical);
    }
}

static void draw_capture_scene(TecmoFramebuffer *fb,
                               const TecmoIntroPostArenaCapture *capture,
                               const uint8_t *chr_bytes,
                               uint64_t chr_byte_count,
                               unsigned scene_first_frame,
                               unsigned target_frame,
                               bool draw_warriors_sprites,
                               int origin_x,
                               int origin_y,
                               int scale)
{
    PostTileCell tiles[TECMO_INTRO_POST_PAGE_COUNT][TECMO_INTRO_POST_TILES_PER_PAGE];
    uint8_t attributes[TECMO_INTRO_POST_PAGE_COUNT][64];
    const TecmoIntroPostPaletteStage *palette_stage;
    int scroll_x = draw_warriors_sprites ? scroll_x_for_frame(capture, target_frame) : 0;

    post_rect(fb, 0, 0, fb->width, fb->height, tecmo_nes_2c02_rgba(0x0FU));
    if (capture == NULL || !capture->available || chr_bytes == NULL || chr_byte_count == 0U) {
        return;
    }

    palette_stage = palette_for_frame(capture, target_frame);
    build_scene_cells(capture, scene_first_frame, target_frame, tiles, attributes);
    draw_scene_tiles(fb,
                     chr_bytes,
                     chr_byte_count,
                     tiles,
                     attributes,
                     palette_stage != NULL ? palette_stage->background : POST_BLACK_PALETTE,
                     origin_x,
                     origin_y,
                     scale,
                     scroll_x);

    if (draw_warriors_sprites && palette_stage != NULL) {
        draw_sprite_stage(fb,
                          chr_bytes,
                          chr_byte_count,
                          sprite_stage_for_frame(capture, target_frame),
                          palette_stage->sprites,
                          origin_x,
                          origin_y,
                          scale);
    }
}

void tecmo_intro_post_arena_draw_ready(TecmoFramebuffer *fb,
                                       const TecmoIntroPostArenaCapture *capture,
                                       const uint8_t *chr_bytes,
                                       uint64_t chr_byte_count,
                                       unsigned native_frame,
                                       int origin_x,
                                       int origin_y,
                                       int scale)
{
    unsigned frame = tecmo_intro_ready_capture_frame(native_frame);
    draw_capture_scene(fb,
                       capture,
                       chr_bytes,
                       chr_byte_count,
                       TECMO_INTRO_READY_CAPTURE_FIRST,
                       frame,
                       false,
                       origin_x,
                       origin_y,
                       scale);
}

void tecmo_intro_post_arena_draw_warriors(TecmoFramebuffer *fb,
                                          const TecmoIntroPostArenaCapture *capture,
                                          const uint8_t *chr_bytes,
                                          uint64_t chr_byte_count,
                                          unsigned native_frame,
                                          int origin_x,
                                          int origin_y,
                                          int scale)
{
    unsigned frame = tecmo_intro_warriors_capture_frame(native_frame);
    draw_capture_scene(fb,
                       capture,
                       chr_bytes,
                       chr_byte_count,
                       TECMO_INTRO_WARRIORS_CAPTURE_FIRST,
                       frame,
                       true,
                       origin_x,
                       origin_y,
                       scale);
}
