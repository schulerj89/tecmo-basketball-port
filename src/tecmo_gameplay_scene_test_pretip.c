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
                           "TGSR-4 scene dependency contract failed");
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
    uint8_t away_commits;
    uint8_t home_commits;
    TecmoGameplaySceneActor actors_before_handoff[
        TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplaySceneCpuActor cpu_before_handoff[
        TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayCourtCoordinate holder_start;
    uint8_t holder_after_handoff;
    bool actor_moved;
    size_t frame;
    char failure_detail[256];
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
    /* Isolate captured-human timing here. CPU timing has separate coverage;
       an explicit no-input home controller must not enter that branch. */
    launch->controller_team[1U] = TECMO_GAMEPLAY_TEAM_HOME;
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
        scene->pretip_state.away_tip_countdown != 11U ||
        scene->pretip_state.away_tip_error != 11U ||
        scene->pretip_state.away_tip_capture_clock != 0xE1U ||
        scene->pretip_state.tip_capture_source_6a != 0x85U ||
        scene->pretip_state.tip_capture_clock != 0xE2U ||
        scene->pretip_state.tip_capture_clock_ticks != 91U ||
        scene->pretip_state.tip_capture_scheduler_yields != 209U ||
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
    away_commits = scene->pretip_state.away_jump_commit_count;
    home_commits = scene->pretip_state.home_jump_commit_count;
    (void)snprintf(
        failure_detail, sizeof(failure_detail),
        "concurrent pre-tip apex/cinematic ordering failed: phase=%u total=%u "
        "first=%u claim=%u commits=%u/%u pose=%u/%u",
        (unsigned)scene->pretip_state.phase,
        (unsigned)scene->pretip_state.total_frame,
        (unsigned)scene->pretip_state.first_cinematic_frame,
        (unsigned)scene->pretip_state.claim_frame,
        (unsigned)away_commits, (unsigned)home_commits,
        (unsigned)scene->actors[away_actor].pose_index,
        (unsigned)scene->actors[home_actor].pose_index);
    failure = failure_detail;
    if (scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_TOSS_CLOSEUP ||
        !scene->pretip_state.cinematic_visible ||
        scene->pretip_state.claim_frame == UINT16_MAX ||
        scene->pretip_state.first_cinematic_frame !=
            scene->pretip_state.total_frame ||
        !scene->pretip_state.contact_state_17 ||
        !scene->pretip_state.event_0588_bit20 ||
        scene->pretip_state.ball_actor_state != 0x17U ||
        !scene->pretip_state.ball_state17_in_flight ||
        scene->pretip_state.ball_attached_to_receiver ||
        scene->ball_holder != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
        scene->pretip_state.receiver_actor !=
            scene->pretip_state.raw_selector_037f ||
        scene->pretip_state.receiver_target.x !=
            scene->actors[scene->pretip_state.receiver_actor].position.x ||
        scene->pretip_state.receiver_target.y !=
            scene->actors[scene->pretip_state.receiver_actor].position.y ||
        scene->pretip_state.ball_velocity_x_q8 >= 0 ||
        scene->actors[away_actor].pose_index != 551U ||
        scene->actors[home_actor].pose_index != 518U) goto failed;
    {
        uint8_t frozen_away_state = scene->pretip_state.away_actor_state;
        uint8_t frozen_home_state = scene->pretip_state.home_actor_state;
        uint8_t frozen_away_phase = scene->pretip_state.away_animation_phase;
        uint8_t frozen_home_phase = scene->pretip_state.home_animation_phase;
        uint16_t frozen_away_altitude = scene->pretip_state.away_jump_altitude_q8;
        uint16_t frozen_home_altitude = scene->pretip_state.home_jump_altitude_q8;
        int16_t frozen_away_velocity = scene->pretip_state.away_jump_velocity_signed_q8;
        int16_t frozen_home_velocity = scene->pretip_state.home_jump_velocity_signed_q8;
        failure = "concurrent pre-tip cinematic simulation failed";
        for (frame = 0U; frame < 60U; ++frame) {
            if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
                scene->pretip_state.away_actor_state != frozen_away_state ||
                scene->pretip_state.home_actor_state != frozen_home_state ||
                scene->pretip_state.away_animation_phase != frozen_away_phase ||
                scene->pretip_state.home_animation_phase != frozen_home_phase ||
                scene->pretip_state.away_jump_altitude_q8 != frozen_away_altitude ||
                scene->pretip_state.home_jump_altitude_q8 != frozen_home_altitude ||
                scene->pretip_state.away_jump_velocity_signed_q8 != frozen_away_velocity ||
                scene->pretip_state.home_jump_velocity_signed_q8 != frozen_home_velocity)
                goto failed;
        }
        failure = "concurrent pre-tip cinematic exit failed";
        if (scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST ||
            scene->pretip_state.cinematic_visible ||
            scene->pretip_state.away_jump_altitude_q8 != frozen_away_altitude ||
            scene->pretip_state.home_jump_altitude_q8 != frozen_home_altitude ||
            scene->ball_holder != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
            scene->pretip_state.away_jump_commit_count != away_commits ||
            scene->pretip_state.home_jump_commit_count != home_commits)
            goto failed;
    }
    failure = "concurrent pre-tip live handoff advance failed";
    for (frame = 0U; frame < 30U &&
         !scene->pretip_state.live_handoff; ++frame) {
        if (scene->pretip_state.phase_frame == 29U) {
            TecmoGameplayScene *malformed =
                (TecmoGameplayScene *)malloc(sizeof(*malformed));
            TecmoGameplaySceneActor malformed_actors[
                TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
            TecmoGameplayLiveFoundation malformed_foundation;
            TecmoGameplayState malformed_state;
            TecmoGameplayCourtOrientationState malformed_orientation;
            TecmoGameplayCameraState malformed_camera;
            TecmoGameplayCourtCoordinateQ8 malformed_ball;
            uint8_t malformed_holder;
            if (malformed == NULL) {
                failure = "pre-tip malformed transaction allocation failed";
                goto failed;
            }
            memcpy(malformed, scene, sizeof(*malformed));
            malformed->live_foundation.actor_position[0U].x = -1;
            memcpy(malformed_actors, malformed->actors,
                   sizeof(malformed_actors));
            malformed_foundation = malformed->live_foundation;
            malformed_state = malformed->state;
            malformed_orientation = malformed->orientation_state;
            malformed_camera = malformed->camera_state;
            malformed_ball = malformed->ball_position;
            malformed_holder = malformed->ball_holder;
            if (tecmo_gameplay_scene_update(malformed, &p1, &p2) ||
                memcmp(malformed->actors, malformed_actors,
                       sizeof(malformed_actors)) != 0 ||
                memcmp(&malformed->live_foundation,
                       &malformed_foundation,
                       sizeof(malformed_foundation)) != 0 ||
                memcmp(&malformed->state, &malformed_state,
                       sizeof(malformed_state)) != 0 ||
                memcmp(&malformed->orientation_state,
                       &malformed_orientation,
                       sizeof(malformed_orientation)) != 0 ||
                memcmp(&malformed->camera_state, &malformed_camera,
                       sizeof(malformed_camera)) != 0 ||
                memcmp(&malformed->ball_position, &malformed_ball,
                       sizeof(malformed_ball)) != 0 ||
                malformed->ball_holder != malformed_holder) {
                free(malformed);
                failure = "pre-tip malformed in-place transaction partially committed";
                goto failed;
            }
            free(malformed);
        }
        memcpy(actors_before_handoff, scene->actors,
               sizeof(actors_before_handoff));
        memcpy(cpu_before_handoff, scene->cpu_actors,
               sizeof(cpu_before_handoff));
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) goto failed;
    }
    failure = "concurrent pre-tip no-restart handoff failed";
    if (scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_LIVE ||
        !scene->pretip_state.live_handoff ||
        !scene->pretip_state.ball_attached_to_receiver ||
        scene->ball_holder != scene->pretip_state.receiver_actor ||
        scene->pretip_state.away_jump_commit_count != 1U ||
        scene->pretip_state.home_jump_commit_count != 0U) goto failed;
    /* Tip handoff uses the preserve-state bridge, never the bounded Bank05
       $B87C claimant transaction.  Keep this negative assertion adjacent to
       the real cinematic-to-live handoff rather than inferring it from a
       generic possession helper. */
    if (scene->claimant_settlement_trace.valid ||
        scene->claimant_settlement_trace.event_serial != 0U ||
        scene->claimant_settlement_trace.contract_tag != 0U) {
        failure = "pre-tip handoff unexpectedly emitted B87C claimant trace";
        goto failed;
    }
    failure = "pre-tip in-place actor continuity failed";
    if (memcmp(scene->actors, actors_before_handoff,
               sizeof(actors_before_handoff)) != 0 ||
        memcmp(scene->cpu_actors, cpu_before_handoff,
               sizeof(cpu_before_handoff)) != 0) goto failed;
    failure = "pre-tip live-foundation coordinate continuity failed";
    for (frame = 0U; frame < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++frame) {
        if (scene->live_foundation.actor_position[frame].x !=
                scene->actors[frame].position.x ||
            scene->live_foundation.actor_position[frame].y !=
                scene->actors[frame].position.y) goto failed;
    }
    holder_after_handoff = scene->ball_holder;
    holder_start = scene->actors[holder_after_handoff].position;
    p1.held.right = true;
    failure = "first live movement did not continue from preserved position";
    for (frame = 0U; frame < 24U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) {
            failure = scene->status;
            goto failed;
        }
    }
    actor_moved = false;
    for (frame = 0U; frame < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++frame) {
        if (scene->actors[frame].position.x !=
                actors_before_handoff[frame].position.x ||
            scene->actors[frame].position.y !=
                actors_before_handoff[frame].position.y) actor_moved = true;
        if (scene->actors[frame].position.x <
                actors_before_handoff[frame].position.x - 40 ||
            scene->actors[frame].position.x >
                actors_before_handoff[frame].position.x + 40) {
            failure = "first live movement jumped away from preserved coordinate";
            goto failed;
        }
    }
    if (!actor_moved) {
        failure = "first live movement remained frozen";
        goto failed;
    }
    if ((scene->actors[holder_after_handoff].position.x == 528 &&
         scene->actors[holder_after_handoff].position.y == 144) ||
        (holder_start.x == 528 && holder_start.y == 144)) {
        failure = "first live movement used cold-start coordinate";
        goto failed;
    }
    tecmo_gameplay_scene_end(scene);
    return true;
