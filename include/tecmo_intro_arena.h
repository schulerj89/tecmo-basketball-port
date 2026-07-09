#ifndef TECMO_INTRO_ARENA_H
#define TECMO_INTRO_ARENA_H

#include "tecmo_framebuffer.h"
#include "tecmo_intro_arena_scene.h"
#include "tecmo_intro_stage.h"
#include "tecmo_nametable_screen.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_INTRO_ARENA_PAGE_COUNT 2U
#define TECMO_INTRO_ARENA_TILES_PER_PAGE 960U
#define TECMO_INTRO_ARENA_PALETTE_STAGE_COUNT 8U
#define TECMO_INTRO_ARENA_MAX_SPRITES 64U
#define TECMO_INTRO_ARENA_MAX_SPRITE_STAGES 128U
#define TECMO_INTRO_ARENA_NATIVE_SPRITE_GROUP_COUNT 2U
#define TECMO_INTRO_ARENA_NATIVE_SPRITE_PIECE_COUNT 71U

typedef enum TecmoArenaNativeSpriteGroupKind {
    TECMO_ARENA_NATIVE_SPRITE_GROUP_JUMBOTRON = 1,
    TECMO_ARENA_NATIVE_SPRITE_GROUP_GOAL = 2
} TecmoArenaNativeSpriteGroupKind;

typedef enum TecmoArenaNativeSpritePieceFlags {
    TECMO_ARENA_NATIVE_SPRITE_FLIP_HORIZONTAL = 0x01U,
    TECMO_ARENA_NATIVE_SPRITE_FLIP_VERTICAL = 0x02U,
    TECMO_ARENA_NATIVE_SPRITE_PRIORITY = 0x04U
} TecmoArenaNativeSpritePieceFlags;

typedef struct TecmoArenaNativeSpriteGroup {
    TecmoArenaNativeSpriteGroupKind kind;
    unsigned draw_order;
    size_t first_piece;
    size_t piece_count;
    int anchor_x;
    int anchor_y;
    int camera_x_multiplier;
    int camera_y_multiplier;
} TecmoArenaNativeSpriteGroup;

typedef struct TecmoArenaNativeSpritePiece {
    int dx;
    int dy;
    uint32_t top_chr_offset;
    uint8_t palette_index;
    uint8_t flags;
} TecmoArenaNativeSpritePiece;

typedef struct TecmoArenaNativeSpriteGroups {
    bool available;
    uint8_t palette[16];
    TecmoArenaNativeSpriteGroup groups[TECMO_INTRO_ARENA_NATIVE_SPRITE_GROUP_COUNT];
    TecmoArenaNativeSpritePiece pieces[TECMO_INTRO_ARENA_NATIVE_SPRITE_PIECE_COUNT];
    size_t group_count;
    size_t piece_count;
    char status[160];
} TecmoArenaNativeSpriteGroups;

typedef struct TecmoArenaNativeSpriteVisibleCounts {
    size_t jumbotron;
    size_t goal;
} TecmoArenaNativeSpriteVisibleCounts;

typedef struct TecmoIntroArenaSprite {
    uint8_t y;
    uint8_t tile;
    uint8_t attributes;
    uint8_t x;
} TecmoIntroArenaSprite;

typedef struct TecmoIntroArenaSpriteStage {
    unsigned capture_frame;
    TecmoIntroArenaSprite sprites[TECMO_INTRO_ARENA_MAX_SPRITES];
    size_t sprite_count;
} TecmoIntroArenaSpriteStage;

typedef struct TecmoIntroArenaCapture {
    bool available;
    bool palette_available;
    uint32_t chr_bank;
    uint32_t page_chr_bank[TECMO_INTRO_ARENA_PAGE_COUNT];
    uint8_t bg_upper_r0;
    uint8_t bg_upper_r1;
    uint8_t bg_lower_r0;
    uint8_t bg_lower_r1;
    int bg_split_row;
    TecmoNametableTile tiles[TECMO_INTRO_ARENA_PAGE_COUNT][TECMO_INTRO_ARENA_TILES_PER_PAGE];
    size_t tile_count[TECMO_INTRO_ARENA_PAGE_COUNT];
    TecmoIntroArenaSprite sprites[TECMO_INTRO_ARENA_MAX_SPRITES];
    size_t sprite_count;
    TecmoIntroArenaSpriteStage sprite_stages[TECMO_INTRO_ARENA_MAX_SPRITE_STAGES];
    size_t sprite_stage_count;
    uint32_t sprite_chr_bank;
    uint8_t attributes[TECMO_INTRO_ARENA_PAGE_COUNT][64];
    uint8_t palette_stages[TECMO_INTRO_ARENA_PALETTE_STAGE_COUNT][16];
    uint8_t sprite_palette_stages[TECMO_INTRO_ARENA_PALETTE_STAGE_COUNT][16];
    unsigned palette_stage_offsets[TECMO_INTRO_ARENA_PALETTE_STAGE_COUNT];
    size_t palette_stage_count;
    unsigned first_capture_frame;
    unsigned last_capture_frame;
    char status[512];
} TecmoIntroArenaCapture;

