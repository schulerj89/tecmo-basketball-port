#define _CRT_SECURE_NO_WARNINGS

#include "asm_inventory.h"
#include "png_writer.h"
#include "tecmo_asset_pack.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define TECMO_MKDIR(path) _mkdir(path)
#else
#include <dirent.h>
#include <sys/stat.h>
#define TECMO_MKDIR(path) mkdir(path, 0755)
#endif

#define LINE_BUFFER_SIZE 4096
#define TECMO_CHR_CONFIG_BYTES 0x40000ULL

typedef int (*FileVisitor)(const char *path, void *context);

static const char *BASELINE_FILES[] = {
    "Header.asm",
    "Main.asm",
    "Main_split_bank01.asm",
    "Variables.inc",
    "Tecmo_00.asm",
    "Tecmo_01.asm",
    "Tecmo_01_split.asm",
    "Tecmo_02.asm",
    "Tecmo_03.asm",
    "Tecmo_04.asm",
    "Tecmo_05.asm",
    "Tecmo_06.asm",
    "Tecmo_07.asm",
    "Tiles.asm",
};

static const char *PRG_BANK_FILES[] = {
    "Tecmo_00.asm",
    "Tecmo_01.asm",
    "Tecmo_02.asm",
    "Tecmo_03.asm",
    "Tecmo_04.asm",
    "Tecmo_05.asm",
    "Tecmo_06.asm",
    "Tecmo_07.asm",
};

static const char *ROSTER_FILES[] = {
    "decomp\\lifted\\bank02\\C-0176_bank02_team_roster_and_player_data_8000_8FFF.asm",
    "decomp\\lifted\\bank02\\C-0177_bank02_roster_team_player_data_9000_BFFF.asm",
};

static const char *ORIGINAL_TITLE_TEXT_FILE =
    "decomp\\lifted\\bank04\\C-0004_bank04_menu_title_text_836B_8384.asm";
static const char *BASELINE_BANK04_FILE = "build\\baseline\\Tecmo_04.asm";
static const char *BASELINE_BANK06_FILE = "build\\baseline\\Tecmo_06.asm";
static const char *BASELINE_BANK07_FILE = "build\\baseline\\Tecmo_07.asm";

static const char *TEAM_CODES[] = {
    "GOLDEN_STATE",
    "NEW_JERSEY",
    "NEW_YORK",
    "SAN_ANTONIO",
    "PHILADELPHIA",
    "SACRAMENTO",
    "MINNESOTA",
    "MILWAUKEE",
    "CHARLOTTE",
    "CLEVELAND",
    "WASHINGTON",
    "PORTLAND",
    "SEATTLE",
    "ATLANTA",
    "BOSTON",
    "CHICAGO",
    "DALLAS",
    "DENVER",
    "DETROIT",
    "HOUSTON",
    "INDIANA",
    "CLIPPERS",
    "LAKERS",
    "MIAMI",
    "ORLANDO",
    "PHOENIX",
    "UTAH",
};

static const char *INSTRUCTIONS[] = {
    "adc", "and", "asl", "bcc", "bcs", "beq", "bit", "bmi",
    "bne", "bpl", "brk", "bvc", "bvs", "clc", "cld", "cli",
    "clv", "cmp", "cpx", "cpy", "dec", "dex", "dey", "eor",
    "inc", "inx", "iny", "jmp", "jsr", "lda", "ldx", "ldy",
    "lsr", "nop", "ora", "pha", "php", "pla", "plp", "rol",
    "ror", "rti", "rts", "sbc", "sec", "sed", "sei", "sta",
    "stx", "sty", "tax", "tay", "tsx", "txa", "txs", "tya",
};

static void copy_text(char *dst, size_t dst_size, const char *src)
{
    size_t len;
    if (dst_size == 0) {
        return;
    }
    len = strlen(src);
    if (len >= dst_size) {
        len = dst_size - 1;
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static void append_path(char *dst, size_t dst_size, const char *left, const char *right)
{
    const char sep =
#ifdef _WIN32
        '\\';
#else
        '/';
#endif
    size_t left_len = strlen(left);
    if (left_len > 0 && (left[left_len - 1] == '\\' || left[left_len - 1] == '/')) {
        (void)snprintf(dst, dst_size, "%s%s", left, right);
    } else {
        (void)snprintf(dst, dst_size, "%s%c%s", left, sep, right);
    }
}

static void normalize_separators(char *path)
{
#ifdef _WIN32
    for (char *p = path; *p != '\0'; ++p) {
        if (*p == '/') {
            *p = '\\';
        }
    }
#else
    for (char *p = path; *p != '\0'; ++p) {
        if (*p == '\\') {
            *p = '/';
        }
    }
#endif
}

static int ensure_directory_recursive(const char *path)
{
    char tmp[TECMO_MAX_PATH_TEXT];
    size_t len;

    if (path == NULL || path[0] == '\0') {
        return -1;
    }

    copy_text(tmp, sizeof(tmp), path);
    normalize_separators(tmp);
    len = strlen(tmp);
    while (len > 0 && (tmp[len - 1] == '\\' || tmp[len - 1] == '/')) {
        tmp[len - 1] = '\0';
        --len;
    }

    for (char *p = tmp + 1; *p != '\0'; ++p) {
        if (*p == '\\' || *p == '/') {
            char saved = *p;
            *p = '\0';
            if (strlen(tmp) > 0) {
                (void)TECMO_MKDIR(tmp);
            }
            *p = saved;
        }
    }

    if (TECMO_MKDIR(tmp) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

static char *left_trim(char *text)
{
    while (*text != '\0' && isspace((unsigned char)*text)) {
        ++text;
    }
    return text;
}

static void right_trim(char *text)
{
    size_t len = strlen(text);
    while (len > 0 && isspace((unsigned char)text[len - 1])) {
        text[len - 1] = '\0';
        --len;
    }
}

static void strip_comment_copy(char *dst, size_t dst_size, const char *src)
{
    size_t i = 0;
    while (src[i] != '\0' && src[i] != ';' && i + 1 < dst_size) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
    right_trim(dst);
}

static int equals_ci(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

static int starts_with_ci(const char *text, const char *prefix)
{
    while (*prefix != '\0') {
        if (tolower((unsigned char)*text) != tolower((unsigned char)*prefix)) {
            return 0;
        }
        ++text;
        ++prefix;
    }
    return 1;
}

static const char *find_ci(const char *text, const char *needle)
{
    size_t needle_len = strlen(needle);
    if (needle_len == 0) {
        return text;
    }

    for (const char *p = text; *p != '\0'; ++p) {
        size_t i = 0;
        while (i < needle_len &&
               p[i] != '\0' &&
               tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) {
            ++i;
        }
        if (i == needle_len) {
            return p;
        }
    }

    return NULL;
}

static int has_extension(const char *path, const char *extension)
{
    size_t path_len = strlen(path);
    size_t ext_len = strlen(extension);
    if (ext_len > path_len) {
        return 0;
    }
    return equals_ci(path + path_len - ext_len, extension);
}

static int is_label_char(char c)
{
    return isalnum((unsigned char)c) || c == '_' || c == '.' || c == '$';
}

static int extract_label(const char *line, char *label, size_t label_size)
{
    char tmp[LINE_BUFFER_SIZE];
    char *text;
    size_t len = 0;

    strip_comment_copy(tmp, sizeof(tmp), line);
    text = left_trim(tmp);

    if (*text == '\0' || *text == '.' || isdigit((unsigned char)*text)) {
        return 0;
    }

    while (text[len] != '\0' && is_label_char(text[len])) {
        ++len;
    }

    if (len == 0 || text[len] != ':') {
        return 0;
    }

    if (len >= label_size) {
        len = label_size - 1;
    }
    memcpy(label, text, len);
    label[len] = '\0';
    return 1;
}

static const char *after_optional_label(const char *line)
{
    const char *comment = strchr(line, ';');
    const char *colon = strchr(line, ':');
    if (colon != NULL && (comment == NULL || colon < comment)) {
        return colon + 1;
    }
    return line;
}

static int is_semantic_label(const char *label)
{
    if (label[0] == 'L' && isxdigit((unsigned char)label[1]) &&
        isxdigit((unsigned char)label[2]) && isxdigit((unsigned char)label[3]) &&
        isxdigit((unsigned char)label[4])) {
        return 0;
    }
    if (label[0] == '.' || label[0] == '$') {
        return 0;
    }
    return 1;
}

static int parse_number_token(const char **cursor, uint32_t *value)
{
    const char *p = *cursor;
    uint32_t result = 0;
    int digits = 0;

    while (*p != '\0' && (isspace((unsigned char)*p) || *p == ',')) {
        ++p;
    }

    if (*p == '\0' || *p == ';') {
        *cursor = p;
        return 0;
    }

    if (*p == '$') {
        ++p;
        while (isxdigit((unsigned char)*p)) {
            char c = (char)tolower((unsigned char)*p);
            result *= 16U;
            result += (uint32_t)(isdigit((unsigned char)c) ? c - '0' : c - 'a' + 10);
            ++p;
            ++digits;
        }
    } else if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
        while (isxdigit((unsigned char)*p)) {
            char c = (char)tolower((unsigned char)*p);
            result *= 16U;
            result += (uint32_t)(isdigit((unsigned char)c) ? c - '0' : c - 'a' + 10);
            ++p;
            ++digits;
        }
    } else if (*p == '%') {
        ++p;
        while (*p == '0' || *p == '1') {
            result = (result << 1U) | (uint32_t)(*p - '0');
            ++p;
            ++digits;
        }
    } else if (*p == '\'' && p[1] != '\0') {
        result = (uint8_t)p[1];
        p += 2;
        if (*p == '\'') {
            ++p;
        }
        digits = 1;
    } else if (isdigit((unsigned char)*p)) {
        while (isdigit((unsigned char)*p)) {
            result *= 10U;
            result += (uint32_t)(*p - '0');
            ++p;
            ++digits;
        }
    } else {
        while (*p != '\0' && *p != ',' && *p != ';') {
            ++p;
        }
        *cursor = p;
        return 0;
    }

    *cursor = p;
    if (digits == 0) {
        return 0;
    }
    *value = result;
    return 1;
}

static size_t parse_byte_values(const char *line, uint8_t *values, size_t capacity)
{
    const char *directive;
    const char *p;
    size_t count = 0;

    directive = find_ci(after_optional_label(line), ".byte");
    if (directive == NULL) {
        return 0;
    }

    p = directive + 5;
    while (*p != '\0' && *p != ';') {
        uint32_t value = 0;
        const char *before = p;
        if (parse_number_token(&p, &value)) {
            if (count < capacity) {
                values[count] = (uint8_t)(value & 0xFFU);
            }
            ++count;
        } else if (p == before) {
            ++p;
        }
    }

    return count;
}

static int parse_comment_address(const char *line, uint32_t *address_out)
{
    const char *comment = strchr(line, ';');
    const char *p;
    uint32_t value = 0;
    int digits = 0;

    if (comment == NULL) {
        return -1;
    }

    p = strchr(comment, '$');
    if (p == NULL) {
        return -1;
    }
    ++p;

    while (isxdigit((unsigned char)*p) && digits < 6) {
        char c = (char)tolower((unsigned char)*p);
        value *= 16U;
        value += (uint32_t)(isdigit((unsigned char)c) ? c - '0' : c - 'a' + 10);
        ++p;
        ++digits;
    }

    if (digits < 4) {
        return -1;
    }

    *address_out = value;
    return 0;
}

static int parse_byte_values_strict(const char *line, uint8_t *values, size_t capacity, size_t *count_out)
{
    char code[LINE_BUFFER_SIZE];
    const char *directive;
    const char *p;
    size_t count = 0;

    *count_out = 0;
    strip_comment_copy(code, sizeof(code), line);
    directive = find_ci(after_optional_label(code), ".byte");
    if (directive == NULL) {
        return 0;
    }

    p = directive + 5;
    while (*p != '\0') {
        uint32_t value = 0;

        while (*p != '\0' && (isspace((unsigned char)*p) || *p == ',')) {
            ++p;
        }
        if (*p == '\0') {
            break;
        }

        if (!parse_number_token(&p, &value) || value > 0xFFU || count >= capacity) {
            return -1;
        }

        values[count++] = (uint8_t)value;
    }

    *count_out = count;
    return 0;
}

static uint64_t count_data_values(const char *line, const char *directive)
{
    const char *found;
    const char *p;
    uint64_t count = 0;

    found = find_ci(after_optional_label(line), directive);
    if (found == NULL) {
        return 0;
    }

    p = found + strlen(directive);
    while (*p != '\0' && *p != ';') {
        uint32_t ignored = 0;
        const char *before = p;
        if (parse_number_token(&p, &ignored)) {
            ++count;
        } else if (p == before) {
            ++p;
        }
    }

    return count;
}

static int first_token_after_label(const char *line, char *token, size_t token_size)
{
    char tmp[LINE_BUFFER_SIZE];
    char *text;
    size_t len = 0;

    strip_comment_copy(tmp, sizeof(tmp), after_optional_label(line));
    text = left_trim(tmp);
    if (*text == '\0') {
        return 0;
    }

    while (text[len] != '\0' && !isspace((unsigned char)text[len])) {
        ++len;
    }
    if (len == 0) {
        return 0;
    }
    if (len >= token_size) {
        len = token_size - 1;
    }
    memcpy(token, text, len);
    token[len] = '\0';
    return 1;
}

static int is_instruction_token(const char *token)
{
    for (size_t i = 0; i < sizeof(INSTRUCTIONS) / sizeof(INSTRUCTIONS[0]); ++i) {
        if (equals_ci(token, INSTRUCTIONS[i])) {
            return 1;
        }
    }
    return 0;
}

void asm_stats_add(AsmStats *dst, const AsmStats *src)
{
    dst->files += src->files;
    dst->lines += src->lines;
    dst->comments += src->comments;
    dst->labels += src->labels;
    dst->semantic_labels += src->semantic_labels;
    dst->segment_directives += src->segment_directives;
    dst->include_directives += src->include_directives;
    dst->byte_directives += src->byte_directives;
    dst->word_directives += src->word_directives;
    dst->emitted_bytes += src->emitted_bytes;
    dst->instructions += src->instructions;
}

int asm_scan_file(const char *path, AsmStats *stats)
{
    FILE *file = fopen(path, "rb");
    char line[LINE_BUFFER_SIZE];

    memset(stats, 0, sizeof(*stats));
    if (file == NULL) {
        return -1;
    }

    stats->files = 1;
    while (fgets(line, sizeof(line), file) != NULL) {
        char *text;
        char label[TECMO_MAX_LABEL_TEXT];
        char token[64];
        uint64_t byte_values;
        uint64_t word_values;

        ++stats->lines;
        text = left_trim(line);
        if (*text == ';') {
            ++stats->comments;
        }

        if (extract_label(line, label, sizeof(label))) {
            ++stats->labels;
            if (is_semantic_label(label)) {
                ++stats->semantic_labels;
            }
        }

        text = (char *)after_optional_label(line);
        text = left_trim(text);
        if (starts_with_ci(text, ".segment")) {
            ++stats->segment_directives;
        }
        if (starts_with_ci(text, ".include")) {
            ++stats->include_directives;
        }

        byte_values = count_data_values(line, ".byte");
        word_values = count_data_values(line, ".word");
        if (byte_values > 0) {
            ++stats->byte_directives;
            stats->emitted_bytes += byte_values;
        }
        if (word_values > 0) {
            ++stats->word_directives;
            stats->emitted_bytes += word_values * 2U;
        }

        if (first_token_after_label(line, token, sizeof(token)) && is_instruction_token(token)) {
            ++stats->instructions;
        }
    }

    fclose(file);
    return 0;
}

#ifdef _WIN32
static int visit_files_recursive(const char *dir, const char *extension, FileVisitor visitor, void *context)
{
    char pattern[TECMO_MAX_PATH_TEXT];
    WIN32_FIND_DATAA data;
    HANDLE handle;

    append_path(pattern, sizeof(pattern), dir, "*");
    handle = FindFirstFileA(pattern, &data);
    if (handle == INVALID_HANDLE_VALUE) {
        return -1;
    }

    do {
        char child[TECMO_MAX_PATH_TEXT];
        if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0) {
            continue;
        }
        append_path(child, sizeof(child), dir, data.cFileName);
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            (void)visit_files_recursive(child, extension, visitor, context);
        } else if (extension == NULL || has_extension(child, extension)) {
            if (visitor(child, context) != 0) {
                FindClose(handle);
                return -1;
            }
        }
    } while (FindNextFileA(handle, &data) != 0);

    FindClose(handle);
    return 0;
}
#else
static int visit_files_recursive(const char *dir, const char *extension, FileVisitor visitor, void *context)
{
    DIR *d = opendir(dir);
    struct dirent *entry;
    if (d == NULL) {
        return -1;
    }
    while ((entry = readdir(d)) != NULL) {
        char child[TECMO_MAX_PATH_TEXT];
        struct stat st;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        append_path(child, sizeof(child), dir, entry->d_name);
        if (stat(child, &st) != 0) {
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            (void)visit_files_recursive(child, extension, visitor, context);
        } else if (extension == NULL || has_extension(child, extension)) {
            if (visitor(child, context) != 0) {
                closedir(d);
                return -1;
            }
        }
    }
    closedir(d);
    return 0;
}
#endif

typedef struct ScanTreeContext {
    AsmStats stats;
} ScanTreeContext;

static int scan_tree_file_visitor(const char *path, void *context)
{
    ScanTreeContext *ctx = (ScanTreeContext *)context;
    AsmStats file_stats;
    if (asm_scan_file(path, &file_stats) == 0) {
        asm_stats_add(&ctx->stats, &file_stats);
    }
    return 0;
}

int asm_scan_tree(const char *root, const char *relative_dir, const char *extension, AsmStats *stats)
{
    char path[TECMO_MAX_PATH_TEXT];
    ScanTreeContext context;

    memset(&context, 0, sizeof(context));
    append_path(path, sizeof(path), root, relative_dir);
    normalize_separators(path);
    if (visit_files_recursive(path, extension, scan_tree_file_visitor, &context) != 0) {
        memset(stats, 0, sizeof(*stats));
        return -1;
    }

    *stats = context.stats;
    return 0;
}

typedef struct TextBuffer {
    char *data;
    size_t length;
    size_t capacity;
} TextBuffer;

typedef enum AssetPackEntryStatus {
    ASSET_PACK_ENTRY_ERROR = -1,
    ASSET_PACK_ENTRY_ABSENT = 0,
    ASSET_PACK_ENTRY_LOADED = 1,
} AssetPackEntryStatus;

#define TECMO_LOCAL_ASSET_PACK_VERSION 1U
#define TECMO_LOCAL_ASSET_PACK_HEADER_SIZE 40U
#define TECMO_LOCAL_ASSET_PACK_ENTRY_SIZE 128U

static void set_inventory_message(char *message, size_t message_size, const char *format, ...)
{
    va_list args;

    if (message == NULL || message_size == 0U) {
        return;
    }
    if (format == NULL) {
        message[0] = '\0';
        return;
    }

    va_start(args, format);
    (void)vsnprintf(message, message_size, format, args);
    va_end(args);
}

static int text_buffer_reserve(TextBuffer *buffer, size_t additional)
{
    size_t needed;
    size_t new_capacity;
    char *new_data;

    if (buffer->length > SIZE_MAX - 1U || additional > SIZE_MAX - buffer->length - 1U) {
        return -1;
    }
    needed = buffer->length + additional + 1U;
    if (needed <= buffer->capacity) {
        return 0;
    }

    new_capacity = buffer->capacity == 0U ? 256U : buffer->capacity;
    while (new_capacity < needed) {
        if (new_capacity > SIZE_MAX / 2U) {
            new_capacity = needed;
            break;
        }
        new_capacity *= 2U;
    }

    new_data = (char *)realloc(buffer->data, new_capacity);
    if (new_data == NULL) {
        return -1;
    }
    buffer->data = new_data;
    buffer->capacity = new_capacity;
    return 0;
}

static int text_buffer_append_n(TextBuffer *buffer, const char *text, size_t length)
{
    if (text_buffer_reserve(buffer, length) != 0) {
        return -1;
    }
    if (length > 0U) {
        memcpy(buffer->data + buffer->length, text, length);
    }
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
    return 0;
}

static int text_buffer_append(TextBuffer *buffer, const char *text)
{
    return text_buffer_append_n(buffer, text, strlen(text));
}

static int text_buffer_append_char(TextBuffer *buffer, char c)
{
    return text_buffer_append_n(buffer, &c, 1U);
}

static int text_buffer_appendf(TextBuffer *buffer, const char *format, ...)
{
    va_list args;
    va_list args_copy;
    int needed;
    size_t length;

    va_start(args, format);
    va_copy(args_copy, args);
    needed = vsnprintf(NULL, 0U, format, args);
    va_end(args);
    if (needed < 0) {
        va_end(args_copy);
        return -1;
    }

    length = (size_t)needed;
    if (text_buffer_reserve(buffer, length) != 0) {
        va_end(args_copy);
        return -1;
    }
    if (vsnprintf(buffer->data + buffer->length, buffer->capacity - buffer->length, format, args_copy) != needed) {
        va_end(args_copy);
        return -1;
    }
    va_end(args_copy);
    buffer->length += length;
    return 0;
}

static int text_buffer_take(TextBuffer *buffer, char **text_out, uint64_t *byte_count)
{
    if (buffer->data == NULL) {
        buffer->data = (char *)malloc(1U);
        if (buffer->data == NULL) {
            return -1;
        }
        buffer->data[0] = '\0';
        buffer->capacity = 1U;
    }
    *text_out = buffer->data;
    *byte_count = (uint64_t)buffer->length;
    buffer->data = NULL;
    buffer->length = 0U;
    buffer->capacity = 0U;
    return 0;
}

static uint32_t read_le_u32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0]) |
           ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) |
           ((uint32_t)bytes[3] << 24U);
}