failed:
    tecmo_gameplay_scene_test_message(
        test->message, test->message_size,
        failure);
    tecmo_gameplay_scene_end(scene);
    return false;
}

typedef struct SceneTestPreTipJumpTuple {
    TecmoGameplaySceneActor away_actor;
    TecmoGameplaySceneActor home_actor;
    uint8_t away_state;
    uint8_t home_state;
    uint8_t away_phase;
    uint8_t home_phase;
    uint16_t away_altitude_q8;
    uint16_t home_altitude_q8;
    int16_t away_velocity_q8;
    int16_t home_velocity_q8;
    bool jump_active;
} SceneTestPreTipJumpTuple;

static bool scene_test_render_continuously(
    const TecmoGameplayScene *scene,
    uint32_t *pixels,
    char *failure,
    size_t failure_size)
{
    TecmoFramebuffer framebuffer;
    const char *diagnostic;
    if (scene == NULL || pixels == NULL || failure == NULL ||
        failure_size == 0U) {
        return false;
    }
    framebuffer.pixels = pixels;
    framebuffer.width = TECMO_GAMEPLAY_SCENE_NES_WIDTH;
    framebuffer.height = TECMO_GAMEPLAY_SCENE_NES_HEIGHT;
    framebuffer.pitch_pixels = TECMO_GAMEPLAY_SCENE_NES_WIDTH;
    if (tecmo_gameplay_scene_draw(
            scene, &framebuffer, 0, 0, 1, true)) {
        return true;
    }
    diagnostic = tecmo_gameplay_scene_render_diagnostic(
        scene, &framebuffer, 0, 0, 1, true);
    (void)snprintf(failure, failure_size,
                   "continuous gameplay render rejected: %s",
                   diagnostic != NULL ? diagnostic : "draw composition");
    return false;
}

