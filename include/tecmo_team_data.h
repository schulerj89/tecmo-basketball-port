#ifndef TECMO_TEAM_DATA_H
#define TECMO_TEAM_DATA_H

#include "tecmo_controls.h"
#include "tecmo_framebuffer.h"
#include "tecmo_start_game_menu.h"
#include "tecmo_team_management.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_TEAM_DATA_SCREEN_COUNT 3U
#define TECMO_TEAM_DATA_SCREEN_CELLS 960U
#define TECMO_TEAM_DATA_SELECTOR_COUNT 29U
#define TECMO_TEAM_DATA_TEAM_COUNT 29U
#define TECMO_TEAM_DATA_REAL_TEAM_COUNT 27U
#define TECMO_TEAM_DATA_PLAYERS_PER_TEAM 12U
#define TECMO_TEAM_DATA_LOGO_CELL_LIMIT 60U
#define TECMO_TEAM_DATA_FONT_COUNT 59U
#define TECMO_TEAM_DATA_PORTRAIT_CELL_COUNT 24U
#define TECMO_TEAM_DATA_PROFILE_PALETTE_COUNT 4U

typedef struct TecmoTeamDataCursor {
    int16_t dx;
    int16_t dy;
    uint32_t top_chr_offset;
    uint32_t bottom_chr_offset;
    uint8_t selector;
    uint8_t raw_tile;
} TecmoTeamDataCursor;

typedef struct TecmoTeamDataSelector {
    uint8_t x;
    uint8_t y;
    uint8_t team_id;
} TecmoTeamDataSelector;

typedef struct TecmoTeamDataTeam {
    char city[16];
    char nickname[16];
    uint8_t conference;
    uint8_t division;
    uint8_t logo_width;
    uint8_t logo_height;
    uint8_t logo_selector;
    uint8_t logo_tile_high;
    uint8_t logo_count;
    uint8_t logo_x;
    uint8_t profile_palette_group;
} TecmoTeamDataTeam;

typedef struct TecmoTeamDataPlayer {
    char name[21];
    uint8_t attributes[7];
    uint8_t profile[6];
    uint8_t source_team;
    uint8_t source_player;
    uint8_t portrait_r0;
    uint8_t portrait_r1;
    uint8_t condition_seed;
    TecmoStartGameMenuCell portrait[TECMO_TEAM_DATA_PORTRAIT_CELL_COUNT];
} TecmoTeamDataPlayer;

typedef struct TecmoTeamDataAsset {
    bool available;
    TecmoStartGameMenuCell screens[TECMO_TEAM_DATA_SCREEN_COUNT]
                                       [TECMO_TEAM_DATA_SCREEN_CELLS];
    uint8_t palettes[TECMO_TEAM_DATA_SCREEN_COUNT][16];
    uint8_t profile_palettes[TECMO_TEAM_DATA_PROFILE_PALETTE_COUNT][16];
    uint8_t sprite_palette[16];
    TecmoTeamDataCursor cursors[2];
    TecmoTeamDataSelector selectors[TECMO_TEAM_DATA_SELECTOR_COUNT];
    TecmoStartGameMenuCell font[TECMO_TEAM_DATA_FONT_COUNT];
    TecmoTeamDataTeam teams[TECMO_TEAM_DATA_TEAM_COUNT];
    TecmoStartGameMenuCell logos[TECMO_TEAM_DATA_REAL_TEAM_COUNT]
                                     [TECMO_TEAM_DATA_LOGO_CELL_LIMIT];
    TecmoTeamDataPlayer players[TECMO_TEAM_DATA_TEAM_COUNT]
                               [TECMO_TEAM_DATA_PLAYERS_PER_TEAM];
    uint8_t selector_initial_cooldown;
    uint8_t selector_repeat_frames;
    uint8_t generic_initial_cooldown;
    uint8_t generic_repeat_frames;
    uint8_t slide_frames;
    uint8_t slide_pixels_per_frame;
    uint8_t profile_cursor_x;
    uint8_t profile_cursor_y;
    uint8_t profile_cursor_stride;
    uint8_t roster_cursor_x;
    uint8_t roster_cursor_y;
    uint8_t roster_cursor_stride;
    uint8_t logo_y;
    uint8_t selector_transition_black_frame;
    uint8_t selector_transition_render_off_frame;
    uint8_t selector_transition_render_on_frame;
    uint8_t selector_transition_first_visible_frame;
    uint8_t selector_transition_palette_step_frames;
    uint8_t selector_transition_stable_frame;
    uint8_t detail_transition_black_frame;
    uint8_t detail_transition_render_off_frame;
    uint8_t detail_transition_render_on_frame;
    uint8_t detail_transition_first_visible_frame;
    uint8_t detail_transition_palette_step_frames;
    uint8_t detail_transition_stable_frame;
    uint8_t entry_transition_render_on_frame;
    uint8_t entry_transition_first_visible_frame;
    uint8_t entry_transition_palette_step_frames;
    uint8_t entry_transition_stable_frame;
    uint32_t expected_chr_size;
    uint32_t expected_chr_fingerprint32;
    uint64_t chr_fingerprint64;
    char status[160];
} TecmoTeamDataAsset;

