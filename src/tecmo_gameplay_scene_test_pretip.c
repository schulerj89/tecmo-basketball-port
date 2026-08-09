#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "tecmo_gameplay_scene_test_internal.h"
#include "tecmo_game.h"
#include "tecmo_win32_keys.h"

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
        {
            bool expected_facing;
            if (!scene_goal_facing_right_for_team(
                    scene, (TecmoGameplayTeam)scene->actors[frame].team,
                    &expected_facing) ||
                scene->actors[frame].facing_right != expected_facing) {
                tecmo_gameplay_scene_test_message(
                    message, message_size,
                    "pre-tip encoded pose retained invalid goal facing");
                tecmo_gameplay_scene_destroy(scene);
                return false;
            }
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

static bool tecmo_gameplay_scene_test_pretip_real_time_presentation_regression(
    TecmoGameplaySceneTestContext *test,
    TecmoGameplayScene *scene)
{
    static const uint64_t native_hz_num = 39375000ULL;
    static const uint64_t native_hz_den = 655171ULL;
    TecmoGameplaySceneLaunch launch;
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    size_t frame;

    memset(&launch, 0, sizeof(launch));
    launch.source = TECMO_GAMEPLAY_SCENE_PRESEASON;
    launch.away_team = 0U;
    launch.home_team = 1U;
    launch.regulation_minutes = 2U;
    launch.difficulty = 1U;
    launch.control_mode = 1U;
    launch.speed_value = 1U;
    launch.controller_team[0U] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch.controller_team[1U] = TECMO_GAMEPLAY_TEAM_HOME;
    launch.game_music_enabled = false;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_scene_launch(scene, &launch)) {
        tecmo_gameplay_scene_test_message(
            test->message, test->message_size,
            "real-time tip presentation regression launch rejected");
        return false;
    }
    for (frame = 0U; frame < 661U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) {
            tecmo_gameplay_scene_test_message(
                test->message, test->message_size,
                "real-time tip presentation contest entry rejected");
            tecmo_gameplay_scene_end(scene);
            return false;
        }
    }
    /* Supply a valid human capture inside the bounded input window; the
       separate no-input regression proves the fail-closed stall contract. */
    p1.held.cancel = true;
    for (frame = 0U; frame < 30U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) {
            tecmo_gameplay_scene_test_message(
                test->message, test->message_size,
                "real-time tip presentation contest update rejected");
            tecmo_gameplay_scene_end(scene);
            return false;
        }
    }
    if ((uint64_t)TECMO_GAMEPLAY_PRETIP_PRESENTATION_FRAMES *
            native_hz_den * 4U < 3U * native_hz_num ||
        scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST ||
        scene->pretip_state.phase_frame != 30U ||
        scene->pretip_state.contest_frame !=
            TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES ||
        !scene->pretip_jump_active) {
        tecmo_gameplay_scene_test_message(
            test->message, test->message_size,
            "real-time tip presentation ended with the 30-frame contest "
            "at 60.1 Hz");
        tecmo_gameplay_scene_end(scene);
        return false;
    }
    p1.held.cancel = true;
    for (frame = 0U;
         frame < TECMO_GAMEPLAY_PRETIP_PRESENTATION_FRAMES -
                     TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) {
            tecmo_gameplay_scene_test_message(
                test->message, test->message_size,
                "real-time tip presentation live handoff rejected");
            tecmo_gameplay_scene_end(scene);
            return false;
        }
    }
    if (tecmo_gameplay_scene_in_pretip(scene) ||
        scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_LIVE ||
        !scene->pretip_state.live_handoff ||
        scene->pretip_state.contest_frame !=
            TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES ||
        scene->pretip_state.total_frame != 721U ||
        scene->frame != 721U || scene->pretip_jump_active) {
        tecmo_gameplay_scene_test_message(
            test->message, test->message_size,
            "real-time tip presentation did not hand off coherently");
        tecmo_gameplay_scene_end(scene);
        return false;
    }
    tecmo_gameplay_scene_end(scene);
    return true;
}

static bool scene_test_pretip_draw_logical_resolution(
    const TecmoGameplayScene *scene,
    uint32_t *pixels,
    bool include_actors)
{
    TecmoFramebuffer framebuffer;
    if (scene == NULL || pixels == NULL) return false;
    framebuffer.pixels = pixels;
    framebuffer.width = TECMO_GAMEPLAY_SCENE_NES_WIDTH;
    framebuffer.height = TECMO_GAMEPLAY_SCENE_NES_HEIGHT;
    framebuffer.pitch_pixels = TECMO_GAMEPLAY_SCENE_NES_WIDTH;
    return tecmo_gameplay_scene_draw(
        scene, &framebuffer, 0, 0, 1, include_actors);
}

static size_t scene_test_pretip_actor_pixel_changes(
    const uint32_t *with_actors,
    const uint32_t *without_actors,
    const TecmoGameplayActorProjection *projection)
{
    int left;
    int top;
    int right;
    int bottom;
    int x;
    int y;
    size_t changes = 0U;
    if (with_actors == NULL || without_actors == NULL ||
        projection == NULL || !projection->visible) {
        return 0U;
    }
    left = (int)projection->screen_x - 24;
    top = (int)projection->screen_y - 32;
    right = (int)projection->screen_x + 24;
    bottom = (int)projection->screen_y + 32;
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right >= TECMO_GAMEPLAY_SCENE_NES_WIDTH) {
        right = TECMO_GAMEPLAY_SCENE_NES_WIDTH - 1;
    }
    if (bottom >= TECMO_GAMEPLAY_SCENE_NES_HEIGHT) {
        bottom = TECMO_GAMEPLAY_SCENE_NES_HEIGHT - 1;
    }
    for (y = top; y <= bottom; ++y) {
        for (x = left; x <= right; ++x) {
            size_t pixel = (size_t)y * TECMO_GAMEPLAY_SCENE_NES_WIDTH +
                           (size_t)x;
            if (with_actors[pixel] != without_actors[pixel]) ++changes;
        }
    }
    return changes;
}

static bool scene_test_pretip_jumper_order(
    const TecmoGameplayScene *scene,
    uint8_t *left_actor_out,
    uint8_t *right_actor_out)
{
    uint8_t first;
    uint8_t second;
    if (scene == NULL || left_actor_out == NULL || right_actor_out == NULL) {
        return false;
    }
    first = scene->pretip_jumper_actor[0U];
    second = scene->pretip_jumper_actor[1U];
    if (first >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        second >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT || first == second ||
        scene->actors[first].anchor.x == scene->actors[second].anchor.x) {
        return false;
    }
    if (scene->actors[first].anchor.x < scene->actors[second].anchor.x) {
        *left_actor_out = first;
        *right_actor_out = second;
    } else {
        *left_actor_out = second;
        *right_actor_out = first;
    }
    return true;
}

static bool scene_test_run_late_human_tip(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *launch)
{
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    size_t frame;
    if (scene == NULL || launch == NULL ||
        !tecmo_gameplay_scene_launch(scene, launch)) {
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    for (frame = 0U; frame < 661U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) return false;
    }
    for (frame = 0U; frame < 29U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) return false;
    }
    p1.held.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST ||
        scene->pretip_state.phase_frame != 30U ||
        scene->pretip_state.contest_frame !=
            TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES ||
        !scene->pretip_state.away_tip_sampled ||
        scene->pretip_state.away_tip_sample_frame != 29U ||
        scene->pretip_state.away_tip_error != 11U ||
        scene->pretip_state.home_tip_sampled ||
        !scene->pretip_jump_active) {
        return false;
    }
    p1.held.cancel = false;
    for (frame = 0U;
         frame < TECMO_GAMEPLAY_PRETIP_PRESENTATION_FRAMES -
                     TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) return false;
    }
    if (tecmo_gameplay_scene_in_pretip(scene) ||
        scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_LIVE ||
        !scene->pretip_state.live_handoff ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene->ball_holder != 3U || scene->pretip_jump_active ||
        scene->pretip_jumper_altitude_q8[0U] != 0U ||
        scene->pretip_jumper_altitude_q8[1U] != 0U ||
        scene->frame != 721U) {
        return false;
    }
    return true;
}

