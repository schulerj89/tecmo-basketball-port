#include "asm_inventory.h"
#include "png_writer.h"
#include "tecmo_asset_pack.h"
#include "asset_pack/tecmo_asset_pack_gameplay_audio.h"
#include "asset_pack/tecmo_asset_pack_gameplay_camera.h"
#include "asset_pack/tecmo_asset_pack_gameplay_movement.h"
#include "asset_pack/tecmo_asset_pack_gameplay_ball_dribble.h"
#include "asset_pack/tecmo_asset_pack_gameplay_fatigue.h"
#include "asset_pack/tecmo_asset_pack_gameplay_cpu_steering.h"
#include "asset_pack/tecmo_asset_pack_gameplay_hud.h"
#include "asset_pack/tecmo_asset_pack_gameplay_court_orientation.h"
#include "asset_pack/tecmo_asset_pack_gameplay_backcourt.h"
#include "asset_pack/tecmo_asset_pack_gameplay_free_throw_lineup.h"
#include "asset_pack/tecmo_asset_pack_gameplay_violation_referee.h"
#include "asset_pack/tecmo_asset_pack_music.h"
#include "tecmo_audio_output.h"
#include "tecmo_bank07.h"
#include "tecmo_game.h"
#include "tecmo_frontend_audio.h"
#include "tecmo_gameplay_audio.h"
#include "tecmo_gameplay_assets.h"
#include "tecmo_gameplay_camera.h"
#include "tecmo_gameplay_movement.h"
#include "tecmo_gameplay_ball_dribble.h"
#include "tecmo_gameplay_fatigue.h"
#include "tecmo_gameplay_cpu_steering.h"
#include "tecmo_gameplay_hud.h"
#include "tecmo_gameplay_court.h"
#include "tecmo_gameplay_court_orientation.h"
#include "tecmo_gameplay_backcourt.h"
#include "tecmo_gameplay_close_shots.h"
#include "tecmo_gameplay_dunk_cutaway.h"
#include "tecmo_gameplay_jump_shots.h"
#include "tecmo_gameplay_shot_resolution.h"
#include "tecmo_gameplay_penalties.h"
#include "tecmo_gameplay_violation_referee.h"
#include "tecmo_gameplay_free_throw_lineup.h"
#include "tecmo_gameplay_free_throw_projection_test.h"
#include "tecmo_gameplay_scene.h"
#include "tecmo_gameplay_state.h"
#include "tecmo_intro_arena_scene.h"
#include "tecmo_nes_video.h"
#include "tecmo_win32_keys.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tecmo_cli_internal.h"

