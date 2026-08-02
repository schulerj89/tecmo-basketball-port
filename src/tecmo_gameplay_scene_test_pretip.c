#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "tecmo_gameplay_scene_test_internal.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool tecmo_gameplay_scene_test_pretip_load(
    TecmoGameplaySceneTestContext *test,
    TecmoGameplayScene *scene)
{
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

    tecmo_gameplay_scene_init(scene);
    if (!tecmo_gameplay_scene_load(scene, project_root, asset_pack_path,
                                   music_player)) {
        tecmo_gameplay_scene_test_message(message, message_size, scene->status);
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    if (!scene->shot_resolution.available ||
        scene->shot_resolution.outcome_flag_mask !=
            scene->jump_shots.constants.outcome_flag_mask ||
        scene->shot_resolution.gameplay_core_fingerprint !=
            scene->jump_shots.gameplay_core_fingerprint) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "TGSR-3 scene dependency contract failed");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    if (!scene->camera_assets.available ||
        !scene->cpu_steering_assets.available ||
        scene->cpu_steering_assets.storage_size !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SIZE ||
        scene->cpu_steering_assets.movement_fingerprint !=
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_FNV1A32 ||
        scene->camera_assets.storage_size !=
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SIZE ||
        scene->camera_assets.gameplay_core_fingerprint != 0x2047CCE0U ||
        scene->camera_assets.gameplay_court_fingerprint != 0xECAB7A93U ||
        scene->court_world.contract_tag !=
            TECMO_GAMEPLAY_COURT_WORLD_CONTRACT_TAG ||
        scene->court_world.width_tiles !=
            TECMO_GAMEPLAY_COURT_WORLD_WIDTH_TILES ||
        scene->court_world.height_tiles !=
            TECMO_GAMEPLAY_COURT_WORLD_HEIGHT_TILES ||
        scene->court_world.width_pixels !=
            TECMO_GAMEPLAY_COURT_WORLD_WIDTH_PIXELS ||
        scene->court_world.height_pixels !=
            TECMO_GAMEPLAY_COURT_WORLD_HEIGHT_PIXELS ||
        scene->court_world.tiles_fingerprint !=
            TECMO_GAMEPLAY_COURT_WORLD_TILES_FNV1A32 ||
        scene->court_world.palette_indices_fingerprint !=
            TECMO_GAMEPLAY_COURT_WORLD_PALETTES_FNV1A32) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "TGCP-2/TGCT-1 live dependency contract failed");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    return true;
}

static bool tecmo_gameplay_scene_test_pretip_initial_launch(
    TecmoGameplaySceneTestContext *test,
    TecmoGameplayScene *scene,
    TecmoGameplaySceneLaunch *launch,
    TecmoControlFrame *p1,
    TecmoControlFrame *p2,
    TecmoGameplayPreTipLineup *tip_lineup)
{
    char *message = test->message;
    size_t message_size = test->message_size;
    size_t frame;

    memset(launch, 0, sizeof(*launch));
    launch->source = TECMO_GAMEPLAY_SCENE_PRESEASON;
    launch->away_team = 0U;
    launch->home_team = 1U;
    launch->regulation_minutes = 2U;
    launch->difficulty = 1U;
    launch->control_mode = 1U;
    launch->speed_value = 1U;
    launch->controller_team[0] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch->controller_team[1] = TECMO_GAMEPLAY_TEAM_HOME;
    launch->game_music_enabled = true;
    launch->home_team = launch->away_team;
    if (tecmo_gameplay_scene_launch(scene, launch) || scene->active) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "invalid gameplay launch was accepted");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    launch->home_team = 1U;
    if (!tecmo_gameplay_scene_launch(scene, launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "gameplay pre-tip launch rejected");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    memset(p1, 0, sizeof(*p1));
    memset(p2, 0, sizeof(*p2));
    if (!tecmo_gameplay_pretip_tip_lineup(
            &scene->pretip_assets, tip_lineup) ||
        tip_lineup->contract_tag != TECMO_GAMEPLAY_PRETIP_LINEUP_TAG ||
        !tecmo_gameplay_scene_in_pretip(scene) ||
        scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_PRESEASON ||
        scene->pretip_state.card_cancel_enabled ||
        scene->state.clock_minutes != 2U || scene->state.clock_seconds != 0U ||
        scene->state.shot_clock != 24U ||
        !scene->audio_player.music->track_pending ||
        scene->audio_player.music->pending_track_id !=
            TECMO_MUSIC_TRACK_PREGAME_MATCHUP_STINGER ||
        scene->ball_position.x_q8 !=
            (int32_t)tip_lineup->ball.x * 256 ||
        scene->ball_position.y_q8 !=
            (int32_t)tip_lineup->ball.y * 256) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pre-tip freeze/track-8 launch contract failed");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    for (frame = 0U; frame < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++frame) {
        if (scene->actors[frame].position.x !=
                tip_lineup->players[frame].x ||
            scene->actors[frame].position.y !=
                tip_lineup->players[frame].y ||
            scene->actors[frame].anchor.x !=
                tip_lineup->players[frame].x ||
            scene->actors[frame].anchor.y !=
                tip_lineup->players[frame].y ||
            scene->actors[frame].pose_index !=
                tip_lineup->player_pose_indices[frame] ||
            scene->actors[frame].sprite_slot_base !=
                tip_lineup->player_sprite_slot_bases[frame] ||
            !scene->actors[frame].pose_orientation_encoded) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "ROM tip-off player lineup contract failed");
            tecmo_gameplay_scene_destroy(scene);
            return false;
        }
    }
    p1->held.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, p1, p2) ||
        !scene->active || scene->pretip_abort_pending ||
        scene->pretip_state.aborted ||
        scene->pretip_state.phase_frame != 1U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "preseason NES-B ignore contract failed");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    p1->held.cancel = false;
    tecmo_gameplay_scene_end(scene);
    if (!tecmo_gameplay_scene_launch(scene, launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "preseason pre-tip relaunch rejected");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    return true;
}