static bool scene_test_run_cpu_to_contest(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *launch,
    TecmoControlFrame *p1,
    TecmoControlFrame *p2)
{
    size_t frame;
    if (scene == NULL || launch == NULL || p1 == NULL || p2 == NULL ||
        !tecmo_gameplay_scene_launch(scene, launch)) {
        return false;
    }
    memset(p1, 0, sizeof(*p1));
    memset(p2, 0, sizeof(*p2));
    for (frame = 0U; frame < 661U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, p1, p2) ||
            scene->pretip_state.away_tip_sampled ||
            scene->pretip_state.home_tip_sampled) {
            return false;
        }
    }
    return scene->pretip_state.phase ==
               TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST &&
           scene->pretip_state.phase_frame == 0U &&
           scene->pretip_state.contest_frame == 0U &&
           !scene->pretip_state.away_tip_sampled &&
           !scene->pretip_state.home_tip_sampled &&
           !scene->pretip_jump_active;
}

static bool scene_test_run_cpu_to_decision(
    TecmoGameplayScene *scene,
    TecmoControlFrame *p1,
    TecmoControlFrame *p2)
{
    size_t iterations = 0U;
    if (scene == NULL || p1 == NULL || p2 == NULL ||
        scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST) {
        return false;
    }
    while ((!scene->pretip_state.away_tip_sampled ||
            !scene->pretip_state.home_tip_sampled ||
            (!scene->pretip_state.claim_resolved &&
             !scene->pretip_state.claim_deferred)) &&
           scene->pretip_state.contest_frame <
               TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES) {
        uint32_t total_before;
        if (iterations >= TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES)
            return false;
        total_before = scene->pretip_state.total_frame;
        if (!tecmo_gameplay_scene_update(scene, p1, p2)) return false;
        ++iterations;
        if (scene->pretip_state.total_frame == total_before) return false;
    }
    return scene->pretip_state.away_tip_sampled &&
           scene->pretip_state.home_tip_sampled &&
           (scene->pretip_state.claim_resolved ||
            scene->pretip_state.claim_deferred) &&
           iterations <= TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES &&
           scene->pretip_state.contest_frame ==
               TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES;
}

static bool scene_test_run_cpu_to_live(
    TecmoGameplayScene *scene,
    TecmoControlFrame *p1,
    TecmoControlFrame *p2,
    TecmoGameplayTeam expected_possession)
{
    size_t guard = 0U;
    if (scene == NULL || p1 == NULL || p2 == NULL) return false;
    while (scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_LIVE &&
           guard < TECMO_GAMEPLAY_PRETIP_PRESENTATION_FRAMES) {
        if (!tecmo_gameplay_scene_update(scene, p1, p2)) return false;
        ++guard;
    }
    return scene->pretip_state.phase == TECMO_GAMEPLAY_PRETIP_LIVE &&
           scene->pretip_state.live_handoff &&
           scene->state.possession == expected_possession &&
           !scene->pretip_jump_active &&
           scene->pretip_jumper_altitude_q8[0U] == 0U &&
           scene->pretip_jumper_altitude_q8[1U] == 0U;
}

