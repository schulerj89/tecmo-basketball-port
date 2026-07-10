#ifndef TECMO_INTRO_FINALE_H
#define TECMO_INTRO_FINALE_H

#include "tecmo_framebuffer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_INTRO_FINALE_SCREEN_COUNT 5U
#define TECMO_INTRO_FINALE_PAGE_COUNT 2U
#define TECMO_INTRO_FINALE_WIDTH 32U
#define TECMO_INTRO_FINALE_HEIGHT 30U
#define TECMO_INTRO_FINALE_TILES_PER_PAGE \
    (TECMO_INTRO_FINALE_WIDTH * TECMO_INTRO_FINALE_HEIGHT)
#define TECMO_INTRO_FINALE_CELL_COUNT \
    (TECMO_INTRO_FINALE_PAGE_COUNT * TECMO_INTRO_FINALE_TILES_PER_PAGE)
#define TECMO_INTRO_FINALE_SPRITE_GROUP_COUNT 1U
#define TECMO_INTRO_FINALE_SPRITE_PALETTE_COUNT 2U
#define TECMO_INTRO_FINALE_PIECE_COUNT 10U
#define TECMO_INTRO_FINALE_PALETTE_STAGE_COUNT 5U
#define TECMO_INTRO_FINALE_TITLE_SLOT_COUNT 44U
#define TECMO_INTRO_FINALE_TITLE_TEXT_SLOT_COUNT 26U
#define TECMO_INTRO_FINALE_TITLE_CELL_COUNT 4U
#define TECMO_INTRO_FINALE_TITLE_BAND_COUNT 3U

#define TECMO_INTRO_FINALE_LOAD_BOUNDARY_FRAMES 1U
#define TECMO_INTRO_FINALE_OPENING_WAIT_FRAMES 50U
#define TECMO_INTRO_FINALE_SHORT_LOOP_STEPS 8U
#define TECMO_INTRO_FINALE_SHORT_LOOP_STEP_FRAMES 2U
#define TECMO_INTRO_FINALE_SHORT_LOOP_WAIT_FRAMES 30U
#define TECMO_INTRO_FINALE_TRANSITION_IN_FRAMES 18U
#define TECMO_INTRO_FINALE_TRANSITION_SWAP_FRAMES 1U
#define TECMO_INTRO_FINALE_TRANSITION_OUT_FRAMES 26U
#define TECMO_INTRO_FINALE_STAGED_WAIT_FRAMES 80U
#define TECMO_INTRO_FINALE_STAGED_DISPATCH_WAIT_FRAMES 75U
#define TECMO_INTRO_FINALE_TITLE_PREROLL_FRAMES 128U
#define TECMO_INTRO_FINALE_TITLE_SLOT_INTERVAL_FRAMES 8U
#define TECMO_INTRO_FINALE_TITLE_WRITE_FRAMES \
    (((TECMO_INTRO_FINALE_TITLE_SLOT_COUNT - 1U) * \
      TECMO_INTRO_FINALE_TITLE_SLOT_INTERVAL_FRAMES) + 1U)
#define TECMO_INTRO_FINALE_TITLE_TAIL_FRAMES 128U
#define TECMO_INTRO_FINALE_TITLE_DISPATCH_WAIT_FRAMES 1U

typedef struct TecmoIntroFinaleCell {
    uint8_t tile_id;
    uint8_t palette_index;
    uint32_t chr_offset;
} TecmoIntroFinaleCell;

typedef struct TecmoIntroFinalePiece {
    int16_t dx;
    int16_t dy;
    uint32_t top_chr_offset;
    uint32_t bottom_chr_offset;
    uint8_t palette_index;
    uint8_t flags;
} TecmoIntroFinalePiece;

typedef struct TecmoIntroFinaleTitleSlot {
    uint8_t page;
    uint8_t column;
    uint8_t row;
    TecmoIntroFinaleCell cells[TECMO_INTRO_FINALE_TITLE_CELL_COUNT];
} TecmoIntroFinaleTitleSlot;

typedef struct TecmoIntroFinaleTitleBand {
    uint16_t start_scanline;
    uint16_t end_scanline;
    uint8_t scroll_channel;
    uint8_t page_channel;
    uint32_t low_chr_base;
    uint32_t high_chr_base;
} TecmoIntroFinaleTitleBand;

