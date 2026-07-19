#ifndef TECMO_SEASON_MENU_H
#define TECMO_SEASON_MENU_H

#include "tecmo_controls.h"
#include "tecmo_framebuffer.h"
#include "tecmo_start_game_menu.h"
#include "tecmo_team_data.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_SEASON_SCREEN_COUNT 5U
#define TECMO_SEASON_SCREEN_CELLS 1920U
#define TECMO_SEASON_TEAM_COUNT 27U
#define TECMO_SEASON_SCHEDULE_COUNT 1107U
#define TECMO_SEASON_FONT_COUNT 59U
#define TECMO_SEASON_LEADER_COUNT 7U

typedef enum TecmoSeasonRoute {
    TECMO_SEASON_ROUTE_TEAM_CONTROL = 0,
    TECMO_SEASON_ROUTE_SCHEDULE = 1,
    TECMO_SEASON_ROUTE_GAME_START = 2,
    TECMO_SEASON_ROUTE_STANDINGS = 3,
    TECMO_SEASON_ROUTE_LEADERS = 4
} TecmoSeasonRoute;

typedef enum TecmoSeasonType {
    TECMO_SEASON_REGULAR = 0,
    TECMO_SEASON_REDUCED = 1,
    TECMO_SEASON_SHORT = 2,
    TECMO_SEASON_PROGRAMMED = 3
} TecmoSeasonType;

typedef struct TecmoSeasonCursor {
    int16_t dx;
    int16_t dy;
    uint32_t top_chr_offset;
    uint32_t bottom_chr_offset;
    uint8_t raw_selector;
    uint8_t raw_tile;
} TecmoSeasonCursor;

typedef struct TecmoSeasonAsset {
    bool available;
    TecmoStartGameMenuCell screens[TECMO_SEASON_SCREEN_COUNT]
                                       [TECMO_SEASON_SCREEN_CELLS];
    uint8_t palettes[TECMO_SEASON_SCREEN_COUNT][16];
    TecmoStartGameMenuCell font[TECMO_SEASON_FONT_COUNT];
    TecmoSeasonCursor cursor;
    uint8_t sprite_palette[16];
    uint8_t schedule[TECMO_SEASON_SCHEDULE_COUNT][2];
    TecmoStartGameMenuCell control_cells[4][3];
    char leader_labels[TECMO_SEASON_LEADER_COUNT][20];
    uint16_t game_counts[4];
    uint8_t team_control_x[3];
    uint8_t team_control_y;
    uint8_t team_control_stride;
    uint16_t game_launch_boundary_cpu;
    uint16_t game_launch_target_cpu;
    uint32_t expected_chr_size;
    uint32_t expected_chr_fingerprint32;
    uint64_t chr_fingerprint64;
    char status[160];
} TecmoSeasonAsset;

typedef enum TecmoSeasonSaveStatus {
    TECMO_SEASON_SAVE_ROM_DEFAULTS,
    TECMO_SEASON_SAVE_LOADED,
    TECMO_SEASON_SAVE_SAVED,
    TECMO_SEASON_SAVE_REJECTED,
    TECMO_SEASON_SAVE_IO_ERROR
} TecmoSeasonSaveStatus;

typedef struct TecmoSeasonSession {
    uint8_t season_type;
    uint8_t team_control[TECMO_SEASON_TEAM_COUNT];
    uint8_t wins[TECMO_SEASON_TEAM_COUNT];
    uint8_t losses[TECMO_SEASON_TEAM_COUNT];
    uint16_t schedule_index;
    bool dirty;
    TecmoSeasonSaveStatus save_status;
    char save_path[1024];
    char status[160];
} TecmoSeasonSession;

typedef enum TecmoSeasonPhase {
    TECMO_SEASON_TEAM_CONTROL,
    TECMO_SEASON_SCHEDULE,
    TECMO_SEASON_SCHEDULE_POPUP,
    TECMO_SEASON_PLAYOFF,
    TECMO_SEASON_RESET_CONFIRM,
    TECMO_SEASON_TYPE_SELECT,
    TECMO_SEASON_STANDINGS,
    TECMO_SEASON_PROGRAMMED_EDITOR,
    TECMO_SEASON_LEADERS,
    TECMO_SEASON_GAME_START
} TecmoSeasonPhase;

typedef enum TecmoSeasonAction {
    TECMO_SEASON_ACTION_NONE,
    TECMO_SEASON_ACTION_BACK_TO_START_MENU
} TecmoSeasonAction;

typedef struct TecmoSeasonState {
    TecmoSeasonPhase phase;
    unsigned frame;
    uint16_t direction_cooldown;
    uint16_t schedule_selection;
    uint8_t team_selection;
    uint8_t popup_selection;
    uint8_t season_type_selection;
    uint8_t standings_page;
    uint8_t editor_panel;
    uint8_t editor_team;
    uint8_t leader_category;
    bool game_launch_blocked;
} TecmoSeasonState;

bool tecmo_season_asset_load(TecmoSeasonAsset *asset,
                             const char *project_root);
bool tecmo_season_asset_chr_available(const TecmoSeasonAsset *asset,
                                      const uint8_t *chr_bytes,
                                      uint64_t chr_byte_count);
void tecmo_season_session_init(TecmoSeasonSession *session,
                               const char *project_root);
bool tecmo_season_session_save(TecmoSeasonSession *session);
void tecmo_season_state_init(TecmoSeasonState *state,
                             TecmoSeasonRoute route,
                             const TecmoSeasonSession *session);
TecmoSeasonAction tecmo_season_update(TecmoSeasonState *state,
                                      const TecmoSeasonAsset *asset,
                                      TecmoSeasonSession *session,
                                      const TecmoControlFrame *controls);
bool tecmo_season_draw(TecmoFramebuffer *framebuffer,
                       const TecmoSeasonAsset *asset,
                       const TecmoSeasonSession *session,
                       const TecmoSeasonState *state,
                       const TecmoTeamDataAsset *team_data,
                       const uint8_t *chr_bytes,
                       uint64_t chr_byte_count,
                       int origin_x,
                       int origin_y,
                       int scale);
const char *tecmo_season_phase_name(TecmoSeasonPhase phase);
const char *tecmo_season_type_name(uint8_t season_type);
const char *tecmo_season_control_name(uint8_t control);
bool tecmo_season_self_test(char *message, size_t message_size);

#endif