static uint64_t read_le_u64(const uint8_t *bytes)
{
    uint64_t value = 0U;

    for (size_t i = 0; i < 8U; ++i) {
        value |= (uint64_t)bytes[i] << (unsigned)(i * 8U);
    }
    return value;
}

static AssetPackEntryStatus load_asset_pack_entry_from_path(const char *pack_path,
                                                            const char *entry_id,
                                                            uint8_t **bytes_out,
                                                            uint64_t *byte_count)
{
    FILE *file;
    uint8_t header[TECMO_LOCAL_ASSET_PACK_HEADER_SIZE];
    uint32_t version;
    uint32_t header_size;
    uint32_t entry_size;
    uint32_t entry_count;
    uint64_t directory_offset;
    AssetPackEntryStatus status = ASSET_PACK_ENTRY_ABSENT;

    if (pack_path == NULL || pack_path[0] == '\0') {
        return ASSET_PACK_ENTRY_ABSENT;
    }

    file = fopen(pack_path, "rb");
    if (file == NULL) {
        return ASSET_PACK_ENTRY_ABSENT;
    }

    if (fread(header, 1U, sizeof(header), file) != sizeof(header) ||
        memcmp(header, "TAP1", 4U) != 0) {
        status = ASSET_PACK_ENTRY_ERROR;
        goto cleanup;
    }

    version = read_le_u32(header + 4U);
    header_size = read_le_u32(header + 8U);
    entry_size = read_le_u32(header + 12U);
    entry_count = read_le_u32(header + 16U);
    directory_offset = read_le_u64(header + 20U);

    if (version != TECMO_LOCAL_ASSET_PACK_VERSION ||
        header_size != TECMO_LOCAL_ASSET_PACK_HEADER_SIZE ||
        entry_size != TECMO_LOCAL_ASSET_PACK_ENTRY_SIZE ||
        directory_offset > (uint64_t)LONG_MAX ||
        fseek(file, (long)directory_offset, SEEK_SET) != 0) {
        status = ASSET_PACK_ENTRY_ERROR;
        goto cleanup;
    }

    for (uint32_t i = 0; i < entry_count; ++i) {
        uint8_t entry[TECMO_LOCAL_ASSET_PACK_ENTRY_SIZE];

        if (fread(entry, 1U, sizeof(entry), file) != sizeof(entry)) {
            status = ASSET_PACK_ENTRY_ERROR;
            goto cleanup;
        }
        if (strncmp((const char *)entry, entry_id, TECMO_ASSET_PACK_ID_SIZE) == 0) {
            fclose(file);
            if (tecmo_asset_pack_read_entry(pack_path, entry_id, bytes_out, byte_count) != 0) {
                *bytes_out = NULL;
                *byte_count = 0U;
                return ASSET_PACK_ENTRY_ERROR;
            }
            return ASSET_PACK_ENTRY_LOADED;
        }
    }

cleanup:
    fclose(file);
    return status;
}

static AssetPackEntryStatus load_asset_pack_entry(const char *project_root,
                                                  const char *entry_id,
                                                  uint8_t **bytes_out,
                                                  uint64_t *byte_count)
{
    const char *env_path = getenv("TECMO_ASSETPACK");
    char path[TECMO_MAX_PATH_TEXT];
    AssetPackEntryStatus status;

    if (bytes_out == NULL || byte_count == NULL) {
        return ASSET_PACK_ENTRY_ERROR;
    }
    *bytes_out = NULL;
    *byte_count = 0U;

    status = load_asset_pack_entry_from_path(env_path, entry_id, bytes_out, byte_count);
    if (status != ASSET_PACK_ENTRY_ABSENT) {
        return status;
    }

    if (project_root != NULL && project_root[0] != '\0') {
        append_path(path, sizeof(path), project_root, "build\\tecmo.assetpack");
        normalize_separators(path);
        status = load_asset_pack_entry_from_path(path, entry_id, bytes_out, byte_count);
        if (status != ASSET_PACK_ENTRY_ABSENT) {
            return status;
        }
    }

    return load_asset_pack_entry_from_path("build\\tecmo.assetpack", entry_id, bytes_out, byte_count);
}

static int copy_asset_text_payload(const uint8_t *bytes, uint64_t byte_count, char **text_out)
{
    char *text;
    size_t size;

    if ((bytes == NULL && byte_count > 0U) || text_out == NULL || byte_count > (uint64_t)(SIZE_MAX - 1U)) {
        return -1;
    }

    size = (size_t)byte_count;
    if (size > 0U && memchr(bytes, '\0', size) != NULL) {
        return -1;
    }

    text = (char *)malloc(size + 1U);
    if (text == NULL) {
        return -1;
    }
    if (size > 0U) {
        memcpy(text, bytes, size);
    }
    text[size] = '\0';
    *text_out = text;
    return 0;
}

static void strip_line_cr(char *line)
{
    size_t len = strlen(line);
    if (len > 0U && line[len - 1U] == '\r') {
        line[len - 1U] = '\0';
    }
}

static int tsv_field_is_blank(const char *field)
{
    for (const unsigned char *p = (const unsigned char *)field; *p != '\0'; ++p) {
        if (!isspace(*p)) {
            return 0;
        }
    }
    return 1;
}

static int copy_tsv_text_field(char *dst, size_t dst_size, const char *field)
{
    size_t len;

    if (dst == NULL || dst_size == 0U || field == NULL) {
        return -1;
    }

    len = strlen(field);
    if (len >= dst_size) {
        return -1;
    }
    for (const unsigned char *p = (const unsigned char *)field; *p != '\0'; ++p) {
        if (*p < 0x20U || *p > 0x7EU) {
            return -1;
        }
    }

    memcpy(dst, field, len + 1U);
    return 0;
}

static int parse_tsv_uint_field(const char *field, uint32_t max_value, uint32_t *value_out)
{
    char tmp[64];
    char *text;
    char *end;
    int base = 10;
    unsigned long value;

    if (field == NULL || value_out == NULL || strlen(field) >= sizeof(tmp)) {
        return -1;
    }
    copy_text(tmp, sizeof(tmp), field);
    text = left_trim(tmp);
    right_trim(text);
    if (text[0] == '\0') {
        return -1;
    }
    if (text[0] == '+' || text[0] == '-') {
        return -1;
    }
    if (text[0] == '$') {
        ++text;
        base = 16;
    } else if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
    }

    errno = 0;
    value = strtoul(text, &end, base);
    if (errno != 0 || end == text || *end != '\0' || value > (unsigned long)max_value) {
        return -1;
    }

    *value_out = (uint32_t)value;
    return 0;
}

static size_t split_tsv_line(char *line, char **fields, size_t max_fields)
{
    char *cursor = line;
    size_t count = 0U;

    for (;;) {
        char *tab;
        if (count >= max_fields) {
            return 0U;
        }
        fields[count++] = cursor;
        tab = strchr(cursor, '\t');
        if (tab == NULL) {
            break;
        }
        *tab = '\0';
        cursor = tab + 1;
    }
    return count;
}

