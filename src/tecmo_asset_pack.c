#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_asset_pack.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TECMO_ASSET_PACK_VERSION 1U
#define TECMO_ASSET_PACK_HEADER_SIZE 40U
#define TECMO_ASSET_PACK_ENTRY_SIZE 128U
#define TECMO_ASSET_PACK_PRG_BANK_BYTES 0x4000ULL
#define TECMO_ASSET_PACK_CHR_BANK_BYTES 0x2000ULL

typedef struct TecmoAssetPackBuildEntry {
    char id[TECMO_ASSET_PACK_ID_SIZE];
    uint32_t type;
    uint32_t bank;
    uint32_t cpu_address;
    uint64_t source_offset;
    uint64_t pack_offset;
    uint64_t size;
    uint32_t flags;
} TecmoAssetPackBuildEntry;

typedef struct TecmoAssetPackDirectoryEntry {
    char id[TECMO_ASSET_PACK_ID_SIZE];
    uint32_t type;
    uint32_t bank;
    uint32_t cpu_address;
    uint64_t source_offset;
    uint64_t pack_offset;
    uint64_t size;
    uint32_t flags;
} TecmoAssetPackDirectoryEntry;

struct TecmoAssetPackBuilder {
    FILE *out;
    char *out_path;
    TecmoAssetPackBuildEntry *entries;
    size_t entry_count;
    size_t entry_capacity;
};

static void set_message(char *message, size_t message_size, const char *text)
{
    if (message == NULL || message_size == 0U) {
        return;
    }
    if (text == NULL) {
        text = "";
    }
    (void)snprintf(message, message_size, "%s", text);
}

static void set_messagef(char *message, size_t message_size, const char *format, ...)
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

static char *copy_string(const char *text)
{
    size_t length;
    char *copy;

    if (text == NULL) {
        text = "";
    }

    length = strlen(text);
    copy = (char *)malloc(length + 1U);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, length + 1U);
    return copy;
}

static int file_tell_u64(FILE *file, uint64_t *position_out)
{
#ifdef _WIN32
    __int64 position = _ftelli64(file);
    if (position < 0) {
        return -1;
    }
    *position_out = (uint64_t)position;
#else
    long position = ftell(file);
    if (position < 0) {
        return -1;
    }
    *position_out = (uint64_t)position;
#endif
    return 0;
}

static int file_seek_u64(FILE *file, uint64_t position)
{
#ifdef _WIN32
    if (position > 0x7FFFFFFFFFFFFFFFULL) {
        return -1;
    }
    return _fseeki64(file, (__int64)position, SEEK_SET);
#else
    if (position > (uint64_t)LONG_MAX) {
        return -1;
    }
    return fseek(file, (long)position, SEEK_SET);
#endif
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

static int write_u32(FILE *file, uint32_t value)
{
    uint8_t bytes[4];

    bytes[0] = (uint8_t)(value & 0xFFU);
    bytes[1] = (uint8_t)((value >> 8U) & 0xFFU);
    bytes[2] = (uint8_t)((value >> 16U) & 0xFFU);
    bytes[3] = (uint8_t)((value >> 24U) & 0xFFU);
    return fwrite(bytes, 1U, sizeof(bytes), file) == sizeof(bytes) ? 0 : -1;
}

static int write_u64(FILE *file, uint64_t value)
{
    uint8_t bytes[8];

    for (size_t i = 0; i < sizeof(bytes); ++i) {
        bytes[i] = (uint8_t)((value >> (unsigned)(i * 8U)) & 0xFFU);
    }
    return fwrite(bytes, 1U, sizeof(bytes), file) == sizeof(bytes) ? 0 : -1;
}

static uint32_t read_u32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0]) |
           ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) |
           ((uint32_t)bytes[3] << 24U);
}

static uint64_t read_u64(const uint8_t *bytes)
{
    uint64_t value = 0;

    for (size_t i = 0; i < 8U; ++i) {
        value |= (uint64_t)bytes[i] << (unsigned)(i * 8U);
    }
    return value;
}

static int read_pack_header(FILE *file,
                            uint32_t *entry_count_out,
                            uint64_t *directory_offset_out)
{
    uint8_t header[TECMO_ASSET_PACK_HEADER_SIZE];
    uint32_t version;
    uint32_t header_size;
    uint32_t entry_size;
    uint32_t entry_count;
    uint64_t directory_offset;

    if (file == NULL || entry_count_out == NULL || directory_offset_out == NULL) {
        return -1;
    }

    if (fread(header, 1U, sizeof(header), file) != sizeof(header) ||
        memcmp(header, "TAP1", 4U) != 0) {
        return -1;
    }

    version = read_u32(header + 4U);
    header_size = read_u32(header + 8U);
    entry_size = read_u32(header + 12U);
    entry_count = read_u32(header + 16U);
    directory_offset = read_u64(header + 20U);
    if (version != TECMO_ASSET_PACK_VERSION ||
        header_size != TECMO_ASSET_PACK_HEADER_SIZE ||
        entry_size != TECMO_ASSET_PACK_ENTRY_SIZE) {
        return -1;
    }

    *entry_count_out = entry_count;
    *directory_offset_out = directory_offset;
    return 0;
}

static int write_header(FILE *file,
                        uint32_t entry_count,
                        uint64_t directory_offset,
                        uint64_t data_offset)
{
    if (fwrite("TAP1", 1U, 4U, file) != 4U ||
        write_u32(file, TECMO_ASSET_PACK_VERSION) != 0 ||
        write_u32(file, TECMO_ASSET_PACK_HEADER_SIZE) != 0 ||
        write_u32(file, TECMO_ASSET_PACK_ENTRY_SIZE) != 0 ||
        write_u32(file, entry_count) != 0 ||
        write_u64(file, directory_offset) != 0 ||
        write_u64(file, data_offset) != 0 ||
        write_u32(file, 0U) != 0) {
        return -1;
    }
    return 0;
}

static int write_directory_entry(FILE *file, const TecmoAssetPackBuildEntry *entry)
{
    uint8_t padding[20];

    memset(padding, 0, sizeof(padding));
    if (fwrite(entry->id, 1U, TECMO_ASSET_PACK_ID_SIZE, file) != TECMO_ASSET_PACK_ID_SIZE ||
        write_u32(file, entry->type) != 0 ||
        write_u32(file, entry->bank) != 0 ||
        write_u32(file, entry->cpu_address) != 0 ||
        write_u64(file, entry->source_offset) != 0 ||
        write_u64(file, entry->pack_offset) != 0 ||
        write_u64(file, entry->size) != 0 ||
        write_u32(file, entry->flags) != 0 ||
        write_u32(file, 0U) != 0 ||
        fwrite(padding, 1U, sizeof(padding), file) != sizeof(padding)) {
        return -1;
    }
    return 0;
}

