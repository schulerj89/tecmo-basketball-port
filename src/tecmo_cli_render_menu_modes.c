#include "tecmo_all_star_menu.h"
#include "tecmo_framebuffer.h"
#include "tecmo_game.h"
#include "tecmo_preseason_menu.h"
#include "tecmo_season_menu.h"
#include "tecmo_start_game_menu.h"
#include "tecmo_team_data.h"
#include "tecmo_team_management.h"

#include <stdio.h>
#include <string.h>

#include "tecmo_cli_internal.h"

static void seed_populated_leader_results(TecmoSeasonSession *session)
{
    if (session == NULL) return;
    session->season_type = TECMO_SEASON_REGULAR;
    session->schedule_index = 0U;
    session->player_stats_coverage =
        TECMO_PLAYER_STATS_IMPLEMENTED_COVERAGE;
    memset(session->wins, 0, sizeof(session->wins));
    memset(session->losses, 0, sizeof(session->losses));
    memset(session->player_stats_totals, 0,
           sizeof(session->player_stats_totals));
    for (uint8_t team = 0U; team < TECMO_PLAYER_STATS_TEAM_COUNT; ++team) {
        session->wins[team] = 82U;
        for (uint8_t roster = 0U;
             roster < TECMO_PLAYER_STATS_ROSTER_COUNT; ++roster) {
            uint16_t key = (uint16_t)team *
                           TECMO_PLAYER_STATS_ROSTER_COUNT + roster;
            session->player_stats_totals[team][roster][
                TECMO_PLAYER_STATS_COUNTER_FGA] = 800U;
            session->player_stats_totals[team][roster][
                TECMO_PLAYER_STATS_COUNTER_FGM] = (uint16_t)(300U + key);
            session->player_stats_totals[team][roster][
                TECMO_PLAYER_STATS_COUNTER_THREE_PA] = 500U;
            session->player_stats_totals[team][roster][
                TECMO_PLAYER_STATS_COUNTER_THREE_PM] =
                (uint16_t)(50U + key);
            session->player_stats_totals[team][roster][
                TECMO_PLAYER_STATS_COUNTER_FTA] = 500U;
            session->player_stats_totals[team][roster][
                TECMO_PLAYER_STATS_COUNTER_FTM] = (uint16_t)(90U + key);
        }
    }
    session->dirty = false;
}

