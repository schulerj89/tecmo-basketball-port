#include "tecmo_nes_video.h"

#include <stddef.h>

#define TECMO_NES_CHR_BANK_BYTES 8192U

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
    static const uint32_t colors[64] = {
        0xFF7C7C7CU, 0xFF0000FCU, 0xFF0000BCU, 0xFF4428BCU,
        0xFF940084U, 0xFFA80020U, 0xFFA81000U, 0xFF881400U,
        0xFF503000U, 0xFF007800U, 0xFF006800U, 0xFF005800U,
        0xFF004058U, 0xFF000000U, 0xFF000000U, 0xFF000000U,
        0xFFBCBCBCU, 0xFF0078F8U, 0xFF0058F8U, 0xFF6844FCU,
        0xFFD800CCU, 0xFFE40058U, 0xFFF83800U, 0xFFE45C10U,
        0xFFAC7C00U, 0xFF00B800U, 0xFF00A800U, 0xFF00A844U,
        0xFF008888U, 0xFF000000U, 0xFF000000U, 0xFF000000U,
        0xFFF8F8F8U, 0xFF3CBCFCU, 0xFF6888FCU, 0xFF9878F8U,
        0xFFF878F8U, 0xFFF85898U, 0xFFF87858U, 0xFFFCA044U,
        0xFFF8B800U, 0xFFB8F818U, 0xFF58D854U, 0xFF58F898U,
        0xFF00E8D8U, 0xFF787878U, 0xFF000000U, 0xFF000000U,
        0xFFFCFCFCU, 0xFFA4E4FCU, 0xFFB8B8F8U, 0xFFD8B8F8U,
        0xFFF8B8F8U, 0xFFF8A4C0U, 0xFFF0D0B0U, 0xFFFCE0A8U,
        0xFFF8D878U, 0xFFD8F878U, 0xFFB8F8B8U, 0xFFB8F8D8U,
        0xFF00FCFCU, 0xFFF8D8F8U, 0xFF000000U, 0xFF000000U,
    };

    return colors[color & 0x3FU];
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