static int read_directory_entry(FILE *file, TecmoAssetPackDirectoryEntry *entry)
{
    uint8_t bytes[TECMO_ASSET_PACK_ENTRY_SIZE];

    if (fread(bytes, 1U, sizeof(bytes), file) != sizeof(bytes)) {
        return -1;
    }

    memset(entry, 0, sizeof(*entry));
    memcpy(entry->id, bytes, TECMO_ASSET_PACK_ID_SIZE);
    entry->id[TECMO_ASSET_PACK_ID_SIZE - 1U] = '\0';
    entry->type = read_u32(bytes + 64U);
    entry->bank = read_u32(bytes + 68U);
    entry->cpu_address = read_u32(bytes + 72U);
    entry->source_offset = read_u64(bytes + 76U);
    entry->pack_offset = read_u64(bytes + 84U);
    entry->size = read_u64(bytes + 92U);
    entry->flags = read_u32(bytes + 100U);
    return 0;
}

static TecmoAssetPackEntryInfo make_entry_info(const char *id,
                                               uint32_t type,
                                               uint32_t bank,
                                               uint32_t cpu_address,
                                               uint64_t source_offset,
                                               uint32_t flags)
{
    TecmoAssetPackEntryInfo entry_info;

    entry_info.id = id;
    entry_info.type = type;
    entry_info.bank = bank;
    entry_info.cpu_address = cpu_address;
    entry_info.source_offset = source_offset;
    entry_info.flags = flags;
    return entry_info;
}

static void release_builder(TecmoAssetPackBuilder *builder)
{
    if (builder == NULL) {
        return;
    }
    if (builder->out != NULL) {
        fclose(builder->out);
    }
    free(builder->out_path);
    free(builder->entries);
    free(builder);
}

static int reserve_build_entry(TecmoAssetPackBuilder *builder, char *message, size_t message_size)
{
    size_t new_capacity;
    TecmoAssetPackBuildEntry *new_entries;

    if (builder->entry_count >= UINT32_MAX) {
        set_message(message, message_size, "Asset pack directory entry count exceeds format limits.");
        return -1;
    }
    if (builder->entry_count < builder->entry_capacity) {
        return 0;
    }

    if (builder->entry_capacity > SIZE_MAX / 2U) {
        set_message(message, message_size, "Asset pack directory capacity overflow.");
        return -1;
    }
    new_capacity = builder->entry_capacity == 0U ? 32U : builder->entry_capacity * 2U;
    if (new_capacity > SIZE_MAX / sizeof(builder->entries[0])) {
        set_message(message, message_size, "Asset pack directory allocation is too large.");
        return -1;
    }

    new_entries = (TecmoAssetPackBuildEntry *)realloc(builder->entries,
                                                      new_capacity * sizeof(builder->entries[0]));
    if (new_entries == NULL) {
        set_message(message, message_size, "Out of memory for asset pack directory.");
        return -1;
    }

    builder->entries = new_entries;
    builder->entry_capacity = new_capacity;
    return 0;
}

static int validate_entry_info(const TecmoAssetPackBuilder *builder,
                               const TecmoAssetPackEntryInfo *entry_info,
                               char *message,
                               size_t message_size)
{
    if (builder == NULL || builder->out == NULL) {
        set_message(message, message_size, "Asset pack builder is not open.");
        return -1;
    }
    if (entry_info == NULL || entry_info->id == NULL || entry_info->id[0] == '\0') {
        set_message(message, message_size, "Asset pack entry id is required.");
        return -1;
    }
    if (strlen(entry_info->id) >= TECMO_ASSET_PACK_ID_SIZE) {
        set_message(message, message_size, "Asset pack entry id is too long.");
        return -1;
    }
    return 0;
}

static int append_build_entry(TecmoAssetPackBuilder *builder,
                              const TecmoAssetPackEntryInfo *entry_info,
                              uint64_t pack_offset,
                              uint64_t byte_count,
                              char *message,
                              size_t message_size)
{
    TecmoAssetPackBuildEntry *entry;

    if (reserve_build_entry(builder, message, message_size) != 0) {
        return -1;
    }

    entry = &builder->entries[builder->entry_count];
    memset(entry, 0, sizeof(*entry));
    (void)snprintf(entry->id, sizeof(entry->id), "%s", entry_info->id);
    entry->type = entry_info->type;
    entry->bank = entry_info->bank;
    entry->cpu_address = entry_info->cpu_address;
    entry->source_offset = entry_info->source_offset;
    entry->pack_offset = pack_offset;
    entry->size = byte_count;
    entry->flags = entry_info->flags;
    ++builder->entry_count;
    return 0;
}

static int write_memory_payload(FILE *out, const void *data, uint64_t byte_count)
{
    const uint8_t *cursor = (const uint8_t *)data;
    uint64_t remaining = byte_count;

    if (byte_count > 0U && data == NULL) {
        return -1;
    }

    while (remaining > 0U) {
        size_t chunk = remaining > 65536U ? 65536U : (size_t)remaining;
        if (fwrite(cursor, 1U, chunk, out) != chunk) {
            return -1;
        }
        cursor += chunk;
        remaining -= (uint64_t)chunk;
    }
    return 0;
}

int tecmo_asset_pack_builder_begin(TecmoAssetPackBuilder **builder_out,
                                   const char *out_path,
                                   char *message,
                                   size_t message_size)
{
    TecmoAssetPackBuilder *builder;

    if (builder_out == NULL) {
        set_message(message, message_size, "Asset pack builder output is required.");
        return -1;
    }
    *builder_out = NULL;

    if (out_path == NULL || out_path[0] == '\0') {
        set_message(message, message_size, "Asset pack output path is required.");
        return -1;
    }

    builder = (TecmoAssetPackBuilder *)calloc(1U, sizeof(*builder));
    if (builder == NULL) {
        set_message(message, message_size, "Out of memory for asset pack builder.");
        return -1;
    }

    builder->out_path = copy_string(out_path);
    if (builder->out_path == NULL) {
        set_message(message, message_size, "Out of memory for asset pack output path.");
        release_builder(builder);
        return -1;
    }

    builder->out = fopen(out_path, "wb");
    if (builder->out == NULL) {
        set_message(message, message_size, "Could not open asset pack output.");
        release_builder(builder);
        return -1;
    }
    if (write_header(builder->out, 0U, 0U, TECMO_ASSET_PACK_HEADER_SIZE) != 0) {
        set_message(message, message_size, "Could not write asset pack header.");
        release_builder(builder);
        return -1;
    }

    *builder_out = builder;
    return 0;
}

int tecmo_asset_pack_builder_add_memory(TecmoAssetPackBuilder *builder,
                                        const TecmoAssetPackEntryInfo *entry_info,
                                        const void *data,
                                        uint64_t byte_count,
                                        char *message,
                                        size_t message_size)
{
    uint64_t pack_offset;

    if (validate_entry_info(builder, entry_info, message, message_size) != 0) {
        return -1;
    }
    if (byte_count > 0U && data == NULL) {
        set_message(message, message_size, "Asset pack memory entry has no bytes.");
        return -1;
    }
    if (file_tell_u64(builder->out, &pack_offset) != 0) {
        set_message(message, message_size, "Could not locate asset pack data offset.");
        return -1;
    }
    if (write_memory_payload(builder->out, data, byte_count) != 0) {
        set_message(message, message_size, "Could not write asset pack memory entry.");
        return -1;
    }

    return append_build_entry(builder, entry_info, pack_offset, byte_count, message, message_size);
}

