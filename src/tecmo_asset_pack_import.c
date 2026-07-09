#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_asset_pack_import.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define TECMO_IMPORT_PATH_SIZE 1024U
#define TECMO_JSON_NESTING_LIMIT 128U

typedef struct TecmoCaptureImport {
    const char *entry_id;
    const char *local_path;
    const char *source_label;
    const char *kind;
    uint32_t flags;
} TecmoCaptureImport;

typedef struct TecmoImportedCapture {
    const TecmoCaptureImport *spec;
} TecmoImportedCapture;

typedef struct TecmoJsonLineState {
    bool had_chars;
    bool started;
    bool complete;
    bool in_string;
    bool escape;
    char expected[TECMO_JSON_NESTING_LIMIT];
    size_t depth;
} TecmoJsonLineState;

typedef struct TecmoCaptureSearchRoots {
    const char *roots[2];
    size_t count;
} TecmoCaptureSearchRoots;

static const TecmoCaptureImport INTRO_CAPTURE_IMPORTS[] = {
    {
        "intro/arena/capture.ndjson",
        "build\\intro_arena_capture.ndjson",
        "build/intro_arena_capture.ndjson",
        "compact-intro-arena-capture",
        TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL,
    },
    {
        "intro/arena/emu_intro_memory_watch.ndjson",
        "build\\emu_intro_memory_watch.ndjson",
        "build/emu_intro_memory_watch.ndjson",
        "raw-intro-memory-watch",
        TECMO_ASSET_PACK_FLAG_LOCAL,
    },
    {
        "intro/arena/emu_intro_arena_irq_watch.ndjson",
        "build\\emu_intro_arena_irq_watch.ndjson",
        "build/emu_intro_arena_irq_watch.ndjson",
        "raw-intro-arena-irq-watch",
        TECMO_ASSET_PACK_FLAG_LOCAL,
    },
    {
        "intro/post-arena/emu_intro_memory_watch.ndjson",
        "build\\emu_intro_memory_watch.ndjson",
        "build/emu_intro_memory_watch.ndjson",
        "raw-intro-memory-watch",
        TECMO_ASSET_PACK_FLAG_LOCAL,
    },
    {
        "intro/post-arena/capture.ndjson",
        "build\\emu_intro_memory_watch.ndjson",
        "build/emu_intro_memory_watch.ndjson",
        "raw-post-arena-capture",
        TECMO_ASSET_PACK_FLAG_LOCAL,
    },
};

static void set_message(char *message, size_t message_size, const char *format, ...)
{
    va_list args;

    if (message == NULL || message_size == 0U) {
        return;
    }

    va_start(args, format);
    (void)vsnprintf(message, message_size, format != NULL ? format : "", args);
    va_end(args);
}

static int append_text(char *buffer,
                       size_t capacity,
                       size_t *length,
                       const char *format,
                       ...)
{
    va_list args;
    int written;
    size_t remaining;

    if (buffer == NULL || length == NULL || *length >= capacity) {
        return -1;
    }

    remaining = capacity - *length;
    va_start(args, format);
    written = vsnprintf(buffer + *length, remaining, format, args);
    va_end(args);
    if (written < 0 || (size_t)written >= remaining) {
        return -1;
    }

    *length += (size_t)written;
    return 0;
}

static int build_project_path(char *dest,
                              size_t dest_size,
                              const char *project_root,
                              const char *relative_path)
{
    int written;
    size_t root_len;

    if (dest == NULL || dest_size == 0U || relative_path == NULL || relative_path[0] == '\0') {
        return -1;
    }

    if (project_root == NULL || project_root[0] == '\0') {
        written = snprintf(dest, dest_size, "%s", relative_path);
        return written >= 0 && (size_t)written < dest_size ? 0 : -1;
    }

    written = snprintf(dest, dest_size, "%s", project_root);
    if (written < 0 || (size_t)written >= dest_size) {
        return -1;
    }

    root_len = (size_t)written;
    if (root_len > 0U &&
        dest[root_len - 1U] != '\\' &&
        dest[root_len - 1U] != '/') {
        if (root_len + 1U >= dest_size) {
            return -1;
        }
        dest[root_len++] = '\\';
        dest[root_len] = '\0';
    }

    written = snprintf(dest + root_len, dest_size - root_len, "%s", relative_path);
    return written >= 0 && (size_t)written < dest_size - root_len ? 0 : -1;
}

static int roots_match(const char *left, const char *right)
{
    if (left == NULL) {
        left = "";
    }
    if (right == NULL) {
        right = "";
    }
    return strcmp(left, right) == 0;
}

