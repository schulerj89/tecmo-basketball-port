#include "tecmo_game.h"
#include "tecmo_nes_video.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COURT_LEFT 48
#define COURT_TOP 54
#define COURT_RIGHT 592
#define COURT_BOTTOM 424
#define TITLE_CHR_BANK_BYTES 8192U
#define TECMO_INTRO_OUTPUT_STEP_COUNT 16U
#define TECMO_INTRO_OUTPUT_TITLE_STEP 6U
#define TECMO_INTRO_OUTPUT_LICENSE_STEP 7U
#define TECMO_INTRO_OUTPUT_ARENA_STEP 8U
#define TECMO_INTRO_OUTPUT_READY_STEP 9U
#define TECMO_INTRO_OUTPUT_WARRIORS_STEP 10U
#define TECMO_INTRO_OUTPUT_CLIPPERS_STEP 11U
#define TECMO_INTRO_OUTPUT_BUCKS_STEP 12U
#define TECMO_INTRO_OUTPUT_PASS_STEP 13U
#define TECMO_INTRO_OUTPUT_FINALE_STEP 14U
#define TECMO_INTRO_OUTPUT_ATTRACT_STEP 15U
#define TECMO_INTRO_TITLE_AUTO_FRAME 133U
#define TECMO_INTRO_LICENSE_AUTO_FRAME 277U
#define TECMO_INTRO_ARENA_TO_READY_FRAME 540U
#define TECMO_INTRO_READY_TO_WARRIORS_FRAME 58U
#define TECMO_INTRO_WARRIORS_TO_NEXT_FRAME 214U
#define MAIN_MENU_PLAY_GAME 0U
#define MAIN_MENU_QUIT 1U
#define MAIN_MENU_COUNT 2U
#define INTRO_PRESENTS_SCREEN_ID 0x00U
#define INTRO_PRESENTS_RECORD_CPU 0xDC85U
#define INTRO_PRESENTS_STREAM_BANK 0x00U
#define INTRO_PRESENTS_STREAM_CPU 0x84FBU
#define INTRO_PRESENTS_DECODER_CPU 0xD9F6U
#define INTRO_PRESENTS_SRC_CPU 0x8549U
#define INTRO_PRESENTS_PPU 0x21CFU
#define INTRO_PRESENTS_LEN 8U
#define INTRO_PRESENTS_TILE_ROW 14U
#define INTRO_PRESENTS_TILE_COL 15U
#define INTRO_PRESENTS_PIXEL_X ((int)INTRO_PRESENTS_TILE_COL * 8)
#define INTRO_PRESENTS_PIXEL_Y ((int)INTRO_PRESENTS_TILE_ROW * 8)
#define INTRO_RABBIT_SCREEN_ID 0x18U
#define INTRO_RABBIT_BASE_X 184
#define INTRO_RABBIT_BASE_Y 30
#define INTRO_RABBIT_Y_PAGE 1
#define INTRO_RABBIT_BOUNDS_MIN_X 165
#define INTRO_RABBIT_BOUNDS_MAX_X 205
#define INTRO_RABBIT_BOUNDS_MIN_Y -34
#define INTRO_RABBIT_BOUNDS_MAX_Y 30
#define INTRO_RABBIT_FIRST_VISIBLE_X 181
#define INTRO_RABBIT_FIRST_VISIBLE_Y 14
#define INTRO_TECMO_SCREEN_ID 0x1AU
#define INTRO_TECMO_INITIAL_BASE_X 98
#define INTRO_TECMO_BASE_Y 92
#define INTRO_TECMO_LOGO_BOUNDS_MIN_X 66
#define INTRO_TECMO_LOGO_BOUNDS_MAX_X 122
#define INTRO_TECMO_LOGO_BOUNDS_MIN_Y 121
#define INTRO_TECMO_LOGO_BOUNDS_MAX_Y 169
#define INTRO_TECMO_STREAM_BOUNDS_MIN_X 58
#define INTRO_TECMO_STREAM_BOUNDS_MAX_X 122
#define INTRO_TECMO_STREAM_BOUNDS_MIN_Y 41
#define INTRO_TECMO_STREAM_BOUNDS_MAX_Y 169

static int text_equals(const char *a, const char *b)
{
    return strcmp(a, b) == 0;
}

static uint32_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return 0xFF000000U | ((uint32_t)r << 16U) | ((uint32_t)g << 8U) | (uint32_t)b;
}

static const char *mode_name(TecmoPlayMode mode)
{
    if (mode == TECMO_MODE_MAIN_MENU) {
        return "MAIN MENU";
    }
    if (mode == TECMO_MODE_TITLE_SCREEN) {
        return "TITLE SCREEN";
    }
    if (mode == TECMO_MODE_INTRO_PROBE) {
        return "INTRO LAB";
    }
    if (mode == TECMO_MODE_CHR_PLAYGROUND) {
        return "CHR PLAYGROUND";
    }
    if (mode == TECMO_MODE_FIRST_SPRITE) {
        return "INTRO OUTPUT";
    }
    if (mode == TECMO_MODE_PLAY_SETUP) {
        return "PLAY SETUP";
    }
    if (mode == TECMO_MODE_ROSTERS) {
        return "ROSTERS";
    }
    if (mode == TECMO_MODE_COURT) {
        return "COURT";
    }
    if (mode == TECMO_MODE_START_GAME_MENU) {
        return "START GAME MENU";
    }
    if (mode == TECMO_MODE_PRESEASON_MENU) {
        return "PRESEASON MENU";
    }
    return "UNKNOWN";
}

static void add_team_if_missing(TecmoRuntime *runtime, const char *team)
{
    for (size_t i = 0; i < runtime->team_count; ++i) {
        if (text_equals(runtime->teams[i], team)) {
            return;
        }
    }
    if (runtime->team_count < sizeof(runtime->teams) / sizeof(runtime->teams[0])) {
        size_t len = strlen(team);
        if (len >= sizeof(runtime->teams[0])) {
            len = sizeof(runtime->teams[0]) - 1U;
        }
        memcpy(runtime->teams[runtime->team_count], team, len);
        runtime->teams[runtime->team_count][len] = '\0';
        ++runtime->team_count;
    }
}

static const RosterRecord *selected_player_record(const TecmoRuntime *runtime)
{
    size_t seen = 0;
    if (runtime->team_count == 0 || runtime->selected_team >= runtime->team_count) {
        return 0;
    }

    for (size_t i = 0; i < runtime->roster.count; ++i) {
        const RosterRecord *record = &runtime->roster.records[i];
        if (text_equals(record->team, runtime->teams[runtime->selected_team])) {
            if (seen == runtime->selected_player) {
                return record;
            }
            ++seen;
        }
    }
    return 0;
}

static size_t selected_team_player_count(const TecmoRuntime *runtime)
{
    size_t count = 0;
    if (runtime->team_count == 0 || runtime->selected_team >= runtime->team_count) {
        return 0;
    }
    for (size_t i = 0; i < runtime->roster.count; ++i) {
        if (text_equals(runtime->roster.records[i].team, runtime->teams[runtime->selected_team])) {
            ++count;
        }
    }
    return count;
}

static uint32_t chr_bank_count(const TecmoRuntime *runtime)
{
    uint64_t count = runtime->title_chr_byte_count / TITLE_CHR_BANK_BYTES;
    if (count == 0) {
        return 1U;
    }
    if (count > 32U) {
        count = 32U;
    }
    return (uint32_t)count;
}

static uint32_t selected_chr_bank(const TecmoRuntime *runtime)
{
    uint32_t count = chr_bank_count(runtime);
    if (runtime->selected_chr_bank >= count) {
        return count - 1U;
    }
    return runtime->selected_chr_bank;
}

static uint32_t selected_chr_table(const TecmoRuntime *runtime)
{
    return runtime->selected_chr_table & 1U;
}

static uint16_t selected_intro_tile_id(const TecmoRuntime *runtime)
{
    return (uint16_t)(selected_chr_table(runtime) * 0x100U + (runtime->intro_source_tile & 0xFFU));
}

static void set_runtime_status(char *dest, size_t dest_size, const char *text)
{
    if (dest_size == 0) {
        return;
    }
    if (text == NULL) {
        text = "";
    }
    (void)snprintf(dest, dest_size, "%s", text);
}

bool tecmo_runtime_init_with_flags(TecmoRuntime *runtime,
                                   TecmoGameMemory *memory,
                                   const char *project_root,
                                   unsigned flags)
{
    const bool allow_empty_roster = (flags & TECMO_RUNTIME_INIT_ALLOW_EMPTY_ROSTER) != 0U;

    memset(runtime, 0, sizeof(*runtime));
    runtime->memory = memory;
    runtime->selected_chr_bank = 31U;
    tecmo_intro_layout_init(runtime);
    tecmo_intro_trace_load(runtime, project_root);
    (void)tecmo_intro_screen_load(&runtime->intro_presents_asset,
                                  project_root,
                                  TECMO_INTRO_PRESENTS_ENTRY_ID,
                                  TECMO_INTRO_SCREEN_PRESENTS);
    (void)tecmo_intro_screen_load(&runtime->intro_license_asset,
                                  project_root,
                                  TECMO_INTRO_LICENSE_ENTRY_ID,
                                  TECMO_INTRO_SCREEN_LICENSE);
    (void)tecmo_intro_arena_tile_layer_load(&runtime->intro_arena_tile_layer, project_root);
    (void)tecmo_intro_arena_sprite_groups_load(&runtime->intro_arena_sprite_groups,
                                               project_root);
    (void)tecmo_intro_arena_capture_load(&runtime->intro_arena_capture, project_root);
    (void)tecmo_intro_ready_asset_load(&runtime->intro_ready_asset, project_root);
    (void)tecmo_intro_warriors_asset_load(&runtime->intro_warriors_asset, project_root);
    (void)tecmo_intro_clippers_asset_load(&runtime->intro_clippers_asset, project_root);
    (void)tecmo_intro_bucks_asset_load(&runtime->intro_bucks_asset, project_root);
    (void)tecmo_intro_pass_asset_load(&runtime->intro_pass_asset, project_root);
    (void)tecmo_intro_finale_asset_load(&runtime->intro_finale_asset, project_root);
    (void)tecmo_title_asset_load(&runtime->title_asset, project_root);
    (void)tecmo_start_game_menu_asset_load(&runtime->start_game_menu_asset, project_root);
    (void)tecmo_preseason_asset_load(&runtime->preseason_asset, project_root);

    if (tecmo_collect_rosters(project_root, &runtime->roster) != 0) {
        if (!allow_empty_roster) {
            return false;
        }
        memset(&runtime->roster, 0, sizeof(runtime->roster));
    }

    for (size_t i = 0; i < runtime->roster.count; ++i) {
        add_team_if_missing(runtime, runtime->roster.records[i].team);
    }

    if (tecmo_load_chr_data(project_root, &runtime->title_chr_bytes, &runtime->title_chr_byte_count) == 0) {
        if (runtime->intro_presents_asset.available &&
            !tecmo_intro_screen_chr_available(&runtime->intro_presents_asset,
                                              runtime->title_chr_bytes,
                                              runtime->title_chr_byte_count)) {
            runtime->intro_presents_asset.available = false;
            set_runtime_status(runtime->intro_presents_asset.status,
                               sizeof(runtime->intro_presents_asset.status),
                               "TISC-1 chr/all contract rejected");
        }
        if (runtime->intro_license_asset.available &&
            !tecmo_intro_screen_chr_available(&runtime->intro_license_asset,
                                              runtime->title_chr_bytes,
                                              runtime->title_chr_byte_count)) {
            runtime->intro_license_asset.available = false;
            set_runtime_status(runtime->intro_license_asset.status,
                               sizeof(runtime->intro_license_asset.status),
                               "TISC-1 chr/all contract rejected");
        }
        if (runtime->intro_finale_asset.available &&
            !tecmo_intro_finale_asset_chr_available(&runtime->intro_finale_asset,
                                                    runtime->title_chr_bytes,
                                                    runtime->title_chr_byte_count)) {
            runtime->intro_finale_asset.available = false;
            set_runtime_status(runtime->intro_finale_asset.status,
                               sizeof(runtime->intro_finale_asset.status),
                               "TFIN-1 chr/all contract rejected");
        }
        if ((runtime->title_asset.attract_available || runtime->title_asset.start_available) &&
            !tecmo_title_asset_chr_available(&runtime->title_asset,
                                             runtime->title_chr_bytes,
                                             runtime->title_chr_byte_count)) {
            runtime->title_asset.attract_available = false;
            runtime->title_asset.start_available = false;
            set_runtime_status(runtime->title_asset.status,
                               sizeof(runtime->title_asset.status),
                               "TATR-2/TTLE-1 chr/all contract rejected");
        }
        if (runtime->start_game_menu_asset.available &&
            !tecmo_start_game_menu_asset_chr_available(&runtime->start_game_menu_asset,
                                                       runtime->title_chr_bytes,
                                                       runtime->title_chr_byte_count)) {
            runtime->start_game_menu_asset.available = false;
            set_runtime_status(runtime->start_game_menu_asset.status,
                               sizeof(runtime->start_game_menu_asset.status),
                               "TSGM-1 chr/all contract rejected");
        }
        if (runtime->preseason_asset.available &&
            !tecmo_preseason_asset_chr_available(&runtime->preseason_asset,
                                                 runtime->title_chr_bytes,
                                                 runtime->title_chr_byte_count)) {
            runtime->preseason_asset.available = false;
            set_runtime_status(runtime->preseason_asset.status,
                               sizeof(runtime->preseason_asset.status),
                               "TPRE-1 chr/all contract rejected");
        }
        if (runtime->selected_chr_bank >= chr_bank_count(runtime)) {
            runtime->selected_chr_bank = chr_bank_count(runtime) - 1U;
        }
        if (tecmo_load_original_title_glyphs(project_root, &runtime->title_glyphs) == 0) {
            runtime->title_probe_available = true;
            (void)tecmo_load_title_glyphs_for_text(project_root, "TECMO PRESENTS", &runtime->intro_glyphs);
        }
    } else {
        tecmo_free_buffer(runtime->title_chr_bytes);
        runtime->title_chr_bytes = NULL;
        runtime->title_chr_byte_count = 0;
    }

    runtime->mode = TECMO_MODE_MAIN_MENU;
    runtime->frame_seconds = 1.0f / 60.0f;
    runtime->player_x = 320.0f;
    runtime->player_y = 260.0f;
    runtime->ball_x = runtime->player_x + 14.0f;
    runtime->ball_y = runtime->player_y - 8.0f;
    return runtime->team_count > 0 || allow_empty_roster;
}

bool tecmo_runtime_init(TecmoRuntime *runtime, TecmoGameMemory *memory, const char *project_root)
{
    return tecmo_runtime_init_with_flags(runtime, memory, project_root, 0U);
}

void tecmo_runtime_shutdown(TecmoRuntime *runtime)
{
    tecmo_free_buffer(runtime->title_chr_bytes);
    runtime->title_chr_bytes = NULL;
    runtime->title_chr_byte_count = 0;
    roster_table_free(&runtime->roster);
}

void tecmo_runtime_set_mode(TecmoRuntime *runtime, TecmoPlayMode mode)
{
    runtime->mode = mode;
    runtime->mode_frame_counter = 0;
    if (mode == TECMO_MODE_MAIN_MENU && runtime->selected_menu_item >= MAIN_MENU_COUNT) {
        runtime->selected_menu_item = MAIN_MENU_PLAY_GAME;
    }
    if (mode == TECMO_MODE_FIRST_SPRITE) {
        runtime->intro_output_step = TECMO_INTRO_OUTPUT_TITLE_STEP;
        runtime->intro_handoff_complete = false;
        runtime->intro_next_screen = 0U;
    }
    if (mode == TECMO_MODE_TITLE_SCREEN) {
        runtime->title_start_armed = false;
        runtime->title_confirming = false;
        runtime->title_confirmation_frame = 0U;
    }
    if (mode == TECMO_MODE_START_GAME_MENU) {
        tecmo_start_game_menu_state_init(&runtime->start_game_menu_state);
    }
    if (mode == TECMO_MODE_PRESEASON_MENU) {
        tecmo_preseason_state_init(&runtime->preseason_state);
        if (runtime->preseason_asset.available) {
            runtime->preseason_state.direction_cooldown =
                runtime->preseason_asset.accepted_input_seed;
        }
    }
    runtime->previous_input = (TecmoInput){0};
    runtime->previous_player_two_input = (TecmoInput){0};
}

static void update_main_menu(TecmoRuntime *runtime, const TecmoControlFrame *controls)
{
    if (controls->pressed.up && runtime->selected_menu_item > 0) {
        --runtime->selected_menu_item;
    }
    if (controls->pressed.down &&
        runtime->selected_menu_item + 1U < MAIN_MENU_COUNT) {
        ++runtime->selected_menu_item;
    }
    if (controls->pressed.cancel) {
        runtime->quit_requested = true;
    }
    if (controls->pressed.confirm) {
        if (runtime->selected_menu_item == MAIN_MENU_PLAY_GAME) {
            tecmo_runtime_set_mode(runtime, TECMO_MODE_FIRST_SPRITE);
        } else {
            runtime->quit_requested = true;
        }
    }
}

static void update_title_screen(TecmoRuntime *runtime, const TecmoControlFrame *controls)
{
    if (runtime->title_confirming) {
        ++runtime->title_confirmation_frame;
        if (runtime->title_confirmation_frame >= tecmo_title_confirmation_handoff_frame()) {
            tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
        }
    } else if (controls->pressed.cancel) {
        tecmo_runtime_set_mode(runtime, TECMO_MODE_MAIN_MENU);
    } else if (runtime->mode_frame_counter >= TECMO_TITLE_START_LOAD_FRAMES) {
        if (!controls->held.confirm) {
            runtime->title_start_armed = true;
        }
        if (runtime->title_start_armed && controls->pressed.confirm) {
            runtime->title_confirming = true;
            runtime->title_confirmation_frame = 1U;
        }
    }
}

static void update_start_game_menu(TecmoRuntime *runtime,
                                   const TecmoControlFrame *controls)
{
    TecmoStartGameMenuAction action =
        tecmo_start_game_menu_update(&runtime->start_game_menu_state,
                                     &runtime->start_game_menu_asset,
                                     controls);
    if (action == TECMO_START_GAME_MENU_ACTION_PLAY_SETUP) {
        tecmo_runtime_set_mode(runtime, TECMO_MODE_PLAY_SETUP);
    } else if (action == TECMO_START_GAME_MENU_ACTION_ROSTERS) {
        tecmo_runtime_set_mode(runtime, TECMO_MODE_ROSTERS);
    } else if (action == TECMO_START_GAME_MENU_ACTION_PRESEASON) {
        tecmo_runtime_set_mode(runtime, TECMO_MODE_PRESEASON_MENU);
    }
}

static void update_preseason_menu(TecmoRuntime *runtime,
                                  const TecmoControlFrame *player_one,
                                  const TecmoControlFrame *player_two)
{
    TecmoPreseasonAction action = tecmo_preseason_update(
        &runtime->preseason_state, &runtime->preseason_asset,
        player_one, player_two);
    if (action == TECMO_PRESEASON_ACTION_BACK_TO_START_MENU) {
        tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
        runtime->start_game_menu_state.frame =
            runtime->start_game_menu_asset.stable_frame;
        runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_ROOT;
        runtime->start_game_menu_state.root_selection = 0U;
        runtime->start_game_menu_state.direction_cooldown =
            runtime->start_game_menu_asset.accepted_input_seed;
        runtime->start_game_menu_state.cursor_delay =
            runtime->start_game_menu_asset.cursor_commit_delay_frames;
    }
}