int tecmo_asset_pack_builder_add_file(TecmoAssetPackBuilder *builder,
                                      const TecmoAssetPackEntryInfo *entry_info,
                                      const char *local_path,
                                      char *message,
                                      size_t message_size)
{
    FILE *input;
    uint8_t buffer[16384];
    uint64_t pack_offset;
    uint64_t byte_count = 0U;
    size_t bytes_read;
    int result = -1;

    if (validate_entry_info(builder, entry_info, message, message_size) != 0) {
        return -1;
    }
    if (local_path == NULL || local_path[0] == '\0') {
        set_message(message, message_size, "Asset pack local file path is required.");
        return -1;
    }

    input = fopen(local_path, "rb");
    if (input == NULL) {
        set_message(message, message_size, "Could not read local asset pack entry file.");
        return -1;
    }

    if (file_tell_u64(builder->out, &pack_offset) != 0) {
        set_message(message, message_size, "Could not locate asset pack data offset.");
        goto cleanup;
    }

    while ((bytes_read = fread(buffer, 1U, sizeof(buffer), input)) > 0U) {
        if (UINT64_MAX - byte_count < (uint64_t)bytes_read) {
            set_message(message, message_size, "Asset pack local file is too large.");
            goto cleanup;
        }
        if (fwrite(buffer, 1U, bytes_read, builder->out) != bytes_read) {
            set_message(message, message_size, "Could not write local asset pack entry.");
            goto cleanup;
        }
        byte_count += (uint64_t)bytes_read;
    }

    if (ferror(input) != 0) {
        set_message(message, message_size, "Could not read local asset pack entry file.");
        goto cleanup;
    }

    if (append_build_entry(builder, entry_info, pack_offset, byte_count, message, message_size) != 0) {
        goto cleanup;
    }
    result = 0;

cleanup:
    fclose(input);
    return result;
}

int tecmo_asset_pack_builder_finish(TecmoAssetPackBuilder *builder,
                                    char *message,
                                    size_t message_size)
{
    uint64_t directory_offset;
    uint32_t entry_count;
    int result = -1;

    if (builder == NULL || builder->out == NULL) {
        set_message(message, message_size, "Asset pack builder is not open.");
        return -1;
    }
    if (builder->entry_count > UINT32_MAX) {
        set_message(message, message_size, "Asset pack directory entry count exceeds format limits.");
        goto cleanup;
    }

    if (file_tell_u64(builder->out, &directory_offset) != 0) {
        set_message(message, message_size, "Could not locate asset pack directory.");
        goto cleanup;
    }

    for (size_t i = 0; i < builder->entry_count; ++i) {
        if (write_directory_entry(builder->out, &builder->entries[i]) != 0) {
            set_message(message, message_size, "Could not write asset pack directory.");
            goto cleanup;
        }
    }

    entry_count = (uint32_t)builder->entry_count;
    if (file_seek_u64(builder->out, 0U) != 0 ||
        write_header(builder->out,
                     entry_count,
                     directory_offset,
                     TECMO_ASSET_PACK_HEADER_SIZE) != 0) {
        set_message(message, message_size, "Could not finalize asset pack header.");
        goto cleanup;
    }

    {
        FILE *out = builder->out;
        builder->out = NULL;
        if (fclose(out) != 0) {
            set_message(message, message_size, "Could not close asset pack output.");
            goto cleanup;
        }
    }

    if (message != NULL && message_size > 0U) {
        (void)snprintf(message,
                       message_size,
                       "Wrote %u entries to %s",
                       entry_count,
                       builder->out_path);
    }
    result = 0;

cleanup:
    release_builder(builder);
    return result;
}

void tecmo_asset_pack_builder_cancel(TecmoAssetPackBuilder *builder)
{
    release_builder(builder);
}