typedef enum TecmoTeamDataPhase {
    TECMO_TEAM_DATA_TEAM_SELECT,
    TECMO_TEAM_DATA_PROFILE,
    TECMO_TEAM_DATA_ROSTER,
    TECMO_TEAM_DATA_PLAYER_DETAIL,
    TECMO_TEAM_DATA_STARTERS,
    TECMO_TEAM_DATA_PLAYBOOK
} TecmoTeamDataPhase;

typedef enum TecmoTeamDataAction {
    TECMO_TEAM_DATA_ACTION_NONE,
    TECMO_TEAM_DATA_ACTION_BACK_TO_START_MENU
} TecmoTeamDataAction;

typedef enum TecmoTeamDataTransition {
    TECMO_TEAM_DATA_TRANSITION_NONE,
    TECMO_TEAM_DATA_TRANSITION_ENTRY_TO_SELECTOR,
    TECMO_TEAM_DATA_TRANSITION_SELECTOR_TO_PROFILE,
    TECMO_TEAM_DATA_TRANSITION_PROFILE_TO_SELECTOR,
    TECMO_TEAM_DATA_TRANSITION_ROSTER_TO_DETAIL,
    TECMO_TEAM_DATA_TRANSITION_DETAIL_TO_ROSTER
} TecmoTeamDataTransition;

typedef struct TecmoTeamDataState {
    TecmoTeamDataPhase phase;
    unsigned frame;
    uint16_t direction_cooldown;
    uint8_t selector_index;
    uint8_t team_id;
    uint8_t profile_selection;
    uint8_t roster_page;
    uint8_t roster_row;
    uint8_t player_index;
    uint8_t slide_frame;
    uint8_t slide_from_page;
    uint8_t slide_to_page;
    int8_t slide_direction;
    uint8_t cursor_delay;
    TecmoTeamDataTransition transition;
    uint8_t transition_frame;
    bool detail_return_to_starters;
    uint8_t detail_return_selection;
    TecmoTeamManagementViewState management_view;
} TecmoTeamDataState;

bool tecmo_team_data_asset_load(TecmoTeamDataAsset *asset,
                                const char *project_root);
bool tecmo_team_data_asset_chr_available(const TecmoTeamDataAsset *asset,
                                         const uint8_t *chr_bytes,
                                         uint64_t chr_byte_count);
void tecmo_team_data_state_init(TecmoTeamDataState *state);
TecmoTeamDataAction tecmo_team_data_update(
    TecmoTeamDataState *state,
    const TecmoTeamDataAsset *asset,
    const TecmoTeamManagementAsset *management_asset,
    TecmoTeamManagementSession *management_session,
    const TecmoControlFrame *controls);
bool tecmo_team_data_draw(TecmoFramebuffer *framebuffer,
                          const TecmoTeamDataAsset *asset,
                          const TecmoTeamDataState *state,
                          const TecmoTeamManagementAsset *management_asset,
                          const TecmoTeamManagementSession *management_session,
                          const uint8_t *chr_bytes,
                          uint64_t chr_byte_count,
                          int origin_x,
                          int origin_y,
                          int scale);
const char *tecmo_team_data_phase_name(TecmoTeamDataPhase phase);
const char *tecmo_team_data_position_name(uint8_t roster_code);
const char *tecmo_team_data_condition_name(uint8_t condition_value);
uint8_t tecmo_team_data_meter_fill_length(const uint8_t profile[6],
                                          size_t meter_index);
unsigned tecmo_team_data_transition_palette_stage(
    const TecmoTeamDataAsset *asset,
    const TecmoTeamDataState *state);
bool tecmo_team_data_transition_render_enabled(
    const TecmoTeamDataAsset *asset,
    const TecmoTeamDataState *state);

#endif
