#define _CRT_SECURE_NO_WARNINGS

#include "asm_inventory.h"
#include "png_writer.h"

#include <ctype.h>
#include <errno.h>
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

int tecmo_collect_rosters(const char *project_root, RosterTable *table)
{
    memset(table, 0, sizeof(*table));
    for (size_t i = 0; i < sizeof(ROSTER_FILES) / sizeof(ROSTER_FILES[0]); ++i) {
        char path[TECMO_MAX_PATH_TEXT];
        append_path(path, sizeof(path), project_root, ROSTER_FILES[i]);
        normalize_separators(path);
        (void)parse_roster_file(path, table);
    }
    return table->count > 0 ? 0 : -1;
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

int tecmo_load_original_title_text(const char *project_root, char *title, size_t title_size)
{
    char path[TECMO_MAX_PATH_TEXT];
    char line[LINE_BUFFER_SIZE];
    FILE *file;
    size_t out_count = 0;

    if (title == NULL || title_size == 0) {
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

int tecmo_load_original_title_glyphs(const char *project_root, TecmoOriginalTitleGlyphs *glyphs)
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

    if (tecmo_load_original_title_text(project_root, glyphs->title_text, sizeof(glyphs->title_text)) != 0) {
        return -1;
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
    if (load_title_setup_summary(bank04, bank04_present, &glyphs->setup_summary) != 0) {
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

int tecmo_load_chr_data(const char *project_root, uint8_t **bytes_out, uint64_t *byte_count)
{
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