static int read_file(const char *path, uint8_t **bytes_out, uint64_t *size_out)
{
    FILE *file = fopen(path, "rb");
    long size;
    uint8_t *bytes;

    if (file == NULL) {
        return -1;
    }
    if (fseek(file, 0L, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }
    size = ftell(file);
    if (size < 0 || fseek(file, 0L, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }

    bytes = (uint8_t *)malloc((size_t)size);
    if (bytes == NULL && size > 0) {
        fclose(file);
        return -1;
    }
    if (size > 0 && fread(bytes, 1U, (size_t)size, file) != (size_t)size) {
        free(bytes);
        fclose(file);
        return -1;
    }
    fclose(file);

    *bytes_out = bytes;
    *size_out = (uint64_t)size;
    return 0;
}

static int append_source_map_entry(char *buffer,
                                   size_t capacity,
                                   size_t *length,
                                   int *first,
                                   const char *id,
                                   const char *kind,
                                   uint64_t source_offset,
                                   uint64_t size,
                                   uint32_t bank,
                                   uint32_t cpu_address)
{
    const char *prefix = *first != 0 ? "" : ",\n";

    *first = 0;
    return append_text(buffer,
                       capacity,
                       length,
                       "%s"
                       "    {\"id\":\"%s\",\"kind\":\"%s\",\"source_offset\":%llu,"
                       "\"size\":%llu,\"bank\":%u,\"cpu_address\":%u}",
                       prefix,
                       id,
                       kind,
                       (unsigned long long)source_offset,
                       (unsigned long long)size,
                       bank,
                       cpu_address);
}

static char *build_ines_source_map(uint32_t mapper,
                                   uint32_t trainer_bytes,
                                   uint32_t prg_banks,
                                   uint32_t chr_banks,
                                   uint64_t prg_offset,
                                   uint64_t chr_offset,
                                   uint64_t chr_size,
                                   size_t *source_map_size_out)
{
    size_t entry_count = (size_t)prg_banks + (size_t)chr_banks + 2U;
    size_t capacity;
    size_t length = 0U;
    char *source_map;
    int first = 1;

    if (entry_count > (SIZE_MAX - 2048U) / 192U) {
        return NULL;
    }
    capacity = 2048U + entry_count * 192U;
    source_map = (char *)malloc(capacity);
    if (source_map == NULL) {
        return NULL;
    }

    if (append_text(source_map,
                    capacity,
                    &length,
                    "{\n"
                    "  \"format\":\"tecmo.assetpack.source-map/1\",\n"
                    "  \"source\":{\n"
                    "    \"kind\":\"ines\",\n"
                    "    \"mapper\":%u,\n"
                    "    \"trainer_bytes\":%u,\n"
                    "    \"prg_offset\":%llu,\n"
                    "    \"prg_bank_bytes\":%llu,\n"
                    "    \"prg_banks\":%u,\n"
                    "    \"chr_offset\":%llu,\n"
                    "    \"chr_bank_bytes\":%llu,\n"
                    "    \"chr_banks\":%u\n"
                    "  },\n"
                    "  \"raw_entries\":[\n",
                    mapper,
                    trainer_bytes,
                    (unsigned long long)prg_offset,
                    (unsigned long long)TECMO_ASSET_PACK_PRG_BANK_BYTES,
                    prg_banks,
                    (unsigned long long)chr_offset,
                    (unsigned long long)TECMO_ASSET_PACK_CHR_BANK_BYTES,
                    chr_banks) != 0) {
        free(source_map);
        return NULL;
    }

    for (uint32_t bank = 0; bank < prg_banks; ++bank) {
        char id[TECMO_ASSET_PACK_ID_SIZE];
        uint64_t offset = prg_offset + (uint64_t)bank * TECMO_ASSET_PACK_PRG_BANK_BYTES;

        (void)snprintf(id, sizeof(id), "prg/bank%02u", bank);
        if (append_source_map_entry(source_map,
                                    capacity,
                                    &length,
                                    &first,
                                    id,
                                    "raw-prg-bank",
                                    offset,
                                    TECMO_ASSET_PACK_PRG_BANK_BYTES,
                                    bank,
                                    0x8000U) != 0) {
            free(source_map);
            return NULL;
        }
    }

    {
        uint32_t fixed_bank = prg_banks - 1U;
        uint64_t offset = prg_offset + (uint64_t)fixed_bank * TECMO_ASSET_PACK_PRG_BANK_BYTES;

        if (append_source_map_entry(source_map,
                                    capacity,
                                    &length,
                                    &first,
                                    "prg/fixed",
                                    "raw-prg-fixed-alias",
                                    offset,
                                    TECMO_ASSET_PACK_PRG_BANK_BYTES,
                                    fixed_bank,
                                    0xC000U) != 0) {
            free(source_map);
            return NULL;
        }
    }

    if (chr_size > 0U) {
        if (append_source_map_entry(source_map,
                                    capacity,
                                    &length,
                                    &first,
                                    "chr/all",
                                    "raw-chr-range",
                                    chr_offset,
                                    chr_size,
                                    0U,
                                    0U) != 0) {
            free(source_map);
            return NULL;
        }

        for (uint32_t bank = 0; bank < chr_banks; ++bank) {
            char id[TECMO_ASSET_PACK_ID_SIZE];
            uint64_t offset = chr_offset + (uint64_t)bank * TECMO_ASSET_PACK_CHR_BANK_BYTES;

            (void)snprintf(id, sizeof(id), "chr/bank%02u", bank);
            if (append_source_map_entry(source_map,
                                        capacity,
                                        &length,
                                        &first,
                                        id,
                                        "raw-chr-bank",
                                        offset,
                                        TECMO_ASSET_PACK_CHR_BANK_BYTES,
                                        bank,
                                        0U) != 0) {
                free(source_map);
                return NULL;
            }
        }
    }

    if (append_text(source_map,
                    capacity,
                    &length,
                    "\n"
                    "  ],\n"
                    "  \"logical_entries\":[],\n"
                    "  \"logical_entry_note\":\"reserved for decomp-derived named entries added by import tools\"\n"
                    "}\n") != 0) {
        free(source_map);
        return NULL;
    }

    *source_map_size_out = length;
    return source_map;
}

int tecmo_asset_pack_build_from_ines(const char *rom_path,
                                     const char *out_path,
                                     char *message,
                                     size_t message_size)
{
    uint8_t *rom = NULL;
    uint64_t rom_size = 0;
    uint64_t prg_offset;
    uint64_t prg_size;
    uint64_t chr_offset;
    uint64_t chr_size;
    uint32_t prg_banks;
    uint32_t chr_banks;
    uint32_t mapper;
    uint32_t trainer_bytes;
    TecmoAssetPackBuilder *builder = NULL;
    TecmoAssetPackEntryInfo entry_info;
    char manifest[512];
    char *source_map = NULL;
    size_t source_map_size = 0U;
    int manifest_length;
    int result = -1;

    if (rom_path == NULL || out_path == NULL) {
        set_message(message, message_size, "ROM path and output path are required.");
        return -1;
    }
    if (read_file(rom_path, &rom, &rom_size) != 0) {
        set_message(message, message_size, "Could not read iNES ROM.");
        return -1;
    }
    if (rom_size < 16U ||
        rom[0] != 'N' ||
        rom[1] != 'E' ||
        rom[2] != 'S' ||
        rom[3] != 0x1AU) {
        set_message(message, message_size, "Input is not an iNES ROM.");
        goto cleanup;
    }

    prg_banks = rom[4];
    chr_banks = rom[5];
    mapper = (uint32_t)(rom[6] >> 4U) | ((uint32_t)rom[7] & 0xF0U);
    trainer_bytes = (rom[6] & 0x04U) != 0U ? 512U : 0U;
    prg_offset = 16ULL + (uint64_t)trainer_bytes;
    prg_size = (uint64_t)prg_banks * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    chr_offset = prg_offset + prg_size;
    chr_size = (uint64_t)chr_banks * TECMO_ASSET_PACK_CHR_BANK_BYTES;

    if (prg_banks == 0U || rom_size < chr_offset + chr_size) {
        set_message(message, message_size, "ROM is shorter than its iNES PRG/CHR sizes.");
        goto cleanup;
    }

    if (tecmo_asset_pack_builder_begin(&builder, out_path, message, message_size) != 0) {
        goto cleanup;
    }

    manifest_length = snprintf(manifest,
                               sizeof(manifest),
                               "format=tecmo.assetpack/1\n"
                               "source=ines\n"
                               "source_map=system/source-map\n"
                               "mapper=%u\n"
                               "trainer_bytes=%u\n"
                               "prg_banks_16k=%u\n"
                               "chr_banks_8k=%u\n"
                               "raw_entry_prefixes=prg/,chr/\n"
                               "logical_entry_namespace=logical/\n"
                               "derived_entry_namespace=derived/\n",
                               mapper,
                               trainer_bytes,
                               prg_banks,
                               chr_banks);
    if (manifest_length < 0 || (size_t)manifest_length >= sizeof(manifest)) {
        set_message(message, message_size, "Could not build asset pack manifest.");
        goto cleanup;
    }

    entry_info = make_entry_info("system/manifest",
                                 TECMO_ASSET_PACK_TYPE_META,
                                 0U,
                                 0U,
                                 0U,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            manifest,
                                            (uint64_t)manifest_length,
                                            message,
                                            message_size) != 0) {
        set_message(message, message_size, "Could not write asset pack manifest.");
        goto cleanup;
    }

    source_map = build_ines_source_map(mapper,
                                       trainer_bytes,
                                       prg_banks,
                                       chr_banks,
                                       prg_offset,
                                       chr_offset,
                                       chr_size,
                                       &source_map_size);
    if (source_map == NULL) {
        set_message(message, message_size, "Could not build asset pack source map.");
        goto cleanup;
    }

    entry_info = make_entry_info("system/source-map",
                                 TECMO_ASSET_PACK_TYPE_META,
                                 0U,
                                 0U,
                                 0U,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            source_map,
                                            (uint64_t)source_map_size,
                                            message,
                                            message_size) != 0) {
        set_message(message, message_size, "Could not write asset pack source map.");
        goto cleanup;
    }
    free(source_map);
    source_map = NULL;

    for (uint32_t bank = 0; bank < prg_banks; ++bank) {
        char id[TECMO_ASSET_PACK_ID_SIZE];
        uint64_t offset = prg_offset + (uint64_t)bank * TECMO_ASSET_PACK_PRG_BANK_BYTES;

        (void)snprintf(id, sizeof(id), "prg/bank%02u", bank);
        entry_info = make_entry_info(id,
                                     TECMO_ASSET_PACK_TYPE_PRG,
                                     bank,
                                     0x8000U,
                                     offset,
                                     TECMO_ASSET_PACK_FLAG_RAW_ROM | TECMO_ASSET_PACK_FLAG_LOCAL);
        if (tecmo_asset_pack_builder_add_memory(builder,
                                                &entry_info,
                                                rom + (size_t)offset,
                                                TECMO_ASSET_PACK_PRG_BANK_BYTES,
                                                message,
                                                message_size) != 0) {
            set_message(message, message_size, "Could not write PRG bank asset.");
            goto cleanup;
        }
    }

    {
        uint32_t fixed_bank = prg_banks - 1U;
        uint64_t offset = prg_offset + (uint64_t)fixed_bank * TECMO_ASSET_PACK_PRG_BANK_BYTES;

        entry_info = make_entry_info("prg/fixed",
                                     TECMO_ASSET_PACK_TYPE_PRG,
                                     fixed_bank,
                                     0xC000U,
                                     offset,
                                     TECMO_ASSET_PACK_FLAG_RAW_ROM | TECMO_ASSET_PACK_FLAG_LOCAL);
        if (tecmo_asset_pack_builder_add_memory(builder,
                                                &entry_info,
                                                rom + (size_t)offset,
                                                TECMO_ASSET_PACK_PRG_BANK_BYTES,
                                                message,
                                                message_size) != 0) {
            set_message(message, message_size, "Could not write fixed PRG bank asset.");
            goto cleanup;
        }
    }

    if (chr_size > 0U) {
        entry_info = make_entry_info("chr/all",
                                     TECMO_ASSET_PACK_TYPE_CHR,
                                     0U,
                                     0U,
                                     chr_offset,
                                     TECMO_ASSET_PACK_FLAG_RAW_ROM | TECMO_ASSET_PACK_FLAG_LOCAL);
        if (tecmo_asset_pack_builder_add_memory(builder,
                                                &entry_info,
                                                rom + (size_t)chr_offset,
                                                chr_size,
                                                message,
                                                message_size) != 0) {
            set_message(message, message_size, "Could not write CHR asset.");
            goto cleanup;
        }

        for (uint32_t bank = 0; bank < chr_banks; ++bank) {
            char id[TECMO_ASSET_PACK_ID_SIZE];
            uint64_t offset = chr_offset + (uint64_t)bank * TECMO_ASSET_PACK_CHR_BANK_BYTES;

            (void)snprintf(id, sizeof(id), "chr/bank%02u", bank);
            entry_info = make_entry_info(id,
                                         TECMO_ASSET_PACK_TYPE_CHR,
                                         bank,
                                         0U,
                                         offset,
                                         TECMO_ASSET_PACK_FLAG_RAW_ROM | TECMO_ASSET_PACK_FLAG_LOCAL);
            if (tecmo_asset_pack_builder_add_memory(builder,
                                                    &entry_info,
                                                    rom + (size_t)offset,
                                                    TECMO_ASSET_PACK_CHR_BANK_BYTES,
                                                    message,
                                                    message_size) != 0) {
                set_message(message, message_size, "Could not write CHR bank asset.");
                goto cleanup;
            }
        }
    }

    {
        unsigned entry_count = 2U + prg_banks + 1U + (chr_size > 0U ? chr_banks + 1U : 0U);
        int finish_result = tecmo_asset_pack_builder_finish(builder, message, message_size);

        builder = NULL;
        if (finish_result != 0) {
            goto cleanup;
        }
        if (message != NULL && message_size > 0U) {
            (void)snprintf(message,
                           message_size,
                           "Wrote %u PRG banks, %u CHR banks, %u entries to %s",
                           prg_banks,
                           chr_banks,
                           entry_count,
                           out_path);
        }
    }
    result = 0;

cleanup:
    if (builder != NULL) {
        tecmo_asset_pack_builder_cancel(builder);
    }
    free(source_map);
    free(rom);
    return result;
}

