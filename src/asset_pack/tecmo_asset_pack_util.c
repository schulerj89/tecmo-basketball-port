#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_asset_pack_util.h"
#include "tecmo_asset_pack_import_layout.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static uint32_t sha256_rotate_right(uint32_t value, unsigned shift)
{
    return (value >> shift) | (value << (32U - shift));
}

static void sha256_transform(uint32_t state[8], const uint8_t block[64])
{
    static const uint32_t constants[64] = {
        0x428A2F98U,0x71374491U,0xB5C0FBCFU,0xE9B5DBA5U,
        0x3956C25BU,0x59F111F1U,0x923F82A4U,0xAB1C5ED5U,
        0xD807AA98U,0x12835B01U,0x243185BEU,0x550C7DC3U,
        0x72BE5D74U,0x80DEB1FEU,0x9BDC06A7U,0xC19BF174U,
        0xE49B69C1U,0xEFBE4786U,0x0FC19DC6U,0x240CA1CCU,
        0x2DE92C6FU,0x4A7484AAU,0x5CB0A9DCU,0x76F988DAU,
        0x983E5152U,0xA831C66DU,0xB00327C8U,0xBF597FC7U,
        0xC6E00BF3U,0xD5A79147U,0x06CA6351U,0x14292967U,
        0x27B70A85U,0x2E1B2138U,0x4D2C6DFCU,0x53380D13U,
        0x650A7354U,0x766A0ABBU,0x81C2C92EU,0x92722C85U,
        0xA2BFE8A1U,0xA81A664BU,0xC24B8B70U,0xC76C51A3U,
        0xD192E819U,0xD6990624U,0xF40E3585U,0x106AA070U,
        0x19A4C116U,0x1E376C08U,0x2748774CU,0x34B0BCB5U,
        0x391C0CB3U,0x4ED8AA4AU,0x5B9CCA4FU,0x682E6FF3U,
        0x748F82EEU,0x78A5636FU,0x84C87814U,0x8CC70208U,
        0x90BEFFFAU,0xA4506CEBU,0xBEF9A3F7U,0xC67178F2U
    };
    uint32_t words[64];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;
    uint32_t g;
    uint32_t h;
    for (size_t index = 0U; index < 16U; ++index) {
        size_t offset = index * 4U;
        words[index] = ((uint32_t)block[offset] << 24U) |
                       ((uint32_t)block[offset + 1U] << 16U) |
                       ((uint32_t)block[offset + 2U] << 8U) |
                       (uint32_t)block[offset + 3U];
    }
    for (size_t index = 16U; index < 64U; ++index) {
        uint32_t prior = words[index - 15U];
        uint32_t recent = words[index - 2U];
        uint32_t sigma0 = sha256_rotate_right(prior, 7U) ^
                          sha256_rotate_right(prior, 18U) ^ (prior >> 3U);
        uint32_t sigma1 = sha256_rotate_right(recent, 17U) ^
                          sha256_rotate_right(recent, 19U) ^ (recent >> 10U);
        words[index] = words[index - 16U] + sigma0 +
                       words[index - 7U] + sigma1;
    }
    a = state[0U];
    b = state[1U];
    c = state[2U];
    d = state[3U];
    e = state[4U];
    f = state[5U];
    g = state[6U];
    h = state[7U];
    for (size_t index = 0U; index < 64U; ++index) {
        uint32_t choice = (e & f) ^ ((~e) & g);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t sum0 = sha256_rotate_right(a, 2U) ^
                        sha256_rotate_right(a, 13U) ^
                        sha256_rotate_right(a, 22U);
        uint32_t sum1 = sha256_rotate_right(e, 6U) ^
                        sha256_rotate_right(e, 11U) ^
                        sha256_rotate_right(e, 25U);
        uint32_t temp1 =
            h + sum1 + choice + constants[index] + words[index];
        uint32_t temp2 = sum0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    state[0U] += a;
    state[1U] += b;
    state[2U] += c;
    state[3U] += d;
    state[4U] += e;
    state[5U] += f;
    state[6U] += g;
    state[7U] += h;
}

int tecmo_asset_pack_sha256_digest(const uint8_t *bytes,
                                   size_t byte_count,
                                   uint8_t digest[32])
{
    uint32_t state[8] = {
        0x6A09E667U,0xBB67AE85U,0x3C6EF372U,0xA54FF53AU,
        0x510E527FU,0x9B05688CU,0x1F83D9ABU,0x5BE0CD19U
    };
    uint8_t tail[128];
    size_t full_bytes;
    size_t remainder;
    size_t padded_size;
    uint64_t bit_count;
    if (bytes == NULL || digest == NULL ||
        (uint64_t)byte_count > UINT64_MAX / 8ULL) {
        return -1;
    }
    full_bytes = byte_count - byte_count % 64U;
    remainder = byte_count - full_bytes;
    for (size_t offset = 0U; offset < full_bytes; offset += 64U) {
        sha256_transform(state, bytes + offset);
    }
    memset(tail, 0, sizeof(tail));
    memcpy(tail, bytes + full_bytes, remainder);
    tail[remainder] = 0x80U;
    padded_size = remainder < 56U ? 64U : 128U;
    bit_count = (uint64_t)byte_count * 8ULL;
    for (size_t index = 0U; index < 8U; ++index) {
        tail[padded_size - 1U - index] =
            (uint8_t)(bit_count >> (index * 8U));
    }
    sha256_transform(state, tail);
    if (padded_size == 128U) {
        sha256_transform(state, tail + 64U);
    }
    for (size_t index = 0U; index < 8U; ++index) {
        digest[index * 4U] = (uint8_t)(state[index] >> 24U);
        digest[index * 4U + 1U] = (uint8_t)(state[index] >> 16U);
        digest[index * 4U + 2U] = (uint8_t)(state[index] >> 8U);
        digest[index * 4U + 3U] = (uint8_t)state[index];
    }
    return 0;
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

uint8_t tecmo_asset_pack_palette_brightness_cap(uint8_t color, uint8_t cap)
{
    uint8_t brightness;
    if (color == 0x0FU) return color;
    brightness = (uint8_t)(color & 0x30U);
    if (brightness > (uint8_t)(cap << 4U)) {
        color = (uint8_t)((color & 0x0FU) | (cap << 4U));
    }
    return color;
}

uint32_t tecmo_asset_pack_bg_chr_offset(uint8_t tile, uint8_t r0, uint8_t r1)
{
    uint8_t selector = tile < 128U ? r0 : r1;
    return (uint32_t)selector * 1024U + (uint32_t)(tile & 0x7FU) * 16U;
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
