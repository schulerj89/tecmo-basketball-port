#define _CRT_SECURE_NO_WARNINGS

#include "png_writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t crc_table[256];
static int crc_table_ready = 0;

static void make_crc_table(void)
{
    for (uint32_t n = 0; n < 256; ++n) {
        uint32_t c = n;
        for (int k = 0; k < 8; ++k) {
            c = (c & 1U) ? (0xEDB88320U ^ (c >> 1U)) : (c >> 1U);
        }
        crc_table[n] = c;
    }
    crc_table_ready = 1;
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len)
{
    uint32_t c = crc;
    if (!crc_table_ready) {
        make_crc_table();
    }
    for (size_t i = 0; i < len; ++i) {
        c = crc_table[(c ^ data[i]) & 0xFFU] ^ (c >> 8U);
    }
    return c;
}

static uint32_t adler32(const uint8_t *data, size_t len)
{
    uint32_t a = 1;
    uint32_t b = 0;
    for (size_t i = 0; i < len; ++i) {
        a = (a + data[i]) % 65521U;
        b = (b + a) % 65521U;
    }
    return (b << 16U) | a;
}

static int write_u32_be(FILE *file, uint32_t value)
{
    uint8_t bytes[4];
    bytes[0] = (uint8_t)((value >> 24U) & 0xFFU);
    bytes[1] = (uint8_t)((value >> 16U) & 0xFFU);
    bytes[2] = (uint8_t)((value >> 8U) & 0xFFU);
    bytes[3] = (uint8_t)(value & 0xFFU);
    return fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes) ? 0 : -1;
}

static int write_chunk(FILE *file, const char type[4], const uint8_t *data, uint32_t len)
{
    uint32_t crc;
    if (write_u32_be(file, len) != 0) {
        return -1;
    }
    if (fwrite(type, 1, 4, file) != 4) {
        return -1;
    }
    if (len > 0 && fwrite(data, 1, len, file) != len) {
        return -1;
    }
    crc = crc32_update(0xFFFFFFFFU, (const uint8_t *)type, 4);
    if (len > 0) {
        crc = crc32_update(crc, data, len);
    }
    crc ^= 0xFFFFFFFFU;
    return write_u32_be(file, crc);
}

static int append_byte(uint8_t *dst, size_t capacity, size_t *pos, uint8_t value)
{
    if (*pos >= capacity) {
        return -1;
    }
    dst[(*pos)++] = value;
    return 0;
}

static int append_u16_le(uint8_t *dst, size_t capacity, size_t *pos, uint16_t value)
{
    return append_byte(dst, capacity, pos, (uint8_t)(value & 0xFFU)) == 0 &&
           append_byte(dst, capacity, pos, (uint8_t)((value >> 8U) & 0xFFU)) == 0
               ? 0
               : -1;
}

static int build_zlib_store_stream(const uint8_t *raw, size_t raw_len, uint8_t **out, size_t *out_len)
{
    size_t max_blocks = raw_len / 65535U + 1U;
    size_t capacity = 2U + raw_len + max_blocks * 5U + 4U;
    uint8_t *buffer = (uint8_t *)malloc(capacity);
    size_t pos = 0;
    size_t offset = 0;
    uint32_t adler = adler32(raw, raw_len);

    if (buffer == NULL) {
        return -1;
    }

    if (append_byte(buffer, capacity, &pos, 0x78U) != 0 ||
        append_byte(buffer, capacity, &pos, 0x01U) != 0) {
        free(buffer);
        return -1;
    }

    while (offset < raw_len || (raw_len == 0 && offset == 0)) {
        size_t remaining = raw_len - offset;
        uint16_t block_len = (uint16_t)(remaining > 65535U ? 65535U : remaining);
        uint8_t final_block = (offset + block_len >= raw_len) ? 1U : 0U;
        uint16_t nlen = (uint16_t)~block_len;

        if (append_byte(buffer, capacity, &pos, final_block) != 0 ||
            append_u16_le(buffer, capacity, &pos, block_len) != 0 ||
            append_u16_le(buffer, capacity, &pos, nlen) != 0) {
            free(buffer);
            return -1;
        }

        if (block_len > 0) {
            if (pos + block_len > capacity) {
                free(buffer);
                return -1;
            }
            memcpy(buffer + pos, raw + offset, block_len);
            pos += block_len;
            offset += block_len;
        } else {
            break;
        }
    }

    if (append_byte(buffer, capacity, &pos, (uint8_t)((adler >> 24U) & 0xFFU)) != 0 ||
        append_byte(buffer, capacity, &pos, (uint8_t)((adler >> 16U) & 0xFFU)) != 0 ||
        append_byte(buffer, capacity, &pos, (uint8_t)((adler >> 8U) & 0xFFU)) != 0 ||
        append_byte(buffer, capacity, &pos, (uint8_t)(adler & 0xFFU)) != 0) {
        free(buffer);
        return -1;
    }

    *out = buffer;
    *out_len = pos;
    return 0;
}

int png_write_rgba8(const char *path, const uint8_t *rgba, uint32_t width, uint32_t height)
{
    static const uint8_t signature[8] = {0x89U, 'P', 'N', 'G', 0x0DU, 0x0AU, 0x1AU, 0x0AU};
    FILE *file;
    uint8_t ihdr[13];
    uint8_t *scanlines = NULL;
    uint8_t *zlib_stream = NULL;
    size_t row_bytes;
    size_t scanline_bytes;
    size_t zlib_len = 0;
    int result = -1;

    if (width == 0 || height == 0) {
        return -1;
    }

    row_bytes = (size_t)width * 4U;
    scanline_bytes = ((size_t)height * (row_bytes + 1U));
    scanlines = (uint8_t *)malloc(scanline_bytes);
    if (scanlines == NULL) {
        return -1;
    }

    for (uint32_t y = 0; y < height; ++y) {
        size_t dst = (size_t)y * (row_bytes + 1U);
        size_t src = (size_t)y * row_bytes;
        scanlines[dst] = 0;
        memcpy(scanlines + dst + 1U, rgba + src, row_bytes);
    }

    if (build_zlib_store_stream(scanlines, scanline_bytes, &zlib_stream, &zlib_len) != 0) {
        free(scanlines);
        return -1;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        free(zlib_stream);
        free(scanlines);
        return -1;
    }

    ihdr[0] = (uint8_t)((width >> 24U) & 0xFFU);
    ihdr[1] = (uint8_t)((width >> 16U) & 0xFFU);
    ihdr[2] = (uint8_t)((width >> 8U) & 0xFFU);
    ihdr[3] = (uint8_t)(width & 0xFFU);
    ihdr[4] = (uint8_t)((height >> 24U) & 0xFFU);
    ihdr[5] = (uint8_t)((height >> 16U) & 0xFFU);
    ihdr[6] = (uint8_t)((height >> 8U) & 0xFFU);
    ihdr[7] = (uint8_t)(height & 0xFFU);
    ihdr[8] = 8;
    ihdr[9] = 6;
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;

    if (fwrite(signature, 1, sizeof(signature), file) == sizeof(signature) &&
        write_chunk(file, "IHDR", ihdr, sizeof(ihdr)) == 0 &&
        write_chunk(file, "IDAT", zlib_stream, (uint32_t)zlib_len) == 0 &&
        write_chunk(file, "IEND", NULL, 0) == 0) {
        result = 0;
    }

    if (fclose(file) != 0) {
        result = -1;
    }

    free(zlib_stream);
    free(scanlines);
    return result;
}