int tecmo_asset_pack_list_entries(const char *pack_path,
                                  TecmoAssetPackListCallback callback,
                                  void *user_data)
{
    FILE *file;
    uint32_t entry_count;
    uint64_t directory_offset;
    int result = -1;

    if (pack_path == NULL || callback == NULL) {
        return -1;
    }

    file = fopen(pack_path, "rb");
    if (file == NULL) {
        return -1;
    }

    if (read_pack_header(file, &entry_count, &directory_offset) != 0 ||
        file_seek_u64(file, directory_offset) != 0) {
        goto cleanup;
    }

    for (uint32_t i = 0; i < entry_count; ++i) {
        TecmoAssetPackDirectoryEntry entry;
        TecmoAssetPackDirectoryEntryInfo entry_info;
        int callback_result;

        if (read_directory_entry(file, &entry) != 0) {
            goto cleanup;
        }

        entry_info.id = entry.id;
        entry_info.type = entry.type;
        entry_info.bank = entry.bank;
        entry_info.cpu_address = entry.cpu_address;
        entry_info.source_offset = entry.source_offset;
        entry_info.byte_count = entry.size;
        entry_info.flags = entry.flags;

        callback_result = callback(&entry_info, user_data);
        if (callback_result != 0) {
            result = callback_result;
            goto cleanup;
        }
    }

    result = 0;

cleanup:
    fclose(file);
    return result;
}

typedef struct TecmoAssetPackDirectoryDumpContext {
    char *buffer;
    size_t buffer_size;
    size_t length;
    size_t required;
    int overflow;
} TecmoAssetPackDirectoryDumpContext;

static void append_directory_dump_text(TecmoAssetPackDirectoryDumpContext *context,
                                       const char *text,
                                       size_t text_length)
{
    if (context == NULL || text == NULL) {
        return;
    }

    if (SIZE_MAX - context->required < text_length) {
        context->required = SIZE_MAX;
        context->overflow = 1;
    } else {
        context->required += text_length;
    }

    if (context->buffer != NULL &&
        context->buffer_size > 0U &&
        context->length < context->buffer_size - 1U) {
        size_t available = context->buffer_size - 1U - context->length;
        size_t copy_length = text_length < available ? text_length : available;

        if (copy_length > 0U) {
            memcpy(context->buffer + context->length, text, copy_length);
            context->length += copy_length;
        }
    }
}

