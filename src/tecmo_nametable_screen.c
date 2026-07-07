#include "tecmo_nametable_screen.h"
#include "tecmo_nes_video.h"

uint16_t tecmo_nametable_map_direct(uint8_t ppu_tile, void *user)
{
    (void)user;
    return (uint16_t)ppu_tile;
}

uint16_t tecmo_nametable_map_low_tiles_to_table1(uint8_t ppu_tile, void *user)
{
    (void)user;
    if (ppu_tile < 0x80U) {
        return (uint16_t)(0x100U + (uint16_t)ppu_tile);
    }
    return (uint16_t)ppu_tile;
}

static uint8_t nametable_attribute_byte(const uint8_t attributes[64], int row, int col)
{
    size_t index;
    if (attributes == 0) {
        return 0U;
    }
    index = (size_t)(row / 4) * 8U + (size_t)(col / 4);
    if (index >= 64U) {
        return 0U;
    }
    return attributes[index];
}

static void build_palette(uint32_t out_palette[4],
                          const uint8_t background_palette[16],
                          uint8_t palette_index)
{
    uint8_t base = (uint8_t)((palette_index & 0x03U) * 4U);

    out_palette[0] = 0x00000000U;
    if (background_palette == 0) {
        out_palette[1] = 0xFFFFFFFFU;
        out_palette[2] = 0xFFFFFFFFU;
        out_palette[3] = 0xFFFFFFFFU;
        return;
    }

    for (size_t i = 1; i < 4U; ++i) {
        out_palette[i] = tecmo_nes_2c02_rgba(background_palette[(size_t)base + i]);
    }
}

bool tecmo_nametable_draw_tiles(TecmoFramebuffer *fb,
                                const uint8_t *chr_bytes,
                                uint64_t chr_byte_count,
                                uint32_t chr_bank,
                                const TecmoNametableTile *tiles,
                                size_t tile_count,
                                const uint8_t attributes[64],
                                const uint8_t background_palette[16],
                                TecmoNametableTileMapper tile_mapper,
                                void *tile_mapper_user,
                                int origin_x,
                                int origin_y,
                                int scale)
{
    if (fb == 0 || chr_bytes == 0 || chr_byte_count == 0 || tiles == 0 || scale <= 0) {
        return false;
    }
    if (tile_mapper == 0) {
        tile_mapper = tecmo_nametable_map_direct;
    }

    for (size_t i = 0; i < tile_count; ++i) {
        const TecmoNametableTile *entry = &tiles[i];
        uint16_t offset;
        int row;
        int col;
        uint8_t attribute;
        uint8_t palette_index;
        uint32_t palette[4];
        uint16_t chr_tile;

        if (entry->ppu < 0x2000U || entry->ppu >= 0x23C0U) {
            continue;
        }

        offset = (uint16_t)(entry->ppu - 0x2000U);
        row = (int)(offset / 32U);
        col = (int)(offset % 32U);
        attribute = nametable_attribute_byte(attributes, row, col);
        palette_index = tecmo_nes_attribute_palette_index(attribute, row, col);
        build_palette(palette, background_palette, palette_index);
        chr_tile = tile_mapper(entry->tile, tile_mapper_user);

        tecmo_draw_chr_tile_ex(fb,
                               chr_bytes,
                               chr_byte_count,
                               chr_bank,
                               chr_tile,
                               origin_x + col * 8 * scale,
                               origin_y + row * 8 * scale,
                               scale,
                               palette,
                               false,
                               false);
    }

    return true;
}
