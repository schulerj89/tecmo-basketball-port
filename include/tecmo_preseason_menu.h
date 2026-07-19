#ifndef TECMO_PRESEASON_MENU_H
#define TECMO_PRESEASON_MENU_H

#include "tecmo_controls.h"
#include "tecmo_framebuffer.h"
#include "tecmo_start_game_menu.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_PRESEASON_OVERLAY_COUNT 3U
#define TECMO_PRESEASON_OVERLAY_CELL_COUNT 510U
#define TECMO_PRESEASON_TEAM_SCREEN_COUNT 4U
#define TECMO_PRESEASON_TEAM_CELL_COUNT 3840U
#define TECMO_PRESEASON_TEAM_COUNT 27U
#define TECMO_PRESEASON_MARKER_PIECES 5U

typedef struct TecmoPreseasonAsset {
    bool available;
    TecmoStartGameMenuOverlay overlays[TECMO_PRESEASON_OVERLAY_COUNT];
    TecmoStartGameMenuCell overlay_cells[TECMO_PRESEASON_OVERLAY_CELL_COUNT];
    TecmoStartGameMenuCell team_cells[TECMO_PRESEASON_TEAM_CELL_COUNT];
    uint8_t team_palettes[TECMO_PRESEASON_TEAM_SCREEN_COUNT][16];
    TecmoStartGameMenuSprite markers[2][TECMO_PRESEASON_MARKER_PIECES];
    uint8_t team_x[TECMO_PRESEASON_TEAM_COUNT];
    uint8_t team_y[TECMO_PRESEASON_TEAM_COUNT];
    uint8_t division_offsets[4];
    uint8_t division_counts[4];
    uint8_t team_ids[TECMO_PRESEASON_TEAM_COUNT];
    uint8_t ownership[2][6];
    uint8_t configs[7];
    uint8_t division_cursor_y[4];
    uint16_t control_cursor_x;
    uint16_t control_cursor_y;
    uint16_t control_cursor_stride;
    uint16_t division_cursor_x;
    uint16_t difficulty_cursor_x;
    uint16_t difficulty_cursor_y;
    uint16_t difficulty_cursor_stride;
    uint16_t team_input_ready_frames;
    uint16_t team_exit_frames;
    uint8_t team_palette_full_frames;
    uint8_t team_first_visible_frame;
    uint8_t team_palette_step_frames;
    uint8_t team_visible_full_frame;
    uint8_t division_return_first_visible_frame;
    uint8_t division_return_palette_step_frames;
    uint8_t division_return_full_frame;
    uint8_t team_entry_stage7_frame;
    uint8_t team_entry_stage6_frame;
    uint8_t team_entry_stage5_frame;
    uint8_t team_entry_black_frame;
    uint8_t team_exit_cap2_frame;
    uint8_t team_exit_cap1_frame;
    uint8_t team_exit_cap0_frame;
    uint8_t team_exit_black_frame;
    uint8_t row_cadence;
    uint8_t cursor_commit_delay_frames;
    uint8_t accepted_input_seed;
    uint8_t repeat_frames;
    uint32_t expected_start_menu_size;
    uint32_t expected_start_menu_fingerprint32;
    uint32_t expected_chr_size;
    uint32_t expected_chr_fingerprint32;
    uint64_t chr_byte_count;
    uint64_t chr_fingerprint;
    char status[160];
} TecmoPreseasonAsset;

typedef enum TecmoPreseasonPhase {
    TECMO_PRESEASON_CONTROL_SETUP,
    TECMO_PRESEASON_CONTROL,
    TECMO_PRESEASON_DIFFICULTY_SETUP,
    TECMO_PRESEASON_DIFFICULTY,
    TECMO_PRESEASON_DIFFICULTY_TEARDOWN,
    TECMO_PRESEASON_CONTROL_TEARDOWN_ROOT,
    TECMO_PRESEASON_DIVISION_SETUP,
    TECMO_PRESEASON_DIVISION,
    TECMO_PRESEASON_DIVISION_TEARDOWN_CONTROL,
    TECMO_PRESEASON_TEAM_ENTRY,
    TECMO_PRESEASON_TEAM,
    TECMO_PRESEASON_TEAM_EXIT
} TecmoPreseasonPhase;

typedef enum TecmoPreseasonAction {
    TECMO_PRESEASON_ACTION_NONE,
    TECMO_PRESEASON_ACTION_BACK_TO_START_MENU,
    TECMO_PRESEASON_ACTION_LAUNCH_GAME
} TecmoPreseasonAction;

typedef enum TecmoPreseasonTeamExitTarget {
    TECMO_PRESEASON_TEAM_EXIT_DIVISION,
    TECMO_PRESEASON_TEAM_EXIT_P2_DIVISION
} TecmoPreseasonTeamExitTarget;

typedef struct TecmoPreseasonState {
    TecmoPreseasonPhase phase;
    TecmoPreseasonTeamExitTarget team_exit_target;
    unsigned frame;
    uint16_t transition_frame;
    uint16_t direction_cooldown;
    uint8_t cursor_delay;
    uint8_t control_selection;
    uint8_t difficulty_selection;
    uint8_t committed_difficulty;
    uint8_t division_selection[2];
    uint8_t team_selection[2];
    uint8_t active_player;
    uint8_t team_palette_frame;
    uint8_t division_return_fade_frame;
    bool division_return_fade_active;
} TecmoPreseasonState;

bool tecmo_preseason_asset_load(TecmoPreseasonAsset *asset,
                                const char *project_root);
bool tecmo_preseason_asset_chr_available(const TecmoPreseasonAsset *asset,
                                         const uint8_t *chr_bytes,
                                         uint64_t chr_byte_count);
void tecmo_preseason_state_init(TecmoPreseasonState *state);
TecmoPreseasonAction tecmo_preseason_update(TecmoPreseasonState *state,
                                            const TecmoPreseasonAsset *asset,
                                            const TecmoControlFrame *player_one,
                                            const TecmoControlFrame *player_two);
bool tecmo_preseason_draw(TecmoFramebuffer *framebuffer,
                          const TecmoPreseasonAsset *asset,
                          const TecmoPreseasonState *state,
                          const TecmoStartGameMenuAsset *start_asset,
                          const uint8_t *chr_bytes,
                          uint64_t chr_byte_count,
                          int origin_x,
                          int origin_y,
                          int scale);
unsigned tecmo_preseason_overlay_visible_rows(const TecmoPreseasonAsset *asset,
                                              const TecmoPreseasonState *state,
                                              size_t overlay_index);
const char *tecmo_preseason_phase_name(TecmoPreseasonPhase phase);

#endif
