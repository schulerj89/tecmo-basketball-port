#ifndef TECMO_INTRO_POST_ARENA_H
#define TECMO_INTRO_POST_ARENA_H

#include "tecmo_framebuffer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_INTRO_READY_WIDTH 32U
#define TECMO_INTRO_READY_HEIGHT 30U
#define TECMO_INTRO_READY_CELL_COUNT (TECMO_INTRO_READY_WIDTH * TECMO_INTRO_READY_HEIGHT)
#define TECMO_INTRO_READY_PALETTE_STAGE_COUNT 5U
#define TECMO_INTRO_READY_MASK_COUNT 12U
#define TECMO_INTRO_READY_MASK_SLOT_COUNT 8U
#define TECMO_INTRO_READY_HANDOFF_FRAME 58U

#define TECMO_INTRO_WARRIORS_PAGE_COUNT 2U
#define TECMO_INTRO_WARRIORS_TILES_PER_PAGE 960U
#define TECMO_INTRO_WARRIORS_PIECE_COUNT 46U
#define TECMO_INTRO_WARRIORS_PATCH_COUNT 2U
#define TECMO_INTRO_WARRIORS_PATCH_TILE_COUNT 64U
#define TECMO_INTRO_WARRIORS_WORDMARK_GLYPH_COUNT 8U
#define TECMO_INTRO_WARRIORS_WORDMARK_TILE_COUNT 32U
#define TECMO_INTRO_WARRIORS_SPLIT_SCANLINE 168U
#define TECMO_INTRO_WARRIORS_HANDOFF_FRAME 214U
#define TECMO_INTRO_WARRIORS_NEXT_SCREEN 0x1BU

#define TECMO_INTRO_CLIPPERS_PAGE_COUNT 2U
#define TECMO_INTRO_CLIPPERS_TILES_PER_PAGE 960U
#define TECMO_INTRO_CLIPPERS_PALETTE_STAGE_COUNT 4U
#define TECMO_INTRO_CLIPPERS_LOWER_SPLIT_SCANLINE 200U
#define TECMO_INTRO_CLIPPERS_MOTION_FRAME 40U
#define TECMO_INTRO_CLIPPERS_MOTION_TICK_FRAMES 2U
#define TECMO_INTRO_CLIPPERS_MOTION_TICK_COUNT 41U
#define TECMO_INTRO_CLIPPERS_POSE_SWITCH_TICK 20U
#define TECMO_INTRO_CLIPPERS_WORDMARK_TILE_COUNT 32U
#define TECMO_INTRO_CLIPPERS_WORDMARK_FRAME 32U
#define TECMO_INTRO_CLIPPERS_HANDOFF_FRAME 151U
#define TECMO_INTRO_CLIPPERS_NEXT_ROUTE 0x883DU

#define TECMO_INTRO_BUCKS_PAGE_COUNT 2U
#define TECMO_INTRO_BUCKS_TILES_PER_PAGE 960U
#define TECMO_INTRO_BUCKS_PALETTE_STAGE_COUNT 4U
#define TECMO_INTRO_BUCKS_WORDMARK_GLYPH_COUNT 5U
#define TECMO_INTRO_BUCKS_WORDMARK_TILE_COUNT 20U
#define TECMO_INTRO_BUCKS_TOP_SPLIT_SCANLINE 31U
#define TECMO_INTRO_BUCKS_LOWER_SPLIT_SCANLINE 168U
#define TECMO_INTRO_BUCKS_WORDMARK_FRAME 10U
#define TECMO_INTRO_BUCKS_FULL_FRAME 14U
#define TECMO_INTRO_BUCKS_HANDOFF_FRAME 83U
#define TECMO_INTRO_BUCKS_NEXT_ROUTE 0x854FU

#define TECMO_INTRO_PASS_PAGE_COUNT 2U
#define TECMO_INTRO_PASS_TILES_PER_PAGE 960U
#define TECMO_INTRO_PASS_PALETTE_STAGE_COUNT 5U
#define TECMO_INTRO_PASS_PIECE_COUNT 46U
#define TECMO_INTRO_PASS_FIRST_MOVE_FRAMES 18U
#define TECMO_INTRO_PASS_SECOND_MOVE_FRAMES 30U
#define TECMO_INTRO_PASS_INITIAL_X 0x68U
#define TECMO_INTRO_PASS_MOVE_DELTA 8U
#define TECMO_INTRO_PASS_HANDOFF_FRAME 52U
#define TECMO_INTRO_PASS_NEXT_ROUTE 0x851CU

typedef struct TecmoIntroNativeTile {
    uint8_t tile_id;
    uint8_t palette_index;
    uint32_t chr_offset;
} TecmoIntroNativeTile;