static int append_tsv_text_field(TextBuffer *buffer, const char *text)
{
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; ++p) {
        char out = '?';
        if (*p == '\t' || *p == '\r' || *p == '\n') {
            out = ' ';
        } else if (*p >= 0x20U && *p <= 0x7EU) {
            out = (char)*p;
        }
        if (text_buffer_append_char(buffer, out) != 0) {
            return -1;
        }
    }
    return 0;
}

static int roster_table_push(RosterTable *table, const RosterRecord *record)
{
    RosterRecord *new_records;
    size_t new_capacity;

    if (table->count < table->capacity) {
        table->records[table->count++] = *record;
        return 0;
    }

    new_capacity = table->capacity == 0 ? 64 : table->capacity * 2;
    new_records = (RosterRecord *)realloc(table->records, new_capacity * sizeof(RosterRecord));
    if (new_records == NULL) {
        return -1;
    }

    table->records = new_records;
    table->capacity = new_capacity;
    table->records[table->count++] = *record;
    return 0;
}

static int roster_table_find_label(const RosterTable *table, const char *label)
{
    for (size_t i = table->count; i > 0; --i) {
        if (strcmp(table->records[i - 1].label, label) == 0) {
            return (int)(i - 1);
        }
    }
    return -1;
}

void roster_table_free(RosterTable *table)
{
    free(table->records);
    table->records = NULL;
    table->count = 0;
    table->capacity = 0;
}

static void label_part_to_display(char *dst, size_t dst_size, const char *src)
{
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j + 1 < dst_size; ++i) {
        dst[j++] = src[i] == '_' ? ' ' : src[i];
    }
    dst[j] = '\0';
}

static int parse_player_label(const char *label, char *team, size_t team_size, char *player, size_t player_size)
{
    const char *payload;
    if (!starts_with_ci(label, "player_")) {
        return 0;
    }
    if (find_ci(label, "_roster_data") != NULL) {
        return 0;
    }

    payload = label + 7;
    for (size_t i = 0; i < sizeof(TEAM_CODES) / sizeof(TEAM_CODES[0]); ++i) {
        size_t len = strlen(TEAM_CODES[i]);
        if (strncmp(payload, TEAM_CODES[i], len) == 0 && payload[len] == '_') {
            label_part_to_display(team, team_size, TEAM_CODES[i]);
            label_part_to_display(player, player_size, payload + len + 1);
            return 1;
        }
    }

    return 0;
}

static int parse_quoted_comment_name(const char *line, char *name, size_t name_size)
{
    const char *quote = strchr(line, '"');
    const char *end;
    size_t len;
    if (quote == NULL) {
        return 0;
    }
    ++quote;
    end = strchr(quote, '"');
    if (end == NULL || end <= quote) {
        return 0;
    }
    len = (size_t)(end - quote);
    if (len >= name_size) {
        len = name_size - 1;
    }
    memcpy(name, quote, len);
    name[len] = '\0';
    return 1;
}

static void roster_base_from_data_label(const char *label, char *base, size_t base_size)
{
    const char suffix[] = "_roster_data";
    size_t label_len = strlen(label);
    size_t suffix_len = strlen(suffix);
    size_t len = label_len;

    if (label_len >= suffix_len && strcmp(label + label_len - suffix_len, suffix) == 0) {
        len = label_len - suffix_len;
    }
    if (len >= base_size) {
        len = base_size - 1;
    }
    memcpy(base, label, len);
    base[len] = '\0';
}

static int parse_roster_file(const char *path, RosterTable *table)
{
    FILE *file = fopen(path, "rb");
    char line[LINE_BUFFER_SIZE];
    int current_index = -1;
    int pending_attrs_index = -1;

    if (file == NULL) {
        return -1;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char label[TECMO_MAX_LABEL_TEXT];

        if (extract_label(line, label, sizeof(label))) {
            char team[TECMO_MAX_NAME_TEXT];
            char player[TECMO_MAX_NAME_TEXT];

            if (find_ci(label, "_roster_data") != NULL) {
                char pending_base[TECMO_MAX_LABEL_TEXT];
                roster_base_from_data_label(label, pending_base, sizeof(pending_base));
                pending_attrs_index = roster_table_find_label(table, pending_base);
                current_index = -1;
                continue;
            }

            if (parse_player_label(label, team, sizeof(team), player, sizeof(player))) {
                RosterRecord record;
                memset(&record, 0, sizeof(record));
                copy_text(record.team, sizeof(record.team), team);
                copy_text(record.player, sizeof(record.player), player);
                copy_text(record.label, sizeof(record.label), label);
                if (roster_table_push(table, &record) == 0) {
                    current_index = (int)(table->count - 1);
                } else {
                    current_index = -1;
                }
                pending_attrs_index = -1;
            }
        }

        if (find_ci(line, ".byte") != NULL) {
            char quoted[TECMO_MAX_NAME_TEXT];
            uint8_t bytes[64];
            size_t count;

            if (current_index >= 0 && parse_quoted_comment_name(line, quoted, sizeof(quoted))) {
                copy_text(table->records[current_index].player, sizeof(table->records[current_index].player), quoted);
            }

            if (pending_attrs_index >= 0) {
                count = parse_byte_values(line, bytes, sizeof(bytes));
                if (count >= 7) {
                    for (size_t i = 0; i < 7; ++i) {
                        table->records[pending_attrs_index].attrs[i] = bytes[i];
                    }
                    table->records[pending_attrs_index].has_attrs = true;
                    pending_attrs_index = -1;
                }
            }
        }
    }

    fclose(file);
    return 0;
}

/*
 * Asset-pack roster entry format: roster/table.tsv
 *
 * The serializer writes one optional header row followed by:
 *   team<TAB>player<TAB>label<TAB>attr0<TAB>...<TAB>attr6
 *
 * Text fields are printable ASCII with tabs/newlines sanitized to spaces.
 * Attribute fields are byte values accepted as decimal, 0xNN, or $NN. If all
 * seven attribute fields are blank the row is retained with has_attrs=false.
 * The parser also accepts a compact 9-column form without the label column.
 */
static int parse_roster_tsv_line(char **fields, size_t field_count, RosterTable *table)
{
    RosterRecord record;
    size_t attr_start;
    int attrs_present = 0;

    if (field_count >= 2U && equals_ci(fields[0], "team") && equals_ci(fields[1], "player")) {
        return 0;
    }

    if (field_count == 10U) {
        attr_start = 3U;
    } else if (field_count == 9U) {
        attr_start = 2U;
    } else if (field_count == 3U || field_count == 2U) {
        attr_start = field_count;
    } else {
        return -1;
    }

    if (tsv_field_is_blank(fields[0]) || tsv_field_is_blank(fields[1])) {
        return -1;
    }

    memset(&record, 0, sizeof(record));
    if (copy_tsv_text_field(record.team, sizeof(record.team), fields[0]) != 0 ||
        copy_tsv_text_field(record.player, sizeof(record.player), fields[1]) != 0) {
        return -1;
    }
    if ((field_count == 10U || field_count == 3U) && !tsv_field_is_blank(fields[2])) {
        if (copy_tsv_text_field(record.label, sizeof(record.label), fields[2]) != 0) {
            return -1;
        }
    } else {
        (void)snprintf(record.label,
                       sizeof(record.label),
                       "pack_roster_%03llu",
                       (unsigned long long)table->count);
    }

    if (field_count == 9U || field_count == 10U) {
        for (size_t i = 0; i < 7U; ++i) {
            if (!tsv_field_is_blank(fields[attr_start + i])) {
                attrs_present = 1;
            }
        }
        if (attrs_present) {
            for (size_t i = 0; i < 7U; ++i) {
                uint32_t value = 0U;
                if (parse_tsv_uint_field(fields[attr_start + i], 0xFFU, &value) != 0) {
                    return -1;
                }
                record.attrs[i] = (uint8_t)value;
            }
            record.has_attrs = true;
        }
    }

    return roster_table_push(table, &record);
}

static int load_roster_table_from_tsv_bytes(const uint8_t *bytes, uint64_t byte_count, RosterTable *table)
{
    char *text = NULL;
    char *line;
    RosterTable parsed;
    int result = -1;

    memset(&parsed, 0, sizeof(parsed));
    if (copy_asset_text_payload(bytes, byte_count, &text) != 0) {
        return -1;
    }

    line = text;
    while (line != NULL) {
        char *next = strchr(line, '\n');
        char *trimmed;
        char *fields[12];
        size_t field_count;

        if (next != NULL) {
            *next = '\0';
            ++next;
        }
        strip_line_cr(line);
        trimmed = left_trim(line);
        if (trimmed[0] != '\0' && trimmed[0] != '#') {
            field_count = split_tsv_line(trimmed, fields, sizeof(fields) / sizeof(fields[0]));
            if (field_count == 0U || parse_roster_tsv_line(fields, field_count, &parsed) != 0) {
                goto cleanup;
            }
        }
        line = next;
    }

    if (parsed.count == 0U) {
        goto cleanup;
    }

    *table = parsed;
    memset(&parsed, 0, sizeof(parsed));
    result = 0;

cleanup:
    roster_table_free(&parsed);
    free(text);
    return result;
}

static AssetPackEntryStatus load_roster_table_from_asset_pack(const char *project_root, RosterTable *table)
{
    uint8_t *entry_bytes = NULL;
    uint64_t entry_size = 0U;
    AssetPackEntryStatus status;

    status = load_asset_pack_entry(project_root, "roster/table.tsv", &entry_bytes, &entry_size);
    if (status == ASSET_PACK_ENTRY_LOADED) {
        status = load_roster_table_from_tsv_bytes(entry_bytes, entry_size, table) == 0
            ? ASSET_PACK_ENTRY_LOADED
            : ASSET_PACK_ENTRY_ERROR;
        tecmo_asset_pack_free(entry_bytes);
    }

    return status;
}

static int collect_rosters_from_decomp_sources(const char *project_root, RosterTable *table)
{
    if (project_root == NULL || table == NULL) {
        return -1;
    }

    memset(table, 0, sizeof(*table));
    for (size_t i = 0; i < sizeof(ROSTER_FILES) / sizeof(ROSTER_FILES[0]); ++i) {
        char path[TECMO_MAX_PATH_TEXT];
        append_path(path, sizeof(path), project_root, ROSTER_FILES[i]);
        normalize_separators(path);
        (void)parse_roster_file(path, table);
    }
    return table->count > 0 ? 0 : -1;
}

int tecmo_serialize_roster_table_tsv(const RosterTable *table, char **text_out, uint64_t *byte_count)
{
    TextBuffer buffer;
    int result = -1;

    if (table == NULL || text_out == NULL || byte_count == NULL) {
        return -1;
    }
    *text_out = NULL;
    *byte_count = 0U;
    memset(&buffer, 0, sizeof(buffer));

    if (text_buffer_append(&buffer, "team\tplayer\tlabel\tattr0\tattr1\tattr2\tattr3\tattr4\tattr5\tattr6\n") != 0) {
        goto cleanup;
    }

    for (size_t i = 0; i < table->count; ++i) {
        const RosterRecord *record = &table->records[i];

        if (append_tsv_text_field(&buffer, record->team) != 0 ||
            text_buffer_append_char(&buffer, '\t') != 0 ||
            append_tsv_text_field(&buffer, record->player) != 0 ||
            text_buffer_append_char(&buffer, '\t') != 0 ||
            append_tsv_text_field(&buffer, record->label) != 0) {
            goto cleanup;
        }

        for (size_t attr = 0; attr < 7U; ++attr) {
            if (text_buffer_append_char(&buffer, '\t') != 0) {
                goto cleanup;
            }
            if (record->has_attrs &&
                text_buffer_appendf(&buffer, "0x%02X", (unsigned)record->attrs[attr]) != 0) {
                goto cleanup;
            }
        }
        if (text_buffer_append_char(&buffer, '\n') != 0) {
            goto cleanup;
        }
    }

    result = text_buffer_take(&buffer, text_out, byte_count);

cleanup:
    free(buffer.data);
    return result;
}

int tecmo_collect_rosters(const char *project_root, RosterTable *table)
{
    AssetPackEntryStatus pack_status;

    if (table == NULL) {
        return -1;
    }
    memset(table, 0, sizeof(*table));
    pack_status = load_roster_table_from_asset_pack(project_root, table);
    if (pack_status == ASSET_PACK_ENTRY_LOADED) {
        return 0;
    }
    if (pack_status == ASSET_PACK_ENTRY_ERROR) {
        return -1;
    }

    return collect_rosters_from_decomp_sources(project_root, table);
}

static void write_c_string(FILE *file, const char *text)
{
    fputc('"', file);
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; ++p) {
        if (*p == '\\' || *p == '"') {
            fputc('\\', file);
            fputc((int)*p, file);
        } else if (*p == '\n') {
            fputs("\\n", file);
        } else if (*p == '\r') {
            fputs("\\r", file);
        } else if (*p == '\t') {
            fputs("\\t", file);
        } else if (*p >= 0x20U && *p <= 0x7EU) {
            fputc((int)*p, file);
        } else {
            fprintf(file, "\\x%02X", (unsigned)*p);
        }
    }
    fputc('"', file);
}

