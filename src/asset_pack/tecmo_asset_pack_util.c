#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_asset_pack_util.h"
#include "tecmo_asset_pack_import_layout.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void tecmo_asset_pack_set_message(char *message,
                                  size_t message_size,
                                  const char *text)
{
    if (message == NULL || message_size == 0U) {
        return;
    }
    if (text == NULL) {
        text = "";
    }
    (void)snprintf(message, message_size, "%s", text);
}

void tecmo_asset_pack_set_messagef(char *message,
                                   size_t message_size,
                                   const char *format,
                                   ...)
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

int tecmo_asset_pack_copy_path(char *dest,
                               size_t dest_size,
                               const char *src)
{
    int written;

    if (dest == NULL || dest_size == 0U || src == NULL) {
        return -1;
    }

    written = snprintf(dest, dest_size, "%s", src);
    return written >= 0 && (size_t)written < dest_size ? 0 : -1;
}

int tecmo_asset_pack_append_text(char *buffer,
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

uint32_t tecmo_asset_pack_read_u32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0]) |
           ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) |
           ((uint32_t)bytes[3] << 24U);
}

uint16_t tecmo_asset_pack_read_u16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U));
}

uint32_t tecmo_asset_pack_fnv1a32(const uint8_t *bytes, size_t byte_count)
{
    uint32_t hash = 2166136261U;
    size_t i;

    for (i = 0U; i < byte_count; ++i) {
        hash ^= bytes[i];
        hash *= 16777619U;
    }
    return hash;
}

void tecmo_asset_pack_store_u16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value & 0xFFU);
    bytes[1] = (uint8_t)(value >> 8U);
}

void tecmo_asset_pack_store_u32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value & 0xFFU);
    bytes[1] = (uint8_t)((value >> 8U) & 0xFFU);
    bytes[2] = (uint8_t)((value >> 16U) & 0xFFU);
    bytes[3] = (uint8_t)((value >> 24U) & 0xFFU);
}

uint8_t tecmo_asset_pack_imported_fade_color(uint8_t color, uint8_t reduction)
{
    if (color == 0x0FU) {
        return color;
    }
    return (color & 0x30U) >= reduction ? (uint8_t)(color - reduction) : 0x0FU;
}

uint8_t tecmo_asset_pack_decoded_palette_index(const uint8_t *page,
                                                unsigned row,
                                                unsigned col)
{
    size_t attribute_index = TECMO_ASSET_PACK_ATTRIBUTE_OFFSET +
                             (size_t)(row / 4U) * 8U + col / 4U;
    unsigned shift = ((row & 2U) != 0U ? 4U : 0U) +
                     ((col & 2U) != 0U ? 2U : 0U);
    return (uint8_t)((page[attribute_index] >> shift) & 3U);
}

int tecmo_asset_pack_validate_chr_pair(uint8_t r0,
                                       uint8_t r1,
                                       uint64_t chr_size,
                                       const char *pair_name,
                                       char *message,
                                       size_t message_size)
{
    if ((r0 & 1U) != 0U || (r1 & 1U) != 0U ||
        ((uint64_t)r0 + 2U) * 1024U > chr_size ||
        ((uint64_t)r1 + 2U) * 1024U > chr_size) {
        tecmo_asset_pack_set_messagef(message,
                                      message_size,
                                      "Arena %s CHR selectors %u/%u are not valid even 2KB-bank selectors.",
                                      pair_name,
                                      (unsigned int)r0,
                                      (unsigned int)r1);
        return -1;
    }
    return 0;
}

TecmoAssetPackEntryInfo tecmo_asset_pack_make_entry_info(
    const char *id,
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

int tecmo_asset_pack_read_file(const char *path,
                               uint8_t **bytes_out,
                               uint64_t *size_out)
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
