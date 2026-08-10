#ifndef TECMO_TEAM_DATA_H
#define TECMO_TEAM_DATA_H

#include "tecmo_controls.h"
#include "tecmo_framebuffer.h"
#include "tecmo_player_stats.h"
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
#define TECMO_TEAM_DATA_LAKERS_TEAM_ID 12U
#define TECMO_TEAM_DATA_PLAYER_KEY_COUNT \
    (TECMO_TEAM_DATA_REAL_TEAM_COUNT * TECMO_TEAM_DATA_PLAYERS_PER_TEAM)
#define TECMO_TEAM_DATA_PLAYER_KEY_INVALID 0xFFFFU
#define TECMO_TEAM_DATA_LEADER_RESULT_COUNT 18U
#define TECMO_TEAM_DATA_STAT_CATEGORY_COUNT 7U

/* The category names are the seven source-backed Bank00 leader labels.  The
 * values are navigation/catalog identity only; they do not imply that TSAV-1
 * or TTDT-1 stores mutable season accumulators. */
typedef enum TecmoTeamDataStatCategory {
    TECMO_TEAM_DATA_STAT_FIELD_GOALS = 0,
    TECMO_TEAM_DATA_STAT_BLOCKED_SHOTS,
    TECMO_TEAM_DATA_STAT_REBOUNDS,
    TECMO_TEAM_DATA_STAT_TOTAL_POINTS,
    TECMO_TEAM_DATA_STAT_STEALS,
    TECMO_TEAM_DATA_STAT_THREE_POINT_SHOTS,
    TECMO_TEAM_DATA_STAT_FREE_THROWS
} TecmoTeamDataStatCategory;

/* A canonical key is team-major and roster-slot-minor: [0, 323] maps exactly
 * to the 27 real teams x 12 roster slots.  Selector identity is retained in
 * TecmoTeamDataPlayerIdentity for callers that need the 29-entry UI model. */
typedef struct TecmoTeamDataPlayerIdentity {
    uint8_t selector_index;
    uint8_t selector_team;
    uint8_t roster_slot;
    uint8_t canonical_team;
    uint8_t canonical_player;
    uint16_t canonical_key;
} TecmoTeamDataPlayerIdentity;

typedef struct TecmoTeamDataMetricPair {
    uint64_t primary;
    uint64_t secondary;
} TecmoTeamDataMetricPair;

/* This is deliberately caller-owned and ephemeral.  available/eligible are
 * explicit so an unavailable metric is never confused with a zero statistic. */
typedef struct TecmoTeamDataStatCandidate {
    uint16_t canonical_key;
    bool available;
    bool eligible;
    TecmoTeamDataMetricPair metric;
} TecmoTeamDataStatCandidate;

typedef struct TecmoTeamDataLeaderEntry {
    TecmoTeamDataPlayerIdentity identity;
    TecmoTeamDataMetricPair metric;
} TecmoTeamDataLeaderEntry;

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
    uint8_t home_uniform_color;
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

/* A non-owning view of the native season accumulator.  TEAM DATA owns no
 * mutable statistics and must never infer them from the ROM-backed TTDT
 * player records.  The renderer only reads counters whose coverage bit is
 * present; callers may pass NULL for a truthful fresh-season zero row. */
typedef struct TecmoTeamDataPlayerStatsSource {
    const TecmoPlayerStatsSeasonTotals *totals;
    const uint8_t *wins;
    const uint8_t *losses;
    uint16_t coverage;
} TecmoTeamDataPlayerStatsSource;

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
bool tecmo_team_data_asset_load_from_pack(TecmoTeamDataAsset *asset,
                                          const char *asset_pack_path);
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
                          const TecmoTeamDataPlayerStatsSource *player_stats,
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

uint16_t tecmo_team_data_player_key(uint8_t canonical_team,
                                    uint8_t canonical_player);
bool tecmo_team_data_player_key_valid(uint16_t canonical_key);
bool tecmo_team_data_identity_contract_valid(
    const TecmoTeamDataAsset *asset);
bool tecmo_team_data_resolve_player_identity(
    const TecmoTeamDataAsset *asset,
    uint8_t selector_index,
    uint8_t roster_slot,
    TecmoTeamDataPlayerIdentity *identity);
const char *tecmo_team_data_stat_category_name(
    TecmoTeamDataStatCategory category);
bool tecmo_team_data_rank_leaders(
    const TecmoTeamDataAsset *asset,
    TecmoTeamDataStatCategory category,
    const TecmoTeamDataStatCandidate *candidates,
    size_t candidate_count,
    TecmoTeamDataLeaderEntry *results,
    size_t result_capacity,
    size_t *result_count);
bool tecmo_team_data_self_test(char *message, size_t message_size);

/* Resolves fixed $DEAB-$DEDF's exact away/home gameplay uniform colors.
   Invalid input leaves uniform_colors unchanged. */
bool tecmo_team_data_resolve_gameplay_uniform_colors(
    const TecmoTeamDataAsset *asset,
    uint8_t away_team,
    uint8_t home_team,
    uint8_t uniform_colors[2]);
unsigned tecmo_team_data_transition_palette_stage(
    const TecmoTeamDataAsset *asset,
    const TecmoTeamDataState *state);
bool tecmo_team_data_transition_render_enabled(
    const TecmoTeamDataAsset *asset,
    const TecmoTeamDataState *state);

#endif