static bool tecmo_gameplay_scene_test_pretip_contest_input_regression(
    TecmoGameplaySceneTestContext *test,
    TecmoGameplayScene *scene)
{
    TecmoGameplayPreTipState state;
    size_t frame;
    if (!tecmo_gameplay_pretip_state_initialize(
            &scene->pretip_assets, &state, false)) {
        tecmo_gameplay_scene_test_message(
            test->message, test->message_size,
            "pre-tip contest regression state initialization failed");
        return false;
    }
    for (frame = 0U; frame < 661U; ++frame) {
        if (!tecmo_gameplay_pretip_update(
                &scene->pretip_assets, &state, false, false)) {
            tecmo_gameplay_scene_test_message(
                test->message, test->message_size,
                "pre-tip contest regression advance failed");
            return false;
        }
    }
    if (state.phase != TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST ||
        state.phase_frame != 0U ||
        !tecmo_gameplay_pretip_update(
            &scene->pretip_assets, &state, false, true) ||
        !state.home_tip_sampled || state.home_tip_sample_frame != 0U ||
        state.home_tip_error != 0U) {
        tecmo_gameplay_scene_test_message(
            test->message, test->message_size,
            "pre-tip JUMP_CONTEST held Home B regression failed");
        return false;
    }
    return true;
}