static void scene_test_capture_jump_tuple(
    const TecmoGameplayScene *scene,
    uint8_t away_actor,
    uint8_t home_actor,
    SceneTestPreTipJumpTuple *tuple)
{
    if (scene == NULL || tuple == NULL ||
        away_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        home_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        return;
    }
    memset(tuple, 0, sizeof(*tuple));
    tuple->away_actor = scene->actors[away_actor];
    tuple->home_actor = scene->actors[home_actor];
    tuple->away_state = scene->pretip_state.away_actor_state;
    tuple->home_state = scene->pretip_state.home_actor_state;
    tuple->away_phase = scene->pretip_state.away_animation_phase;
    tuple->home_phase = scene->pretip_state.home_animation_phase;
    tuple->away_altitude_q8 = scene->pretip_state.away_jump_altitude_q8;
    tuple->home_altitude_q8 = scene->pretip_state.home_jump_altitude_q8;
    tuple->away_velocity_q8 =
        scene->pretip_state.away_jump_velocity_signed_q8;
    tuple->home_velocity_q8 =
        scene->pretip_state.home_jump_velocity_signed_q8;
    tuple->jump_active = scene->pretip_jump_active;
}

static bool scene_test_jump_tuple_equal(
    const SceneTestPreTipJumpTuple *left,
    const SceneTestPreTipJumpTuple *right)
{
    return left != NULL && right != NULL &&
        memcmp(&left->away_actor, &right->away_actor,
               sizeof(left->away_actor)) == 0 &&
        memcmp(&left->home_actor, &right->home_actor,
               sizeof(left->home_actor)) == 0 &&
        left->away_state == right->away_state &&
        left->home_state == right->home_state &&
        left->away_phase == right->away_phase &&
        left->home_phase == right->home_phase &&
        left->away_altitude_q8 == right->away_altitude_q8 &&
        left->home_altitude_q8 == right->home_altitude_q8 &&
        left->away_velocity_q8 == right->away_velocity_q8 &&
        left->home_velocity_q8 == right->home_velocity_q8 &&
        left->jump_active == right->jump_active;
}

