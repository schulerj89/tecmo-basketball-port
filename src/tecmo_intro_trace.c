#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_game.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_intro_trace_status(char *dest, size_t dest_size, const char *text)
{
    if (dest_size == 0) {
        return;
    }
    if (text == NULL) {
        text = "";
    }
    (void)snprintf(dest, dest_size, "%s", text);
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
    while (cursor < end &&
           (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n')) {
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

static bool parse_json_string_field(const char *start,
                                    const char *end,
                                    const char *key,
                                    char *out,
                                    size_t out_size)
{
    const char *value = find_json_value(start, end, key);
    size_t count = 0;

    if (out_size == 0) {
        return false;
    }
    out[0] = '\0';
    if (value == NULL || value >= end || *value != '"') {
        return false;
    }
    ++value;
    while (value < end && *value != '"') {
        if (count + 1U < out_size) {
            out[count++] = *value;
        }
        ++value;
    }
    out[count] = '\0';
    return value < end && *value == '"';
}

static bool parse_json_int_field(const char *start, const char *end, const char *key, int *out)
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

static bool parse_json_hex_byte_field(const char *start, const char *end, const char *key, uint8_t *out)
{
    char text[16];
    char *parse_end;
    unsigned long parsed;

    if (!parse_json_string_field(start, end, key, text, sizeof(text))) {
        return false;
    }
    parsed = strtoul(text, &parse_end, 16);
    if (parse_end == text || parsed > 0xFFUL) {
        return false;
    }
    *out = (uint8_t)parsed;
    return true;
}

static bool parse_json_hex_byte_array_after(const char *start,
                                            const char *end,
                                            const char *key,
                                            uint8_t *out,
                                            size_t out_count)
{
    const char *value = find_json_value(start, end, key);
    const char *cursor;
    size_t count = 0;

    if (out == NULL || out_count == 0 || value == NULL || value >= end || *value != '[') {
        return false;
    }
    cursor = value + 1;
    while (cursor < end && count < out_count) {
        char text[8];
        char *parse_end;
        size_t len;
        unsigned long parsed;
        const char *open = strchr(cursor, '"');
        const char *close;
        if (open == NULL || open >= end) {
            break;
        }
        close = strchr(open + 1, '"');
        if (close == NULL || close >= end) {
            break;
        }
        len = (size_t)(close - open - 1);
        if (len == 0 || len >= sizeof(text)) {
            return false;
        }
        memcpy(text, open + 1, len);
        text[len] = '\0';
        parsed = strtoul(text, &parse_end, 16);
        if (parse_end == text || parsed > 0xFFUL) {
            return false;
        }
        out[count++] = (uint8_t)parsed;
        cursor = close + 1;
    }

    return count == out_count;
}

static void append_intro_trace_sprite(TecmoRuntime *runtime,
                                      uint8_t group,
                                      uint8_t tile_low,
                                      uint8_t attributes,
                                      int screen_x,
                                      int screen_y)
{
    TecmoIntroTraceSprite *sprite;
    if (runtime->intro_trace_sprite_count >= TECMO_MAX_INTRO_TRACE_SPRITES) {
        runtime->intro_trace_truncated = true;
        return;
    }
    sprite = &runtime->intro_trace_sprites[runtime->intro_trace_sprite_count++];
    sprite->group = group;
    sprite->tile_low = tile_low;
    sprite->attributes = attributes;
    sprite->screen_x = screen_x;
    sprite->screen_y = screen_y;
}

size_t tecmo_intro_trace_group_count(const TecmoRuntime *runtime, uint8_t group)
{
    size_t count = 0;
    for (size_t i = 0; i < runtime->intro_trace_sprite_count; ++i) {
        if (runtime->intro_trace_sprites[i].group == group) {
            ++count;
        }
    }
    return count;
}

static bool load_intro_trace_from_json(TecmoRuntime *runtime, const char *json, size_t json_size)
{
    const char *json_end = json + json_size;
    const char *primary = strstr(json, "\"primary_streams\"");
    const char *section_end = json_end;
    const char *cursor = primary != NULL ? primary : json;
    int chr_bank = 31;

    (void)parse_json_int_field(json, json_end, "chr_bank", &chr_bank);
    if (chr_bank < 0) {
        chr_bank = 31;
    }
    runtime->intro_trace_chr_bank = (uint32_t)chr_bank;
    runtime->intro_trace_sprite_count = 0;
    runtime->intro_trace_truncated = false;
    runtime->intro_l88e7_palette_available = parse_json_hex_byte_array_after(json,
                                                                             json_end,
                                                                             "palette_snapshot_bytes_hex",
                                                                             runtime->intro_l88e7_palette,
                                                                             sizeof(runtime->intro_l88e7_palette));
    runtime->intro_l88e7_irq_vector_available = parse_json_string_field(json,
                                                                        json_end,
                                                                        "vector_target_cpu",
                                                                        runtime->intro_l88e7_irq_vector,
                                                                        sizeof(runtime->intro_l88e7_irq_vector));
    runtime->intro_presents_data_available = parse_json_string_field(json,
                                                                     json_end,
                                                                     "presents_match_cpu",
                                                                     runtime->intro_presents_data_cpu,
                                                                     sizeof(runtime->intro_presents_data_cpu));

    while (cursor < section_end) {
        const char *component_pos = strstr(cursor, "\"component\"");
        const char *record_end;
        char component[48];
        char role[64];
        uint8_t tile_low;
        uint8_t attributes;
        int screen_x;
        int screen_y;
        bool appended = false;

        if (component_pos == NULL || component_pos >= section_end) {
            break;
        }
        record_end = strchr(component_pos, '}');
        if (record_end == NULL || record_end >= section_end) {
            break;
        }

        if (parse_json_string_field(component_pos, record_end, "component", component, sizeof(component)) &&
            parse_json_string_field(component_pos, record_end, "role", role, sizeof(role)) &&
            parse_json_hex_byte_field(component_pos, record_end, "oam_tile_low_after_offset_hex", &tile_low) &&
            parse_json_hex_byte_field(component_pos, record_end, "attributes_hex", &attributes) &&
            parse_json_int_field(component_pos, record_end, "screen_x_from_current_base", &screen_x) &&
            parse_json_int_field(component_pos, record_end, "screen_y_from_current_base", &screen_y)) {
            if (strcmp(component, "rabbit_full_stream") == 0) {
                append_intro_trace_sprite(runtime, INTRO_TRACE_GROUP_RABBIT, tile_low, attributes, screen_x, screen_y);
                appended = true;
            } else if (strcmp(component, "tecmo_selector0") == 0) {
                append_intro_trace_sprite(runtime, INTRO_TRACE_GROUP_TECMO_STREAM, tile_low, attributes, screen_x, screen_y);
                appended = true;
                if (strcmp(role, "tecmo_logo_candidate") == 0) {
                    append_intro_trace_sprite(runtime, INTRO_TRACE_GROUP_TECMO_LOGO, tile_low, attributes, screen_x, screen_y);
                }
            } else if (strcmp(component, "a7db_selector0") == 0) {
                append_intro_trace_sprite(runtime, INTRO_TRACE_GROUP_A7DB_SELECTOR0, tile_low, attributes, screen_x, screen_y);
                appended = true;
            }
        }
        (void)appended;

        cursor = record_end + 1;
    }

    runtime->intro_trace_available = runtime->intro_trace_sprite_count > 0;
    return runtime->intro_trace_available;
}

static void append_runtime_path(char *dest, size_t dest_size, const char *root, const char *relative)
{
    size_t root_len;
    if (dest == NULL || dest_size == 0) {
        return;
    }
    dest[0] = '\0';
    if (root == NULL || root[0] == '\0') {
        root = ".";
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

void tecmo_intro_trace_load(TecmoRuntime *runtime, const char *project_root)
{
    const char *allow_loose_trace;
    char project_trace_path[TECMO_MAX_PATH_TEXT];
    char cwd_trace_path[TECMO_MAX_PATH_TEXT];
    const char *trace_paths[4];
    char *json = NULL;
    size_t json_size = 0;

    runtime->intro_trace_available = false;
    runtime->intro_trace_chr_bank = 31U;
    runtime->intro_trace_sprite_count = 0;
    runtime->intro_trace_truncated = false;
    runtime->intro_l88e7_irq_vector_available = false;
    runtime->intro_presents_data_available = false;
    runtime->intro_l88e7_irq_vector[0] = '\0';
    runtime->intro_presents_data_cpu[0] = '\0';
    set_intro_trace_status(runtime->intro_trace_status,
                           sizeof(runtime->intro_trace_status),
                           "LOOSE INTRO TRACE DISABLED");

    allow_loose_trace = getenv("TECMO_ALLOW_LOOSE_INTRO_TRACE");
    if (allow_loose_trace == NULL || strcmp(allow_loose_trace, "1") != 0) {
        return;
    }
    set_intro_trace_status(runtime->intro_trace_status,
                           sizeof(runtime->intro_trace_status),
                           "LOOSE INTRO TRACE ENABLED BUT UNAVAILABLE");

    append_runtime_path(project_trace_path,
                        sizeof(project_trace_path),
                        project_root,
                        "build\\intro_composite_trace.json");
    (void)snprintf(cwd_trace_path, sizeof(cwd_trace_path), "build\\intro_composite_trace.json");
    trace_paths[0] = project_trace_path;
    trace_paths[1] = cwd_trace_path;
    trace_paths[2] = "intro_composite_trace.json";
    trace_paths[3] = "..\\build\\intro_composite_trace.json";

    for (size_t i = 0; i < sizeof(trace_paths) / sizeof(trace_paths[0]); ++i) {
        json = read_text_file(trace_paths[i], &json_size);
        if (json != NULL) {
            break;
        }
    }

    if (json == NULL) {
        return;
    }

    if (load_intro_trace_from_json(runtime, json, json_size)) {
        (void)snprintf(runtime->intro_trace_status,
                       sizeof(runtime->intro_trace_status),
                       "LOCAL TRACE %u SPRITES R%u S0%u A%u L%u",
                       (unsigned)runtime->intro_trace_sprite_count,
                       (unsigned)tecmo_intro_trace_group_count(runtime, INTRO_TRACE_GROUP_RABBIT),
                       (unsigned)tecmo_intro_trace_group_count(runtime, INTRO_TRACE_GROUP_A7DB_SELECTOR0),
                       (unsigned)tecmo_intro_trace_group_count(runtime, INTRO_TRACE_GROUP_TECMO_STREAM),
                       (unsigned)tecmo_intro_trace_group_count(runtime, INTRO_TRACE_GROUP_TECMO_LOGO));
    } else {
        set_intro_trace_status(runtime->intro_trace_status,
                               sizeof(runtime->intro_trace_status),
                               "TRACE JSON FOUND BUT NO SPRITES PARSED");
    }

    free(json);
}