int tecmo_generate_roster_c(const char *project_root, const char *out_dir)
{
    RosterTable table;
    char header_path[TECMO_MAX_PATH_TEXT];
    char source_path[TECMO_MAX_PATH_TEXT];
    FILE *header;
    FILE *source;

    if (tecmo_collect_rosters(project_root, &table) != 0) {
        return -1;
    }

    if (ensure_directory_recursive(out_dir) != 0) {
        roster_table_free(&table);
        return -1;
    }

    append_path(header_path, sizeof(header_path), out_dir, "tecmo_rosters.h");
    append_path(source_path, sizeof(source_path), out_dir, "tecmo_rosters.c");
    normalize_separators(header_path);
    normalize_separators(source_path);

    header = fopen(header_path, "wb");
    if (header == NULL) {
        roster_table_free(&table);
        return -1;
    }

    fprintf(header,
            "#ifndef TECMO_GENERATED_ROSTERS_H\n"
            "#define TECMO_GENERATED_ROSTERS_H\n\n"
            "#include <stddef.h>\n"
            "#include <stdint.h>\n\n"
            "typedef struct TecmoRosterRecord {\n"
            "    const char *team;\n"
            "    const char *player;\n"
            "    uint8_t player_id;\n"
            "    uint8_t number_bcd;\n"
            "    uint8_t height_bcd;\n"
            "    uint8_t weight_raw;\n"
            "    uint8_t fg_raw;\n"
            "    uint8_t ft_raw;\n"
            "    uint8_t three_pt_raw;\n"
            "} TecmoRosterRecord;\n\n"
            "extern const TecmoRosterRecord TECMO_ROSTER_RECORDS[];\n"
            "extern const size_t TECMO_ROSTER_RECORDS_COUNT;\n\n"
            "#endif\n");
    fclose(header);

    source = fopen(source_path, "wb");
    if (source == NULL) {
        roster_table_free(&table);
        return -1;
    }

    fprintf(source,
            "#include \"tecmo_rosters.h\"\n\n"
            "const TecmoRosterRecord TECMO_ROSTER_RECORDS[] = {\n");
    for (size_t i = 0; i < table.count; ++i) {
        const RosterRecord *record = &table.records[i];
        fprintf(source, "    {");
        write_c_string(source, record->team);
        fprintf(source, ", ");
        write_c_string(source, record->player);
        fprintf(source, ", 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X},\n",
                (unsigned)record->attrs[0],
                (unsigned)record->attrs[1],
                (unsigned)record->attrs[2],
                (unsigned)record->attrs[3],
                (unsigned)record->attrs[4],
                (unsigned)record->attrs[5],
                (unsigned)record->attrs[6]);
    }
    fprintf(source,
            "};\n\n"
            "const size_t TECMO_ROSTER_RECORDS_COUNT = sizeof(TECMO_ROSTER_RECORDS) / sizeof(TECMO_ROSTER_RECORDS[0]);\n");
    fclose(source);

    roster_table_free(&table);
    return 0;
}

static uint16_t title_ppu_address_for_index(size_t index, uint8_t *render_x_out);

static void title_glyphs_set_pack_defaults(TecmoOriginalTitleGlyphs *glyphs)
{
    glyphs->dispatcher_call_index = 0x38U;
    glyphs->dispatcher_bank = 0x06U;
    glyphs->dispatcher_target = 0x9E50U;
    glyphs->dispatcher_matches_expected = true;
    glyphs->chr_config_0100 = 0x06U;
    glyphs->setup_selector_0352 = 0x1FU;
    glyphs->ba16_update_flags_or_05b6 = 0x01U;
    glyphs->ba16_update_flag_modeled = true;
}

/*
 * Asset-pack title text entry format: title/original-text.txt
 *
 * The payload is a single printable ASCII line. A trailing CR/LF is ignored.
 * This keeps the entry hand-editable while matching the lifted ASM text bytes.
 */
static int parse_title_text_payload(const uint8_t *bytes, uint64_t byte_count, char *title, size_t title_size)
{
    size_t start = 0U;
    size_t end;
    size_t size;

    if (title == NULL || title_size == 0U || (bytes == NULL && byte_count > 0U) || byte_count > (uint64_t)SIZE_MAX) {
        return -1;
    }
    title[0] = '\0';
    size = (size_t)byte_count;

    if (size >= 3U && bytes[0] == 0xEFU && bytes[1] == 0xBBU && bytes[2] == 0xBFU) {
        start = 3U;
    }
    end = size;
    while (end > start && (bytes[end - 1U] == '\n' || bytes[end - 1U] == '\r')) {
        --end;
    }
    if (end == start || end - start >= title_size) {
        return -1;
    }

    for (size_t i = start; i < end; ++i) {
        uint8_t value = bytes[i];
        if (value < 0x20U || value > 0x7EU) {
            title[0] = '\0';
            return -1;
        }
        title[i - start] = (char)value;
    }
    title[end - start] = '\0';
    return 0;
}

static AssetPackEntryStatus load_title_text_from_asset_pack(const char *project_root, char *title, size_t title_size)
{
    uint8_t *entry_bytes = NULL;
    uint64_t entry_size = 0U;
    AssetPackEntryStatus status;

    status = load_asset_pack_entry(project_root, "title/original-text.txt", &entry_bytes, &entry_size);
    if (status == ASSET_PACK_ENTRY_LOADED) {
        status = parse_title_text_payload(entry_bytes, entry_size, title, title_size) == 0
            ? ASSET_PACK_ENTRY_LOADED
            : ASSET_PACK_ENTRY_ERROR;
        tecmo_asset_pack_free(entry_bytes);
    }

    return status;
}

/*
 * Asset-pack title glyph-map entry format: title/glyph-map.tsv
 *
 * The serializer writes:
 *   index<TAB>char_code<TAB>char<TAB>render_x<TAB>ppu_address<TAB>tile_index<TAB>glyph0<TAB>...<TAB>glyph3
 *
 * char_code is the authoritative printable ASCII byte. The char column is a
 * readability aid, so spaces are represented as a literal space field. Numeric
 * fields accept decimal, 0xNN, or $NN. Rows must be ordered by zero-based index.
 */
static int parse_title_glyph_tsv_line(char **fields, size_t field_count, TecmoOriginalTitleGlyphs *glyphs)
{
    uint32_t index = 0U;
    uint32_t char_code = 0U;
    uint32_t render_x = 0U;
    uint32_t ppu_address = 0U;
    uint32_t tile_index = 0U;
    TecmoTitleGlyph *glyph;

    if (field_count >= 1U && equals_ci(fields[0], "index")) {
        return 0;
    }
    if (field_count != 10U) {
        return -1;
    }
    if (parse_tsv_uint_field(fields[0], TECMO_TITLE_MAX_CHARS, &index) != 0 ||
        index != glyphs->glyph_count ||
        index >= TECMO_TITLE_MAX_CHARS ||
        index + 1U >= sizeof(glyphs->title_text)) {
        return -1;
    }

    if (parse_tsv_uint_field(fields[1], 0x7EU, &char_code) != 0) {
        if (strlen(fields[2]) != 1U) {
            return -1;
        }
        char_code = (uint8_t)fields[2][0];
    }
    if (char_code < 0x20U || char_code > 0x7EU) {
        return -1;
    }

    if (parse_tsv_uint_field(fields[3], 0xFFU, &render_x) != 0 ||
        parse_tsv_uint_field(fields[4], 0xFFFFU, &ppu_address) != 0 ||
        parse_tsv_uint_field(fields[5], 0xFFU, &tile_index) != 0) {
        return -1;
    }

    glyph = &glyphs->glyphs[index];
    memset(glyph, 0, sizeof(*glyph));
    glyph->character = (uint8_t)char_code;
    glyph->render_x = (uint8_t)render_x;
    glyph->ppu_address = (uint16_t)ppu_address;
    glyph->tile_index = (uint8_t)tile_index;
    for (size_t i = 0; i < 4U; ++i) {
        uint32_t value = 0U;
        if (parse_tsv_uint_field(fields[6U + i], 0xFFU, &value) != 0) {
            return -1;
        }
        glyph->glyph_tiles[i] = (uint8_t)value;
    }

    glyphs->title_text[index] = (char)char_code;
    glyphs->title_text[index + 1U] = '\0';
    glyphs->glyph_count = index + 1U;
    return 0;
}

static const TecmoTitleGlyph *find_packed_title_glyph_for_char(const TecmoOriginalTitleGlyphs *source, uint8_t character)
{
    for (size_t i = 0; i < source->glyph_count; ++i) {
        if (source->glyphs[i].character == character) {
            return &source->glyphs[i];
        }
    }
    return NULL;
}

static int synthesize_title_glyphs_from_map(const TecmoOriginalTitleGlyphs *source,
                                            const char *title_text,
                                            TecmoOriginalTitleGlyphs *glyphs)
{
    size_t title_len;

    if (source == NULL || title_text == NULL || title_text[0] == '\0' || glyphs == NULL) {
        return -1;
    }
    title_len = strlen(title_text);
    if (title_len == 0U || title_len > TECMO_TITLE_MAX_CHARS || title_len >= sizeof(glyphs->title_text)) {
        return -1;
    }

    memset(glyphs, 0, sizeof(*glyphs));
    title_glyphs_set_pack_defaults(glyphs);
    for (size_t i = 0; i < title_len; ++i) {
        unsigned char character = (unsigned char)title_text[i];
        const TecmoTitleGlyph *source_glyph;
        TecmoTitleGlyph *glyph;

        if (character < 0x20U || character > 0x7EU) {
            return -1;
        }
        source_glyph = find_packed_title_glyph_for_char(source, (uint8_t)character);
        if (source_glyph == NULL) {
            return -1;
        }

        glyph = &glyphs->glyphs[i];
        glyph->character = (uint8_t)character;
        glyph->tile_index = source_glyph->tile_index;
        for (size_t tile = 0; tile < 4U; ++tile) {
            glyph->glyph_tiles[tile] = source_glyph->glyph_tiles[tile];
        }
        glyph->ppu_address = title_ppu_address_for_index(i, &glyph->render_x);
        glyphs->title_text[i] = (char)character;
        glyphs->title_text[i + 1U] = '\0';
    }

    glyphs->glyph_count = title_len;
    return 0;
}

static AssetPackEntryStatus load_title_glyph_map_from_tsv_bytes(const uint8_t *bytes,
                                                                uint64_t byte_count,
                                                                const char *expected_title,
                                                                TecmoOriginalTitleGlyphs *glyphs)
{
    char *text = NULL;
    char *line;
    TecmoOriginalTitleGlyphs parsed;
    AssetPackEntryStatus result = ASSET_PACK_ENTRY_ERROR;

    if (glyphs == NULL) {
        return ASSET_PACK_ENTRY_ERROR;
    }
    memset(&parsed, 0, sizeof(parsed));
    title_glyphs_set_pack_defaults(&parsed);
    if (copy_asset_text_payload(bytes, byte_count, &text) != 0) {
        return ASSET_PACK_ENTRY_ERROR;
    }

    line = text;
    while (line != NULL) {
        char *next = strchr(line, '\n');
        char *trimmed;
        char *fields[12];
        size_t field_count;

        if (next != NULL) {
            *next = '\0';
            ++next;
        }
        strip_line_cr(line);
        trimmed = left_trim(line);
        if (trimmed[0] != '\0' && trimmed[0] != '#') {
            field_count = split_tsv_line(trimmed, fields, sizeof(fields) / sizeof(fields[0]));
            if (field_count == 0U || parse_title_glyph_tsv_line(fields, field_count, &parsed) != 0) {
                goto cleanup;
            }
        }
        line = next;
    }

    if (parsed.glyph_count == 0U ||
        (expected_title != NULL &&
         strcmp(parsed.title_text, expected_title) != 0 &&
         synthesize_title_glyphs_from_map(&parsed, expected_title, glyphs) != 0)) {
        result = parsed.glyph_count == 0U ? ASSET_PACK_ENTRY_ERROR : ASSET_PACK_ENTRY_ABSENT;
        goto cleanup;
    }

    if (expected_title == NULL || strcmp(parsed.title_text, expected_title) == 0) {
        *glyphs = parsed;
    }
    result = ASSET_PACK_ENTRY_LOADED;

cleanup:
    free(text);
    return result;
}

static AssetPackEntryStatus load_title_glyph_map_from_asset_pack(const char *project_root,
                                                                 const char *expected_title,
                                                                 TecmoOriginalTitleGlyphs *glyphs)
{
    uint8_t *entry_bytes = NULL;
    uint64_t entry_size = 0U;
    AssetPackEntryStatus status;

    status = load_asset_pack_entry(project_root, "title/glyph-map.tsv", &entry_bytes, &entry_size);
    if (status == ASSET_PACK_ENTRY_LOADED) {
        status = load_title_glyph_map_from_tsv_bytes(entry_bytes, entry_size, expected_title, glyphs);
        tecmo_asset_pack_free(entry_bytes);
    }

    return status;
}

int tecmo_serialize_title_text(const char *title_text, char **text_out, uint64_t *byte_count)
{
    TextBuffer buffer;
    int result = -1;

    if (title_text == NULL || title_text[0] == '\0' || text_out == NULL || byte_count == NULL) {
        return -1;
    }
    *text_out = NULL;
    *byte_count = 0U;
    memset(&buffer, 0, sizeof(buffer));

    for (const unsigned char *p = (const unsigned char *)title_text; *p != '\0'; ++p) {
        if (*p < 0x20U || *p > 0x7EU || text_buffer_append_char(&buffer, (char)*p) != 0) {
            goto cleanup;
        }
    }
    if (text_buffer_append_char(&buffer, '\n') != 0) {
        goto cleanup;
    }

    result = text_buffer_take(&buffer, text_out, byte_count);

cleanup:
    free(buffer.data);
    return result;
}

int tecmo_serialize_title_glyph_map_tsv(const TecmoOriginalTitleGlyphs *glyphs,
                                        char **text_out,
                                        uint64_t *byte_count)
{
    TextBuffer buffer;
    int result = -1;

    if (glyphs == NULL ||
        glyphs->glyph_count == 0U ||
        glyphs->glyph_count > TECMO_TITLE_MAX_CHARS ||
        text_out == NULL ||
        byte_count == NULL) {
        return -1;
    }
    *text_out = NULL;
    *byte_count = 0U;
    memset(&buffer, 0, sizeof(buffer));

    if (text_buffer_append(&buffer,
                           "index\tchar_code\tchar\trender_x\tppu_address\ttile_index\tglyph0\tglyph1\tglyph2\tglyph3\n") != 0) {
        goto cleanup;
    }

    for (size_t i = 0; i < glyphs->glyph_count; ++i) {
        const TecmoTitleGlyph *glyph = &glyphs->glyphs[i];

        if (glyph->character < 0x20U || glyph->character > 0x7EU) {
            goto cleanup;
        }
        if (text_buffer_appendf(&buffer,
                                "%llu\t0x%02X\t",
                                (unsigned long long)i,
                                (unsigned)glyph->character) != 0 ||
            text_buffer_append_char(&buffer, (char)glyph->character) != 0 ||
            text_buffer_appendf(&buffer,
                                "\t0x%02X\t0x%04X\t0x%02X\t0x%02X\t0x%02X\t0x%02X\t0x%02X\n",
                                (unsigned)glyph->render_x,
                                (unsigned)glyph->ppu_address,
                                (unsigned)glyph->tile_index,
                                (unsigned)glyph->glyph_tiles[0],
                                (unsigned)glyph->glyph_tiles[1],
                                (unsigned)glyph->glyph_tiles[2],
                                (unsigned)glyph->glyph_tiles[3]) != 0) {
            goto cleanup;
        }
    }

    result = text_buffer_take(&buffer, text_out, byte_count);

cleanup:
    free(buffer.data);
    return result;
}