static bool tecmo_gameplay_scene_test_pretip_descent_live(
    TecmoGameplaySceneTestContext *test,
    TecmoGameplayScene *scene,
    TecmoControlFrame *p1,
    TecmoControlFrame *p2)
{
    char *message = test->message;
    size_t message_size = test->message_size;
    size_t frame;

    for (frame = 0U; frame < 481U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, p1, p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "pre-tip descent entry update rejected");
            tecmo_gameplay_scene_destroy(scene);
            return false;
        }
    }
    if (scene->pretip_state.phase !=
            TECMO_GAMEPLAY_PRETIP_BALL_DESCENT ||
        scene->pretip_state.phase_frame != 0U ||
        scene->pretip_state.total_frame != 481U ||
        scene->ball_position.y_q8 !=
            TECMO_GAMEPLAY_PRETIP_DESCENT_START_Y * 256 ||
        scene->state.clock_minutes != 2U || scene->state.clock_seconds != 0U ||
        scene->state.shot_clock != 24U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pre-tip descent entry/freeze contract failed");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    for (frame = 0U; frame < 30U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, p1, p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "pre-tip descent midpoint update rejected");
            tecmo_gameplay_scene_destroy(scene);
            return false;
        }
    }
    if (scene->pretip_state.phase_frame != 30U ||
        scene->ball_position.y_q8 < 107 * 256 ||
        scene->ball_position.y_q8 > 109 * 256) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pre-tip descent midpoint bounds failed");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    for (frame = 0U; frame < 30U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, p1, p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "pre-tip descent endpoint update rejected");
            tecmo_gameplay_scene_destroy(scene);
            return false;
        }
    }
    if (scene->pretip_state.phase_frame != 60U ||
        scene->ball_position.y_q8 !=
            TECMO_GAMEPLAY_PRETIP_DESCENT_END_Y * 256) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pre-tip descent endpoint clamp failed");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    for (frame = 0U; frame < 59U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, p1, p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "pre-tip descent hold update rejected");
            tecmo_gameplay_scene_destroy(scene);
            return false;
        }
    }
    if (scene->pretip_state.phase_frame != 119U ||
        scene->ball_position.y_q8 !=
            TECMO_GAMEPLAY_PRETIP_DESCENT_END_Y * 256) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pre-tip descent hold contract failed");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    for (frame = 600U; frame < 691U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, p1, p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "pre-tip live handoff update rejected");
            tecmo_gameplay_scene_destroy(scene);
            return false;
        }
    }
    if (tecmo_gameplay_scene_in_pretip(scene) ||
        scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_LIVE ||
        !scene->pretip_state.live_handoff ||
        scene->pretip_state.total_frame != 691U ||
        scene->frame != 691U ||
        scene->state.clock_minutes != 2U || scene->state.clock_seconds != 0U ||
        scene->state.shot_clock != 24U ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene->ball_holder != 0U ||
        !scene->audio_player.music->track_pending ||
        scene->audio_player.music->pending_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pre-tip 691-frame track-8-to-5 handoff failed");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    tecmo_gameplay_scene_end(scene);
    return true;
}