static bool tecmo_gameplay_scene_test_pretip_cpu_decision_regression(
    TecmoGameplaySceneTestContext *test,
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *launch)
{
    TecmoGameplaySceneLaunch cpu_launch;
    TecmoGameplaySceneCourtProjection initial_projection;
    TecmoGameplaySceneCourtProjection rising_projection;
    TecmoGameplaySceneActor malformed_actors[
        TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayPreTipState malformed_state;
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    uint8_t away_actor;
    uint8_t home_actor;
    uint8_t winner;
    uint16_t initial_away_pose;
    uint16_t initial_home_pose;
    int32_t center_ball_x_q8;
    int32_t previous_ball_x_q8;
    uint32_t frame_before;
    size_t frame;
    const char *failure =
        "pre-tip CPU decision regression failed";

    if (test == NULL || scene == NULL || launch == NULL) return false;
    cpu_launch = *launch;
    cpu_launch.source = TECMO_GAMEPLAY_SCENE_PRESEASON;
    cpu_launch.game_music_enabled = false;

    /* Away human versus Home CPU.  The human sample is earlier, while the
       unassigned Home side gets the deterministic automatic approximation. */
    cpu_launch.controller_team[0U] = TECMO_GAMEPLAY_TEAM_AWAY;
    cpu_launch.controller_team[1U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    if (!scene_test_run_cpu_to_contest(scene, &cpu_launch, &p1, &p2)) {
        failure = "pre-tip Away-human/Home-CPU contest entry failed";
        goto failed;
    }
    away_actor = scene->pretip_jumper_actor[0U];
    home_actor = scene->pretip_jumper_actor[1U];
    if (away_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        home_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        away_actor == home_actor ||
        !tecmo_gameplay_scene_court_projection(scene, &initial_projection) ||
        !initial_projection.players[away_actor].visible ||
        !initial_projection.players[home_actor].visible) {
        failure = "pre-tip CPU visual contest setup was invalid";
        goto failed;
    }
    initial_away_pose = scene->actors[away_actor].pose_index;
    initial_home_pose = scene->actors[home_actor].pose_index;
    if (initial_away_pose != scene->pretip_jumper_standing_pose[0U] ||
        initial_home_pose != scene->pretip_jumper_standing_pose[1U] ||
        scene->pretip_jumper_altitude_q8[0U] != 0U ||
        scene->pretip_jumper_altitude_q8[1U] != 0U) {
        failure = "pre-tip CPU visual contest did not start at gather";
        goto failed;
    }
    p1.held.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) {
        failure = "pre-tip Away-human sample update failed";
        goto failed;
    }
    p1.held.cancel = false;
    if (!scene->pretip_state.away_tip_sampled ||
        scene->pretip_state.away_tip_sample_frame != 0U ||
        scene->pretip_state.away_tip_error != 0U ||
        scene->pretip_state.home_tip_sampled) {
        failure = "pre-tip Away-human input did not retain priority";
        goto failed;
    }
    if (!scene_test_run_cpu_to_decision(scene, &p1, &p2) ||
        !scene->pretip_state.away_tip_sampled ||
        scene->pretip_state.away_tip_sample_frame != 0U ||
        scene->pretip_state.home_tip_sample_frame !=
            TECMO_GAMEPLAY_PRETIP_AUTOMATIC_SINGLE_FRAME ||
        scene->pretip_state.home_tip_error !=
            TECMO_GAMEPLAY_PRETIP_MAX_SAMPLE_ERROR ||
        !tecmo_gameplay_pretip_tip_winner(
            &scene->pretip_assets, &scene->pretip_state, &winner) ||
        winner != TECMO_GAMEPLAY_PRETIP_AWAY_WINNER ||
        !tecmo_gameplay_scene_court_projection(scene, &rising_projection) ||
        !scene->actors[away_actor].pose_orientation_encoded ||
        !scene->actors[home_actor].pose_orientation_encoded ||
        scene->pretip_state.away_jump_altitude_q8 == 0U ||
        scene->pretip_state.away_jump_altitude_q8 !=
            (uint16_t)(((uint32_t)scene->pretip_state.away_jump_velocity_q8 *
                        (scene->pretip_state.phase_frame >
                             scene->pretip_state.away_jump_commit_frame
                         ? scene->pretip_state.phase_frame -
                               scene->pretip_state.away_jump_commit_frame
                         : 0U)) /
                       TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES) ||
        scene->pretip_state.home_jump_altitude_q8 !=
            (uint16_t)(((uint32_t)scene->pretip_state.home_jump_velocity_q8 *
                        (scene->pretip_state.phase_frame >
                             scene->pretip_state.home_jump_commit_frame
                         ? scene->pretip_state.phase_frame -
                               scene->pretip_state.home_jump_commit_frame
                         : 0U)) /
                       TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES) ||
        scene->pretip_state.away_jump_altitude_q8 <=
            scene->pretip_state.home_jump_altitude_q8 ||
        scene->pretip_jumper_altitude_q8[0U] !=
            scene->pretip_state.away_jump_altitude_q8 ||
        scene->pretip_jumper_altitude_q8[1U] !=
            scene->pretip_state.home_jump_altitude_q8 ||
        !rising_projection.players[away_actor].visible ||
        !rising_projection.players[home_actor].visible ||
        rising_projection.players[away_actor].screen_y >=
            initial_projection.players[away_actor].screen_y ||
        rising_projection.players[home_actor].screen_y >=
            initial_projection.players[home_actor].screen_y ||
        scene->actors[away_actor].pose_index == initial_away_pose ||
        scene->actors[home_actor].pose_index == initial_home_pose) {
        failure = "pre-tip Away-human/Home-CPU decision or arc failed";
        goto failed;
    }
    if (!scene_test_run_cpu_to_live(
            scene, &p1, &p2, TECMO_GAMEPLAY_TEAM_AWAY)) {
        failure = "pre-tip Away-human/Home-CPU handoff failed";
        goto failed;
    }
    tecmo_gameplay_scene_end(scene);

    /* Home human versus Away CPU uses the reversed controller orientation. */
    cpu_launch.controller_team[0U] = TECMO_GAMEPLAY_TEAM_HOME;
    cpu_launch.controller_team[1U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    if (!scene_test_run_cpu_to_contest(scene, &cpu_launch, &p1, &p2)) {
        failure = "pre-tip Home-human/Away-CPU contest entry failed";
        goto failed;
    }
    p1.held.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) {
        failure = "pre-tip Home-human sample update failed";
        goto failed;
    }
    p1.held.cancel = false;
    if (!scene->pretip_state.home_tip_sampled ||
        scene->pretip_state.home_tip_sample_frame != 0U ||
        scene->pretip_state.home_tip_error != 0U ||
        scene->pretip_state.away_tip_sampled ||
        !scene_test_run_cpu_to_decision(scene, &p1, &p2) ||
        scene->pretip_state.away_tip_sample_frame !=
            TECMO_GAMEPLAY_PRETIP_AUTOMATIC_SINGLE_FRAME ||
        scene->pretip_state.away_tip_error !=
            TECMO_GAMEPLAY_PRETIP_MAX_SAMPLE_ERROR ||
        !tecmo_gameplay_pretip_tip_winner(
            &scene->pretip_assets, &scene->pretip_state, &winner) ||
        winner != TECMO_GAMEPLAY_PRETIP_HOME_WINNER ||
        !scene_test_run_cpu_to_live(
            scene, &p1, &p2, TECMO_GAMEPLAY_TEAM_HOME)) {
        failure = "pre-tip Home-human/Away-CPU routing or handoff failed";
        goto failed;
    }
    tecmo_gameplay_scene_end(scene);

    /* CPU versus CPU must make both decisions in the same visible contest. */
    cpu_launch.controller_team[0U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    cpu_launch.controller_team[1U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    if (!scene_test_run_cpu_to_contest(scene, &cpu_launch, &p1, &p2)) {
        failure = "pre-tip CPU-versus-CPU contest entry failed";
        goto failed;
    }
    center_ball_x_q8 = scene->ball_position.x_q8;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->pretip_state.away_tip_sampled ||
        scene->pretip_state.home_tip_sampled ||
        scene->ball_position.x_q8 != center_ball_x_q8) {
        failure = "pre-tip ball-X pre-resolution no-snap contract failed";
        goto failed;
    }
    if (!scene_test_run_cpu_to_decision(scene, &p1, &p2) ||
        scene->pretip_state.away_tip_sample_frame !=
            TECMO_GAMEPLAY_PRETIP_AUTOMATIC_BOTH_AWAY_FRAME ||
        scene->pretip_state.home_tip_sample_frame !=
            TECMO_GAMEPLAY_PRETIP_AUTOMATIC_BOTH_HOME_FRAME ||
        scene->pretip_state.away_tip_error !=
            TECMO_GAMEPLAY_PRETIP_MAX_SAMPLE_ERROR ||
        scene->pretip_state.home_tip_error !=
            TECMO_GAMEPLAY_PRETIP_MAX_SAMPLE_ERROR ||
        !tecmo_gameplay_pretip_tip_winner(
            &scene->pretip_assets, &scene->pretip_state, &winner) ||
        winner != TECMO_GAMEPLAY_PRETIP_AWAY_WINNER ||
        scene->ball_position.x_q8 != center_ball_x_q8) {
        failure = "pre-tip CPU-versus-CPU decision or ball-X resolution snap failed";
        goto failed;
    }
    previous_ball_x_q8 = center_ball_x_q8;
    for (frame = 0U; frame < 8U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->ball_position.x_q8 >= previous_ball_x_q8 ||
            scene->ball_position.x_q8 < center_ball_x_q8 - 8 * 256) {
            failure = "pre-tip ball-X monotonic bounded presentation failed";
            goto failed;
        }
        previous_ball_x_q8 = scene->ball_position.x_q8;
    }
    if (scene->ball_position.x_q8 != center_ball_x_q8 - 8 * 256 ||
        !tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->ball_position.x_q8 != previous_ball_x_q8) {
        failure = "pre-tip ball-X capped presentation checkpoint failed";
        goto failed;
    }
    if (!scene_test_run_cpu_to_live(
            scene, &p1, &p2, TECMO_GAMEPLAY_TEAM_AWAY)) {
        failure = "pre-tip CPU-versus-CPU decision or handoff failed";
        goto failed;
    }
    tecmo_gameplay_scene_end(scene);

    /* Deliberately disable both automatic branches after contest entry. The
       helper must stop at the bounded input clock and reject, never loop over
       a stalled frame 30 state. */
    if (!scene_test_run_cpu_to_contest(scene, &cpu_launch, &p1, &p2)) {
        failure = "pre-tip no-progress guard setup failed";
        goto failed;
    }
    scene->launch.controller_team[0U] = TECMO_GAMEPLAY_TEAM_AWAY;
    scene->launch.controller_team[1U] = TECMO_GAMEPLAY_TEAM_HOME;
    if (scene_test_run_cpu_to_decision(scene, &p1, &p2) ||
        scene->pretip_state.contest_frame !=
            TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES) {
        failure = "pre-tip automatic decision max-iteration/no-progress guard failed";
        goto failed;
    }
    tecmo_gameplay_scene_end(scene);

    /* At the exact CPU decision frame, an invalid state must fail closed
       before the automatic path can record either sample. */
    if (!scene_test_run_cpu_to_contest(scene, &cpu_launch, &p1, &p2)) {
        failure = "pre-tip malformed-state setup failed";
        goto failed;
    }
    for (frame = 0U;
         frame < TECMO_GAMEPLAY_PRETIP_AUTOMATIC_BOTH_AWAY_FRAME; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) {
            failure = "pre-tip malformed-state decision setup failed";
            goto failed;
        }
    }
    malformed_state = scene->pretip_state;
    memcpy(malformed_actors, scene->actors, sizeof(malformed_actors));
    frame_before = scene->frame;
    scene->pretip_state.phase_frame =
        TECMO_GAMEPLAY_PRETIP_PRESENTATION_FRAMES;
    malformed_state.phase_frame =
        TECMO_GAMEPLAY_PRETIP_PRESENTATION_FRAMES;
    if (tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        memcmp(&scene->pretip_state, &malformed_state,
               sizeof(malformed_state)) != 0 ||
        memcmp(scene->actors, malformed_actors,
               sizeof(malformed_actors)) != 0 ||
        scene->frame != frame_before ||
        scene->pretip_state.away_tip_sampled ||
        scene->pretip_state.home_tip_sampled) {
        failure = "pre-tip malformed state was not fail-closed";
        goto failed;
    }
    tecmo_gameplay_scene_end(scene);
    return true;

failed:
    tecmo_gameplay_scene_test_message(
        test != NULL ? test->message : NULL,
        test != NULL ? test->message_size : 0U,
        failure);
    tecmo_gameplay_scene_end(scene);
    return false;
}

