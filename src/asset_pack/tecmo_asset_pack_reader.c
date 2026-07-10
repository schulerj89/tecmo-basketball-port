#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_asset_pack.h"

#include "tecmo_asset_pack_format.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct TecmoAssetPackDirectoryDumpContext {
    char *buffer;
    size_t buffer_size;
    size_t length;
    size_t required;
    int overflow;
} TecmoAssetPackDirectoryDumpContext;

static uint32_t read_u32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0]) |
           ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) |
           ((uint32_t)bytes[3] << 24U);
}

static uint64_t read_u64(const uint8_t *bytes)
{
    uint64_t value = 0U;
    for (size_t i = 0U; i < 8U; ++i) {
        value |= (uint64_t)bytes[i] << (unsigned)(i * 8U);
    }
    return value;
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

static int read_pack_header(FILE *file,
                            uint32_t *entry_count_out,
                            uint64_t *directory_offset_out)
{
    uint8_t header[TECMO_ASSET_PACK_HEADER_SIZE];
    uint32_t version;
    uint32_t header_size;
    uint32_t entry_size;

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
    if (version != TECMO_ASSET_PACK_VERSION ||
        header_size != TECMO_ASSET_PACK_HEADER_SIZE ||
        entry_size != TECMO_ASSET_PACK_ENTRY_SIZE) {
        return -1;
    }

    *entry_count_out = read_u32(header + 16U);
    *directory_offset_out = read_u64(header + 20U);
    return 0;
}

static int read_directory_entry(FILE *file, TecmoAssetPackDirectoryEntry *entry)
{
    uint8_t bytes[TECMO_ASSET_PACK_ENTRY_SIZE];
    if (fread(bytes, 1U, sizeof(bytes), file) != sizeof(bytes)) return -1;

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

int tecmo_asset_pack_list_entries(const char *pack_path,
                                  TecmoAssetPackListCallback callback,
                                  void *user_data)
{
    FILE *file;
    uint32_t entry_count;
    uint64_t directory_offset;
    int result = -1;

    if (pack_path == NULL || callback == NULL) return -1;
    file = fopen(pack_path, "rb");
    if (file == NULL) return -1;

    if (read_pack_header(file, &entry_count, &directory_offset) != 0 ||
        file_seek_u64(file, directory_offset) != 0) {
        goto cleanup;
    }

    for (uint32_t i = 0U; i < entry_count; ++i) {
        TecmoAssetPackDirectoryEntry entry;
        TecmoAssetPackDirectoryEntryInfo entry_info;
        int callback_result;

        if (read_directory_entry(file, &entry) != 0) goto cleanup;
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

static void append_directory_dump_text(TecmoAssetPackDirectoryDumpContext *context,
                                       const char *text,
                                       size_t text_length)
{
    if (context == NULL || text == NULL) return;
    if (SIZE_MAX - context->required < text_length) {
        context->required = SIZE_MAX;
        context->overflow = 1;
    } else {
        context->required += text_length;
    }

    if (context->buffer != NULL && context->buffer_size > 0U &&
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
    TecmoAssetPackDirectoryDumpContext *context =
        (TecmoAssetPackDirectoryDumpContext *)user_data;
    char line[512];
    int written;

    if (entry_info == NULL || context == NULL) return -1;
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
    if (written < 0 || (size_t)written >= sizeof(line)) {
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
    static const char header[] =
        "id\ttype\tbank\tcpu_address\tsource_offset\tbyte_count\tflags\n";
    TecmoAssetPackDirectoryDumpContext context;
    size_t required_size;
    int list_result;

    if (pack_path == NULL || (buffer == NULL && buffer_size > 0U)) return -1;
    if (required_size_out != NULL) *required_size_out = 0U;

    memset(&context, 0, sizeof(context));
    context.buffer = buffer;
    context.buffer_size = buffer_size;
    if (buffer != NULL && buffer_size > 0U) buffer[0] = '\0';

    append_directory_dump_text(&context, header, sizeof(header) - 1U);
    list_result = tecmo_asset_pack_list_entries(
        pack_path, append_directory_dump_entry, &context);
    if (buffer != NULL && buffer_size > 0U) buffer[context.length] = '\0';

    if (list_result != 0 || context.overflow || SIZE_MAX - context.required < 1U) {
        if (required_size_out != NULL) *required_size_out = SIZE_MAX;
        return -1;
    }
    required_size = context.required + 1U;
    if (required_size_out != NULL) *required_size_out = required_size;
    if (buffer != NULL && buffer_size < required_size) return -1;
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
    if (file == NULL) return -1;

    if (read_pack_header(file, &entry_count, &directory_offset) != 0 ||
        file_seek_u64(file, directory_offset) != 0) {
        goto cleanup;
    }

    for (uint32_t i = 0U; i < entry_count; ++i) {
        TecmoAssetPackDirectoryEntry entry;
        if (read_directory_entry(file, &entry) != 0) goto cleanup;
        if (strncmp(entry.id, entry_id, TECMO_ASSET_PACK_ID_SIZE) == 0) {
            if (entry.size > (uint64_t)SIZE_MAX) goto cleanup;
            bytes = (uint8_t *)malloc((size_t)entry.size);
            if (bytes == NULL && entry.size > 0U) goto cleanup;
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

void tecmo_asset_pack_free(void *buffer)
{
    free(buffer);
}
