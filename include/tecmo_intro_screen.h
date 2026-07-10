#ifndef TECMO_INTRO_SCREEN_H
#define TECMO_INTRO_SCREEN_H

#include "tecmo_framebuffer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_INTRO_PRESENTS_ENTRY_ID "intro/tecmo-presents-screen"
#define TECMO_INTRO_LICENSE_ENTRY_ID "intro/nba-license-screen"
#define TECMO_INTRO_SCREEN_WIDTH 32U
#define TECMO_INTRO_SCREEN_HEIGHT 30U
#define TECMO_INTRO_SCREEN_CELL_COUNT \
    (TECMO_INTRO_SCREEN_WIDTH * TECMO_INTRO_SCREEN_HEIGHT)
#define TECMO_INTRO_SCREEN_MAX_PALETTE_STAGES 9U
#define TECMO_INTRO_SCREEN_MAX_PALETTE_COLORS 32U
#define TECMO_INTRO_SCREEN_MAX_SPRITES 20U
#define TECMO_INTRO_SCREEN_SPRITE_FLIP_HORIZONTAL 0x01U
#define TECMO_INTRO_SCREEN_SPRITE_FLIP_VERTICAL 0x02U

typedef enum TecmoIntroScreenKind {
    TECMO_INTRO_SCREEN_PRESENTS = 0,
    TECMO_INTRO_SCREEN_LICENSE = 1
} TecmoIntroScreenKind;

typedef struct TecmoIntroScreenCell {
    uint8_t tile_id;
    uint8_t palette_index;
    uint32_t chr_offset;
} TecmoIntroScreenCell;

typedef struct TecmoIntroScreenSprite {
    int16_t x;
    int16_t y;
    uint32_t chr_offset;
    uint8_t palette_index;
    uint8_t flags;
} TecmoIntroScreenSprite;

typedef struct TecmoIntroScreenAsset {
    bool available;
    TecmoIntroScreenKind kind;
    uint8_t palette_stage_count;
    uint8_t palette_stride;
    uint16_t duration_frames;
    uint16_t palette_frames[TECMO_INTRO_SCREEN_MAX_PALETTE_STAGES];
    uint8_t palettes[TECMO_INTRO_SCREEN_MAX_PALETTE_STAGES]
                    [TECMO_INTRO_SCREEN_MAX_PALETTE_COLORS];
    TecmoIntroScreenCell cells[TECMO_INTRO_SCREEN_CELL_COUNT];
    uint16_t sprite_count;
    uint16_t sprite_first_frame;
    uint16_t sprite_hide_frame;
    TecmoIntroScreenSprite sprites[TECMO_INTRO_SCREEN_MAX_SPRITES];
    uint64_t chr_byte_count;
    uint64_t chr_fingerprint;
    char status[160];
} TecmoIntroScreenAsset;

bool tecmo_intro_screen_load(TecmoIntroScreenAsset *asset,
                             const char *project_root,
                             const char *entry_id,
                             TecmoIntroScreenKind expected_kind);
bool tecmo_intro_screen_chr_available(const TecmoIntroScreenAsset *asset,
                                      const uint8_t *chr_bytes,
                                      uint64_t chr_byte_count);
uint8_t tecmo_intro_screen_palette_stage(const TecmoIntroScreenAsset *asset,
                                         unsigned frame);
bool tecmo_intro_screen_draw(TecmoFramebuffer *fb,
                             const TecmoIntroScreenAsset *asset,
                             const uint8_t *chr_bytes,
                             uint64_t chr_byte_count,
                             unsigned frame,
                             int origin_x,
                             int origin_y,
                             int scale);

#endif