static bool tecmo_gameplay_scene_test_pretip_anchor_facing_regression(
    TecmoGameplaySceneTestContext *test,
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *launch)
{
    TecmoGameplayPreTipState state_before;
    TecmoGameplaySceneActor actors_before[
        TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplaySceneActor failed_actors[
        TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    uint8_t away_actor;
    uint8_t home_actor;
    uint8_t left_actor;
    uint8_t right_actor;
    int16_t away_x;
    int16_t home_x;
    uint32_t frame_before;
    uint32_t failed_frame;
    size_t frame;

    if (test == NULL || scene == NULL || launch == NULL ||
        !tecmo_gameplay_scene_launch(scene, launch)) {
        tecmo_gameplay_scene_test_message(
            test != NULL ? test->message : NULL,
            test != NULL ? test->message_size : 0U,
            "pre-tip anchor-facing regression launch rejected");
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    for (frame = 0U; frame < 660U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) goto failed;
    }
    away_actor = scene->pretip_jumper_actor[0U];
    home_actor = scene->pretip_jumper_actor[1U];
    if (away_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        home_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        away_actor == home_actor) {
        tecmo_gameplay_scene_test_message(
            test->message, test->message_size,
            "pre-tip anchor-facing regression mapping was malformed");
        goto failed;
    }
    away_x = scene->actors[away_actor].anchor.x;
    home_x = scene->actors[home_actor].anchor.x;
    if (away_x == home_x) {
        tecmo_gameplay_scene_test_message(
            test->message, test->message_size,
            "pre-tip anchor-facing regression setup was equal");
        goto failed;
    }

    state_before = scene->pretip_state;
    memcpy(actors_before, scene->actors, sizeof(actors_before));
    frame_before = scene->frame;
    scene->actors[home_actor].anchor.x = away_x;
    scene->actors[home_actor].position.x = away_x;
    if (tecmo_gameplay_scene_update(scene, &p1, &p2)) {
        tecmo_gameplay_scene_test_message(
            test->message, test->message_size,
            "equal tip jumper anchors were accepted");
        goto failed;
    }
    memcpy(failed_actors, scene->actors, sizeof(failed_actors));
    failed_frame = scene->frame;
    failed_actors[home_actor].anchor = actors_before[home_actor].anchor;
    failed_actors[home_actor].position = actors_before[home_actor].position;
    memcpy(scene->actors, actors_before, sizeof(actors_before));
    scene->pretip_state = state_before;
    scene->frame = frame_before;
    if (memcmp(failed_actors, actors_before, sizeof(failed_actors)) != 0 ||
        failed_frame != frame_before) {
        tecmo_gameplay_scene_test_message(
            test->message, test->message_size,
            "equal tip jumper anchor rejection was not transactional");
        goto failed;
    }

    /* Swap the team-to-position mapping to prove the helper follows actual
       anchors rather than assuming Away is the left jumper. */
    scene->actors[away_actor].anchor.x = home_x;
    scene->actors[away_actor].position.x = home_x;
    scene->actors[home_actor].anchor.x = away_x;
    scene->actors[home_actor].position.x = away_x;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST ||
        scene->pretip_state.phase_frame != 0U ||
        !scene_test_pretip_jumper_order(
            scene, &left_actor, &right_actor)) {
        tecmo_gameplay_scene_test_message(
            test->message, test->message_size,
            "crossed team-to-position tip ordering was rejected");
        goto failed;
    }
    /* Keep this crossed-anchor presentation route resolved with a valid
       team-routed human capture; the separate no-input scene regression
       remains fail-closed and stalled. */
    p1.held.cancel = true;
    for (frame = 0U;
         frame < TECMO_GAMEPLAY_PRETIP_PRESENTATION_FRAMES; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) goto failed;
    }
    p1.held.cancel = false;
    if (tecmo_gameplay_scene_in_pretip(scene) ||
        scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_LIVE ||
        scene->pretip_jump_active) {
        tecmo_gameplay_scene_test_message(
            test->message, test->message_size,
            "crossed tip mapping did not land into LIVE");
        goto failed;
    }
    for (frame = 0U; frame < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++frame) {
        bool expected_facing;
        if (!scene_goal_facing_right_for_team(
                scene, (TecmoGameplayTeam)scene->actors[frame].team,
                &expected_facing) ||
            scene->actors[frame].facing_right != expected_facing) {
            tecmo_gameplay_scene_test_message(
                test->message, test->message_size,
                "crossed tip landing did not restore TGOR facing");
            goto failed;
        }
    }
    tecmo_gameplay_scene_end(scene);
    return true;

failed:
    tecmo_gameplay_scene_end(scene);
    return false;
}