static int load_original_title_text_from_decomp(const char *project_root, char *title, size_t title_size)
{
    char path[TECMO_MAX_PATH_TEXT];
    char line[LINE_BUFFER_SIZE];
    FILE *file;
    size_t out_count = 0;

    if (project_root == NULL || title == NULL || title_size == 0) {
        return -1;
    }

    title[0] = '\0';
    append_path(path, sizeof(path), project_root, ORIGINAL_TITLE_TEXT_FILE);
    normalize_separators(path);

    file = fopen(path, "rb");
    if (file == NULL) {
        return -1;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        uint8_t bytes[128];
        size_t count = 0;
        if (parse_byte_values_strict(line, bytes, sizeof(bytes), &count) != 0) {
            fclose(file);
            title[0] = '\0';
            return -1;
        }
        for (size_t i = 0; i < count && out_count + 1 < title_size; ++i) {
            uint8_t value = bytes[i];
            if (value < 0x20U || value > 0x7EU) {
                fclose(file);
                title[0] = '\0';
                return -1;
            }
            title[out_count++] = (char)value;
        }
        if (count > 0 && out_count + 1 >= title_size) {
            fclose(file);
            title[0] = '\0';
            return -1;
        }
    }

    fclose(file);
    title[out_count] = '\0';
    return out_count > 0 ? 0 : -1;
}

int tecmo_load_original_title_text(const char *project_root, char *title, size_t title_size)
{
    AssetPackEntryStatus pack_status;

    if (title == NULL || title_size == 0) {
        return -1;
    }

    title[0] = '\0';
    pack_status = load_title_text_from_asset_pack(project_root, title, title_size);
    if (pack_status == ASSET_PACK_ENTRY_LOADED) {
        return 0;
    }
    if (pack_status == ASSET_PACK_ENTRY_ERROR) {
        return -1;
    }
    {
        TecmoOriginalTitleGlyphs pack_glyphs;
        pack_status = load_title_glyph_map_from_asset_pack(project_root, NULL, &pack_glyphs);
        if (pack_status == ASSET_PACK_ENTRY_LOADED) {
            if (strlen(pack_glyphs.title_text) >= title_size) {
                return -1;
            }
            copy_text(title, title_size, pack_glyphs.title_text);
            return 0;
        }
        if (pack_status == ASSET_PACK_ENTRY_ERROR) {
            return -1;
        }
    }

    return load_original_title_text_from_decomp(project_root, title, title_size);
}

static int load_baseline_byte_map(const char *project_root,
                                  const char *relative_file,
                                  uint32_t bank_index,
                                  uint32_t cpu_base,
                                  uint8_t *bytes,
                                  bool *present)
{
    char path[TECMO_MAX_PATH_TEXT];
    FILE *file;
    char line[LINE_BUFFER_SIZE];
    uint32_t rom_base = bank_index * 0x4000U;

    memset(bytes, 0, 0x4000U);
    memset(present, 0, 0x4000U * sizeof(present[0]));

    append_path(path, sizeof(path), project_root, relative_file);
    normalize_separators(path);

    file = fopen(path, "rb");
    if (file == NULL) {
        return -1;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        uint8_t values[128];
        size_t count = parse_byte_values(line, values, sizeof(values));
        uint32_t rom_address = 0;
        if (count == 0 || parse_comment_address(line, &rom_address) != 0) {
            continue;
        }

        for (size_t i = 0; i < count; ++i) {
            uint32_t offset = rom_address + (uint32_t)i - rom_base;
            if (offset < 0x4000U) {
                uint32_t cpu_address = cpu_base + offset;
                uint32_t cpu_offset = cpu_address - cpu_base;
                if (cpu_offset < 0x4000U) {
                    bytes[cpu_offset] = values[i];
                    present[cpu_offset] = true;
                }
            }
        }
    }

    fclose(file);
    return 0;
}

static int read_mapped_byte(const uint8_t *bytes,
                            const bool *present,
                            uint32_t cpu_base,
                            uint32_t cpu_address,
                            uint8_t *value_out)
{
    uint32_t offset = cpu_address - cpu_base;
    if (cpu_address < cpu_base || offset >= 0x4000U || !present[offset]) {
        return -1;
    }
    *value_out = bytes[offset];
    return 0;
}

static int add_address_count(TecmoAddressCount *targets, uint8_t *target_count, uint16_t target)
{
    for (uint8_t i = 0; i < *target_count; ++i) {
        if (targets[i].target == target) {
            if (targets[i].count < 0xFFU) {
                ++targets[i].count;
            }
            return 0;
        }
    }

    if (*target_count >= TECMO_TITLE_SETUP_MAX_TARGETS) {
        return -1;
    }

    targets[*target_count].target = target;
    targets[*target_count].count = 1;
    ++(*target_count);
    return 0;
}

static bool is_known_title_setup_call(uint16_t target)
{
    return target == 0xBA95U ||
           target == 0xBAA4U ||
           target == 0xC000U ||
           target == 0xC009U ||
           target == 0xC054U ||
           target == 0xC05AU ||
           target == 0xC06FU;
}

static bool is_fixed_title_setup_call(uint16_t target)
{
    return target == 0xC000U ||
           target == 0xC009U ||
           target == 0xC054U ||
           target == 0xC05AU ||
           target == 0xC06FU;
}

static int fixed_title_setup_call_index(uint16_t target)
{
    switch (target) {
    case 0xC000U:
        return 0;
    case 0xC009U:
        return 1;
    case 0xC054U:
        return 2;
    case 0xC05AU:
        return 3;
    case 0xC06FU:
        return 4;
    default:
        return -1;
    }
}

static int collect_fixed_helper_range(const uint8_t *bytes,
                                      const bool *present,
                                      uint16_t start,
                                      uint16_t end,
                                      bool *seen_helpers,
                                      TecmoTitleSetupSummary *summary)
{
    for (uint32_t address = start; address + 2U <= end; ++address) {
        uint8_t opcode = 0;
        uint8_t lo = 0;
        uint8_t hi = 0;
        uint16_t target;
        int helper_index;

        if (read_mapped_byte(bytes, present, 0x8000U, address, &opcode) != 0) {
            return -1;
        }
        if (opcode != 0x20U) {
            continue;
        }
        if (read_mapped_byte(bytes, present, 0x8000U, address + 1U, &lo) != 0 ||
            read_mapped_byte(bytes, present, 0x8000U, address + 2U, &hi) != 0) {
            return -1;
        }

        target = (uint16_t)(((uint16_t)hi << 8U) | lo);
        helper_index = fixed_title_setup_call_index(target);
        if (helper_index < 0) {
            continue;
        }

        if (!seen_helpers[helper_index]) {
            seen_helpers[helper_index] = true;
            ++summary->fixed_helper_unique_count;
        }
        ++summary->fixed_helper_call_invocations;

        if (target == 0xC000U) {
            uint8_t previous_opcode = 0;
            uint8_t wait_value = 0;
            ++summary->fixed_wait_call_count;
            if (address >= (uint32_t)start + 2U &&
                read_mapped_byte(bytes, present, 0x8000U, address - 2U, &previous_opcode) == 0 &&
                read_mapped_byte(bytes, present, 0x8000U, address - 1U, &wait_value) == 0 &&
                previous_opcode == 0xA9U) {
                summary->fixed_wait_request_total = (uint16_t)(summary->fixed_wait_request_total + wait_value);
            }
        } else if (target == 0xC009U) {
            ++summary->fixed_setup_finalize_call_count;
        } else if (target == 0xC054U) {
            ++summary->fixed_stream_finalize_call_count;
        } else if (target == 0xC05AU || target == 0xC06FU) {
            ++summary->fixed_staging_seed_call_count;
        }
    }

    return 0;
}

static int collect_fixed_helper_summary(const uint8_t *bytes,
                                        const bool *present,
                                        TecmoTitleSetupSummary *summary)
{
    bool seen_helpers[5] = {false, false, false, false, false};

    if (collect_fixed_helper_range(bytes,
                                   present,
                                   summary->adjacent_driver_start,
                                   summary->adjacent_driver_end,
                                   seen_helpers,
                                   summary) != 0 ||
        collect_fixed_helper_range(bytes,
                                   present,
                                   summary->stream_copy_start,
                                   summary->stream_copy_end,
                                   seen_helpers,
                                   summary) != 0) {
        return -1;
    }

    summary->fixed_helper_summary_loaded = true;
    return 0;
}

static int collect_palette_probe_range(const uint8_t *bytes,
                                       const bool *present,
                                       uint16_t start,
                                       uint16_t end,
                                       TecmoTitleSetupSummary *summary)
{
    for (uint32_t address = start; address <= end; ++address) {
        uint8_t opcode = 0;
        if (read_mapped_byte(bytes, present, 0x8000U, address, &opcode) != 0) {
            return -1;
        }

        if (opcode == 0xA9U && address + 1U <= end) {
            uint8_t immediate = 0;
            if (read_mapped_byte(bytes, present, 0x8000U, address + 1U, &immediate) != 0) {
                return -1;
            }
            if (immediate == 0x3FU) {
                ++summary->palette_direct_high_literal_count;
            }
        }

        if ((opcode == 0x8CU || opcode == 0x8DU || opcode == 0x8EU) && address + 2U <= end) {
            uint8_t lo = 0;
            uint8_t hi = 0;
            uint16_t target;
            if (read_mapped_byte(bytes, present, 0x8000U, address + 1U, &lo) != 0 ||
                read_mapped_byte(bytes, present, 0x8000U, address + 2U, &hi) != 0) {
                return -1;
            }
            target = (uint16_t)(((uint16_t)hi << 8U) | lo);
            if (target == 0x2006U) {
                ++summary->palette_direct_ppu_addr_write_count;
            } else if (target == 0x2007U) {
                ++summary->palette_direct_ppu_data_write_count;
            }
        }
    }

    ++summary->palette_probe_range_count;
    return 0;
}

static int collect_palette_probe_summary(const uint8_t *bytes,
                                         const bool *present,
                                         TecmoTitleSetupSummary *summary)
{
    if (collect_palette_probe_range(bytes,
                                    present,
                                    summary->exact_entry_start,
                                    summary->exact_entry_end,
                                    summary) != 0 ||
        collect_palette_probe_range(bytes,
                                    present,
                                    summary->adjacent_driver_start,
                                    summary->adjacent_driver_end,
                                    summary) != 0 ||
        collect_palette_probe_range(bytes,
                                    present,
                                    summary->stream_copy_start,
                                    summary->stream_copy_end,
                                    summary) != 0) {
        return -1;
    }

    summary->palette_fixed_helper_candidate_count = summary->fixed_helper_call_invocations;
    summary->palette_queue_decode_pending =
        summary->palette_fixed_helper_candidate_count > 0U &&
        summary->palette_direct_ppu_addr_write_count == 0U &&
        summary->palette_direct_ppu_data_write_count == 0U &&
        summary->palette_direct_high_literal_count == 0U;
    summary->palette_probe_summary_loaded = true;
    return 0;
}

static int collect_fixed_vector_summary(const uint8_t *bank07,
                                        const bool *bank07_present,
                                        TecmoTitleSetupSummary *summary)
{
    static const uint16_t fixed_targets[] = {
        0xC000U,
        0xC009U,
        0xC054U,
        0xC05AU,
        0xC06FU,
    };

    summary->fixed_vector_min_target = 0xFFFFU;
    for (size_t i = 0; i < sizeof(fixed_targets) / sizeof(fixed_targets[0]); ++i) {
        uint16_t entry = fixed_targets[i];
        uint8_t opcode = 0;

        ++summary->fixed_vector_entry_count;
        if (read_mapped_byte(bank07, bank07_present, 0xC000U, entry, &opcode) != 0) {
            return -1;
        }
        if (opcode == 0x4CU) {
            uint8_t lo = 0;
            uint8_t hi = 0;
            uint16_t target;
            ++summary->fixed_vector_jmp_entry_count;
            if (read_mapped_byte(bank07, bank07_present, 0xC000U, entry + 1U, &lo) != 0 ||
                read_mapped_byte(bank07, bank07_present, 0xC000U, entry + 2U, &hi) != 0) {
                return -1;
            }
            target = (uint16_t)(((uint16_t)hi << 8U) | lo);
            if (target < summary->fixed_vector_min_target) {
                summary->fixed_vector_min_target = target;
            }
            if (target > summary->fixed_vector_max_target) {
                summary->fixed_vector_max_target = target;
            }
            ++summary->fixed_vector_resolved_target_count;
        } else if ((opcode == 0x8CU || opcode == 0x8DU || opcode == 0x8EU) &&
                   entry + 2U <= 0xFFFFU) {
            uint8_t lo = 0;
            uint8_t hi = 0;
            uint16_t target;
            if (read_mapped_byte(bank07, bank07_present, 0xC000U, entry + 1U, &lo) != 0 ||
                read_mapped_byte(bank07, bank07_present, 0xC000U, entry + 2U, &hi) != 0) {
                return -1;
            }
            target = (uint16_t)(((uint16_t)hi << 8U) | lo);
            if (target == 0x2006U || target == 0x2007U) {
                ++summary->fixed_vector_entry_ppu_write_count;
            }
        }
    }

    if (summary->fixed_vector_resolved_target_count == 0U) {
        summary->fixed_vector_min_target = 0U;
    }
    summary->fixed_vector_summary_loaded = true;
    return 0;
}

