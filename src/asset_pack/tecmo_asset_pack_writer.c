#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_asset_pack_writer.h"

#include "tecmo_asset_pack_format.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

struct TecmoAssetPackBuilder {
    FILE *out;
    char *out_path;
    TecmoAssetPackBuildEntry *entries;
    size_t entry_count;
    size_t entry_capacity;
};

static void set_message(char *message, size_t message_size, const char *text)
{
    if (message == NULL || message_size == 0U) return;
    (void)snprintf(message, message_size, "%s", text != NULL ? text : "");
}

static char *copy_string(const char *text)
{
    size_t length;
    char *copy;

    if (text == NULL) text = "";
    length = strlen(text);
    copy = (char *)malloc(length + 1U);
    if (copy == NULL) return NULL;
    memcpy(copy, text, length + 1U);
    return copy;
}

static int file_tell_u64(FILE *file, uint64_t *position_out)
{
#ifdef _WIN32
    __int64 position = _ftelli64(file);
    if (position < 0) return -1;
    *position_out = (uint64_t)position;
#else
    long position = ftell(file);
    if (position < 0) return -1;
    *position_out = (uint64_t)position;
#endif
    return 0;
}

static int file_seek_u64(FILE *file, uint64_t position)
{
#ifdef _WIN32
    if (position > 0x7FFFFFFFFFFFFFFFULL) return -1;
    return _fseeki64(file, (__int64)position, SEEK_SET);
#else
    if (position > (uint64_t)LONG_MAX) return -1;
    return fseek(file, (long)position, SEEK_SET);
#endif
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
    for (size_t i = 0U; i < sizeof(bytes); ++i) {
        bytes[i] = (uint8_t)((value >> (unsigned)(i * 8U)) & 0xFFU);
    }
    return fwrite(bytes, 1U, sizeof(bytes), file) == sizeof(bytes) ? 0 : -1;
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

static int write_directory_entry(FILE *file,
                                 const TecmoAssetPackBuildEntry *entry)
{
    uint8_t padding[20];
    memset(padding, 0, sizeof(padding));
    if (fwrite(entry->id, 1U, TECMO_ASSET_PACK_ID_SIZE, file) !=
            TECMO_ASSET_PACK_ID_SIZE ||
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

static void release_builder(TecmoAssetPackBuilder *builder)
{
    if (builder == NULL) return;
    if (builder->out != NULL) fclose(builder->out);
    free(builder->out_path);
    free(builder->entries);
    free(builder);
}

static int reserve_build_entry(TecmoAssetPackBuilder *builder,
                               char *message,
                               size_t message_size)
{
    size_t new_capacity;
    TecmoAssetPackBuildEntry *new_entries;

    if (builder->entry_count >= UINT32_MAX) {
        set_message(message, message_size,
                    "Asset pack directory entry count exceeds format limits.");
        return -1;
    }
    if (builder->entry_count < builder->entry_capacity) return 0;
    if (builder->entry_capacity > SIZE_MAX / 2U) {
        set_message(message, message_size,
                    "Asset pack directory capacity overflow.");
        return -1;
    }
    new_capacity = builder->entry_capacity == 0U
                       ? 32U
                       : builder->entry_capacity * 2U;
    if (new_capacity > SIZE_MAX / sizeof(builder->entries[0])) {
        set_message(message, message_size,
                    "Asset pack directory allocation is too large.");
        return -1;
    }

    new_entries = (TecmoAssetPackBuildEntry *)realloc(
        builder->entries, new_capacity * sizeof(builder->entries[0]));
    if (new_entries == NULL) {
        set_message(message, message_size,
                    "Out of memory for asset pack directory.");
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
    if (entry_info == NULL || entry_info->id == NULL ||
        entry_info->id[0] == '\0') {
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
    if (reserve_build_entry(builder, message, message_size) != 0) return -1;

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

static int write_memory_payload(FILE *out,
                                const void *data,
                                uint64_t byte_count)
{
    const uint8_t *cursor = (const uint8_t *)data;
    uint64_t remaining = byte_count;
    if (byte_count > 0U && data == NULL) return -1;

    while (remaining > 0U) {
        size_t chunk = remaining > 65536U ? 65536U : (size_t)remaining;
        if (fwrite(cursor, 1U, chunk, out) != chunk) return -1;
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
        set_message(message, message_size,
                    "Asset pack builder output is required.");
        return -1;
    }
    *builder_out = NULL;
    if (out_path == NULL || out_path[0] == '\0') {
        set_message(message, message_size,
                    "Asset pack output path is required.");
        return -1;
    }

    builder = (TecmoAssetPackBuilder *)calloc(1U, sizeof(*builder));
    if (builder == NULL) {
        set_message(message, message_size,
                    "Out of memory for asset pack builder.");
        return -1;
    }
    builder->out_path = copy_string(out_path);
    if (builder->out_path == NULL) {
        set_message(message, message_size,
                    "Out of memory for asset pack output path.");
        release_builder(builder);
        return -1;
    }
    builder->out = fopen(out_path, "wb");
    if (builder->out == NULL) {
        set_message(message, message_size,
                    "Could not open asset pack output.");
        release_builder(builder);
        return -1;
    }
    if (write_header(builder->out, 0U, 0U,
                     TECMO_ASSET_PACK_HEADER_SIZE) != 0) {
        set_message(message, message_size,
                    "Could not write asset pack header.");
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
        set_message(message, message_size,
                    "Asset pack memory entry has no bytes.");
        return -1;
    }
    if (file_tell_u64(builder->out, &pack_offset) != 0) {
        set_message(message, message_size,
                    "Could not locate asset pack data offset.");
        return -1;
    }
    if (write_memory_payload(builder->out, data, byte_count) != 0) {
        set_message(message, message_size,
                    "Could not write asset pack memory entry.");
        return -1;
    }
    return append_build_entry(builder, entry_info, pack_offset, byte_count,
                              message, message_size);
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
        set_message(message, message_size,
                    "Asset pack local file path is required.");
        return -1;
    }
    input = fopen(local_path, "rb");
    if (input == NULL) {
        set_message(message, message_size,
                    "Could not read local asset pack entry file.");
        return -1;
    }
    if (file_tell_u64(builder->out, &pack_offset) != 0) {
        set_message(message, message_size,
                    "Could not locate asset pack data offset.");
        goto cleanup;
    }

    while ((bytes_read = fread(buffer, 1U, sizeof(buffer), input)) > 0U) {
        if (UINT64_MAX - byte_count < (uint64_t)bytes_read) {
            set_message(message, message_size,
                        "Asset pack local file is too large.");
            goto cleanup;
        }
        if (fwrite(buffer, 1U, bytes_read, builder->out) != bytes_read) {
            set_message(message, message_size,
                        "Could not write local asset pack entry.");
            goto cleanup;
        }
        byte_count += (uint64_t)bytes_read;
    }
    if (ferror(input) != 0) {
        set_message(message, message_size,
                    "Could not read local asset pack entry file.");
        goto cleanup;
    }
    if (append_build_entry(builder, entry_info, pack_offset, byte_count,
                           message, message_size) != 0) {
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
        set_message(message, message_size,
                    "Asset pack directory entry count exceeds format limits.");
        goto cleanup;
    }
    if (file_tell_u64(builder->out, &directory_offset) != 0) {
        set_message(message, message_size,
                    "Could not locate asset pack directory.");
        goto cleanup;
    }
    for (size_t i = 0U; i < builder->entry_count; ++i) {
        if (write_directory_entry(builder->out, &builder->entries[i]) != 0) {
            set_message(message, message_size,
                        "Could not write asset pack directory.");
            goto cleanup;
        }
    }

    entry_count = (uint32_t)builder->entry_count;
    if (file_seek_u64(builder->out, 0U) != 0 ||
        write_header(builder->out, entry_count, directory_offset,
                     TECMO_ASSET_PACK_HEADER_SIZE) != 0) {
        set_message(message, message_size,
                    "Could not finalize asset pack header.");
        goto cleanup;
    }
    {
        FILE *out = builder->out;
        builder->out = NULL;
        if (fclose(out) != 0) {
            set_message(message, message_size,
                        "Could not close asset pack output.");
            goto cleanup;
        }
    }
    if (message != NULL && message_size > 0U) {
        (void)snprintf(message, message_size, "Wrote %u entries to %s",
                       entry_count, builder->out_path);
    }
    result = 0;

cleanup:
    release_builder(builder);
    return result;
}

size_t tecmo_asset_pack_builder_entry_count(const TecmoAssetPackBuilder *builder)
{
    return builder != NULL ? builder->entry_count : 0U;
}

void tecmo_asset_pack_builder_cancel(TecmoAssetPackBuilder *builder)
{
    release_builder(builder);
}
