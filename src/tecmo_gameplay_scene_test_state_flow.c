#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "tecmo_gameplay_scene_test_internal.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool scene_test_free_throw_lineup_unbound(
    const TecmoGameplayScene *scene)
{
    TecmoGameplayFreeThrowLineup untouched;
    TecmoGameplayFreeThrowLineup expected;
    memset(&untouched, 0xA5, sizeof(untouched));
    expected = untouched;
    return scene != NULL &&
           !scene->free_throw_lineup_active &&
           scene->free_throw_lineup_transition_serial == 0U &&
           scene->free_throw_lineup_orientation ==
               TECMO_GAMEPLAY_FREE_THROW_LINEUP_UNDEFINED_INDEX &&
           scene->free_throw_shooter == TECMO_GAMEPLAY_SCENE_NO_ACTOR &&
           scene->free_throw_secondary == TECMO_GAMEPLAY_SCENE_NO_ACTOR &&
           !tecmo_gameplay_scene_free_throw_lineup(scene, &untouched) &&
           memcmp(&untouched, &expected, sizeof(untouched)) == 0;
}

static bool scene_test_free_throw_lineup_bound(
    const TecmoGameplayScene *scene,
    uint8_t expected_orientation,
    uint8_t expected_shooter,
    uint8_t expected_secondary,
    uint16_t expected_camera_x)
{
    TecmoGameplayFreeThrowLineup lineup;
    TecmoGameplaySceneCourtCoordinates coordinates;
    TecmoGameplaySceneCourtFrame frame;
    TecmoGameplayScene malformed;
    TecmoGameplayFreeThrowLineup rejected_lineup;
    TecmoGameplayFreeThrowLineup expected_rejected_lineup;
    TecmoGameplaySceneCourtCoordinates rejected_coordinates;
    TecmoGameplaySceneCourtCoordinates expected_rejected_coordinates;
    size_t actor;
    if (scene == NULL ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
        !scene->free_throw_lineup_active ||
        scene->free_throw_lineup_orientation != expected_orientation ||
        scene->free_throw_shooter != expected_shooter ||
        scene->free_throw_secondary != expected_secondary ||
        scene->free_throw_lineup_transition_serial !=
            scene->orientation_state.transition_serial ||
        scene->orientation_state.current_direction !=
            expected_orientation ||
        scene->camera_state.camera_x != expected_camera_x ||
        !tecmo_gameplay_scene_free_throw_lineup(scene, &lineup) ||
        !tecmo_gameplay_scene_court_coordinates(scene, &coordinates) ||
        !tecmo_gameplay_scene_court_frame(scene, &frame) ||
        lineup.orientation != expected_orientation ||
        lineup.shooter_slot != expected_shooter ||
        lineup.secondary_slot != expected_secondary ||
        !lineup.actors[expected_shooter].shooter ||
        lineup.actors[expected_shooter].secondary ||
        lineup.actors[expected_shooter].pose_defined ||
        lineup.actors[expected_shooter].raw_world_x !=
            (expected_orientation == 0U ? 250U : 518U) ||
        lineup.actors[expected_shooter].raw_world_y != 148U ||
        lineup.actors[expected_shooter].direction_index !=
            (expected_orientation == 0U ? 1U : 0U) ||
        lineup.actors[expected_secondary].shooter ||
        !lineup.actors[expected_secondary].secondary ||
        !lineup.actors[expected_secondary].pose_defined ||
        coordinates.contract_tag !=
            TECMO_GAMEPLAY_SCENE_COURT_COORDINATES_TAG ||
        frame.contract_tag != TECMO_GAMEPLAY_SCENE_COURT_FRAME_TAG ||
        frame.slice.direction != expected_orientation ||
        frame.slice.transition_serial !=
            scene->free_throw_lineup_transition_serial ||
        frame.slice.viewport.camera_x != expected_camera_x ||
        frame.projection.camera_x != expected_camera_x) {
        return false;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
         ++actor) {
        if (!lineup.actors[actor].position_defined ||
            scene->actors[actor].position.x !=
                (int16_t)lineup.actors[actor].raw_world_x ||
            scene->actors[actor].position.y !=
                (int16_t)lineup.actors[actor].raw_world_y ||
            scene->actors[actor].anchor.x !=
                scene->actors[actor].position.x ||
            scene->actors[actor].anchor.y !=
                scene->actors[actor].position.y ||
            coordinates.players[actor].x !=
                scene->actors[actor].position.x ||
            coordinates.players[actor].y !=
                scene->actors[actor].position.y) {
            return false;
        }
    }

    /*
     * Public snapshots are transactional. A stale or unreachable TGFL
     * binding must fail closed without exposing a partial lineup or court.
     */
    malformed = *scene;
    malformed.free_throw_lineup_orientation =
        TECMO_GAMEPLAY_FREE_THROW_LINEUP_UNDEFINED_INDEX;
    memset(&rejected_lineup, 0xA5, sizeof(rejected_lineup));
    expected_rejected_lineup = rejected_lineup;
    memset(&rejected_coordinates, 0x5A, sizeof(rejected_coordinates));
    expected_rejected_coordinates = rejected_coordinates;
    if (tecmo_gameplay_scene_free_throw_lineup(
            &malformed, &rejected_lineup) ||
        memcmp(&rejected_lineup, &expected_rejected_lineup,
               sizeof(rejected_lineup)) != 0 ||
        tecmo_gameplay_scene_court_coordinates(
            &malformed, &rejected_coordinates) ||
        memcmp(&rejected_coordinates, &expected_rejected_coordinates,
               sizeof(rejected_coordinates)) != 0) {
        return false;
    }
    return true;
}