static int append_directory_dump_entry(const TecmoAssetPackDirectoryEntryInfo *entry_info,
                                       void *user_data)
{
    TecmoAssetPackDirectoryDumpContext *context = (TecmoAssetPackDirectoryDumpContext *)user_data;
    char line[512];
    int written;

    if (entry_info == NULL || context == NULL) {
        return -1;
    }

    written = snprintf(line,
                       sizeof(line),
                       "%s\ttype=0x%08X\tbank=%u\tcpu=0x%04X\tsource=%llu\tbytes=%llu\tflags=0x%08X\n",
                       entry_info->id,
                       (unsigned int)entry_info->type,
                       (unsigned int)entry_info->bank,
                       (unsigned int)entry_info->cpu_address,
                       (unsigned long long)entry_info->source_offset,
                       (unsigned long long)entry_info->byte_count,
                       (unsigned int)entry_info->flags);
    if (written < 0) {
        context->overflow = 1;
        return 0;
    }
    if ((size_t)written >= sizeof(line)) {
        context->overflow = 1;
        return 0;
    }

    append_directory_dump_text(context, line, (size_t)written);
    return 0;
}

int tecmo_asset_pack_dump_directory(const char *pack_path,
                                    char *buffer,
                                    size_t buffer_size,
                                    size_t *required_size_out)
{
    static const char header[] = "id\ttype\tbank\tcpu_address\tsource_offset\tbyte_count\tflags\n";
    TecmoAssetPackDirectoryDumpContext context;
    size_t required_size;
    int list_result;

    if (pack_path == NULL || (buffer == NULL && buffer_size > 0U)) {
        return -1;
    }
    if (required_size_out != NULL) {
        *required_size_out = 0U;
    }

    memset(&context, 0, sizeof(context));
    context.buffer = buffer;
    context.buffer_size = buffer_size;
    if (buffer != NULL && buffer_size > 0U) {
        buffer[0] = '\0';
    }

    append_directory_dump_text(&context, header, sizeof(header) - 1U);
    list_result = tecmo_asset_pack_list_entries(pack_path, append_directory_dump_entry, &context);

    if (buffer != NULL && buffer_size > 0U) {
        buffer[context.length] = '\0';
    }

    if (list_result != 0 || context.overflow || SIZE_MAX - context.required < 1U) {
        if (required_size_out != NULL) {
            *required_size_out = SIZE_MAX;
        }
        return -1;
    }

    required_size = context.required + 1U;
    if (required_size_out != NULL) {
        *required_size_out = required_size;
    }
    if (buffer != NULL && buffer_size < required_size) {
        return -1;
    }

    return 0;
}

int tecmo_asset_pack_read_entry(const char *pack_path,
                                const char *entry_id,
                                uint8_t **bytes_out,
                                uint64_t *byte_count)
{
    FILE *file;
    uint32_t entry_count;
    uint64_t directory_offset;
    uint8_t *bytes = NULL;
    int result = -1;

    if (pack_path == NULL || entry_id == NULL || bytes_out == NULL || byte_count == NULL) {
        return -1;
    }
    *bytes_out = NULL;
    *byte_count = 0U;

    file = fopen(pack_path, "rb");
    if (file == NULL) {
        return -1;
    }

    if (read_pack_header(file, &entry_count, &directory_offset) != 0 ||
        file_seek_u64(file, directory_offset) != 0) {
        goto cleanup;
    }

    for (uint32_t i = 0; i < entry_count; ++i) {
        TecmoAssetPackDirectoryEntry entry;
        if (read_directory_entry(file, &entry) != 0) {
            goto cleanup;
        }
        if (strncmp(entry.id, entry_id, TECMO_ASSET_PACK_ID_SIZE) == 0) {
            if (entry.size > (uint64_t)SIZE_MAX) {
                goto cleanup;
            }
            bytes = (uint8_t *)malloc((size_t)entry.size);
            if (bytes == NULL && entry.size > 0U) {
                goto cleanup;
            }
            if (file_seek_u64(file, entry.pack_offset) != 0 ||
                (entry.size > 0U &&
                 fread(bytes, 1U, (size_t)entry.size, file) != (size_t)entry.size)) {
                free(bytes);
                bytes = NULL;
                goto cleanup;
            }
            *bytes_out = bytes;
            *byte_count = entry.size;
            bytes = NULL;
            result = 0;
            goto cleanup;
        }
    }

cleanup:
    free(bytes);
    fclose(file);
    return result;
}

static int make_self_test_path(char *path,
                               size_t path_size,
                               const char *suffix,
                               unsigned int index)
{
    const char *directory;
    const char *separator = "";
    size_t directory_length;
    unsigned long long tag;
    int written;

    if (path == NULL || path_size == 0U || suffix == NULL) {
        return -1;
    }

    directory = getenv("TMPDIR");
    if (directory == NULL || directory[0] == '\0') {
        directory = getenv("TEMP");
    }
    if (directory == NULL || directory[0] == '\0') {
        directory = getenv("TMP");
    }
    if (directory == NULL || directory[0] == '\0') {
        directory = ".";
    }

    directory_length = strlen(directory);
    if (directory_length > 0U &&
        directory[directory_length - 1U] != '/' &&
        directory[directory_length - 1U] != '\\') {
        separator = "/";
    }

    tag = (unsigned long long)time(NULL) ^
          (unsigned long long)(uintptr_t)path ^
          (unsigned long long)index;
    written = snprintf(path,
                       path_size,
                       "%s%stecmo_asset_pack_self_test_%llu_%u_%s",
                       directory,
                       separator,
                       tag,
                       index,
                       suffix);
    if (written < 0 || (size_t)written >= path_size) {
        return -1;
    }
    return 0;
}

static void remove_self_test_path(const char *path)
{
    if (path != NULL && path[0] != '\0') {
        (void)remove(path);
    }
}

static int write_self_test_file(const char *path, const uint8_t *bytes, size_t byte_count)
{
    FILE *file;
    int result = -1;

    if (path == NULL || (bytes == NULL && byte_count > 0U)) {
        return -1;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        return -1;
    }
    if (byte_count == 0U || fwrite(bytes, 1U, byte_count, file) == byte_count) {
        result = 0;
    }
    if (fclose(file) != 0) {
        result = -1;
    }
    return result;
}

static int bytes_contain_text(const uint8_t *bytes, uint64_t byte_count, const char *needle)
{
    size_t haystack_size;
    size_t needle_size;

    if (bytes == NULL || needle == NULL || byte_count > (uint64_t)SIZE_MAX) {
        return 0;
    }

    haystack_size = (size_t)byte_count;
    needle_size = strlen(needle);
    if (needle_size == 0U) {
        return 1;
    }
    if (needle_size > haystack_size) {
        return 0;
    }

    for (size_t i = 0; i <= haystack_size - needle_size; ++i) {
        if (memcmp(bytes + i, needle, needle_size) == 0) {
            return 1;
        }
    }
    return 0;
}

