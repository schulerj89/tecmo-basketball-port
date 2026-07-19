#include "tecmo_nes_video.h"

#include <stddef.h>
#include <stdio.h>

#define TECMO_NES_CHR_BANK_BYTES 8192U

/* Exact RGB triples from FCEUX 2.6.6 palettes/FCEUX.pal (192 bytes).
 * Keep this embedded: native rendering must not depend on an emulator install. */
static const uint8_t fceux_2_6_6_palette_rgb[64U * 3U] = {
    0x74U, 0x74U, 0x74U, 0x24U, 0x18U, 0x8CU, 0x00U, 0x00U, 0xA8U,
    0x44U, 0x00U, 0x9CU, 0x8CU, 0x00U, 0x74U, 0xA8U, 0x00U, 0x10U,
    0xA4U, 0x00U, 0x00U, 0x7CU, 0x08U, 0x00U, 0x40U, 0x2CU, 0x00U,
    0x00U, 0x44U, 0x00U, 0x00U, 0x50U, 0x00U, 0x00U, 0x3CU, 0x14U,
    0x18U, 0x3CU, 0x5CU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0xBCU, 0xBCU, 0xBCU, 0x00U, 0x70U, 0xECU,
    0x20U, 0x38U, 0xECU, 0x80U, 0x00U, 0xF0U, 0xBCU, 0x00U, 0xBCU,
    0xE4U, 0x00U, 0x58U, 0xD8U, 0x28U, 0x00U, 0xC8U, 0x4CU, 0x0CU,
    0x88U, 0x70U, 0x00U, 0x00U, 0x94U, 0x00U, 0x00U, 0xA8U, 0x00U,
    0x00U, 0x90U, 0x38U, 0x00U, 0x80U, 0x88U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0xFCU, 0xFCU, 0xFCU,
    0x3CU, 0xBCU, 0xFCU, 0x5CU, 0x94U, 0xFCU, 0xCCU, 0x88U, 0xFCU,
    0xF4U, 0x78U, 0xFCU, 0xFCU, 0x74U, 0xB4U, 0xFCU, 0x74U, 0x60U,
    0xFCU, 0x98U, 0x38U, 0xF0U, 0xBCU, 0x3CU, 0x80U, 0xD0U, 0x10U,
    0x4CU, 0xDCU, 0x48U, 0x58U, 0xF8U, 0x98U, 0x00U, 0xE8U, 0xD8U,
    0x78U, 0x78U, 0x78U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0xFCU, 0xFCU, 0xFCU, 0xA8U, 0xE4U, 0xFCU, 0xC4U, 0xD4U, 0xFCU,
    0xD4U, 0xC8U, 0xFCU, 0xFCU, 0xC4U, 0xFCU, 0xFCU, 0xC4U, 0xD8U,
    0xFCU, 0xBCU, 0xB0U, 0xFCU, 0xD8U, 0xA8U, 0xFCU, 0xE4U, 0xA0U,
    0xE0U, 0xFCU, 0xA0U, 0xA8U, 0xF0U, 0xBCU, 0xB0U, 0xFCU, 0xCCU,
    0x9CU, 0xFCU, 0xF0U, 0xC4U, 0xC4U, 0xC4U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U,
};

_Static_assert(sizeof(fceux_2_6_6_palette_rgb) == 192U,
               "FCEUX 2.6.6 palette must contain 64 RGB triples");

static void video_rect(TecmoFramebuffer *fb, int x, int y, int w, int h, uint32_t color)
{
    int x0 = x;
    int y0 = y;
    int x1 = x + w;
    int y1 = y + h;

    if (fb == 0 || fb->pixels == 0 || w <= 0 || h <= 0) {
        return;
    }
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > fb->width) x1 = fb->width;
    if (y1 > fb->height) y1 = fb->height;
    if (x0 >= x1 || y0 >= y1) {
        return;
    }

    for (int row = y0; row < y1; ++row) {
        uint32_t *dst = fb->pixels + (size_t)row * (size_t)fb->pitch_pixels + (size_t)x0;
        for (int col = x0; col < x1; ++col) {
            *dst++ = color;
        }
    }
}

uint32_t tecmo_nes_2c02_rgba(uint8_t color)
{
    size_t offset = (size_t)(color & 0x3FU) * 3U;
    return 0xFF000000U |
           ((uint32_t)fceux_2_6_6_palette_rgb[offset] << 16U) |
           ((uint32_t)fceux_2_6_6_palette_rgb[offset + 1U] << 8U) |
           (uint32_t)fceux_2_6_6_palette_rgb[offset + 2U];
}