static int collect_jsr_targets(const uint8_t *bytes,
                               const bool *present,
                               uint16_t start,
                               uint16_t end,
                               TecmoAddressCount *targets,
                               uint8_t *target_count,
                               uint8_t *invocations,
                               uint16_t *first_unclassified,
                               bool *has_fixed_helper)
{
    *target_count = 0;
    if (invocations != NULL) {
        *invocations = 0;
    }

    for (uint32_t address = start; address + 2U <= end; ++address) {
        uint8_t opcode = 0;
        if (read_mapped_byte(bytes, present, 0x8000U, address, &opcode) != 0) {
            return -1;
        }
        if (opcode == 0x20U) {
            uint8_t lo = 0;
            uint8_t hi = 0;
            uint16_t target;
            if (read_mapped_byte(bytes, present, 0x8000U, address + 1U, &lo) != 0 ||
                read_mapped_byte(bytes, present, 0x8000U, address + 2U, &hi) != 0) {
                return -1;
            }
            target = (uint16_t)(((uint16_t)hi << 8U) | lo);
            if (add_address_count(targets, target_count, target) != 0) {
                return -1;
            }
            if (invocations != NULL && *invocations < 0xFFU) {
                ++(*invocations);
            }
            if (has_fixed_helper != NULL && is_fixed_title_setup_call(target)) {
                *has_fixed_helper = true;
            }
            if (first_unclassified != NULL && *first_unclassified == 0U && !is_known_title_setup_call(target)) {
                *first_unclassified = target;
            }
        }
    }

    return 0;
}

static int collect_sta_targets(const uint8_t *bytes,
                               const bool *present,
                               uint16_t start,
                               uint16_t end,
                               TecmoAddressCount *targets,
                               uint8_t *target_count)
{
    *target_count = 0;

    for (uint32_t address = start; address <= end; ++address) {
        uint8_t opcode = 0;
        uint16_t target = 0;
        if (read_mapped_byte(bytes, present, 0x8000U, address, &opcode) != 0) {
            return -1;
        }

        if (opcode == 0x85U) {
            uint8_t zp = 0;
            if (address + 1U > end ||
                read_mapped_byte(bytes, present, 0x8000U, address + 1U, &zp) != 0) {
                return -1;
            }
            target = zp;
        } else if (opcode == 0x8DU) {
            uint8_t lo = 0;
            uint8_t hi = 0;
            if (address + 2U > end ||
                read_mapped_byte(bytes, present, 0x8000U, address + 1U, &lo) != 0 ||
                read_mapped_byte(bytes, present, 0x8000U, address + 2U, &hi) != 0) {
                return -1;
            }
            target = (uint16_t)(((uint16_t)hi << 8U) | lo);
        } else {
            continue;
        }

        if (add_address_count(targets, target_count, target) != 0) {
            return -1;
        }
    }

    return 0;
}

static bool mapped_range_present(const bool *present, uint32_t cpu_base, uint16_t start, uint16_t end_exclusive)
{
    for (uint32_t address = start; address < end_exclusive; ++address) {
        uint32_t offset = address - cpu_base;
        if (address < cpu_base || offset >= 0x4000U || !present[offset]) {
            return false;
        }
    }
    return true;
}

static void add_selected_stream_index(bool *selected, uint8_t *selected_count, uint8_t index)
{
    if (index >= TECMO_TITLE_SETUP_STREAM_TABLE_ENTRIES || selected[index]) {
        return;
    }
    selected[index] = true;
    ++(*selected_count);
}

static int apply_title_stream_to_staging_summary(const uint8_t *bank04,
                                                 const bool *bank04_present,
                                                 uint8_t stream_index,
                                                 TecmoTitleSetupSummary *summary)
{
    const uint16_t staging_base = 0x01FDU;
    uint8_t lo = 0;
    uint8_t hi = 0;
    uint8_t record_count = 0;
    uint8_t base_x = 0;
    uint8_t base_y = 0;
    uint16_t pointer;
    uint16_t bytes_consumed;

    if (stream_index >= TECMO_TITLE_SETUP_STREAM_TABLE_ENTRIES ||
        read_mapped_byte(bank04, bank04_present, 0x8000U, 0xB33FU + stream_index, &lo) != 0 ||
        read_mapped_byte(bank04, bank04_present, 0x8000U, 0xB34EU + stream_index, &hi) != 0) {
        return -1;
    }

    pointer = (uint16_t)(((uint16_t)hi << 8U) | lo);
    if (read_mapped_byte(bank04, bank04_present, 0x8000U, pointer, &record_count) != 0 ||
        read_mapped_byte(bank04, bank04_present, 0x8000U, pointer + 1U, &base_x) != 0 ||
        read_mapped_byte(bank04, bank04_present, 0x8000U, pointer + 2U, &base_y) != 0) {
        return -1;
    }

    bytes_consumed = (uint16_t)(1U + summary->stream_base_parameter_bytes +
                                (uint16_t)record_count * summary->stream_source_fields_per_record);
    if ((uint32_t)pointer + (uint32_t)bytes_consumed > 0xC000U ||
        !mapped_range_present(bank04_present, 0x8000U, pointer, (uint16_t)(pointer + bytes_consumed))) {
        return -1;
    }

    for (uint16_t record = 0; record < record_count; ++record) {
        uint16_t source = (uint16_t)(pointer + 3U + (uint16_t)record * 4U);
        uint8_t field0 = 0;
        uint8_t field1 = 0;
        uint8_t field2 = 0;
        uint8_t field3 = 0;
        uint8_t staged0;
        uint8_t staged1;
        uint8_t staged2;
        uint8_t staged3;

        if (read_mapped_byte(bank04, bank04_present, 0x8000U, source, &field0) != 0 ||
            read_mapped_byte(bank04, bank04_present, 0x8000U, source + 1U, &field1) != 0 ||
            read_mapped_byte(bank04, bank04_present, 0x8000U, source + 2U, &field2) != 0 ||
            read_mapped_byte(bank04, bank04_present, 0x8000U, source + 3U, &field3) != 0) {
            return -1;
        }

        staged0 = (uint8_t)(field0 + base_x);
        staged1 = (uint8_t)(field1 + 0x3CU);
        staged2 = field2;
        staged3 = (uint8_t)(field3 + base_y);
        (void)staged0;
        (void)staged1;
        (void)staged2;
        (void)staged3;
    }

    if (record_count > 0U) {
        uint16_t first_write = (uint16_t)(staging_base + 3U);
        uint16_t last_write = (uint16_t)(staging_base + 2U + (uint16_t)record_count * 4U);
        if (summary->stream_staging_record_count == 0U || first_write < summary->stream_staging_first_write) {
            summary->stream_staging_first_write = first_write;
        }
        if (last_write > summary->stream_staging_last_write) {
            summary->stream_staging_last_write = last_write;
        }
    }

    summary->stream_staging_base_address = staging_base;
    if (summary->stream_staging_stream_count < 0xFFU) {
        ++summary->stream_staging_stream_count;
    }
    summary->stream_staging_record_count = (uint16_t)(summary->stream_staging_record_count + record_count);
    summary->stream_staging_bytes_written =
        (uint16_t)(summary->stream_staging_bytes_written +
                   (uint16_t)record_count * summary->stream_staged_fields_per_record);
    return 0;
}

static int apply_selected_title_streams_to_staging_summary(const uint8_t *bank04,
                                                           const bool *bank04_present,
                                                           const bool *selected_streams,
                                                           TecmoTitleSetupSummary *summary)
{
    for (uint8_t index = 0; index < TECMO_TITLE_SETUP_STREAM_TABLE_ENTRIES; ++index) {
        if (selected_streams[index] &&
            apply_title_stream_to_staging_summary(bank04, bank04_present, index, summary) != 0) {
            return -1;
        }
    }

    summary->stream_staging_summary_loaded = true;
    return 0;
}

static int load_title_stream_format_summary(const uint8_t *bank04,
                                            const bool *bank04_present,
                                            TecmoTitleSetupSummary *summary)
{
    bool selected_streams[TECMO_TITLE_SETUP_STREAM_TABLE_ENTRIES];
    memset(selected_streams, 0, sizeof(selected_streams));

    summary->stream_table_entry_count = TECMO_TITLE_SETUP_STREAM_TABLE_ENTRIES;
    summary->dynamic_selector_row_count = TECMO_TITLE_SETUP_SELECTOR_ROWS;
    summary->stream_base_parameter_bytes = 2U;
    summary->stream_source_fields_per_record = 4U;
    summary->stream_staged_fields_per_record = 4U;
    add_selected_stream_index(selected_streams, &summary->selected_stream_count, 0U);

    for (uint8_t index = 0; index < TECMO_TITLE_SETUP_STREAM_TABLE_ENTRIES; ++index) {
        uint8_t lo = 0;
        uint8_t hi = 0;
        uint8_t record_count = 0;
        uint16_t pointer;
        uint16_t bytes_consumed;
        uint16_t emitted_bytes;

        if (read_mapped_byte(bank04, bank04_present, 0x8000U, 0xB33FU + index, &lo) != 0 ||
            read_mapped_byte(bank04, bank04_present, 0x8000U, 0xB34EU + index, &hi) != 0) {
            return -1;
        }

        pointer = (uint16_t)(((uint16_t)hi << 8U) | lo);
        if (read_mapped_byte(bank04, bank04_present, 0x8000U, pointer, &record_count) != 0) {
            return -1;
        }

        bytes_consumed = (uint16_t)(1U + summary->stream_base_parameter_bytes +
                                    (uint16_t)record_count * summary->stream_source_fields_per_record);
        emitted_bytes = (uint16_t)((uint16_t)record_count * summary->stream_staged_fields_per_record);
        if ((uint32_t)pointer + (uint32_t)bytes_consumed <= 0xC000U &&
            mapped_range_present(bank04_present, 0x8000U, pointer, (uint16_t)(pointer + bytes_consumed))) {
            ++summary->verified_stream_table_entry_count;
        }
        if (record_count > summary->max_stream_record_count) {
            summary->max_stream_record_count = record_count;
        }
        if (bytes_consumed > summary->max_stream_bytes_consumed) {
            summary->max_stream_bytes_consumed = bytes_consumed;
        }
        if (emitted_bytes > summary->max_stream_emitted_bytes) {
            summary->max_stream_emitted_bytes = emitted_bytes;
        }
    }

    for (uint8_t row = 0; row < TECMO_TITLE_SETUP_SELECTOR_ROWS; ++row) {
        uint8_t lo = 0;
        uint8_t hi = 0;
        uint16_t pointer;
        bool terminated = false;

        if (read_mapped_byte(bank04, bank04_present, 0x8000U, 0xB317U + row, &lo) != 0 ||
            read_mapped_byte(bank04, bank04_present, 0x8000U, 0xB31CU + row, &hi) != 0) {
            return -1;
        }

        pointer = (uint16_t)(((uint16_t)hi << 8U) | lo);
        for (uint8_t offset = 0; offset < 16U; ++offset) {
            uint8_t value = 0;
            if (read_mapped_byte(bank04, bank04_present, 0x8000U, pointer + offset, &value) != 0) {
                return -1;
            }
            if ((value & 0x80U) != 0) {
                terminated = true;
                break;
            }
            add_selected_stream_index(selected_streams, &summary->selected_stream_count, value);
        }

        if (terminated) {
            ++summary->terminated_selector_row_count;
        }
    }

    summary->stream_format_summary_loaded = true;
    summary->stream_effect_summary_loaded = true;
    if (apply_selected_title_streams_to_staging_summary(bank04, bank04_present, selected_streams, summary) != 0) {
        return -1;
    }
    return 0;
}

static int load_title_setup_summary(const uint8_t *bank04,
                                    const bool *bank04_present,
                                    const uint8_t *bank07,
                                    const bool *bank07_present,
                                    TecmoTitleSetupSummary *summary)
{
    static const struct {
        uint16_t start;
        uint16_t end_exclusive;
    } table_refs[TECMO_TITLE_SETUP_TABLE_REFS] = {
        {0xBA88U, 0xBA95U},
        {0xB317U, 0xB31CU},
        {0xB31CU, 0xB321U},
        {0xB33FU, 0xB34EU},
        {0xB34EU, 0xB35DU},
    };
    bool has_fixed_helper = false;

    memset(summary, 0, sizeof(*summary));
    summary->exact_entry_start = 0xBA16U;
    summary->exact_entry_end = 0xBA24U;
    summary->adjacent_driver_start = 0xBA25U;
    summary->adjacent_driver_end = 0xBA84U;
    summary->stream_copy_start = 0xBAA4U;
    summary->stream_copy_end = 0xBAF0U;
    summary->table_reference_count = TECMO_TITLE_SETUP_TABLE_REFS;
    summary->stream_decode_pending = true;

    if (!mapped_range_present(bank04_present, 0x8000U, summary->exact_entry_start, (uint16_t)(summary->exact_entry_end + 1U)) ||
        !mapped_range_present(bank04_present, 0x8000U, summary->adjacent_driver_start, (uint16_t)(summary->adjacent_driver_end + 1U)) ||
        !mapped_range_present(bank04_present, 0x8000U, summary->stream_copy_start, (uint16_t)(summary->stream_copy_end + 1U))) {
        return -1;
    }

    for (size_t i = 0; i < TECMO_TITLE_SETUP_TABLE_REFS; ++i) {
        if (mapped_range_present(bank04_present, 0x8000U, table_refs[i].start, table_refs[i].end_exclusive)) {
            ++summary->verified_table_reference_count;
        }
    }

    if (collect_jsr_targets(bank04,
                            bank04_present,
                            summary->adjacent_driver_start,
                            summary->adjacent_driver_end,
                            summary->driver_calls,
                            &summary->driver_call_count,
                            &summary->driver_call_invocations,
                            &summary->first_unclassified_call,
                            &has_fixed_helper) != 0 ||
        collect_sta_targets(bank04,
                            bank04_present,
                            summary->adjacent_driver_start,
                            summary->adjacent_driver_end,
                            summary->driver_writes,
                            &summary->driver_write_count) != 0 ||
        collect_sta_targets(bank04,
                            bank04_present,
                            summary->stream_copy_start,
                            summary->stream_copy_end,
                            summary->stream_writes,
                            &summary->stream_write_count) != 0) {
        return -1;
    }
    if (collect_fixed_helper_summary(bank04, bank04_present, summary) != 0) {
        return -1;
    }
    if (collect_palette_probe_summary(bank04, bank04_present, summary) != 0) {
        return -1;
    }
    if (collect_fixed_vector_summary(bank07, bank07_present, summary) != 0) {
        return -1;
    }
    if (load_title_stream_format_summary(bank04, bank04_present, summary) != 0) {
        return -1;
    }

    summary->fixed_helper_effects_pending = has_fixed_helper;
    summary->loaded = true;
    return 0;
}