static int self_test_read_and_compare(const char *pack_path,
                                      const char *entry_id,
                                      const uint8_t *expected,
                                      uint64_t expected_size,
                                      char *message,
                                      size_t message_size)
{
    uint8_t *bytes = NULL;
    uint64_t byte_count = 0U;
    int result = -1;

    if (tecmo_asset_pack_read_entry(pack_path, entry_id, &bytes, &byte_count) != 0) {
        set_messagef(message, message_size, "Self-test could not read entry '%s'.", entry_id);
        return -1;
    }
    if (byte_count != expected_size ||
        (expected_size > 0U && memcmp(bytes, expected, (size_t)expected_size) != 0)) {
        set_messagef(message, message_size, "Self-test readback mismatch for entry '%s'.", entry_id);
        goto cleanup;
    }

    result = 0;

cleanup:
    tecmo_asset_pack_free(bytes);
    return result;
}

static int self_test_entry_contains_text(const char *pack_path,
                                         const char *entry_id,
                                         const char *needle,
                                         char *message,
                                         size_t message_size)
{
    uint8_t *bytes = NULL;
    uint64_t byte_count = 0U;
    int result = -1;

    if (tecmo_asset_pack_read_entry(pack_path, entry_id, &bytes, &byte_count) != 0) {
        set_messagef(message, message_size, "Self-test could not read entry '%s'.", entry_id);
        return -1;
    }
    if (!bytes_contain_text(bytes, byte_count, needle)) {
        set_messagef(message,
                     message_size,
                     "Self-test entry '%s' did not contain '%s'.",
                     entry_id,
                     needle);
        goto cleanup;
    }

    result = 0;

cleanup:
    tecmo_asset_pack_free(bytes);
    return result;
}

typedef struct TecmoAssetPackSelfTestBuilderListState {
    unsigned int count;
    int saw_memory;
    int saw_file;
    uint64_t memory_size;
    uint64_t file_size;
    char *message;
    size_t message_size;
} TecmoAssetPackSelfTestBuilderListState;

static int self_test_builder_list_entry(const TecmoAssetPackDirectoryEntryInfo *entry_info,
                                        void *user_data)
{
    TecmoAssetPackSelfTestBuilderListState *state =
        (TecmoAssetPackSelfTestBuilderListState *)user_data;

    if (entry_info == NULL || state == NULL) {
        return -1;
    }

    ++state->count;
    if (strcmp(entry_info->id, "test/memory") == 0) {
        if (entry_info->type != TECMO_ASSET_PACK_TYPE_DATA ||
            entry_info->bank != 7U ||
            entry_info->cpu_address != 0x8123U ||
            entry_info->source_offset != 0x1234ULL ||
            entry_info->byte_count != state->memory_size ||
            entry_info->flags != TECMO_ASSET_PACK_FLAG_DERIVED) {
            set_message(state->message,
                        state->message_size,
                        "Self-test memory directory metadata mismatch.");
            return -1;
        }
        state->saw_memory = 1;
    } else if (strcmp(entry_info->id, "test/file") == 0) {
        if (entry_info->type != TECMO_ASSET_PACK_TYPE_DATA ||
            entry_info->bank != 3U ||
            entry_info->cpu_address != 0x9000U ||
            entry_info->source_offset != 0x5678ULL ||
            entry_info->byte_count != state->file_size ||
            entry_info->flags != TECMO_ASSET_PACK_FLAG_LOCAL) {
            set_message(state->message,
                        state->message_size,
                        "Self-test file directory metadata mismatch.");
            return -1;
        }
        state->saw_file = 1;
    }

    return 0;
}

typedef struct TecmoAssetPackSelfTestInesListState {
    unsigned int count;
    int saw_manifest;
    int saw_source_map;
    int saw_prg_bank0;
    int saw_prg_fixed;
    int saw_chr_all;
    int saw_chr_bank0;
} TecmoAssetPackSelfTestInesListState;

static int self_test_ines_list_entry(const TecmoAssetPackDirectoryEntryInfo *entry_info,
                                     void *user_data)
{
    TecmoAssetPackSelfTestInesListState *state =
        (TecmoAssetPackSelfTestInesListState *)user_data;

    if (entry_info == NULL || state == NULL) {
        return -1;
    }

    ++state->count;
    if (strcmp(entry_info->id, "system/manifest") == 0) {
        state->saw_manifest = 1;
    } else if (strcmp(entry_info->id, "system/source-map") == 0) {
        state->saw_source_map = 1;
    } else if (strcmp(entry_info->id, "prg/bank00") == 0) {
        state->saw_prg_bank0 = 1;
    } else if (strcmp(entry_info->id, "prg/fixed") == 0) {
        state->saw_prg_fixed = 1;
    } else if (strcmp(entry_info->id, "chr/all") == 0) {
        state->saw_chr_all = 1;
    } else if (strcmp(entry_info->id, "chr/bank00") == 0) {
        state->saw_chr_bank0 = 1;
    }

    return 0;
}