static bool configure_start_game_menu_mode(TecmoRuntime *runtime, const char *mode_name, TecmoCliRenderModeState *state, bool *handled_out)
{
    bool arena_render_succeeded = state->arena_render_succeeded;
    bool render_runtime = true;
    int result = state->result;

    *handled_out = false;
            if (strcmp(mode_name, "menu") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_MAIN_MENU);
            } else if (strcmp(mode_name, "start-game-menu") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
                runtime->start_game_menu_state.frame = 32U;
                runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_ROOT;
            } else if (strncmp(mode_name, "start-game-menu-frame", 21) == 0) {
                *handled_out = true;
                unsigned frame;
                if (!tecmo_cli_parse_render_frame_suffix(mode_name, "start-game-menu-frame", &frame) ||
                    frame > 32U) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
                    runtime->start_game_menu_state.frame = frame;
                    runtime->start_game_menu_state.phase = frame < 32U
                        ? TECMO_START_GAME_MENU_REVEAL : TECMO_START_GAME_MENU_ROOT;
                }
            } else if (strncmp(mode_name, "start-game-menu-cursor", 22) == 0) {
                *handled_out = true;
                unsigned selection;
                if (!tecmo_cli_parse_render_frame_suffix(mode_name, "start-game-menu-cursor", &selection) ||
                    selection >= TECMO_START_GAME_MENU_ROOT_COUNT) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
                    runtime->start_game_menu_state.frame = 32U;
                    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_ROOT;
                    runtime->start_game_menu_state.root_selection = (uint8_t)selection;
                }
            } else if (strncmp(mode_name, "start-game-menu-season-frame", 28) == 0) {
                *handled_out = true;
                unsigned frame;
                if (!tecmo_cli_parse_render_frame_suffix(mode_name, "start-game-menu-season-frame", &frame) ||
                    frame > 32U) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
                    runtime->start_game_menu_state.frame = 32U + frame;
                    runtime->start_game_menu_state.slide_frame = (uint16_t)frame;
                    runtime->start_game_menu_state.root_selection = 1U;
                    runtime->start_game_menu_state.phase = frame < 32U
                        ? TECMO_START_GAME_MENU_SEASON_SLIDE_IN : TECMO_START_GAME_MENU_SEASON;
                    runtime->start_game_menu_state.cursor_delay = frame < 32U ? 0U :
                        runtime->start_game_menu_asset.cursor_commit_delay_frames;
                    runtime->start_game_menu_state.direction_cooldown = frame < 32U
                        ? runtime->start_game_menu_asset.accepted_input_seed
                        : (uint16_t)(runtime->start_game_menu_asset.accepted_input_seed - 1U);
                }
            } else if (strcmp(mode_name, "start-game-menu-season") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
                runtime->start_game_menu_state.frame = 64U;
                runtime->start_game_menu_state.slide_frame = 32U;
                runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_SEASON;
            } else if (strncmp(mode_name, "start-game-menu-music-setup-frame", 33) == 0 ||
                       strncmp(mode_name, "start-game-menu-speed-setup-frame", 33) == 0 ||
                       strncmp(mode_name, "start-game-menu-period-setup-frame", 34) == 0) {
                *handled_out = true;
                const char *prefix;
                TecmoStartGameMenuPhase popup_phase;
                size_t overlay_index;
                unsigned frame;
                unsigned setup_frames;
                if (strncmp(mode_name, "start-game-menu-music-setup-frame", 33) == 0) {
                    prefix = "start-game-menu-music-setup-frame";
                    popup_phase = TECMO_START_GAME_MENU_MUSIC;
                    overlay_index = 0U;
                } else if (strncmp(mode_name, "start-game-menu-speed-setup-frame", 33) == 0) {
                    prefix = "start-game-menu-speed-setup-frame";
                    popup_phase = TECMO_START_GAME_MENU_SPEED;
                    overlay_index = 1U;
                } else {
                    prefix = "start-game-menu-period-setup-frame";
                    popup_phase = TECMO_START_GAME_MENU_PERIOD;
                    overlay_index = 2U;
                }
                setup_frames = runtime->start_game_menu_asset.overlays[overlay_index].height *
                               runtime->start_game_menu_asset.popup_row_cadence;
                if (popup_phase == TECMO_START_GAME_MENU_PERIOD)
                    setup_frames += runtime->start_game_menu_asset.period_setup_extra_frames;
                if (!tecmo_cli_parse_render_frame_suffix(mode_name, prefix, &frame) ||
                    frame > setup_frames) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
                    runtime->start_game_menu_state.frame = 32U + frame;
                    runtime->start_game_menu_state.popup_phase = popup_phase;
                    runtime->start_game_menu_state.phase = frame < setup_frames
                        ? TECMO_START_GAME_MENU_POPUP_SETUP : popup_phase;
                    runtime->start_game_menu_state.transition_frame = (uint16_t)frame;
                    runtime->start_game_menu_state.direction_cooldown = frame < setup_frames
                        ? runtime->start_game_menu_asset.accepted_input_seed
                        : (uint16_t)(runtime->start_game_menu_asset.accepted_input_seed - 1U);
                    runtime->start_game_menu_state.cursor_delay = frame < setup_frames ? 0U :
                        runtime->start_game_menu_asset.cursor_commit_delay_frames;
                    runtime->start_game_menu_state.root_selection = popup_phase == TECMO_START_GAME_MENU_MUSIC
                        ? 6U : popup_phase == TECMO_START_GAME_MENU_SPEED ? 4U : 5U;
                    runtime->start_game_menu_state.setting_selection = popup_phase == TECMO_START_GAME_MENU_MUSIC
                        ? runtime->start_game_menu_state.music_value :
                        popup_phase == TECMO_START_GAME_MENU_SPEED
                            ? runtime->start_game_menu_state.speed_value
                            : runtime->start_game_menu_state.period_index;
                }
            } else if (strncmp(mode_name, "start-game-menu-music-teardown-frame", 36) == 0 ||
                       strncmp(mode_name, "start-game-menu-speed-teardown-frame", 36) == 0 ||
                       strncmp(mode_name, "start-game-menu-period-teardown-frame", 37) == 0) {
                *handled_out = true;
                const char *prefix;
                TecmoStartGameMenuPhase popup_phase;
                size_t overlay_index;
                unsigned frame;
                unsigned teardown_frames;
                if (strncmp(mode_name, "start-game-menu-music-teardown-frame", 36) == 0) {
                    prefix = "start-game-menu-music-teardown-frame";
                    popup_phase = TECMO_START_GAME_MENU_MUSIC;
                    overlay_index = 0U;
                } else if (strncmp(mode_name, "start-game-menu-speed-teardown-frame", 36) == 0) {
                    prefix = "start-game-menu-speed-teardown-frame";
                    popup_phase = TECMO_START_GAME_MENU_SPEED;
                    overlay_index = 1U;
                } else {
                    prefix = "start-game-menu-period-teardown-frame";
                    popup_phase = TECMO_START_GAME_MENU_PERIOD;
                    overlay_index = 2U;
                }
                teardown_frames = runtime->start_game_menu_asset.overlays[overlay_index].height *
                                  runtime->start_game_menu_asset.popup_row_cadence;
                if (!tecmo_cli_parse_render_frame_suffix(mode_name, prefix, &frame) ||
                    frame > teardown_frames) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
                    runtime->start_game_menu_state.frame = 40U + frame;
                    runtime->start_game_menu_state.popup_phase = popup_phase;
                    runtime->start_game_menu_state.phase = frame < teardown_frames
                        ? TECMO_START_GAME_MENU_POPUP_TEARDOWN : TECMO_START_GAME_MENU_ROOT;
                    runtime->start_game_menu_state.transition_frame = (uint16_t)frame;
                    runtime->start_game_menu_state.direction_cooldown = frame < teardown_frames
                        ? runtime->start_game_menu_asset.accepted_input_seed
                        : (uint16_t)(runtime->start_game_menu_asset.accepted_input_seed - 1U);
                    runtime->start_game_menu_state.cursor_delay = frame < teardown_frames ? 0U :
                        runtime->start_game_menu_asset.cursor_commit_delay_frames;
                    runtime->start_game_menu_state.root_selection = popup_phase == TECMO_START_GAME_MENU_MUSIC
                        ? 6U : popup_phase == TECMO_START_GAME_MENU_SPEED ? 4U : 5U;
                    runtime->start_game_menu_state.setting_selection = popup_phase == TECMO_START_GAME_MENU_MUSIC
                        ? runtime->start_game_menu_state.music_value :
                        popup_phase == TECMO_START_GAME_MENU_SPEED
                            ? runtime->start_game_menu_state.speed_value
                            : runtime->start_game_menu_state.period_index;
                }
            } else if (strncmp(mode_name, "start-game-menu-exit-root-frame", 31) == 0 ||
                       strncmp(mode_name, "start-game-menu-exit-season-frame", 33) == 0) {
                *handled_out = true;
                bool from_season = strncmp(mode_name,
                                           "start-game-menu-exit-season-frame", 33) == 0;
                const char *prefix = from_season ? "start-game-menu-exit-season-frame"
                                                 : "start-game-menu-exit-root-frame";
                unsigned frame;
                if (!tecmo_cli_parse_render_frame_suffix(mode_name, prefix, &frame) ||
                    frame >= runtime->start_game_menu_asset.exit_handoff_frame) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
                    runtime->start_game_menu_state.frame = from_season ? 64U + frame : 32U + frame;
                    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_EXIT;
                    runtime->start_game_menu_state.transition_frame = (uint16_t)frame;
                    runtime->start_game_menu_state.pending_action = from_season
                        ? TECMO_START_GAME_MENU_ACTION_PLAY_SETUP
                        : TECMO_START_GAME_MENU_ACTION_ROSTERS;
                    runtime->start_game_menu_state.exit_from_season = from_season;
                    runtime->start_game_menu_state.root_selection = from_season ? 1U : 3U;
                    runtime->start_game_menu_state.season_selection = from_season ? 2U : 0U;
                    runtime->start_game_menu_state.direction_cooldown =
                        runtime->start_game_menu_asset.accepted_input_seed;
                    runtime->start_game_menu_state.slide_frame = from_season
                        ? runtime->start_game_menu_asset.slide_frames : 0U;
                }
            } else if (strcmp(mode_name, "start-game-menu-music") == 0 ||
                       strcmp(mode_name, "start-game-menu-speed") == 0 ||
                       strcmp(mode_name, "start-game-menu-period") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
                runtime->start_game_menu_state.frame = 32U;
                if (strcmp(mode_name, "start-game-menu-music") == 0) {
                    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_MUSIC;
                    runtime->start_game_menu_state.setting_selection =
                        runtime->start_game_menu_state.music_value;
                } else if (strcmp(mode_name, "start-game-menu-speed") == 0) {
                    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_SPEED;
                    runtime->start_game_menu_state.setting_selection =
                        runtime->start_game_menu_state.speed_value;
                } else {
                    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_PERIOD;
                    runtime->start_game_menu_state.setting_selection =
                        runtime->start_game_menu_state.period_index;
                }
}

    state->arena_render_succeeded = arena_render_succeeded;
    state->result = result;
    return render_runtime;
}

