#ifndef TECMO_START_GAME_MENU_H
#define TECMO_START_GAME_MENU_H

#include "tecmo_controls.h"
#include "tecmo_framebuffer.h"
#include "tecmo_title_screen.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_START_GAME_MENU_CELL_COUNT 1920U
#define TECMO_START_GAME_MENU_PALETTE_STAGE_COUNT 9U
#define TECMO_START_GAME_MENU_EMBLEM_COUNT 49U
#define TECMO_START_GAME_MENU_OVERLAY_COUNT 3U
#define TECMO_START_GAME_MENU_OVERLAY_CELL_COUNT 206U
#define TECMO_START_GAME_MENU_DIGIT_COUNT 10U
#define TECMO_START_GAME_MENU_ROOT_COUNT 7U
#define TECMO_START_GAME_MENU_SEASON_COUNT 6U

typedef struct TecmoStartGameMenuCell {
    uint8_t tile_id;
    uint8_t palette_index;
    uint32_t chr_offset;
} TecmoStartGameMenuCell;

typedef struct TecmoStartGameMenuSprite {
    int16_t dx;
    int16_t dy;
    uint32_t top_chr_offset;
    uint32_t bottom_chr_offset;
    uint8_t palette_index;
    uint8_t flags;
} TecmoStartGameMenuSprite;

typedef struct TecmoStartGameMenuOverlay {
    uint32_t cell_start;
    uint16_t cell_count;
    uint16_t width;
    uint16_t height;
    uint16_t x;
    uint16_t y;
    uint8_t type;
} TecmoStartGameMenuOverlay;

typedef struct TecmoStartGameMenuAsset {
    bool available;
    TecmoStartGameMenuCell cells[TECMO_START_GAME_MENU_CELL_COUNT];
    uint8_t palettes[TECMO_START_GAME_MENU_PALETTE_STAGE_COUNT][32];
    TecmoStartGameMenuSprite emblem[TECMO_START_GAME_MENU_EMBLEM_COUNT];
    TecmoStartGameMenuSprite cursor;
    TecmoStartGameMenuOverlay overlays[TECMO_START_GAME_MENU_OVERLAY_COUNT];
    TecmoStartGameMenuCell overlay_cells[TECMO_START_GAME_MENU_OVERLAY_CELL_COUNT];
    TecmoStartGameMenuCell digits[TECMO_START_GAME_MENU_DIGIT_COUNT];
    uint16_t stage_frames[TECMO_START_GAME_MENU_PALETTE_STAGE_COUNT];
    uint8_t routes[TECMO_START_GAME_MENU_ROOT_COUNT];
    uint8_t period_values[5];
    uint16_t stable_frame;
    uint16_t repeat_frames;
    uint16_t slide_frames;
    uint16_t background_step;
    uint16_t emblem_step;
    uint16_t cursor_x;
    uint16_t cursor_y;
    uint16_t cursor_stride;
    uint16_t emblem_anchor_x;
    uint16_t emblem_anchor_y;
    uint8_t accepted_input_seed;
    uint8_t period_setup_extra_frames;
    uint8_t exit_palette_step_frames;
    uint8_t exit_black_frame;
    uint8_t exit_handoff_frame;
    uint8_t root_input_mask;
    uint8_t generic_input_mask;
    uint8_t period_input_mask;
    uint8_t direction_mask;
    uint8_t initial_input_gate;
    uint8_t popup_row_cadence;
    uint8_t popup_setup_order;
    uint8_t popup_teardown_order;
    uint8_t cursor_commit_delay_frames;
    uint32_t expected_chr_byte_count;
    uint32_t expected_chr_fingerprint32;
    uint64_t chr_byte_count;
    uint64_t chr_fingerprint;
    char status[160];
} TecmoStartGameMenuAsset;

typedef enum TecmoStartGameMenuPhase {
    TECMO_START_GAME_MENU_REVEAL,
    TECMO_START_GAME_MENU_ROOT,
    TECMO_START_GAME_MENU_SEASON_SLIDE_IN,
    TECMO_START_GAME_MENU_SEASON,
    TECMO_START_GAME_MENU_SEASON_SLIDE_OUT,
    TECMO_START_GAME_MENU_MUSIC,
    TECMO_START_GAME_MENU_SPEED,
    TECMO_START_GAME_MENU_PERIOD,
    TECMO_START_GAME_MENU_POPUP_SETUP,
    TECMO_START_GAME_MENU_POPUP_TEARDOWN,
    TECMO_START_GAME_MENU_EXIT
} TecmoStartGameMenuPhase;

typedef enum TecmoStartGameMenuAction {
    TECMO_START_GAME_MENU_ACTION_NONE,
    TECMO_START_GAME_MENU_ACTION_PLAY_SETUP,
    TECMO_START_GAME_MENU_ACTION_ROSTERS,
    TECMO_START_GAME_MENU_ACTION_PRESEASON
} TecmoStartGameMenuAction;

typedef struct TecmoStartGameMenuState {
    TecmoStartGameMenuPhase phase;
    unsigned frame;
    uint16_t slide_frame;
    uint8_t root_selection;
    uint8_t season_selection;
    uint8_t setting_selection;
    uint8_t music_value;
    uint8_t speed_value;
    uint8_t period_index;
    uint16_t direction_cooldown;
    TecmoStartGameMenuPhase popup_phase;
    uint16_t transition_frame;
    uint8_t cursor_delay;
    TecmoStartGameMenuAction pending_action;
    bool exit_from_season;
} TecmoStartGameMenuState;

bool tecmo_start_game_menu_asset_load(TecmoStartGameMenuAsset *asset,
                                      const char *project_root);
bool tecmo_start_game_menu_asset_chr_available(const TecmoStartGameMenuAsset *asset,
                                               const uint8_t *chr_bytes,
                                               uint64_t chr_byte_count);
void tecmo_start_game_menu_state_init(TecmoStartGameMenuState *state);
TecmoStartGameMenuAction tecmo_start_game_menu_update(
    TecmoStartGameMenuState *state,
    const TecmoStartGameMenuAsset *asset,
    const TecmoControlFrame *controls);
bool tecmo_start_game_menu_draw(TecmoFramebuffer *framebuffer,
                                const TecmoStartGameMenuAsset *asset,
                                const TecmoStartGameMenuState *state,
                                const TecmoTitleAsset *title_asset,
                                const uint8_t *chr_bytes,
                                uint64_t chr_byte_count,
                                int origin_x,
                                int origin_y,
                                int scale);
unsigned tecmo_start_game_menu_palette_stage(const TecmoStartGameMenuAsset *asset,
                                             const TecmoStartGameMenuState *state);
unsigned tecmo_start_game_menu_overlay_visible_rows(const TecmoStartGameMenuAsset *asset,
                                                    const TecmoStartGameMenuState *state);
bool tecmo_start_game_menu_cursor_visible(const TecmoStartGameMenuAsset *asset,
                                          const TecmoStartGameMenuState *state);
const char *tecmo_start_game_menu_phase_name(TecmoStartGameMenuPhase phase);

#endif
