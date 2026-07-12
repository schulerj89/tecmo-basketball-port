#ifndef TECMO_TITLE_SCREEN_H
#define TECMO_TITLE_SCREEN_H

#include "tecmo_framebuffer.h"

#include <stdbool.h>
#include <stdint.h>

#define TECMO_TITLE_CELL_COUNT 960U
#define TECMO_TITLE_SPRITE_COUNT 49U
#define TECMO_TITLE_START_LOAD_FRAMES 10U

typedef struct TecmoTitleCell {
    uint8_t tile_id;
    uint8_t palette_index;
    uint32_t chr_offset;
} TecmoTitleCell;

typedef struct TecmoTitleSprite {
    int16_t dx;
    int16_t dy;
    uint32_t top_chr_offset;
    uint32_t bottom_chr_offset;
    uint8_t palette_index;
    uint8_t flags;
} TecmoTitleSprite;

typedef struct TecmoTitleAsset {
    bool attract_available;
    bool start_available;
    TecmoTitleCell attract_cells[TECMO_TITLE_CELL_COUNT];
    TecmoTitleCell start_cells[TECMO_TITLE_CELL_COUNT];
    uint8_t attract_palette[48];
    uint8_t start_palette[16];
    TecmoTitleSprite sprites[TECMO_TITLE_SPRITE_COUNT];
    uint8_t attract_attributes[2][16];
    uint8_t prompt_rows[2][10];
    uint64_t chr_byte_count;
    uint64_t chr_fingerprint;
    char status[160];
} TecmoTitleAsset;

bool tecmo_title_asset_load(TecmoTitleAsset *asset, const char *project_root);
bool tecmo_title_asset_chr_available(const TecmoTitleAsset *asset,
                                     const uint8_t *chr_bytes,
                                     uint64_t chr_byte_count);
bool tecmo_title_attract_draw(TecmoFramebuffer *fb,
                              const TecmoTitleAsset *asset,
                              const uint8_t *chr_bytes,
                              uint64_t chr_byte_count,
                              unsigned frame,
                              int origin_x, int origin_y, int scale);
bool tecmo_title_start_draw(TecmoFramebuffer *fb,
                            const TecmoTitleAsset *asset,
                            const uint8_t *chr_bytes,
                            uint64_t chr_byte_count,
                            bool confirming,
                            unsigned confirmation_frame,
                            int origin_x, int origin_y, int scale);
unsigned tecmo_title_attract_reset_frame(void);
bool tecmo_title_attract_natural_complete(unsigned frame);
unsigned tecmo_title_confirmation_handoff_frame(void);

#endif
