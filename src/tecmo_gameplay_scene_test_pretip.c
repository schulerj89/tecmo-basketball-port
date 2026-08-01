#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "tecmo_gameplay_scene_test_internal.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool tecmo_gameplay_scene_test_pretip(
    TecmoGameplaySceneTestContext *test)
{
    TecmoGameplaySceneLaunch launch = test->launch;
    TecmoControlFrame p1 = test->p1;
    TecmoControlFrame p2 = test->p2;
    char *message = test->message;
    size_t message_size = test->message_size;
    const char *project_root = test->project_root;
    const char *asset_pack_path = test->asset_pack_path;
    TecmoMusicPlayer *music_player = test->music_player;
    TecmoGameplayScene missing_scene;
    TecmoGameplaySceneCourtSlice court_slice;
    TecmoGameplaySceneCourtSlice unchanged_court_slice;
    TecmoGameplaySceneCourtFrame court_frame;
    TecmoGameplaySceneCourtFrame unchanged_court_frame;
    TecmoGameplayPreTipLineup tip_lineup;
    size_t frame;

#define TEST_SCENE (*test->scene)
    tecmo_gameplay_scene_test_set_skip_pretip(false);

    tecmo_gameplay_scene_init(&missing_scene);
    if (tecmo_gameplay_scene_load(&missing_scene, project_root,
                                  "?:\\missing-gameplay.assetpack",
                                  music_player) || missing_scene.available) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "missing gameplay pack was accepted");
        tecmo_gameplay_scene_destroy(&missing_scene);
        return false;
    }
    memset(&court_slice, 0xA5, sizeof(court_slice));
    unchanged_court_slice = court_slice;
    if (tecmo_gameplay_scene_court_slice(
            &missing_scene, &court_slice) ||
        tecmo_gameplay_scene_court_slice(NULL, &court_slice) ||
        tecmo_gameplay_scene_court_slice(&missing_scene, NULL) ||
        memcmp(&court_slice, &unchanged_court_slice,
               sizeof(court_slice)) != 0) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "unavailable TGCT scene slice mutated output");
        tecmo_gameplay_scene_destroy(&missing_scene);
        return false;
    }
    memset(&court_frame, 0xA5, sizeof(court_frame));
    unchanged_court_frame = court_frame;
    if (tecmo_gameplay_scene_court_frame(
            &missing_scene, &court_frame) ||
        tecmo_gameplay_scene_court_frame(NULL, &court_frame) ||
        tecmo_gameplay_scene_court_frame(&missing_scene, NULL) ||
        memcmp(&court_frame, &unchanged_court_frame,
               sizeof(court_frame)) != 0) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "unavailable camera-coherent court frame mutated output");
        tecmo_gameplay_scene_destroy(&missing_scene);
        return false;
    }
    tecmo_gameplay_scene_destroy(&missing_scene);
    tecmo_gameplay_scene_destroy(&missing_scene);

    tecmo_gameplay_scene_init(&TEST_SCENE);
    if (!tecmo_gameplay_scene_load(&TEST_SCENE, project_root, asset_pack_path,
                                   music_player)) {
        tecmo_gameplay_scene_test_message(message, message_size, TEST_SCENE.status);
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    if (!TEST_SCENE.shot_resolution.available ||
        TEST_SCENE.shot_resolution.outcome_flag_mask !=
            TEST_SCENE.jump_shots.constants.outcome_flag_mask ||
        TEST_SCENE.shot_resolution.gameplay_core_fingerprint !=
            TEST_SCENE.jump_shots.gameplay_core_fingerprint) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "TGSR-3 scene dependency contract failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    if (!TEST_SCENE.camera_assets.available ||
        !TEST_SCENE.cpu_steering_assets.available ||
        TEST_SCENE.cpu_steering_assets.storage_size !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SIZE ||
        TEST_SCENE.cpu_steering_assets.movement_fingerprint !=
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_FNV1A32 ||
        TEST_SCENE.camera_assets.storage_size !=
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SIZE ||
        TEST_SCENE.camera_assets.gameplay_core_fingerprint != 0x2047CCE0U ||
        TEST_SCENE.camera_assets.gameplay_court_fingerprint != 0xECAB7A93U ||
        TEST_SCENE.court_world.contract_tag !=
            TECMO_GAMEPLAY_COURT_WORLD_CONTRACT_TAG ||
        TEST_SCENE.court_world.width_tiles !=
            TECMO_GAMEPLAY_COURT_WORLD_WIDTH_TILES ||
        TEST_SCENE.court_world.height_tiles !=
            TECMO_GAMEPLAY_COURT_WORLD_HEIGHT_TILES ||
        TEST_SCENE.court_world.width_pixels !=
            TECMO_GAMEPLAY_COURT_WORLD_WIDTH_PIXELS ||
        TEST_SCENE.court_world.height_pixels !=
            TECMO_GAMEPLAY_COURT_WORLD_HEIGHT_PIXELS ||
        TEST_SCENE.court_world.tiles_fingerprint !=
            TECMO_GAMEPLAY_COURT_WORLD_TILES_FNV1A32 ||
        TEST_SCENE.court_world.palette_indices_fingerprint !=
            TECMO_GAMEPLAY_COURT_WORLD_PALETTES_FNV1A32) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "TGCP-2/TGCT-1 live dependency contract failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    memset(&launch, 0, sizeof(launch));
    launch.source = TECMO_GAMEPLAY_SCENE_PRESEASON;
    launch.away_team = 0U;
    launch.home_team = 1U;
    launch.regulation_minutes = 2U;
    launch.difficulty = 1U;
    launch.control_mode = 1U;
    launch.speed_value = 1U;
    launch.controller_team[0] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch.controller_team[1] = TECMO_GAMEPLAY_TEAM_HOME;
    launch.game_music_enabled = true;
    launch.home_team = launch.away_team;
    if (tecmo_gameplay_scene_launch(&TEST_SCENE, &launch) || TEST_SCENE.active) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "invalid gameplay launch was accepted");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    launch.home_team = 1U;
    if (!tecmo_gameplay_scene_launch(&TEST_SCENE, &launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "gameplay pre-tip launch rejected");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_pretip_tip_lineup(
            &TEST_SCENE.pretip_assets, &tip_lineup) ||
        tip_lineup.contract_tag != TECMO_GAMEPLAY_PRETIP_LINEUP_TAG ||
        !tecmo_gameplay_scene_in_pretip(&TEST_SCENE) ||
        TEST_SCENE.pretip_state.phase != TECMO_GAMEPLAY_PRETIP_PRESEASON ||
        TEST_SCENE.pretip_state.card_cancel_enabled ||
        TEST_SCENE.state.clock_minutes != 2U || TEST_SCENE.state.clock_seconds != 0U ||
        TEST_SCENE.state.shot_clock != 24U ||
        !TEST_SCENE.audio_player.music->track_pending ||
        TEST_SCENE.audio_player.music->pending_track_id !=
            TECMO_MUSIC_TRACK_PREGAME_MATCHUP_STINGER ||
        TEST_SCENE.ball_position.x_q8 !=
            (int32_t)tip_lineup.ball.x * 256 ||
        TEST_SCENE.ball_position.y_q8 !=
            (int32_t)tip_lineup.ball.y * 256) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pre-tip freeze/track-8 launch contract failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    for (frame = 0U; frame < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++frame) {
        if (TEST_SCENE.actors[frame].position.x !=
                tip_lineup.players[frame].x ||
            TEST_SCENE.actors[frame].position.y !=
                tip_lineup.players[frame].y ||
            TEST_SCENE.actors[frame].anchor.x !=
                tip_lineup.players[frame].x ||
            TEST_SCENE.actors[frame].anchor.y !=
                tip_lineup.players[frame].y ||
            TEST_SCENE.actors[frame].pose_index !=
                tip_lineup.player_pose_indices[frame] ||
            TEST_SCENE.actors[frame].sprite_slot_base !=
                tip_lineup.player_sprite_slot_bases[frame] ||
            !TEST_SCENE.actors[frame].pose_orientation_encoded) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "ROM tip-off player lineup contract failed");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    p1.held.cancel = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        !TEST_SCENE.active || TEST_SCENE.pretip_abort_pending ||
        TEST_SCENE.pretip_state.aborted ||
        TEST_SCENE.pretip_state.phase_frame != 1U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "preseason NES-B ignore contract failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    p1.held.cancel = false;
    tecmo_gameplay_scene_end(&TEST_SCENE);
    if (!tecmo_gameplay_scene_launch(&TEST_SCENE, &launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "preseason pre-tip relaunch rejected");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    for (frame = 0U; frame < 481U; ++frame) {
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "pre-tip descent entry update rejected");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    if (TEST_SCENE.pretip_state.phase !=
            TECMO_GAMEPLAY_PRETIP_BALL_DESCENT ||
        TEST_SCENE.pretip_state.phase_frame != 0U ||
        TEST_SCENE.pretip_state.total_frame != 481U ||
        TEST_SCENE.ball_position.y_q8 !=
            TECMO_GAMEPLAY_PRETIP_DESCENT_START_Y * 256 ||
        TEST_SCENE.state.clock_minutes != 2U || TEST_SCENE.state.clock_seconds != 0U ||
        TEST_SCENE.state.shot_clock != 24U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pre-tip descent entry/freeze contract failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    for (frame = 0U; frame < 30U; ++frame) {
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "pre-tip descent midpoint update rejected");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    if (TEST_SCENE.pretip_state.phase_frame != 30U ||
        TEST_SCENE.ball_position.y_q8 < 107 * 256 ||
        TEST_SCENE.ball_position.y_q8 > 109 * 256) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pre-tip descent midpoint bounds failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    for (frame = 0U; frame < 30U; ++frame) {
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "pre-tip descent endpoint update rejected");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    if (TEST_SCENE.pretip_state.phase_frame != 60U ||
        TEST_SCENE.ball_position.y_q8 !=
            TECMO_GAMEPLAY_PRETIP_DESCENT_END_Y * 256) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pre-tip descent endpoint clamp failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    for (frame = 0U; frame < 59U; ++frame) {
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "pre-tip descent hold update rejected");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    if (TEST_SCENE.pretip_state.phase_frame != 119U ||
        TEST_SCENE.ball_position.y_q8 !=
            TECMO_GAMEPLAY_PRETIP_DESCENT_END_Y * 256) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pre-tip descent hold contract failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    for (frame = 600U; frame < 691U; ++frame) {
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "pre-tip live handoff update rejected");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    if (tecmo_gameplay_scene_in_pretip(&TEST_SCENE) ||
        TEST_SCENE.pretip_state.phase != TECMO_GAMEPLAY_PRETIP_LIVE ||
        !TEST_SCENE.pretip_state.live_handoff ||
        TEST_SCENE.pretip_state.total_frame != 691U ||
        TEST_SCENE.frame != 691U ||
        TEST_SCENE.state.clock_minutes != 2U || TEST_SCENE.state.clock_seconds != 0U ||
        TEST_SCENE.state.shot_clock != 24U ||
        TEST_SCENE.state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        TEST_SCENE.ball_holder != 0U ||
        !TEST_SCENE.audio_player.music->track_pending ||
        TEST_SCENE.audio_player.music->pending_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pre-tip 691-frame track-8-to-5 handoff failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_scene_end(&TEST_SCENE);
    launch.source = TECMO_GAMEPLAY_SCENE_SEASON;
    if (!tecmo_gameplay_scene_launch(&TEST_SCENE, &launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "gameplay pre-tip abort relaunch rejected");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    p1.held.cancel = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.active || !TEST_SCENE.pretip_abort_pending ||
        !TEST_SCENE.pretip_state.card_cancel_enabled ||
        !tecmo_gameplay_scene_consume_pretip_abort(&TEST_SCENE) ||
        tecmo_gameplay_scene_consume_pretip_abort(&TEST_SCENE) ||
        TEST_SCENE.result_ready ||
        TEST_SCENE.state.clock_minutes != 2U || TEST_SCENE.state.clock_seconds != 0U ||
        TEST_SCENE.state.shot_clock != 24U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pre-tip NES-B abort contract failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_scene_end(&TEST_SCENE);
    launch.source = TECMO_GAMEPLAY_SCENE_PRESEASON;
    launch.controller_team[0] = TECMO_GAMEPLAY_TEAM_HOME;
    launch.controller_team[1] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    p1.held.cancel = false;
    if (!tecmo_gameplay_scene_launch(&TEST_SCENE, &launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pre-tip timing relaunch rejected");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    for (frame = 0U; frame < 437U; ++frame) {
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "pre-tip timing advance rejected");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    p1.held.cancel = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        !TEST_SCENE.pretip_state.home_tip_sampled ||
        TEST_SCENE.pretip_state.home_tip_error != 0U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pre-tip home timing sample rejected");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    p1.held.cancel = false;
    for (frame = 438U; frame < 691U; ++frame) {
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "pre-tip timing handoff rejected");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    if (TEST_SCENE.state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        TEST_SCENE.ball_holder != TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        TEST_SCENE.orientation_state.current_direction != 1U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pre-tip timing possession contract failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_scene_end(&TEST_SCENE);
#undef TEST_SCENE
    test->launch = launch;
    test->p1 = p1;
    test->p2 = p2;
    return true;
}