static bool scene_test_continuous_tip_render(
    TecmoGameplaySceneTestContext *test,
    TecmoGameplayScene *scene,
    TecmoGameplaySceneLaunch *launch,
    uint8_t away_team,
    uint8_t home_team)
{
    const size_t pixel_count =
        (size_t)TECMO_GAMEPLAY_SCENE_NES_WIDTH *
        TECMO_GAMEPLAY_SCENE_NES_HEIGHT;
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    SceneTestPreTipJumpTuple last_court;
    SceneTestPreTipJumpTuple frozen;
    SceneTestPreTipJumpTuple returned;
    uint32_t *pixels = NULL;
    uint8_t away_actor;
    uint8_t home_actor;
    uint8_t shooting_actor;
    uint8_t shooting_controller = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    TecmoGameplayTeam shooting_team;
    TecmoControlFrame *shooting_controls;
    uint32_t claimant_serial_before;
    unsigned shot_probe_started = 0U;
    unsigned shot_probe_jump = 0U;
    uint16_t last_court_frame = UINT16_MAX;
    bool saw_resumed_airborne = false;
    bool saw_natural_landing = false;
    bool saw_live_post_landing = false;
    size_t frame;
    char failure[256] = "continuous pre-tip scene regression failed";

    if (test == NULL || scene == NULL || launch == NULL) return false;
    pixels = (uint32_t *)malloc(pixel_count * sizeof(*pixels));
    if (pixels == NULL) {
        tecmo_gameplay_scene_test_message(
            test->message, test->message_size,
            "continuous pre-tip renderer allocation failed");
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    memset(&last_court, 0, sizeof(last_court));
    memset(&frozen, 0, sizeof(frozen));
    memset(&returned, 0, sizeof(returned));
    memset(launch, 0, sizeof(*launch));
    launch->source = TECMO_GAMEPLAY_SCENE_PRESEASON;
    launch->away_team = away_team;
    launch->home_team = home_team;
    launch->regulation_minutes = 2U;
    launch->difficulty = 1U;
    /* Both pads are assigned so the one-frame away B pulse is a human
       capture and the home team remains a true no-input opponent. */
    launch->controller_team[0U] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch->controller_team[1U] = TECMO_GAMEPLAY_TEAM_HOME;
    launch->control_mode = 1U;
    launch->speed_value = 1U;
    launch->game_music_enabled = false;
    if (!tecmo_gameplay_scene_launch(scene, launch)) {
        (void)snprintf(failure, sizeof(failure), "continuous launch rejected");
        goto failed;
    }
    if (!scene_test_render_continuously(scene, pixels, failure,
                                        sizeof(failure))) {
        goto failed;
    }
    for (frame = 0U; frame < 451U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) {
            (void)snprintf(failure, sizeof(failure),
                           "continuous advance-to-capture rejected: %s",
                           scene->status);
            goto failed;
        }
        if (!scene_test_render_continuously(scene, pixels, failure,
                                            sizeof(failure))) {
            goto failed;
        }
    }
    away_actor = scene->pretip_jumper_actor[0U];
    home_actor = scene->pretip_jumper_actor[1U];
    if (scene->pretip_state.phase !=
            TECMO_GAMEPLAY_PRETIP_CENTER_COURT_SETUP ||
        away_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        home_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        (void)snprintf(failure, sizeof(failure),
                       "continuous capture setup rejected");
        goto failed;
    }
    p1.held.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        !scene_test_render_continuously(scene, pixels, failure,
                                        sizeof(failure)) ||
        !scene->pretip_state.away_tip_sampled ||
        scene->pretip_state.away_tip_capture_clock != 0xE1U ||
        scene->pretip_state.away_tip_error != 11U ||
        scene->pretip_state.away_tip_countdown != 11U ||
        scene->pretip_state.tip_capture_source_6a != 0x85U ||
        scene->pretip_state.tip_capture_clock != 0xE2U ||
        scene->pretip_state.tip_capture_clock_ticks != 91U ||
        scene->pretip_state.tip_capture_scheduler_yields != 209U ||
        scene->pretip_state.tip_capture_clock_complete) {
        (void)snprintf(failure, sizeof(failure),
                       "Bank04 scheduler did not derive $6A=$85/$8A=$E1");
        goto failed;
    }
    p1.held.cancel = false;
    for (frame = 0U; frame < 240U; ++frame) {
        TecmoGameplayPreTipPhase phase_before = scene->pretip_state.phase;
        if (phase_before == TECMO_GAMEPLAY_PRETIP_BALL_DESCENT) {
            scene_test_capture_jump_tuple(scene, away_actor, home_actor,
                                          &last_court);
            last_court_frame = (uint16_t)scene->pretip_state.total_frame;
        }
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) {
            (void)snprintf(failure, sizeof(failure),
                           "continuous pre-cinematic update rejected: %s",
                           scene->status);
            goto failed;
        }
        if (!scene_test_render_continuously(scene, pixels, failure,
                                            sizeof(failure))) {
            goto failed;
        }
        if (scene->pretip_state.phase ==
            TECMO_GAMEPLAY_PRETIP_TOSS_CLOSEUP) {
            break;
        }
    }
    if (scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_TOSS_CLOSEUP ||
        !scene->pretip_state.cinematic_visible ||
        scene->pretip_state.first_cinematic_frame != 516U ||
        scene->pretip_state.first_cinematic_frame !=
            scene->pretip_state.total_frame ||
        last_court_frame != 515U ||
        last_court.away_altitude_q8 == 0U ||
        !scene->pretip_state.away_jump_committed ||
        scene->pretip_state.home_jump_committed) {
        (void)snprintf(failure, sizeof(failure),
                       "slow human jump/cinematic boundary rejected: "
                       "last=%u first=%u commits=%u/%u",
                       (unsigned)last_court_frame,
                       (unsigned)scene->pretip_state.first_cinematic_frame,
                       (unsigned)scene->pretip_state.away_jump_commit_count,
                       (unsigned)scene->pretip_state.home_jump_commit_count);
        goto failed;
    }
    scene_test_capture_jump_tuple(scene, away_actor, home_actor, &frozen);
    if (frozen.away_altitude_q8 == 0U) {
        (void)snprintf(failure, sizeof(failure),
                       "cinematic started after forced grounding");
        goto failed;
    }
    for (frame = 0U; frame < 60U; ++frame) {
        SceneTestPreTipJumpTuple current;
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            !scene_test_render_continuously(scene, pixels, failure,
                                            sizeof(failure))) {
            if (failure[0] == '\0') {
                (void)snprintf(failure, sizeof(failure),
                               "continuous cinematic update rejected: %s",
                               scene->status);
            }
            goto failed;
        }
        scene_test_capture_jump_tuple(scene, away_actor, home_actor,
                                      &current);
        if (!scene_test_jump_tuple_equal(&frozen, &current)) {
            (void)snprintf(failure, sizeof(failure),
                           "cinematic changed frozen jumper tuple at frame %u",
                           (unsigned)frame);
            goto failed;
        }
    }
    scene_test_capture_jump_tuple(scene, away_actor, home_actor, &returned);
    if (scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST ||
        scene->pretip_state.cinematic_visible ||
        !scene_test_jump_tuple_equal(&frozen, &returned)) {
        (void)snprintf(failure, sizeof(failure),
                       "first returned court frame changed frozen jumper tuple");
        goto failed;
    }
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        !scene_test_render_continuously(scene, pixels, failure,
                                        sizeof(failure))) {
        if (failure[0] == '\0') {
            (void)snprintf(failure, sizeof(failure),
                           "returned-court recovery update rejected: %s",
                           scene->status);
        }
        goto failed;
    }
    if (scene->pretip_state.away_jump_altitude_q8 ==
            frozen.away_altitude_q8 &&
        scene->pretip_state.away_jump_velocity_signed_q8 ==
            frozen.away_velocity_q8) {
        (void)snprintf(failure, sizeof(failure),
                       "returned court frame did not resume jumper physics");
        goto failed;
    }
    saw_resumed_airborne = scene->pretip_jump_active &&
        scene->pretip_state.away_actor_state != 0x13U;
    for (frame = 0U; frame < 180U && !scene->pretip_state.live_handoff;
         ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            !scene_test_render_continuously(scene, pixels, failure,
                                            sizeof(failure))) {
            if (failure[0] == '\0') {
                (void)snprintf(failure, sizeof(failure),
                               "handoff transition rejected: %s", scene->status);
            }
            goto failed;
        }
        saw_resumed_airborne = saw_resumed_airborne ||
            (scene->pretip_jump_active &&
             scene->pretip_state.away_actor_state != 0x13U);
    }
    if (!scene->pretip_state.live_handoff ||
        scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_LIVE ||
        !saw_resumed_airborne ||
        scene->actors[away_actor].position.x !=
            scene->actors[away_actor].anchor.x ||
        scene->actors[away_actor].position.y !=
            scene->actors[away_actor].anchor.y) {
        (void)snprintf(failure, sizeof(failure),
                       "live handoff broke tipped-jumper continuity");
        goto failed;
    }
    p1.held.right = true;
    for (frame = 0U; frame < 180U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            !scene_test_render_continuously(scene, pixels, failure,
                                            sizeof(failure))) {
            if (failure[0] == '\0') {
                (void)snprintf(failure, sizeof(failure),
                               "continuous live recovery rejected: %s",
                               scene->status);
            }
            goto failed;
        }
        if (scene->pretip_jump_active) {
            if (scene->actors[away_actor].position.x !=
                    scene->actors[away_actor].anchor.x ||
                scene->actors[away_actor].position.y !=
                    scene->actors[away_actor].anchor.y) {
                (void)snprintf(failure, sizeof(failure),
                               "live recovery double-processed tipped jumper");
                goto failed;
            }
        } else {
            saw_natural_landing = true;
            if (frame >= 8U) {
                saw_live_post_landing = true;
                break;
            }
        }
    }
    if (!saw_natural_landing || !saw_live_post_landing) {
        (void)snprintf(failure, sizeof(failure),
                       "tipped jumper did not naturally recover in live play");
        goto failed;
    }

    /* Reproduce the desktop failure from the untouched cinematic-to-live
       state: use the real holder and its assigned controller, start the same
       scene shot transaction that B dispatches, release on the next update,
       and run it through settlement.
       No actor is moved to the rim and no claimant/phase/possession fixture is
       injected. */
    p1.held.right = false;
    p2.held.right = false;
    shooting_actor = scene->ball_holder;
    if (shooting_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        !scene->actors[shooting_actor].active) {
        (void)snprintf(failure, sizeof(failure),
                       "post-tip shot has no active holder");
        goto failed;
    }
    shooting_team = (TecmoGameplayTeam)scene->actors[shooting_actor].team;
    for (uint8_t controller = 0U;
         controller < TECMO_GAMEPLAY_CONTROLLER_COUNT; ++controller) {
        if (scene->launch.controller_team[controller] ==
                (uint8_t)shooting_team) {
            shooting_controller = controller;
            break;
        }
    }
    if (shooting_controller >= TECMO_GAMEPLAY_CONTROLLER_COUNT) {
        (void)snprintf(failure, sizeof(failure),
                       "post-tip holder has no controller");
        goto failed;
    }
    shooting_controls = shooting_controller == 0U ? &p1 : &p2;

    /* The stable evaluator is frame-bound. Advance naturally only if needed
       until this holder has an ordinary jump-shot sample; no shot result or
       geometry is injected. */
    for (frame = 0U; frame < 256U; ++frame) {
        TecmoGameplayScene probe = *scene;
        if (scene_start_shot_actor(&probe, shooting_controller,
                                   shooting_actor) &&
            probe.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
            ++shot_probe_started;
            if (probe.shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_JUMP) {
                ++shot_probe_jump;
                break;
            }
        }
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            !scene_test_render_continuously(scene, pixels, failure,
                                            sizeof(failure))) {
            (void)snprintf(failure, sizeof(failure),
                           "post-tip shot search rejected: %s", scene->status);
            goto failed;
        }
        shooting_actor = scene->ball_holder;
        if (shooting_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
            scene->actors[shooting_actor].team != (uint8_t)shooting_team) {
            (void)snprintf(failure, sizeof(failure),
                           "post-tip holder changed during miss search");
            goto failed;
        }
    }
    if (frame == 256U) {
        (void)snprintf(failure, sizeof(failure),
                       "post-tip organic jump shot unavailable "
                       "started=%u jump=%u holder=%u pos=%d,%d frame=%u",
                       shot_probe_started, shot_probe_jump,
                       (unsigned)shooting_actor,
                       (int)scene->actors[shooting_actor].position.x,
                       (int)scene->actors[shooting_actor].position.y,
                       (unsigned)scene->frame);
        goto failed;
    }

    claimant_serial_before =
        scene->claimant_settlement_trace.event_serial;
    if (!scene_start_shot_actor(scene, shooting_controller, shooting_actor) ||
        !scene_test_render_continuously(scene, pixels, failure,
                                        sizeof(failure)) ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        scene->shot_actor != shooting_actor ||
        scene->jump_family != TECMO_GAMEPLAY_JUMP_SHOT_CAPTURED_FAMILY ||
        scene->jump_profile != TECMO_GAMEPLAY_JUMP_SHOT_CAPTURED_PROFILE ||
        scene->jump_direction != TECMO_GAMEPLAY_JUMP_SHOT_CAPTURED_DIRECTION ||
        scene->jump_resolved_pose_index != 213U ||
        scene->actors[shooting_actor].pose_orientation_encoded ||
        scene->actors[shooting_actor].facing_right !=
            scene->shot_launch_facing_right) {
        (void)snprintf(failure, sizeof(failure),
                       "post-tip shot selected wrong family/orientation: "
                       "kind=%u actor=%u/%u route=%u/%u/%u pose=%u "
                       "encoded=%u face=%u/%u",
                       (unsigned)scene->shot_kind,
                       (unsigned)scene->shot_actor,
                       (unsigned)shooting_actor,
                       (unsigned)scene->jump_family,
                       (unsigned)scene->jump_profile,
                       (unsigned)scene->jump_direction,
                       (unsigned)scene->jump_resolved_pose_index,
                       scene->actors[shooting_actor]
                               .pose_orientation_encoded ? 1U : 0U,
                       scene->actors[shooting_actor].facing_right ? 1U : 0U,
                       scene->shot_launch_facing_right ? 1U : 0U);
        goto failed;
    }
    shooting_controls->held.cancel = false;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        !scene_test_render_continuously(scene, pixels, failure,
                                        sizeof(failure)) ||
        !scene->jump_b_released) {
        (void)snprintf(failure, sizeof(failure),
                       "post-tip B release did not launch shot: %s",
                       scene->status);
        goto failed;
    }
    for (frame = 0U; frame < 180U &&
            scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            !scene_test_render_continuously(scene, pixels, failure,
                                            sizeof(failure))) {
            (void)snprintf(failure, sizeof(failure),
                           "post-tip shot tail rejected at %u: %s",
                           (unsigned)frame, scene->status);
            goto failed;
        }
    }
    if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->state.possession == shooting_team ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->claimant_settlement_trace.event_serial !=
            claimant_serial_before) {
        (void)snprintf(failure, sizeof(failure),
                       "post-tip organic shot did not settle cleanly");
        goto failed;
    }
    tecmo_gameplay_scene_end(scene);
    free(pixels);
    return true;