static void update_probe_screen(TecmoRuntime *runtime, const TecmoControlFrame *controls)
{
    if (runtime->mode == TECMO_MODE_INTRO_PROBE) {
        uint32_t count = chr_bank_count(runtime);
        if (controls->pressed.bank_prev && runtime->selected_chr_bank > 0U) {
            --runtime->selected_chr_bank;
        }
        if (controls->pressed.bank_next &&
            runtime->selected_chr_bank + 1U < count) {
            ++runtime->selected_chr_bank;
        }
        if (controls->pressed.table_toggle) {
            runtime->selected_chr_table ^= 1U;
        }
        if (controls->pressed.tab) {
            runtime->intro_canvas_focus = !runtime->intro_canvas_focus;
        }
        if (controls->pressed.left) {
            if (runtime->intro_canvas_focus) {
                tecmo_intro_layout_move_canvas_cursor(runtime, -1, 0);
            } else {
                tecmo_intro_layout_move_source_tile(runtime, -1, 0);
            }
        }
        if (controls->pressed.right) {
            if (runtime->intro_canvas_focus) {
                tecmo_intro_layout_move_canvas_cursor(runtime, 1, 0);
            } else {
                tecmo_intro_layout_move_source_tile(runtime, 1, 0);
            }
        }
        if (controls->pressed.up) {
            if (runtime->intro_canvas_focus) {
                tecmo_intro_layout_move_canvas_cursor(runtime, 0, -1);
            } else {
                tecmo_intro_layout_move_source_tile(runtime, 0, -1);
            }
        }
        if (controls->pressed.down) {
            if (runtime->intro_canvas_focus) {
                tecmo_intro_layout_move_canvas_cursor(runtime, 0, 1);
            } else {
                tecmo_intro_layout_move_source_tile(runtime, 0, 1);
            }
        }
        if (controls->pressed.shoot) {
            tecmo_intro_layout_add_current(runtime);
        }
        if (controls->pressed.preset_rabbit) {
            tecmo_intro_layout_add_rabbit_preset(runtime);
        }
        if (controls->pressed.preset_tecmo) {
            tecmo_intro_layout_add_tecmo_logo_preset(runtime);
        }
        if (controls->pressed.preset_composite) {
            tecmo_intro_layout_add_composite_preset(runtime);
        }
        if (controls->pressed.remove) {
            tecmo_intro_layout_remove_last(runtime);
        }
        if (controls->pressed.save) {
            (void)tecmo_intro_layout_save(runtime);
        }
    } else if (runtime->mode == TECMO_MODE_CHR_PLAYGROUND) {
        uint32_t count = chr_bank_count(runtime);
        if (controls->pressed.left && runtime->selected_chr_bank > 0U) {
            --runtime->selected_chr_bank;
        }
        if (controls->pressed.right &&
            runtime->selected_chr_bank + 1U < count) {
            ++runtime->selected_chr_bank;
        }
        if (controls->pressed.tab) {
            runtime->selected_chr_bank = (runtime->selected_chr_bank + 1U) % count;
        }
        if (controls->pressed.up ||
            controls->pressed.down ||
            controls->pressed.table_toggle) {
            runtime->selected_chr_table ^= 1U;
        }
    }

    if (controls->pressed.confirm ||
        controls->pressed.cancel) {
        tecmo_runtime_set_mode(runtime, TECMO_MODE_MAIN_MENU);
    }
}

static void update_roster_selection(TecmoRuntime *runtime, const TecmoControlFrame *controls, bool allow_start_game)
{
    size_t player_count = selected_team_player_count(runtime);

    if (controls->pressed.left && runtime->selected_team > 0) {
        --runtime->selected_team;
        runtime->selected_player = 0;
    }
    if (controls->pressed.right &&
        runtime->selected_team + 1U < runtime->team_count) {
        ++runtime->selected_team;
        runtime->selected_player = 0;
    }
    if (controls->pressed.up && runtime->selected_player > 0) {
        --runtime->selected_player;
    }
    if (controls->pressed.down &&
        runtime->selected_player + 1U < player_count) {
        ++runtime->selected_player;
    }
    if (controls->pressed.cancel) {
        tecmo_runtime_set_mode(runtime, TECMO_MODE_MAIN_MENU);
    }
    if (allow_start_game && controls->pressed.confirm) {
        tecmo_runtime_set_mode(runtime, TECMO_MODE_COURT);
    }
}

static void clamp_player_to_court(TecmoRuntime *runtime)
{
    if (runtime->player_x < (float)COURT_LEFT + 12.0f) {
        runtime->player_x = (float)COURT_LEFT + 12.0f;
    }
    if (runtime->player_x > (float)COURT_RIGHT - 12.0f) {
        runtime->player_x = (float)COURT_RIGHT - 12.0f;
    }
    if (runtime->player_y < (float)COURT_TOP + 18.0f) {
        runtime->player_y = (float)COURT_TOP + 18.0f;
    }
    if (runtime->player_y > (float)COURT_BOTTOM - 18.0f) {
        runtime->player_y = (float)COURT_BOTTOM - 18.0f;
    }
}

static void update_court(TecmoRuntime *runtime, const TecmoControlFrame *controls)
{
    const float speed = 3.0f;
    const float hoop_x = 542.0f;
    const float hoop_y = 126.0f;
    const TecmoInput *input = &controls->held;

    if (input->left) {
        runtime->player_x -= speed;
    }
    if (input->right) {
        runtime->player_x += speed;
    }
    if (input->up) {
        runtime->player_y -= speed;
    }
    if (input->down) {
        runtime->player_y += speed;
    }
    clamp_player_to_court(runtime);

    if (controls->pressed.cancel) {
        tecmo_runtime_set_mode(runtime, TECMO_MODE_PLAY_SETUP);
        runtime->ball_in_air = false;
    }

    if (!runtime->ball_in_air) {
        runtime->ball_x = runtime->player_x + 14.0f;
        runtime->ball_y = runtime->player_y - 8.0f;
        if (controls->pressed.shoot) {
            float dx = hoop_x - runtime->ball_x;
            float dy = hoop_y - runtime->ball_y;
            runtime->ball_vx = dx / 44.0f;
            runtime->ball_vy = dy / 44.0f - 4.0f;
            runtime->ball_in_air = true;
        }
    } else {
        float dx;
        float dy;
        runtime->ball_x += runtime->ball_vx;
        runtime->ball_y += runtime->ball_vy;
        runtime->ball_vy += 0.18f;
        dx = runtime->ball_x - hoop_x;
        dy = runtime->ball_y - hoop_y;
        if ((dx * dx + dy * dy) < 20.0f * 20.0f && runtime->ball_vy > 0.0f) {
            ++runtime->score;
            runtime->ball_in_air = false;
        }
        if (runtime->ball_y > (float)COURT_BOTTOM + 40.0f ||
            runtime->ball_x < 0.0f ||
            runtime->ball_x > 640.0f) {
            runtime->ball_in_air = false;
        }
    }
}

static void update_first_sprite_probe(TecmoRuntime *runtime, const TecmoControlFrame *controls)
{
    if (controls->pressed.confirm) {
        tecmo_runtime_set_mode(runtime, TECMO_MODE_TITLE_SCREEN);
    } else if (controls->pressed.cancel) {
        tecmo_runtime_set_mode(runtime, TECMO_MODE_MAIN_MENU);
    } else if (controls->pressed.left &&
               runtime->intro_output_step > 0U) {
        --runtime->intro_output_step;
        runtime->mode_frame_counter = 0;
    } else if (controls->pressed.right &&
               runtime->intro_output_step + 1U < TECMO_INTRO_OUTPUT_STEP_COUNT) {
        ++runtime->intro_output_step;
        runtime->mode_frame_counter = 0;
    } else if (runtime->intro_output_step == TECMO_INTRO_OUTPUT_TITLE_STEP &&
               runtime->mode_frame_counter >= TECMO_INTRO_TITLE_AUTO_FRAME) {
        runtime->intro_output_step = TECMO_INTRO_OUTPUT_LICENSE_STEP;
        runtime->mode_frame_counter = 0;
    } else if (runtime->intro_output_step == TECMO_INTRO_OUTPUT_LICENSE_STEP &&
               runtime->mode_frame_counter >= TECMO_INTRO_LICENSE_AUTO_FRAME) {
        runtime->intro_output_step = TECMO_INTRO_OUTPUT_ARENA_STEP;
        runtime->mode_frame_counter = 0;
    } else if (runtime->intro_output_step == TECMO_INTRO_OUTPUT_ARENA_STEP &&
               runtime->mode_frame_counter >= TECMO_INTRO_ARENA_TO_READY_FRAME) {
        runtime->intro_output_step = TECMO_INTRO_OUTPUT_READY_STEP;
        runtime->mode_frame_counter = 0;
    } else if (runtime->intro_output_step == TECMO_INTRO_OUTPUT_READY_STEP &&
               runtime->mode_frame_counter >= TECMO_INTRO_READY_TO_WARRIORS_FRAME) {
        runtime->intro_output_step = TECMO_INTRO_OUTPUT_WARRIORS_STEP;
        runtime->mode_frame_counter = 0;
    } else if (runtime->intro_output_step == TECMO_INTRO_OUTPUT_WARRIORS_STEP &&
               runtime->mode_frame_counter >= TECMO_INTRO_WARRIORS_TO_NEXT_FRAME) {
        runtime->intro_next_screen = TECMO_INTRO_WARRIORS_NEXT_SCREEN;
        runtime->intro_output_step = TECMO_INTRO_OUTPUT_CLIPPERS_STEP;
        runtime->mode_frame_counter = 0U;
    } else if (runtime->intro_output_step == TECMO_INTRO_OUTPUT_CLIPPERS_STEP &&
               runtime->mode_frame_counter >= TECMO_INTRO_CLIPPERS_HANDOFF_FRAME) {
        runtime->intro_output_step = TECMO_INTRO_OUTPUT_BUCKS_STEP;
        runtime->mode_frame_counter = 0U;
    } else if (runtime->intro_output_step == TECMO_INTRO_OUTPUT_BUCKS_STEP &&
               runtime->mode_frame_counter >= TECMO_INTRO_BUCKS_HANDOFF_FRAME) {
        runtime->intro_output_step = TECMO_INTRO_OUTPUT_PASS_STEP;
        runtime->mode_frame_counter = 0U;
    } else if (runtime->intro_output_step == TECMO_INTRO_OUTPUT_PASS_STEP &&
               runtime->mode_frame_counter >= TECMO_INTRO_PASS_HANDOFF_FRAME) {
        runtime->intro_output_step = TECMO_INTRO_OUTPUT_FINALE_STEP;
        runtime->mode_frame_counter = 0U;
    } else if (runtime->intro_output_step == TECMO_INTRO_OUTPUT_FINALE_STEP &&
               runtime->mode_frame_counter >= tecmo_intro_finale_hold_frame()) {
        runtime->intro_output_step = TECMO_INTRO_OUTPUT_ATTRACT_STEP;
        runtime->intro_handoff_complete = false;
        runtime->mode_frame_counter = 0U;
    } else if (runtime->intro_output_step == TECMO_INTRO_OUTPUT_ATTRACT_STEP &&
               runtime->mode_frame_counter >= tecmo_title_attract_reset_frame()) {
        runtime->intro_output_step = TECMO_INTRO_OUTPUT_TITLE_STEP;
        runtime->intro_handoff_complete = false;
        runtime->mode_frame_counter = 0U;
    } else if (runtime->intro_output_step == TECMO_INTRO_OUTPUT_ATTRACT_STEP &&
               tecmo_title_attract_natural_complete(runtime->mode_frame_counter)) {
        runtime->intro_handoff_complete = true;
    }
}

static void write_runtime_watch_memory(TecmoRuntime *runtime)
{
    tecmo_cpu_ram_write(runtime->memory, 0x0000, (uint8_t)runtime->mode);
    tecmo_cpu_ram_write(runtime->memory, 0x0001, (uint8_t)runtime->selected_team);
    tecmo_cpu_ram_write(runtime->memory, 0x0002, (uint8_t)runtime->selected_player);
    tecmo_cpu_ram_write(runtime->memory, 0x0003,
                        runtime->mode == TECMO_MODE_START_GAME_MENU
                            ? runtime->start_game_menu_state.root_selection
                            : (uint8_t)runtime->selected_menu_item);
    tecmo_cpu_ram_write(runtime->memory, 0x0004, (uint8_t)(runtime->frame_counter & 0xFFU));
    tecmo_cpu_ram_write(runtime->memory, 0x0005, (uint8_t)((runtime->frame_counter >> 8U) & 0xFFU));
    tecmo_cpu_ram_write(runtime->memory, 0x0006, (uint8_t)selected_chr_bank(runtime));
    tecmo_cpu_ram_write(runtime->memory, 0x0007, (uint8_t)chr_bank_count(runtime));
    tecmo_cpu_ram_write(runtime->memory, 0x0008, (uint8_t)selected_chr_table(runtime));
}

void tecmo_runtime_update_players(TecmoRuntime *runtime,
                                  const TecmoInput *player_one,
                                  const TecmoInput *player_two)
{
    TecmoControlFrame player_one_controls;
    TecmoControlFrame player_two_controls;

    tecmo_control_frame_build(&player_one_controls,
                              player_one,
                              &runtime->previous_input);
    tecmo_control_frame_build(&player_two_controls,
                              player_two,
                              &runtime->previous_player_two_input);
    ++runtime->frame_counter;
    ++runtime->mode_frame_counter;

    if (player_one_controls.pressed.debug_toggle) {
        runtime->debug_overlay = !runtime->debug_overlay;
    }

    if (runtime->mode == TECMO_MODE_MAIN_MENU) {
        update_main_menu(runtime, &player_one_controls);
    } else if (runtime->mode == TECMO_MODE_TITLE_SCREEN) {
        update_title_screen(runtime, &player_one_controls);
    } else if (runtime->mode == TECMO_MODE_INTRO_PROBE ||
               runtime->mode == TECMO_MODE_CHR_PLAYGROUND) {
        update_probe_screen(runtime, &player_one_controls);
    } else if (runtime->mode == TECMO_MODE_FIRST_SPRITE) {
        update_first_sprite_probe(runtime, &player_one_controls);
    } else if (runtime->mode == TECMO_MODE_PLAY_SETUP) {
        update_roster_selection(runtime, &player_one_controls, true);
    } else if (runtime->mode == TECMO_MODE_ROSTERS) {
        update_roster_selection(runtime, &player_one_controls, false);
    } else if (runtime->mode == TECMO_MODE_COURT) {
        update_court(runtime, &player_one_controls);
    } else if (runtime->mode == TECMO_MODE_START_GAME_MENU) {
        update_start_game_menu(runtime, &player_one_controls);
    } else if (runtime->mode == TECMO_MODE_PRESEASON_MENU) {
        update_preseason_menu(runtime, &player_one_controls,
                              &player_two_controls);
    }

    write_runtime_watch_memory(runtime);
    runtime->previous_input = player_one_controls.held;
    runtime->previous_player_two_input = player_two_controls.held;
}

void tecmo_runtime_update(TecmoRuntime *runtime, const TecmoInput *input)
{
    tecmo_runtime_update_players(runtime, input, NULL);
}

static void clear(TecmoFramebuffer *fb, uint32_t color)
{
    for (int y = 0; y < fb->height; ++y) {
        uint32_t *row = fb->pixels + (size_t)y * (size_t)fb->pitch_pixels;
        for (int x = 0; x < fb->width; ++x) {
            row[x] = color;
        }
    }
}

static void rect(TecmoFramebuffer *fb, int x, int y, int w, int h, uint32_t color)
{
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > fb->width ? fb->width : x + w;
    int y1 = y + h > fb->height ? fb->height : y + h;
    for (int py = y0; py < y1; ++py) {
        uint32_t *row = fb->pixels + (size_t)py * (size_t)fb->pitch_pixels;
        for (int px = x0; px < x1; ++px) {
            row[px] = color;
        }
    }
}

static void outline_rect(TecmoFramebuffer *fb, int x, int y, int w, int h, uint32_t color)
{
    rect(fb, x, y, w, 1, color);
    rect(fb, x, y + h - 1, w, 1, color);
    rect(fb, x, y, 1, h, color);
    rect(fb, x + w - 1, y, 1, h, color);
}