static bool configure_preseason_mode(TecmoRuntime *runtime, const char *mode_name, TecmoCliRenderModeState *state, bool *handled_out)
{
    uint32_t *pixels = state->pixels;
    const int width = state->width;
    const int height = state->height;
    TecmoFramebuffer framebuffer = {0};
    bool arena_render_succeeded = state->arena_render_succeeded;
    bool render_runtime = true;
    int result = state->result;

    *handled_out = false;
            if (strcmp(mode_name, "preseason-control") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_PRESEASON_MENU);
                runtime->preseason_state.phase = TECMO_PRESEASON_CONTROL;
                runtime->preseason_state.transition_frame =
                    runtime->preseason_asset.overlays[0].height;
            } else if (strncmp(mode_name, "preseason-control-setup-frame", 29) == 0) {
                *handled_out = true;
                unsigned frame;
                unsigned overlay_height = runtime->preseason_asset.overlays[0].height;
                if (!tecmo_cli_parse_render_frame_suffix(mode_name,
                                               "preseason-control-setup-frame",
                                               &frame) || frame > overlay_height) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_PRESEASON_MENU);
                    runtime->preseason_state.phase = frame < overlay_height
                        ? TECMO_PRESEASON_CONTROL_SETUP : TECMO_PRESEASON_CONTROL;
                    runtime->preseason_state.transition_frame = (uint16_t)frame;
                    runtime->preseason_state.cursor_delay = frame < overlay_height ? 0U :
                        runtime->preseason_asset.cursor_commit_delay_frames;
                }
            } else if (strcmp(mode_name, "preseason-difficulty") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_PRESEASON_MENU);
                runtime->preseason_state.phase = TECMO_PRESEASON_DIFFICULTY;
                runtime->preseason_state.transition_frame =
                    runtime->preseason_asset.overlays[2].height;
                runtime->preseason_state.difficulty_selection = 1U;
                runtime->preseason_state.committed_difficulty = 1U;
            } else if (strcmp(mode_name, "preseason-division") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_PRESEASON_MENU);
                runtime->preseason_state.phase = TECMO_PRESEASON_DIVISION;
                runtime->preseason_state.transition_frame =
                    runtime->preseason_asset.overlays[1].height;
                runtime->preseason_state.control_selection = 2U;
            } else if (strncmp(mode_name, "preseason-team-entry-frame", 26) == 0) {
                *handled_out = true;
                unsigned frame;
                unsigned ready = runtime->preseason_asset.team_input_ready_frames;
                if (!tecmo_cli_parse_render_frame_suffix(mode_name,
                                               "preseason-team-entry-frame",
                                               &frame) || frame > ready) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_PRESEASON_MENU);
                    runtime->preseason_state.phase = frame < ready
                        ? TECMO_PRESEASON_TEAM_ENTRY : TECMO_PRESEASON_TEAM;
                    runtime->preseason_state.transition_frame = (uint16_t)frame;
                    runtime->preseason_state.control_selection = 2U;
                    runtime->preseason_state.team_palette_frame = frame < ready
                        ? 0U : (uint8_t)ready;
                    runtime->preseason_state.cursor_delay = frame < ready ? 0U :
                        runtime->preseason_asset.cursor_commit_delay_frames;
                }
            } else if (strncmp(mode_name, "preseason-p1-team-frame", 23) == 0) {
                *handled_out = true;
                unsigned frame;
                if (!tecmo_cli_parse_render_frame_suffix(mode_name, "preseason-p1-team-frame",
                                               &frame) ||
                    frame < runtime->preseason_asset.team_input_ready_frames ||
                    frame > runtime->preseason_asset.team_palette_full_frames) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_PRESEASON_MENU);
                    runtime->preseason_state.phase = TECMO_PRESEASON_TEAM;
                    runtime->preseason_state.transition_frame =
                        runtime->preseason_asset.team_input_ready_frames;
                    runtime->preseason_state.control_selection = 2U;
                    runtime->preseason_state.team_palette_frame = (uint8_t)frame;
                }
            } else if (strncmp(mode_name, "preseason-team-exit-frame", 25) == 0) {
                *handled_out = true;
                unsigned frame;
                if (!tecmo_cli_parse_render_frame_suffix(mode_name, "preseason-team-exit-frame",
                                               &frame) ||
                    frame > runtime->preseason_asset.team_exit_frames) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_PRESEASON_MENU);
                    runtime->preseason_state.phase = TECMO_PRESEASON_TEAM_EXIT;
                    runtime->preseason_state.transition_frame = (uint16_t)frame;
                    runtime->preseason_state.control_selection = 2U;
                    runtime->preseason_state.team_palette_frame =
                        runtime->preseason_asset.team_palette_full_frames;
                }
            } else if (strncmp(mode_name, "preseason-p2-division-return-frame", 34) == 0) {
                *handled_out = true;
                unsigned frame;
                if (!tecmo_cli_parse_render_frame_suffix(mode_name,
                                               "preseason-p2-division-return-frame",
                                               &frame) ||
                    frame > runtime->preseason_asset.division_return_full_frame) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_PRESEASON_MENU);
                    runtime->preseason_state.phase = TECMO_PRESEASON_DIVISION;
                    runtime->preseason_state.transition_frame =
                        runtime->preseason_asset.overlays[1].height;
                    runtime->preseason_state.control_selection = 2U;
                    runtime->preseason_state.active_player = 1U;
                    runtime->preseason_state.team_selection[1] = 1U;
                    runtime->preseason_state.division_return_fade_frame = (uint8_t)frame;
                    runtime->preseason_state.division_return_fade_active =
                        frame < runtime->preseason_asset.division_return_full_frame;
                }
            } else if (strcmp(mode_name, "preseason-p2-team") == 0 ||
                       strncmp(mode_name, "preseason-p2-team-frame", 23) == 0) {
                *handled_out = true;
                unsigned frame = runtime->preseason_asset.team_palette_full_frames;
                if (strcmp(mode_name, "preseason-p2-team") != 0 &&
                    (!tecmo_cli_parse_render_frame_suffix(mode_name, "preseason-p2-team-frame",
                                                &frame) ||
                     frame < runtime->preseason_asset.team_input_ready_frames ||
                     frame > runtime->preseason_asset.team_palette_full_frames)) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_PRESEASON_MENU);
                    runtime->preseason_state.phase = TECMO_PRESEASON_TEAM;
                    runtime->preseason_state.transition_frame =
                        runtime->preseason_asset.team_input_ready_frames;
                    runtime->preseason_state.control_selection = 2U;
                    runtime->preseason_state.active_player = 1U;
                    runtime->preseason_state.team_selection[0] = 0U;
                    runtime->preseason_state.team_selection[1] = 1U;
                    runtime->preseason_state.team_palette_frame = (uint8_t)frame;
                }
            } else if (strcmp(mode_name, "preseason-invalid-state") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_PRESEASON_MENU);
                runtime->preseason_state.active_player = 2U;
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                arena_render_succeeded = tecmo_preseason_draw(
                    &framebuffer, &runtime->preseason_asset, &runtime->preseason_state,
                    &runtime->start_game_menu_asset, runtime->title_chr_bytes,
                    runtime->title_chr_byte_count, 64, 0, 2);
                render_runtime = false;
                result = arena_render_succeeded ? 0 : 1;
}

    state->arena_render_succeeded = arena_render_succeeded;
    state->result = result;
    return render_runtime;
}