static bool scene_test_enter_free_throw_sequence(
    TecmoGameplayScene *scene,
    TecmoGameplayTeam scoring_team,
    uint8_t attempts)
{
    TecmoGameplayFoulRequest request;
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    size_t frame;

    if (scene == NULL || attempts == 0U) return false;
    request.fouling_team = scene_other_team(scoring_team);
    request.free_throw_team = scoring_team;
    request.counter_effect = TECMO_GAMEPLAY_FOUL_COUNTER_BOTH;
    request.player_index = 0U;
    request.free_throw_attempts = attempts;
    if (!tecmo_gameplay_request_foul(&scene->state, &request)) return false;

    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    for (frame = 0U; frame < TECMO_GAMEPLAY_PRESENTATION_LEAD_IN_FRAMES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->state.phase != TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION) {
            return false;
        }
    }
    p1.released.shoot = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
        scene->state.free_throws.scoring_team != scoring_team ||
        scene->state.free_throws.attempts_remaining != attempts ||
        scene->free_throw_frame != 0U ||
        !scene_test_free_throw_lineup_bound(
            scene,
            scoring_team == TECMO_GAMEPLAY_TEAM_AWAY ? 0U : 1U,
            scoring_team == TECMO_GAMEPLAY_TEAM_AWAY ? 0U : 5U,
            scoring_team == TECMO_GAMEPLAY_TEAM_AWAY ? 5U : 0U,
            scoring_team == TECMO_GAMEPLAY_TEAM_AWAY
                ? TECMO_GAMEPLAY_FREE_THROW_ORIENTATION_0_CAMERA_X
                : TECMO_GAMEPLAY_FREE_THROW_ORIENTATION_1_CAMERA_X) ||
        !tecmo_gameplay_state_valid(&scene->state)) {
        return false;
    }
    if (scene->launch.game_music_enabled) {
        if (scene->audio_player.sfx_pending ||
            scene->audio_player.music == NULL ||
            !scene->audio_player.music->track_pending ||
            scene->audio_player.music->pending_track_id !=
                TECMO_MUSIC_TRACK_GAMEPLAY) {
            return false;
        }
        tecmo_gameplay_audio_render_samples(&scene->audio_player, NULL, 1024U);
        if (scene->audio_player.sfx_pending ||
            !scene->audio_player.music->playing ||
            scene->audio_player.music->current_track_id !=
                TECMO_MUSIC_TRACK_GAMEPLAY ||
            scene->audio_player.music->track_pending) {
            return false;
        }
    }
    return true;
}

static bool scene_test_cpu_offense_all_difficulties(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *base_launch,
    uint8_t *failed_difficulty)
{
    TecmoGameplaySceneLaunch launch;
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    uint8_t difficulty;
    size_t frame;
    if (scene == NULL || base_launch == NULL || failed_difficulty == NULL) {
        return false;
    }
    launch = *base_launch;
    launch.controller_team[0] = TECMO_GAMEPLAY_TEAM_HOME;
    launch.controller_team[1] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    launch.game_music_enabled = false;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    for (difficulty = 0U; difficulty <= 2U; ++difficulty) {
        *failed_difficulty = difficulty;
        launch.difficulty = difficulty;
        if (!tecmo_gameplay_scene_launch(scene, &launch)) return false;
        /* TGFT can reduce a tired holder to TGMO's minimum Q4 rate. The
           bounded approach still has to finish well before shot-clock expiry. */
        for (frame = 0U; frame < 600U &&
             scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE; ++frame) {
            if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
                scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
                scene->state.shot_clock == 0U) {
                return false;
            }
        }
        if (!scene_shot_is_close(scene->shot_kind) ||
            scene->shot_actor >= TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
            scene->actors[scene->shot_actor].position.x <
                (int16_t)scene->orientation_state.offensive_hoop.x ||
            scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
            scene->state.shot_clock == 0U) {
            return false;
        }
        tecmo_gameplay_scene_end(scene);
    }
    return true;
}

