#ifndef TECMO_INTRO_POST_ARENA_H
#define TECMO_INTRO_POST_ARENA_H

#include "tecmo_framebuffer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_INTRO_POST_PAGE_COUNT 2U
#define TECMO_INTRO_POST_TILES_PER_PAGE 960U
#define TECMO_INTRO_POST_MAX_TILE_EVENTS 4096U
#define TECMO_INTRO_POST_MAX_ATTRIBUTE_EVENTS 1024U
#define TECMO_INTRO_POST_MAX_PALETTE_STAGES 32U
#define TECMO_INTRO_POST_MAX_SPRITES 64U
#define TECMO_INTRO_POST_MAX_SPRITE_STAGES 128U
#define TECMO_INTRO_READY_CAPTURE_FIRST 1034U
#define TECMO_INTRO_READY_CAPTURE_LAST 1091U
#define TECMO_INTRO_WARRIORS_CAPTURE_FIRST 1092U
#define TECMO_INTRO_WARRIORS_CAPTURE_LAST 1305U

typedef struct TecmoIntroPostMapperState {
    uint8_t regs[8];
} TecmoIntroPostMapperState;

typedef struct TecmoIntroPostTileEvent {
    unsigned frame;
    uint16_t ppu;
    uint8_t tile;
    TecmoIntroPostMapperState mapper;
} TecmoIntroPostTileEvent;

typedef struct TecmoIntroPostAttributeEvent {
    unsigned frame;
    uint16_t ppu;
    uint8_t value;
} TecmoIntroPostAttributeEvent;

typedef struct TecmoIntroPostPaletteStage {
    unsigned frame;
    uint8_t background[16];
    uint8_t sprites[16];
} TecmoIntroPostPaletteStage;

typedef struct TecmoIntroPostSprite {
    uint8_t y;
    uint8_t tile;
    uint8_t attributes;
    uint8_t x;
} TecmoIntroPostSprite;

typedef struct TecmoIntroPostSpriteStage {
    unsigned frame;
    TecmoIntroPostMapperState mapper;
    TecmoIntroPostSprite sprites[TECMO_INTRO_POST_MAX_SPRITES];
    size_t sprite_count;
} TecmoIntroPostSpriteStage;

typedef struct TecmoIntroPostScrollStage {
    unsigned frame;
    int scroll_x;
} TecmoIntroPostScrollStage;

typedef struct TecmoIntroPostArenaCapture {
    bool available;
    bool truncated;
    TecmoIntroPostTileEvent tile_events[TECMO_INTRO_POST_MAX_TILE_EVENTS];
    size_t tile_event_count;
    TecmoIntroPostAttributeEvent attribute_events[TECMO_INTRO_POST_MAX_ATTRIBUTE_EVENTS];
    size_t attribute_event_count;
    TecmoIntroPostPaletteStage palette_stages[TECMO_INTRO_POST_MAX_PALETTE_STAGES];
    size_t palette_stage_count;
    TecmoIntroPostSpriteStage sprite_stages[TECMO_INTRO_POST_MAX_SPRITE_STAGES];
    size_t sprite_stage_count;
    TecmoIntroPostScrollStage scroll_stages[TECMO_INTRO_POST_MAX_SPRITE_STAGES];
    size_t scroll_stage_count;
    unsigned first_capture_frame;
    unsigned last_capture_frame;
    char status[512];
} TecmoIntroPostArenaCapture;

bool tecmo_intro_post_arena_capture_load(TecmoIntroPostArenaCapture *capture,
                                         const char *project_root);

void tecmo_intro_post_arena_draw_ready(TecmoFramebuffer *fb,
                                       const TecmoIntroPostArenaCapture *capture,
                                       const uint8_t *chr_bytes,
                                       uint64_t chr_byte_count,
                                       unsigned native_frame,
                                       int origin_x,
                                       int origin_y,
                                       int scale);

void tecmo_intro_post_arena_draw_warriors(TecmoFramebuffer *fb,
                                          const TecmoIntroPostArenaCapture *capture,
                                          const uint8_t *chr_bytes,
                                          uint64_t chr_byte_count,
                                          unsigned native_frame,
                                          int origin_x,
                                          int origin_y,
                                          int scale);

unsigned tecmo_intro_ready_capture_frame(unsigned native_frame);
unsigned tecmo_intro_warriors_capture_frame(unsigned native_frame);

#endif