static bool configure_all_star_mode(TecmoRuntime *runtime, const char *mode_name, TecmoCliRenderModeState *state, bool *handled_out)
{
    uint32_t *pixels = state->pixels;
    const int width = state->width;
    const int height = state->height;
    TecmoFramebuffer framebuffer = {0};
    bool arena_render_succeeded = state->arena_render_succeeded;
    bool render_runtime = true;
    int result = state->result;

    *handled_out = false;
            if (strcmp(mode_name, "all-star-control") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_ALL_STAR_MENU);
                runtime->all_star_state.phase = TECMO_ALL_STAR_CONTROL;
                runtime->all_star_state.transition_frame = 14U;
            } else if (strncmp(mode_name, "all-star-control-setup-frame", 28) == 0) {
                *handled_out = true;
                unsigned frame;
                if (!tecmo_cli_parse_render_frame_suffix(mode_name,
                                               "all-star-control-setup-frame",
                                               &frame) || frame > 14U) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_ALL_STAR_MENU);
                    runtime->all_star_state.phase = frame < 14U
                        ? TECMO_ALL_STAR_CONTROL_SETUP : TECMO_ALL_STAR_CONTROL;
                    runtime->all_star_state.transition_frame = (uint16_t)frame;
                    runtime->all_star_state.cursor_delay = frame < 14U ? 0U :
                        runtime->all_star_asset.cursor_commit_delay_frames;
                }
            } else if (strncmp(mode_name, "all-star-control-teardown-frame", 31) == 0) {
                *handled_out = true;
                unsigned frame;
                if (!tecmo_cli_parse_render_frame_suffix(
                        mode_name, "all-star-control-teardown-frame", &frame) ||
                    frame > 14U) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_ALL_STAR_MENU);
                    runtime->all_star_state.phase = frame < 14U
                        ? TECMO_ALL_STAR_CONTROL_TEARDOWN_ROOT
                        : TECMO_ALL_STAR_CONTROL;
                    runtime->all_star_state.transition_frame = (uint16_t)frame;
                    runtime->all_star_state.cursor_delay = frame < 14U ? 0U :
                        runtime->all_star_asset.cursor_commit_delay_frames;
                }
            } else if (strcmp(mode_name, "all-star-difficulty") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_ALL_STAR_MENU);
                runtime->all_star_state.phase = TECMO_ALL_STAR_DIFFICULTY;
                runtime->all_star_state.transition_frame = 8U;
                runtime->all_star_state.difficulty_selection = 1U;
                runtime->all_star_state.committed_difficulty = 1U;
            } else if (strcmp(mode_name, "all-star-team-east") == 0 ||
                       strcmp(mode_name, "all-star-team-west") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_ALL_STAR_MENU);
                runtime->all_star_state.phase = TECMO_ALL_STAR_TEAM;
                runtime->all_star_state.transition_frame = 6U;
                runtime->all_star_state.control_selection =
                    strcmp(mode_name, "all-star-team-west") == 0 ? 4U : 1U;
                runtime->all_star_state.team_selection =
                    strcmp(mode_name, "all-star-team-west") == 0 ? 1U : 0U;
            } else if (strcmp(mode_name, "all-star-invalid-state") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_ALL_STAR_MENU);
                runtime->all_star_state.control_selection =
                    TECMO_ALL_STAR_CONTROL_COUNT;
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                arena_render_succeeded = tecmo_all_star_draw(
                    &framebuffer, &runtime->all_star_asset,
                    &runtime->all_star_state, &runtime->preseason_asset,
                    &runtime->start_game_menu_asset,
                    runtime->title_chr_bytes, runtime->title_chr_byte_count,
                    64, 0, 2);
                render_runtime = false;
                result = arena_render_succeeded ? 0 : 1;
}

    state->arena_render_succeeded = arena_render_succeeded;
    state->result = result;
    return render_runtime;
}