static bool tecmo_gameplay_scene_test_pretip_jump_presentation(
    TecmoGameplaySceneTestContext *test,
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *launch)
{
    static const uint16_t stage_frames[] = {
        0U, 7U, 15U, 25U, 26U, 35U, 51U, 52U, 59U
    };
    static const uint16_t stage_poses[] = {
        TECMO_GAMEPLAY_JUMP_MAKE_GATHER_POSE,
        TECMO_GAMEPLAY_JUMP_MAKE_GATHER_POSE,
        TECMO_GAMEPLAY_JUMP_TURN_POSE,
        TECMO_GAMEPLAY_JUMP_RELEASE_POSE,
        TECMO_GAMEPLAY_JUMP_FLIGHT_POSE,
        TECMO_GAMEPLAY_JUMP_FLIGHT_POSE,
        TECMO_GAMEPLAY_JUMP_FLIGHT_POSE,
        TECMO_GAMEPLAY_JUMP_SLOT0_IDLE_POSE,
        TECMO_GAMEPLAY_JUMP_SLOT0_IDLE_POSE
    };
    static const uint16_t stage_altitudes_q8[] = {
        0U, 0U, 2730U, 6144U, 6144U, 6144U, 0U, 0U, 0U
    };
    const size_t pixel_count =
        (size_t)TECMO_GAMEPLAY_SCENE_NES_WIDTH *
        TECMO_GAMEPLAY_SCENE_NES_HEIGHT;
    TecmoGameplaySceneActor setup_actors[
        TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplaySceneCpuActor setup_cpu_actors[
        TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayState setup_state;
    TecmoGameplayCameraState setup_camera;
    TecmoGameplaySceneCourtProjection baseline_projection;
    TecmoGameplaySceneCourtProjection projection;
    TecmoGameplaySceneCourtCoordinates coordinates;
    TecmoGameplaySceneCourtProjection sentinel_projection;
    TecmoGameplaySceneActor late_first_actors[
        TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    uint32_t *background_pixels = NULL;
    uint32_t *actor_pixels = NULL;
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    uint8_t away_actor;
    uint8_t home_actor;
    uint8_t left_actor;
    uint8_t right_actor;
    uint16_t setup_action_serial;
    uint32_t setup_camera_follow_count;
    TecmoGameplayPreTipState stalled_state;
    uint32_t stalled_scene_frame;
    uint8_t setup_shot_actor;
    TecmoGameplaySceneShotKind setup_shot_kind;
    uint8_t setup_ball_holder;
    size_t frame;
    size_t stage;
    bool logical_resolution_contact_checked = false;
    bool ok = false;

    if (test == NULL || scene == NULL || launch == NULL ||
        !tecmo_gameplay_scene_launch(scene, launch)) {
        tecmo_gameplay_scene_test_message(
            test != NULL ? test->message : NULL,
            test != NULL ? test->message_size : 0U,
            "pre-tip jump presentation launch rejected");
        return false;
    }
    away_actor = scene->pretip_assets.tip_actor_indices[0U];
    home_actor = scene->pretip_assets.tip_actor_indices[1U];
    memcpy(setup_actors, scene->actors, sizeof(setup_actors));
    memcpy(setup_cpu_actors, scene->cpu_actors, sizeof(setup_cpu_actors));
    setup_state = scene->state;
    setup_camera = scene->camera_state;
    setup_action_serial = scene->action_serial;
    setup_camera_follow_count = scene->camera_follow_count;
    setup_shot_actor = scene->shot_actor;
    setup_shot_kind = scene->shot_kind;
    setup_ball_holder = scene->ball_holder;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    for (frame = 0U; frame < 660U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) goto cleanup;
    }
    if (memcmp(setup_actors, scene->actors, sizeof(setup_actors)) != 0 ||
        memcmp(setup_cpu_actors, scene->cpu_actors,
               sizeof(setup_cpu_actors)) != 0 ||
        memcmp(&setup_state, &scene->state, sizeof(setup_state)) != 0 ||
        memcmp(&setup_camera, &scene->camera_state,
               sizeof(setup_camera)) != 0 ||
        setup_action_serial != scene->action_serial ||
        setup_camera_follow_count != scene->camera_follow_count ||
        setup_shot_actor != scene->shot_actor ||
        setup_shot_kind != scene->shot_kind ||
        setup_ball_holder != scene->ball_holder ||
        scene->pretip_jump_active ||
        scene->pretip_jumper_altitude_q8[0U] != 0U ||
        scene->pretip_jumper_altitude_q8[1U] != 0U) {
        tecmo_gameplay_scene_test_message(
            test->message, test->message_size,
            "pre-tip non-contest mutation leaked into jump presentation");
        goto cleanup;
    }
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST ||
        scene->pretip_state.phase_frame != 0U ||
        scene->pretip_state.contest_frame != 0U ||
        scene->pretip_state.total_frame != 661U ||
        away_actor != 4U || home_actor != 9U ||
        scene->pretip_jumper_actor[0U] != away_actor ||
        scene->pretip_jumper_actor[1U] != home_actor ||
        scene->actors[away_actor].team != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene->actors[home_actor].team != TECMO_GAMEPLAY_TEAM_HOME ||
        !scene_test_pretip_jumper_order(
            scene, &left_actor, &right_actor) ||
        scene->actors[away_actor].pose_index !=
            scene->pretip_jumper_standing_pose[0U] ||
        scene->actors[home_actor].pose_index !=
            scene->pretip_jumper_standing_pose[1U] ||
        !scene->actors[away_actor].pose_orientation_encoded ||
        !scene->actors[home_actor].pose_orientation_encoded) {
        tecmo_gameplay_scene_test_message(
            test->message, test->message_size,
            "pre-tip jumper identity or action-pose orientation contract failed");
        goto cleanup;
    }
    if (!tecmo_gameplay_scene_court_projection(
            scene, &baseline_projection) ||
        baseline_projection.camera_x != 0x0100U ||
        !baseline_projection.players[away_actor].visible ||
        !baseline_projection.players[home_actor].visible) {
        tecmo_gameplay_scene_test_message(
            test->message, test->message_size,
            "pre-tip center camera/jumper visibility contract failed");
        goto cleanup;
    }
    background_pixels = (uint32_t *)malloc(pixel_count * sizeof(uint32_t));
    actor_pixels = (uint32_t *)malloc(pixel_count * sizeof(uint32_t));
    if (background_pixels == NULL || actor_pixels == NULL) {
        tecmo_gameplay_scene_test_message(
            test->message, test->message_size,
            "native logical-resolution tip frame allocation failed");
        goto cleanup;
    }
    for (stage = 0U; stage < sizeof(stage_frames) / sizeof(stage_frames[0]);
         ++stage) {
        while (scene->pretip_state.phase ==
                   TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST &&
               scene->pretip_state.phase_frame < stage_frames[stage]) {
            if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) goto cleanup;
        }
        if (scene->pretip_state.phase !=
                TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST ||
            scene->pretip_state.phase_frame != stage_frames[stage] ||
            scene->pretip_state.contest_frame !=
                (stage_frames[stage] <
                         TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES
                     ? stage_frames[stage]
                     : TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES) ||
            scene->pretip_state.total_frame !=
                661U + stage_frames[stage] ||
            scene->pretip_jumper_altitude_q8[0U] !=
                stage_altitudes_q8[stage] ||
            scene->pretip_jumper_altitude_q8[1U] !=
                stage_altitudes_q8[stage] ||
            scene->actors[away_actor].pose_index != stage_poses[stage] ||
            scene->actors[home_actor].pose_index != stage_poses[stage] ||
            scene->actors[away_actor].position.x !=
                scene->actors[away_actor].anchor.x ||
            scene->actors[away_actor].position.y !=
                scene->actors[away_actor].anchor.y ||
            scene->actors[home_actor].position.x !=
                scene->actors[home_actor].anchor.x ||
            scene->actors[home_actor].position.y !=
                scene->actors[home_actor].anchor.y ||
            !tecmo_gameplay_scene_court_projection(scene, &projection) ||
            !tecmo_gameplay_scene_court_coordinates(scene, &coordinates) ||
            !projection.players[away_actor].visible ||
            !projection.players[home_actor].visible ||
            projection.players[away_actor].screen_x !=
                baseline_projection.players[away_actor].screen_x ||
            projection.players[home_actor].screen_x !=
                baseline_projection.players[home_actor].screen_x ||
            projection.players[away_actor].screen_y !=
                (uint8_t)(baseline_projection.players[away_actor].screen_y -
                    stage_altitudes_q8[stage] /
                        TECMO_GAMEPLAY_COURT_COORDINATE_Q8_SCALE) ||
            projection.players[home_actor].screen_y !=
                (uint8_t)(baseline_projection.players[home_actor].screen_y -
                    stage_altitudes_q8[stage] /
                        TECMO_GAMEPLAY_COURT_COORDINATE_Q8_SCALE) ||
            coordinates.players[away_actor].x !=
                scene->actors[away_actor].anchor.x ||
            coordinates.players[away_actor].y !=
                scene->actors[away_actor].anchor.y ||
            coordinates.players[home_actor].x !=
                scene->actors[home_actor].anchor.x ||
            coordinates.players[home_actor].y !=
                scene->actors[home_actor].anchor.y) {
            tecmo_gameplay_scene_test_message(
                test->message, test->message_size,
                "pre-tip jumper stage pose/anchor/projected-Y contract failed");
            goto cleanup;
        }
        if (!scene->actors[left_actor].facing_right ||
            scene->actors[right_actor].facing_right ||
            scene->actors[away_actor].pose_orientation_encoded ||
            scene->actors[home_actor].pose_orientation_encoded) {
            tecmo_gameplay_scene_test_message(
                test->message, test->message_size,
                "generic tip action pose inward-facing contract failed");
            goto cleanup;
        }
        for (frame = 0U; frame < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++frame) {
            if ((uint8_t)frame == away_actor ||
                (uint8_t)frame == home_actor) {
                continue;
            }
            if (scene->actors[frame].facing_right !=
                    setup_actors[frame].facing_right ||
                scene->actors[frame].pose_orientation_encoded !=
                    setup_actors[frame].pose_orientation_encoded ||
                scene->actors[frame].pose_index != setup_actors[frame].pose_index) {
                tecmo_gameplay_scene_test_message(
                    test->message, test->message_size,
                    "standing tip pose orientation was changed by jump action");
                goto cleanup;
            }
        }
        if (stage_frames[stage] ==
                TECMO_GAMEPLAY_PRETIP_JUMP_CONTACT_FRAME) {
            TecmoGameplayResolvedPose contact_away_pose;
            TecmoGameplayResolvedPose contact_home_pose;
            if (!tecmo_gameplay_scene_render_resolve_actor_pose(
                    scene, away_actor, &contact_away_pose) ||
                !tecmo_gameplay_scene_render_resolve_actor_pose(
                    scene, home_actor, &contact_home_pose) ||
                contact_away_pose.pointer_index !=
                    TECMO_GAMEPLAY_JUMP_FLIGHT_POSE ||
                contact_home_pose.pointer_index !=
                    TECMO_GAMEPLAY_JUMP_FLIGHT_POSE ||
                !scene_test_pretip_draw_logical_resolution(
                    scene, background_pixels, false) ||
                !scene_test_pretip_draw_logical_resolution(
                    scene, actor_pixels, true) ||
                scene_test_pretip_actor_pixel_changes(
                    actor_pixels, background_pixels,
                    &projection.players[away_actor]) == 0U ||
                scene_test_pretip_actor_pixel_changes(
                    actor_pixels, background_pixels,
                    &projection.players[home_actor]) == 0U) {
                tecmo_gameplay_scene_test_message(
                    test->message, test->message_size,
                    "native logical-resolution generic tip action frame was not readable");
                goto cleanup;
            }
            logical_resolution_contact_checked = true;
        }
    }
    if (!logical_resolution_contact_checked ||
        !scene_test_pretip_draw_logical_resolution(
            scene, background_pixels, false) ||
        !scene_test_pretip_draw_logical_resolution(
            scene, actor_pixels, true) ||
        scene_test_pretip_actor_pixel_changes(
            actor_pixels, background_pixels,
            &projection.players[away_actor]) == 0U ||
        scene_test_pretip_actor_pixel_changes(
            actor_pixels, background_pixels,
            &projection.players[home_actor]) == 0U) {
        tecmo_gameplay_scene_test_message(
            test->message, test->message_size,
            "native logical-resolution landed tip actors were not readable");
        goto cleanup;
    }
    {
        TecmoGameplayResolvedPose away_pose;
        TecmoGameplayResolvedPose home_pose;
        if (scene->actors[away_actor].pose_index !=
                TECMO_GAMEPLAY_JUMP_SLOT0_IDLE_POSE ||
            !tecmo_gameplay_scene_render_resolve_actor_pose(
                scene, away_actor, &away_pose) ||
            !tecmo_gameplay_scene_render_resolve_actor_pose(
                scene, home_actor, &home_pose) ||
            away_pose.pointer_index != TECMO_GAMEPLAY_JUMP_SLOT0_IDLE_POSE ||
            home_pose.pointer_index != TECMO_GAMEPLAY_JUMP_SLOT0_IDLE_POSE) {
            tecmo_gameplay_scene_test_message(
                test->message, test->message_size,
                "native logical-resolution generic tip pose resolution failed");
            goto cleanup;
        }
    }
    scene->pretip_jumper_actor[0U] = home_actor;
    memset(&sentinel_projection, 0xA5, sizeof(sentinel_projection));
    projection = sentinel_projection;
    if (tecmo_gameplay_scene_court_projection(scene, &projection) ||
        memcmp(&projection, &sentinel_projection,
               sizeof(projection)) != 0) {
        tecmo_gameplay_scene_test_message(
            test->message, test->message_size,
            "malformed tip jumper mapping was not rejected");
        goto cleanup;
    }
    scene->pretip_jumper_actor[0U] = away_actor;
    scene->pretip_state.phase = TECMO_GAMEPLAY_PRETIP_LIVE;
    projection = sentinel_projection;
    if (tecmo_gameplay_scene_court_projection(scene, &projection) ||
        memcmp(&projection, &sentinel_projection,
               sizeof(projection)) != 0) {
        tecmo_gameplay_scene_test_message(
            test->message, test->message_size,
            "malformed active tip phase was not rejected");
        goto cleanup;
    }
    scene->pretip_state.phase = TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST;
    scene->pretip_state.phase_frame =
        TECMO_GAMEPLAY_PRETIP_JUMP_DURATION;
    projection = sentinel_projection;
    if (tecmo_gameplay_scene_court_projection(scene, &projection) ||
        memcmp(&projection, &sentinel_projection,
               sizeof(projection)) != 0) {
        tecmo_gameplay_scene_test_message(
            test->message, test->message_size,
            "malformed tip phase frame was not rejected");
        goto cleanup;
    }
    scene->pretip_state.phase_frame =
        TECMO_GAMEPLAY_PRETIP_JUMP_DURATION - 1U;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        !scene->pretip_state.contest_stalled ||
        scene->pretip_state.away_tip_sampled ||
        scene->pretip_state.home_tip_sampled ||
        scene->pretip_state.claim_resolved ||
        scene->pretip_state.claim_deferred ||
        scene->pretip_state.live_handoff ||
        scene->pretip_state.phase !=
            TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST ||
        scene->pretip_state.phase_frame !=
            TECMO_GAMEPLAY_PRETIP_PRESENTATION_FRAMES - 1U ||
        scene->pretip_state.total_frame != 720U ||
        scene->frame != 720U ||
        !tecmo_gameplay_pretip_state_validate(
            &scene->pretip_assets, &scene->pretip_state)) {
        tecmo_gameplay_scene_test_message(
            test->message, test->message_size,
            "scene no-input stall invariant was not valid at frame 720");
        goto cleanup;
    }
    stalled_state = scene->pretip_state;
    stalled_scene_frame = scene->frame;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        memcmp(&scene->pretip_state, &stalled_state,
               sizeof(stalled_state)) != 0 ||
        scene->frame != stalled_scene_frame) {
        tecmo_gameplay_scene_test_message(
            test->message, test->message_size,
            "scene no-input stall repeated update was not deterministic");
        goto cleanup;
    }
    tecmo_gameplay_scene_end(scene);
    if (!scene_test_run_late_human_tip(scene, launch)) goto cleanup;
    memcpy(late_first_actors, scene->actors, sizeof(late_first_actors));
    setup_state = scene->state;
    tecmo_gameplay_scene_end(scene);
    if (!scene_test_run_late_human_tip(scene, launch) ||
        memcmp(late_first_actors, scene->actors,
               sizeof(late_first_actors)) != 0 ||
        memcmp(&setup_state, &scene->state, sizeof(setup_state)) != 0 ||
        scene->frame != 721U || scene->ball_holder != 3U ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_AWAY) {
        tecmo_gameplay_scene_test_message(
            test->message, test->message_size,
            "human late-sample frame-721 checkpoint was nondeterministic");
        goto cleanup;
    }
    ok = true;