static void add_capture_search_root(TecmoCaptureSearchRoots *search_roots, const char *project_root)
{
    if (search_roots == NULL || project_root == NULL || project_root[0] == '\0') {
        return;
    }

    for (size_t i = 0U; i < search_roots->count; ++i) {
        if (roots_match(search_roots->roots[i], project_root)) {
            return;
        }
    }

    if (search_roots->count < sizeof(search_roots->roots) / sizeof(search_roots->roots[0])) {
        search_roots->roots[search_roots->count++] = project_root;
    }
}

static int process_json_line_char(TecmoJsonLineState *state, int value)
{
    if (state == NULL) {
        return -1;
    }

    if (value == '\r') {
        return state->in_string ? -1 : 0;
    }

    if (!state->started) {
        if (isspace((unsigned char)value)) {
            state->had_chars = true;
            return 0;
        }
        if (value != '{') {
            return -1;
        }
        state->had_chars = true;
        state->started = true;
        state->expected[0] = '}';
        state->depth = 1U;
        return 0;
    }

    state->had_chars = true;
    if (state->complete) {
        return isspace((unsigned char)value) ? 0 : -1;
    }

    if (state->in_string) {
        if ((unsigned char)value < 0x20U) {
            return -1;
        }
        if (state->escape) {
            state->escape = false;
        } else if (value == '\\') {
            state->escape = true;
        } else if (value == '"') {
            state->in_string = false;
        }
        return 0;
    }

    if (value == '"') {
        state->in_string = true;
    } else if (value == '{') {
        if (state->depth >= TECMO_JSON_NESTING_LIMIT) {
            return -1;
        }
        state->expected[state->depth++] = '}';
    } else if (value == '[') {
        if (state->depth >= TECMO_JSON_NESTING_LIMIT) {
            return -1;
        }
        state->expected[state->depth++] = ']';
    } else if (value == '}' || value == ']') {
        if (state->depth == 0U || state->expected[state->depth - 1U] != value) {
            return -1;
        }
        --state->depth;
        if (state->depth == 0U) {
            state->complete = true;
        }
    } else if ((unsigned char)value < 0x20U && !isspace((unsigned char)value)) {
        return -1;
    }

    return 0;
}

static int finish_json_line(const TecmoJsonLineState *state)
{
    if (state == NULL) {
        return -1;
    }
    if (!state->had_chars && !state->started) {
        return 0;
    }
    if (!state->started ||
        !state->complete ||
        state->in_string ||
        state->escape ||
        state->depth != 0) {
        return -1;
    }
    return 0;
}

static int validate_ndjson_file(const char *path,
                                const char *entry_id,
                                bool *present_out,
                                char *message,
                                size_t message_size)
{
    FILE *file;
    TecmoJsonLineState state;
    unsigned long line = 1UL;
    unsigned long record_count = 0UL;
    int value;

    if (present_out != NULL) {
        *present_out = false;
    }

    errno = 0;
    file = fopen(path, "rb");
    if (file == NULL) {
        if (errno == ENOENT) {
            return 0;
        }
        set_message(message,
                    message_size,
                    "Could not read present intro capture file for %s: %s",
                    entry_id,
                    path);
        return -1;
    }

    if (present_out != NULL) {
        *present_out = true;
    }
    memset(&state, 0, sizeof(state));

    while ((value = fgetc(file)) != EOF) {
        if (value == '\n') {
            if (finish_json_line(&state) != 0) {
                set_message(message,
                            message_size,
                            "Malformed intro capture file for %s at line %lu: %s",
                            entry_id,
                            line,
                            path);
                fclose(file);
                return -1;
            }
            if (state.started) {
                ++record_count;
            }
            memset(&state, 0, sizeof(state));
            ++line;
            continue;
        }

        if (process_json_line_char(&state, value) != 0) {
            set_message(message,
                        message_size,
                        "Malformed intro capture file for %s at line %lu: %s",
                        entry_id,
                        line,
                        path);
            fclose(file);
            return -1;
        }
    }

    if (ferror(file) != 0) {
        set_message(message,
                    message_size,
                    "Could not read present intro capture file for %s: %s",
                    entry_id,
                    path);
        fclose(file);
        return -1;
    }

    if (finish_json_line(&state) != 0) {
        set_message(message,
                    message_size,
                    "Malformed intro capture file for %s at line %lu: %s",
                    entry_id,
                    line,
                    path);
        fclose(file);
        return -1;
    }
    if (state.started) {
        ++record_count;
    }

    fclose(file);

    if (record_count == 0UL) {
        set_message(message,
                    message_size,
                    "Malformed intro capture file for %s: no NDJSON records in %s",
                    entry_id,
                    path);
        return -1;
    }

    return 0;
}