static bool configure_team_data_mode(TecmoRuntime *runtime, const char *mode_name, TecmoCliRenderModeState *state, bool *handled_out)
{
    uint32_t *pixels = state->pixels;
    const int width = state->width;
    const int height = state->height;
    TecmoFramebuffer framebuffer = {0};
    bool arena_render_succeeded = state->arena_render_succeeded;
    bool render_runtime = true;
    int result = state->result;

    *handled_out = false;
            if (strcmp(mode_name, "team-data-select") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_TEAM_DATA);
                runtime->team_data_state.cursor_delay = 1U;
            } else if (strcmp(mode_name, "team-data-profile") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_TEAM_DATA);
                runtime->team_data_state.phase = TECMO_TEAM_DATA_PROFILE;
                runtime->team_data_state.team_id = 0U;
                runtime->team_data_state.cursor_delay = 1U;
            } else if (strcmp(mode_name, "team-data-starters") == 0 ||
                       strcmp(mode_name, "team-data-starters-reset") == 0 ||
                       strcmp(mode_name, "team-data-starters-bench") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_TEAM_DATA);
                runtime->team_data_state.phase = TECMO_TEAM_DATA_STARTERS;
                runtime->team_data_state.team_id = 0U;
                tecmo_team_management_view_init_starters(
                    &runtime->team_data_state.management_view);
                if (strcmp(mode_name, "team-data-starters-reset") == 0) {
                    runtime->team_data_state.management_view.view =
                        TECMO_TEAM_MANAGEMENT_VIEW_STARTER_RESET;
                } else if (strcmp(mode_name, "team-data-starters-bench") == 0) {
                    runtime->team_data_state.management_view.view =
                        TECMO_TEAM_MANAGEMENT_VIEW_STARTER_BENCH;
                    runtime->team_data_state.management_view.selection = 1U;
                }
            } else if (strcmp(mode_name, "team-data-playbook") == 0 ||
                       strcmp(mode_name, "team-data-playbook-reset") == 0 ||
                       strncmp(mode_name, "team-data-playbook-replace-frame", 32) == 0) {
                *handled_out = true;
                unsigned frame = 0U;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_TEAM_DATA);
                runtime->team_data_state.phase = TECMO_TEAM_DATA_PLAYBOOK;
                runtime->team_data_state.team_id = 0U;
                tecmo_team_management_view_init_playbook(
                    &runtime->team_data_state.management_view);
                if (strcmp(mode_name, "team-data-playbook-reset") == 0) {
                    runtime->team_data_state.management_view.view =
                        TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK_RESET;
                } else if (strncmp(mode_name,
                                   "team-data-playbook-replace-frame", 32) == 0) {
                    if (!tecmo_cli_parse_render_frame_suffix(
                            mode_name, "team-data-playbook-replace-frame", &frame) ||
                        frame > runtime->team_management_asset.carousel_frames) {
                        printf("Unsupported render-test mode: %s\n", mode_name);
                        render_runtime = false;
                    } else {
                        runtime->team_data_state.management_view.view =
                            TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK_REPLACE;
                        runtime->team_data_state.management_view.carousel_direction =
                            frame < runtime->team_management_asset.carousel_frames
                                ? 1 : 0;
                        runtime->team_data_state.management_view.carousel_frame =
                            (uint8_t)frame;
                        runtime->team_data_state.management_view.carousel_origin =
                            frame == runtime->team_management_asset.carousel_frames
                                ? 1U : 0U;
                    }
                }
            } else if (strcmp(mode_name, "team-data-roster-page1") == 0 ||
                       strcmp(mode_name, "team-data-roster-page2") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_TEAM_DATA);
                runtime->team_data_state.phase = TECMO_TEAM_DATA_ROSTER;
                runtime->team_data_state.team_id = 0U;
                runtime->team_data_state.roster_page =
                    strcmp(mode_name, "team-data-roster-page2") == 0 ? 1U : 0U;
                runtime->team_data_state.cursor_delay = 1U;
            } else if (strncmp(mode_name, "team-data-roster-slide-frame", 28) == 0) {
                *handled_out = true;
                unsigned frame;
                if (!tecmo_cli_parse_render_frame_suffix(mode_name,
                                               "team-data-roster-slide-frame",
                                               &frame) ||
                    frame > runtime->team_data_asset.slide_frames) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_TEAM_DATA);
                    runtime->team_data_state.phase = TECMO_TEAM_DATA_ROSTER;
                    runtime->team_data_state.team_id = 0U;
                    runtime->team_data_state.slide_from_page = 0U;
                    runtime->team_data_state.slide_to_page = 1U;
                    runtime->team_data_state.slide_direction = 1;
                    runtime->team_data_state.slide_frame = (uint8_t)frame;
                }
            } else if (strcmp(mode_name, "team-data-player-detail") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_TEAM_DATA);
                runtime->team_data_state.phase = TECMO_TEAM_DATA_PLAYER_DETAIL;
                runtime->team_data_state.team_id = 0U;
                runtime->team_data_state.player_index = 0U;
                runtime->team_data_state.cursor_delay = 1U;
            } else if (strncmp(mode_name,
                               "team-data-entry-transition-frame", 32) == 0) {
                *handled_out = true;
                unsigned frame;
                if (!tecmo_cli_parse_render_frame_suffix(
                        mode_name, "team-data-entry-transition-frame", &frame) ||
                    frame >
                        runtime->team_data_asset.entry_transition_stable_frame) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_TEAM_DATA);
                    runtime->team_data_state.transition =
                        TECMO_TEAM_DATA_TRANSITION_ENTRY_TO_SELECTOR;
                    runtime->team_data_state.transition_frame = (uint8_t)frame;
                }
            } else if (strncmp(mode_name,
                               "team-data-selector-profile-transition-frame",
                               43) == 0) {
                *handled_out = true;
                unsigned frame;
                if (!tecmo_cli_parse_render_frame_suffix(
                        mode_name,
                        "team-data-selector-profile-transition-frame",
                        &frame) ||
                    frame >
                        runtime->team_data_asset.selector_transition_stable_frame) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_TEAM_DATA);
                    runtime->team_data_state.phase = TECMO_TEAM_DATA_TEAM_SELECT;
                    runtime->team_data_state.selector_index = 2U;
                    runtime->team_data_state.team_id = 0U;
                    runtime->team_data_state.transition =
                        TECMO_TEAM_DATA_TRANSITION_SELECTOR_TO_PROFILE;
                    runtime->team_data_state.transition_frame = (uint8_t)frame;
                }
            } else if (strncmp(mode_name,
                               "team-data-roster-detail-transition-frame",
                               40) == 0) {
                *handled_out = true;
                unsigned frame;
                if (!tecmo_cli_parse_render_frame_suffix(
                        mode_name,
                        "team-data-roster-detail-transition-frame", &frame) ||
                    frame >
                        runtime->team_data_asset.detail_transition_stable_frame) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_TEAM_DATA);
                    runtime->team_data_state.phase = TECMO_TEAM_DATA_ROSTER;
                    runtime->team_data_state.team_id = 0U;
                    runtime->team_data_state.player_index = 0U;
                    runtime->team_data_state.transition =
                        TECMO_TEAM_DATA_TRANSITION_ROSTER_TO_DETAIL;
                    runtime->team_data_state.transition_frame = (uint8_t)frame;
                }
            } else if (strncmp(mode_name,
                               "team-data-detail-roster-transition-frame",
                               40) == 0) {
                *handled_out = true;
                unsigned frame;
                if (!tecmo_cli_parse_render_frame_suffix(
                        mode_name,
                        "team-data-detail-roster-transition-frame", &frame) ||
                    frame >
                        runtime->team_data_asset.selector_transition_stable_frame) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_TEAM_DATA);
                    runtime->team_data_state.phase =
                        TECMO_TEAM_DATA_PLAYER_DETAIL;
                    runtime->team_data_state.team_id = 0U;
                    runtime->team_data_state.player_index = 0U;
                    runtime->team_data_state.transition =
                        TECMO_TEAM_DATA_TRANSITION_DETAIL_TO_ROSTER;
                    runtime->team_data_state.transition_frame = (uint8_t)frame;
                }
            } else if (strcmp(mode_name, "team-data-invalid-state") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_TEAM_DATA);
                runtime->team_data_state.team_id = TECMO_TEAM_DATA_TEAM_COUNT;
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                arena_render_succeeded = tecmo_team_data_draw(
                    &framebuffer, &runtime->team_data_asset,
                    &runtime->team_data_state,
                    &runtime->team_management_asset,
                    &runtime->team_management_session,
                    runtime->title_chr_bytes,
                    runtime->title_chr_byte_count, 64, 0, 2);
                render_runtime = false;
                result = arena_render_succeeded ? 0 : 1;
}

    state->arena_render_succeeded = arena_render_succeeded;
    state->result = result;
    return render_runtime;
}

