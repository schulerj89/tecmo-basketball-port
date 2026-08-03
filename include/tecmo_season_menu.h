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
#define TECMO_SEASON_LEADER_SCREEN_COUNT 7U
#define TECMO_SEASON_LEADER_SCREEN_CELLS 960U
#define TECMO_SEASON_POPUP_CELL_COUNT 496U
#define TECMO_SEASON_NO_RAW_INDEX 0xFFFFU
#define TECMO_SEASON_NO_TEAM 0xFFU
#define TECMO_SEASON_STANDINGS_MAX_DIVISION_TEAMS 7U

typedef struct TecmoSeasonPopupOverlay {
    uint16_t cell_start;
    uint16_t cell_count;
    uint16_t width;
    uint16_t height;
    uint16_t origin_x;
    uint16_t origin_y;
} TecmoSeasonPopupOverlay;

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
    uint8_t division_starts[4];
    uint8_t division_teams[TECMO_SEASON_TEAM_COUNT];
    uint8_t leader_up[TECMO_SEASON_LEADER_COUNT];
    uint8_t leader_down[TECMO_SEASON_LEADER_COUNT];
    uint8_t leader_cursor_x[TECMO_SEASON_LEADER_COUNT];
    uint8_t leader_cursor_y[TECMO_SEASON_LEADER_COUNT];
    uint8_t leader_template[TECMO_SEASON_LEADER_COUNT];
    TecmoStartGameMenuCell leader_screens[TECMO_SEASON_LEADER_SCREEN_COUNT]
                                               [TECMO_SEASON_LEADER_SCREEN_CELLS];
    uint8_t leader_palettes[TECMO_SEASON_LEADER_SCREEN_COUNT][16];
    char schedule_text[3][24];
    char reset_text[6][32];
    char season_type_text[5][16];
    char overtime_text[3];
    TecmoStartGameMenuCell zero_games_behind_cells[2];
    TecmoStartGameMenuCell half_game_cell;
    uint8_t menu_boxes[3][4];
    TecmoSeasonPopupOverlay popup_overlays[3];
    TecmoStartGameMenuCell popup_cells[TECMO_SEASON_POPUP_CELL_COUNT];
    uint8_t popup_cursor_x[3][4];
    uint8_t popup_cursor_y[3][4];
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
    /* Native TSAV/runtime validation caps wins+losses per team at the
     * proven mode targets: regular/reduced/short/programmed = 82/42/26/82. */
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

/* A logical season ordinal is distinct from the source schedule's raw index. */
typedef struct TecmoSeasonScheduleRecord {
    uint16_t logical_ordinal;
    uint16_t raw_index;
    uint8_t away_team;
    uint8_t home_team;
} TecmoSeasonScheduleRecord;

typedef struct TecmoSeasonProgress {
    uint8_t season_type;
    uint16_t logical_ordinal;
    uint16_t total_games;
    uint16_t completed_games;
    uint16_t remaining_games;
    uint16_t next_raw_index;
    uint8_t next_away_team;
    uint8_t next_home_team;
    bool complete;
} TecmoSeasonProgress;

/* Native policy: exact winning ratio by cross multiplication (no display
 * truncation), then wins, then the source division/display order.  Games
 * behind are exported as unsigned half-game units and floor at zero when an
 * arbitrary programmed record would otherwise produce a negative value.  The
 * original standings tie policy is not claimed. */
typedef struct TecmoSeasonStandingsRow {
    uint8_t team_id;
    uint8_t division;
    uint8_t source_order;
    uint16_t wins;
    uint16_t losses;
    uint16_t games_played;
    uint32_t winning_ratio_numerator;
    uint32_t winning_ratio_denominator;
    uint16_t games_behind_half;
} TecmoSeasonStandingsRow;

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
    TECMO_SEASON_ACTION_BACK_TO_START_MENU,
    TECMO_SEASON_ACTION_LAUNCH_GAME
} TecmoSeasonAction;

typedef struct TecmoSeasonGameResult {
    uint16_t game_index;
    uint16_t away_score;
    uint16_t home_score;
    uint8_t away_team;
    uint8_t home_team;
    bool overtime;
} TecmoSeasonGameResult;

typedef struct TecmoSeasonPendingGame {
    uint16_t game_index;
    uint8_t away_team;
    uint8_t home_team;
} TecmoSeasonPendingGame;

typedef struct TecmoSeasonState {
    TecmoSeasonPhase phase;
    unsigned frame;
    uint16_t direction_cooldown;
    uint16_t schedule_selection;
    uint16_t playoff_scroll;
    uint16_t standings_slide;
    uint8_t team_selection;
    uint8_t popup_selection;
    uint8_t popup_rows_visible;
    uint8_t popup_animation_tick;
    uint8_t popup_open_sync_frames;
    uint8_t season_type_selection;
    uint8_t standings_page;
    uint8_t editor_panel;
    uint8_t editor_target_panel;
    uint8_t editor_team;
    uint8_t leader_category;
    uint8_t leader_page;
    uint8_t standings_target_page;
    int8_t standings_slide_direction;
    uint8_t game_result_count;
    uint8_t game_result_visible_rows;
    TecmoSeasonGameResult game_results[4];
    TecmoSeasonPendingGame pending_game;
    bool programmed_return_to_schedule;
    bool popup_closing;
    bool leaders_results;
    bool game_prepare_pending;
    bool game_result_pending;
    bool season_complete;
    bool game_launch_blocked;
    TecmoSeasonPhase popup_target_phase;
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
bool tecmo_season_schedule_raw_index(const TecmoSeasonAsset *asset,
                                     uint8_t season_type,
                                     uint16_t ordinal,
                                     uint16_t *raw_index);
bool tecmo_season_game_count(const TecmoSeasonAsset *asset,
                             uint8_t season_type,
                             uint16_t *game_count);
bool tecmo_season_schedule_record(const TecmoSeasonAsset *asset,
                                  uint8_t season_type,
                                  uint16_t ordinal,
                                  TecmoSeasonScheduleRecord *record);
bool tecmo_season_next_progress(const TecmoSeasonAsset *asset,
                                const TecmoSeasonSession *session,
                                TecmoSeasonProgress *progress);
bool tecmo_season_build_standings_rows(
    const TecmoSeasonAsset *asset,
    const TecmoSeasonSession *session,
    uint8_t division,
    TecmoSeasonStandingsRow *rows,
    size_t row_capacity,
    size_t *row_count);
const char *tecmo_season_standings_policy(void);
bool tecmo_season_commit_game_result(TecmoSeasonState *state,
                                     const TecmoSeasonAsset *asset,
                                     TecmoSeasonSession *session,
                                     const TecmoSeasonGameResult *result);
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