bool tecmo_intro_arena_capture_load(TecmoIntroArenaCapture *capture, const char *project_root);
bool tecmo_intro_arena_tile_layer_load(TecmoArenaTileLayer *layer, const char *project_root);
bool tecmo_intro_arena_sprite_groups_load(TecmoArenaNativeSpriteGroups *sprite_groups,
                                          const char *project_root);

unsigned tecmo_intro_arena_display_frame(unsigned native_frame);

const uint8_t *tecmo_intro_arena_palette_for_frame(const TecmoIntroArenaCapture *capture,
                                                   unsigned frame);

bool tecmo_intro_arena_tile_layer_chr_available(const TecmoArenaTileLayer *layer,
                                                const uint8_t *chr_bytes,
                                                uint64_t chr_byte_count);

size_t tecmo_intro_arena_native_sprite_group_count(
    const TecmoArenaNativeSpriteGroups *sprite_groups);
const TecmoArenaNativeSpriteGroup *tecmo_intro_arena_native_sprite_group(
    const TecmoArenaNativeSpriteGroups *sprite_groups,
    TecmoArenaNativeSpriteGroupKind kind);
size_t tecmo_intro_arena_native_sprite_piece_count(
    const TecmoArenaNativeSpriteGroups *sprite_groups,
    TecmoArenaNativeSpriteGroupKind kind);
bool tecmo_intro_arena_native_sprite_chr_available(
    const TecmoArenaNativeSpriteGroups *sprite_groups,
    const uint8_t *chr_bytes,
    uint64_t chr_byte_count);
TecmoArenaNativeSpriteVisibleCounts tecmo_intro_arena_native_sprite_visible_counts(
    const TecmoArenaNativeSpriteGroups *sprite_groups,
    const TecmoIntroArenaTransitionState *state);

bool tecmo_intro_arena_draw_page(TecmoFramebuffer *fb,
                                 const TecmoIntroArenaCapture *capture,
                                 const uint8_t *chr_bytes,
                                 uint64_t chr_byte_count,
                                 unsigned page,
                                 unsigned frame,
                                 int origin_x,
                                 int origin_y,
                                 int scale);

bool tecmo_intro_arena_draw_composite(TecmoFramebuffer *fb,
                                      const TecmoIntroArenaCapture *capture,
                                      const uint8_t *chr_bytes,
                                      uint64_t chr_byte_count,
                                      unsigned frame,
                                      int origin_x,
                                      int origin_y,
                                      int scale);

bool tecmo_intro_arena_draw_native_chr(TecmoFramebuffer *fb,
                                       const TecmoArenaTileLayer *layer,
                                       const uint8_t *chr_bytes,
                                       uint64_t chr_byte_count,
                                       unsigned frame,
                                       int origin_x,
                                       int origin_y,
                                       int scale);

TecmoArenaNativeSpriteVisibleCounts tecmo_intro_arena_draw_native_sprite_groups(
    TecmoFramebuffer *fb,
    const TecmoArenaNativeSpriteGroups *sprite_groups,
    const uint8_t *chr_bytes,
    uint64_t chr_byte_count,
    const TecmoIntroArenaTransitionState *state,
    int origin_x,
    int origin_y,
    int scale);

size_t tecmo_intro_arena_draw_sprites(TecmoFramebuffer *fb,
                                      const TecmoIntroArenaCapture *capture,
                                      const uint8_t *chr_bytes,
                                      uint64_t chr_byte_count,
                                      unsigned frame,
                                      int origin_x,
                                      int origin_y,
                                      int scale);

#endif