static bool configure_season_mode(TecmoRuntime *runtime, const char *mode_name, TecmoCliRenderModeState *state, bool *handled_out)
{
    uint32_t *pixels = state->pixels;
    const int width = state->width;
    const int height = state->height;
    TecmoFramebuffer framebuffer = {0};
    bool arena_render_succeeded = state->arena_render_succeeded;
    bool render_runtime = true;
    int result = state->result;

    *handled_out = false;
            if (strcmp(mode_name, "season-team-control") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_SEASON_MENU);
                tecmo_season_state_init(&runtime->season_state,
                                        TECMO_SEASON_ROUTE_TEAM_CONTROL,
                                        &runtime->season_session);
            } else if (strcmp(mode_name, "season-schedule") == 0 ||
                       strcmp(mode_name, "season-schedule-popup") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_SEASON_MENU);
                tecmo_season_state_init(&runtime->season_state,
                                        TECMO_SEASON_ROUTE_SCHEDULE,
                                        &runtime->season_session);
                if (strcmp(mode_name, "season-schedule-popup") == 0)
                    runtime->season_state.phase = TECMO_SEASON_SCHEDULE_POPUP;
                if (strcmp(mode_name, "season-schedule-popup") == 0)
                    runtime->season_state.popup_rows_visible =
                        runtime->season_asset.menu_boxes[0][1];
            } else if (strcmp(mode_name, "season-playoff") == 0 ||
                       strcmp(mode_name, "season-playoff-mid") == 0 ||
                       strcmp(mode_name, "season-playoff-east") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_SEASON_MENU);
                tecmo_season_state_init(&runtime->season_state,
                                        TECMO_SEASON_ROUTE_SCHEDULE,
                                        &runtime->season_session);
                runtime->season_state.phase = TECMO_SEASON_PLAYOFF;
                if (strcmp(mode_name, "season-playoff-mid") == 0)
                    runtime->season_state.playoff_scroll = 128U;
                else if (strcmp(mode_name, "season-playoff-east") == 0)
                    runtime->season_state.playoff_scroll = 252U;
            } else if (strcmp(mode_name, "season-standings-east") == 0 ||
                       strcmp(mode_name, "season-standings-west") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_SEASON_MENU);
                tecmo_season_state_init(&runtime->season_state,
                                        TECMO_SEASON_ROUTE_STANDINGS,
                                        &runtime->season_session);
                runtime->season_state.standings_page =
                    strcmp(mode_name, "season-standings-west") == 0 ? 1U : 0U;
            } else if (strcmp(mode_name, "season-standings-programmed") == 0) {
                *handled_out = true;
                runtime->season_session.season_type = TECMO_SEASON_PROGRAMMED;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_SEASON_MENU);
                tecmo_season_state_init(&runtime->season_state,
                                        TECMO_SEASON_ROUTE_STANDINGS,
                                        &runtime->season_session);
            } else if (strcmp(mode_name,
                               "season-leaders-results-populated-page6") == 0 ||
                       strcmp(mode_name,
                              "season-leaders-results-populated-page12") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_SEASON_MENU);
                tecmo_season_state_init(&runtime->season_state,
                                        TECMO_SEASON_ROUTE_LEADERS,
                                        &runtime->season_session);
                seed_populated_leader_results(&runtime->season_session);
                runtime->season_state.leader_category = 0U;
                runtime->season_state.leader_page =
                    strcmp(mode_name,
                           "season-leaders-results-populated-page12") == 0
                        ? 12U : 6U;
                runtime->season_state.leaders_results = true;
            } else if (strncmp(mode_name,
                               "season-leaders-results-populated", 32) == 0) {
                *handled_out = true;
                unsigned category = 0U;
                if (mode_name[32] != '\0' &&
                    (!tecmo_cli_parse_render_frame_suffix(
                        mode_name, "season-leaders-results-populated",
                        &category) ||
                     (category != 0U && category != 3U &&
                      category != 5U && category != 6U))) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                    goto season_mode_configured;
                }
                tecmo_runtime_set_mode(runtime, TECMO_MODE_SEASON_MENU);
                tecmo_season_state_init(&runtime->season_state,
                                        TECMO_SEASON_ROUTE_LEADERS,
                                        &runtime->season_session);
                seed_populated_leader_results(&runtime->season_session);
                runtime->season_state.leader_category = (uint8_t)category;
                runtime->season_state.leaders_results = true;
            } else if (strcmp(mode_name, "season-leaders-results") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_SEASON_MENU);
                tecmo_season_state_init(&runtime->season_state,
                                        TECMO_SEASON_ROUTE_LEADERS,
                                        &runtime->season_session);
                runtime->season_state.leaders_results = true;
            } else if (strncmp(mode_name, "season-leaders", 14) == 0) {
                *handled_out = true;
                unsigned category = 0U;
                if (mode_name[14] != '\0' &&
                    (!tecmo_cli_parse_render_frame_suffix(mode_name, "season-leaders",
                                                &category) || category >= 7U)) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_SEASON_MENU);
                    tecmo_season_state_init(&runtime->season_state,
                                            TECMO_SEASON_ROUTE_LEADERS,
                                            &runtime->season_session);
                    runtime->season_state.leader_category = (uint8_t)category;
                }
            } else if (strcmp(mode_name, "season-game-start") == 0) {
                *handled_out = true;
                TecmoControlFrame no_input;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_SEASON_MENU);
                tecmo_season_state_init(&runtime->season_state,
                                        TECMO_SEASON_ROUTE_GAME_START,
                                        &runtime->season_session);
                memset(&no_input, 0, sizeof(no_input));
                (void)tecmo_season_update(&runtime->season_state,
                                          &runtime->season_asset,
                                          &runtime->season_session,
                                          &no_input);
            } else if (strcmp(mode_name, "season-invalid-state") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_SEASON_MENU);
                tecmo_season_state_init(&runtime->season_state,
                                        TECMO_SEASON_ROUTE_TEAM_CONTROL,
                                        &runtime->season_session);
                runtime->season_state.team_selection = TECMO_SEASON_TEAM_COUNT;
                memset(pixels, 0,
                       (size_t)width * (size_t)height * sizeof(*pixels));
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                arena_render_succeeded = tecmo_season_draw(
                    &framebuffer, &runtime->season_asset,
                    &runtime->season_session, &runtime->season_state,
                    &runtime->team_data_asset, runtime->title_chr_bytes,
                    runtime->title_chr_byte_count, 64, 0, 2);
                render_runtime = false;
                result = arena_render_succeeded ? 0 : 1;
}

season_mode_configured:
    state->arena_render_succeeded = arena_render_succeeded;
    state->result = result;
    return render_runtime;
}

bool tecmo_cli_configure_render_frontend_mode(TecmoRuntime *runtime, const char *mode_name, TecmoCliRenderModeState *state, bool *handled_out)
{
    bool handled;
    bool result;

    result = configure_start_game_menu_mode(runtime, mode_name, state, &handled);
    if (handled) { *handled_out = true; return result; }
    result = configure_preseason_mode(runtime, mode_name, state, &handled);
    if (handled) { *handled_out = true; return result; }
    result = configure_all_star_mode(runtime, mode_name, state, &handled);
    if (handled) { *handled_out = true; return result; }
    result = configure_team_data_mode(runtime, mode_name, state, &handled);
    if (handled) { *handled_out = true; return result; }
    result = configure_season_mode(runtime, mode_name, state, &handled);
    if (handled) { *handled_out = true; return result; }
    *handled_out = false;
    return true;
}
