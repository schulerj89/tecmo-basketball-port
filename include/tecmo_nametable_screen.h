#ifndef TECMO_NAMETABLE_SCREEN_H
#define TECMO_NAMETABLE_SCREEN_H

#include "tecmo_framebuffer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct TecmoNametableTile {
    uint16_t ppu;
    uint8_t tile;
} TecmoNametableTile;

typedef uint16_t (*TecmoNametableTileMapper)(uint8_t ppu_tile, void *user);

uint16_t tecmo_nametable_map_direct(uint8_t ppu_tile, void *user);
uint16_t tecmo_nametable_map_low_tiles_to_table1(uint8_t ppu_tile, void *user);

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
                                int scale);

#endif