cleanup:
    free(background_pixels);
    free(actor_pixels);
    tecmo_gameplay_scene_end(scene);
    return ok;
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
    /* The prior card/descent phases ignore a held B for cancellation here;
       route Away B only across the bounded jump-contest window, then clear
       it so the accepted frame-721 handoff remains a normal scene path. */
    p1->held.cancel = true;
    for (frame = 600U; frame < 721U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, p1, p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "pre-tip live handoff update rejected");
            tecmo_gameplay_scene_destroy(scene);
            return false;
        }
    }
    p1->held.cancel = false;
    if (tecmo_gameplay_scene_in_pretip(scene) ||
        scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_LIVE ||
        !scene->pretip_state.live_handoff ||
        scene->pretip_state.total_frame != 721U ||
        scene->frame != 721U ||
        scene->state.clock_minutes != 2U || scene->state.clock_seconds != 0U ||
        scene->state.shot_clock != 24U ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene->ball_holder != 3U ||
        !scene->audio_player.music->track_pending ||
        scene->audio_player.music->pending_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pre-tip 721-frame track-8-to-5 handoff failed");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    if (!tecmo_gameplay_scene_update(scene, p1, p2) ||
        scene->frame != 722U ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->ball_holder != 3U ||
        scene->actors[scene->ball_holder].team !=
            TECMO_GAMEPLAY_TEAM_AWAY) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "pre-tip first live possession update failed");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    tecmo_gameplay_scene_end(scene);
    return true;
}

static bool tecmo_gameplay_scene_test_pretip_normal_home_handoff(
    TecmoGameplaySceneTestContext *test,
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *launch)
{
    char *message = test->message;
    size_t message_size = test->message_size;
    TecmoGameplaySceneLaunch normal = *launch;
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    size_t frame;

    normal.controller_team[0] = TECMO_GAMEPLAY_TEAM_AWAY;
    normal.controller_team[1] = TECMO_GAMEPLAY_TEAM_HOME;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_scene_launch(scene, &normal) ||
        scene->launch.controller_team[0] != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene->launch.controller_team[1] != TECMO_GAMEPLAY_TEAM_HOME) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "pre-tip normal assignment Home handoff launch rejected");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    for (frame = 0U; frame < 661U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) {
            tecmo_gameplay_scene_test_message(
                message, message_size,
                "pre-tip normal assignment contest entry rejected");
            tecmo_gameplay_scene_destroy(scene);
            return false;
        }
    }
    p2.held.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        !scene->pretip_state.home_tip_sampled ||
        scene->pretip_state.home_tip_sample_frame != 0U ||
        scene->pretip_state.home_tip_error != 0U ||
        scene->pretip_state.away_tip_sampled) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "pre-tip normal assignment Home controller routing failed");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    p2.held.cancel = false;
    p1.held.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        !scene->pretip_state.home_tip_sampled ||
        scene->pretip_state.home_tip_sample_frame != 0U ||
        !scene->pretip_state.away_tip_sampled ||
        scene->pretip_state.away_tip_sample_frame != 1U ||
        scene->pretip_state.away_tip_error != 1U) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "pre-tip normal assignment later Away sample failed");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    p1.held.cancel = false;
    for (frame = 663U; frame < 721U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) {
            tecmo_gameplay_scene_test_message(
                message, message_size,
                "pre-tip normal assignment live handoff rejected");
            tecmo_gameplay_scene_destroy(scene);
            return false;
        }
    }
    if (tecmo_gameplay_scene_in_pretip(scene) ||
        scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_LIVE ||
        !scene->pretip_state.live_handoff ||
        scene->pretip_state.contest_frame !=
            TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES ||
        scene->pretip_state.total_frame != 721U ||
        scene->frame != 721U ||
        scene->pretip_jump_active ||
        scene->pretip_jumper_altitude_q8[0U] != 0U ||
        scene->pretip_jumper_altitude_q8[1U] != 0U ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        scene->ball_holder != 8U ||
        scene->orientation_state.current_direction != 1U ||
        scene->pretip_state.home_tip_error != 0U ||
        scene->pretip_state.away_tip_error != 1U) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "pre-tip normal assignment Home possession handoff failed");
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    for (frame = 0U; frame < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++frame) {
        bool expected_facing;
        if (!scene_goal_facing_right_for_team(
                scene, (TecmoGameplayTeam)scene->actors[frame].team,
                &expected_facing) ||
            scene->actors[frame].facing_right != expected_facing) {
            tecmo_gameplay_scene_test_message(
                message, message_size,
                "Home tip landing did not restore TGOR facing");
            tecmo_gameplay_scene_end(scene);
            return false;
        }
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
    for (frame = 664U; frame < 721U; ++frame) {
        p1->held.cancel = true;
        if (!tecmo_gameplay_scene_update(scene, p1, p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "pre-tip timing handoff rejected");
            tecmo_gameplay_scene_destroy(scene);
            return false;
        }
    }
    p1->held.cancel = false;
    if (scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_LIVE ||
        !scene->pretip_state.live_handoff ||
        scene->pretip_state.total_frame != 721U ||
        scene->frame != 721U ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        scene->ball_holder != 8U ||
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
        !tecmo_gameplay_scene_test_pretip_real_time_presentation_regression(
            test, scene) ||
        !tecmo_gameplay_scene_test_pretip_initial_launch(
            test, scene, &launch, &p1, &p2, &tip_lineup) ||
        !tecmo_gameplay_scene_test_pretip_descent_live(
            test, scene, &p1, &p2) ||
        !tecmo_gameplay_scene_test_pretip_normal_home_handoff(
            test, scene, &launch) ||
        !tecmo_gameplay_scene_test_pretip_anchor_facing_regression(
            test, scene, &launch) ||
        !tecmo_gameplay_scene_test_pretip_cpu_decision_regression(
            test, scene, &launch) ||
        !tecmo_gameplay_scene_test_pretip_abort_and_timing(
            test, scene, &launch, &p1, &p2)) {
        return false;
    }
    test->launch = launch;
    test->p1 = p1;
    test->p2 = p2;
    return true;
}