typedef struct TecmoIntroFinaleAsset {
    bool available;
    TecmoIntroFinaleCell screens[TECMO_INTRO_FINALE_SCREEN_COUNT]
                                         [TECMO_INTRO_FINALE_CELL_COUNT];
    uint8_t palettes[TECMO_INTRO_FINALE_SCREEN_COUNT]
                    [TECMO_INTRO_FINALE_PALETTE_STAGE_COUNT][16];
    uint8_t palette_stage_count[TECMO_INTRO_FINALE_SCREEN_COUNT];
    uint8_t sprite_palettes[TECMO_INTRO_FINALE_SPRITE_PALETTE_COUNT][16];
    TecmoIntroFinalePiece pieces[TECMO_INTRO_FINALE_PIECE_COUNT];
    TecmoIntroFinaleTitleSlot title_slots[TECMO_INTRO_FINALE_TITLE_SLOT_COUNT];
    uint8_t short_anchor_x[TECMO_INTRO_FINALE_SHORT_LOOP_STEPS];
    uint8_t short_anchor_y[TECMO_INTRO_FINALE_SHORT_LOOP_STEPS];
    uint8_t reverse_initial_x;
    uint8_t reverse_second_x;
    int8_t reverse_delta_x;
    uint8_t reverse_anchor_y;
    uint8_t staged_anchor_x;
    uint8_t staged_anchor_y;
    uint16_t reverse_palette_frames[TECMO_INTRO_FINALE_PALETTE_STAGE_COUNT];
    TecmoIntroFinaleTitleBand title_bands[TECMO_INTRO_FINALE_TITLE_BAND_COUNT];
    char status[160];
} TecmoIntroFinaleAsset;

typedef enum TecmoIntroFinaleScene {
    TECMO_INTRO_FINALE_OPENING_SCREEN,
    TECMO_INTRO_FINALE_SHORT_SPRITE_LOOP,
    TECMO_INTRO_FINALE_SELECTOR_TRANSITION,
    TECMO_INTRO_FINALE_STAGED_GROUP,
    TECMO_INTRO_FINALE_TITLE,
    TECMO_INTRO_FINALE_TERMINATOR_HOLD
} TecmoIntroFinaleScene;

typedef enum TecmoIntroFinalePhase {
    TECMO_INTRO_FINALE_PHASE_LOAD,
    TECMO_INTRO_FINALE_PHASE_DISPATCH_WAIT,
    TECMO_INTRO_FINALE_PHASE_SHORT_LOOP,
    TECMO_INTRO_FINALE_PHASE_BLACK,
    TECMO_INTRO_FINALE_PHASE_FIRST_MOVE,
    TECMO_INTRO_FINALE_PHASE_HOLD,
    TECMO_INTRO_FINALE_PHASE_SECOND_MOVE,
    TECMO_INTRO_FINALE_PHASE_STAGED_WAIT,
    TECMO_INTRO_FINALE_PHASE_TITLE_PREROLL,
    TECMO_INTRO_FINALE_PHASE_TITLE_WRITE,
    TECMO_INTRO_FINALE_PHASE_TITLE_TAIL,
    TECMO_INTRO_FINALE_PHASE_TERMINATOR_HOLD
} TecmoIntroFinalePhase;

typedef struct TecmoIntroFinaleState {
    unsigned frame;
    unsigned scene_frame;
    TecmoIntroFinaleScene scene;
    TecmoIntroFinalePhase phase;
    uint8_t screen_index;
    uint8_t short_loop_step;
    uint8_t palette_stage;
    uint8_t sprite_variant_index;
    uint8_t title_slots_written;
    uint8_t scroll_x;
    uint8_t scroll_page;
    uint8_t secondary_scroll_x;
    uint8_t secondary_scroll_page;
    uint8_t player_x;
    uint8_t player_y;
    bool sprites_visible;
    bool black;
    bool persistent_hold;
} TecmoIntroFinaleState;

bool tecmo_intro_finale_asset_load(TecmoIntroFinaleAsset *asset,
                                   const char *project_root);
bool tecmo_intro_finale_asset_chr_available(const TecmoIntroFinaleAsset *asset,
                                            const uint8_t *chr_bytes,
                                            uint64_t chr_byte_count);
void tecmo_intro_finale_state(const TecmoIntroFinaleAsset *asset,
                              unsigned frame,
                              TecmoIntroFinaleState *state);
const char *tecmo_intro_finale_scene_name(TecmoIntroFinaleScene scene);
const char *tecmo_intro_finale_phase_name(TecmoIntroFinalePhase phase);
unsigned tecmo_intro_finale_hold_frame(void);
unsigned tecmo_intro_finale_scene_start_frame(TecmoIntroFinaleScene scene);

bool tecmo_intro_finale_draw(TecmoFramebuffer *fb,
                             const TecmoIntroFinaleAsset *asset,
                             const uint8_t *chr_bytes,
                             uint64_t chr_byte_count,
                             unsigned frame,
                             int origin_x,
                             int origin_y,
                             int scale);

#endif