int tecmo_asset_pack_self_test(char *message, size_t message_size)
{
    static const uint8_t memory_entry_bytes[] = {0x00U, 0x01U, 0x7FU, 0x80U, 0xFFU};
    static const uint8_t file_entry_bytes[] = {
        'l', 'o', 'c', 'a', 'l', '-', 'f', 'i', 'l', 'e', '-', 'e', 'n', 't', 'r', 'y'
    };
    char builder_pack_path[1024] = {0};
    char local_file_path[1024] = {0};
    char rom_path[1024] = {0};
    char ines_pack_path[1024] = {0};
    TecmoAssetPackBuilder *builder = NULL;
    TecmoAssetPackEntryInfo entry_info;
    TecmoAssetPackSelfTestBuilderListState builder_list_state;
    TecmoAssetPackSelfTestInesListState ines_list_state;
    uint8_t *rom = NULL;
    char *dump = NULL;
    uint64_t prg_offset = 16ULL;
    uint64_t chr_offset = 16ULL + 2ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    uint64_t chr_size = TECMO_ASSET_PACK_CHR_BANK_BYTES;
    uint64_t rom_size = 16ULL + 2ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES + TECMO_ASSET_PACK_CHR_BANK_BYTES;
    size_t dump_size = 0U;
    int result = -1;

    if (message != NULL && message_size > 0U) {
        message[0] = '\0';
    }

    if (make_self_test_path(builder_pack_path, sizeof(builder_pack_path), "builder.assetpack", 1U) != 0 ||
        make_self_test_path(local_file_path, sizeof(local_file_path), "local.bin", 2U) != 0 ||
        make_self_test_path(rom_path, sizeof(rom_path), "input.nes", 3U) != 0 ||
        make_self_test_path(ines_pack_path, sizeof(ines_pack_path), "ines.assetpack", 4U) != 0) {
        set_message(message, message_size, "Self-test could not create temporary paths.");
        goto cleanup;
    }

    remove_self_test_path(builder_pack_path);
    remove_self_test_path(local_file_path);
    remove_self_test_path(rom_path);
    remove_self_test_path(ines_pack_path);

    if (tecmo_asset_pack_builder_begin(&builder, builder_pack_path, message, message_size) != 0) {
        goto cleanup;
    }

    entry_info = make_entry_info("test/memory",
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 7U,
                                 0x8123U,
                                 0x1234ULL,
                                 TECMO_ASSET_PACK_FLAG_DERIVED);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            memory_entry_bytes,
                                            sizeof(memory_entry_bytes),
                                            message,
                                            message_size) != 0) {
        goto cleanup;
    }

    if (write_self_test_file(local_file_path, file_entry_bytes, sizeof(file_entry_bytes)) != 0) {
        set_message(message, message_size, "Self-test could not write temporary file entry source.");
        goto cleanup;
    }

    entry_info = make_entry_info("test/file",
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 3U,
                                 0x9000U,
                                 0x5678ULL,
                                 TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_file(builder,
                                          &entry_info,
                                          local_file_path,
                                          message,
                                          message_size) != 0) {
        goto cleanup;
    }
    if (tecmo_asset_pack_builder_finish(builder, message, message_size) != 0) {
        builder = NULL;
        goto cleanup;
    }
    builder = NULL;

    if (self_test_read_and_compare(builder_pack_path,
                                   "test/memory",
                                   memory_entry_bytes,
                                   sizeof(memory_entry_bytes),
                                   message,
                                   message_size) != 0 ||
        self_test_read_and_compare(builder_pack_path,
                                   "test/file",
                                   file_entry_bytes,
                                   sizeof(file_entry_bytes),
                                   message,
                                   message_size) != 0) {
        goto cleanup;
    }

    memset(&builder_list_state, 0, sizeof(builder_list_state));
    builder_list_state.memory_size = sizeof(memory_entry_bytes);
    builder_list_state.file_size = sizeof(file_entry_bytes);
    builder_list_state.message = message;
    builder_list_state.message_size = message_size;
    if (tecmo_asset_pack_list_entries(builder_pack_path,
                                      self_test_builder_list_entry,
                                      &builder_list_state) != 0 ||
        builder_list_state.count != 2U ||
        !builder_list_state.saw_memory ||
        !builder_list_state.saw_file) {
        if (message != NULL && message_size > 0U && message[0] == '\0') {
            set_message(message, message_size, "Self-test directory enumeration mismatch.");
        }
        goto cleanup;
    }

    if (tecmo_asset_pack_dump_directory(builder_pack_path, NULL, 0U, &dump_size) != 0 ||
        dump_size == 0U) {
        set_message(message, message_size, "Self-test could not size directory dump.");
        goto cleanup;
    }
    dump = (char *)malloc(dump_size);
    if (dump == NULL) {
        set_message(message, message_size, "Self-test could not allocate directory dump.");
        goto cleanup;
    }
    if (tecmo_asset_pack_dump_directory(builder_pack_path, dump, dump_size, &dump_size) != 0 ||
        strstr(dump, "test/memory") == NULL ||
        strstr(dump, "test/file") == NULL) {
        set_message(message, message_size, "Self-test directory dump did not include expected entries.");
        goto cleanup;
    }
    free(dump);
    dump = NULL;

    if (rom_size > (uint64_t)SIZE_MAX) {
        set_message(message, message_size, "Self-test ROM fixture is too large.");
        goto cleanup;
    }
    rom = (uint8_t *)calloc(1U, (size_t)rom_size);
    if (rom == NULL) {
        set_message(message, message_size, "Self-test could not allocate ROM fixture.");
        goto cleanup;
    }
    rom[0] = 'N';
    rom[1] = 'E';
    rom[2] = 'S';
    rom[3] = 0x1AU;
    rom[4] = 2U;
    rom[5] = 1U;

    for (size_t i = 0; i < (size_t)TECMO_ASSET_PACK_PRG_BANK_BYTES; ++i) {
        rom[(size_t)prg_offset + i] = (uint8_t)(i & 0xFFU);
        rom[(size_t)(prg_offset + TECMO_ASSET_PACK_PRG_BANK_BYTES) + i] =
            (uint8_t)(0x80U ^ (i & 0xFFU));
    }
    for (size_t i = 0; i < (size_t)TECMO_ASSET_PACK_CHR_BANK_BYTES; ++i) {
        rom[(size_t)chr_offset + i] = (uint8_t)(0x40U ^ (i & 0xFFU));
    }

    if (write_self_test_file(rom_path, rom, (size_t)rom_size) != 0) {
        set_message(message, message_size, "Self-test could not write temporary iNES ROM.");
        goto cleanup;
    }
    if (tecmo_asset_pack_build_from_ines(rom_path, ines_pack_path, message, message_size) != 0) {
        goto cleanup;
    }

    if (self_test_entry_contains_text(ines_pack_path,
                                      "system/manifest",
                                      "format=tecmo.assetpack/1",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/manifest",
                                      "prg_banks_16k=2",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/manifest",
                                      "chr_banks_8k=1",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"format\":\"tecmo.assetpack.source-map/1\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"id\":\"prg/bank00\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"id\":\"chr/bank00\"",
                                      message,
                                      message_size) != 0) {
        goto cleanup;
    }

    if (self_test_read_and_compare(ines_pack_path,
                                   "prg/bank00",
                                   rom + (size_t)prg_offset,
                                   TECMO_ASSET_PACK_PRG_BANK_BYTES,
                                   message,
                                   message_size) != 0 ||
        self_test_read_and_compare(ines_pack_path,
                                   "prg/fixed",
                                   rom + (size_t)(prg_offset + TECMO_ASSET_PACK_PRG_BANK_BYTES),
                                   TECMO_ASSET_PACK_PRG_BANK_BYTES,
                                   message,
                                   message_size) != 0 ||
        self_test_read_and_compare(ines_pack_path,
                                   "chr/all",
                                   rom + (size_t)chr_offset,
                                   chr_size,
                                   message,
                                   message_size) != 0 ||
        self_test_read_and_compare(ines_pack_path,
                                   "chr/bank00",
                                   rom + (size_t)chr_offset,
                                   TECMO_ASSET_PACK_CHR_BANK_BYTES,
                                   message,
                                   message_size) != 0) {
        goto cleanup;
    }

    memset(&ines_list_state, 0, sizeof(ines_list_state));
    if (tecmo_asset_pack_list_entries(ines_pack_path,
                                      self_test_ines_list_entry,
                                      &ines_list_state) != 0 ||
        ines_list_state.count != 7U ||
        !ines_list_state.saw_manifest ||
        !ines_list_state.saw_source_map ||
        !ines_list_state.saw_prg_bank0 ||
        !ines_list_state.saw_prg_fixed ||
        !ines_list_state.saw_chr_all ||
        !ines_list_state.saw_chr_bank0) {
        set_message(message, message_size, "Self-test iNES pack directory enumeration mismatch.");
        goto cleanup;
    }

    set_message(message, message_size, "Asset pack self-test passed.");
    result = 0;

cleanup:
    if (builder != NULL) {
        tecmo_asset_pack_builder_cancel(builder);
    }
    free(dump);
    free(rom);
    remove_self_test_path(builder_pack_path);
    remove_self_test_path(local_file_path);
    remove_self_test_path(rom_path);
    remove_self_test_path(ines_pack_path);
    return result;
}

void tecmo_asset_pack_free(void *buffer)
{
    free(buffer);
}