typedef struct TecmoWin32TipHarness {
    TecmoRuntime *runtime;
    TecmoGameMemory memory;
    TecmoControls controls[TECMO_GAMEPLAY_CONTROLLER_COUNT];
    TecmoWin32KeyboardState keyboard;
    bool scene_initialized;
} TecmoWin32TipHarness;

static void scene_test_win32_tip_harness_destroy(
    TecmoWin32TipHarness *harness)
{
    if (harness == NULL) return;
    if (harness->scene_initialized && harness->runtime != NULL) {
        tecmo_gameplay_scene_destroy(&harness->runtime->gameplay_scene);
    }
    free(harness->runtime);
    memset(harness, 0, sizeof(*harness));
}

static bool scene_test_win32_tip_harness_init(
    TecmoWin32TipHarness *harness,
    const char *project_root,
    const char *asset_pack_path,
    TecmoMusicPlayer *music_player,
    const TecmoGameplaySceneLaunch *launch)
{
    if (harness == NULL || project_root == NULL || asset_pack_path == NULL ||
        music_player == NULL || launch == NULL) {
        return false;
    }
    memset(harness, 0, sizeof(*harness));
    harness->runtime = (TecmoRuntime *)calloc(1U, sizeof(*harness->runtime));
    if (harness->runtime == NULL) return false;
    harness->runtime->memory = &harness->memory;
    harness->runtime->mode = TECMO_MODE_COURT;
    harness->runtime->frame_seconds = (float)(
        (double)TECMO_MUSIC_TICK_DENOMINATOR /
        (double)TECMO_MUSIC_TICK_NUMERATOR);
    tecmo_controls_init(&harness->controls[0U]);
    tecmo_controls_init(&harness->controls[1U]);
    tecmo_win32_keyboard_init(&harness->keyboard);
    tecmo_gameplay_scene_init(&harness->runtime->gameplay_scene);
    harness->scene_initialized = true;
    if (!tecmo_gameplay_scene_load(
            &harness->runtime->gameplay_scene,
            project_root, asset_pack_path, music_player) ||
        !tecmo_gameplay_scene_launch(
            &harness->runtime->gameplay_scene, launch)) {
        scene_test_win32_tip_harness_destroy(harness);
        return false;
    }
    harness->runtime->mode = TECMO_MODE_COURT;
    return true;
}

static bool scene_test_win32_tip_harness_relaunch(
    TecmoWin32TipHarness *harness,
    const TecmoGameplaySceneLaunch *launch)
{
    TecmoGameplayScene *scene;
    if (harness == NULL || harness->runtime == NULL || launch == NULL) {
        return false;
    }
    scene = &harness->runtime->gameplay_scene;
    tecmo_gameplay_scene_end(scene);
    harness->runtime->previous_input = (TecmoInput){0};
    harness->runtime->previous_player_two_input = (TecmoInput){0};
    tecmo_controls_init(&harness->controls[0U]);
    tecmo_controls_init(&harness->controls[1U]);
    tecmo_win32_keyboard_init(&harness->keyboard);
    harness->runtime->mode = TECMO_MODE_COURT;
    return tecmo_gameplay_scene_launch(scene, launch);
}

static bool scene_test_win32_tip_key(
    TecmoWin32TipHarness *harness,
    uint32_t virtual_key,
    bool physical_down,
    bool mapped)
{
    TecmoWin32KeyBinding binding;
    bool logical_down = false;
    if (harness == NULL || harness->runtime == NULL ||
        !tecmo_win32_keyboard_update(
            &harness->keyboard, virtual_key, physical_down,
            &binding, &logical_down)) {
        return !mapped;
    }
    if (!mapped || binding.player_index >= TECMO_GAMEPLAY_CONTROLLER_COUNT) {
        return false;
    }
    tecmo_controls_set_button(
        &harness->controls[binding.player_index], binding.button,
        logical_down);
    return true;
}

static bool scene_test_win32_tip_update_observe(
    TecmoWin32TipHarness *harness,
    bool *player_one_cancel_out,
    bool *player_one_released_cancel_out)
{
    TecmoGameplayScene *scene;
    const TecmoInput *player_one;
    const TecmoInput *player_two;
    uint32_t frame_before;
    if (harness == NULL || harness->runtime == NULL) return false;
    scene = &harness->runtime->gameplay_scene;
    frame_before = scene->frame;
    tecmo_win32_keyboard_begin_controls_frame(
        &harness->keyboard, harness->controls,
        sizeof(harness->controls) / sizeof(harness->controls[0]));
    player_one = tecmo_controls_held(&harness->controls[0U]);
    player_two = tecmo_controls_held(&harness->controls[1U]);
    if (player_one == NULL || player_two == NULL) {
        tecmo_win32_keyboard_end_controls_frame(
            &harness->keyboard, harness->controls,
            sizeof(harness->controls) / sizeof(harness->controls[0]));
        return false;
    }
    if (player_one_cancel_out != NULL) {
        *player_one_cancel_out = player_one->cancel;
    }
    if (player_one_released_cancel_out != NULL) {
        *player_one_released_cancel_out =
            tecmo_controls_released(
                &harness->controls[0U], TECMO_CONTROL_CANCEL);
    }
    tecmo_runtime_update_players(
        harness->runtime, player_one, player_two);
    tecmo_win32_keyboard_end_controls_frame(
        &harness->keyboard, harness->controls,
        sizeof(harness->controls) / sizeof(harness->controls[0]));
    return scene->active && scene->frame == frame_before + 1U;
}

static bool scene_test_win32_tip_update(TecmoWin32TipHarness *harness)
{
    return scene_test_win32_tip_update_observe(harness, NULL, NULL);
}

static bool scene_test_win32_tip_advance(
    TecmoWin32TipHarness *harness,
    size_t update_count)
{
    size_t frame;
    for (frame = 0U; frame < update_count; ++frame) {
        if (!scene_test_win32_tip_update(harness)) return false;
    }
    return true;
}

static bool scene_test_win32_tip_advance_to_contest(
    TecmoWin32TipHarness *harness,
    size_t update_count)
{
    if (!scene_test_win32_tip_advance(harness, update_count)) return false;
    return harness->runtime->gameplay_scene.pretip_state.phase ==
               TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST &&
           harness->runtime->gameplay_scene.pretip_state.phase_frame == 0U &&
           !harness->runtime->gameplay_scene.pretip_jump_active;
}