static uint8_t glyph_bits(char c, int row)
{
    static const uint8_t font[43][7] = {
        {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
        {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}, {0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E},
        {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
        {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
        {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
        {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
        {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
        {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
        {0x0F,0x10,0x10,0x13,0x11,0x11,0x0F}, {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
        {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, {0x07,0x02,0x02,0x02,0x12,0x12,0x0C},
        {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
        {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
        {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
        {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
        {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}, {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
        {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, {0x11,0x11,0x11,0x11,0x0A,0x0A,0x04},
        {0x11,0x11,0x11,0x15,0x15,0x1B,0x11}, {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
        {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}, {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x1F,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C}, {0x04,0x04,0x04,0x04,0x00,0x00,0x04},
        {0x00,0x00,0x0E,0x00,0x00,0x0E,0x00}, {0x00,0x00,0x04,0x00,0x04,0x00,0x00},
        {0x02,0x02,0x04,0x00,0x00,0x00,0x00},
    };
    int index = -1;
    if (c >= '0' && c <= '9') {
        index = c - '0';
    } else if (c >= 'A' && c <= 'Z') {
        index = 10 + c - 'A';
    } else if (c >= 'a' && c <= 'z') {
        index = 10 + c - 'a';
    } else if (c == ' ') {
        index = 36;
    } else if (c == '-') {
        index = 37;
    } else if (c == '.') {
        index = 38;
    } else if (c == '!') {
        index = 39;
    } else if (c == '=') {
        index = 40;
    } else if (c == ':') {
        index = 41;
    } else if (c == '\'') {
        index = 42;
    }
    if (index < 0) {
        return 0;
    }
    return font[index][row];
}

static void draw_text(TecmoFramebuffer *fb, int x, int y, const char *text, uint32_t color, int scale)
{
    int cursor_x = x;
    if (scale < 1) {
        scale = 1;
    }
    for (const char *p = text; *p != '\0'; ++p) {
        for (int row = 0; row < 7; ++row) {
            uint8_t bits = glyph_bits(*p, row);
            for (int col = 0; col < 5; ++col) {
                if ((bits & (uint8_t)(1U << (4 - col))) != 0) {
                    rect(fb, cursor_x + col * scale, y + row * scale, scale, scale, color);
                }
            }
        }
        cursor_x += 6 * scale;
    }
}

static int text_width_pixels(const char *text, int scale)
{
    if (scale < 1) {
        scale = 1;
    }
    return (int)strlen(text) * 6 * scale;
}

static void draw_centered_text(TecmoFramebuffer *fb, int y, const char *text, uint32_t color, int scale)
{
    int width = text_width_pixels(text, scale);
    int x = (fb->width - width) / 2;
    draw_text(fb, x, y, text, color, scale);
}

static void draw_button(TecmoFramebuffer *fb, int x, int y, int w, int h, const char *label, bool selected)
{
    uint32_t fill = selected ? rgb(220, 198, 80) : rgb(34, 42, 50);
    uint32_t border = selected ? rgb(248, 244, 198) : rgb(100, 118, 132);
    uint32_t text = selected ? rgb(24, 24, 22) : rgb(230, 235, 226);

    rect(fb, x, y, w, h, border);
    rect(fb, x + 2, y + 2, w - 4, h - 4, fill);
    draw_text(fb, x + 18, y + 16, label, text, 2);
}

static void render_main_menu(const TecmoRuntime *runtime, TecmoFramebuffer *fb)
{
    clear(fb, rgb(14, 18, 22));
    rect(fb, 0, 0, fb->width, 70, rgb(120, 16, 24));
    draw_text(fb, 34, 24, "TECMO BASKETBALL NATIVE PORT", rgb(248, 248, 232), 2);
    draw_text(fb, 74, 128, "LOCAL HOBBY PORT PROTOTYPE", rgb(144, 176, 192), 1);

    draw_button(fb, 150, 186, 340, 42, "PLAY GAME", runtime->selected_menu_item == MAIN_MENU_PLAY_GAME);
    draw_button(fb, 150, 248, 340, 42, "QUIT", runtime->selected_menu_item == MAIN_MENU_QUIT);

    draw_text(fb, 150, 364, "UP DOWN SELECT   ENTER CONFIRM   ESC QUIT", rgb(226, 228, 208), 1);
    draw_text(fb, 82, 452, "NO ROM ASM OR EXTRACTED ASSETS ARE LOADED FROM THIS REPO", rgb(124, 148, 160), 1);
}

static bool render_intro_native_title_screen(const TecmoRuntime *runtime,
                                             TecmoFramebuffer *fb,
                                             const char *prompt,
                                             bool draw_debug);

static void render_title_screen_mode(const TecmoRuntime *runtime, TecmoFramebuffer *fb)
{
    clear(fb, rgb(0, 0, 0));
    if (runtime->mode_frame_counter < TECMO_TITLE_START_LOAD_FRAMES) {
        return;
    }
    if (!tecmo_title_start_draw(fb, &runtime->title_asset,
                                runtime->title_chr_bytes,
                                runtime->title_chr_byte_count,
                                runtime->title_confirming,
                                runtime->title_confirmation_frame,
                                64, 0, 2))
        draw_centered_text(fb, 212, "NATIVE TTLE-1 TITLE UNAVAILABLE", rgb(252, 236, 170), 2);
}

static void render_start_game_menu_mode(const TecmoRuntime *runtime,
                                        TecmoFramebuffer *fb)
{
    clear(fb, rgb(0, 0, 0));
    if (!tecmo_start_game_menu_draw(fb,
                                    &runtime->start_game_menu_asset,
                                    &runtime->start_game_menu_state,
                                    &runtime->title_asset,
                                    runtime->title_chr_bytes,
                                    runtime->title_chr_byte_count,
                                    64, 0, 2)) {
        draw_centered_text(fb, 212, "NATIVE TSGM-1 START MENU UNAVAILABLE",
                           rgb(252, 236, 170), 2);
    }
}

static void render_preseason_menu_mode(const TecmoRuntime *runtime,
                                       TecmoFramebuffer *fb)
{
    clear(fb, rgb(0, 0, 0));
    if (!tecmo_preseason_draw(fb,
                              &runtime->preseason_asset,
                              &runtime->preseason_state,
                              &runtime->start_game_menu_asset,
                              runtime->title_chr_bytes,
                              runtime->title_chr_byte_count,
                              64, 0, 2)) {
        draw_centered_text(fb, 212, "NATIVE TPRE-1 PRESEASON MENU UNAVAILABLE",
                           rgb(252, 236, 170), 2);
    }
}

static void render_roster_browser(const TecmoRuntime *runtime, TecmoFramebuffer *fb, bool play_setup)
{
    char line[256];
    const char *team = runtime->team_count > 0 ? runtime->teams[runtime->selected_team] : "NO DATA";
    size_t player_row = 0;

    clear(fb, rgb(16, 20, 24));
    rect(fb, 0, 0, fb->width, 48, rgb(120, 16, 24));
    draw_text(fb, 24, 16, play_setup ? "PLAY PROTOTYPE SETUP" : "ROSTERS", rgb(248, 248, 232), 2);

    (void)snprintf(line, sizeof(line), "TEAM %u OF %u: %s",
                   (unsigned)(runtime->selected_team + 1U),
                   (unsigned)runtime->team_count,
                   team);
    draw_text(fb, 32, 72, line, rgb(238, 238, 214), 2);
    draw_text(fb, 32, 102,
              play_setup ? "ARROWS SELECT   ENTER START PROTOTYPE   ESC MENU" : "ARROWS BROWSE ROSTERS   ESC MENU",
              rgb(144, 176, 192),
              1);

    for (size_t i = 0; i < runtime->roster.count; ++i) {
        const RosterRecord *record = &runtime->roster.records[i];
        if (!text_equals(record->team, team)) {
            continue;
        }
        if (player_row == runtime->selected_player) {
            rect(fb, 28, 132 + (int)player_row * 22, 430, 18, rgb(220, 198, 80));
        }
        (void)snprintf(line, sizeof(line), "%2u  %-20s  #%02u  %u'%u\"",
                       (unsigned)(player_row + 1U),
                       record->player,
                       (unsigned)(((record->attrs[1] >> 4U) * 10U) + (record->attrs[1] & 0x0FU)),
                       (unsigned)(record->attrs[2] >> 4U),
                       (unsigned)(record->attrs[2] & 0x0FU));
        draw_text(fb, 36, 136 + (int)player_row * 22, line,
                  player_row == runtime->selected_player ? rgb(28, 28, 24) : rgb(220, 224, 218), 1);
        ++player_row;
    }
}

static void render_court(const TecmoRuntime *runtime, TecmoFramebuffer *fb)
{
    char line[256];
    const RosterRecord *player = selected_player_record(runtime);

    clear(fb, rgb(12, 58, 46));
    rect(fb, COURT_LEFT, COURT_TOP, COURT_RIGHT - COURT_LEFT, COURT_BOTTOM - COURT_TOP, rgb(197, 140, 78));
    rect(fb, COURT_LEFT + 6, COURT_TOP + 6, COURT_RIGHT - COURT_LEFT - 12, COURT_BOTTOM - COURT_TOP - 12, rgb(226, 170, 98));
    rect(fb, 314, COURT_TOP + 6, 4, COURT_BOTTOM - COURT_TOP - 12, rgb(245, 236, 210));
    rect(fb, 500, 92, 84, 72, rgb(178, 92, 68));
    rect(fb, 538, 116, 8, 28, rgb(245, 236, 210));
    rect(fb, 528, 120, 28, 5, rgb(245, 236, 210));

    rect(fb, (int)runtime->player_x - 8, (int)runtime->player_y - 18, 16, 36, rgb(42, 70, 164));
    rect(fb, (int)runtime->player_x - 6, (int)runtime->player_y - 26, 12, 10, rgb(228, 182, 134));
    rect(fb, (int)runtime->ball_x - 5, (int)runtime->ball_y - 5, 10, 10, rgb(214, 98, 36));

    (void)snprintf(line, sizeof(line), "SCORE %u", runtime->score * 2U);
    draw_text(fb, 24, 18, line, rgb(248, 248, 232), 2);
    if (player != 0) {
        (void)snprintf(line, sizeof(line), "%s - %s", runtime->teams[runtime->selected_team], player->player);
        draw_text(fb, 220, 20, line, rgb(248, 248, 232), 1);
    }
    draw_text(fb, 24, 452, "ARROWS MOVE   SPACE SHOOT   ESC TEAM SELECT", rgb(236, 232, 208), 1);
}

static void draw_debug_text(TecmoFramebuffer *fb, int x, int y, const char *text)
{
    draw_text(fb, x + 1, y + 1, text, rgb(0, 0, 0), 1);
    draw_text(fb, x, y, text, rgb(214, 250, 210), 1);
}

static void render_debug_overlay(const TecmoRuntime *runtime, TecmoFramebuffer *fb)
{
    char line[256];
    const TecmoGameMemory *memory = runtime->memory;
    const float fps = runtime->frame_seconds > 0.00001f ? 1.0f / runtime->frame_seconds : 0.0f;
    const int x = 10;
    const int y = 314;

    rect(fb, x - 6, y - 8, 412, 152, rgb(10, 14, 18));
    rect(fb, x - 4, y - 6, 408, 148, rgb(28, 38, 42));

    (void)snprintf(line, sizeof(line), "DBG MODE %s FRAME %u", mode_name(runtime->mode), runtime->frame_counter);
    draw_debug_text(fb, x, y, line);

    (void)snprintf(line, sizeof(line), "FPS %.1f MENU %u TEAM %u PLAYER %u CHR %02u/%02u T%u",
                   (double)fps,
                   (unsigned)runtime->selected_menu_item,
                   (unsigned)(runtime->selected_team + 1U),
                   (unsigned)(runtime->selected_player + 1U),
                   (unsigned)selected_chr_bank(runtime),
                   (unsigned)(chr_bank_count(runtime) - 1U),
                   (unsigned)selected_chr_table(runtime));
    draw_debug_text(fb, x, y + 20, line);

    (void)snprintf(line, sizeof(line), "PERM %llu OF %llu HI %llu",
                   (unsigned long long)memory->permanent.used,
                   (unsigned long long)memory->permanent.capacity,
                   (unsigned long long)memory->permanent.high_water);
    draw_debug_text(fb, x, y + 40, line);

    (void)snprintf(line, sizeof(line), "TRAN %llu OF %llu HI %llu",
                   (unsigned long long)memory->transient.used,
                   (unsigned long long)memory->transient.capacity,
                   (unsigned long long)memory->transient.high_water);
    draw_debug_text(fb, x, y + 60, line);

    (void)snprintf(line, sizeof(line), "RAM 0000: %02X %02X %02X %02X %02X %02X %02X %02X",
                   (unsigned)tecmo_cpu_ram_read(memory, 0x0000),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x0001),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x0002),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x0003),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x0004),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x0005),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x0006),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x0007));
    draw_debug_text(fb, x, y + 80, line);

    (void)snprintf(line, sizeof(line), "RAM 0008: %02X %02X %02X %02X %02X %02X %02X %02X",
                   (unsigned)tecmo_cpu_ram_read(memory, 0x0008),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x0009),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x000A),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x000B),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x000C),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x000D),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x000E),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x000F));
    draw_debug_text(fb, x, y + 100, line);

    draw_debug_text(fb, x, y + 124, "F3 TOGGLE DEBUG OVERLAY");
}

void tecmo_render_original_title_probe(TecmoFramebuffer *framebuffer, const char *title_text)
{
    const char *title = title_text != NULL && title_text[0] != '\0' ? title_text : "TITLE DATA MISSING";

    clear(framebuffer, rgb(8, 10, 24));
    rect(framebuffer, 0, 0, framebuffer->width, 46, rgb(18, 18, 34));
    rect(framebuffer, 0, framebuffer->height - 52, framebuffer->width, 52, rgb(18, 18, 34));
    rect(framebuffer, 52, 112, framebuffer->width - 104, 156, rgb(106, 18, 32));
    rect(framebuffer, 60, 120, framebuffer->width - 120, 140, rgb(20, 26, 54));
    rect(framebuffer, 72, 132, framebuffer->width - 144, 116, rgb(170, 42, 44));
    rect(framebuffer, 84, 144, framebuffer->width - 168, 92, rgb(28, 34, 66));

    draw_centered_text(framebuffer, 168, title, rgb(252, 236, 170), 3);
    draw_centered_text(framebuffer, 284, "SOURCE BACKED TITLE PROBE", rgb(142, 174, 190), 1);
    draw_centered_text(framebuffer, 306, "BANK 04 TITLE TABLE  RENDERER STEP 1", rgb(142, 174, 190), 1);

    rect(framebuffer, 132, 356, 376, 2, rgb(236, 214, 112));
    draw_centered_text(framebuffer, 376, "CHR PALETTE AND LAYOUT MAPPING NEXT", rgb(230, 232, 214), 1);
}

void tecmo_render_intro_c051_d861_model(TecmoFramebuffer *framebuffer)
{
    static const TecmoIntroSpriteRecord synthetic_records[] = {
        {0, 0x24U, 0x02U, 0},
        {8, 0x26U, 0x02U, 16},
        {-24, 0xFDU, 0x00U, 32},
    };
    const TecmoIntroSpriteStageConfig synthetic_config = {48, 64, 1U};
    TecmoIntroStagedSprite staged[sizeof(synthetic_records) / sizeof(synthetic_records[0])];
    const uint32_t panel = rgb(20, 24, 32);
    const uint32_t panel_dark = rgb(9, 11, 16);
    const uint32_t line = rgb(78, 98, 112);
    const uint32_t text = rgb(230, 232, 214);
    const uint32_t muted = rgb(142, 174, 190);
    const uint32_t accent = rgb(252, 236, 118);
    size_t staged_count;
    char row[128];
    char self_test_message[96];
    bool self_test_ok;

    self_test_ok = tecmo_intro_stage_self_test(self_test_message, sizeof(self_test_message));
    staged_count = tecmo_intro_stage_sprite_records(synthetic_records,
                                                    sizeof(synthetic_records) / sizeof(synthetic_records[0]),
                                                    &synthetic_config,
                                                    staged,
                                                    sizeof(staged) / sizeof(staged[0]));
    clear(framebuffer, rgb(6, 7, 10));
    rect(framebuffer, 0, 0, framebuffer->width, 54, rgb(18, 18, 34));
    draw_text(framebuffer, 24, 18, "C051 D861 INTRO SPRITE STAGING MODEL", text, 2);
    draw_text(framebuffer, 30, 64, "READ ONLY DIAGNOSTIC  SYNTHETIC RECORDS ONLY", muted, 1);
    draw_text(framebuffer, 30, 76, self_test_message, self_test_ok ? accent : rgb(232, 92, 76), 1);

    rect(framebuffer, 30, 92, 260, 134, panel);
    outline_rect(framebuffer, 30, 92, 260, 134, line);
    draw_text(framebuffer, 48, 110, "INPUT STREAM", accent, 1);
    draw_text(framebuffer, 48, 132, "COUNT BYTE SELECTS RECORD TOTAL", text, 1);
    draw_text(framebuffer, 48, 154, "EACH RECORD IS FOUR FIELDS", text, 1);
    draw_text(framebuffer, 70, 184, "Y    TILE    ATTR    X", muted, 1);
    rect(framebuffer, 66, 202, 34, 14, rgb(54, 65, 72));
    rect(framebuffer, 116, 202, 46, 14, rgb(54, 65, 72));
    rect(framebuffer, 178, 202, 42, 14, rgb(54, 65, 72));
    rect(framebuffer, 236, 202, 28, 14, rgb(54, 65, 72));

    rect(framebuffer, 350, 92, 260, 134, panel);
    outline_rect(framebuffer, 350, 92, 260, 134, line);
    draw_text(framebuffer, 368, 110, "OAM SHAPED OUTPUT", accent, 1);
    draw_text(framebuffer, 368, 132, "BASE IS 0200 PLUS N TIMES 4", text, 1);
    draw_text(framebuffer, 368, 154, "COUNTER IS 058D", text, 1);
    draw_text(framebuffer, 368, 184, "0200 YBASE PLUS Y", muted, 1);
    draw_text(framebuffer, 368, 202, "0201 TILE PLUS 0D", muted, 1);
    draw_text(framebuffer, 492, 184, "0202 ATTR", muted, 1);
    draw_text(framebuffer, 492, 202, "0203 XBASE PLUS X", muted, 1);

    rect(framebuffer, 292, 151, 54, 6, accent);
    rect(framebuffer, 336, 146, 10, 16, accent);

    rect(framebuffer, 42, 262, 556, 92, panel_dark);
    outline_rect(framebuffer, 42, 262, 556, 92, line);
    draw_text(framebuffer, 60, 280, "NATIVE MODEL STEPS", accent, 1);
    draw_text(framebuffer, 60, 304, "1  BANK04 CHOOSES POINTER INDEX AND BASE OFFSETS", text, 1);
    draw_text(framebuffer, 60, 324, "2  C051 ENTERS D861 AND STAGES FOUR BYTE RECORDS", text, 1);
    draw_text(framebuffer, 60, 344, "3  INTRO LAB MAPS OAM TILE LOWS TO TABLE ONE 8X16 PAIRS", text, 1);

    rect(framebuffer, 42, 382, 556, 56, panel_dark);
    outline_rect(framebuffer, 42, 382, 556, 56, line);
    draw_text(framebuffer, 60, 400, "CURRENT SAFE FACTS", accent, 1);
    draw_text(framebuffer, 60, 420, "RABBIT LOWS 25 27 29 2B  TECMO VISUAL CANDIDATE 180 TO 193", text, 1);
    draw_text(framebuffer, 60, 456, "NO ROM BYTES  NO ASM PAYLOADS  NO CHR DATA  NO LOCAL PATHS", muted, 1);

    for (size_t i = 0; i < staged_count; ++i) {
        (void)snprintf(row,
                       sizeof(row),
                       "S%u Y%02X T%02X A%02X X%02X",
                       (unsigned)i,
                       (unsigned)staged[i].y,
                       (unsigned)staged[i].tile,
                       (unsigned)staged[i].attributes,
                       (unsigned)staged[i].x);
        draw_text(framebuffer, 368, 232 + (int)i * 10, row, muted, 1);
    }
}

static void draw_chr_tile_ex(TecmoFramebuffer *fb,
                             const uint8_t *chr_bytes,
                             uint64_t chr_byte_count,
                             uint32_t chr_bank,
                             uint16_t tile,
                             int x,
                             int y,
                             int scale,
                             const uint32_t palette[4],
                             bool flip_horizontal,
                             bool flip_vertical)
{
    tecmo_draw_chr_tile_ex(fb,
                           chr_bytes,
                           chr_byte_count,
                           chr_bank,
                           tile,
                           x,
                           y,
                           scale,
                           palette,
                           flip_horizontal,
                           flip_vertical);
}

static void draw_chr_tile(TecmoFramebuffer *fb,
                          const uint8_t *chr_bytes,
                          uint64_t chr_byte_count,
                          uint32_t chr_bank,
                          uint16_t tile,
                          int x,
                          int y,
                          int scale)
{
    tecmo_draw_chr_tile(fb, chr_bytes, chr_byte_count, chr_bank, tile, x, y, scale);
}

bool tecmo_render_intro_presents_screen(const TecmoRuntime *runtime,
                                        TecmoFramebuffer *framebuffer)
{
    const int scale = 2;
    const int origin_x = 64;
    const int origin_y = 0;
    bool drew = false;

    clear(framebuffer, rgb(0, 0, 0));
    if (runtime != NULL) {
        drew = tecmo_intro_screen_draw(framebuffer,
                                       &runtime->intro_presents_asset,
                                       runtime->title_chr_bytes,
                                       runtime->title_chr_byte_count,
                                       runtime->mode_frame_counter,
                                       origin_x,
                                       origin_y,
                                       scale);
    }
    if (!drew) {
        draw_centered_text(framebuffer, 212, "NATIVE TECMO SCREEN UNAVAILABLE", rgb(252, 236, 170), 2);
    }
    return drew;
}

static bool render_intro_native_title_screen(const TecmoRuntime *runtime,
                                             TecmoFramebuffer *fb,
                                             const char *prompt,
                                             bool draw_debug)
{
    bool drew = tecmo_render_intro_presents_screen(runtime, fb);

    if (prompt != NULL && prompt[0] != '\0') {
        draw_text(fb, 22, 22, prompt, rgb(230, 232, 214), 1);
    }
    if (draw_debug) {
        char line[160];
        (void)snprintf(line,
                       sizeof(line),
                       "NATIVE TISC-1  STAGE %u  MODEFRAME %u  ROM-RESOLVED CHR",
                       runtime != NULL
                           ? (unsigned)tecmo_intro_screen_palette_stage(
                                 &runtime->intro_presents_asset,
                                 runtime->mode_frame_counter)
                           : 0U,
                       runtime != NULL ? runtime->mode_frame_counter : 0U);
        draw_text(fb, 22, 446, line, rgb(92, 116, 128), 1);
    }
    return drew;
}

bool tecmo_render_intro_license_screen(const TecmoRuntime *runtime, TecmoFramebuffer *framebuffer)
{
    const int scale = 2;
    const int origin_x = 64;
    const int origin_y = 0;
    bool drew = false;

    clear(framebuffer, rgb(0, 0, 0));
    if (runtime != NULL) {
        drew = tecmo_intro_screen_draw(framebuffer,
                                       &runtime->intro_license_asset,
                                       runtime->title_chr_bytes,
                                       runtime->title_chr_byte_count,
                                       runtime->mode_frame_counter,
                                       origin_x,
                                       origin_y,
                                       scale);
    }
    if (!drew) {
        draw_centered_text(framebuffer, 212, "NATIVE NBA SCREEN UNAVAILABLE", rgb(252, 236, 170), 2);
    }
    return drew;
}

static void draw_title_glyph(TecmoFramebuffer *fb,
                             const uint8_t *chr_bytes,
                             uint64_t chr_byte_count,
                             uint32_t chr_bank,
                             uint32_t chr_table,
                             const TecmoTitleGlyph *glyph,
                             int x,
                             int y,
                             int scale)
{
    uint16_t base = (uint16_t)((chr_table & 1U) * 0x100U);
    if (glyph->glyph_tiles[0] != 0xFFU) {
        draw_chr_tile(fb, chr_bytes, chr_byte_count, chr_bank, (uint16_t)(base + glyph->glyph_tiles[0]), x, y, scale);
    }
    if (glyph->glyph_tiles[1] != 0xFFU) {
        draw_chr_tile(fb, chr_bytes, chr_byte_count, chr_bank, (uint16_t)(base + glyph->glyph_tiles[1]), x + 8 * scale, y, scale);
    }
    if (glyph->glyph_tiles[2] != 0xFFU) {
        draw_chr_tile(fb, chr_bytes, chr_byte_count, chr_bank, (uint16_t)(base + glyph->glyph_tiles[2]), x, y + 8 * scale, scale);
    }
    if (glyph->glyph_tiles[3] != 0xFFU) {
        draw_chr_tile(fb, chr_bytes, chr_byte_count, chr_bank, (uint16_t)(base + glyph->glyph_tiles[3]), x + 8 * scale, y + 8 * scale, scale);
    }
}

static void draw_title_glyph_palette(TecmoFramebuffer *fb,
                                     const uint8_t *chr_bytes,
                                     uint64_t chr_byte_count,
                                     uint32_t chr_bank,
                                     uint32_t chr_table,
                                     const TecmoTitleGlyph *glyph,
                                     int x,
                                     int y,
                                     int scale,
                                     const uint32_t palette[4])
{
    uint16_t base = (uint16_t)((chr_table & 1U) * 0x100U);
    if (glyph->glyph_tiles[0] != 0xFFU) {
        draw_chr_tile_ex(fb, chr_bytes, chr_byte_count, chr_bank, (uint16_t)(base + glyph->glyph_tiles[0]), x, y, scale, palette, false, false);
    }
    if (glyph->glyph_tiles[1] != 0xFFU) {
        draw_chr_tile_ex(fb, chr_bytes, chr_byte_count, chr_bank, (uint16_t)(base + glyph->glyph_tiles[1]), x + 8 * scale, y, scale, palette, false, false);
    }
    if (glyph->glyph_tiles[2] != 0xFFU) {
        draw_chr_tile_ex(fb, chr_bytes, chr_byte_count, chr_bank, (uint16_t)(base + glyph->glyph_tiles[2]), x, y + 8 * scale, scale, palette, false, false);
    }
    if (glyph->glyph_tiles[3] != 0xFFU) {
        draw_chr_tile_ex(fb, chr_bytes, chr_byte_count, chr_bank, (uint16_t)(base + glyph->glyph_tiles[3]), x + 8 * scale, y + 8 * scale, scale, palette, false, false);
    }
}

static void draw_title_glyph_range(TecmoFramebuffer *fb,
                                   const uint8_t *chr_bytes,
                                   uint64_t chr_byte_count,
                                   uint32_t chr_bank,
                                   uint32_t chr_table,
                                   const TecmoOriginalTitleGlyphs *glyphs,
                                   size_t first,
                                   size_t count,
                                   int x,
                                   int y,
                                   int scale)
{
    int glyph_width = 16 * scale;
    for (size_t i = 0; i < count && first + i < glyphs->glyph_count; ++i) {
        const TecmoTitleGlyph *glyph = &glyphs->glyphs[first + i];
        if (glyph->character != ' ') {
            draw_title_glyph(fb,
                             chr_bytes,
                             chr_byte_count,
                             chr_bank,
                             chr_table,
                             glyph,
                             x + (int)i * glyph_width,
                             y,
                             scale);
        }
    }
}

static void draw_title_glyph_range_palette(TecmoFramebuffer *fb,
                                           const uint8_t *chr_bytes,
                                           uint64_t chr_byte_count,
                                           uint32_t chr_bank,
                                           uint32_t chr_table,
                                           const TecmoOriginalTitleGlyphs *glyphs,
                                           size_t first,
                                           size_t count,
                                           int x,
                                           int y,
                                           int scale,
                                           const uint32_t palette[4])
{
    int glyph_width = 16 * scale;
    for (size_t i = 0; i < count && first + i < glyphs->glyph_count; ++i) {
        const TecmoTitleGlyph *glyph = &glyphs->glyphs[first + i];
        if (glyph->character != ' ') {
            draw_title_glyph_palette(fb,
                                     chr_bytes,
                                     chr_byte_count,
                                     chr_bank,
                                     chr_table,
                                     glyph,
                                     x + (int)i * glyph_width,
                                     y,
                                     scale,
                                     palette);
        }
    }
}

static bool get_intro_trace_bounds(const TecmoRuntime *runtime,
                                   uint8_t group,
                                   int *min_x,
                                   int *min_y,
                                   int *max_x,
                                   int *max_y)
{
    bool found = false;
    int local_min_x = 0;
    int local_min_y = 0;
    int local_max_x = 0;
    int local_max_y = 0;

    for (size_t i = 0; i < runtime->intro_trace_sprite_count; ++i) {
        const TecmoIntroTraceSprite *sprite = &runtime->intro_trace_sprites[i];
        if (sprite->group != group) {
            continue;
        }
        if (!found) {
            local_min_x = sprite->screen_x;
            local_min_y = sprite->screen_y;
            local_max_x = sprite->screen_x + 8;
            local_max_y = sprite->screen_y + 16;
            found = true;
        } else {
            if (sprite->screen_x < local_min_x) {
                local_min_x = sprite->screen_x;
            }
            if (sprite->screen_y < local_min_y) {
                local_min_y = sprite->screen_y;
            }
            if (sprite->screen_x + 8 > local_max_x) {
                local_max_x = sprite->screen_x + 8;
            }
            if (sprite->screen_y + 16 > local_max_y) {
                local_max_y = sprite->screen_y + 16;
            }
        }
    }

    if (found) {
        *min_x = local_min_x;
        *min_y = local_min_y;
        *max_x = local_max_x;
        *max_y = local_max_y;
    }
    return found;
}

static void draw_intro_trace_sprite_with_chr_bank(TecmoFramebuffer *fb,
                                                  const TecmoRuntime *runtime,
                                                  const TecmoIntroTraceSprite *sprite,
                                                  uint32_t chr_bank,
                                                  int x,
                                                  int y,
                                                  int scale)
{
    static const uint32_t palettes[4][4] = {
        {0x00000000U, 0xFF5A6274U, 0xFFC4CCD8U, 0xFFFFFCF0U},
        {0x00000000U, 0xFF8A4E58U, 0xFFE06A84U, 0xFFFFF0DAU},
        {0x00000000U, 0xFF8C2638U, 0xFFFF1F72U, 0xFFFFFFFFU},
        {0x00000000U, 0xFF84452EU, 0xFFFF8A4EU, 0xFFFFF0C8U},
    };
    uint8_t palette_index = (uint8_t)(sprite->attributes & 3U);
    bool flip_horizontal = (sprite->attributes & 0x40U) != 0U;
    bool flip_vertical = (sprite->attributes & 0x80U) != 0U;
    uint16_t top_tile = (uint16_t)(0x100U + (uint16_t)(sprite->tile_low & 0xFEU));
    uint16_t bottom_tile = (uint16_t)(top_tile + 1U);
    uint16_t first_tile = flip_vertical ? bottom_tile : top_tile;
    uint16_t second_tile = flip_vertical ? top_tile : bottom_tile;

    draw_chr_tile_ex(fb,
                     runtime->title_chr_bytes,
                     runtime->title_chr_byte_count,
                     chr_bank,
                     first_tile,
                     x,
                     y,
                     scale,
                     palettes[palette_index],
                     flip_horizontal,
                     flip_vertical);
    draw_chr_tile_ex(fb,
                     runtime->title_chr_bytes,
                     runtime->title_chr_byte_count,
                     chr_bank,
                     second_tile,
                     x,
                     y + 8 * scale,
                     scale,
                     palettes[palette_index],
                     flip_horizontal,
                     flip_vertical);
}

static void draw_intro_trace_sprite(TecmoFramebuffer *fb,
                                    const TecmoRuntime *runtime,
                                    const TecmoIntroTraceSprite *sprite,
                                    int x,
                                    int y,
                                    int scale)
{
    draw_intro_trace_sprite_with_chr_bank(fb, runtime, sprite, runtime->intro_trace_chr_bank, x, y, scale);
}

static void draw_intro_trace_group(TecmoFramebuffer *fb,
                                   const TecmoRuntime *runtime,
                                   uint8_t group,
                                   int target_x,
                                   int target_y,
                                   int scale)
{
    int min_x;
    int min_y;
    int max_x;
    int max_y;

    if (!get_intro_trace_bounds(runtime, group, &min_x, &min_y, &max_x, &max_y)) {
        return;
    }
    (void)max_x;
    (void)max_y;

    for (size_t i = 0; i < runtime->intro_trace_sprite_count; ++i) {
        const TecmoIntroTraceSprite *sprite = &runtime->intro_trace_sprites[i];
        int x;
        int y;
        if (sprite->group != group) {
            continue;
        }
        x = target_x + (sprite->screen_x - min_x) * scale;
        y = target_y + (sprite->screen_y - min_y) * scale;
        draw_intro_trace_sprite(fb, runtime, sprite, x, y, scale);
    }
}

static void draw_intro_trace_group_scaled_box(TecmoFramebuffer *fb,
                                              const TecmoRuntime *runtime,
                                              uint8_t group,
                                              int target_x,
                                              int target_y,
                                              int scale,
                                              uint32_t border)
{
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int width;
    int height;

    if (!get_intro_trace_bounds(runtime, group, &min_x, &min_y, &max_x, &max_y)) {
        draw_text(fb, target_x, target_y, "NO RECORDS PARSED", rgb(232, 92, 76), 1);
        return;
    }

    width = (max_x - min_x) * scale;
    height = (max_y - min_y) * scale;
    outline_rect(fb, target_x - 2, target_y - 2, width + 4, height + 4, border);
    draw_intro_trace_group(fb, runtime, group, target_x, target_y, scale);
}

static const TecmoIntroTraceSprite *find_intro_trace_sprite(const TecmoRuntime *runtime,
                                                            uint8_t group,
                                                            bool require_visible,
                                                            size_t *group_index_out)
{
    size_t group_index = 0;
    for (size_t i = 0; i < runtime->intro_trace_sprite_count; ++i) {
        const TecmoIntroTraceSprite *sprite = &runtime->intro_trace_sprites[i];
        if (sprite->group != group) {
            continue;
        }
        if (!require_visible || (sprite->screen_y >= 0 && sprite->screen_y < 240)) {
            if (group_index_out != NULL) {
                *group_index_out = group_index;
            }
            return sprite;
        }
        ++group_index;
    }
    if (group_index_out != NULL) {
        *group_index_out = 0;
    }
    return NULL;
}

static void describe_intro_trace_sprite(const TecmoIntroTraceSprite *sprite,
                                        size_t record_index,
                                        const char *prefix,
                                        char *out,
                                        size_t out_size)
{
    uint8_t oam_y;
    uint8_t oam_x;
    uint16_t pair_top;
    uint16_t pair_bottom;
    if (out == NULL || out_size == 0) {
        return;
    }
    if (sprite == NULL) {
        (void)snprintf(out, out_size, "%s MISSING", prefix);
        return;
    }

    oam_y = (uint8_t)sprite->screen_y;
    oam_x = (uint8_t)sprite->screen_x;
    pair_top = tecmo_intro_oam_tile_pair_top(sprite->tile_low, 1U);
    pair_bottom = tecmo_intro_oam_tile_pair_bottom(sprite->tile_low, 1U);
    (void)snprintf(out,
                   out_size,
                   "%s R%02u OAM Y%02X T%02X A%02X X%02X  CHR %03X/%03X",
                   prefix,
                   (unsigned)record_index,
                   (unsigned)oam_y,
                   (unsigned)sprite->tile_low,
                   (unsigned)sprite->attributes,
                   (unsigned)oam_x,
                   (unsigned)pair_top,
                   (unsigned)pair_bottom);
}

static void describe_intro_trace_sprite_short(const TecmoIntroTraceSprite *sprite,
                                              size_t record_index,
                                              const char *prefix,
                                              char *out,
                                              size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    if (sprite == NULL) {
        (void)snprintf(out, out_size, "%s MISSING", prefix);
        return;
    }
    (void)snprintf(out,
                   out_size,
                   "%s R%02u Y%02X T%02X A%02X X%02X",
                   prefix,
                   (unsigned)record_index,
                   (unsigned)((uint8_t)sprite->screen_y),
                   (unsigned)sprite->tile_low,
                   (unsigned)sprite->attributes,
                   (unsigned)((uint8_t)sprite->screen_x));
}

static void draw_intro_trace_byte_table(const TecmoRuntime *runtime,
                                        TecmoFramebuffer *fb,
                                        int x,
                                        int y)
{
    const size_t rows_per_column = 8;
    size_t record_index = 0;
    char line[160];

    draw_text(fb, x, y, "FULL D861 RABBIT STREAM  16 RECORDS  64 BYTES", rgb(252, 236, 118), 1);
    draw_text(fb, x, y + 18, "REC  Y TILE AT X  CHR    V", rgb(142, 174, 190), 1);
    draw_text(fb, x + 294, y + 18, "REC  Y TILE AT X  CHR    V", rgb(142, 174, 190), 1);

    for (size_t i = 0; i < runtime->intro_trace_sprite_count && record_index < 16U; ++i) {
        const TecmoIntroTraceSprite *sprite = &runtime->intro_trace_sprites[i];
        uint8_t oam_y;
        uint8_t oam_x;
        uint16_t pair_top;
        uint16_t pair_bottom;
        bool visible_y;
        int column_x;
        int row_y;

        if (sprite->group != INTRO_TRACE_GROUP_RABBIT) {
            continue;
        }

        oam_y = (uint8_t)sprite->screen_y;
        oam_x = (uint8_t)sprite->screen_x;
        pair_top = tecmo_intro_oam_tile_pair_top(sprite->tile_low, 1U);
        pair_bottom = tecmo_intro_oam_tile_pair_bottom(sprite->tile_low, 1U);
        visible_y = sprite->screen_y >= 0 && sprite->screen_y < 240;
        column_x = x + (record_index >= rows_per_column ? 294 : 0);
        row_y = y + 34 + (int)(record_index % rows_per_column) * 11;
        (void)snprintf(line,
                       sizeof(line),
                       "R%02u %02X  %02X  %02X %02X %03X/%03X %c",
                       (unsigned)record_index,
                       (unsigned)oam_y,
                       (unsigned)sprite->tile_low,
                       (unsigned)sprite->attributes,
                       (unsigned)oam_x,
                       (unsigned)pair_top,
                       (unsigned)pair_bottom,
                       visible_y ? 'Y' : 'N');
        draw_text(fb, column_x, row_y, line, rgb(230, 232, 214), 1);
        ++record_index;
    }

    if (record_index == 0) {
        draw_text(fb, x, y + 36, "NO RABBIT STREAM ROWS PARSED", rgb(232, 92, 76), 1);
    } else if (record_index < 16U) {
        (void)snprintf(line, sizeof(line), "ONLY %u RABBIT ROWS PARSED", (unsigned)record_index);
        draw_text(fb, x + 294, y + 122, line, rgb(232, 92, 76), 1);
    }
}

void tecmo_render_first_sprite_probe(const TecmoRuntime *runtime, TecmoFramebuffer *fb)
{
    const TecmoIntroTraceSprite *first_staged;
    const TecmoIntroTraceSprite *first_visible;
    size_t first_staged_index = 0;
    size_t first_visible_index = 0;
    char line[192];

    clear(fb, rgb(0, 0, 0));
    rect(fb, 0, 0, fb->width, 58, rgb(18, 18, 34));
    draw_text(fb, 24, 20, "PLAY GAME - FIRST INTRO SPRITE PROBE", rgb(248, 248, 232), 2);
    draw_text(fb, 24, 66, "BANK07 RESET -> CC30 -> E41A -> BANK04 825D -> 88E8 -> 8988 -> C051/D861", rgb(142, 174, 190), 1);
    draw_text(fb, 24, 84, "ENTER OR ESC RETURNS TO MENU", rgb(142, 174, 190), 1);

    if (!runtime->title_probe_available || !runtime->intro_trace_available) {
        draw_centered_text(fb, 206, "LOCAL INTRO TRACE OR CHR DATA MISSING", rgb(252, 236, 170), 2);
        draw_centered_text(fb, 242, "RUN TOOLS FIND-INTROCOMPOSITETRACE FIRST", rgb(230, 232, 214), 1);
        draw_centered_text(fb, 264, runtime->intro_trace_status, rgb(142, 174, 190), 1);
        return;
    }

    first_staged = find_intro_trace_sprite(runtime, INTRO_TRACE_GROUP_RABBIT, false, &first_staged_index);
    first_visible = find_intro_trace_sprite(runtime, INTRO_TRACE_GROUP_RABBIT, true, &first_visible_index);

    draw_text(fb, 24, 116, "FIRST D861 OUTPUT RECORD", rgb(252, 236, 118), 1);
    describe_intro_trace_sprite(first_staged, first_staged_index, "STAGED", line, sizeof(line));
    draw_text(fb, 24, 136, line, rgb(230, 232, 214), 1);
    draw_text(fb, 24, 154, "OFFSCREEN IN ORIGINAL  Y WRAPS TO FE", rgb(142, 174, 190), 1);
    if (first_staged != NULL) {
        outline_rect(fb, 72, 188, 8 * 7 + 2, 16 * 7 + 2, rgb(80, 96, 110));
        draw_intro_trace_sprite(fb, runtime, first_staged, 73, 189, 7);
    }

    draw_text(fb, 326, 116, "FIRST VISIBLE RABBIT RECORD", rgb(252, 236, 118), 1);
    describe_intro_trace_sprite(first_visible, first_visible_index, "VISIBLE", line, sizeof(line));
    draw_text(fb, 326, 136, line, rgb(230, 232, 214), 1);
    draw_text(fb, 326, 154, "FIRST RABBIT SPRITE WITH ONSCREEN Y", rgb(142, 174, 190), 1);
    if (first_visible != NULL) {
        outline_rect(fb, 386, 188, 8 * 7 + 2, 16 * 7 + 2, rgb(80, 96, 110));
        draw_intro_trace_sprite(fb, runtime, first_visible, 387, 189, 7);
    }

    rect(fb, 24, 318, 592, 138, rgb(12, 14, 18));
    outline_rect(fb, 24, 318, 592, 138, rgb(66, 78, 88));
    draw_intro_trace_byte_table(runtime, fb, 42, 336);

    draw_text(fb, 24, 464, runtime->intro_trace_status, rgb(92, 116, 128), 1);
}

static void draw_l88e7_seed_panel(const TecmoRuntime *runtime, TecmoFramebuffer *fb)
{
    char line[160];
    rect(fb, 24, 62, 592, 96, rgb(12, 14, 18));
    outline_rect(fb, 24, 62, 592, 96, rgb(66, 78, 88));
    draw_text(fb, 40, 78, "BANK04 L88E7 SEEDS", rgb(252, 236, 118), 1);
    draw_text(fb, 40, 96, "$88=A8  $8A=3C  $0352=01  $0100=05  CHR SLOTS $57=08 $58=09", rgb(230, 232, 214), 1);
    draw_text(fb, 40, 112, "C05A -> D700 LOADS PALETTE SNAPSHOT $89DD, NOT SPRITE PLACEMENT", rgb(142, 174, 190), 1);
    (void)snprintf(line,
                   sizeof(line),
                   "$0100=05 -> IRQ %s   PRESENTS DATA -> %s",
                   runtime->intro_l88e7_irq_vector_available ? runtime->intro_l88e7_irq_vector : "UNRESOLVED",
                   runtime->intro_presents_data_available ? runtime->intro_presents_data_cpu : "UNRESOLVED");
    draw_text(fb, 40, 130, line, rgb(142, 174, 190), 1);

    if (runtime->intro_l88e7_palette_available) {
        for (size_t row = 0; row < 4U; ++row) {
            (void)snprintf(line,
                           sizeof(line),
                           "%02X %02X %02X %02X",
                           (unsigned)runtime->intro_l88e7_palette[row * 4U + 0U],
                           (unsigned)runtime->intro_l88e7_palette[row * 4U + 1U],
                           (unsigned)runtime->intro_l88e7_palette[row * 4U + 2U],
                           (unsigned)runtime->intro_l88e7_palette[row * 4U + 3U]);
            draw_text(fb, 410, 78 + (int)row * 15, line, rgb(230, 232, 214), 1);
        }
    } else {
        draw_text(fb, 410, 96, "PALETTE BYTES NEED TRACE REGEN", rgb(232, 92, 76), 1);
    }
}

static void draw_l88e7_group_summary(const TecmoRuntime *runtime,
                                     TecmoFramebuffer *fb,
                                     uint8_t group,
                                     const char *prefix,
                                     int x,
                                     int y)
{
    const TecmoIntroTraceSprite *first_staged;
    const TecmoIntroTraceSprite *first_visible;
    size_t first_staged_index = 0;
    size_t first_visible_index = 0;
    char line[192];

    first_staged = find_intro_trace_sprite(runtime, group, false, &first_staged_index);
    first_visible = find_intro_trace_sprite(runtime, group, true, &first_visible_index);
    draw_text(fb, x, y, prefix, rgb(252, 236, 118), 1);
    describe_intro_trace_sprite_short(first_staged, first_staged_index, "FIRST", line, sizeof(line));
    draw_text(fb, x, y + 16, line, rgb(230, 232, 214), 1);
    describe_intro_trace_sprite_short(first_visible, first_visible_index, "VISIBLE", line, sizeof(line));
    draw_text(fb, x, y + 32, line, rgb(142, 174, 190), 1);
}

void tecmo_render_intro_l88e7_proof(const TecmoRuntime *runtime, TecmoFramebuffer *fb)
{
    char line[192];
    size_t selector0_count;
    size_t selector1_count;

    clear(fb, rgb(0, 0, 0));
    rect(fb, 0, 0, fb->width, 52, rgb(18, 18, 34));
    draw_text(fb, 24, 18, "BANK04 L88E7 FIRST INTRO PROOF", rgb(248, 248, 232), 2);

    if (!runtime->title_probe_available || !runtime->intro_trace_available) {
        draw_centered_text(fb, 206, "LOCAL INTRO TRACE OR CHR DATA MISSING", rgb(252, 236, 170), 2);
        draw_centered_text(fb, 242, "RUN TOOLS FIND-INTROCOMPOSITETRACE FIRST", rgb(230, 232, 214), 1);
        draw_centered_text(fb, 264, runtime->intro_trace_status, rgb(142, 174, 190), 1);
        return;
    }

    selector0_count = tecmo_intro_trace_group_count(runtime, INTRO_TRACE_GROUP_A7DB_SELECTOR0);
    selector1_count = tecmo_intro_trace_group_count(runtime, INTRO_TRACE_GROUP_RABBIT);
    draw_l88e7_seed_panel(runtime, fb);

    rect(fb, 24, 164, 276, 284, rgb(12, 14, 18));
    outline_rect(fb, 24, 164, 276, 284, rgb(66, 78, 88));
    (void)snprintf(line,
                   sizeof(line),
                   "STREAM0 A7DB SEL00  BASE X00 Y0000  %02u REC",
                   (unsigned)selector0_count);
    draw_text(fb, 38, 180, line, rgb(252, 236, 118), 1);
    draw_intro_trace_group_scaled_box(fb, runtime, INTRO_TRACE_GROUP_A7DB_SELECTOR0, 44, 188, 1, rgb(100, 118, 132));
    draw_l88e7_group_summary(runtime, fb, INTRO_TRACE_GROUP_A7DB_SELECTOR0, "SELECTOR 0 OAM SUMMARY", 112, 214);

    rect(fb, 324, 164, 292, 284, rgb(12, 14, 18));
    outline_rect(fb, 324, 164, 292, 284, rgb(66, 78, 88));
    (void)snprintf(line,
                   sizeof(line),
                   "STREAM1 A7DB SEL01  BASE XB8 Y011E  %02u REC",
                   (unsigned)selector1_count);
    draw_text(fb, 338, 180, line, rgb(252, 236, 118), 1);
    draw_intro_trace_group_scaled_box(fb, runtime, INTRO_TRACE_GROUP_RABBIT, 382, 214, 3, rgb(100, 118, 132));
    draw_l88e7_group_summary(runtime, fb, INTRO_TRACE_GROUP_RABBIT, "SELECTOR 1 RABBIT SUMMARY", 338, 406);

    (void)snprintf(line,
                   sizeof(line),
                   "NEXT: DECODE BANK00 STREAM INTERPRETER THAT PLACES PRESENTS FROM %s",
                   runtime->intro_presents_data_available ? runtime->intro_presents_data_cpu : "DATA LEAD");
    draw_text(fb, 24, 456, line, rgb(92, 116, 128), 1);
}

static void draw_intro_visual_tecmo_logo(TecmoFramebuffer *fb,
                                         const TecmoRuntime *runtime,
                                         int x,
                                         int y,
                                         int scale)
{
    static const uint32_t logo_palette[4] = {
        0x00000000U,
        0xFFFF1F72U,
        0xFFFF1F72U,
        0xFFFFFFFFU,
    };

    for (uint16_t letter = 0; letter < 5U; ++letter) {
        uint16_t tile_base = (uint16_t)(0x180U + letter * 4U);
        int letter_x = x + (int)letter * 16 * scale;
        for (uint16_t tile = 0; tile < 4U; ++tile) {
            int tile_x = letter_x + (int)(tile & 1U) * 8 * scale;
            int tile_y = y + (int)(tile >> 1U) * 8 * scale;
            draw_chr_tile_ex(fb,
                             runtime->title_chr_bytes,
                             runtime->title_chr_byte_count,
                             runtime->intro_trace_chr_bank,
                             (uint16_t)(tile_base + tile),
                             tile_x,
                             tile_y,
                             scale,
                             logo_palette,
                             false,
                             false);
        }
    }
}

static void draw_intro_presents_text(TecmoFramebuffer *fb,
                                     const TecmoRuntime *runtime,
                                     int x,
                                     int y,
                                     int scale)
{
    static const uint32_t presents_palette[4] = {
        0x00000000U,
        0xFF6E7480U,
        0xFFFFFFFFU,
        0xFFFFFFFFU,
    };
    const char *text = "PRESENTS";

    for (size_t i = 0; text[i] != '\0'; ++i) {
        uint16_t tile = 0x080U;
        char c = text[i];
        if (c >= '0' && c <= '9') {
            tile = (uint16_t)(0x082U + (uint16_t)(c - '0'));
        } else if (c >= 'A' && c <= 'D') {
            tile = (uint16_t)(0x08CU + (uint16_t)(c - 'A'));
        } else if (c >= 'E' && c <= 'T') {
            tile = (uint16_t)(0x090U + (uint16_t)(c - 'E'));
        } else if (c >= 'U' && c <= 'Z') {
            tile = (uint16_t)(0x0A0U + (uint16_t)(c - 'U'));
        }
        draw_chr_tile_ex(fb,
                         runtime->title_chr_bytes,
                         runtime->title_chr_byte_count,
                         runtime->intro_trace_chr_bank,
                         tile,
                         x + (int)i * 8 * scale,
                         y,
                         scale,
                         presents_palette,
                         false,
                         false);
    }
}

static const char *intro_output_step_label(uint8_t step)
{
    if (step == 0U) {
        return "FIRST D861 RECORD";
    }
    if (step == 1U) {
        return "FIRST VISIBLE RABBIT RECORD";
    }
    if (step == 2U) {
        return "FULL A7DB SELECTOR 1 RABBIT STREAM";
    }
    if (step == 3U) {
        return "L88E7 A7DB SELECTOR STREAMS";
    }
    if (step == 4U) {
        return "SCREEN 00 PRESENTS NAMETABLE";
    }
    if (step == 5U) {
        return "COORDINATE AUDIT";
    }
    if (step == TECMO_INTRO_OUTPUT_TITLE_STEP) {
        return "ROM-NATIVE TECMO AND RABBIT";
    }
    if (step == TECMO_INTRO_OUTPUT_LICENSE_STEP) {
        return "NBA LICENSE SCREEN";
    }
    if (step == TECMO_INTRO_OUTPUT_ARENA_STEP) {
        return "BANK04 ARENA TRANSITION TRACE";
    }
    if (step == TECMO_INTRO_OUTPUT_READY_STEP) {
        return "ROM READY REVEAL";
    }
    if (step == TECMO_INTRO_OUTPUT_WARRIORS_STEP) return "ROM WARRIORS TRANSITION";
    if (step == TECMO_INTRO_OUTPUT_CLIPPERS_STEP) return "ROM CLIPPERS TRANSITION";
    if (step == TECMO_INTRO_OUTPUT_BUCKS_STEP) return "ROM BUCKS TRANSITION";
    if (step == TECMO_INTRO_OUTPUT_PASS_STEP) return "ROM PASS TRANSITION";
    return "ROM INTRO FINALE AND TITLE";
}

static void draw_intro_output_header(TecmoFramebuffer *fb, uint8_t step)
{
    char line[128];
    rect(fb, 0, 0, fb->width, 58, rgb(18, 18, 34));
    (void)snprintf(line,
                   sizeof(line),
                   "PLAY GAME - FIRST INTRO OUTPUT %u OF %u",
                   (unsigned)step + 1U,
                   (unsigned)TECMO_INTRO_OUTPUT_STEP_COUNT);
    draw_text(fb, 24, 18, line, rgb(248, 248, 232), 2);
    draw_text(fb, 24, 66, "LEFT RIGHT STEP   ENTER ESC MENU", rgb(142, 174, 190), 1);
    draw_text(fb, 24, 82, "SOURCE ORDER VIEW - NOT THE FINAL COMPOSITE TITLE SCREEN", rgb(142, 174, 190), 1);
    draw_text(fb, 24, 106, intro_output_step_label(step), rgb(252, 236, 118), 1);
}

static bool draw_intro_trace_group_centered(TecmoFramebuffer *fb,
                                            const TecmoRuntime *runtime,
                                            uint8_t group,
                                            int center_x,
                                            int center_y,
                                            int scale,
                                            uint32_t border)
{
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int width;
    int height;
    int target_x;
    int target_y;

    if (!get_intro_trace_bounds(runtime, group, &min_x, &min_y, &max_x, &max_y)) {
        return false;
    }

    width = (max_x - min_x) * scale;
    height = (max_y - min_y) * scale;
    target_x = center_x - width / 2;
    target_y = center_y - height / 2;
    outline_rect(fb, target_x - 3, target_y - 3, width + 6, height + 6, border);
    draw_intro_trace_group(fb, runtime, group, target_x, target_y, scale);
    return true;
}

static void draw_intro_trace_group_absolute(TecmoFramebuffer *fb,
                                            const TecmoRuntime *runtime,
                                            uint8_t group,
                                            int origin_x,
                                            int origin_y,
                                            int scale)
{
    for (size_t i = 0; i < runtime->intro_trace_sprite_count; ++i) {
        const TecmoIntroTraceSprite *sprite = &runtime->intro_trace_sprites[i];
        int x;
        int y;
        if (sprite->group != group) {
            continue;
        }
        x = origin_x + sprite->screen_x * scale;
        y = origin_y + sprite->screen_y * scale;
        draw_intro_trace_sprite(fb, runtime, sprite, x, y, scale);
    }
}

static size_t draw_intro_trace_group_oam_base(TecmoFramebuffer *fb,
                                              const TecmoRuntime *runtime,
                                              uint8_t group,
                                              int base_x,
                                              uint8_t base_y,
                                              uint32_t chr_bank,
                                              int origin_x,
                                              int origin_y,
                                              int scale)
{
    size_t visible_count = 0;

    for (size_t i = 0; i < runtime->intro_trace_sprite_count; ++i) {
        const TecmoIntroTraceSprite *sprite = &runtime->intro_trace_sprites[i];
        uint8_t oam_x;
        uint8_t oam_y;
        int x;
        int y;

        if (sprite->group != group) {
            continue;
        }

        oam_x = (uint8_t)(base_x + sprite->screen_x);
        oam_y = (uint8_t)((int)base_y + sprite->screen_y);
        if (oam_y >= 240U) {
            continue;
        }

        x = origin_x + (int)oam_x * scale;
        y = origin_y + (int)oam_y * scale;
        draw_intro_trace_sprite_with_chr_bank(fb, runtime, sprite, chr_bank, x, y, scale);
        ++visible_count;
    }

    return visible_count;
}

bool tecmo_render_intro_arena_transition(const TecmoRuntime *runtime, TecmoFramebuffer *fb)
{
    const int scale = 2;
    const int viewport_x = 64;
    const int viewport_y = 0;
    const int viewport_w = 256 * scale;
    const int viewport_h = 240 * scale;
    TecmoIntroArenaTransitionState state;
    unsigned native_frame = runtime != NULL ? runtime->mode_frame_counter : 240U;
    unsigned frame = native_frame;
    int background_y;
    TecmoArenaNativeSpriteVisibleCounts visible_counts = {0U, 0U};
    bool drew_arena = false;
    bool drew_native_arena = false;
    bool native_layer_ready = false;
    bool native_sprite_groups_ready = false;
    bool show_debug = runtime == NULL || runtime->debug_overlay;
    char line[192];

    tecmo_intro_arena_transition_state(frame, &state);
    background_y = viewport_y - (int)state.scroll_y_0301 * scale;
    clear(fb, rgb(0, 0, 0));
    rect(fb, viewport_x, viewport_y, viewport_w, viewport_h, rgb(0, 0, 0));

    if (runtime != NULL && runtime->title_chr_bytes != NULL &&
        runtime->title_chr_byte_count != 0U) {
        native_layer_ready = tecmo_intro_arena_tile_layer_chr_available(
            &runtime->intro_arena_tile_layer,
            runtime->title_chr_bytes,
            runtime->title_chr_byte_count);
        native_sprite_groups_ready = tecmo_intro_arena_native_sprite_chr_available(
            &runtime->intro_arena_sprite_groups,
            runtime->title_chr_bytes,
            runtime->title_chr_byte_count);
        if (native_layer_ready && native_sprite_groups_ready) {
            drew_native_arena = tecmo_intro_arena_draw_native_chr(
                fb,
                &runtime->intro_arena_tile_layer,
                runtime->title_chr_bytes,
                runtime->title_chr_byte_count,
                frame,
                viewport_x,
                background_y,
                scale);
            if (drew_native_arena) {
                visible_counts = tecmo_intro_arena_draw_native_sprite_groups(
                    fb,
                    &runtime->intro_arena_sprite_groups,
                    runtime->title_chr_bytes,
                    runtime->title_chr_byte_count,
                    &state,
                    viewport_x,
                    viewport_y,
                    scale);
            }
            drew_arena = drew_native_arena;
        }
    }

    if (!drew_arena) {
        if (!native_layer_ready) {
            draw_centered_text(fb, 196, "ROM ARENA TILE LAYER OR CHR DATA MISSING", rgb(252, 236, 170), 2);
        } else {
            draw_centered_text(fb, 196, "ROM ARENA SPRITE GROUPS OR CHR DATA MISSING", rgb(252, 236, 170), 2);
        }
        draw_centered_text(fb, 232, "REBUILD THE ROM-DERIVED ASSET PACK", rgb(230, 232, 214), 1);
        if (runtime != NULL) {
            draw_centered_text(fb,
                               254,
                               native_layer_ready ? runtime->intro_arena_sprite_groups.status
                                                  : runtime->intro_arena_tile_layer.status,
                               rgb(142, 174, 190),
                               1);
        }
    }

    if (show_debug) {
        rect(fb, 0, 0, 640, 20, rgb(0, 0, 0));
        (void)snprintf(line,
                       sizeof(line),
                       "BANK04 $88E8 -> SCREEN18 $A2ED  F%u/%u %s  PAL%u  $88=%02X $8A=%02X $0301=%02X BG%+d",
                       native_frame,
                       frame,
                       drew_native_arena ? "EXACT-ROM-LAYER" : tecmo_intro_arena_phase_name(state.phase),
                       runtime != NULL && runtime->intro_arena_tile_layer.available ? 1U : 0U,
                       (unsigned)state.seed_88,
                       (unsigned)state.scroll_8a,
                       (unsigned)state.scroll_y_0301,
                       background_y - viewport_y);
        draw_text(fb, 12, 7, line, rgb(230, 232, 214), 1);

        rect(fb, 0, 456, 640, 24, rgb(0, 0, 0));
        if (drew_native_arena) {
            (void)snprintf(line,
                           sizeof(line),
                           "SCREEN18 EXACT ROM LAYERS 32x51  VISIBLE JUMBOTRON %u GOAL %u",
                           (unsigned)visible_counts.jumbotron,
                           (unsigned)visible_counts.goal);
        } else {
            (void)snprintf(line,
                           sizeof(line),
                           "SCREEN18 P0 %u P1 %u  BG R0/R1 %02X/%02X -> %02X/%02X R%d  OAM %u/%u CHR %02u/T0",
                           runtime != NULL ? (unsigned)runtime->intro_arena_capture.tile_count[0] : 0U,
                           runtime != NULL ? (unsigned)runtime->intro_arena_capture.tile_count[1] : 0U,
                           runtime != NULL ? (unsigned)runtime->intro_arena_capture.bg_upper_r0 : 0U,
                           runtime != NULL ? (unsigned)runtime->intro_arena_capture.bg_upper_r1 : 0U,
                           runtime != NULL ? (unsigned)runtime->intro_arena_capture.bg_lower_r0 : 0U,
                           runtime != NULL ? (unsigned)runtime->intro_arena_capture.bg_lower_r1 : 0U,
                           runtime != NULL ? runtime->intro_arena_capture.bg_split_row : 0,
                           runtime != NULL ? (unsigned)runtime->intro_arena_capture.sprite_count : 0U,
                           (unsigned)(visible_counts.jumbotron + visible_counts.goal),
                           runtime != NULL ? (unsigned)runtime->intro_arena_capture.sprite_chr_bank : 0U);
        }
        draw_text(fb, 12, 464, line, rgb(142, 174, 190), 1);
    }

    return drew_arena;
}

bool tecmo_render_intro_ready_screen(const TecmoRuntime *runtime, TecmoFramebuffer *fb)
{
    const int scale = 2;
    const int viewport_x = 64;
    const int viewport_y = 0;
    const int viewport_w = 256 * scale;
    const int viewport_h = 240 * scale;
    unsigned native_frame = runtime != NULL ? runtime->mode_frame_counter : 24U;
    bool show_debug = runtime == NULL || runtime->debug_overlay;
    bool rendered = false;
    TecmoIntroReadyState state;
    char line[192];

    clear(fb, rgb(0, 0, 0));
    rect(fb, viewport_x, viewport_y, viewport_w, viewport_h, rgb(0, 0, 0));
    if (runtime != NULL) {
        rendered = tecmo_intro_post_arena_draw_ready(fb,
                                                     &runtime->intro_ready_asset,
                                                     runtime->title_chr_bytes,
                                                     runtime->title_chr_byte_count,
                                                     native_frame,
                                                     viewport_x,
                                                     viewport_y,
                                                     scale);
    }
    if (!rendered) {
        draw_centered_text(fb, 212, "ROM READY ASSET OR CHR DATA MISSING", rgb(252, 236, 170), 2);
        if (runtime != NULL) {
            draw_centered_text(fb, 238, runtime->intro_ready_asset.status, rgb(142, 174, 190), 1);
        }
    }
    tecmo_intro_ready_state(native_frame, &state);

    if (show_debug) {
        rect(fb, 0, 0, 640, 20, rgb(0, 0, 0));
        (void)snprintf(line,
                       sizeof(line),
                       "POST ARENA READY  F%u PAL %u MASK %u BLACK %u HANDOFF %u",
                       native_frame,
                       (unsigned)state.palette_stage,
                       (unsigned)state.mask_index,
                       state.black ? 1U : 0U,
                       state.handoff ? 1U : 0U);
        draw_text(fb, 12, 7, line, rgb(230, 232, 214), 1);
    }
    return rendered;
}

bool tecmo_render_intro_warriors_transition(const TecmoRuntime *runtime, TecmoFramebuffer *fb)
{
    const int scale = 2;
    const int viewport_x = 64;
    const int viewport_y = 0;
    const int viewport_w = 256 * scale;
    const int viewport_h = 240 * scale;
    unsigned native_frame = runtime != NULL ? runtime->mode_frame_counter : 64U;
    bool show_debug = runtime == NULL || runtime->debug_overlay;
    bool rendered = false;
    TecmoIntroWarriorsState state;
    char line[192];

    clear(fb, rgb(0, 0, 0));
    rect(fb, viewport_x, viewport_y, viewport_w, viewport_h, rgb(0, 0, 0));
    if (runtime != NULL) {
        rendered = tecmo_intro_post_arena_draw_warriors(fb,
                                                        &runtime->intro_warriors_asset,
                                                        runtime->title_chr_bytes,
                                                        runtime->title_chr_byte_count,
                                                        native_frame,
                                                        viewport_x,
                                                        viewport_y,
                                                        scale);
    }
    if (!rendered) {
        draw_centered_text(fb, 212, "ROM WARRIORS ASSET OR CHR DATA MISSING", rgb(252, 236, 170), 2);
        if (runtime != NULL) {
            draw_centered_text(fb, 238, runtime->intro_warriors_asset.status, rgb(142, 174, 190), 1);
        }
    }
    tecmo_intro_warriors_state(native_frame, &state);

    if (show_debug) {
        rect(fb, 0, 0, 640, 20, rgb(0, 0, 0));
        (void)snprintf(line,
                       sizeof(line),
                       "POST ARENA WARRIORS F%u %s PAL%u PAN%u PATCH%u NEXT%02X",
                       native_frame,
                       tecmo_intro_warriors_phase_name(state.phase),
                       (unsigned)state.palette_stage,
                       (unsigned)state.pan,
                       (unsigned)state.patch_count,
                       (unsigned)state.next_screen);
        draw_text(fb, 12, 7, line, rgb(230, 232, 214), 1);
    }
    return rendered;
}

bool tecmo_render_intro_clippers_transition(const TecmoRuntime *runtime, TecmoFramebuffer *fb)
{
    const int scale = 2;
    const int viewport_x = 64;
    const int viewport_y = 0;
    unsigned native_frame = runtime != NULL ? runtime->mode_frame_counter : 60U;
    bool show_debug = runtime == NULL || runtime->debug_overlay;
    bool rendered = false;
    TecmoIntroClippersState state;
    char line[192];

    clear(fb, rgb(0, 0, 0));
    if (runtime != NULL) {
        rendered = tecmo_intro_post_arena_draw_clippers(fb,
                                                        &runtime->intro_clippers_asset,
                                                        runtime->title_chr_bytes,
                                                        runtime->title_chr_byte_count,
                                                        native_frame,
                                                        viewport_x,
                                                        viewport_y,
                                                        scale);
    }
    if (!rendered) {
        draw_centered_text(fb, 212, "ROM CLIPPERS ASSET OR CHR DATA MISSING", rgb(252, 236, 170), 2);
        if (runtime != NULL) {
            draw_centered_text(fb, 238, runtime->intro_clippers_asset.status, rgb(142, 174, 190), 1);
        }
    }
    tecmo_intro_clippers_state(native_frame, &state);
    if (show_debug) {
        rect(fb, 0, 0, 640, 20, rgb(0, 0, 0));
        (void)snprintf(line,
                       sizeof(line),
                       "POST ARENA CLIPPERS F%u PAL%u MOVE%u SCROLL%u PAGE%u NEXT%04X",
                       native_frame,
                       (unsigned)state.palette_stage,
                       (unsigned)state.motion,
                       (unsigned)state.scroll_x,
                       (unsigned)state.pose_page,
                       (unsigned)state.next_route);
        draw_text(fb, 12, 7, line, rgb(230, 232, 214), 1);
    }
    return rendered;
}

bool tecmo_render_intro_bucks_transition(const TecmoRuntime *runtime, TecmoFramebuffer *fb)
{
    const int scale = 2;
    const int viewport_x = 64;
    const int viewport_y = 0;
    unsigned native_frame = runtime != NULL ? runtime->mode_frame_counter : 32U;
    bool show_debug = runtime == NULL || runtime->debug_overlay;
    bool rendered = false;
    TecmoIntroBucksState state;
    char line[192];
    clear(fb, rgb(0, 0, 0));
    if (runtime != NULL) {
        rendered = tecmo_intro_post_arena_draw_bucks(fb,
                                                     &runtime->intro_bucks_asset,
                                                     runtime->title_chr_bytes,
                                                     runtime->title_chr_byte_count,
                                                     native_frame,
                                                     viewport_x,
                                                     viewport_y,
                                                     scale);
    }
    if (!rendered) {
        draw_centered_text(fb, 212, "ROM BUCKS ASSET OR CHR DATA MISSING", rgb(252, 236, 170), 2);
        if (runtime != NULL) draw_centered_text(fb, 238, runtime->intro_bucks_asset.status, rgb(142, 174, 190), 1);
    }
    tecmo_intro_bucks_state(native_frame, &state);
    if (show_debug) {
        rect(fb, 0, 0, 640, 20, rgb(0, 0, 0));
        (void)snprintf(line, sizeof(line),
                       "POST ARENA BUCKS F%u PAL%u FLASH%u SCROLL%u WORD%u NEXT%04X",
                       native_frame, (unsigned)state.palette_stage, (unsigned)state.flash_pass,
                       (unsigned)state.scroll_x, (unsigned)state.wordmark_glyph_count,
                       (unsigned)state.next_route);
        draw_text(fb, 12, 7, line, rgb(230, 232, 214), 1);
    }
    return rendered;
}

bool tecmo_render_intro_pass_transition(const TecmoRuntime *runtime, TecmoFramebuffer *fb)
{
    const int scale = 2;
    const int viewport_x = 64;
    const int viewport_y = 0;
    unsigned native_frame = runtime != NULL ? runtime->mode_frame_counter : 30U;
    bool show_debug = runtime == NULL || runtime->debug_overlay;
    bool rendered = false;
    TecmoIntroPassState state;
    char line[192];
    clear(fb, rgb(0, 0, 0));
    if (runtime != NULL) {
        rendered = tecmo_intro_post_arena_draw_pass(fb,
                                                    &runtime->intro_pass_asset,
                                                    runtime->title_chr_bytes,
                                                    runtime->title_chr_byte_count,
                                                    native_frame,
                                                    viewport_x,
                                                    viewport_y,
                                                    scale);
    }
    if (!rendered) {
        draw_centered_text(fb, 212, "ROM PASS ASSET OR CHR DATA MISSING", rgb(252, 236, 170), 2);
        if (runtime != NULL) draw_centered_text(fb, 238, runtime->intro_pass_asset.status, rgb(142, 174, 190), 1);
    }
    tecmo_intro_pass_state(native_frame, &state);
    if (show_debug) {
        rect(fb, 0, 0, 640, 20, rgb(0, 0, 0));
        (void)snprintf(line, sizeof(line),
                       "POST ARENA PASS F%u %s PAL%u X%u SCROLL%u M%u/%u NEXT%04X",
                       native_frame, tecmo_intro_pass_phase_name(state.phase),
                       (unsigned)state.palette_stage, (unsigned)state.player_x,
                       (unsigned)state.scroll_x, (unsigned)state.first_move_count,
                       (unsigned)state.second_move_count, (unsigned)state.next_route);
        draw_text(fb, 12, 7, line, rgb(230, 232, 214), 1);
    }
    return rendered;
}

bool tecmo_render_intro_finale(const TecmoRuntime *runtime, TecmoFramebuffer *fb)
{
    const int scale = 2;
    const int viewport_x = 64;
    const int viewport_y = 0;
    unsigned native_frame = runtime != NULL ? runtime->mode_frame_counter : 0U;
    bool show_debug = runtime == NULL || runtime->debug_overlay;
    bool rendered = false;
    TecmoIntroFinaleState state;
    char line[192];

    clear(fb, rgb(0, 0, 0));
    if (runtime != NULL) {
        rendered = tecmo_intro_finale_draw(fb,
                                           &runtime->intro_finale_asset,
                                           runtime->title_chr_bytes,
                                           runtime->title_chr_byte_count,
                                           native_frame,
                                           viewport_x,
                                           viewport_y,
                                           scale);
    }
    if (!rendered) {
        draw_centered_text(fb, 212, "ROM INTRO FINALE ASSET OR CHR DATA MISSING",
                           rgb(252, 236, 170), 2);
        if (runtime != NULL) {
            draw_centered_text(fb, 238, runtime->intro_finale_asset.status,
                               rgb(142, 174, 190), 1);
        }
    }
    tecmo_intro_finale_state(runtime != NULL ? &runtime->intro_finale_asset : NULL,
                             native_frame,
                             &state);
    if (show_debug) {
        rect(fb, 0, 0, 640, 20, rgb(0, 0, 0));
        (void)snprintf(line,
                       sizeof(line),
                       "INTRO FINALE F%u %s/%s LOCAL%u LOOP%u XY%u,%u TITLE%u P%u:%u S%u:%u HOLD%u",
                       native_frame,
                       tecmo_intro_finale_scene_name(state.scene),
                       tecmo_intro_finale_phase_name(state.phase),
                       state.scene_frame,
                       (unsigned)state.short_loop_step,
                       (unsigned)state.player_x,
                       (unsigned)state.player_y,
                       (unsigned)state.title_slots_written,
                       (unsigned)state.scroll_page,
                       (unsigned)state.scroll_x,
                       (unsigned)state.secondary_scroll_page,
                       (unsigned)state.secondary_scroll_x,
                       state.persistent_hold ? 1U : 0U);
        draw_text(fb, 12, 7, line, rgb(230, 232, 214), 1);
    }
    return rendered;
}

static void draw_intro_presents_nametable_position(TecmoFramebuffer *fb,
                                                   const TecmoRuntime *runtime)
{
    const int scale = 2;
    const int nametable_x = 64;
    const int nametable_y = 0;

    (void)tecmo_intro_screen_draw(fb,
                                  &runtime->intro_presents_asset,
                                  runtime->title_chr_bytes,
                                  runtime->title_chr_byte_count,
                                  runtime->mode_frame_counter,
                                  nametable_x,
                                  nametable_y,
                                  scale);
}

static void draw_coordinate_panel(TecmoFramebuffer *fb,
                                  int x,
                                  int y,
                                  int w,
                                  int h,
                                  const char *title)
{
    rect(fb, x, y, w, h, rgb(12, 14, 18));
    outline_rect(fb, x, y, w, h, rgb(66, 78, 88));
    draw_text(fb, x + 10, y + 12, title, rgb(252, 236, 118), 1);
}

static void render_intro_coordinate_audit(const TecmoRuntime *runtime, TecmoFramebuffer *fb)
{
    char line[160];

    draw_text(fb, 48, 132, "TECMO PRESENTS BACKGROUND AND RABBIT OAM ARE ROM-VERIFIED", rgb(230, 232, 214), 1);
    draw_text(fb, 48, 150, "SCREEN00 PLUS BD9E OAM ARE IMPORTED AS ONE STRICT NATIVE ASSET",
              rgb(142, 174, 190), 1);

    draw_coordinate_panel(fb, 24, 184, 188, 232, "PRESENTS BG");
    (void)snprintf(line, sizeof(line), "SCREEN %02X  NAMETABLE", (unsigned)INTRO_PRESENTS_SCREEN_ID);
    draw_text(fb, 36, 216, line, rgb(230, 232, 214), 1);
    (void)snprintf(line,
                   sizeof(line),
                   "PPU %04X-%04X",
                   (unsigned)INTRO_PRESENTS_PPU,
                   (unsigned)(INTRO_PRESENTS_PPU + INTRO_PRESENTS_LEN - 1U));
    draw_text(fb, 36, 234, line, rgb(142, 174, 190), 1);
    (void)snprintf(line,
                   sizeof(line),
                   "X %u-%u  Y %u-%u",
                   (unsigned)(INTRO_PRESENTS_TILE_COL * 8U),
                   (unsigned)((INTRO_PRESENTS_TILE_COL + INTRO_PRESENTS_LEN) * 8U),
                   (unsigned)(INTRO_PRESENTS_TILE_ROW * 8U),
                   (unsigned)((INTRO_PRESENTS_TILE_ROW + 1U) * 8U));
    draw_text(fb, 36, 252, line, rgb(142, 174, 190), 1);
    draw_intro_presents_text(fb, runtime, 52, 324, 1);

    draw_coordinate_panel(fb, 226, 184, 188, 232, "RABBIT OAM");
    (void)snprintf(line,
                   sizeof(line),
                   "SCREEN %02X  A7DB SEL01",
                   (unsigned)INTRO_RABBIT_SCREEN_ID);
    draw_text(fb, 238, 216, line, rgb(230, 232, 214), 1);
    (void)snprintf(line,
                   sizeof(line),
                   "BASE %u %u  PAGE %u",
                   (unsigned)INTRO_RABBIT_BASE_X,
                   (unsigned)INTRO_RABBIT_BASE_Y,
                   (unsigned)INTRO_RABBIT_Y_PAGE);
    draw_text(fb, 238, 234, line, rgb(142, 174, 190), 1);
    (void)snprintf(line,
                   sizeof(line),
                   "BOUNDS X%d-%d",
                   INTRO_RABBIT_BOUNDS_MIN_X,
                   INTRO_RABBIT_BOUNDS_MAX_X);
    draw_text(fb, 238, 252, line, rgb(142, 174, 190), 1);
    (void)snprintf(line,
                   sizeof(line),
                   "Y%d-%d  VISIBLE %d %d",
                   INTRO_RABBIT_BOUNDS_MIN_Y,
                   INTRO_RABBIT_BOUNDS_MAX_Y,
                   INTRO_RABBIT_FIRST_VISIBLE_X,
                   INTRO_RABBIT_FIRST_VISIBLE_Y);
    draw_text(fb, 238, 270, line, rgb(142, 174, 190), 1);
    draw_intro_trace_group_scaled_box(fb, runtime, INTRO_TRACE_GROUP_RABBIT, 286, 304, 1, rgb(80, 96, 110));

    draw_coordinate_panel(fb, 428, 184, 188, 232, "TECMO OAM");
    (void)snprintf(line,
                   sizeof(line),
                   "SCREEN %02X  A90F SEL00",
                   (unsigned)INTRO_TECMO_SCREEN_ID);
    draw_text(fb, 440, 216, line, rgb(230, 232, 214), 1);
    (void)snprintf(line,
                   sizeof(line),
                   "BASE 62-R88 INIT %u %u",
                   (unsigned)INTRO_TECMO_INITIAL_BASE_X,
                   (unsigned)INTRO_TECMO_BASE_Y);
    draw_text(fb, 440, 234, line, rgb(142, 174, 190), 1);
    (void)snprintf(line,
                   sizeof(line),
                   "LOGO X%d-%d",
                   INTRO_TECMO_LOGO_BOUNDS_MIN_X,
                   INTRO_TECMO_LOGO_BOUNDS_MAX_X);
    draw_text(fb, 440, 252, line, rgb(142, 174, 190), 1);
    (void)snprintf(line,
                   sizeof(line),
                   "Y%d-%d FULL Y%d-%d",
                   INTRO_TECMO_LOGO_BOUNDS_MIN_Y,
                   INTRO_TECMO_LOGO_BOUNDS_MAX_Y,
                   INTRO_TECMO_STREAM_BOUNDS_MIN_Y,
                   INTRO_TECMO_STREAM_BOUNDS_MAX_Y);
    draw_text(fb, 440, 270, line, rgb(142, 174, 190), 1);
    draw_intro_trace_group_scaled_box(fb, runtime, INTRO_TRACE_GROUP_TECMO_LOGO, 486, 304, 1, rgb(80, 96, 110));
}

static void render_intro_coordinate_assembly_preview(const TecmoRuntime *runtime, TecmoFramebuffer *fb)
{
    const int scale = 2;
    const int canvas_x = 64;
    const int canvas_y = 0;
    const int canvas_w = 256 * scale;
    const int canvas_h = 240 * scale;
    char line[160];

    clear(fb, rgb(0, 0, 0));
    rect(fb, canvas_x, canvas_y, canvas_w, canvas_h, rgb(0, 0, 0));
    outline_rect(fb, canvas_x, canvas_y, canvas_w, canvas_h, rgb(52, 62, 72));

    draw_intro_trace_group_absolute(fb, runtime, INTRO_TRACE_GROUP_TECMO_LOGO, canvas_x, canvas_y, scale);
    draw_intro_trace_group_absolute(fb, runtime, INTRO_TRACE_GROUP_RABBIT, canvas_x, canvas_y, scale);
    draw_intro_presents_text(fb,
                             runtime,
                             canvas_x + INTRO_PRESENTS_PIXEL_X * scale,
                             canvas_y + INTRO_PRESENTS_PIXEL_Y * scale,
                             scale);

    outline_rect(fb,
                 canvas_x + INTRO_PRESENTS_PIXEL_X * scale - 2,
                 canvas_y + INTRO_PRESENTS_PIXEL_Y * scale - 2,
                 (int)INTRO_PRESENTS_LEN * 8 * scale + 4,
                 8 * scale + 4,
                 rgb(92, 116, 128));
    outline_rect(fb,
                 canvas_x + INTRO_RABBIT_BOUNDS_MIN_X * scale,
                 canvas_y + INTRO_RABBIT_BOUNDS_MIN_Y * scale,
                 (INTRO_RABBIT_BOUNDS_MAX_X - INTRO_RABBIT_BOUNDS_MIN_X) * scale,
                 (INTRO_RABBIT_BOUNDS_MAX_Y - INTRO_RABBIT_BOUNDS_MIN_Y) * scale,
                 rgb(118, 80, 96));
    outline_rect(fb,
                 canvas_x + INTRO_TECMO_LOGO_BOUNDS_MIN_X * scale,
                 canvas_y + INTRO_TECMO_LOGO_BOUNDS_MIN_Y * scale,
                 (INTRO_TECMO_LOGO_BOUNDS_MAX_X - INTRO_TECMO_LOGO_BOUNDS_MIN_X) * scale,
                 (INTRO_TECMO_LOGO_BOUNDS_MAX_Y - INTRO_TECMO_LOGO_BOUNDS_MIN_Y) * scale,
                 rgb(96, 118, 80));

    rect(fb, 14, 376, 612, 78, rgb(0, 0, 0));
    outline_rect(fb, 14, 376, 612, 78, rgb(52, 62, 72));
    draw_text(fb, 24, 390, "CROSS STATE COORDINATE ASSEMBLY PREVIEW", rgb(248, 248, 232), 1);
    draw_text(fb, 24, 406, "USES LOGO-ONLY A90F RECORDS  NOT FULL A90F CONTEXT", rgb(252, 236, 118), 1);
    (void)snprintf(line,
                   sizeof(line),
                   "PRESENTS X%d-%d Y%d-%d  RABBIT X%d-%d Y%d-%d  TECMO X%d-%d Y%d-%d",
                   INTRO_PRESENTS_PIXEL_X,
                   INTRO_PRESENTS_PIXEL_X + (int)INTRO_PRESENTS_LEN * 8,
                   INTRO_PRESENTS_PIXEL_Y,
                   INTRO_PRESENTS_PIXEL_Y + 8,
                   INTRO_RABBIT_BOUNDS_MIN_X,
                   INTRO_RABBIT_BOUNDS_MAX_X,
                   INTRO_RABBIT_BOUNDS_MIN_Y,
                   INTRO_RABBIT_BOUNDS_MAX_Y,
                   INTRO_TECMO_LOGO_BOUNDS_MIN_X,
                   INTRO_TECMO_LOGO_BOUNDS_MAX_X,
                   INTRO_TECMO_LOGO_BOUNDS_MIN_Y,
                   INTRO_TECMO_LOGO_BOUNDS_MAX_Y);
    draw_text(fb, 24, 422, line, rgb(230, 232, 214), 1);
    draw_text(fb, 24, 440, "NORMAL PLAY USES THE ROM-DERIVED TISC-1 OPENING; TRACE DATA IS DEBUG-ONLY", rgb(142, 174, 190), 1);
}

static void render_intro_splash_play(const TecmoRuntime *runtime, TecmoFramebuffer *fb)
{
    uint8_t step = runtime->intro_output_step;
    const TecmoIntroTraceSprite *sprite;
    size_t record_index = 0;
    size_t selector0_count;
    size_t selector1_count;
    char line[192];

    if (step >= TECMO_INTRO_OUTPUT_STEP_COUNT) {
        step = TECMO_INTRO_OUTPUT_STEP_COUNT - 1U;
    }

    if ((!runtime->title_probe_available &&
         step != TECMO_INTRO_OUTPUT_TITLE_STEP &&
         step != TECMO_INTRO_OUTPUT_LICENSE_STEP &&
         step != TECMO_INTRO_OUTPUT_ARENA_STEP &&
         step != TECMO_INTRO_OUTPUT_READY_STEP &&
         step != TECMO_INTRO_OUTPUT_WARRIORS_STEP &&
         step != TECMO_INTRO_OUTPUT_CLIPPERS_STEP &&
         step != TECMO_INTRO_OUTPUT_BUCKS_STEP &&
         step != TECMO_INTRO_OUTPUT_PASS_STEP &&
         step != TECMO_INTRO_OUTPUT_FINALE_STEP &&
         step != TECMO_INTRO_OUTPUT_ATTRACT_STEP) ||
        (!runtime->intro_trace_available &&
         step != TECMO_INTRO_OUTPUT_TITLE_STEP &&
         step != TECMO_INTRO_OUTPUT_LICENSE_STEP &&
         step != TECMO_INTRO_OUTPUT_ARENA_STEP &&
         step != TECMO_INTRO_OUTPUT_READY_STEP &&
         step != TECMO_INTRO_OUTPUT_WARRIORS_STEP &&
         step != TECMO_INTRO_OUTPUT_CLIPPERS_STEP &&
         step != TECMO_INTRO_OUTPUT_BUCKS_STEP &&
         step != TECMO_INTRO_OUTPUT_PASS_STEP &&
         step != TECMO_INTRO_OUTPUT_FINALE_STEP &&
         step != TECMO_INTRO_OUTPUT_ATTRACT_STEP)) {
        tecmo_render_first_sprite_probe(runtime, fb);
        return;
    }

    if (step == TECMO_INTRO_OUTPUT_TITLE_STEP) {
        (void)render_intro_native_title_screen(runtime, fb, NULL, false);
        return;
    }

    if (step == TECMO_INTRO_OUTPUT_LICENSE_STEP) {
        tecmo_render_intro_license_screen(runtime, fb);
        return;
    }

    if (step == TECMO_INTRO_OUTPUT_ARENA_STEP) {
        tecmo_render_intro_arena_transition(runtime, fb);
        return;
    }

    if (step == TECMO_INTRO_OUTPUT_READY_STEP) {
        tecmo_render_intro_ready_screen(runtime, fb);
        return;
    }

    if (step == TECMO_INTRO_OUTPUT_WARRIORS_STEP) {
        tecmo_render_intro_warriors_transition(runtime, fb);
        return;
    }

    if (step == TECMO_INTRO_OUTPUT_CLIPPERS_STEP) {
        tecmo_render_intro_clippers_transition(runtime, fb);
        return;
    }

    if (step == TECMO_INTRO_OUTPUT_BUCKS_STEP) {
        tecmo_render_intro_bucks_transition(runtime, fb);
        return;
    }

    if (step == TECMO_INTRO_OUTPUT_PASS_STEP) {
        tecmo_render_intro_pass_transition(runtime, fb);
        return;
    }

    if (step == TECMO_INTRO_OUTPUT_FINALE_STEP) {
        tecmo_render_intro_finale(runtime, fb);
        return;
    }
    if (step == TECMO_INTRO_OUTPUT_ATTRACT_STEP) {
        clear(fb, rgb(0, 0, 0));
        (void)tecmo_title_attract_draw(fb, &runtime->title_asset,
                                      runtime->title_chr_bytes,
                                      runtime->title_chr_byte_count,
                                      runtime->mode_frame_counter,
                                      64, 0, 2);
        return;
    }

    clear(fb, rgb(0, 0, 0));
    draw_intro_output_header(fb, step);

    if (step == 0U) {
        sprite = find_intro_trace_sprite(runtime, INTRO_TRACE_GROUP_RABBIT, false, &record_index);
        describe_intro_trace_sprite(sprite, record_index, "STAGED", line, sizeof(line));
        draw_text(fb, 48, 136, "FIRST EMITTED SPRITE ROW FROM THE LOCAL D861 TRACE", rgb(230, 232, 214), 1);
        draw_text(fb, 48, 154, line, rgb(230, 232, 214), 1);
        draw_text(fb, 48, 172, "ORIGINAL Y WRAPS OFFSCREEN HERE; IT IS ENLARGED FOR INSPECTION", rgb(142, 174, 190), 1);
        if (sprite != NULL) {
            outline_rect(fb, 288, 214, 8 * 8 + 2, 16 * 8 + 2, rgb(80, 96, 110));
            draw_intro_trace_sprite(fb, runtime, sprite, 289, 215, 8);
        }
    } else if (step == 1U) {
        sprite = find_intro_trace_sprite(runtime, INTRO_TRACE_GROUP_RABBIT, true, &record_index);
        describe_intro_trace_sprite(sprite, record_index, "VISIBLE", line, sizeof(line));
        draw_text(fb, 48, 136, "FIRST RABBIT SPRITE ROW WITH AN ONSCREEN Y POSITION", rgb(230, 232, 214), 1);
        draw_text(fb, 48, 154, line, rgb(230, 232, 214), 1);
        draw_text(fb, 48, 172, "THIS IS STILL ONE 8X16 SPRITE PAIR, NOT THE WHOLE RABBIT", rgb(142, 174, 190), 1);
        if (sprite != NULL) {
            outline_rect(fb, 288, 214, 8 * 8 + 2, 16 * 8 + 2, rgb(80, 96, 110));
            draw_intro_trace_sprite(fb, runtime, sprite, 289, 215, 8);
        }
    } else if (step == 2U) {
        selector1_count = tecmo_intro_trace_group_count(runtime, INTRO_TRACE_GROUP_RABBIT);
        (void)snprintf(line,
                       sizeof(line),
                       "A7DB SELECTOR 01  BASE XB8 Y011E  %02u RECORDS",
                       (unsigned)selector1_count);
        draw_text(fb, 48, 136, line, rgb(230, 232, 214), 1);
        draw_intro_trace_group_centered(fb, runtime, INTRO_TRACE_GROUP_RABBIT, 320, 270, 3, rgb(80, 96, 110));
        draw_l88e7_group_summary(runtime, fb, INTRO_TRACE_GROUP_RABBIT, "SELECTOR 1 RABBIT SUMMARY", 48, 410);
    } else if (step == 3U) {
        selector0_count = tecmo_intro_trace_group_count(runtime, INTRO_TRACE_GROUP_A7DB_SELECTOR0);
        selector1_count = tecmo_intro_trace_group_count(runtime, INTRO_TRACE_GROUP_RABBIT);
        (void)snprintf(line,
                       sizeof(line),
                       "L88E7 L8988 OUTPUTS TWO A7DB STREAMS  SEL00 %02u REC  SEL01 %02u REC",
                       (unsigned)selector0_count,
                       (unsigned)selector1_count);
        draw_text(fb, 48, 132, line, rgb(230, 232, 214), 1);
        draw_text(fb, 48, 148, "$0100=05 SELECTS FIXED IRQ VECTOR FCF6; THAT IS SETUP, NOT TEXT", rgb(142, 174, 190), 1);
        draw_text(fb, 78, 178, "SEL00 COMPANION STREAM", rgb(252, 236, 118), 1);
        draw_intro_trace_group_scaled_box(fb, runtime, INTRO_TRACE_GROUP_A7DB_SELECTOR0, 92, 198, 1, rgb(80, 96, 110));
        draw_text(fb, 392, 178, "SEL01 RABBIT STREAM", rgb(252, 236, 118), 1);
        draw_intro_trace_group_scaled_box(fb, runtime, INTRO_TRACE_GROUP_RABBIT, 412, 198, 2, rgb(80, 96, 110));
    } else if (step == 4U) {
        draw_intro_presents_nametable_position(fb, runtime);
        draw_text(fb, 48, 292, "SCREEN ID 00 NAMETABLE STREAM, NOT A7DB OAM", rgb(230, 232, 214), 1);
        (void)snprintf(line,
                       sizeof(line),
                       "C003 -> D92E -> D9F6  REC $%04X  BANK%02X STREAM $%04X",
                       (unsigned)INTRO_PRESENTS_RECORD_CPU,
                       (unsigned)INTRO_PRESENTS_STREAM_BANK,
                       (unsigned)INTRO_PRESENTS_STREAM_CPU);
        draw_text(fb, 48, 310, line, rgb(142, 174, 190), 1);
        (void)snprintf(line,
                       sizeof(line),
                       "SRC $%04X -> PPU $%04X-$%04X  ROW %u  COL %u-%u",
                       (unsigned)INTRO_PRESENTS_SRC_CPU,
                       (unsigned)INTRO_PRESENTS_PPU,
                       (unsigned)(INTRO_PRESENTS_PPU + INTRO_PRESENTS_LEN - 1U),
                       (unsigned)INTRO_PRESENTS_TILE_ROW,
                       (unsigned)INTRO_PRESENTS_TILE_COL,
                       (unsigned)(INTRO_PRESENTS_TILE_COL + INTRO_PRESENTS_LEN - 1U));
        draw_text(fb, 48, 328, line, rgb(142, 174, 190), 1);
        (void)snprintf(line,
                       sizeof(line),
                       "MATCH %s  ROM SCREEN00 DECODES 58 NON-FF BG TILES",
                       runtime->intro_presents_data_available ? runtime->intro_presents_data_cpu : "UNRESOLVED");
        draw_text(fb, 48, 346, line, rgb(142, 174, 190), 1);
    } else if (step == 5U) {
        render_intro_coordinate_audit(runtime, fb);
    }

    draw_text(fb, 24, 464, runtime->intro_trace_status, rgb(92, 116, 128), 1);
}

static void draw_chr_bank_sheet(TecmoFramebuffer *fb,
                                const uint8_t *chr_bytes,
                                uint64_t chr_byte_count,
                                uint32_t chr_bank,
                                uint32_t chr_table,
                                int x,
                                int y,
                                int scale)
{
    uint16_t tile_base = (uint16_t)((chr_table & 1U) * 0x100U);
    for (uint16_t tile = 0; tile < 256U; ++tile) {
        int tile_x = x + (int)(tile & 0x0FU) * 8 * scale;
        int tile_y = y + (int)(tile >> 4U) * 8 * scale;
        draw_chr_tile(fb, chr_bytes, chr_byte_count, chr_bank, (uint16_t)(tile_base + tile), tile_x, tile_y, scale);
    }
}

static void render_chr_playground(const TecmoRuntime *runtime, TecmoFramebuffer *fb)
{
    const uint32_t chr_bank = selected_chr_bank(runtime);
    const uint32_t bank_count = chr_bank_count(runtime);
    const uint32_t chr_table = selected_chr_table(runtime);
    const uint16_t tile_base = (uint16_t)(chr_table * 0x100U);
    const int tile_scale = 2;
    const int cell_w = 34;
    const int cell_h = 28;
    const int grid_x = 28;
    const int grid_y = 74;
    char line[160];

    clear(fb, rgb(8, 10, 16));
    rect(fb, 0, 0, fb->width, 52, rgb(18, 18, 34));
    (void)snprintf(line,
                   sizeof(line),
                   "CHR PLAYGROUND - BANK %02u OF %02u TABLE %u",
                   (unsigned)chr_bank,
                   (unsigned)(bank_count - 1U),
                   (unsigned)chr_table);
    draw_text(fb, 24, 18, line, rgb(248, 248, 232), 2);
    draw_text(fb, 356, 40, "LR BANK UP DN TABLE TAB NEXT ENTER ESC", rgb(142, 174, 190), 1);

    if (!runtime->title_probe_available) {
        draw_centered_text(fb, 212, "LOCAL CHR DATA UNAVAILABLE", rgb(252, 236, 170), 2);
        return;
    }

    (void)snprintf(line,
                   sizeof(line),
                   "TILE ID GRID %03X-%03X  NUMBERS LETTERS AND TITLE PARTS",
                   (unsigned)(tile_base + 0x80U),
                   (unsigned)(tile_base + 0xAFU));
    draw_text(fb, 28, 56, line, rgb(142, 174, 190), 1);
    for (uint8_t row = 0; row < 3; ++row) {
        for (uint8_t col = 0; col < 16; ++col) {
            uint16_t tile = (uint16_t)(tile_base + 0x80U + row * 16U + col);
            int x = grid_x + (int)col * cell_w;
            int y = grid_y + (int)row * cell_h;
            (void)snprintf(line, sizeof(line), "%03X", (unsigned)tile);
            draw_text(fb, x, y - 10, line, rgb(92, 116, 128), 1);
            draw_chr_tile(fb,
                          runtime->title_chr_bytes,
                          runtime->title_chr_byte_count,
                          chr_bank,
                          tile,
                          x + 5,
                          y,
                          tile_scale);
        }
    }

    rect(fb, 26, 174, 588, 2, rgb(52, 60, 72));
    (void)snprintf(line,
                   sizeof(line),
                   "ASSEMBLED 2X2 TITLE GLYPHS FROM BANK 06 MAP AND BANK %02u TABLE %u",
                   (unsigned)chr_bank,
                   (unsigned)chr_table);
    draw_text(fb, 28, 190, line, rgb(142, 174, 190), 1);
    {
        int scale = 2;
        int glyph_width = 16 * scale;
        int title_width = (int)runtime->title_glyphs.glyph_count * glyph_width;
        int start_x;
        if (title_width > fb->width - 40) {
            scale = 1;
            glyph_width = 16;
            title_width = (int)runtime->title_glyphs.glyph_count * glyph_width;
        }
        start_x = (fb->width - title_width) / 2;
        for (size_t i = 0; i < runtime->title_glyphs.glyph_count; ++i) {
            draw_title_glyph(fb,
                             runtime->title_chr_bytes,
                             runtime->title_chr_byte_count,
                             chr_bank,
                             chr_table,
                             &runtime->title_glyphs.glyphs[i],
                             start_x + (int)i * glyph_width,
                             218,
                             scale);
        }
    }

    draw_text(fb, 28, 286, "TITLE MAP SAMPLE", rgb(230, 232, 214), 1);
    for (size_t row = 0; row < 4; ++row) {
        char *out = line;
        size_t remaining = sizeof(line);
        size_t start = row * 3U;
        int written;
        line[0] = '\0';
        for (size_t i = start; i < start + 3U && i < runtime->title_glyphs.glyph_count; ++i) {
            const TecmoTitleGlyph *glyph = &runtime->title_glyphs.glyphs[i];
            written = snprintf(out,
                               remaining,
                               "%c=%02X[%02X %02X %02X %02X]  ",
                               glyph->character,
                               (unsigned)glyph->tile_index,
                               (unsigned)glyph->glyph_tiles[0],
                               (unsigned)glyph->glyph_tiles[1],
                               (unsigned)glyph->glyph_tiles[2],
                               (unsigned)glyph->glyph_tiles[3]);
            if (written < 0 || (size_t)written >= remaining) {
                break;
            }
            out += written;
            remaining -= (size_t)written;
        }
        draw_text(fb, 28, 308 + (int)row * 18, line, rgb(142, 174, 190), 1);
    }

    draw_text(fb, 28, 398, "SOURCE CANDIDATES: BANK04 C-0116..C-0140 DRIVER, BANK00 C-0191 C-0192 TEXT LAYOUT", rgb(142, 174, 190), 1);
    draw_text(fb, 28, 420, "PLAYGROUND OUTPUT STAYS LOCAL UNDER BUILD WHEN RENDERED BY TESTS", rgb(230, 232, 214), 1);
}

static void render_intro_layout_lab(const TecmoRuntime *runtime, TecmoFramebuffer *fb)
{
    const uint32_t chr_bank = selected_chr_bank(runtime);
    const uint32_t bank_count = chr_bank_count(runtime);
    const uint32_t chr_table = selected_chr_table(runtime);
    const uint16_t selected_tile = selected_intro_tile_id(runtime);
    const int sheet_x = 30;
    const int sheet_y = 76;
    const int sheet_scale = 2;
    const int canvas_x = 330;
    const int canvas_y = 82;
    const int canvas_w = 284;
    const int canvas_h = 230;
    char line[128];

    clear(fb, rgb(6, 7, 10));
    rect(fb, 0, 0, fb->width, 52, rgb(18, 18, 34));
    (void)snprintf(line,
                   sizeof(line),
                   "INTRO LAB - BANK %02u OF %02u TABLE %u",
                   (unsigned)chr_bank,
                   (unsigned)(bank_count - 1U),
                   (unsigned)chr_table);
    draw_text(fb, 22, 18, line, rgb(248, 248, 232), 2);
    draw_text(fb, 282, 40, "QE BANK T TABLE TAB FOCUS R RAB M TECMO C COMP", rgb(142, 174, 190), 1);

    if (!runtime->title_probe_available) {
        draw_centered_text(fb, 212, "LOCAL CHR DATA UNAVAILABLE", rgb(252, 236, 170), 2);
        return;
    }

    (void)snprintf(line, sizeof(line), "REAL CHR BANK %02u TABLE %u SHEET", (unsigned)chr_bank, (unsigned)chr_table);
    draw_text(fb, sheet_x, 58, line, rgb(142, 174, 190), 1);
    for (uint8_t col = 0; col < 16U; ++col) {
        (void)snprintf(line, sizeof(line), "%X", (unsigned)col);
        draw_text(fb, sheet_x + (int)col * 16 + 5, sheet_y - 12, line, rgb(92, 116, 128), 1);
    }
    for (uint8_t row = 0; row < 16U; ++row) {
        (void)snprintf(line, sizeof(line), "%X", (unsigned)row);
        draw_text(fb, sheet_x - 12, sheet_y + (int)row * 16 + 5, line, rgb(92, 116, 128), 1);
    }
    draw_chr_bank_sheet(fb,
                        runtime->title_chr_bytes,
                        runtime->title_chr_byte_count,
                        chr_bank,
                        chr_table,
                        sheet_x,
                        sheet_y,
                        sheet_scale);
    {
        int source_col = (int)(runtime->intro_source_tile & 0x0FU);
        int source_row = (int)((runtime->intro_source_tile >> 4U) & 0x0FU);
        uint32_t color = runtime->intro_canvas_focus ? rgb(92, 116, 128) : rgb(252, 236, 118);
        outline_rect(fb,
                     sheet_x + source_col * 16 - 2,
                     sheet_y + source_row * 16 - 2,
                     20,
                     20,
                     color);
    }

    rect(fb, canvas_x - 2, canvas_y - 2, canvas_w + 4, canvas_h + 4, rgb(72, 86, 96));
    rect(fb, canvas_x, canvas_y, canvas_w, canvas_h, rgb(0, 0, 0));
    for (int x = 0; x <= canvas_w; x += 16) {
        rect(fb, canvas_x + x, canvas_y, 1, canvas_h, rgb(22, 26, 32));
    }
    for (int y = 0; y <= canvas_h; y += 16) {
        rect(fb, canvas_x, canvas_y + y, canvas_w, 1, rgb(22, 26, 32));
    }
    draw_text(fb, canvas_x, canvas_y - 20, "ASSET-BACKED TARGET CANVAS", rgb(142, 174, 190), 1);

    if (runtime->intro_placement_count == 0 && runtime->intro_glyphs.glyph_count >= 14U) {
        draw_title_glyph_range(fb,
                               runtime->title_chr_bytes,
                               runtime->title_chr_byte_count,
                               chr_bank,
                               chr_table,
                               &runtime->intro_glyphs,
                               0,
                               5,
                               canvas_x + 62,
                               canvas_y + 58,
                               2);
        draw_title_glyph_range(fb,
                               runtime->title_chr_bytes,
                               runtime->title_chr_byte_count,
                               chr_bank,
                               chr_table,
                               &runtime->intro_glyphs,
                               6,
                               8,
                               canvas_x + 14,
                               canvas_y + 138,
                               2);
    } else if (runtime->intro_placement_count == 0) {
        draw_text(fb, canvas_x + 24, canvas_y + 104, "INTRO GLYPH MAP MISSING", rgb(252, 236, 170), 1);
    }

    for (size_t i = 0; i < runtime->intro_placement_count; ++i) {
        const TecmoIntroPlacement *placement = &runtime->intro_placements[i];
        if (!placement->active) {
            continue;
        }
        for (size_t tile_index = 0; tile_index < placement->tile_count; ++tile_index) {
            draw_chr_tile(fb,
                          runtime->title_chr_bytes,
                          runtime->title_chr_byte_count,
                          placement->chr_bank,
                          placement->tile_ids[tile_index],
                          canvas_x + placement->pixel_x + (int)tile_index * 8 * placement->scale,
                          canvas_y + placement->pixel_y,
                          placement->scale);
        }
    }

    {
        int cursor_x = canvas_x + runtime->intro_canvas_cell_x * INTRO_CANVAS_CELL_SIZE;
        int cursor_y = canvas_y + runtime->intro_canvas_cell_y * INTRO_CANVAS_CELL_SIZE;
        uint32_t color = runtime->intro_canvas_focus ? rgb(252, 236, 118) : rgb(92, 116, 128);
        outline_rect(fb, cursor_x, cursor_y, INTRO_CANVAS_CELL_SIZE, INTRO_CANVAS_CELL_SIZE, color);
    }

    (void)snprintf(line,
                   sizeof(line),
                   "FOCUS %s  SRC B%02u T%u %03X  CELL %02d %02d  RECORDS %02u",
                   runtime->intro_canvas_focus ? "CANVAS" : "SOURCE",
                   (unsigned)chr_bank,
                   (unsigned)chr_table,
                   (unsigned)selected_tile,
                   runtime->intro_canvas_cell_x,
                   runtime->intro_canvas_cell_y,
                   (unsigned)runtime->intro_placement_count);
    draw_text(fb, 30, 342, line, rgb(230, 232, 214), 1);
    draw_text(fb, 30, 360, "ARROWS MOVE FOCUS  SPACE RECORD  S SAVE  BACKSPACE REMOVE  ENTER ESC MENU", rgb(142, 174, 190), 1);
    draw_text(fb, 30, 378, runtime->intro_layout_status, runtime->intro_layout_dirty ? rgb(252, 236, 118) : rgb(142, 174, 190), 1);

    if (runtime->intro_placement_count == 0) {
        draw_text(fb, 30, 406, "NO RECORDS YET  SPACE ADDS THE SELECTED TILE TO THE CANVAS", rgb(92, 116, 128), 1);
    } else {
        size_t first = runtime->intro_placement_count > 8U ? runtime->intro_placement_count - 8U : 0U;
        draw_text(fb, 30, 400, "LOCAL PLACEMENT RECORDS", rgb(230, 232, 214), 1);
        for (size_t i = first; i < runtime->intro_placement_count; ++i) {
            const TecmoIntroPlacement *placement = &runtime->intro_placements[i];
            (void)snprintf(line,
                           sizeof(line),
                           "%02u  %s  TILE %03X  CELL %02d %02d",
                           (unsigned)(i + 1U),
                           placement->label,
                           (unsigned)(placement->tile_count > 0 ? placement->tile_ids[0] : 0U),
                           placement->canvas_cell_x,
                           placement->canvas_cell_y);
            draw_text(fb, 30, 408 + (int)(i - first) * 8, line, rgb(142, 174, 190), 1);
        }
    }
    draw_text(fb, 30, 470, "S WRITES IGNORED BUILD INTRO_LAYOUT_PICKS.JSON", rgb(92, 116, 128), 1);
}

void tecmo_render_original_title_chr_probe(TecmoFramebuffer *framebuffer,
                                           const TecmoOriginalTitleGlyphs *glyphs,
                                           const uint8_t *chr_bytes,
                                           uint64_t chr_byte_count,
                                           uint32_t chr_bank)
{
    char line[160];
    int scale = 3;
    int glyph_width = 16 * scale;
    int title_width;
    int start_x;
    int y = 150;

    clear(framebuffer, rgb(8, 10, 24));
    rect(framebuffer, 0, 0, framebuffer->width, 48, rgb(18, 18, 34));
    rect(framebuffer, 0, framebuffer->height - 58, framebuffer->width, 58, rgb(18, 18, 34));
    rect(framebuffer, 36, 94, framebuffer->width - 72, 192, rgb(52, 28, 58));
    rect(framebuffer, 46, 104, framebuffer->width - 92, 172, rgb(16, 22, 48));
    rect(framebuffer, 58, 116, framebuffer->width - 116, 148, rgb(96, 34, 52));
    rect(framebuffer, 68, 126, framebuffer->width - 136, 128, rgb(20, 26, 54));

    if (glyphs == NULL || glyphs->glyph_count == 0 || chr_bytes == NULL || chr_byte_count == 0) {
        draw_centered_text(framebuffer, 188, "CHR TITLE DATA MISSING", rgb(252, 236, 170), 2);
        return;
    }

    title_width = (int)glyphs->glyph_count * glyph_width;
    if (title_width > framebuffer->width - 32) {
        scale = 2;
        glyph_width = 16 * scale;
        title_width = (int)glyphs->glyph_count * glyph_width;
    }
    if (title_width > framebuffer->width - 32) {
        scale = 1;
        glyph_width = 16 * scale;
        title_width = (int)glyphs->glyph_count * glyph_width;
    }
    start_x = (framebuffer->width - title_width) / 2;

    for (size_t i = 0; i < glyphs->glyph_count; ++i) {
        draw_title_glyph(framebuffer, chr_bytes, chr_byte_count, chr_bank, 0U, &glyphs->glyphs[i], start_x + (int)i * glyph_width, y, scale);
    }

    draw_centered_text(framebuffer, 294, "NATIVE CHR TITLE GLYPH PROBE", rgb(142, 174, 190), 1);
    (void)snprintf(line,
                   sizeof(line),
                   "C711 %02X -> BANK %02X:%04X  CHR BANK %02u",
                   (unsigned)glyphs->dispatcher_call_index,
                   (unsigned)glyphs->dispatcher_bank,
                   (unsigned)glyphs->dispatcher_target,
                   (unsigned)chr_bank);
    draw_centered_text(framebuffer, 316, line, rgb(142, 174, 190), 1);
    (void)snprintf(line,
                   sizeof(line),
                   "SETUP 0100=%02X 0352=%02X  BA16 05B6|=%02X",
                   (unsigned)glyphs->chr_config_0100,
                   (unsigned)glyphs->setup_selector_0352,
                   (unsigned)glyphs->ba16_update_flags_or_05b6);
    draw_centered_text(framebuffer, 338, line, rgb(230, 232, 214), 1);

    if (glyphs->setup_summary.loaded) {
        const TecmoTitleSetupSummary *setup = &glyphs->setup_summary;
        (void)snprintf(line,
                       sizeof(line),
                       "STAGE BA25 CALLS %02u/%02u WRITES %02u TABLES %02u/%02u",
                       (unsigned)setup->driver_call_count,
                       (unsigned)setup->driver_call_invocations,
                       (unsigned)setup->driver_write_count,
                       (unsigned)setup->verified_table_reference_count,
                       (unsigned)setup->table_reference_count);
        draw_centered_text(framebuffer, 356, line, rgb(142, 174, 190), 1);

        if (setup->fixed_helper_summary_loaded) {
            if (setup->fixed_vector_summary_loaded) {
                (void)snprintf(line,
                               sizeof(line),
                               "FIXED %02u-%02u VEC %02u-%02u WAIT %03u SEED %02u FIN %02u-%02u",
                               (unsigned)setup->fixed_helper_unique_count,
                               (unsigned)setup->fixed_helper_call_invocations,
                               (unsigned)setup->fixed_vector_jmp_entry_count,
                               (unsigned)setup->fixed_vector_entry_count,
                               (unsigned)setup->fixed_wait_request_total,
                               (unsigned)setup->fixed_staging_seed_call_count,
                               (unsigned)setup->fixed_setup_finalize_call_count,
                               (unsigned)setup->fixed_stream_finalize_call_count);
            } else {
                (void)snprintf(line,
                               sizeof(line),
                               "FIXED %02u-%02u WAIT %02u=%03u SEED %02u FIN %02u-%02u",
                               (unsigned)setup->fixed_helper_unique_count,
                               (unsigned)setup->fixed_helper_call_invocations,
                               (unsigned)setup->fixed_wait_call_count,
                               (unsigned)setup->fixed_wait_request_total,
                               (unsigned)setup->fixed_staging_seed_call_count,
                               (unsigned)setup->fixed_setup_finalize_call_count,
                               (unsigned)setup->fixed_stream_finalize_call_count);
            }
            draw_centered_text(framebuffer, 374, line, rgb(142, 174, 190), 1);
        }

        if (setup->first_unclassified_call != 0U) {
            (void)snprintf(line,
                           sizeof(line),
                           "STREAM BAA4 WRITES %02u  UNCLASSIFIED CALL %04X",
                           (unsigned)setup->stream_write_count,
                           (unsigned)setup->first_unclassified_call);
        } else {
            (void)snprintf(line,
                           sizeof(line),
                           "STREAM BAA4 WRITES %02u  FIXED HELPERS PENDING",
                           (unsigned)setup->stream_write_count);
        }
        draw_centered_text(framebuffer, 392, line, rgb(142, 174, 190), 1);

        if (setup->stream_format_summary_loaded) {
            (void)snprintf(line,
                           sizeof(line),
                           "STREAM TABLE %02u/%02u SELECTED %02u ROWS %02u/%02u MAX %02u REC",
                           (unsigned)setup->verified_stream_table_entry_count,
                           (unsigned)setup->stream_table_entry_count,
                           (unsigned)setup->selected_stream_count,
                           (unsigned)setup->terminated_selector_row_count,
                           (unsigned)setup->dynamic_selector_row_count,
                           (unsigned)setup->max_stream_record_count);
            draw_centered_text(framebuffer, 410, line, rgb(142, 174, 190), 1);
        }

        if (setup->stream_effect_summary_loaded) {
            (void)snprintf(line,
                           sizeof(line),
                           "STREAM EFFECT BASE %02u SRC %02u OUT %02u MAX %03u TO %03u",
                           (unsigned)setup->stream_base_parameter_bytes,
                           (unsigned)setup->stream_source_fields_per_record,
                           (unsigned)setup->stream_staged_fields_per_record,
                           (unsigned)setup->max_stream_bytes_consumed,
                           (unsigned)setup->max_stream_emitted_bytes);
            draw_centered_text(framebuffer, 428, line, rgb(142, 174, 190), 1);
        }

        if (setup->stream_staging_summary_loaded) {
            (void)snprintf(line,
                           sizeof(line),
                           "STAGING %02u STREAMS %03u REC %04u BYTES %04X-%04X",
                           (unsigned)setup->stream_staging_stream_count,
                           (unsigned)setup->stream_staging_record_count,
                           (unsigned)setup->stream_staging_bytes_written,
                           (unsigned)setup->stream_staging_first_write,
                           (unsigned)setup->stream_staging_last_write);
            draw_centered_text(framebuffer, 442, line, rgb(142, 174, 190), 1);
        }
    }

    rect(framebuffer, 116, 452, 408, 2, rgb(236, 214, 112));
    if (glyphs->setup_summary.loaded && glyphs->setup_summary.palette_probe_summary_loaded) {
        const TecmoTitleSetupSummary *setup = &glyphs->setup_summary;
        (void)snprintf(line,
                       sizeof(line),
                       "PAL PPU %02u-%02u HIGH %02u VEC %02u-%02u QUEUE %s",
                       (unsigned)setup->palette_direct_ppu_addr_write_count,
                       (unsigned)setup->palette_direct_ppu_data_write_count,
                       (unsigned)setup->palette_direct_high_literal_count,
                       (unsigned)setup->fixed_vector_jmp_entry_count,
                       (unsigned)setup->fixed_vector_entry_count,
                       setup->palette_queue_decode_pending ? "PENDING" : "CHECK");
        draw_centered_text(framebuffer, 464, line, rgb(230, 232, 214), 1);
    } else {
        draw_centered_text(framebuffer, 464, "HELPER DETAILS AND PALETTE DECODE NEXT", rgb(230, 232, 214), 1);
    }
}

void tecmo_runtime_render(const TecmoRuntime *runtime, TecmoFramebuffer *framebuffer)
{
    if (runtime->mode == TECMO_MODE_MAIN_MENU) {
        render_main_menu(runtime, framebuffer);
    } else if (runtime->mode == TECMO_MODE_TITLE_SCREEN) {
        render_title_screen_mode(runtime, framebuffer);
    } else if (runtime->mode == TECMO_MODE_INTRO_PROBE) {
        render_intro_layout_lab(runtime, framebuffer);
    } else if (runtime->mode == TECMO_MODE_CHR_PLAYGROUND) {
        render_chr_playground(runtime, framebuffer);
    } else if (runtime->mode == TECMO_MODE_FIRST_SPRITE) {
        render_intro_splash_play(runtime, framebuffer);
    } else if (runtime->mode == TECMO_MODE_PLAY_SETUP) {
        render_roster_browser(runtime, framebuffer, true);
    } else if (runtime->mode == TECMO_MODE_ROSTERS) {
        render_roster_browser(runtime, framebuffer, false);
    } else if (runtime->mode == TECMO_MODE_COURT) {
        render_court(runtime, framebuffer);
    } else if (runtime->mode == TECMO_MODE_START_GAME_MENU) {
        render_start_game_menu_mode(runtime, framebuffer);
    } else if (runtime->mode == TECMO_MODE_PRESEASON_MENU) {
        render_preseason_menu_mode(runtime, framebuffer);
    }

    if (runtime->debug_overlay) {
        render_debug_overlay(runtime, framebuffer);
    }
}