bool tecmo_nes_video_self_test(char *message, size_t message_size)
{
    uint32_t hash = 2166136261U;
    static const struct PaletteAnchor {
        uint8_t index;
        uint32_t rgba;
    } anchors[] = {
        {0x00U, 0xFF747474U},
        {0x01U, 0xFF24188CU},
        {0x1BU, 0xFF009038U},
        {0x2DU, 0xFF787878U},
        {0x3DU, 0xFFC4C4C4U},
        {0x3FU, 0xFF000000U},
    };

    for (size_t i = 0U; i < sizeof(fceux_2_6_6_palette_rgb); ++i) {
        hash ^= fceux_2_6_6_palette_rgb[i];
        hash *= 16777619U;
    }
    if (hash != 0x9F872B25U) {
        if (message != NULL && message_size > 0U) {
            (void)snprintf(message, message_size,
                           "FCEUX.pal fingerprint mismatch: %08X", hash);
        }
        return false;
    }
    for (size_t i = 0U; i < sizeof(anchors) / sizeof(anchors[0]); ++i) {
        if (tecmo_nes_2c02_rgba(anchors[i].index) != anchors[i].rgba) {
            if (message != NULL && message_size > 0U) {
                (void)snprintf(message, message_size,
                               "FCEUX.pal color %02X mismatch",
                               (unsigned)anchors[i].index);
            }
            return false;
        }
    }
    if (tecmo_nes_2c02_rgba(0x41U) != tecmo_nes_2c02_rgba(0x01U) ||
        tecmo_nes_2c02_rgba(0xFFU) != tecmo_nes_2c02_rgba(0x3FU)) {
        if (message != NULL && message_size > 0U) {
            (void)snprintf(message, message_size, "NES palette index masking mismatch");
        }
        return false;
    }
    if (message != NULL && message_size > 0U) {
        (void)snprintf(message, message_size,
                       "NES VIDEO TEST PASS: embedded FCEUX 2.6.6 FCEUX.pal (192 bytes, FNV1a32 9F872B25)");
    }
    return true;
}

uint8_t tecmo_nes_attribute_palette_index(uint8_t attribute, int tile_row, int tile_col)
{
    uint8_t shift = 0U;

    if ((tile_row & 0x03) >= 2) {
        shift = (uint8_t)(shift + 4U);
    }
    if ((tile_col & 0x03) >= 2) {
        shift = (uint8_t)(shift + 2U);
    }

    return (uint8_t)((attribute >> shift) & 0x03U);
}

void tecmo_draw_chr_tile_at_offset_ex(TecmoFramebuffer *fb,
                                      const uint8_t *chr_bytes,
                                      uint64_t chr_byte_count,
                                      uint64_t tile_offset,
                                      int x,
                                      int y,
                                      int scale,
                                      const uint32_t palette[4],
                                      bool flip_horizontal,
                                      bool flip_vertical)
{
    if (fb == 0 || chr_bytes == 0 || palette == 0 || scale <= 0) {
        return;
    }
    if (tile_offset + 15ULL >= chr_byte_count) {
        return;
    }

    for (int row = 0; row < 8; ++row) {
        int source_row = flip_vertical ? 7 - row : row;
        uint8_t plane0 = chr_bytes[tile_offset + (uint64_t)source_row];
        uint8_t plane1 = chr_bytes[tile_offset + (uint64_t)source_row + 8ULL];
        for (int col = 0; col < 8; ++col) {
            uint8_t bit = (uint8_t)(flip_horizontal ? col : 7 - col);
            uint8_t value = (uint8_t)(((plane0 >> bit) & 1U) | (((plane1 >> bit) & 1U) << 1U));
            uint32_t color = palette[value];
            if (value == 0 || color == 0) {
                continue;
            }
            video_rect(fb, x + col * scale, y + row * scale, scale, scale, color);
        }
    }
}

void tecmo_draw_chr_tile_ex(TecmoFramebuffer *fb,
                            const uint8_t *chr_bytes,
                            uint64_t chr_byte_count,
                            uint32_t chr_bank,
                            uint16_t tile,
                            int x,
                            int y,
                            int scale,
                            const uint32_t palette[4],
                            bool flip_horizontal,
                            bool flip_vertical)
{
    uint64_t tile_offset = (uint64_t)chr_bank * TECMO_NES_CHR_BANK_BYTES + (uint64_t)tile * 16ULL;

    if (tile > 0x1FFU) {
        return;
    }
    tecmo_draw_chr_tile_at_offset_ex(fb,
                                     chr_bytes,
                                     chr_byte_count,
                                     tile_offset,
                                     x,
                                     y,
                                     scale,
                                     palette,
                                     flip_horizontal,
                                     flip_vertical);
}

void tecmo_draw_chr_tile(TecmoFramebuffer *fb,
                         const uint8_t *chr_bytes,
                         uint64_t chr_byte_count,
                         uint32_t chr_bank,
                         uint16_t tile,
                         int x,
                         int y,
                         int scale)
{
    static const uint32_t palette[4] = {
        0x00000000U,
        0xFF614C5CU,
        0xFFC9B45CU,
        0xFFF8F0C8U,
    };
    tecmo_draw_chr_tile_ex(fb, chr_bytes, chr_byte_count, chr_bank, tile, x, y, scale, palette, false, false);
}