bool tecmo_gameplay_scene_test_state_flow(
    TecmoGameplaySceneTestContext *test)
{
    TecmoGameplaySceneLaunch launch = test->launch;
    TecmoControlFrame p1 = test->p1;
    TecmoControlFrame p2 = test->p2;
    char *message = test->message;
    size_t message_size = test->message_size;
    TecmoGameplaySceneResult result;
    uint16_t away_score_before;
    uint16_t home_score_before;
    uint8_t shot_actor;
    uint8_t failed_difficulty;
    int16_t x;
    int16_t cpu_holder_start_x;
    size_t frame;

#define TEST_SCENE (*test->scene)
    if (!tecmo_gameplay_scene_test_close_clock_collision(&TEST_SCENE, &launch)) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "close-shot countdown/dual-expiry settlement failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    launch.controller_team[1] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    if (!tecmo_gameplay_scene_launch(&TEST_SCENE, &launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "single-controller gameplay launch rejected");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    TEST_SCENE.fatigue_state.capacity[TECMO_GAMEPLAY_TEAM_AWAY][0U] = 10U;
    TEST_SCENE.fatigue_state.countdown[TECMO_GAMEPLAY_TEAM_AWAY][0U] = 1U;
    TEST_SCENE.fatigue_state.condition[TECMO_GAMEPLAY_TEAM_AWAY][0U] = 10U;
    TEST_SCENE.actors[0U].condition = 10U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.fatigue_state.capacity[TECMO_GAMEPLAY_TEAM_AWAY][0U] != 9U ||
        TEST_SCENE.fatigue_state.countdown[TECMO_GAMEPLAY_TEAM_AWAY][0U] != 9U ||
        TEST_SCENE.fatigue_state.condition[TECMO_GAMEPLAY_TEAM_AWAY][0U] != 9U ||
        TEST_SCENE.actors[0U].condition != 9U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "TGFT-1 live condition synchronization failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    TEST_SCENE.actors[0U].position.x = 149;
    TEST_SCENE.actors[0U].position.y = 148;
    TEST_SCENE.actors[0U].movement_fractional_accumulator = 15U;
    p1.held.left = true;
    if (!scene_attach_ball(&TEST_SCENE) ||
        !tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        TEST_SCENE.actors[0U].position.x != 149 ||
        TEST_SCENE.actors[0U].movement_boundary_latched ||
        TEST_SCENE.actors[0U].movement_action_state !=
            TECMO_GAMEPLAY_MOVEMENT_INPUT_LEFT) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "TGMO out-of-bounds approach setup failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
        TEST_SCENE.state.violation != TECMO_GAMEPLAY_VIOLATION_OUT_OF_BOUNDS ||
        TEST_SCENE.state.restart_possession != TECMO_GAMEPLAY_TEAM_HOME ||
        TEST_SCENE.actors[0U].position.x != 149 ||
        TEST_SCENE.actors[0U].movement_boundary_latched ||
        TEST_SCENE.ball_holder != 0U ||
        !TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.pending_sfx_id != 6U) {
        char failure[384];
        (void)snprintf(
            failure, sizeof(failure),
            "TGMO/TPNL out-of-bounds entry failed: phase=%u violation=%u restart=%u x=%d latch=%u holder=%u sfx=%u/%u control=%u team=%u action=%u direction=%u shot=%u",
            (unsigned)TEST_SCENE.state.phase,
            (unsigned)TEST_SCENE.state.violation,
            (unsigned)TEST_SCENE.state.restart_possession,
            (int)TEST_SCENE.actors[0U].position.x,
            TEST_SCENE.actors[0U].movement_boundary_latched ? 1U : 0U,
            (unsigned)TEST_SCENE.ball_holder,
            TEST_SCENE.audio_player.sfx_pending ? 1U : 0U,
            (unsigned)TEST_SCENE.audio_player.pending_sfx_id,
            (unsigned)TEST_SCENE.controlled_actor[0U],
            (unsigned)TEST_SCENE.launch.controller_team[0U],
            (unsigned)TEST_SCENE.actors[0U].movement_action_state,
            (unsigned)TEST_SCENE.actors[0U].movement_direction,
            (unsigned)TEST_SCENE.shot_kind);
        tecmo_gameplay_scene_test_message(message, message_size, failure);
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    for (frame = 0U;
         frame < TECMO_GAMEPLAY_VIOLATION_RELEASE_LEAD_IN_FRAMES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "out-of-bounds lead-in failed");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    p1.released.shoot = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        TEST_SCENE.state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        TEST_SCENE.state.shot_clock != TECMO_GAMEPLAY_SHOT_CLOCK_SECONDS ||
        TEST_SCENE.state.clock_divider != TECMO_GAMEPLAY_POSSESSION_DIVIDER_FRAMES ||
        TEST_SCENE.ball_holder != TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        TEST_SCENE.actors[0U].movement_boundary_latched) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "out-of-bounds restart settlement failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_audio_stop_all(&TEST_SCENE.audio_player);
    if (!scene_handoff_possession(
            &TEST_SCENE, TECMO_GAMEPLAY_TEAM_AWAY, 0U)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "out-of-bounds test possession reset failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    TEST_SCENE.state.shot_clock = 1U;
    TEST_SCENE.state.clock_divider = 1U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_audio_queue_dmc_clip(
            &TEST_SCENE.audio_player,
            TECMO_GAMEPLAY_DMC_BANK05_A8D6_LONG) ||
        !tecmo_gameplay_state_valid(&TEST_SCENE.state) ||
        !tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
        !TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.pending_sfx_id != 6U ||
        TEST_SCENE.audio_player.dmc.active ||
        TEST_SCENE.audio_player.music == NULL ||
        TEST_SCENE.audio_player.music->playing ||
        TEST_SCENE.audio_player.music->track_pending) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "shot-clock violation reset/cue ordering failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&TEST_SCENE.audio_player, NULL, 1024U);
    if (TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.current_sfx_id != 6U ||
        !tecmo_gameplay_audio_queue_dmc_clip(
            &TEST_SCENE.audio_player,
            TECMO_GAMEPLAY_DMC_BANK05_A8D6_LONG)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "single violation cue consumption failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    for (frame = 0U;
         frame < TECMO_GAMEPLAY_VIOLATION_RELEASE_LEAD_IN_FRAMES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
            TEST_SCENE.audio_player.sfx_pending) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "violation reset/cue repeated after entry");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    p1.released.shoot = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        TEST_SCENE.state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        TEST_SCENE.ball_holder < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        !TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.pending_sfx_id != 5U ||
        !TEST_SCENE.audio_player.music->track_pending ||
        TEST_SCENE.audio_player.music->pending_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY ||
        !tecmo_gameplay_state_valid(&TEST_SCENE.state)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "violation restart holder synchronization failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&TEST_SCENE.audio_player, NULL, 1024U);
    if (TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.current_sfx_id != 5U ||
        !TEST_SCENE.audio_player.music->playing ||
        TEST_SCENE.audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "violation live-return audio restart failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.audio_player.sfx_pending) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "violation restart cue repeated");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    /* Allow the exact fatigue path to reach TGMO's minimum Q4 rate while
       retaining a bound below the 24-second possession clock. */
    for (frame = 0U; frame < 600U &&
         TEST_SCENE.shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE; ++frame) {
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "native offense update failed");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    if (TEST_SCENE.shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        TEST_SCENE.shot_actor < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "native offense did not start a shot");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_scene_end(&TEST_SCENE);
    if (!scene_test_cpu_offense_all_difficulties(
            &TEST_SCENE, &launch, &failed_difficulty)) {
        char failure[192];
        (void)snprintf(
            failure, sizeof(failure),
            "CPU offense stalled before a close shot at difficulty %u",
            (unsigned)failed_difficulty);
        tecmo_gameplay_scene_test_message(message, message_size, failure);
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }

    launch.controller_team[0] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch.controller_team[1] = TECMO_GAMEPLAY_TEAM_HOME;
    if (!tecmo_gameplay_scene_test_combined_restart_is_inert(&TEST_SCENE, &launch, 1U) ||
        !tecmo_gameplay_scene_test_combined_restart_is_inert(&TEST_SCENE, &launch, 3U)) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "combined violation restart action suppression failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    if (!tecmo_gameplay_scene_test_jump_period_expiry(&TEST_SCENE, &launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "jump-miss period-expiry route failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    if (!tecmo_gameplay_scene_test_jump_make_period_expiry(&TEST_SCENE, &launch, true) ||
        !tecmo_gameplay_scene_test_jump_make_period_expiry(&TEST_SCENE, &launch, false)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "jump-make period-expiry route failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    if (!tecmo_gameplay_scene_launch(&TEST_SCENE, &launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "period-expiry gameplay launch rejected");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    TEST_SCENE.state.clock_minutes = 0U;
    TEST_SCENE.state.clock_seconds = 1U;
    TEST_SCENE.state.clock_divider = 2U;
    TEST_SCENE.state.shot_clock = 20U;
    TEST_SCENE.actors[TEST_SCENE.ball_holder].position.x = 0x013CU;
    TEST_SCENE.actors[TEST_SCENE.ball_holder].position.y = 180;
    TEST_SCENE.actors[TEST_SCENE.ball_holder].facing_right = true;
    scene_attach_ball(&TEST_SCENE);
    TEST_SCENE.action_serial = 1U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_state_valid(&TEST_SCENE.state) ||
        !tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        TEST_SCENE.shot_frame != 1U || TEST_SCENE.action_serial != 2U ||
        TEST_SCENE.jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "period-expiry live shot setup failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    shot_actor = TEST_SCENE.shot_actor;
    away_score_before = TEST_SCENE.state.score[TECMO_GAMEPLAY_TEAM_AWAY];
    home_score_before = TEST_SCENE.state.score[TECMO_GAMEPLAY_TEAM_HOME];
    memset(&p1, 0, sizeof(p1));
    for (frame = 0U; frame < 96U &&
         TEST_SCENE.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE; ++frame) {
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "period-expiry shot update failed");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    if (TEST_SCENE.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        TEST_SCENE.state.phase !=
            TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE ||
        !TEST_SCENE.state.period_expiry_zero_action_observed ||
        TEST_SCENE.state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        TEST_SCENE.state.score[TECMO_GAMEPLAY_TEAM_AWAY] != away_score_before ||
        TEST_SCENE.state.score[TECMO_GAMEPLAY_TEAM_HOME] != home_score_before ||
        TEST_SCENE.jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN ||
        !TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.pending_sfx_id != 11U ||
        TEST_SCENE.ball_holder != shot_actor ||
        TEST_SCENE.actors[TEST_SCENE.ball_holder].team != TECMO_GAMEPLAY_TEAM_AWAY ||
        !tecmo_gameplay_state_valid(&TEST_SCENE.state)) {
        char failure[192];
        (void)snprintf(
            failure, sizeof(failure),
            "period-expiry shot settlement diverged: shot=%u phase=%u holder=%u possession=%u valid=%u",
            (unsigned)TEST_SCENE.shot_kind, (unsigned)TEST_SCENE.state.phase,
            (unsigned)TEST_SCENE.ball_holder, (unsigned)TEST_SCENE.state.possession,
            tecmo_gameplay_state_valid(&TEST_SCENE.state) ? 1U : 0U);
        tecmo_gameplay_scene_test_message(message, message_size, failure);
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    if (!tecmo_gameplay_audio_queue_dmc_clip(
            &TEST_SCENE.audio_player,
            TECMO_GAMEPLAY_DMC_BANK05_A8D6_LONG) ||
        !tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
        TEST_SCENE.state.period != 2U ||
        TEST_SCENE.state.banner != TECMO_GAMEPLAY_BANNER_SECOND_PERIOD ||
        TEST_SCENE.state.period_expiry_zero_action_observed ||
        TEST_SCENE.state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        TEST_SCENE.state.score[TECMO_GAMEPLAY_TEAM_AWAY] != away_score_before ||
        TEST_SCENE.state.score[TECMO_GAMEPLAY_TEAM_HOME] != home_score_before ||
        TEST_SCENE.ball_holder != shot_actor ||
        TEST_SCENE.audio_player.sfx_pending || TEST_SCENE.audio_player.dmc.active ||
        TEST_SCENE.audio_player.music == NULL ||
        TEST_SCENE.audio_player.music->playing ||
        TEST_SCENE.audio_player.music->track_pending ||
        !tecmo_gameplay_state_valid(&TEST_SCENE.state)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "period-expiry audio reset transition failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    if (!tecmo_gameplay_audio_queue_dmc_clip(
            &TEST_SCENE.audio_player,
            TECMO_GAMEPLAY_DMC_BANK05_A8D6_LONG)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "period exact-once DMC probe failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    TEST_SCENE.state.phase_frame = TECMO_GAMEPLAY_PERIOD_BANNER_FRAMES - 1U;
    TEST_SCENE.ball_holder = 5U;
    scene_attach_ball(&TEST_SCENE);
    TEST_SCENE.action_serial = 3U;
    x = TEST_SCENE.actors[0].position.x;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.held.right = true;
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        TEST_SCENE.state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        TEST_SCENE.ball_holder != 0U || TEST_SCENE.controlled_actor[0] != 0U ||
        TEST_SCENE.actors[0].position.x != x || TEST_SCENE.action_serial != 3U ||
        TEST_SCENE.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        !TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.pending_sfx_id != 5U ||
        !TEST_SCENE.audio_player.dmc.active ||
        !TEST_SCENE.audio_player.music->track_pending ||
        TEST_SCENE.audio_player.music->pending_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "period restart action suppression failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&TEST_SCENE.audio_player, NULL, 1024U);
    if (!TEST_SCENE.audio_player.music->playing ||
        TEST_SCENE.audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "period live-return music restart failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_scene_end(&TEST_SCENE);

    launch.controller_team[0] = TECMO_GAMEPLAY_TEAM_HOME;
    launch.controller_team[1] = TECMO_GAMEPLAY_TEAM_AWAY;
    if (!tecmo_gameplay_scene_launch(&TEST_SCENE, &launch) ||
        TEST_SCENE.controlled_actor[0] < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        TEST_SCENE.controlled_actor[1] >= TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "swapped controller ownership mapping failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    x = TEST_SCENE.actors[TEST_SCENE.controlled_actor[0]].position.x;
    memset(&p1, 0, sizeof(p1));
    p1.held.right = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.actors[TEST_SCENE.controlled_actor[0]].position.x != x ||
        !tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.actors[TEST_SCENE.controlled_actor[0]].position.x != x + 1) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "swapped controller TGMO movement failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_scene_end(&TEST_SCENE);

    launch.controller_team[0] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch.controller_team[1] = TECMO_GAMEPLAY_TEAM_HOME;
    if (!tecmo_gameplay_scene_launch(&TEST_SCENE, &launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "combined-button gameplay launch rejected");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    /* Keep the A-selected receiver inside the supported TGJS/TGSR slot-0 miss
       context so this test remains about A-before-B resolution. */
    TEST_SCENE.actors[1].position.x = 0x013CU;
    TEST_SCENE.actors[1].position.y = 180;
    TEST_SCENE.actors[1].facing_right = true;
    TEST_SCENE.action_serial = 1U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.held.shoot = true;
    p1.pressed.shoot = true;
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        TEST_SCENE.shot_actor != 1U || TEST_SCENE.action_serial != 2U ||
        TEST_SCENE.jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "combined NES A+B resolution failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_scene_end(&TEST_SCENE);

    /* TGBD follows the actual holder's TGMO animation phase. Unrelated pad
       activity cannot trigger it; a stationary holder still dribbles. */
    if (!tecmo_gameplay_scene_launch(&TEST_SCENE, &launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "defender dribble-policy launch rejected");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p2.held.right = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        !tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.audio_player.dmc.active) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "defender movement queued holder DMC");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_scene_end(&TEST_SCENE);

    launch.controller_team[1] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    if (!tecmo_gameplay_scene_launch(&TEST_SCENE, &launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "NO_TEAM dribble-policy launch rejected");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    memset(&p2, 0, sizeof(p2));
    p2.held.right = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.audio_player.dmc.active) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "NO_TEAM pad movement queued holder DMC");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_scene_end(&TEST_SCENE);

    if (!tecmo_gameplay_scene_launch(&TEST_SCENE, &launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "human holder dribble-policy launch rejected");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    for (frame = 0U; frame <= 14U; ++frame) {
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
            (frame < 14U &&
             TEST_SCENE.audio_player.dmc.active)) {
            tecmo_gameplay_scene_test_message(
                message, message_size,
                "human holder TGBD phase queued an early DMC");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
        if ((frame == 0U &&
             TEST_SCENE.ball_position.y_q8 != 176 * 256) ||
            (frame == 3U &&
             TEST_SCENE.ball_position.y_q8 != 182 * 256) ||
            (frame == 7U &&
             TEST_SCENE.ball_position.y_q8 != 191 * 256) ||
            (frame == 11U &&
             TEST_SCENE.ball_position.y_q8 != 197 * 256)) {
            tecmo_gameplay_scene_test_message(
                message, message_size,
                "human holder TGBD visible bounce vector failed");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    if (!TEST_SCENE.audio_player.dmc.active) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "stationary human holder missed native TGBD phase DMC");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_scene_end(&TEST_SCENE);

    launch.controller_team[0] = TECMO_GAMEPLAY_TEAM_HOME;
    if (!tecmo_gameplay_scene_launch(&TEST_SCENE, &launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "CPU holder dribble-policy launch rejected");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    cpu_holder_start_x = TEST_SCENE.actors[TEST_SCENE.ball_holder].position.x;
    for (frame = 0U; frame <= 17U; ++frame) {
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
            (frame < 17U &&
             TEST_SCENE.audio_player.dmc.active)) {
            tecmo_gameplay_scene_test_message(
                message, message_size,
                "CPU holder TGBD phase queued an early DMC");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
        if (frame == 0U &&
            (TEST_SCENE.actors[0].position.x != cpu_holder_start_x ||
             TEST_SCENE.actors[0].movement_action_state !=
                 TECMO_GAMEPLAY_MOVEMENT_INPUT_LEFT ||
             TEST_SCENE.cpu_actors[0].decision_serial != 1U ||
             TEST_SCENE.cpu_actors[0].snapshot_fingerprint != 0xBD36E345U ||
             TEST_SCENE.cpu_actors[0].target_kind !=
                 TECMO_GAMEPLAY_CPU_STEERING_HARNESS_HOOP_APPROACH ||
             TEST_SCENE.cpu_actors[0].target_position.x != 208 ||
             TEST_SCENE.cpu_actors[0].target_position.y != 148 ||
             TEST_SCENE.cpu_actors[0].direction != 1U ||
             TEST_SCENE.cpu_actors[0].held_direction_bits !=
                 TECMO_GAMEPLAY_MOVEMENT_INPUT_LEFT ||
             !TEST_SCENE.cpu_actors[0].writes_direction ||
             TEST_SCENE.cpu_actors[1].decision_serial != 1U ||
             TEST_SCENE.cpu_actors[1].target_kind !=
                 TECMO_GAMEPLAY_CPU_STEERING_HARNESS_LINKED_ACTOR ||
             TEST_SCENE.cpu_actors[1].linked_actor != 6U ||
             TEST_SCENE.cpu_actors[1].target_position.x != 395 ||
             TEST_SCENE.cpu_actors[1].target_position.y != 190 ||
             TEST_SCENE.cpu_actors[5].decision_serial != 0U)) {
            tecmo_gameplay_scene_test_message(
                message, message_size,
                "live TGAI snapshot/target/TGMO-latency contract failed");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    if (!TEST_SCENE.audio_player.dmc.active ||
        TEST_SCENE.actors[0].position.x >= cpu_holder_start_x ||
        TEST_SCENE.cpu_actors[0].decision_serial !=
            18U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "CPU holder missed native TGBD phase DMC");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_scene_end(&TEST_SCENE);
    launch.controller_team[0] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch.controller_team[1] = TECMO_GAMEPLAY_TEAM_HOME;

    launch.game_music_enabled = false;
    if (!tecmo_gameplay_scene_launch(&TEST_SCENE, &launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "music-off restart launch rejected");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    TEST_SCENE.state.shot_clock = 1U;
    TEST_SCENE.state.clock_divider = 1U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
        !TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.pending_sfx_id != 6U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "music-off violation entry failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&TEST_SCENE.audio_player, NULL, 1024U);
    if (TEST_SCENE.audio_player.sfx_pending) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "music-off violation cue was not consumed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    for (frame = 0U;
         frame < TECMO_GAMEPLAY_VIOLATION_RELEASE_LEAD_IN_FRAMES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "music-off violation lead-in failed");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    p1.released.shoot = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        TEST_SCENE.audio_player.sfx_pending) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "music-off restart queued neutral cue");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_scene_end(&TEST_SCENE);
    launch.game_music_enabled = true;

    if (!tecmo_gameplay_scene_launch(&TEST_SCENE, &launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "native steal-policy launch rejected");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    TEST_SCENE.actors[TEST_SCENE.controlled_actor[1]].position.x =
        TEST_SCENE.actors[TEST_SCENE.ball_holder].position.x + 1;
    TEST_SCENE.actors[TEST_SCENE.controlled_actor[1]].position.y =
        TEST_SCENE.actors[TEST_SCENE.ball_holder].position.y;
    TEST_SCENE.action_serial = 1U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p2.held.cancel = true;
    p2.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.action_serial != 2U ||
        TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        TEST_SCENE.state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        TEST_SCENE.ball_holder != TEST_SCENE.controlled_actor[1]) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "native action-serial steal policy diverged");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_scene_end(&TEST_SCENE);

    if (!tecmo_gameplay_scene_launch(&TEST_SCENE, &launch) ||
        !scene_test_free_throw_lineup_unbound(&TEST_SCENE)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "foul/free-throw gameplay launch rejected");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    TEST_SCENE.actors[TEST_SCENE.controlled_actor[1]].position.x =
        TEST_SCENE.actors[TEST_SCENE.ball_holder].position.x + 1;
    TEST_SCENE.actors[TEST_SCENE.controlled_actor[1]].position.y =
        TEST_SCENE.actors[TEST_SCENE.ball_holder].position.y;
    TEST_SCENE.action_serial = 3U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p2.held.cancel = true;
    p2.pressed.cancel = true;
    if (!tecmo_gameplay_audio_queue_dmc_clip(
            &TEST_SCENE.audio_player,
            TECMO_GAMEPLAY_DMC_BANK05_A8D6_LONG) ||
        !tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION ||
        TEST_SCENE.state.team_fouls[TECMO_GAMEPLAY_TEAM_HOME] != 1U ||
        TEST_SCENE.state.individual_fouls[TECMO_GAMEPLAY_TEAM_HOME][0] != 1U ||
        TEST_SCENE.action_serial != 4U ||
        TEST_SCENE.audio_player.sfx_pending || TEST_SCENE.audio_player.dmc.active ||
        TEST_SCENE.audio_player.music == NULL ||
        TEST_SCENE.audio_player.music->playing ||
        TEST_SCENE.audio_player.music->track_pending) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "foul entry audio reset/policy diverged");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_audio_queue_dmc_clip(
            &TEST_SCENE.audio_player,
            TECMO_GAMEPLAY_DMC_BANK05_A8D6_LONG)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "foul exact-once DMC probe failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    for (frame = 0U; frame < TECMO_GAMEPLAY_PRESENTATION_LEAD_IN_FRAMES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
            !TEST_SCENE.audio_player.dmc.active) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "foul audio reset repeated after entry");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    p1.released.shoot = true;
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
        TEST_SCENE.state.free_throws.attempts_remaining != 2U ||
        TEST_SCENE.free_throw_frame != 0U || TEST_SCENE.audio_player.sfx_pending ||
        !scene_test_free_throw_lineup_bound(
            &TEST_SCENE, 0U, 0U, 5U,
            TECMO_GAMEPLAY_FREE_THROW_ORIENTATION_0_CAMERA_X) ||
        TEST_SCENE.audio_player.music == NULL ||
        !TEST_SCENE.audio_player.music->track_pending ||
        TEST_SCENE.audio_player.music->pending_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "foul dismissal/free-throw handoff failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&TEST_SCENE.audio_player, NULL, 1024U);
    if (TEST_SCENE.audio_player.sfx_pending ||
        !TEST_SCENE.audio_player.music->playing ||
        TEST_SCENE.audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY ||
        TEST_SCENE.audio_player.music->track_pending) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "free-throw setup music was not consumed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    /* Human attempts are owned by the scoring team's pad and have no timer.
       Exercise every tempting false-positive before accepting held/current B. */
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.held.shoot = true;
    p1.held.up = true;
    p1.held.right = true;
    p1.held.tab = true;
    p1.held.confirm = true;
    p1.pressed.shoot = true;
    p1.pressed.left = true;
    p1.pressed.tab = true;
    p1.pressed.confirm = true;
    p1.pressed.cancel = true;
    p1.released.shoot = true;
    p1.released.down = true;
    p1.released.tab = true;
    p1.released.confirm = true;
    p1.released.cancel = true;
    p2.held.cancel = true;
    p2.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
        TEST_SCENE.state.free_throws.attempts_remaining != 2U ||
        TEST_SCENE.free_throw_frame != 0U || TEST_SCENE.action_serial != 4U ||
        TEST_SCENE.audio_player.sfx_pending ||
        !tecmo_gameplay_state_valid(&TEST_SCENE.state)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "non-owner/free-throw non-B input launched");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    for (frame = 0U;
         frame < TECMO_GAMEPLAY_FREE_THROW_CPU_OBSERVED_LAUNCH_UPDATES * 2U;
         ++frame) {
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
            TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
            TEST_SCENE.state.free_throws.attempts_remaining != 2U ||
            TEST_SCENE.free_throw_frame != 0U || TEST_SCENE.action_serial != 4U ||
            TEST_SCENE.audio_player.sfx_pending ||
            !tecmo_gameplay_state_valid(&TEST_SCENE.state)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "human free throw gained a timer fallback");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    p1.held.cancel = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
        TEST_SCENE.state.free_throws.attempts_remaining != 1U ||
        TEST_SCENE.free_throw_frame != 0U || TEST_SCENE.action_serial != 5U ||
        TEST_SCENE.state.score[TECMO_GAMEPLAY_TEAM_AWAY] != 1U ||
        !scene_test_free_throw_lineup_bound(
            &TEST_SCENE, 0U, 0U, 5U,
            TECMO_GAMEPLAY_FREE_THROW_ORIENTATION_0_CAMERA_X) ||
        !TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.pending_sfx_id != 12U ||
        !tecmo_gameplay_state_valid(&TEST_SCENE.state)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "owned held-B free throw did not launch");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&TEST_SCENE.audio_player, NULL, 1024U);
    if (TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.current_sfx_id != 12U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "made away free-throw mailbox was not side-result 12");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
        TEST_SCENE.state.free_throws.attempts_remaining != 1U ||
        TEST_SCENE.free_throw_frame != 0U || TEST_SCENE.action_serial != 5U ||
        TEST_SCENE.state.score[TECMO_GAMEPLAY_TEAM_AWAY] != 1U ||
        !tecmo_gameplay_state_valid(&TEST_SCENE.state)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pressed-only free throw input launched");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    p1.held.cancel = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        TEST_SCENE.state.free_throws.attempts_remaining != 0U ||
        TEST_SCENE.state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        TEST_SCENE.free_throw_frame != 0U || TEST_SCENE.action_serial != 6U ||
        TEST_SCENE.state.score[TECMO_GAMEPLAY_TEAM_AWAY] != 2U ||
        TEST_SCENE.ball_holder < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        TEST_SCENE.controlled_actor[1] != TEST_SCENE.ball_holder ||
        !TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.pending_sfx_id != 12U ||
        TEST_SCENE.audio_player.music == NULL ||
        !TEST_SCENE.audio_player.music->playing ||
        TEST_SCENE.audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY ||
        TEST_SCENE.audio_player.music->track_pending ||
        !scene_test_free_throw_lineup_unbound(&TEST_SCENE) ||
        !tecmo_gameplay_state_valid(&TEST_SCENE.state)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "human free-throw settlement failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&TEST_SCENE.audio_player, NULL, 1024U);
    memset(&p1, 0, sizeof(p1));
    if (TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.current_sfx_id != 12U ||
        !TEST_SCENE.audio_player.music->playing ||
        TEST_SCENE.audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY ||
        TEST_SCENE.audio_player.music->track_pending ||
        !tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.current_sfx_id != 12U ||
        TEST_SCENE.audio_player.music->track_pending ||
        TEST_SCENE.audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "final human free-throw audio repeated or missing");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    TEST_SCENE.free_throw_frame = 7U;
    tecmo_gameplay_scene_end(&TEST_SCENE);
    if (TEST_SCENE.free_throw_frame != 0U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "free-throw timer survived scene end");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }

    /* Home ownership uses its assigned pad, independently of controller index. */
    if (!tecmo_gameplay_scene_launch(&TEST_SCENE, &launch) ||
        TEST_SCENE.free_throw_frame != 0U ||
        !scene_test_enter_free_throw_sequence(
            &TEST_SCENE, TECMO_GAMEPLAY_TEAM_HOME, 1U)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "home free-throw ownership setup failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.held.cancel = true;
    p2.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
        TEST_SCENE.state.free_throws.attempts_remaining != 1U ||
        TEST_SCENE.action_serial != 0U || TEST_SCENE.free_throw_frame != 0U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "home free throw accepted the wrong pad/edge");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p2.held.cancel = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        TEST_SCENE.state.free_throws.attempts_remaining != 0U ||
        TEST_SCENE.state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        TEST_SCENE.state.score[TECMO_GAMEPLAY_TEAM_HOME] != 1U ||
        TEST_SCENE.ball_holder >= TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        TEST_SCENE.controlled_actor[0] != TEST_SCENE.ball_holder ||
        TEST_SCENE.action_serial != 1U || TEST_SCENE.free_throw_frame != 0U ||
        !scene_test_free_throw_lineup_unbound(&TEST_SCENE) ||
        !TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.pending_sfx_id != 13U ||
        TEST_SCENE.audio_player.music == NULL ||
        !TEST_SCENE.audio_player.music->playing ||
        TEST_SCENE.audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY ||
        TEST_SCENE.audio_player.music->track_pending ||
        !tecmo_gameplay_state_valid(&TEST_SCENE.state)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "home owned held-B free throw failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&TEST_SCENE.audio_player, NULL, 1024U);
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.current_sfx_id != 13U ||
        TEST_SCENE.audio_player.music->track_pending ||
        !tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.current_sfx_id != 13U ||
        TEST_SCENE.audio_player.music->track_pending ||
        TEST_SCENE.audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "final home free-throw audio repeated or missing");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_scene_end(&TEST_SCENE);

    /* With no controller assigned to the scoring side, use the observed
       125-update launch schedule and reset it for the following attempt. */
    launch.controller_team[1] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    if (!tecmo_gameplay_scene_launch(&TEST_SCENE, &launch) ||
        !scene_test_enter_free_throw_sequence(
            &TEST_SCENE, TECMO_GAMEPLAY_TEAM_HOME, 2U)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "CPU free-throw timer setup failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    for (frame = 0U;
         frame + 1U <
             TECMO_GAMEPLAY_FREE_THROW_CPU_OBSERVED_LAUNCH_UPDATES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
            TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
            TEST_SCENE.state.free_throws.attempts_remaining != 2U ||
            TEST_SCENE.free_throw_frame != frame + 1U ||
            TEST_SCENE.action_serial != 0U || TEST_SCENE.audio_player.sfx_pending ||
            !tecmo_gameplay_state_valid(&TEST_SCENE.state)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "CPU free throw launched before observed schedule");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
        TEST_SCENE.state.free_throws.attempts_remaining != 1U ||
        TEST_SCENE.free_throw_frame != 0U || TEST_SCENE.action_serial != 1U ||
        TEST_SCENE.state.score[TECMO_GAMEPLAY_TEAM_HOME] != 0U ||
        !scene_test_free_throw_lineup_bound(
            &TEST_SCENE, 1U, 5U, 0U,
            TECMO_GAMEPLAY_FREE_THROW_ORIENTATION_1_CAMERA_X) ||
        !TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.pending_sfx_id != 13U ||
        !tecmo_gameplay_state_valid(&TEST_SCENE.state)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "CPU free throw missed observed launch update");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&TEST_SCENE.audio_player, NULL, 1024U);
    if (TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.current_sfx_id != 13U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "missed home free-throw mailbox was not side-result 13");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.state.free_throws.attempts_remaining != 1U ||
        TEST_SCENE.free_throw_frame != 1U || TEST_SCENE.action_serial != 1U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "second CPU free-throw timer did not reset");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    for (frame = 1U;
         frame + 1U <
             TECMO_GAMEPLAY_FREE_THROW_CPU_OBSERVED_LAUNCH_UPDATES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
            TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
            TEST_SCENE.state.free_throws.attempts_remaining != 1U ||
            TEST_SCENE.free_throw_frame != frame + 1U ||
            TEST_SCENE.action_serial != 1U || TEST_SCENE.audio_player.sfx_pending) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "second CPU free throw launched early");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        TEST_SCENE.state.free_throws.attempts_remaining != 0U ||
        TEST_SCENE.state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        TEST_SCENE.state.score[TECMO_GAMEPLAY_TEAM_HOME] != 0U ||
        TEST_SCENE.ball_holder >= TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        TEST_SCENE.controlled_actor[0] != TEST_SCENE.ball_holder ||
        TEST_SCENE.free_throw_frame != 0U || TEST_SCENE.action_serial != 2U ||
        !scene_test_free_throw_lineup_unbound(&TEST_SCENE) ||
        !TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.pending_sfx_id != 13U ||
        TEST_SCENE.audio_player.music == NULL ||
        !TEST_SCENE.audio_player.music->playing ||
        TEST_SCENE.audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY ||
        TEST_SCENE.audio_player.music->track_pending ||
        !tecmo_gameplay_state_valid(&TEST_SCENE.state)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "CPU free-throw settlement failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&TEST_SCENE.audio_player, NULL, 1024U);
    if (TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.current_sfx_id != 13U ||
        !TEST_SCENE.audio_player.music->playing ||
        TEST_SCENE.audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY ||
        TEST_SCENE.audio_player.music->track_pending ||
        !tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.current_sfx_id != 13U ||
        TEST_SCENE.audio_player.music->track_pending ||
        TEST_SCENE.audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "final CPU free-throw audio repeated or missing");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_scene_end(&TEST_SCENE);
    launch.controller_team[1] = TECMO_GAMEPLAY_TEAM_HOME;

    if (!tecmo_gameplay_scene_launch(&TEST_SCENE, &launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "halftime gameplay launch rejected");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    TEST_SCENE.state.period = 2U;
    TEST_SCENE.state.clock_minutes = 0U;
    TEST_SCENE.state.clock_seconds = 1U;
    TEST_SCENE.state.clock_divider = 1U;
    TEST_SCENE.state.shot_clock = 12U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_state_valid(&TEST_SCENE.state) ||
        !tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_FIXED_WAIT) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "halftime expiry entry failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    for (frame = 0U; frame < 40U &&
         TEST_SCENE.state.phase ==
             TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_FIXED_WAIT; ++frame) {
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "halftime expiry wait failed");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    if (TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_HALFTIME_BANNER) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "halftime banner transition failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    for (frame = 0U; frame < TECMO_GAMEPLAY_HALFTIME_BANNER_FRAMES &&
         TEST_SCENE.state.phase == TECMO_GAMEPLAY_PHASE_HALFTIME_BANNER; ++frame) {
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "halftime banner update failed");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    if (TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_HALFTIME_SCORE_SCREEN ||
        !TEST_SCENE.audio_player.music->track_pending ||
        TEST_SCENE.audio_player.music->pending_track_id !=
            TECMO_MUSIC_TRACK_PRESENTATION) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "halftime score/music transition failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    p1.released.shoot = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
        TEST_SCENE.state.banner != TECMO_GAMEPLAY_BANNER_THIRD_PERIOD) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "halftime dismissal failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_scene_end(&TEST_SCENE);

    if (!tecmo_gameplay_scene_launch(&TEST_SCENE, &launch) ||
        !tecmo_gameplay_set_score(&TEST_SCENE.state,
                                  TECMO_GAMEPLAY_TEAM_AWAY, 4U) ||
        !tecmo_gameplay_set_score(&TEST_SCENE.state,
                                  TECMO_GAMEPLAY_TEAM_HOME, 2U)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "final gameplay setup failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    TEST_SCENE.state.period = 4U;
    TEST_SCENE.state.clock_minutes = 0U;
    TEST_SCENE.state.clock_seconds = 1U;
    TEST_SCENE.state.clock_divider = 1U;
    TEST_SCENE.state.shot_clock = 12U;
    memset(&p1, 0, sizeof(p1));
    if (!tecmo_gameplay_state_valid(&TEST_SCENE.state) ||
        !tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "final expiry entry failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    for (frame = 0U; frame < 40U &&
         TEST_SCENE.state.phase ==
             TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_FIXED_WAIT; ++frame) {
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "final expiry wait failed");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    if (TEST_SCENE.state.phase != TECMO_GAMEPLAY_PHASE_FINAL_SCORE_SCREEN) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "final score transition failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    p1.released.shoot = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        !tecmo_gameplay_scene_result(&TEST_SCENE, &result) ||
        result.source != launch.source ||
        result.game_index != launch.game_index ||
        result.away_team != launch.away_team ||
        result.home_team != launch.home_team ||
        result.away_score != 4U || result.home_score != 2U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "final result handoff failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_scene_end(&TEST_SCENE);
    tecmo_gameplay_scene_destroy(&TEST_SCENE);
#undef TEST_SCENE
    test->launch = launch;
    test->p1 = p1;
    test->p2 = p2;
    return true;
}