bool tecmo_cli_validate_render_asset_contract(
    const TecmoRuntime *runtime, const char *mode_name)
{
    int result = 0;
                if ((strncmp(mode_name, "title-confirm-frame", 19) == 0 ||
                     strncmp(mode_name, "title-attract-frame", 19) == 0 ||
                     strcmp(mode_name, "title-screen") == 0 ||
                     strcmp(mode_name, "boot-title") == 0) &&
                    (!runtime->title_asset.attract_available ||
                     !runtime->title_asset.start_available ||
                     !tecmo_title_asset_chr_available(&runtime->title_asset,
                                                       runtime->title_chr_bytes,
                                                       runtime->title_chr_byte_count))) {
                    result = 1;
                } else if (strncmp(mode_name, "start-game-menu", 15) == 0 &&
                           (!runtime->start_game_menu_asset.available ||
                            !tecmo_start_game_menu_asset_chr_available(
                                &runtime->start_game_menu_asset,
                                runtime->title_chr_bytes,
                                runtime->title_chr_byte_count) ||
                            (runtime->start_game_menu_state.frame < 8U &&
                             (!runtime->title_asset.start_available ||
                              !tecmo_title_asset_chr_available(
                                  &runtime->title_asset,
                                  runtime->title_chr_bytes,
                                  runtime->title_chr_byte_count))))) {
                    result = 1;
                } else if (strncmp(mode_name, "preseason", 9) == 0 &&
                           (!runtime->preseason_asset.available ||
                            !runtime->start_game_menu_asset.available ||
                            !tecmo_preseason_asset_chr_available(
                                &runtime->preseason_asset,
                                runtime->title_chr_bytes,
                                runtime->title_chr_byte_count))) {
                    result = 1;
                } else if (strncmp(mode_name, "all-star", 8) == 0 &&
                           (!runtime->all_star_asset.available ||
                            !runtime->preseason_asset.available ||
                            !runtime->start_game_menu_asset.available ||
                            !tecmo_all_star_asset_chr_available(
                                &runtime->all_star_asset,
                                runtime->title_chr_bytes,
                                runtime->title_chr_byte_count))) {
                    result = 1;
                } else if (strncmp(mode_name, "team-data", 9) == 0 &&
                           (!runtime->team_data_asset.available ||
                            !tecmo_team_data_asset_chr_available(
                                &runtime->team_data_asset,
                                runtime->title_chr_bytes,
                                runtime->title_chr_byte_count) ||
                            ((runtime->team_data_state.phase ==
                                  TECMO_TEAM_DATA_STARTERS ||
                              runtime->team_data_state.phase ==
                                  TECMO_TEAM_DATA_PLAYBOOK) &&
                             (!runtime->team_management_asset.available ||
                              !tecmo_team_management_session_valid(
                                  &runtime->team_management_session) ||
                              !tecmo_team_management_asset_chr_available(
                                  &runtime->team_management_asset,
                                  runtime->title_chr_bytes,
                                  runtime->title_chr_byte_count))))) {
                    result = 1;
                } else if (strncmp(mode_name, "season-", 7) == 0 &&
                           (!runtime->season_asset.available ||
                            !runtime->team_data_asset.available ||
                            !tecmo_season_asset_chr_available(
                                &runtime->season_asset,
                                runtime->title_chr_bytes,
                                runtime->title_chr_byte_count))) {
                    result = 1;
                } else if ((strcmp(mode_name, "play") == 0 ||
                     strncmp(mode_name, "play-fade", 9) == 0 ||
                     strcmp(mode_name, "play-step6") == 0) &&
                    (!runtime->intro_presents_asset.available ||
                     !tecmo_intro_screen_chr_available(&runtime->intro_presents_asset,
                                                       runtime->title_chr_bytes,
                                                       runtime->title_chr_byte_count))) {
                    result = 1;
                } else if (strcmp(mode_name, "play-step7") == 0 &&
                           (!runtime->intro_license_asset.available ||
                            !tecmo_intro_screen_chr_available(&runtime->intro_license_asset,
                                                              runtime->title_chr_bytes,
                                                              runtime->title_chr_byte_count))) {
                    result = 1;
                }
    return result == 0;
}

