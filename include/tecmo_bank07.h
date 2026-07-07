#ifndef TECMO_BANK07_H
#define TECMO_BANK07_H

#include "tecmo_memory.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct TecmoBank07SpriteRecord {
    int relative_y;
    uint8_t tile;
    uint8_t attributes;
    int relative_x;
} TecmoBank07SpriteRecord;

typedef struct TecmoBank07SpriteStageConfig {
    int base_x;
    int base_y;
    uint8_t tile_offset;
    uint8_t attribute_or;
} TecmoBank07SpriteStageConfig;

typedef struct TecmoBank07OamSprite {
    uint8_t y;
    uint8_t tile;
    uint8_t attributes;
    uint8_t x;
} TecmoBank07OamSprite;

size_t tecmo_bank07_d861_stage_sprite_records(const TecmoBank07SpriteRecord *records,
                                               size_t record_count,
                                               const TecmoBank07SpriteStageConfig *config,
                                               TecmoBank07OamSprite *entries,
                                               size_t entry_capacity);
void tecmo_bank07_d2d2_hide_unused_oam(TecmoGameMemory *memory);
void tecmo_bank07_d700_copy_setup_bytes(TecmoGameMemory *memory, const uint8_t source[16]);
void tecmo_bank07_sprite_8x16_pair_for_table(uint8_t oam_tile_low,
                                             uint32_t chr_table,
                                             uint16_t out_tiles[2]);
bool tecmo_bank07_self_test(char *message, size_t message_size);

#endif