typedef struct TecmoIntroReadyAsset {
    bool available;
    TecmoIntroNativeTile cells[TECMO_INTRO_READY_CELL_COUNT];
    uint8_t palettes[TECMO_INTRO_READY_PALETTE_STAGE_COUNT][16];
    uint8_t palette_frames[TECMO_INTRO_READY_PALETTE_STAGE_COUNT];
    uint8_t masks[TECMO_INTRO_READY_MASK_COUNT][TECMO_INTRO_READY_MASK_SLOT_COUNT];
    char status[160];
} TecmoIntroReadyAsset;

typedef struct TecmoIntroReadyState {
    unsigned frame;
    uint8_t palette_stage;
    uint8_t mask_index;
    bool black;
    bool handoff;
} TecmoIntroReadyState;

typedef struct TecmoIntroWarriorsTile {
    uint8_t tile_id;
    uint8_t palette_index;
    uint32_t moving_chr_offset;
    uint32_t lower_chr_offset;
} TecmoIntroWarriorsTile;

typedef struct TecmoIntroWarriorsPiece {
    int16_t dx;
    int16_t dy;
    uint32_t top_chr_offset;
    uint32_t bottom_chr_offset;
    uint8_t palette_index;
    uint8_t flags;
} TecmoIntroWarriorsPiece;

typedef struct TecmoIntroWarriorsPatchTile {
    uint8_t tile_id;
    uint8_t palette_index;
    uint32_t chr_offset;
} TecmoIntroWarriorsPatchTile;

typedef struct TecmoIntroWarriorsAsset {
    bool available;
    TecmoIntroWarriorsTile pages[TECMO_INTRO_WARRIORS_PAGE_COUNT]
                                         [TECMO_INTRO_WARRIORS_TILES_PER_PAGE];
    uint8_t background_palette[16];
    uint8_t sprite_palette[16];
    TecmoIntroWarriorsPiece pieces[TECMO_INTRO_WARRIORS_PIECE_COUNT];
    TecmoIntroWarriorsPatchTile patches[TECMO_INTRO_WARRIORS_PATCH_COUNT]
                                                [TECMO_INTRO_WARRIORS_PATCH_TILE_COUNT];
    TecmoIntroWarriorsPatchTile wordmark[TECMO_INTRO_WARRIORS_WORDMARK_TILE_COUNT];
    char status[160];
} TecmoIntroWarriorsAsset;

typedef enum TecmoIntroWarriorsPhase {
    TECMO_INTRO_WARRIORS_LOAD,
    TECMO_INTRO_WARRIORS_FADE,
    TECMO_INTRO_WARRIORS_WORDMARK,
    TECMO_INTRO_WARRIORS_PAN,
    TECMO_INTRO_WARRIORS_HOLD,
    TECMO_INTRO_WARRIORS_PATCH_ONE,
    TECMO_INTRO_WARRIORS_PATCH_TWO,
    TECMO_INTRO_WARRIORS_BLACK,
    TECMO_INTRO_WARRIORS_HANDOFF
} TecmoIntroWarriorsPhase;

typedef struct TecmoIntroWarriorsState {
    unsigned frame;
    TecmoIntroWarriorsPhase phase;
    uint8_t palette_stage;
    uint8_t pan;
    uint8_t patch_count;
    uint8_t wordmark_glyph_count;
    bool sprites_visible;
    bool wordmark_visible;
    bool black;
    bool handoff;
    uint8_t next_screen;
} TecmoIntroWarriorsState;

typedef struct TecmoIntroClippersTile {
    uint8_t tile_id;
    uint8_t palette_index;
    uint32_t base_chr_offset;
    uint32_t lower_chr_offset;
} TecmoIntroClippersTile;

typedef struct TecmoIntroClippersAsset {
    bool available;
    TecmoIntroClippersTile pages[TECMO_INTRO_CLIPPERS_PAGE_COUNT]
                                         [TECMO_INTRO_CLIPPERS_TILES_PER_PAGE];
    TecmoIntroClippersTile wordmark[TECMO_INTRO_CLIPPERS_WORDMARK_TILE_COUNT];
    uint8_t palettes[TECMO_INTRO_CLIPPERS_PALETTE_STAGE_COUNT][16];
    char status[160];
} TecmoIntroClippersAsset;

typedef struct TecmoIntroClippersState {
    unsigned frame;
    uint8_t palette_stage;
    uint8_t motion;
    uint8_t scroll_x;
    uint8_t pose_page;
    bool wordmark_visible;
    bool handoff;
    uint16_t next_route;
} TecmoIntroClippersState;

typedef struct TecmoIntroBucksAsset {
    bool available;
    TecmoIntroClippersTile pages[TECMO_INTRO_BUCKS_PAGE_COUNT]
                                      [TECMO_INTRO_BUCKS_TILES_PER_PAGE];
    TecmoIntroClippersTile wordmark[TECMO_INTRO_BUCKS_WORDMARK_TILE_COUNT];
    uint8_t palettes[TECMO_INTRO_BUCKS_PALETTE_STAGE_COUNT][16];
    uint8_t thresholds[6];
    char status[160];
} TecmoIntroBucksAsset;

