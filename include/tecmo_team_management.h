#ifndef TECMO_TEAM_MANAGEMENT_H
#define TECMO_TEAM_MANAGEMENT_H

#include "tecmo_controls.h"
#include "tecmo_framebuffer.h"
#include "tecmo_start_game_menu.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_TEAM_MANAGEMENT_TEAM_COUNT 29U
#define TECMO_TEAM_MANAGEMENT_PLAYER_COUNT 12U
#define TECMO_TEAM_MANAGEMENT_STARTER_COUNT 5U
#define TECMO_TEAM_MANAGEMENT_BENCH_COUNT 7U
#define TECMO_TEAM_MANAGEMENT_PLAYBOOK_SLOT_COUNT 4U
#define TECMO_TEAM_MANAGEMENT_PLAY_COUNT 8U
#define TECMO_TEAM_MANAGEMENT_SCREEN_CELLS 960U
#define TECMO_TEAM_MANAGEMENT_DIAGRAM_CELLS 64U

struct TecmoTeamDataAsset;

typedef struct TecmoTeamManagementAsset {
    bool available;
    TecmoStartGameMenuCell starters_screen[TECMO_TEAM_MANAGEMENT_SCREEN_CELLS];
    TecmoStartGameMenuCell playbook_screens[2][TECMO_TEAM_MANAGEMENT_SCREEN_CELLS];
    TecmoStartGameMenuCell diagrams[TECMO_TEAM_MANAGEMENT_PLAY_COUNT]
                                       [TECMO_TEAM_MANAGEMENT_DIAGRAM_CELLS];
    uint8_t palettes[2][16];
    uint8_t marker[16];
    char play_names[TECMO_TEAM_MANAGEMENT_PLAY_COUNT][18];
    uint8_t default_starters[TECMO_TEAM_MANAGEMENT_TEAM_COUNT]
                            [TECMO_TEAM_MANAGEMENT_STARTER_COUNT];
    uint8_t default_playbooks[TECMO_TEAM_MANAGEMENT_TEAM_COUNT]
                             [TECMO_TEAM_MANAGEMENT_PLAYBOOK_SLOT_COUNT];
    uint8_t starter_selector_count;
    uint8_t bench_selector_count;
    uint8_t held_repeat_frames;
    uint8_t carousel_frames;
    uint8_t carousel_pixels_per_frame;
    uint8_t starters_cursor_x;
    uint8_t starters_cursor_y;
    uint8_t starters_cursor_stride;
    uint8_t playbook_cursor_y;
    uint32_t expected_chr_size;
    uint32_t expected_chr_fingerprint32;
    uint64_t chr_fingerprint64;
    char status[160];
} TecmoTeamManagementAsset;

typedef struct TecmoTeamManagementSession {
    bool initialized;
    uint8_t starters[TECMO_TEAM_MANAGEMENT_TEAM_COUNT]
                    [TECMO_TEAM_MANAGEMENT_STARTER_COUNT];
    uint8_t playbooks[TECMO_TEAM_MANAGEMENT_TEAM_COUNT]
                     [TECMO_TEAM_MANAGEMENT_PLAYBOOK_SLOT_COUNT];
} TecmoTeamManagementSession;

typedef enum TecmoTeamManagementView {
    TECMO_TEAM_MANAGEMENT_VIEW_NONE,
    TECMO_TEAM_MANAGEMENT_VIEW_STARTERS,
    TECMO_TEAM_MANAGEMENT_VIEW_STARTER_RESET,
    TECMO_TEAM_MANAGEMENT_VIEW_STARTER_BENCH,
    TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK,
    TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK_REPLACE,
    TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK_RESET
} TecmoTeamManagementView;

typedef struct TecmoTeamManagementViewState {
    TecmoTeamManagementView view;
    uint8_t selection;
    uint8_t secondary_selection;
    uint8_t carousel_origin;
    uint8_t carousel_pending_origin;
    uint8_t carousel_frame;
    int8_t carousel_direction;
    uint8_t direction_cooldown;
} TecmoTeamManagementViewState;

typedef enum TecmoTeamManagementAction {
    TECMO_TEAM_MANAGEMENT_ACTION_NONE,
    TECMO_TEAM_MANAGEMENT_ACTION_BACK_TO_PROFILE,
    TECMO_TEAM_MANAGEMENT_ACTION_PLAYER_DETAIL
} TecmoTeamManagementAction;

bool tecmo_team_management_asset_load(TecmoTeamManagementAsset *asset,
                                      const char *project_root);
bool tecmo_team_management_asset_chr_available(
    const TecmoTeamManagementAsset *asset,
    const uint8_t *chr_bytes,
    uint64_t chr_byte_count);
bool tecmo_team_management_session_init(
    TecmoTeamManagementSession *session,
    const TecmoTeamManagementAsset *asset);
bool tecmo_team_management_session_valid(
    const TecmoTeamManagementSession *session);
void tecmo_team_management_view_init_starters(
    TecmoTeamManagementViewState *state);
void tecmo_team_management_view_init_playbook(
    TecmoTeamManagementViewState *state);
TecmoTeamManagementAction tecmo_team_management_update(
    TecmoTeamManagementViewState *state,
    TecmoTeamManagementSession *session,
    const TecmoTeamManagementAsset *asset,
    const struct TecmoTeamDataAsset *team_data,
    uint8_t team_id,
    const TecmoControlFrame *controls,
    uint8_t *player_detail_index);
bool tecmo_team_management_draw(
    TecmoFramebuffer *framebuffer,
    const TecmoTeamManagementAsset *asset,
    const struct TecmoTeamDataAsset *team_data,
    const TecmoTeamManagementSession *session,
    const TecmoTeamManagementViewState *state,
    uint8_t team_id,
    const uint8_t *chr_bytes,
    uint64_t chr_byte_count,
    int origin_x,
    int origin_y,
    int scale);
const char *tecmo_team_management_view_name(TecmoTeamManagementView view);
bool tecmo_team_management_self_test(const char *project_root,
                                     char *message,
                                     size_t message_size);

#endif