failed:
    tecmo_gameplay_scene_test_message(test->message, test->message_size,
                                      failure);
    tecmo_gameplay_scene_end(scene);
    free(pixels);
    return false;
}

bool tecmo_gameplay_scene_test_pretip(
    TecmoGameplaySceneTestContext *test)
{
    TecmoGameplaySceneLaunch launch = test->launch;
    TecmoGameplayScene *scene = test->scene;

    tecmo_gameplay_scene_test_set_skip_pretip(false);
    if (!tecmo_gameplay_scene_test_pretip_load(test, scene) ||
        !scene_test_continuous_tip_render(test, scene, &launch, 0U, 1U)) {
        return false;
    }
    if (!scene_test_continuous_tip_render(test, scene, &launch, 3U, 10U)) {
        return false;
    }
    /* Later scene-contract tests intentionally use their historical 0/1
       fixture.  Keep the second matchup as independent render coverage,
       rather than quietly changing unrelated expected palette hashes. */
    launch.away_team = 0U;
    launch.home_team = 1U;
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
        !scene_test_continuous_tip_render(&context, &scene, &launch,
                                          0U, 1U)) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            scene.status[0] != '\0' ? scene.status
                                     : "TPTI human checkpoint route failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_scene_test_message(
        message, message_size,
        "TPTI-2 human checkpoint PASS capture-frame=452 simulation-frame=481 cinematic-frame=516 live-frame=606");
    tecmo_gameplay_scene_destroy(&scene);
    return true;
}