typedef struct TecmoIntroBucksState {
    unsigned frame;
    uint8_t palette_stage;
    uint8_t scroll_x;
    uint8_t flash_pass;
    uint8_t wordmark_glyph_count;
    bool prior;
    bool black;
    bool handoff;
    uint16_t next_route;
} TecmoIntroBucksState;

typedef struct TecmoIntroPassTile {
    uint8_t tile_id;
    uint8_t palette_index;
    uint32_t chr_offset;
} TecmoIntroPassTile;

typedef struct TecmoIntroPassPiece {
    int16_t dx;
    int16_t dy;
    uint32_t top_chr_offset;
    uint32_t bottom_chr_offset;
    uint8_t palette_index;
    uint8_t flags;
} TecmoIntroPassPiece;

typedef enum TecmoIntroPassPhase {
    TECMO_INTRO_PASS_PRIOR,
    TECMO_INTRO_PASS_BLACK,
    TECMO_INTRO_PASS_FIRST_MOVE,
    TECMO_INTRO_PASS_SECOND_MOVE,
    TECMO_INTRO_PASS_HOLD,
    TECMO_INTRO_PASS_HANDOFF
} TecmoIntroPassPhase;

typedef struct TecmoIntroPassAsset {
    bool available;
    TecmoIntroPassTile pages[TECMO_INTRO_PASS_PAGE_COUNT]
                                  [TECMO_INTRO_PASS_TILES_PER_PAGE];
    TecmoIntroPassPiece pieces[TECMO_INTRO_PASS_PIECE_COUNT];
    uint8_t palettes[TECMO_INTRO_PASS_PALETTE_STAGE_COUNT][16];
    uint8_t sprite_palette[16];
    char status[160];
} TecmoIntroPassAsset;

typedef struct TecmoIntroPassState {
    unsigned frame;
    TecmoIntroPassPhase phase;
    uint8_t palette_stage;
    uint8_t player_x;
    uint8_t scroll_x;
    uint8_t first_move_count;
    uint8_t second_move_count;
    bool sprites_visible;
    bool black;
    bool handoff;
    uint16_t next_route;
} TecmoIntroPassState;

bool tecmo_intro_ready_asset_load(TecmoIntroReadyAsset *asset, const char *project_root);
bool tecmo_intro_warriors_asset_load(TecmoIntroWarriorsAsset *asset,
                                     const char *project_root);
bool tecmo_intro_clippers_asset_load(TecmoIntroClippersAsset *asset,
                                     const char *project_root);
bool tecmo_intro_bucks_asset_load(TecmoIntroBucksAsset *asset,
                                  const char *project_root);
bool tecmo_intro_pass_asset_load(TecmoIntroPassAsset *asset,
                                 const char *project_root);

void tecmo_intro_ready_state(unsigned frame, TecmoIntroReadyState *state);
void tecmo_intro_warriors_state(unsigned frame, TecmoIntroWarriorsState *state);
void tecmo_intro_clippers_state(unsigned frame, TecmoIntroClippersState *state);
void tecmo_intro_bucks_state(unsigned frame, TecmoIntroBucksState *state);
void tecmo_intro_pass_state(unsigned frame, TecmoIntroPassState *state);
const char *tecmo_intro_warriors_phase_name(TecmoIntroWarriorsPhase phase);
const char *tecmo_intro_pass_phase_name(TecmoIntroPassPhase phase);

bool tecmo_intro_post_arena_draw_ready(TecmoFramebuffer *fb,
                                       const TecmoIntroReadyAsset *asset,
                                       const uint8_t *chr_bytes,
                                       uint64_t chr_byte_count,
                                       unsigned frame,
                                       int origin_x,
                                       int origin_y,
                                       int scale);

bool tecmo_intro_post_arena_draw_warriors(TecmoFramebuffer *fb,
                                          const TecmoIntroWarriorsAsset *asset,
                                          const uint8_t *chr_bytes,
                                          uint64_t chr_byte_count,
                                          unsigned frame,
                                          int origin_x,
                                          int origin_y,
                                          int scale);

bool tecmo_intro_post_arena_draw_clippers(TecmoFramebuffer *fb,
                                          const TecmoIntroClippersAsset *asset,
                                          const uint8_t *chr_bytes,
                                          uint64_t chr_byte_count,
                                          unsigned frame,
                                          int origin_x,
                                          int origin_y,
                                          int scale);

bool tecmo_intro_post_arena_draw_bucks(TecmoFramebuffer *fb,
                                       const TecmoIntroBucksAsset *asset,
                                       const uint8_t *chr_bytes,
                                       uint64_t chr_byte_count,
                                       unsigned frame,
                                       int origin_x,
                                       int origin_y,
                                       int scale);

bool tecmo_intro_post_arena_draw_pass(TecmoFramebuffer *fb,
                                      const TecmoIntroPassAsset *asset,
                                      const uint8_t *chr_bytes,
                                      uint64_t chr_byte_count,
                                      unsigned frame,
                                      int origin_x,
                                      int origin_y,
                                      int scale);

#endif