static int title_char_to_tile(uint8_t code, const uint8_t *bank06, const bool *bank06_present, uint8_t *tile_out)
{
    if (code == 0x2EU || code == 0x20U) {
        *tile_out = 0x18U;
        return 0;
    }
    if (code == 0x2DU) {
        *tile_out = 0x25U;
        return 0;
    }
    if (code < 0x3AU && code >= 0x17U) {
        *tile_out = (uint8_t)(code - 0x17U);
        return 0;
    }

    return read_mapped_byte(bank06, bank06_present, 0x8000U, 0xA273U + code, tile_out);
}

static uint16_t title_ppu_address_for_index(size_t index, uint8_t *render_x_out)
{
    uint8_t render_x = (uint8_t)((index + 0x10U) & 0x1FU);
    uint8_t high = 0x22U;
    uint8_t column = render_x;

    if (render_x >= 0x10U) {
        high ^= 0x04U;
        column = (uint8_t)(render_x - 0x10U);
    }

    *render_x_out = render_x;
    return (uint16_t)(((uint16_t)high << 8U) | ((uint16_t)column * 2U));
}

static int load_title_glyphs_for_text(const char *project_root,
                                      const char *title_text,
                                      TecmoOriginalTitleGlyphs *glyphs)
{
    uint8_t bank04[0x4000];
    uint8_t bank06[0x4000];
    uint8_t bank07[0x4000];
    bool bank04_present[0x4000];
    bool bank06_present[0x4000];
    bool bank07_present[0x4000];
    size_t title_len;

    if (glyphs == NULL) {
        return -1;
    }
    memset(glyphs, 0, sizeof(*glyphs));

    if (title_text == NULL || title_text[0] == '\0') {
        return -1;
    }
    for (size_t i = 0; title_text[i] != '\0'; ++i) {
        unsigned char value = (unsigned char)title_text[i];
        if (value < 0x20U || value > 0x7EU || i + 1U >= sizeof(glyphs->title_text)) {
            return -1;
        }
        glyphs->title_text[i] = (char)value;
        glyphs->title_text[i + 1U] = '\0';
    }

    title_len = strlen(glyphs->title_text);
    if (title_len == 0 || title_len > TECMO_TITLE_MAX_CHARS) {
        return -1;
    }

    if (load_baseline_byte_map(project_root, BASELINE_BANK04_FILE, 4U, 0x8000U, bank04, bank04_present) != 0 ||
        load_baseline_byte_map(project_root, BASELINE_BANK06_FILE, 6U, 0x8000U, bank06, bank06_present) != 0 ||
        load_baseline_byte_map(project_root, BASELINE_BANK07_FILE, 7U, 0xC000U, bank07, bank07_present) != 0) {
        return -1;
    }

    glyphs->dispatcher_call_index = 0x38U;
    if (read_mapped_byte(bank07, bank07_present, 0xC000U, 0xCAF5U + glyphs->dispatcher_call_index, &glyphs->dispatcher_bank) != 0) {
        return -1;
    }
    {
        uint8_t lo = 0;
        uint8_t hi = 0;
        if (read_mapped_byte(bank07, bank07_present, 0xC000U, 0xCB33U + glyphs->dispatcher_call_index, &lo) != 0 ||
            read_mapped_byte(bank07, bank07_present, 0xC000U, 0xCB71U + glyphs->dispatcher_call_index, &hi) != 0) {
            return -1;
        }
        glyphs->dispatcher_target = (uint16_t)(((uint16_t)hi << 8U) | lo);
    }
    glyphs->dispatcher_matches_expected = glyphs->dispatcher_bank == 0x06U && glyphs->dispatcher_target == 0x9E50U;
    glyphs->chr_config_0100 = 0x06U;
    glyphs->setup_selector_0352 = 0x1FU;
    glyphs->ba16_update_flags_or_05b6 = 0x01U;
    glyphs->ba16_update_flag_modeled = true;
    if (load_title_setup_summary(bank04, bank04_present, bank07, bank07_present, &glyphs->setup_summary) != 0) {
        return -1;
    }

    for (size_t i = 0; i < title_len; ++i) {
        TecmoTitleGlyph *glyph = &glyphs->glyphs[i];
        uint8_t tile_index = 0;
        uint32_t glyph_base;

        glyph->character = (uint8_t)glyphs->title_text[i];
        if (title_char_to_tile(glyph->character, bank06, bank06_present, &tile_index) != 0) {
            return -1;
        }
        glyph->tile_index = tile_index;
        glyph->ppu_address = title_ppu_address_for_index(i, &glyph->render_x);

        glyph_base = 0xAF05U + (uint32_t)tile_index * 4U;
        for (size_t tile = 0; tile < 4; ++tile) {
            if (read_mapped_byte(bank06, bank06_present, 0x8000U, glyph_base + (uint32_t)tile, &glyph->glyph_tiles[tile]) != 0) {
                return -1;
            }
        }
    }

    glyphs->glyph_count = title_len;
    return 0;
}

int tecmo_load_original_title_glyphs(const char *project_root, TecmoOriginalTitleGlyphs *glyphs)
{
    char title_text[TECMO_MAX_NAME_TEXT];
    AssetPackEntryStatus pack_status;

    pack_status = load_title_glyph_map_from_asset_pack(project_root, NULL, glyphs);
    if (pack_status == ASSET_PACK_ENTRY_LOADED) {
        return 0;
    }
    if (pack_status == ASSET_PACK_ENTRY_ERROR) {
        return -1;
    }
    if (tecmo_load_original_title_text(project_root, title_text, sizeof(title_text)) != 0) {
        return -1;
    }
    return load_title_glyphs_for_text(project_root, title_text, glyphs);
}

int tecmo_load_title_glyphs_for_text(const char *project_root,
                                     const char *title_text,
                                     TecmoOriginalTitleGlyphs *glyphs)
{
    AssetPackEntryStatus pack_status;

    pack_status = load_title_glyph_map_from_asset_pack(project_root, title_text, glyphs);
    if (pack_status == ASSET_PACK_ENTRY_LOADED) {
        return 0;
    }
    if (pack_status == ASSET_PACK_ENTRY_ERROR) {
        return -1;
    }
    return load_title_glyphs_for_text(project_root, title_text, glyphs);
}

static int add_decomp_derived_text_entry(TecmoAssetPackBuilder *builder,
                                         const char *entry_id,
                                         const char *text,
                                         uint64_t byte_count,
                                         char *message,
                                         size_t message_size)
{
    TecmoAssetPackEntryInfo entry_info;
    char builder_message[256];

    memset(&entry_info, 0, sizeof(entry_info));
    entry_info.id = entry_id;
    entry_info.type = TECMO_ASSET_PACK_TYPE_DATA;
    entry_info.flags = TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL;

    builder_message[0] = '\0';
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            text,
                                            byte_count,
                                            builder_message,
                                            sizeof(builder_message)) != 0) {
        if (builder_message[0] != '\0') {
            set_inventory_message(message, message_size, "Could not append %s: %s", entry_id, builder_message);
        } else {
            set_inventory_message(message, message_size, "Could not append %s.", entry_id);
        }
        return -1;
    }

    return 0;
}

int tecmo_asset_pack_builder_add_decomp_derived_entries(const char *project_root,
                                                        TecmoAssetPackBuilder *builder,
                                                        char *message,
                                                        size_t message_size)
{
    RosterTable roster;
    TecmoOriginalTitleGlyphs glyphs;
    char title_text[TECMO_MAX_NAME_TEXT];
    char *roster_text = NULL;
    char *title_text_payload = NULL;
    char *glyph_text = NULL;
    uint64_t roster_byte_count = 0U;
    uint64_t title_byte_count = 0U;
    uint64_t glyph_byte_count = 0U;
    int result = -1;

    memset(&roster, 0, sizeof(roster));
    memset(&glyphs, 0, sizeof(glyphs));
    title_text[0] = '\0';

    if (project_root == NULL || project_root[0] == '\0') {
        set_inventory_message(message, message_size, "Project root is required for decomp-derived asset pack entries.");
        return -1;
    }
    if (builder == NULL) {
        set_inventory_message(message, message_size, "Asset pack builder is required for decomp-derived entries.");
        return -1;
    }

    /* Avoid reading old logical entries from TECMO_ASSETPACK while building a replacement pack. */
    if (collect_rosters_from_decomp_sources(project_root, &roster) != 0) {
        set_inventory_message(message,
                              message_size,
                              "Could not collect roster/table.tsv from decomp roster sources.");
        goto cleanup;
    }
    if (tecmo_serialize_roster_table_tsv(&roster, &roster_text, &roster_byte_count) != 0) {
        set_inventory_message(message, message_size, "Could not serialize roster/table.tsv.");
        goto cleanup;
    }
    if (add_decomp_derived_text_entry(builder,
                                      "roster/table.tsv",
                                      roster_text,
                                      roster_byte_count,
                                      message,
                                      message_size) != 0) {
        goto cleanup;
    }
    free(roster_text);
    roster_text = NULL;

    if (load_original_title_text_from_decomp(project_root, title_text, sizeof(title_text)) != 0) {
        set_inventory_message(message,
                              message_size,
                              "Could not collect title/original-text.txt from the decomp title text source.");
        goto cleanup;
    }
    if (tecmo_serialize_title_text(title_text, &title_text_payload, &title_byte_count) != 0) {
        set_inventory_message(message, message_size, "Could not serialize title/original-text.txt.");
        goto cleanup;
    }
    if (add_decomp_derived_text_entry(builder,
                                      "title/original-text.txt",
                                      title_text_payload,
                                      title_byte_count,
                                      message,
                                      message_size) != 0) {
        goto cleanup;
    }
    free(title_text_payload);
    title_text_payload = NULL;

    if (load_title_glyphs_for_text(project_root, title_text, &glyphs) != 0) {
        set_inventory_message(message,
                              message_size,
                              "Could not collect title/glyph-map.tsv from baseline title glyph sources.");
        goto cleanup;
    }
    if (tecmo_serialize_title_glyph_map_tsv(&glyphs, &glyph_text, &glyph_byte_count) != 0) {
        set_inventory_message(message, message_size, "Could not serialize title/glyph-map.tsv.");
        goto cleanup;
    }
    if (add_decomp_derived_text_entry(builder,
                                      "title/glyph-map.tsv",
                                      glyph_text,
                                      glyph_byte_count,
                                      message,
                                      message_size) != 0) {
        goto cleanup;
    }
    free(glyph_text);
    glyph_text = NULL;

    set_inventory_message(message,
                          message_size,
                          "Added decomp-derived logical entries: roster/table.tsv (%llu records), title/original-text.txt, title/glyph-map.tsv (%llu glyphs).",
                          (unsigned long long)roster.count,
                          (unsigned long long)glyphs.glyph_count);
    result = 0;

cleanup:
    free(roster_text);
    free(title_text_payload);
    free(glyph_text);
    roster_table_free(&roster);
    return result;
}

static int convert_tiles_file(const char *path, FILE *out, uint64_t *byte_count)
{
    FILE *file = fopen(path, "rb");
    char line[LINE_BUFFER_SIZE];
    if (file == NULL) {
        return -1;
    }

    *byte_count = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        uint8_t values[128];
        size_t count = parse_byte_values(line, values, sizeof(values));
        for (size_t i = 0; i < count; ++i) {
            if (out != NULL && fputc(values[i], out) == EOF) {
                fclose(file);
                return -1;
            }
            ++(*byte_count);
        }
    }

    fclose(file);
    return 0;
}

static int load_chr_padded(const char *project_root, uint8_t **bytes_out, uint64_t *byte_count)
{
    char path[TECMO_MAX_PATH_TEXT];
    FILE *file;
    char line[LINE_BUFFER_SIZE];
    uint8_t *bytes;
    uint64_t count = 0;

    append_path(path, sizeof(path), project_root, "build\\baseline\\Tiles.asm");
    normalize_separators(path);

    file = fopen(path, "rb");
    if (file == NULL) {
        return -1;
    }

    bytes = (uint8_t *)calloc((size_t)TECMO_CHR_CONFIG_BYTES, 1);
    if (bytes == NULL) {
        fclose(file);
        return -1;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        uint8_t values[128];
        size_t parsed = parse_byte_values(line, values, sizeof(values));
        for (size_t i = 0; i < parsed; ++i) {
            if (count >= TECMO_CHR_CONFIG_BYTES) {
                free(bytes);
                fclose(file);
                return -1;
            }
            bytes[count++] = values[i];
        }
    }

    fclose(file);
    *bytes_out = bytes;
    *byte_count = TECMO_CHR_CONFIG_BYTES;
    return 0;
}

static int file_exists(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    fclose(file);
    return 1;
}

static int load_chr_from_asset_pack_path(const char *path, uint8_t **bytes_out, uint64_t *byte_count)
{
    uint8_t *entry_bytes = NULL;
    uint64_t entry_size = 0;
    uint8_t *padded;
    uint64_t copy_size;

    if (tecmo_asset_pack_read_entry(path, "chr/all", &entry_bytes, &entry_size) != 0) {
        return -1;
    }

    padded = (uint8_t *)calloc((size_t)TECMO_CHR_CONFIG_BYTES, 1);
    if (padded == NULL) {
        tecmo_asset_pack_free(entry_bytes);
        return -1;
    }

    copy_size = entry_size < TECMO_CHR_CONFIG_BYTES ? entry_size : TECMO_CHR_CONFIG_BYTES;
    if (copy_size > 0U) {
        memcpy(padded, entry_bytes, (size_t)copy_size);
    }
    tecmo_asset_pack_free(entry_bytes);

    *bytes_out = padded;
    *byte_count = TECMO_CHR_CONFIG_BYTES;
    return 0;
}

static int load_chr_from_asset_pack(const char *project_root, uint8_t **bytes_out, uint64_t *byte_count)
{
    const char *env_path = getenv("TECMO_ASSETPACK");
    char path[TECMO_MAX_PATH_TEXT];

    if (env_path != NULL && env_path[0] != '\0' &&
        load_chr_from_asset_pack_path(env_path, bytes_out, byte_count) == 0) {
        return 0;
    }

    append_path(path, sizeof(path), project_root, "build\\tecmo.assetpack");
    normalize_separators(path);
    if (file_exists(path) && load_chr_from_asset_pack_path(path, bytes_out, byte_count) == 0) {
        return 0;
    }

    if (load_chr_from_asset_pack_path("build\\tecmo.assetpack", bytes_out, byte_count) == 0) {
        return 0;
    }

    return -1;
}

int tecmo_load_chr_data(const char *project_root, uint8_t **bytes_out, uint64_t *byte_count)
{
    if (load_chr_from_asset_pack(project_root, bytes_out, byte_count) == 0) {
        return 0;
    }
    return load_chr_padded(project_root, bytes_out, byte_count);
}