void tecmo_cli_print_render_diagnostics(
    const TecmoRuntime *runtime, const char *mode_name,
    bool arena_render_succeeded)
{
            if (strncmp(mode_name, "start-game-menu", 15) == 0) {
                printf("start-game-menu-state frame=%u phase=%s root=%u season=%u slide=%u setting=%u transition=%u rows=%u palette=%u cursor=%u cursor-delay=%u cooldown=%u pending=%u\n",
                       runtime->start_game_menu_state.frame,
                       tecmo_start_game_menu_phase_name(runtime->start_game_menu_state.phase),
                       (unsigned)runtime->start_game_menu_state.root_selection,
                       (unsigned)runtime->start_game_menu_state.season_selection,
                       (unsigned)runtime->start_game_menu_state.slide_frame,
                       (unsigned)runtime->start_game_menu_state.setting_selection,
                       (unsigned)runtime->start_game_menu_state.transition_frame,
                       tecmo_start_game_menu_overlay_visible_rows(
                           &runtime->start_game_menu_asset, &runtime->start_game_menu_state),
                       tecmo_start_game_menu_palette_stage(
                           &runtime->start_game_menu_asset, &runtime->start_game_menu_state),
                       tecmo_start_game_menu_cursor_visible(
                           &runtime->start_game_menu_asset, &runtime->start_game_menu_state) ? 1U : 0U,
                       (unsigned)runtime->start_game_menu_state.cursor_delay,
                       (unsigned)runtime->start_game_menu_state.direction_cooldown,
                       (unsigned)runtime->start_game_menu_state.pending_action);
            }
            if (strncmp(mode_name, "preseason", 9) == 0) {
                printf("preseason-state phase=%s transition=%u control=%u difficulty=%u committed=%u active-player=%u divisions=%u/%u teams=%u/%u palette=%u return-fade=%u/%u cooldown=%u\n",
                       tecmo_preseason_phase_name(runtime->preseason_state.phase),
                       (unsigned)runtime->preseason_state.transition_frame,
                       (unsigned)runtime->preseason_state.control_selection,
                       (unsigned)runtime->preseason_state.difficulty_selection,
                       (unsigned)runtime->preseason_state.committed_difficulty,
                       (unsigned)runtime->preseason_state.active_player,
                       (unsigned)runtime->preseason_state.division_selection[0],
                       (unsigned)runtime->preseason_state.division_selection[1],
                       (unsigned)runtime->preseason_state.team_selection[0],
                       (unsigned)runtime->preseason_state.team_selection[1],
                       (unsigned)runtime->preseason_state.team_palette_frame,
                       runtime->preseason_state.division_return_fade_active ? 1U : 0U,
                       (unsigned)runtime->preseason_state.division_return_fade_frame,
                       (unsigned)runtime->preseason_state.direction_cooldown);
            }
            if (strncmp(mode_name, "all-star", 8) == 0) {
                printf("all-star-state phase=%s transition=%u control=%u difficulty=%u committed=%u team=%u owners=%u/%u teams=%u/%u terminal=%u cooldown=%u rows=%u/%u/%u\n",
                       tecmo_all_star_phase_name(runtime->all_star_state.phase),
                       (unsigned)runtime->all_star_state.transition_frame,
                       (unsigned)runtime->all_star_state.control_selection,
                       (unsigned)runtime->all_star_state.difficulty_selection,
                       (unsigned)runtime->all_star_state.committed_difficulty,
                       (unsigned)runtime->all_star_state.team_selection,
                       (unsigned)runtime->all_star_state.west_owner,
                       (unsigned)runtime->all_star_state.east_owner,
                       (unsigned)runtime->all_star_state.west_team,
                       (unsigned)runtime->all_star_state.east_team,
                       runtime->all_star_state.terminal_commit ? 1U : 0U,
                       (unsigned)runtime->all_star_state.direction_cooldown,
                       tecmo_all_star_overlay_visible_rows(
                           &runtime->all_star_asset, &runtime->all_star_state, 0U),
                       tecmo_all_star_overlay_visible_rows(
                           &runtime->all_star_asset, &runtime->all_star_state, 1U),
                       tecmo_all_star_overlay_visible_rows(
                           &runtime->all_star_asset, &runtime->all_star_state, 2U));
            }
            if (strncmp(mode_name, "team-data", 9) == 0) {
                printf("team-data-state phase=%s selector=%u team=%u profile=%u page=%u row=%u player=%u slide=%u/%u direction=%d cooldown=%u transition=%u transition-frame=%u palette=%u render=%u\n",
                       tecmo_team_data_phase_name(runtime->team_data_state.phase),
                       (unsigned)runtime->team_data_state.selector_index,
                       (unsigned)runtime->team_data_state.team_id,
                       (unsigned)runtime->team_data_state.profile_selection,
                       (unsigned)runtime->team_data_state.roster_page,
                       (unsigned)runtime->team_data_state.roster_row,
                       (unsigned)runtime->team_data_state.player_index,
                       (unsigned)runtime->team_data_state.slide_frame,
                       (unsigned)runtime->team_data_asset.slide_frames,
                       (int)runtime->team_data_state.slide_direction,
                       (unsigned)runtime->team_data_state.direction_cooldown,
                       (unsigned)runtime->team_data_state.transition,
                       (unsigned)runtime->team_data_state.transition_frame,
                       tecmo_team_data_transition_palette_stage(
                           &runtime->team_data_asset,
                           &runtime->team_data_state),
                       tecmo_team_data_transition_render_enabled(
                           &runtime->team_data_asset,
                           &runtime->team_data_state) ? 1U : 0U);
            }
            if (strncmp(mode_name, "season-", 7) == 0) {
                printf("season-state phase=%s type=%s schedule=%u team=%u popup=%u popup-rows=%u playoff-scroll=%u page=%u panel=%u editor-team=%u leader=%u leader-result=%u game-results=%u/%u game-pending=%u launch-blocked=%u save=%u\n",
                       tecmo_season_phase_name(runtime->season_state.phase),
                       tecmo_season_type_name(runtime->season_session.season_type),
                       (unsigned)runtime->season_state.schedule_selection,
                       (unsigned)runtime->season_state.team_selection,
                       (unsigned)runtime->season_state.popup_selection,
                       (unsigned)runtime->season_state.popup_rows_visible,
                       (unsigned)runtime->season_state.playoff_scroll,
                       (unsigned)runtime->season_state.standings_page,
                       (unsigned)runtime->season_state.editor_panel,
                       (unsigned)runtime->season_state.editor_team,
                       (unsigned)runtime->season_state.leader_category,
                       runtime->season_state.leaders_results ? 1U : 0U,
                       (unsigned)runtime->season_state.game_result_visible_rows,
                       (unsigned)(runtime->season_state.game_result_count * 2U),
                       runtime->season_state.game_result_pending ? 1U : 0U,
                       runtime->season_state.game_launch_blocked ? 1U : 0U,
                       (unsigned)runtime->season_session.save_status);
            }
            tecmo_cli_print_intro_render_capture_status(runtime, mode_name, arena_render_succeeded);
}