static bool scene_test_run_win32_x_tip(
    const char *project_root,
    const char *asset_pack_path,
    TecmoMusicPlayer *music_player,
    char *message,
    size_t message_size)
{
    TecmoGameplaySceneLaunch launch;
    TecmoWin32KeyBinding binding = {99U, TECMO_CONTROL_DEBUG_TOGGLE};
    TecmoWin32TipHarness harness;
    TecmoGameplayScene *scene;
    const char *failure = "Win32 X tip path failed";
    char diagnostic[192];
    bool ok = false;
    bool effective_cancel = false;
    bool released_cancel = false;

    memset(&harness, 0, sizeof(harness));
    memset(&launch, 0, sizeof(launch));
    launch.source = TECMO_GAMEPLAY_SCENE_PRESEASON;
    launch.away_team = 0U;
    launch.home_team = 1U;
    launch.regulation_minutes = 2U;
    launch.difficulty = 1U;
    launch.control_mode = 1U;
    launch.speed_value = 1U;
    launch.controller_team[0U] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch.controller_team[1U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    launch.game_music_enabled = false;

    if (!tecmo_win32_translate_key('X', &binding) ||
        binding.player_index != 0U || binding.button != TECMO_CONTROL_CANCEL ||
        tecmo_win32_translate_key('B', &binding) ||
        !scene_test_win32_tip_harness_init(
            &harness, project_root, asset_pack_path, music_player, &launch)) {
        failure = "Win32 X/B translation or tip harness initialization failed";
        goto cleanup;
    }
    scene = &harness.runtime->gameplay_scene;

    /* An unrelated key and the literal B must not manufacture Player 1 NES B. */
    if (!scene_test_win32_tip_key(&harness, 'B', true, false) ||
        !scene_test_win32_tip_key(&harness, 'V', true, false) ||
        harness.controls[0U].current.cancel ||
        !scene_test_win32_tip_advance_to_contest(&harness, 661U) ||
        !scene_test_win32_tip_key(&harness, 'X', true, true) ||
        !harness.controls[0U].current.cancel ||
        !scene_test_win32_tip_update(&harness) ||
        scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST ||
        scene->pretip_state.phase_frame != 1U ||
        !scene->pretip_jump_active ||
        !scene->pretip_state.away_tip_sampled ||
        scene->pretip_state.away_tip_sample_frame != 0U ||
        scene->pretip_state.away_tip_error != 0U ||
        scene->pretip_state.home_tip_sampled) {
        (void)snprintf(
            diagnostic, sizeof(diagnostic),
            "assigned Away X frame failed: phase=%u frame=%u active=%u sampled=%u sample=%u error=%u home=%u status=%s",
            (unsigned)scene->pretip_state.phase,
            (unsigned)scene->pretip_state.phase_frame,
            scene->pretip_jump_active ? 1U : 0U,
            scene->pretip_state.away_tip_sampled ? 1U : 0U,
            (unsigned)scene->pretip_state.away_tip_sample_frame,
            (unsigned)scene->pretip_state.away_tip_error,
            scene->pretip_state.home_tip_sampled ? 1U : 0U,
            scene->status);
        failure = diagnostic;
        goto cleanup;
    }

    /* An unmapped physical B cannot cancel an already-held X alias. */
    if (!scene_test_win32_tip_key(&harness, 'B', true, false) ||
        !harness.controls[0U].current.cancel ||
        !scene_test_win32_tip_update(&harness) ||
        !scene_test_win32_tip_key(&harness, 'B', false, false) ||
        !harness.controls[0U].current.cancel ||
        !scene_test_win32_tip_update(&harness) ||
        !scene_test_win32_tip_key(&harness, 'X', false, true) ||
        harness.controls[0U].current.cancel ||
        !scene_test_win32_tip_update(&harness)) {
        failure = "physical B interfered with the held-X keyboard state";
        goto cleanup;
    }

    /* A mapped X down/up pair drained before the next update must still
       reach one visible contest frame, then release without sticking. */
    if (!scene_test_win32_tip_harness_relaunch(&harness, &launch) ||
        !scene_test_win32_tip_advance_to_contest(&harness, 661U) ||
        !scene_test_win32_tip_key(&harness, 'X', true, true) ||
        !scene_test_win32_tip_key(&harness, 'X', false, true) ||
        harness.controls[0U].current.cancel ||
        !scene_test_win32_tip_update_observe(
            &harness, &effective_cancel, NULL) ||
        !effective_cancel ||
        scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST ||
        scene->pretip_state.phase_frame != 1U ||
        !scene->pretip_jump_active ||
        !scene->pretip_state.away_tip_sampled ||
        scene->pretip_state.away_tip_sample_frame != 0U ||
        !scene_test_win32_tip_update_observe(
            &harness, &effective_cancel, &released_cancel) ||
        effective_cancel || !released_cancel ||
        scene->pretip_state.phase_frame != 2U ||
        !scene->pretip_state.away_tip_sampled ||
        scene->pretip_state.away_tip_sample_frame != 0U) {
        failure = "fast X down/up before the update did not pulse one contest frame";
        goto cleanup;
    }

    /* Pressing X before contest and releasing it before the first contest
       update must not create a stale tip sample. */
    if (!scene_test_win32_tip_harness_relaunch(&harness, &launch) ||
        !scene_test_win32_tip_advance(&harness, 660U) ||
        scene->pretip_state.phase == TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST ||
        !scene_test_win32_tip_key(&harness, 'X', true, true) ||
        !scene_test_win32_tip_update(&harness) ||
        scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST ||
        scene->pretip_state.phase_frame != 0U ||
        scene->pretip_state.away_tip_sampled ||
        !scene_test_win32_tip_key(&harness, 'X', false, true) ||
        !scene_test_win32_tip_update(&harness) ||
        scene->pretip_state.away_tip_sampled ||
        scene->pretip_state.home_tip_sampled) {
        failure = "X released before contest was incorrectly sampled";
        goto cleanup;
    }

    /* Player 1's X must not act for an unassigned controller. */
    launch.controller_team[0U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    launch.controller_team[1U] = TECMO_GAMEPLAY_TEAM_HOME;
    if (!scene_test_win32_tip_harness_relaunch(&harness, &launch) ||
        !scene_test_win32_tip_advance_to_contest(&harness, 661U) ||
        !scene_test_win32_tip_key(&harness, 'X', true, true) ||
        !scene_test_win32_tip_update(&harness) ||
        scene->pretip_state.away_tip_sampled ||
        scene->pretip_state.home_tip_sampled) {
        failure = "unassigned Player 1 X incorrectly sampled the tip";
        goto cleanup;
    }
    ok = true;

cleanup:
    if (!ok) {
        tecmo_gameplay_scene_test_message(message, message_size, failure);
    }
    scene_test_win32_tip_harness_destroy(&harness);
    return ok;
}

bool tecmo_gameplay_scene_test_pretip_human_checkpoint(
    const char *project_root,
    const char *asset_pack_path,
    TecmoMusicPlayer *music_player,
    char *message,
    size_t message_size)
{
    TecmoGameplayScene scene;
    TecmoGameplaySceneLaunch launch;
    TecmoGameplaySceneActor first_actors[
        TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayState first_state;
    memset(&scene, 0, sizeof(scene));
    memset(&launch, 0, sizeof(launch));
    tecmo_gameplay_scene_init(&scene);
    tecmo_gameplay_scene_test_set_skip_pretip(false);
    launch.source = TECMO_GAMEPLAY_SCENE_PRESEASON;
    launch.away_team = 0U;
    launch.home_team = 1U;
    launch.regulation_minutes = 2U;
    launch.difficulty = 1U;
    launch.control_mode = 1U;
    launch.speed_value = 1U;
    launch.controller_team[0U] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch.controller_team[1U] = TECMO_GAMEPLAY_TEAM_HOME;
    launch.game_music_enabled = false;
    if (!tecmo_gameplay_scene_load(
            &scene, project_root, asset_pack_path, music_player) ||
        !scene_test_run_late_human_tip(&scene, &launch)) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            scene.status[0] != '\0' ? scene.status
                                     : "TPTI human checkpoint route failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memcpy(first_actors, scene.actors, sizeof(first_actors));
    first_state = scene.state;
    tecmo_gameplay_scene_end(&scene);
    if (!scene_test_run_late_human_tip(&scene, &launch) ||
        memcmp(first_actors, scene.actors, sizeof(first_actors)) != 0 ||
        memcmp(&first_state, &scene.state, sizeof(first_state)) != 0 ||
        scene.frame != 721U || scene.state.possession !=
            TECMO_GAMEPLAY_TEAM_AWAY || scene.ball_holder != 3U ||
        scene.pretip_state.away_tip_sample_frame != 29U ||
        scene.pretip_state.away_tip_error != 11U ||
        scene.pretip_jump_active) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "TPTI human checkpoint was nondeterministic or did not land");
        tecmo_gameplay_scene_end(&scene);
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_scene_end(&scene);
    if (!scene_test_run_win32_x_tip(
            project_root, asset_pack_path, music_player,
            message, message_size)) {
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_scene_test_message(
        message, message_size,
        "TPTI-2 human checkpoint PASS frame=721 late-sample=29 win32-X=assigned-Away-frame-0 fast-X=one-frame B-unmapped");
    tecmo_gameplay_scene_destroy(&scene);
    return true;
}