static TecmoAssetPackEntryInfo capture_entry_info(const TecmoCaptureImport *spec)
{
    TecmoAssetPackEntryInfo entry_info;

    entry_info.id = spec->entry_id;
    entry_info.type = TECMO_ASSET_PACK_TYPE_DATA;
    entry_info.bank = 0U;
    entry_info.cpu_address = 0U;
    entry_info.source_offset = 0U;
    entry_info.flags = spec->flags;
    return entry_info;
}

static int add_intro_capture_source_map(TecmoAssetPackBuilder *builder,
                                        const TecmoImportedCapture *imports,
                                        unsigned import_count,
                                        char *message,
                                        size_t message_size)
{
    char source_map[2048];
    size_t length = 0U;
    TecmoAssetPackEntryInfo entry_info;

    if (import_count == 0U) {
        return 0;
    }

    if (append_text(source_map,
                    sizeof(source_map),
                    &length,
                    "{\n"
                    "  \"format\":\"tecmo.assetpack.intro-capture-imports/1\",\n"
                    "  \"entries\":[\n") != 0) {
        set_message(message, message_size, "Could not build intro capture import map.");
        return -1;
    }

    for (unsigned i = 0U; i < import_count; ++i) {
        const TecmoCaptureImport *spec = imports[i].spec;

        if (append_text(source_map,
                        sizeof(source_map),
                        &length,
                        "    {\"id\":\"%s\",\"kind\":\"%s\",\"source\":\"%s\"}%s\n",
                        spec->entry_id,
                        spec->kind,
                        spec->source_label,
                        i + 1U < import_count ? "," : "") != 0) {
            set_message(message, message_size, "Could not build intro capture import map.");
            return -1;
        }
    }

    if (append_text(source_map,
                    sizeof(source_map),
                    &length,
                    "  ]\n"
                    "}\n") != 0) {
        set_message(message, message_size, "Could not build intro capture import map.");
        return -1;
    }

    entry_info.id = "intro/captures/source-map";
    entry_info.type = TECMO_ASSET_PACK_TYPE_META;
    entry_info.bank = 0U;
    entry_info.cpu_address = 0U;
    entry_info.source_offset = 0U;
    entry_info.flags = TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL;
    return tecmo_asset_pack_builder_add_memory(builder,
                                               &entry_info,
                                               source_map,
                                               (uint64_t)length,
                                               message,
                                               message_size);
}

int tecmo_asset_pack_import_intro_captures(const char *primary_project_root,
                                           const char *fallback_project_root,
                                           TecmoAssetPackBuilder *builder,
                                           unsigned *imported_count_out,
                                           char *message,
                                           size_t message_size)
{
    TecmoImportedCapture imported[sizeof(INTRO_CAPTURE_IMPORTS) / sizeof(INTRO_CAPTURE_IMPORTS[0])];
    TecmoCaptureSearchRoots search_roots;
    unsigned import_count = 0U;

    if (imported_count_out != NULL) {
        *imported_count_out = 0U;
    }
    if (builder == NULL) {
        set_message(message, message_size, "Asset pack builder is required for intro capture import.");
        return -1;
    }

    memset(&search_roots, 0, sizeof(search_roots));
    add_capture_search_root(&search_roots, primary_project_root);
    add_capture_search_root(&search_roots, fallback_project_root);
    if (search_roots.count == 0U) {
        add_capture_search_root(&search_roots, ".");
    }

    for (size_t i = 0U; i < sizeof(INTRO_CAPTURE_IMPORTS) / sizeof(INTRO_CAPTURE_IMPORTS[0]); ++i) {
        const TecmoCaptureImport *spec = &INTRO_CAPTURE_IMPORTS[i];
        TecmoAssetPackEntryInfo entry_info;
        char path[TECMO_IMPORT_PATH_SIZE];
        bool imported_entry = false;

        for (size_t root_index = 0U; root_index < search_roots.count; ++root_index) {
            bool present = false;

            if (build_project_path(path, sizeof(path), search_roots.roots[root_index], spec->local_path) != 0) {
                set_message(message,
                            message_size,
                            "Intro capture source path is too long for %s",
                            spec->local_path);
                return -1;
            }

            if (validate_ndjson_file(path, spec->entry_id, &present, message, message_size) != 0) {
                return -1;
            }
            if (!present) {
                continue;
            }

            entry_info = capture_entry_info(spec);
            if (tecmo_asset_pack_builder_add_file(builder,
                                                  &entry_info,
                                                  path,
                                                  message,
                                                  message_size) != 0) {
                return -1;
            }

            imported[import_count].spec = spec;
            ++import_count;
            imported_entry = true;
            break;
        }
        if (imported_entry) {
            continue;
        }
    }

    if (add_intro_capture_source_map(builder, imported, import_count, message, message_size) != 0) {
        return -1;
    }

    if (imported_count_out != NULL) {
        *imported_count_out = import_count;
    }
    return 0;
}