void tecmo_free_buffer(void *buffer)
{
    free(buffer);
}

int tecmo_analyze_chr(const char *project_root, uint64_t *byte_count)
{
    char path[TECMO_MAX_PATH_TEXT];
    append_path(path, sizeof(path), project_root, "build\\baseline\\Tiles.asm");
    normalize_separators(path);
    return convert_tiles_file(path, NULL, byte_count);
}

int tecmo_export_chr(const char *project_root, const char *out_path, uint64_t *bytes_written)
{
    char tiles_path[TECMO_MAX_PATH_TEXT];
    FILE *out;
    uint64_t explicit_bytes = 0;
    int result;

    append_path(tiles_path, sizeof(tiles_path), project_root, "build\\baseline\\Tiles.asm");
    normalize_separators(tiles_path);

    out = fopen(out_path, "wb");
    if (out == NULL) {
        return -1;
    }

    result = convert_tiles_file(tiles_path, out, &explicit_bytes);
    if (result == 0 && explicit_bytes < TECMO_CHR_CONFIG_BYTES) {
        uint64_t pad_count = TECMO_CHR_CONFIG_BYTES - explicit_bytes;
        for (uint64_t i = 0; i < pad_count; ++i) {
            if (fputc(0, out) == EOF) {
                result = -1;
                break;
            }
        }
    }
    if (fclose(out) != 0) {
        result = -1;
    }
    *bytes_written = result == 0 ? TECMO_CHR_CONFIG_BYTES : explicit_bytes;
    return result;
}

static void put_rgba(uint8_t *image, uint32_t width, uint32_t x, uint32_t y, const uint8_t color[4])
{
    size_t offset = ((size_t)y * width + x) * 4U;
    image[offset + 0] = color[0];
    image[offset + 1] = color[1];
    image[offset + 2] = color[2];
    image[offset + 3] = color[3];
}

static void render_chr_bank_sheet(const uint8_t *chr, uint32_t bank_index, uint8_t *image)
{
    static const uint8_t palette[4][4] = {
        {18, 18, 20, 255},
        {88, 96, 112, 255},
        {172, 184, 196, 255},
        {244, 246, 248, 255},
    };
    const uint32_t columns = 16;
    const uint32_t width = 128;
    const uint32_t tiles_per_bank = 512;
    uint64_t bank_offset = (uint64_t)bank_index * 8192ULL;

    memset(image, 0, (size_t)width * 256U * 4U);
    for (uint32_t tile = 0; tile < tiles_per_bank; ++tile) {
        uint64_t tile_offset = bank_offset + (uint64_t)tile * 16ULL;
        uint32_t tile_x = (tile % columns) * 8U;
        uint32_t tile_y = (tile / columns) * 8U;

        for (uint32_t row = 0; row < 8; ++row) {
            uint8_t plane0 = chr[tile_offset + row];
            uint8_t plane1 = chr[tile_offset + row + 8U];
            for (uint32_t col = 0; col < 8; ++col) {
                uint8_t bit = (uint8_t)(7U - col);
                uint8_t value = (uint8_t)(((plane0 >> bit) & 1U) | (((plane1 >> bit) & 1U) << 1U));
                put_rgba(image, width, tile_x + col, tile_y + row, palette[value]);
            }
        }
    }
}

int tecmo_export_chr_png_sheets(const char *project_root, const char *out_dir, uint64_t *sheets_written)
{
    uint8_t *chr = NULL;
    uint8_t *image = NULL;
    uint64_t chr_bytes = 0;
    uint64_t bank_count;

    *sheets_written = 0;
    if (ensure_directory_recursive(out_dir) != 0) {
        return -1;
    }
    if (load_chr_padded(project_root, &chr, &chr_bytes) != 0) {
        return -1;
    }

    image = (uint8_t *)malloc(128U * 256U * 4U);
    if (image == NULL) {
        free(chr);
        return -1;
    }

    bank_count = chr_bytes / 8192ULL;
    for (uint64_t bank = 0; bank < bank_count; ++bank) {
        char file_name[64];
        char out_path[TECMO_MAX_PATH_TEXT];
        (void)snprintf(file_name, sizeof(file_name), "chr_bank%02llu.png", (unsigned long long)bank);
        append_path(out_path, sizeof(out_path), out_dir, file_name);
        normalize_separators(out_path);
        render_chr_bank_sheet(chr, (uint32_t)bank, image);
        if (png_write_rgba8(out_path, image, 128, 256) != 0) {
            free(image);
            free(chr);
            return -1;
        }
        ++(*sheets_written);
    }

    free(image);
    free(chr);
    return 0;
}

static void print_stats_line(const char *name, const AsmStats *stats)
{
    printf("%-34s files=%4llu  lines=%7llu  labels=%5llu  semantic=%5llu  instr=%6llu  data_bytes=%7llu\n",
           name,
           (unsigned long long)stats->files,
           (unsigned long long)stats->lines,
           (unsigned long long)stats->labels,
           (unsigned long long)stats->semantic_labels,
           (unsigned long long)stats->instructions,
           (unsigned long long)stats->emitted_bytes);
}

void tecmo_print_summary(const char *project_root)
{
    AsmStats baseline_total;
    AsmStats lifted_total;
    AsmStats contracts_total;
    uint64_t chr_bytes = 0;
    RosterTable roster;

    memset(&baseline_total, 0, sizeof(baseline_total));
    for (size_t i = 0; i < sizeof(BASELINE_FILES) / sizeof(BASELINE_FILES[0]); ++i) {
        char path[TECMO_MAX_PATH_TEXT];
        AsmStats stats;
        append_path(path, sizeof(path), project_root, "build\\baseline");
        append_path(path, sizeof(path), path, BASELINE_FILES[i]);
        normalize_separators(path);
        if (asm_scan_file(path, &stats) == 0) {
            asm_stats_add(&baseline_total, &stats);
        }
    }

    (void)asm_scan_tree(project_root, "decomp\\lifted", ".asm", &lifted_total);
    (void)asm_scan_tree(project_root, "decomp\\contracts", ".md", &contracts_total);
    (void)tecmo_analyze_chr(project_root, &chr_bytes);

    printf("Tecmo Basketball C port inventory\n");
    printf("Project root: %s\n\n", project_root);

    print_stats_line("baseline banks/assets", &baseline_total);
    print_stats_line("lifted ASM chunks", &lifted_total);
    print_stats_line("contracts/docs", &contracts_total);
    printf("\n");

    if (tecmo_collect_rosters(project_root, &roster) == 0) {
        printf("Roster records parsed: %llu\n", (unsigned long long)roster.count);
        roster_table_free(&roster);
    } else {
        printf("Roster records parsed: 0\n");
    }

    printf("CHR bytes parsed from Tiles.asm: %llu explicit", (unsigned long long)chr_bytes);
    if (chr_bytes > 0) {
        printf(", %llu after linker fill (%llu NES 8x8 2bpp tiles, %llu x 8KB CHR banks)",
               (unsigned long long)TECMO_CHR_CONFIG_BYTES,
               (unsigned long long)(TECMO_CHR_CONFIG_BYTES / 16U),
               (unsigned long long)(TECMO_CHR_CONFIG_BYTES / 8192U));
    }
    printf("\n");
}

void tecmo_print_banks(const char *project_root)
{
    printf("Baseline PRG bank scan\n");
    for (size_t i = 0; i < sizeof(PRG_BANK_FILES) / sizeof(PRG_BANK_FILES[0]); ++i) {
        char path[TECMO_MAX_PATH_TEXT];
        AsmStats stats;
        append_path(path, sizeof(path), project_root, "build\\baseline");
        append_path(path, sizeof(path), path, PRG_BANK_FILES[i]);
        normalize_separators(path);
        if (asm_scan_file(path, &stats) == 0) {
            print_stats_line(PRG_BANK_FILES[i], &stats);
        } else {
            printf("%-34s missing\n", PRG_BANK_FILES[i]);
        }
    }
}

typedef struct ChunkGroupContext {
    uint64_t counts[8];
    uint64_t total;
} ChunkGroupContext;

static int bank_from_path(const char *path)
{
    const char *p = find_ci(path, "bank");
    while (p != NULL) {
        if (isdigit((unsigned char)p[4]) && isdigit((unsigned char)p[5])) {
            int value = (p[4] - '0') * 10 + (p[5] - '0');
            if (value >= 0 && value <= 7) {
                return value;
            }
        }
        p = find_ci(p + 4, "bank");
    }
    return -1;
}

static int chunk_group_visitor(const char *path, void *context)
{
    ChunkGroupContext *ctx = (ChunkGroupContext *)context;
    int bank = bank_from_path(path);
    ++ctx->total;
    if (bank >= 0 && bank <= 7) {
        ++ctx->counts[bank];
    }
    return 0;
}

static int print_chunk_visitor(const char *path, void *context)
{
    unsigned *printed = (unsigned *)context;
    FILE *file;
    char first_line[LINE_BUFFER_SIZE];
    char display_path[TECMO_MAX_PATH_TEXT];

    if (*printed >= 24U) {
        return 0;
    }

    copy_text(display_path, sizeof(display_path), path);
    file = fopen(path, "rb");
    if (file != NULL && fgets(first_line, sizeof(first_line), file) != NULL) {
        right_trim(first_line);
        printf("  %s\n    %s\n", display_path, first_line);
    } else {
        printf("  %s\n", display_path);
    }
    if (file != NULL) {
        fclose(file);
    }
    ++(*printed);
    return 0;
}

void tecmo_print_chunks(const char *project_root)
{
    char lifted_path[TECMO_MAX_PATH_TEXT];
    ChunkGroupContext context;
    unsigned printed = 0;

    memset(&context, 0, sizeof(context));
    append_path(lifted_path, sizeof(lifted_path), project_root, "decomp\\lifted");
    normalize_separators(lifted_path);
    (void)visit_files_recursive(lifted_path, ".asm", chunk_group_visitor, &context);

    printf("Lifted chunk files: %llu\n", (unsigned long long)context.total);
    for (int i = 0; i < 8; ++i) {
        printf("  bank%02d: %llu\n", i, (unsigned long long)context.counts[i]);
    }
    printf("\nFirst lifted chunks discovered:\n");
    (void)visit_files_recursive(lifted_path, ".asm", print_chunk_visitor, &printed);
    if (context.total > printed) {
        printf("  ... %llu more\n", (unsigned long long)(context.total - printed));
    }
}

static unsigned bcd_to_number(uint8_t raw)
{
    return (unsigned)(((raw >> 4U) * 10U) + (raw & 0x0FU));
}

static double pct_value(uint8_t raw)
{
    return (double)raw * 0.4;
}

static int team_matches(const char *team, const char *filter)
{
    char team_norm[TECMO_MAX_NAME_TEXT];
    char filter_norm[TECMO_MAX_NAME_TEXT];

    if (filter == NULL || filter[0] == '\0' || equals_ci(filter, "--all")) {
        return 1;
    }

    copy_text(team_norm, sizeof(team_norm), team);
    copy_text(filter_norm, sizeof(filter_norm), filter);
    for (char *p = team_norm; *p != '\0'; ++p) {
        if (*p == '_') {
            *p = ' ';
        }
    }
    for (char *p = filter_norm; *p != '\0'; ++p) {
        if (*p == '_') {
            *p = ' ';
        }
    }
    return equals_ci(team_norm, filter_norm);
}

void tecmo_print_roster(const char *project_root, const char *team_filter)
{
    RosterTable table;
    char current_team[TECMO_MAX_NAME_TEXT] = "";
    size_t shown = 0;

    if (tecmo_collect_rosters(project_root, &table) != 0) {
        printf("No roster records found.\n");
        return;
    }

    for (size_t i = 0; i < table.count; ++i) {
        RosterRecord *rec = &table.records[i];
        if (!team_matches(rec->team, team_filter)) {
            continue;
        }
        if (strcmp(current_team, rec->team) != 0) {
            copy_text(current_team, sizeof(current_team), rec->team);
            printf("\n%s\n", current_team);
        }
        if (rec->has_attrs) {
            printf("  %-22s id=$%02X  #%u  %u'%u\"  %u lbs  FG %.1f  FT %.1f  3PT %.1f\n",
                   rec->player,
                   (unsigned)rec->attrs[0],
                   bcd_to_number(rec->attrs[1]),
                   (unsigned)(rec->attrs[2] >> 4U),
                   (unsigned)(rec->attrs[2] & 0x0FU),
                   (unsigned)rec->attrs[3] * 4U,
                   pct_value(rec->attrs[4]),
                   pct_value(rec->attrs[5]),
                   pct_value(rec->attrs[6]));
        } else {
            printf("  %-22s attrs pending\n", rec->player);
        }
        ++shown;
    }

    if (shown == 0) {
        printf("No roster records matched '%s'. Try CHICAGO or --all.\n", team_filter == NULL ? "" : team_filter);
    } else {
        printf("\nRecords shown: %llu of %llu parsed\n",
               (unsigned long long)shown,
               (unsigned long long)table.count);
    }

    roster_table_free(&table);
}

void tecmo_print_assets(const char *project_root)
{
    uint64_t chr_bytes = 0;
    if (tecmo_analyze_chr(project_root, &chr_bytes) != 0) {
        printf("Could not parse build\\baseline\\Tiles.asm\n");
        return;
    }

    printf("CHR asset scan\n");
    printf("  source          build\\baseline\\Tiles.asm\n");
    printf("  explicit bytes  %llu\n", (unsigned long long)chr_bytes);
    printf("  linker fill     %llu zero bytes from tecmo.cfg\n",
           (unsigned long long)(chr_bytes < TECMO_CHR_CONFIG_BYTES ? TECMO_CHR_CONFIG_BYTES - chr_bytes : 0));
    printf("  exported bytes  %llu\n", (unsigned long long)TECMO_CHR_CONFIG_BYTES);
    printf("  NES tiles       %llu (16 bytes each, 8x8 2bpp)\n",
           (unsigned long long)(TECMO_CHR_CONFIG_BYTES / 16U));
    printf("  8KB CHR banks   %llu\n", (unsigned long long)(TECMO_CHR_CONFIG_BYTES / 8192U));
    printf("  note            baseline Tiles.asm is raw CHR data; semantic sprite/tile grouping must come from pattern/OAM/layout tables in lifted banks.\n");
}
