#ifndef TECMO_ALL_STAR_MENU_H
#define TECMO_ALL_STAR_MENU_H

#include "tecmo_controls.h"
#include "tecmo_framebuffer.h"
#include "tecmo_preseason_menu.h"
#include "tecmo_start_game_menu.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_ALL_STAR_TEAM_CELL_COUNT 42U
#define TECMO_ALL_STAR_CONTROL_COUNT 7U
#define TECMO_ALL_STAR_DIFFICULTY_COUNT 3U
#define TECMO_ALL_STAR_TEAM_SIDE_COUNT 2U

typedef struct TecmoAllStarAsset {
    bool available;
    TecmoStartGameMenuOverlay team_overlay;
    TecmoStartGameMenuCell team_cells[TECMO_ALL_STAR_TEAM_CELL_COUNT];
    uint8_t ownership[2][6];
    uint8_t team_mode_rows[2];
    uint8_t configs[3];
    uint16_t cursor_x[3];
    uint16_t cursor_y[3];
    uint16_t cursor_stride[3];
    uint8_t row_cadence;
    uint8_t cursor_commit_delay_frames;
    uint8_t accepted_input_seed;
    uint8_t repeat_frames;
    uint8_t west_team_code;
    uint8_t east_team_code;
    uint8_t release_input_mask;
    uint8_t direction_input_mask;
    uint32_t expected_preseason_size;
    uint32_t expected_preseason_fingerprint32;
    uint32_t expected_start_menu_size;
    uint32_t expected_start_menu_fingerprint32;
    uint32_t expected_chr_size;
    uint32_t expected_chr_fingerprint32;
    uint64_t chr_byte_count;
    uint64_t chr_fingerprint;
    char status[160];
} TecmoAllStarAsset;

typedef enum TecmoAllStarPhase {
    TECMO_ALL_STAR_CONTROL_SETUP,
    TECMO_ALL_STAR_CONTROL,
    TECMO_ALL_STAR_DIFFICULTY_SETUP,
    TECMO_ALL_STAR_DIFFICULTY,
    TECMO_ALL_STAR_DIFFICULTY_TEARDOWN,
    TECMO_ALL_STAR_TEAM_SETUP,
    TECMO_ALL_STAR_TEAM,
    TECMO_ALL_STAR_TEAM_TEARDOWN,
    TECMO_ALL_STAR_CONTROL_TEARDOWN_ROOT
} TecmoAllStarPhase;

typedef enum TecmoAllStarAction {
    TECMO_ALL_STAR_ACTION_NONE,
    TECMO_ALL_STAR_ACTION_BACK_TO_START_MENU
} TecmoAllStarAction;

typedef struct TecmoAllStarState {
    TecmoAllStarPhase phase;
    unsigned frame;
    uint16_t transition_frame;
    uint16_t direction_cooldown;
    uint8_t cursor_delay;
    uint8_t control_selection;
    uint8_t difficulty_selection;
    uint8_t committed_difficulty;
    uint8_t team_selection;
    uint8_t west_owner;
    uint8_t east_owner;
    uint8_t west_team;
    uint8_t east_team;
    bool terminal_commit;
    bool terminal_from_team;
} TecmoAllStarState;

bool tecmo_all_star_asset_load(TecmoAllStarAsset *asset,
                               const char *project_root);
bool tecmo_all_star_asset_chr_available(const TecmoAllStarAsset *asset,
                                        const uint8_t *chr_bytes,
                                        uint64_t chr_byte_count);
void tecmo_all_star_state_init(TecmoAllStarState *state);
TecmoAllStarAction tecmo_all_star_update(TecmoAllStarState *state,
                                         const TecmoAllStarAsset *asset,
                                         const TecmoControlFrame *controls);
bool tecmo_all_star_draw(TecmoFramebuffer *framebuffer,
                         const TecmoAllStarAsset *asset,
                         const TecmoAllStarState *state,
                         const TecmoPreseasonAsset *preseason_asset,
                         const TecmoStartGameMenuAsset *start_asset,
                         const uint8_t *chr_bytes,
                         uint64_t chr_byte_count,
                         int origin_x,
                         int origin_y,
                         int scale);
unsigned tecmo_all_star_overlay_visible_rows(const TecmoAllStarAsset *asset,
                                             const TecmoAllStarState *state,
                                             size_t overlay_index);
const char *tecmo_all_star_phase_name(TecmoAllStarPhase phase);

#endif
