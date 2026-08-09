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

static bool scene_test_concurrent_tip_simulation(
    TecmoGameplaySceneTestContext *test,
    TecmoGameplayScene *scene,
    TecmoGameplaySceneLaunch *launch)
{
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    uint8_t away_actor;
    uint8_t home_actor;
    uint16_t apex_tick;
    uint8_t away_commits;
    uint8_t home_commits;
    size_t frame;
    const char *failure = "concurrent pre-tip scene regression failed";
    if (test == NULL || scene == NULL || launch == NULL) return false;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    memset(launch, 0, sizeof(*launch));
    launch->source = TECMO_GAMEPLAY_SCENE_PRESEASON;
    launch->away_team = 0U;
    launch->home_team = 1U;
    launch->regulation_minutes = 2U;
    launch->difficulty = 1U;
    launch->controller_team[0U] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch->controller_team[1U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    launch->control_mode = 1U;
    launch->speed_value = 1U;
    launch->game_music_enabled = false;
    if (!tecmo_gameplay_scene_launch(scene, launch)) goto failed;
    failure = "concurrent pre-tip advance to capture failed";
    for (frame = 0U; frame < 451U; ++frame)
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) goto failed;
    away_actor = scene->pretip_jumper_actor[0U];
    home_actor = scene->pretip_jumper_actor[1U];
    failure = "concurrent pre-tip live object seed failed";
    if (scene->pretip_state.phase !=
            TECMO_GAMEPLAY_PRETIP_CENTER_COURT_SETUP ||
        scene->pretip_state.away_actor_state != 0x22U ||
        scene->pretip_state.home_actor_state != 0x13U ||
        scene->pretip_state.ball_actor_state != 0x1AU)
        goto failed;
    failure = "concurrent pre-tip Bank04 latch failed";
    p1.held.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        !scene->pretip_state.away_tip_sampled ||
        scene->pretip_state.away_tip_countdown != 0x0CU ||
        scene->pretip_state.away_jump_committed) goto failed;
    failure = "concurrent pre-tip center setup retention failed";
    p1.held.cancel = false;
    for (frame = 1U; frame < 30U; ++frame)
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) goto failed;
    failure = "concurrent pre-tip simulation boundary failed";
    if (scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_BALL_DESCENT ||
        scene->pretip_state.simulation_tick != 0U ||
        scene->pretip_state.simulation_active) goto failed;
    failure = "concurrent pre-tip ballistic/contact advance failed";
    for (frame = 0U; frame < 120U &&
         scene->pretip_state.phase == TECMO_GAMEPLAY_PRETIP_BALL_DESCENT;
         ++frame)
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) goto failed;
    apex_tick = scene->pretip_state.home_apex_frame;
    away_commits = scene->pretip_state.away_jump_commit_count;
    home_commits = scene->pretip_state.home_jump_commit_count;
    failure = "concurrent pre-tip apex/cinematic ordering failed";
    if (scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_TOSS_CLOSEUP ||
        !scene->pretip_state.cinematic_visible || apex_tick == 0U ||
        apex_tick >= scene->pretip_state.simulation_tick ||
        !scene->pretip_state.contact_state_17 ||
        !scene->pretip_state.event_0588_bit20 ||
        scene->pretip_state.ball_actor_state != 0x17U ||
        scene->actors[away_actor].pose_index != 551U ||
        scene->actors[home_actor].pose_index != 583U) goto failed;
    failure = "concurrent pre-tip cinematic simulation failed";
    for (frame = 0U; frame < 60U; ++frame)
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) goto failed;
    failure = "concurrent pre-tip cinematic exit failed";
    if (scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST ||
        scene->pretip_state.cinematic_visible ||
        scene->actors[away_actor].pose_index != 469U ||
        scene->actors[home_actor].pose_index != 501U ||
        scene->pretip_state.away_jump_commit_count != away_commits ||
        scene->pretip_state.home_jump_commit_count != home_commits) goto failed;
    failure = "concurrent pre-tip live handoff advance failed";
    for (frame = 0U; frame < 30U; ++frame)
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) goto failed;
    failure = "concurrent pre-tip no-restart handoff failed";
    if (scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_LIVE ||
        !scene->pretip_state.live_handoff ||
        scene->pretip_state.away_jump_commit_count != 1U ||
        scene->pretip_state.home_jump_commit_count != 1U) goto failed;
    tecmo_gameplay_scene_end(scene);
    return true;
failed:
    tecmo_gameplay_scene_test_message(
        test->message, test->message_size,
        failure);
    tecmo_gameplay_scene_end(scene);
    return false;
}

bool tecmo_gameplay_scene_test_pretip(
    TecmoGameplaySceneTestContext *test)
{
    TecmoGameplaySceneLaunch launch = test->launch;
    TecmoGameplayScene *scene = test->scene;

    tecmo_gameplay_scene_test_set_skip_pretip(false);
    if (!tecmo_gameplay_scene_test_pretip_load(test, scene) ||
        !scene_test_concurrent_tip_simulation(test, scene, &launch)) {
        return false;
    }
    launch.controller_team[1U] = TECMO_GAMEPLAY_TEAM_HOME;
    launch.game_music_enabled = true;
    test->launch = launch;
    memset(&test->p1, 0, sizeof(test->p1));
    memset(&test->p2, 0, sizeof(test->p2));
    return true;
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
    TecmoGameplaySceneTestContext context;
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
    memset(&context, 0, sizeof(context));
    context.message = message;
    context.message_size = message_size;
    if (!tecmo_gameplay_scene_load(
            &scene, project_root, asset_pack_path, music_player) ||
        !scene_test_concurrent_tip_simulation(&context, &scene, &launch)) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            scene.status[0] != '\0' ? scene.status
                                     : "TPTI human checkpoint route failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_scene_test_message(
        message, message_size,
        "TPTI-2 human checkpoint PASS capture-frame=452 simulation-frame=481 cinematic-frame=500 live-frame=590");
    tecmo_gameplay_scene_destroy(&scene);
    return true;
}