static bool tecmo_gameplay_scene_test_pretip_abort_and_timing(
    TecmoGameplaySceneTestContext *test,
    TecmoGameplayScene *scene,
    TecmoGameplaySceneLaunch *launch,
    TecmoControlFrame *p1,
    TecmoControlFrame *p2)
{
    char *message = test->message;
    size_t message_size = test->message_size;
    size_t frame;

    launch->source = TECMO_GAMEPLAY_SCENE_SEASON;
    if (!tecmo_gameplay_scene_launch(scene, launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "gameplay pre-tip abort relaunch rejected");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    p1->held.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, p1, p2) ||
        scene->active || !scene->pretip_abort_pending ||
        !scene->pretip_state.card_cancel_enabled ||
        !tecmo_gameplay_scene_consume_pretip_abort(scene) ||
        tecmo_gameplay_scene_consume_pretip_abort(scene) ||
        scene->result_ready ||
        scene->state.clock_minutes != 2U || scene->state.clock_seconds != 0U ||
        scene->state.shot_clock != 24U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pre-tip NES-B abort contract failed");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    tecmo_gameplay_scene_end(scene);
    launch->source = TECMO_GAMEPLAY_SCENE_PRESEASON;
    launch->controller_team[0] = TECMO_GAMEPLAY_TEAM_HOME;
    launch->controller_team[1] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    p1->held.cancel = false;
    if (!tecmo_gameplay_scene_launch(scene, launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pre-tip timing relaunch rejected");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    for (frame = 0U; frame < 437U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, p1, p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "pre-tip timing advance rejected");
            tecmo_gameplay_scene_destroy(scene);
            return false;
        }
    }
    p1->held.cancel = false;
    p1->released.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, p1, p2) ||
        scene->pretip_state.home_tip_sampled ||
        scene->pretip_state.away_tip_sampled ||
        scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_CLOSEUP ||
        scene->pretip_state.phase_frame != 195U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pre-tip close-up B/release ignore failed");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    p1->held.cancel = false;
    p1->released.cancel = false;
    for (frame = 438U; frame < 661U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, p1, p2)) {
            tecmo_gameplay_scene_test_message(
                message, message_size, "pre-tip contest entry rejected");
            tecmo_gameplay_scene_destroy(scene);
            return false;
        }
    }
    p1->held.cancel = false;
    p2->held.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, p1, p2) ||
        scene->pretip_state.home_tip_sampled ||
        scene->pretip_state.home_tip_sample_frame !=
            TECMO_GAMEPLAY_PRETIP_NO_SAMPLE_FRAME ||
        scene->pretip_state.away_tip_sampled) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pre-tip unassigned contest input was accepted");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    p2->held.cancel = false;
    p1->pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, p1, p2) ||
        scene->pretip_state.home_tip_sampled ||
        scene->pretip_state.home_tip_sample_frame !=
            TECMO_GAMEPLAY_PRETIP_NO_SAMPLE_FRAME ||
        scene->pretip_state.away_tip_sampled) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "pre-tip pressed-only contest input was accepted");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    p1->pressed.cancel = false;
    p1->held.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, p1, p2) ||
        !scene->pretip_state.home_tip_sampled ||
        scene->pretip_state.home_tip_sample_frame != 2U ||
        scene->pretip_state.home_tip_error != 2U ||
        scene->pretip_state.away_tip_sampled) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pre-tip home contest timing sample rejected");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    for (frame = 664U; frame < 691U; ++frame) {
        p1->held.cancel = true;
        if (!tecmo_gameplay_scene_update(scene, p1, p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "pre-tip timing handoff rejected");
            tecmo_gameplay_scene_destroy(scene);
            return false;
        }
    }
    p1->held.cancel = false;
    if (scene->state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        scene->ball_holder != TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        scene->orientation_state.current_direction != 1U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pre-tip timing possession contract failed");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    tecmo_gameplay_scene_end(scene);

    launch->controller_team[0] = TECMO_GAMEPLAY_TEAM_HOME;
    launch->controller_team[1] = TECMO_GAMEPLAY_TEAM_AWAY;
    if (!tecmo_gameplay_scene_launch(scene, launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pre-tip reversed-assignment launch rejected");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    for (frame = 0U; frame < 661U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, p1, p2)) {
            tecmo_gameplay_scene_test_message(
                message, message_size,
                "pre-tip reversed-assignment contest entry rejected");
            tecmo_gameplay_scene_destroy(scene);
            return false;
        }
    }
    p1->held.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, p1, p2) ||
        !scene->pretip_state.home_tip_sampled ||
        scene->pretip_state.away_tip_sampled ||
        scene->pretip_state.home_tip_sample_frame != 0U) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "pre-tip reversed Home controller routing failed");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    p1->held.cancel = false;
    tecmo_gameplay_scene_end(scene);

    if (!tecmo_gameplay_scene_launch(scene, launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pre-tip reversed-away launch rejected");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    for (frame = 0U; frame < 661U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, p1, p2)) {
            tecmo_gameplay_scene_test_message(
                message, message_size,
                "pre-tip reversed-away contest entry rejected");
            tecmo_gameplay_scene_destroy(scene);
            return false;
        }
    }
    p1->held.cancel = false;
    p2->held.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, p1, p2) ||
        !scene->pretip_state.away_tip_sampled ||
        scene->pretip_state.home_tip_sampled ||
        scene->pretip_state.away_tip_sample_frame != 0U) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "pre-tip reversed Away controller routing failed");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    p2->held.cancel = false;
    tecmo_gameplay_scene_end(scene);
    return true;
}

bool tecmo_gameplay_scene_test_pretip(
    TecmoGameplaySceneTestContext *test)
{
    TecmoGameplaySceneLaunch launch = test->launch;
    TecmoControlFrame p1 = test->p1;
    TecmoControlFrame p2 = test->p2;
    TecmoGameplayScene *scene = test->scene;
    TecmoGameplayPreTipLineup tip_lineup;

    tecmo_gameplay_scene_test_set_skip_pretip(false);
    if (!tecmo_gameplay_scene_test_pretip_load(test, scene) ||
        !tecmo_gameplay_scene_test_pretip_contest_input_regression(
            test, scene) ||
        !tecmo_gameplay_scene_test_pretip_initial_launch(
            test, scene, &launch, &p1, &p2, &tip_lineup) ||
        !tecmo_gameplay_scene_test_pretip_descent_live(
            test, scene, &p1, &p2) ||
        !tecmo_gameplay_scene_test_pretip_abort_and_timing(
            test, scene, &launch, &p1, &p2)) {
        return false;
    }
    test->launch = launch;
    test->p1 = p1;
    test->p2 = p2;
    return true;
}
