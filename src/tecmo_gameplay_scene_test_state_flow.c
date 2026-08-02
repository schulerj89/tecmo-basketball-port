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

static bool scene_test_violations_and_cpu_offense(
    TecmoGameplayScene *scene,
    TecmoGameplaySceneLaunch *launch_input,
    TecmoControlFrame *p1_input,
    TecmoControlFrame *p2_input,
    char *message,
    size_t message_size)
{
    TecmoGameplaySceneLaunch launch = *launch_input;
    TecmoControlFrame p1 = *p1_input;
    TecmoControlFrame p2 = *p2_input;
    uint8_t failed_difficulty;
    size_t frame;
    if (!tecmo_gameplay_scene_test_close_clock_collision(scene, &launch)) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "close-shot countdown/dual-expiry settlement failed");
        return false;
    }
    launch.controller_team[1] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    if (!tecmo_gameplay_scene_launch(scene, &launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "single-controller gameplay launch rejected");
        return false;
    }
    scene->fatigue_state.capacity[TECMO_GAMEPLAY_TEAM_AWAY][0U] = 10U;
    scene->fatigue_state.countdown[TECMO_GAMEPLAY_TEAM_AWAY][0U] = 1U;
    scene->fatigue_state.condition[TECMO_GAMEPLAY_TEAM_AWAY][0U] = 10U;
    scene->actors[0U].condition = 10U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->fatigue_state.capacity[TECMO_GAMEPLAY_TEAM_AWAY][0U] != 9U ||
        scene->fatigue_state.countdown[TECMO_GAMEPLAY_TEAM_AWAY][0U] != 9U ||
        scene->fatigue_state.condition[TECMO_GAMEPLAY_TEAM_AWAY][0U] != 9U ||
        scene->actors[0U].condition != 9U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "TGFT-1 live condition synchronization failed");
        return false;
    }
    scene->actors[0U].position.x = 149;
    scene->actors[0U].position.y = 148;
    scene->actors[0U].movement_fractional_accumulator = 15U;
    p1.held.left = true;
    if (!scene_attach_ball(scene) ||
        !tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->actors[0U].position.x != 149 ||
        scene->actors[0U].movement_boundary_latched ||
        scene->actors[0U].movement_action_state !=
            TECMO_GAMEPLAY_MOVEMENT_INPUT_LEFT) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "TGMO out-of-bounds approach setup failed");
        return false;
    }
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
        scene->state.violation != TECMO_GAMEPLAY_VIOLATION_OUT_OF_BOUNDS ||
        scene->state.restart_possession != TECMO_GAMEPLAY_TEAM_HOME ||
        scene->actors[0U].position.x != 149 ||
        scene->actors[0U].movement_boundary_latched ||
        scene->ball_holder != 0U ||
        !scene->audio_player.sfx_pending ||
        scene->audio_player.pending_sfx_id != 6U) {
        char failure[384];
        (void)snprintf(
            failure, sizeof(failure),
            "TGMO/TPNL out-of-bounds entry failed: phase=%u violation=%u restart=%u x=%d latch=%u holder=%u sfx=%u/%u control=%u team=%u action=%u direction=%u shot=%u",
            (unsigned)scene->state.phase,
            (unsigned)scene->state.violation,
            (unsigned)scene->state.restart_possession,
            (int)scene->actors[0U].position.x,
            scene->actors[0U].movement_boundary_latched ? 1U : 0U,
            (unsigned)scene->ball_holder,
            scene->audio_player.sfx_pending ? 1U : 0U,
            (unsigned)scene->audio_player.pending_sfx_id,
            (unsigned)scene->controlled_actor[0U],
            (unsigned)scene->launch.controller_team[0U],
            (unsigned)scene->actors[0U].movement_action_state,
            (unsigned)scene->actors[0U].movement_direction,
            (unsigned)scene->shot_kind);
        tecmo_gameplay_scene_test_message(message, message_size, failure);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    for (frame = 0U;
         frame < TECMO_GAMEPLAY_VIOLATION_RELEASE_LEAD_IN_FRAMES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "out-of-bounds lead-in failed");
            return false;
        }
    }
    p1.released.shoot = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        scene->state.shot_clock != TECMO_GAMEPLAY_SHOT_CLOCK_SECONDS ||
        scene->state.clock_divider != TECMO_GAMEPLAY_POSSESSION_DIVIDER_FRAMES ||
        scene->ball_holder != TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        scene->actors[0U].movement_boundary_latched) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "out-of-bounds restart settlement failed");
        return false;
    }
    tecmo_gameplay_audio_stop_all(&scene->audio_player);
    if (!scene_handoff_possession(
            scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "out-of-bounds test possession reset failed");
        return false;
    }
    scene->state.shot_clock = 1U;
    scene->state.clock_divider = 1U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_audio_queue_dmc_clip(
            &scene->audio_player,
            TECMO_GAMEPLAY_DMC_BANK05_A8D6_LONG) ||
        !tecmo_gameplay_state_valid(&scene->state) ||
        !tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
        !scene->audio_player.sfx_pending ||
        scene->audio_player.pending_sfx_id != 6U ||
        scene->audio_player.dmc.active ||
        scene->audio_player.music == NULL ||
        scene->audio_player.music->playing ||
        scene->audio_player.music->track_pending) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "shot-clock violation reset/cue ordering failed");
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene->audio_player, NULL, 1024U);
    if (scene->audio_player.sfx_pending ||
        scene->audio_player.current_sfx_id != 6U ||
        !tecmo_gameplay_audio_queue_dmc_clip(
            &scene->audio_player,
            TECMO_GAMEPLAY_DMC_BANK05_A8D6_LONG)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "single violation cue consumption failed");
        return false;
    }
    for (frame = 0U;
         frame < TECMO_GAMEPLAY_VIOLATION_RELEASE_LEAD_IN_FRAMES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->audio_player.sfx_pending) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "violation reset/cue repeated after entry");
            return false;
        }
    }
    p1.released.shoot = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        scene->ball_holder < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        !scene->audio_player.sfx_pending ||
        scene->audio_player.pending_sfx_id != 5U ||
        !scene->audio_player.music->track_pending ||
        scene->audio_player.music->pending_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY ||
        !tecmo_gameplay_state_valid(&scene->state)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "violation restart holder synchronization failed");
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene->audio_player, NULL, 1024U);
    if (scene->audio_player.sfx_pending ||
        scene->audio_player.current_sfx_id != 5U ||
        !scene->audio_player.music->playing ||
        scene->audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "violation live-return audio restart failed");
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->audio_player.sfx_pending) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "violation restart cue repeated");
        return false;
    }
    /* Allow the exact fatigue path to reach TGMO's minimum Q4 rate while
       retaining a bound below the 24-second possession clock. */
    for (frame = 0U; frame < 600U &&
         scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "native offense update failed");
            return false;
        }
    }
    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->shot_actor < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "native offense did not start a shot");
        return false;
    }
    tecmo_gameplay_scene_end(scene);
    if (!scene_test_cpu_offense_all_difficulties(
            scene, &launch, &failed_difficulty)) {
        char failure[192];
        (void)snprintf(
            failure, sizeof(failure),
            "CPU offense stalled before a close shot at difficulty %u",
            (unsigned)failed_difficulty);
        tecmo_gameplay_scene_test_message(message, message_size, failure);
        return false;
    }
    *launch_input = launch;
    *p1_input = p1;
    *p2_input = p2;
    return true;
}

static bool scene_test_period_expiry_and_restart(
    TecmoGameplayScene *scene,
    TecmoGameplaySceneLaunch *launch_input,
    TecmoControlFrame *p1_input,
    TecmoControlFrame *p2_input,
    char *message,
    size_t message_size)
{
    TecmoGameplaySceneLaunch launch = *launch_input;
    TecmoControlFrame p1 = *p1_input;
    TecmoControlFrame p2 = *p2_input;
    uint16_t away_score_before;
    uint16_t home_score_before;
    uint8_t shot_actor;
    int16_t x;
    size_t frame;
    launch.controller_team[0] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch.controller_team[1] = TECMO_GAMEPLAY_TEAM_HOME;
    if (!tecmo_gameplay_scene_test_combined_restart_is_inert(scene, &launch, 1U) ||
        !tecmo_gameplay_scene_test_combined_restart_is_inert(scene, &launch, 3U)) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "combined violation restart action suppression failed");
        return false;
    }
    if (!tecmo_gameplay_scene_test_jump_period_expiry(scene, &launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "jump-miss period-expiry route failed");
        return false;
    }
    if (!tecmo_gameplay_scene_test_jump_make_period_expiry(scene, &launch, true) ||
        !tecmo_gameplay_scene_test_jump_make_period_expiry(scene, &launch, false)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "jump-make period-expiry route failed");
        return false;
    }
    if (!tecmo_gameplay_scene_launch(scene, &launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "period-expiry gameplay launch rejected");
        return false;
    }
    scene->state.clock_minutes = 0U;
    scene->state.clock_seconds = 1U;
    scene->state.clock_divider = 2U;
    scene->state.shot_clock = 20U;
    scene->actors[scene->ball_holder].position.x = 0x013CU;
    scene->actors[scene->ball_holder].position.y = 180;
    scene->actors[scene->ball_holder].facing_right = true;
    scene_attach_ball(scene);
    scene->action_serial = 1U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_state_valid(&scene->state) ||
        !tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        scene->shot_frame != 1U || scene->action_serial != 2U ||
        scene->jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "period-expiry live shot setup failed");
        return false;
    }
    shot_actor = scene->shot_actor;
    away_score_before = scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY];
    home_score_before = scene->state.score[TECMO_GAMEPLAY_TEAM_HOME];
    memset(&p1, 0, sizeof(p1));
    for (frame = 0U; frame < 96U &&
         scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "period-expiry shot update failed");
            return false;
        }
    }
    if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->state.phase !=
            TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE ||
        !scene->state.period_expiry_zero_action_observed ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY] != away_score_before ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_HOME] != home_score_before ||
        scene->jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN ||
        !scene->audio_player.sfx_pending ||
        scene->audio_player.pending_sfx_id != 11U ||
        scene->ball_holder != shot_actor ||
        scene->actors[scene->ball_holder].team != TECMO_GAMEPLAY_TEAM_AWAY ||
        !tecmo_gameplay_state_valid(&scene->state)) {
        char failure[192];
        (void)snprintf(
            failure, sizeof(failure),
            "period-expiry shot settlement diverged: shot=%u phase=%u holder=%u possession=%u valid=%u",
            (unsigned)scene->shot_kind, (unsigned)scene->state.phase,
            (unsigned)scene->ball_holder, (unsigned)scene->state.possession,
            tecmo_gameplay_state_valid(&scene->state) ? 1U : 0U);
        tecmo_gameplay_scene_test_message(message, message_size, failure);
        return false;
    }
    if (!tecmo_gameplay_audio_queue_dmc_clip(
            &scene->audio_player,
            TECMO_GAMEPLAY_DMC_BANK05_A8D6_LONG) ||
        !tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
        scene->state.period != 2U ||
        scene->state.banner != TECMO_GAMEPLAY_BANNER_SECOND_PERIOD ||
        scene->state.period_expiry_zero_action_observed ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY] != away_score_before ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_HOME] != home_score_before ||
        scene->ball_holder != shot_actor ||
        scene->audio_player.sfx_pending || scene->audio_player.dmc.active ||
        scene->audio_player.music == NULL ||
        scene->audio_player.music->playing ||
        scene->audio_player.music->track_pending ||
        !tecmo_gameplay_state_valid(&scene->state)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "period-expiry audio reset transition failed");
        return false;
    }
    if (!tecmo_gameplay_audio_queue_dmc_clip(
            &scene->audio_player,
            TECMO_GAMEPLAY_DMC_BANK05_A8D6_LONG)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "period exact-once DMC probe failed");
        return false;
    }
    scene->state.phase_frame = TECMO_GAMEPLAY_PERIOD_BANNER_FRAMES - 1U;
    scene->ball_holder = 5U;
    scene_attach_ball(scene);
    scene->action_serial = 3U;
    x = scene->actors[0].position.x;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.held.right = true;
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene->ball_holder != 0U || scene->controlled_actor[0] != 0U ||
        scene->actors[0].position.x != x || scene->action_serial != 3U ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        !scene->audio_player.sfx_pending ||
        scene->audio_player.pending_sfx_id != 5U ||
        !scene->audio_player.dmc.active ||
        !scene->audio_player.music->track_pending ||
        scene->audio_player.music->pending_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "period restart action suppression failed");
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene->audio_player, NULL, 1024U);
    if (!scene->audio_player.music->playing ||
        scene->audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "period live-return music restart failed");
        return false;
    }
    tecmo_gameplay_scene_end(scene);
    *launch_input = launch;
    *p1_input = p1;
    *p2_input = p2;
    return true;
}

static bool scene_test_controller_policy(
    TecmoGameplayScene *scene,
    TecmoGameplaySceneLaunch *launch_input,
    TecmoControlFrame *p1_input,
    TecmoControlFrame *p2_input,
    char *message,
    size_t message_size)
{
    TecmoGameplaySceneLaunch launch = *launch_input;
    TecmoControlFrame p1 = *p1_input;
    TecmoControlFrame p2 = *p2_input;
    int16_t x;
    launch.controller_team[0] = TECMO_GAMEPLAY_TEAM_HOME;
    launch.controller_team[1] = TECMO_GAMEPLAY_TEAM_AWAY;
    if (!tecmo_gameplay_scene_launch(scene, &launch) ||
        scene->controlled_actor[0] < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        scene->controlled_actor[1] >= TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "swapped controller ownership mapping failed");
        return false;
    }
    x = scene->actors[scene->controlled_actor[0]].position.x;
    memset(&p1, 0, sizeof(p1));
    p1.held.right = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->actors[scene->controlled_actor[0]].position.x != x ||
        !tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->actors[scene->controlled_actor[0]].position.x != x + 1) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "swapped controller TGMO movement failed");
        return false;
    }
    tecmo_gameplay_scene_end(scene);

    launch.controller_team[0] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch.controller_team[1] = TECMO_GAMEPLAY_TEAM_HOME;
    if (!tecmo_gameplay_scene_launch(scene, &launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "combined-button gameplay launch rejected");
        return false;
    }
    /* Keep the A-selected receiver inside the supported TGJS/TGSR slot-0 miss
       context so this test remains about A-before-B resolution. */
    scene->actors[1].position.x = 0x013CU;
    scene->actors[1].position.y = 180;
    scene->actors[1].facing_right = true;
    scene->action_serial = 1U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.held.shoot = true;
    p1.pressed.shoot = true;
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->shot_actor != 1U || scene->action_serial != 2U ||
        scene->jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "combined NES A+B resolution failed");
        return false;
    }
    tecmo_gameplay_scene_end(scene);
    *launch_input = launch;
    *p1_input = p1;
    *p2_input = p2;
    return true;
}

static bool scene_test_dribble_policy(
    TecmoGameplayScene *scene,
    TecmoGameplaySceneLaunch *launch_input,
    TecmoControlFrame *p1_input,
    TecmoControlFrame *p2_input,
    char *message,
    size_t message_size)
{
    TecmoGameplaySceneLaunch launch = *launch_input;
    TecmoControlFrame p1 = *p1_input;
    TecmoControlFrame p2 = *p2_input;
    int16_t cpu_holder_start_x;
    size_t frame;
    /* TGBD follows the actual holder's TGMO animation phase. Unrelated pad
       activity cannot trigger it; a stationary holder still dribbles. */
    if (!tecmo_gameplay_scene_launch(scene, &launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "defender dribble-policy launch rejected");
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p2.held.right = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        !tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->audio_player.dmc.active) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "defender movement queued holder DMC");
        return false;
    }
    tecmo_gameplay_scene_end(scene);

    launch.controller_team[1] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    if (!tecmo_gameplay_scene_launch(scene, &launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "NO_TEAM dribble-policy launch rejected");
        return false;
    }
    memset(&p2, 0, sizeof(p2));
    p2.held.right = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->audio_player.dmc.active) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "NO_TEAM pad movement queued holder DMC");
        return false;
    }
    tecmo_gameplay_scene_end(scene);

    if (!tecmo_gameplay_scene_launch(scene, &launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "human holder dribble-policy launch rejected");
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    for (frame = 0U; frame <= 14U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            (frame < 14U &&
             scene->audio_player.dmc.active)) {
            tecmo_gameplay_scene_test_message(
                message, message_size,
                "human holder TGBD phase queued an early DMC");
            return false;
        }
        if ((frame == 0U &&
             scene->ball_position.y_q8 != 176 * 256) ||
            (frame == 3U &&
             scene->ball_position.y_q8 != 182 * 256) ||
            (frame == 7U &&
             scene->ball_position.y_q8 != 191 * 256) ||
            (frame == 11U &&
             scene->ball_position.y_q8 != 197 * 256)) {
            tecmo_gameplay_scene_test_message(
                message, message_size,
                "human holder TGBD visible bounce vector failed");
            return false;
        }
    }
    if (!scene->audio_player.dmc.active) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "stationary human holder missed native TGBD phase DMC");
        return false;
    }
    tecmo_gameplay_scene_end(scene);

    launch.controller_team[0] = TECMO_GAMEPLAY_TEAM_HOME;
    if (!tecmo_gameplay_scene_launch(scene, &launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "CPU holder dribble-policy launch rejected");
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    cpu_holder_start_x = scene->actors[scene->ball_holder].position.x;
    for (frame = 0U; frame <= 17U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            (frame < 17U &&
             scene->audio_player.dmc.active)) {
            tecmo_gameplay_scene_test_message(
                message, message_size,
                "CPU holder TGBD phase queued an early DMC");
            return false;
        }
        if (frame == 0U &&
            (scene->actors[0].position.x != cpu_holder_start_x ||
             scene->actors[0].movement_action_state !=
                 TECMO_GAMEPLAY_MOVEMENT_INPUT_LEFT ||
             scene->cpu_actors[0].decision_serial != 1U ||
             scene->cpu_actors[0].snapshot_fingerprint != 0xBD36E345U ||
             scene->cpu_actors[0].target_kind !=
                 TECMO_GAMEPLAY_CPU_STEERING_HARNESS_HOOP_APPROACH ||
             scene->cpu_actors[0].target_position.x != 208 ||
             scene->cpu_actors[0].target_position.y != 148 ||
             scene->cpu_actors[0].direction != 1U ||
             scene->cpu_actors[0].held_direction_bits !=
                 TECMO_GAMEPLAY_MOVEMENT_INPUT_LEFT ||
             !scene->cpu_actors[0].writes_direction ||
            scene->cpu_actors[1].decision_serial != 1U ||
            scene->cpu_actors[1].target_kind !=
                 TECMO_GAMEPLAY_CPU_STEERING_HARNESS_EXPLICIT_TARGET ||
            scene->cpu_actors[1].linked_actor != 6U ||
            scene->cpu_actors[1].target_position.x != 288 ||
            scene->cpu_actors[1].target_position.y != 112 ||
            scene->cpu_actors[5].decision_serial != 0U)) {
            tecmo_gameplay_scene_test_message(
                message, message_size,
                "live TGAI snapshot/target/TGMO-latency contract failed");
            return false;
        }
    }
    if (!scene->audio_player.dmc.active ||
        scene->actors[0].position.x >= cpu_holder_start_x ||
        scene->cpu_actors[0].decision_serial !=
            18U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "CPU holder missed native TGBD phase DMC");
        return false;
    }
    tecmo_gameplay_scene_end(scene);
    launch.controller_team[0] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch.controller_team[1] = TECMO_GAMEPLAY_TEAM_HOME;
    *launch_input = launch;
    *p1_input = p1;
    *p2_input = p2;
    return true;
}

static bool scene_test_cpu_target_snapshot(
    const TecmoGameplayScene *scene,
    const TecmoGameplayCourtCoordinate
        snapshot[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT],
    size_t actor,
    uint8_t orientation,
    char *message,
    size_t message_size)
{
    static const TecmoGameplayCourtCoordinate formation_targets[5] = {
        {256, 148}, {288, 112}, {288, 184}, {352, 96}, {352, 200}
    };
    static const int8_t defender_depth_split[5] = {
        0, -10, 10, -14, 14
    };
    const TecmoGameplaySceneActor *item;
    const TecmoGameplaySceneCpuActor *cpu;
    const TecmoGameplayCourtCoordinate *linked;
    uint8_t linked_actor;
    int32_t expected_x;
    int32_t expected_y;
    int32_t goal_side;
    if (scene == NULL || snapshot == NULL ||
        actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        orientation >= TECMO_GAMEPLAY_COURT_ORIENTATION_COUNT) {
        return false;
    }
    item = &scene->actors[actor];
    cpu = &scene->cpu_actors[actor];
    linked_actor = cpu->linked_actor;
    if (item->roster_index >= TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        linked_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        linked_actor == actor) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "CPU formation snapshot target metadata setup failed");
        return false;
    }
    linked = &snapshot[linked_actor];
    if (item->team == (uint8_t)scene->state.possession) {
        expected_x = orientation == 0U
            ? formation_targets[item->roster_index].x
            : TECMO_GAMEPLAY_COURT_WORLD_MAX_X -
                formation_targets[item->roster_index].x;
        expected_y = formation_targets[item->roster_index].y;
    } else {
        goal_side = orientation == 0U ? -1 : 1;
        expected_x = (int32_t)linked->x + goal_side * 32;
        expected_y = (int32_t)linked->y +
            defender_depth_split[item->roster_index];
        {
            int32_t boundary_y = expected_y;
            bool goal_side_outside =
                expected_x < TECMO_GAMEPLAY_COURT_WORLD_MIN_X ||
                expected_x > TECMO_GAMEPLAY_COURT_WORLD_MAX_X;
            if (boundary_y < TECMO_GAMEPLAY_COURT_WORLD_MIN_Y) {
                boundary_y = TECMO_GAMEPLAY_COURT_WORLD_MIN_Y;
            } else if (boundary_y > TECMO_GAMEPLAY_COURT_WORLD_MAX_Y) {
                boundary_y = TECMO_GAMEPLAY_COURT_WORLD_MAX_Y;
            }
            if (!goal_side_outside &&
                (expected_x <
                     TECMO_GAMEPLAY_LEFT_BOUNDARY_BASE - boundary_y / 2 ||
                 expected_x >
                     TECMO_GAMEPLAY_RIGHT_BOUNDARY_BASE + boundary_y / 2)) {
                goal_side_outside = true;
            }
            if (goal_side_outside) {
                expected_x = (int32_t)linked->x - goal_side * 32;
            }
        }
    }
    if (expected_x < TECMO_GAMEPLAY_COURT_WORLD_MIN_X) {
        expected_x = TECMO_GAMEPLAY_COURT_WORLD_MIN_X;
    } else if (expected_x > TECMO_GAMEPLAY_COURT_WORLD_MAX_X) {
        expected_x = TECMO_GAMEPLAY_COURT_WORLD_MAX_X;
    }
    if (expected_y < TECMO_GAMEPLAY_COURT_WORLD_MIN_Y) {
        expected_y = TECMO_GAMEPLAY_COURT_WORLD_MIN_Y;
    } else if (expected_y > TECMO_GAMEPLAY_COURT_WORLD_MAX_Y) {
        expected_y = TECMO_GAMEPLAY_COURT_WORLD_MAX_Y;
    }
    if (!cpu->target_valid ||
        cpu->target_kind !=
            TECMO_GAMEPLAY_CPU_STEERING_HARNESS_EXPLICIT_TARGET ||
        cpu->target_position.x != (int16_t)expected_x ||
        cpu->target_position.y != (int16_t)expected_y) {
        char failure[256];
        (void)snprintf(
            failure, sizeof(failure),
            "CPU formation target snapshot mismatch: orientation=%u actor=%u expected=(%d,%d) actual=(%d,%d) linked=(%d,%d)",
            (unsigned)orientation, (unsigned)actor, (int)expected_x,
            (int)expected_y, (int)cpu->target_position.x,
            (int)cpu->target_position.y, (int)linked->x, (int)linked->y);
        tecmo_gameplay_scene_test_message(message, message_size, failure);
        return false;
    }
    return true;
}

static bool scene_test_cpu_formation_regression(
    TecmoGameplayScene *scene,
    TecmoGameplaySceneLaunch *launch_input,
    TecmoControlFrame *p1_input,
    TecmoControlFrame *p2_input,
    char *message,
    size_t message_size)
{
    TecmoGameplaySceneLaunch launch = *launch_input;
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    TecmoGameplayCpuSteeringHarnessInput zero_input;
    TecmoGameplayCpuSteeringHarnessResult zero_result;
    TecmoGameplayCourtCoordinate initial_positions[
        TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayCourtCoordinate pre_update_positions[
        TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    uint16_t equal_streak[TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT] = {0};
    uint16_t longest_equal_streak[
        TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT] = {0};
    bool meaningful_move[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT] = {0};
    size_t meaningful_count = 0U;
    size_t frame;
    size_t actor;
    size_t pair;

    if (scene == NULL || launch_input == NULL || p1_input == NULL ||
        p2_input == NULL) {
        return false;
    }
    launch.controller_team[0] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch.controller_team[1] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    launch.game_music_enabled = false;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_scene_launch(scene, &launch) ||
        scene->ball_holder != 0U || scene->controlled_actor[0] != 0U) {
        tecmo_gameplay_scene_test_message(
            message, message_size, "CPU formation regression launch rejected");
        return false;
    }

    /* A defender at the attacked boundary must keep a real 32-pixel target
       delta. Capture the linked coordinate before the update so this seam
       cannot accidentally validate against a post-movement actor position. */
    {
        TecmoGameplayCourtCoordinate linked_snapshot =
            scene->actors[0U].position;
        TecmoGameplayCourtCoordinate split_link_snapshot;
        int16_t expected_target_x;
        int16_t expected_split_target_x;
        char failure[192];
        scene->actors[0U].position.x = (int16_t)(
            TECMO_GAMEPLAY_LEFT_BOUNDARY_BASE - linked_snapshot.y / 2);
        scene->actors[0U].anchor = scene->actors[0U].position;
        linked_snapshot = scene->actors[0U].position;
        expected_target_x = (int16_t)(linked_snapshot.x + 32);
        scene->actors[1U].position.x = TECMO_GAMEPLAY_LEFT_BOUNDARY_BASE;
        scene->actors[1U].position.y = TECMO_GAMEPLAY_COURT_WORLD_MIN_Y;
        scene->actors[1U].anchor = scene->actors[1U].position;
        split_link_snapshot = scene->actors[1U].position;
        expected_split_target_x = (int16_t)(split_link_snapshot.x + 32);
        if (!scene_attach_ball(scene) ||
            !tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->cpu_actors[5U].target_position.x != expected_target_x ||
            scene->cpu_actors[5U].target_position.y != linked_snapshot.y ||
            scene->cpu_actors[6U].target_position.x !=
                expected_split_target_x ||
            scene->cpu_actors[6U].target_position.y !=
                TECMO_GAMEPLAY_COURT_WORLD_MIN_Y) {
            (void)snprintf(
                failure, sizeof(failure),
                "CPU defender boundary fallback failed: orientation=0 target=(%d,%d) linked=(%d,%d) split-target=(%d,%d)",
                (int)scene->cpu_actors[5U].target_position.x,
                (int)scene->cpu_actors[5U].target_position.y,
                (int)linked_snapshot.x, (int)linked_snapshot.y,
                (int)scene->cpu_actors[6U].target_position.x,
                (int)scene->cpu_actors[6U].target_position.y);
            tecmo_gameplay_scene_test_message(message, message_size, failure);
            return false;
        }
    }
    tecmo_gameplay_scene_end(scene);

    launch.controller_team[0] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    launch.controller_team[1] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    if (!tecmo_gameplay_scene_launch(scene, &launch) ||
        !scene_handoff_possession(scene, TECMO_GAMEPLAY_TEAM_HOME, 5U)) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "CPU defender orientation-1 boundary setup rejected");
        return false;
    }
    {
        TecmoGameplayCourtCoordinate linked_snapshot =
            scene->actors[5U].position;
        int16_t expected_target_x;
        char failure[192];
        scene->actors[5U].position.x = (int16_t)(
            TECMO_GAMEPLAY_RIGHT_BOUNDARY_BASE + linked_snapshot.y / 2);
        scene->actors[5U].anchor = scene->actors[5U].position;
        linked_snapshot = scene->actors[5U].position;
        expected_target_x = (int16_t)(linked_snapshot.x - 32);
        if (!scene_attach_ball(scene) ||
            !tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->cpu_actors[0U].target_position.x != expected_target_x ||
            scene->cpu_actors[0U].target_position.y != linked_snapshot.y) {
            (void)snprintf(
                failure, sizeof(failure),
                "CPU defender boundary fallback failed: orientation=1 target=(%d,%d) linked=(%d,%d)",
                (int)scene->cpu_actors[0U].target_position.x,
                (int)scene->cpu_actors[0U].target_position.y,
                (int)linked_snapshot.x, (int)linked_snapshot.y);
            tecmo_gameplay_scene_test_message(message, message_size, failure);
            return false;
        }
    }
    tecmo_gameplay_scene_end(scene);

    launch.controller_team[0] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch.controller_team[1] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    if (!tecmo_gameplay_scene_launch(scene, &launch) ||
        scene->ball_holder != 0U || scene->controlled_actor[0] != 0U) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "CPU formation regression relaunch rejected");
        return false;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        initial_positions[actor] = scene->actors[actor].position;
    }

    for (frame = 0U; frame < 160U; ++frame) {
        for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
            pre_update_positions[actor] = scene->actors[actor].position;
        }
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE) {
            tecmo_gameplay_scene_test_message(
                message, message_size,
                "CPU formation regression live update rejected");
            return false;
        }
        if (scene->actors[0].position.x != initial_positions[0].x ||
            scene->actors[0].position.y != initial_positions[0].y) {
            tecmo_gameplay_scene_test_message(
                message, message_size,
                "CPU policy moved the neutral human-controlled actor");
            return false;
        }
        for (actor = 1U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
             ++actor) {
            int32_t dx = (int32_t)scene->actors[actor].position.x -
                         initial_positions[actor].x;
            int32_t dy = (int32_t)scene->actors[actor].position.y -
                         initial_positions[actor].y;
            const TecmoGameplaySceneCpuActor *cpu =
                &scene->cpu_actors[actor];
            uint8_t expected_link = actor <
                TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT
                    ? (uint8_t)(actor +
                        TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT)
                    : (uint8_t)(actor -
                        TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT);
            if (dx < 0) dx = -dx;
            if (dy < 0) dy = -dy;
            if (dx + dy >= 12 && !meaningful_move[actor]) {
                meaningful_move[actor] = true;
                ++meaningful_count;
            }
            if (cpu->decision_serial != frame + 1U || !cpu->target_valid ||
                cpu->linked_actor != expected_link ||
                cpu->target_kind >=
                    TECMO_GAMEPLAY_CPU_STEERING_HARNESS_TARGET_KIND_COUNT ||
                !tecmo_gameplay_court_coordinate_valid(
                    &cpu->target_position) ||
                !tecmo_gameplay_movement_input_valid(
                    cpu->held_direction_bits) ||
                (!cpu->writes_direction &&
                 (cpu->direction !=
                      TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION ||
                  cpu->held_direction_bits !=
                      TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL)) ||
                (cpu->writes_direction &&
                 cpu->direction >=
                     TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT)) {
                tecmo_gameplay_scene_test_message(
                    message, message_size,
                    "CPU formation target metadata lost coherence");
                return false;
            }
            if (!scene_test_cpu_target_snapshot(
                    scene, pre_update_positions, actor, 0U,
                    message, message_size)) {
                return false;
            }
        }
        for (pair = 0U; pair < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT;
             ++pair) {
            size_t away = pair;
            size_t home = pair + TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT;
            bool equal = scene->actors[away].position.x ==
                             scene->actors[home].position.x &&
                         scene->actors[away].position.y ==
                             scene->actors[home].position.y;
            bool neutral = !scene->cpu_actors[home].writes_direction &&
                scene->cpu_actors[home].held_direction_bits ==
                    TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
            if (away != 0U) {
                neutral = neutral &&
                    !scene->cpu_actors[away].writes_direction &&
                    scene->cpu_actors[away].held_direction_bits ==
                        TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
            }
            if (equal && neutral) {
                ++equal_streak[pair];
                if (equal_streak[pair] > longest_equal_streak[pair]) {
                    longest_equal_streak[pair] = equal_streak[pair];
                }
            } else {
                equal_streak[pair] = 0U;
            }
        }
    }
    if (!scene_handoff_possession(
            scene, TECMO_GAMEPLAY_TEAM_AWAY, 1U)) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "CPU orientation-0 formation-slot-0 setup rejected");
        return false;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        pre_update_positions[actor] = scene->actors[actor].position;
    }
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "CPU orientation-0 formation-slot-0 update rejected");
        return false;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        if (actor != 1U &&
            !scene_test_cpu_target_snapshot(
                scene, pre_update_positions, actor, 0U,
                message, message_size)) {
            return false;
        }
    }
    if (scene->orientation_state.current_direction != 0U ||
        scene->cpu_actors[5U].target_kind !=
            TECMO_GAMEPLAY_CPU_STEERING_HARNESS_EXPLICIT_TARGET ||
        scene->cpu_actors[5U].linked_actor != 0U ||
        scene->cpu_actors[5U].target_position.x !=
            initial_positions[0U].x - 32) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "CPU defender orientation-0 goal-side split failed");
        return false;
    }
    if (meaningful_count < 3U) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "CPU formation regression did not move multiple actors");
        return false;
    }
    for (pair = 0U; pair < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT;
         ++pair) {
        if (longest_equal_streak[pair] >= 12U) {
            tecmo_gameplay_scene_test_message(
                message, message_size,
                "CPU fixed pair entered a sustained neutral collapse plateau");
            return false;
        }
    }

    memset(&zero_input, 0, sizeof(zero_input));
    zero_input.contract_tag = TECMO_GAMEPLAY_CPU_STEERING_HARNESS_INPUT_TAG;
    zero_input.actor = 5U;
    zero_input.possession = (uint8_t)scene->state.possession;
    zero_input.orientation = scene->orientation_state.current_direction;
    zero_input.ball_holder = scene->ball_holder;
    zero_input.matchup_actor = 0U;
    zero_input.difficulty = scene->launch.difficulty;
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        zero_input.actor_position[actor] = scene->actors[actor].position;
    }
    zero_input.actor_position[5U] = zero_input.actor_position[0U];
    if (!tecmo_gameplay_cpu_steering_harness_evaluate(
            &scene->cpu_steering_assets, &zero_input, &zero_result) ||
        zero_result.horizontal_delta != 0 ||
        zero_result.depth_delta != 0 || zero_result.writes_direction ||
        zero_result.direction != TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION ||
        zero_result.target_kind !=
            TECMO_GAMEPLAY_CPU_STEERING_HARNESS_LINKED_ACTOR) {
        tecmo_gameplay_scene_test_message(
            message, message_size, "CPU zero-vector no-write seam regressed");
        return false;
    }
    tecmo_gameplay_scene_end(scene);
    launch.controller_team[0] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    launch.controller_team[1] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    if (!tecmo_gameplay_scene_launch(scene, &launch) ||
        !scene_handoff_possession(
            scene, TECMO_GAMEPLAY_TEAM_HOME, 5U)) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "CPU defender orientation-1 setup rejected");
        return false;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        pre_update_positions[actor] = scene->actors[actor].position;
    }
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->orientation_state.current_direction != 1U ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "CPU orientation-1 formation update rejected");
        return false;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        if (actor != 5U &&
            !scene_test_cpu_target_snapshot(
                scene, pre_update_positions, actor, 1U,
                message, message_size)) {
            return false;
        }
    }
    if (!scene_handoff_possession(
            scene, TECMO_GAMEPLAY_TEAM_HOME, 6U)) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "CPU orientation-1 formation-slot-0 setup rejected");
        return false;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        pre_update_positions[actor] = scene->actors[actor].position;
    }
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->orientation_state.current_direction != 1U ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "CPU orientation-1 formation-slot-0 update rejected");
        return false;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        if (actor != 6U &&
            !scene_test_cpu_target_snapshot(
                scene, pre_update_positions, actor, 1U,
                message, message_size)) {
            return false;
        }
    }
    tecmo_gameplay_scene_end(scene);
    launch.controller_team[0] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch.controller_team[1] = TECMO_GAMEPLAY_TEAM_HOME;
    *launch_input = launch;
    *p1_input = p1;
    *p2_input = p2;
    return true;
}

static bool scene_test_music_and_steal_policy(
    TecmoGameplayScene *scene,
    TecmoGameplaySceneLaunch *launch_input,
    TecmoControlFrame *p1_input,
    TecmoControlFrame *p2_input,
    char *message,
    size_t message_size)
{
    TecmoGameplaySceneLaunch launch = *launch_input;
    TecmoControlFrame p1 = *p1_input;
    TecmoControlFrame p2 = *p2_input;
    size_t frame;
    launch.game_music_enabled = false;
    if (!tecmo_gameplay_scene_launch(scene, &launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "music-off restart launch rejected");
        return false;
    }
    scene->state.shot_clock = 1U;
    scene->state.clock_divider = 1U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
        !scene->audio_player.sfx_pending ||
        scene->audio_player.pending_sfx_id != 6U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "music-off violation entry failed");
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene->audio_player, NULL, 1024U);
    if (scene->audio_player.sfx_pending) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "music-off violation cue was not consumed");
        return false;
    }
    for (frame = 0U;
         frame < TECMO_GAMEPLAY_VIOLATION_RELEASE_LEAD_IN_FRAMES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "music-off violation lead-in failed");
            return false;
        }
    }
    p1.released.shoot = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->audio_player.sfx_pending) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "music-off restart queued neutral cue");
        return false;
    }
    tecmo_gameplay_scene_end(scene);
    launch.game_music_enabled = true;

    if (!tecmo_gameplay_scene_launch(scene, &launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "native steal-policy launch rejected");
        return false;
    }
    scene->actors[scene->controlled_actor[1]].position.x =
        scene->actors[scene->ball_holder].position.x + 1;
    scene->actors[scene->controlled_actor[1]].position.y =
        scene->actors[scene->ball_holder].position.y;
    scene->action_serial = 1U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p2.held.cancel = true;
    p2.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->action_serial != 2U ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        scene->ball_holder != scene->controlled_actor[1]) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "native action-serial steal policy diverged");
        return false;
    }
    tecmo_gameplay_scene_end(scene);
    *launch_input = launch;
    *p1_input = p1;
    *p2_input = p2;
    return true;
}

static bool scene_test_foul_and_away_free_throws(
    TecmoGameplayScene *scene,
    TecmoGameplaySceneLaunch *launch_input,
    TecmoControlFrame *p1_input,
    TecmoControlFrame *p2_input,
    char *message,
    size_t message_size)
{
    TecmoGameplaySceneLaunch launch = *launch_input;
    TecmoControlFrame p1 = *p1_input;
    TecmoControlFrame p2 = *p2_input;
    size_t frame;
    if (!tecmo_gameplay_scene_launch(scene, &launch) ||
        !scene_test_free_throw_lineup_unbound(scene)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "foul/free-throw gameplay launch rejected");
        return false;
    }
    scene->actors[scene->controlled_actor[1]].position.x =
        scene->actors[scene->ball_holder].position.x + 1;
    scene->actors[scene->controlled_actor[1]].position.y =
        scene->actors[scene->ball_holder].position.y;
    scene->action_serial = 3U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p2.held.cancel = true;
    p2.pressed.cancel = true;
    if (!tecmo_gameplay_audio_queue_dmc_clip(
            &scene->audio_player,
            TECMO_GAMEPLAY_DMC_BANK05_A8D6_LONG) ||
        !tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION ||
        scene->state.team_fouls[TECMO_GAMEPLAY_TEAM_HOME] != 1U ||
        scene->state.individual_fouls[TECMO_GAMEPLAY_TEAM_HOME][0] != 1U ||
        scene->action_serial != 4U ||
        scene->audio_player.sfx_pending || scene->audio_player.dmc.active ||
        scene->audio_player.music == NULL ||
        scene->audio_player.music->playing ||
        scene->audio_player.music->track_pending) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "foul entry audio reset/policy diverged");
        return false;
    }
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_audio_queue_dmc_clip(
            &scene->audio_player,
            TECMO_GAMEPLAY_DMC_BANK05_A8D6_LONG)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "foul exact-once DMC probe failed");
        return false;
    }
    for (frame = 0U; frame < TECMO_GAMEPLAY_PRESENTATION_LEAD_IN_FRAMES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            !scene->audio_player.dmc.active) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "foul audio reset repeated after entry");
            return false;
        }
    }
    p1.released.shoot = true;
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
        scene->state.free_throws.attempts_remaining != 2U ||
        scene->free_throw_frame != 0U || scene->audio_player.sfx_pending ||
        !scene_test_free_throw_lineup_bound(
            scene, 0U, 0U, 5U,
            TECMO_GAMEPLAY_FREE_THROW_ORIENTATION_0_CAMERA_X) ||
        scene->audio_player.music == NULL ||
        !scene->audio_player.music->track_pending ||
        scene->audio_player.music->pending_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "foul dismissal/free-throw handoff failed");
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene->audio_player, NULL, 1024U);
    if (scene->audio_player.sfx_pending ||
        !scene->audio_player.music->playing ||
        scene->audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY ||
        scene->audio_player.music->track_pending) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "free-throw setup music was not consumed");
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
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
        scene->state.free_throws.attempts_remaining != 2U ||
        scene->free_throw_frame != 0U || scene->action_serial != 4U ||
        scene->audio_player.sfx_pending ||
        !tecmo_gameplay_state_valid(&scene->state)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "non-owner/free-throw non-B input launched");
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    for (frame = 0U;
         frame < TECMO_GAMEPLAY_FREE_THROW_CPU_OBSERVED_LAUNCH_UPDATES * 2U;
         ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
            scene->state.free_throws.attempts_remaining != 2U ||
            scene->free_throw_frame != 0U || scene->action_serial != 4U ||
            scene->audio_player.sfx_pending ||
            !tecmo_gameplay_state_valid(&scene->state)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "human free throw gained a timer fallback");
            return false;
        }
    }
    p1.held.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
        scene->state.free_throws.attempts_remaining != 1U ||
        scene->free_throw_frame != 0U || scene->action_serial != 5U ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY] != 1U ||
        !scene_test_free_throw_lineup_bound(
            scene, 0U, 0U, 5U,
            TECMO_GAMEPLAY_FREE_THROW_ORIENTATION_0_CAMERA_X) ||
        !scene->audio_player.sfx_pending ||
        scene->audio_player.pending_sfx_id != 12U ||
        !tecmo_gameplay_state_valid(&scene->state)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "owned held-B free throw did not launch");
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene->audio_player, NULL, 1024U);
    if (scene->audio_player.sfx_pending ||
        scene->audio_player.current_sfx_id != 12U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "made away free-throw mailbox was not side-result 12");
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
        scene->state.free_throws.attempts_remaining != 1U ||
        scene->free_throw_frame != 0U || scene->action_serial != 5U ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY] != 1U ||
        !tecmo_gameplay_state_valid(&scene->state)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pressed-only free throw input launched");
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    p1.held.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->state.free_throws.attempts_remaining != 0U ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        scene->free_throw_frame != 0U || scene->action_serial != 6U ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY] != 2U ||
        scene->ball_holder < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        scene->controlled_actor[1] != scene->ball_holder ||
        !scene->audio_player.sfx_pending ||
        scene->audio_player.pending_sfx_id != 12U ||
        scene->audio_player.music == NULL ||
        !scene->audio_player.music->playing ||
        scene->audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY ||
        scene->audio_player.music->track_pending ||
        !scene_test_free_throw_lineup_unbound(scene) ||
        !tecmo_gameplay_state_valid(&scene->state)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "human free-throw settlement failed");
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene->audio_player, NULL, 1024U);
    memset(&p1, 0, sizeof(p1));
    if (scene->audio_player.sfx_pending ||
        scene->audio_player.current_sfx_id != 12U ||
        !scene->audio_player.music->playing ||
        scene->audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY ||
        scene->audio_player.music->track_pending ||
        !tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->audio_player.sfx_pending ||
        scene->audio_player.current_sfx_id != 12U ||
        scene->audio_player.music->track_pending ||
        scene->audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "final human free-throw audio repeated or missing");
        return false;
    }
    scene->free_throw_frame = 7U;
    tecmo_gameplay_scene_end(scene);
    if (scene->free_throw_frame != 0U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "free-throw timer survived scene end");
        return false;
    }
    *launch_input = launch;
    *p1_input = p1;
    *p2_input = p2;
    return true;
}

static bool scene_test_home_and_cpu_free_throws(
    TecmoGameplayScene *scene,
    TecmoGameplaySceneLaunch *launch_input,
    TecmoControlFrame *p1_input,
    TecmoControlFrame *p2_input,
    char *message,
    size_t message_size)
{
    TecmoGameplaySceneLaunch launch = *launch_input;
    TecmoControlFrame p1 = *p1_input;
    TecmoControlFrame p2 = *p2_input;
    size_t frame;
    /* Home ownership uses its assigned pad, independently of controller index. */
    if (!tecmo_gameplay_scene_launch(scene, &launch) ||
        scene->free_throw_frame != 0U ||
        !scene_test_enter_free_throw_sequence(
            scene, TECMO_GAMEPLAY_TEAM_HOME, 1U)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "home free-throw ownership setup failed");
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.held.cancel = true;
    p2.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
        scene->state.free_throws.attempts_remaining != 1U ||
        scene->action_serial != 0U || scene->free_throw_frame != 0U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "home free throw accepted the wrong pad/edge");
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p2.held.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->state.free_throws.attempts_remaining != 0U ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_HOME] != 1U ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        scene->controlled_actor[0] != scene->ball_holder ||
        scene->action_serial != 1U || scene->free_throw_frame != 0U ||
        !scene_test_free_throw_lineup_unbound(scene) ||
        !scene->audio_player.sfx_pending ||
        scene->audio_player.pending_sfx_id != 13U ||
        scene->audio_player.music == NULL ||
        !scene->audio_player.music->playing ||
        scene->audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY ||
        scene->audio_player.music->track_pending ||
        !tecmo_gameplay_state_valid(&scene->state)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "home owned held-B free throw failed");
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene->audio_player, NULL, 1024U);
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (scene->audio_player.sfx_pending ||
        scene->audio_player.current_sfx_id != 13U ||
        scene->audio_player.music->track_pending ||
        !tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->audio_player.sfx_pending ||
        scene->audio_player.current_sfx_id != 13U ||
        scene->audio_player.music->track_pending ||
        scene->audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "final home free-throw audio repeated or missing");
        return false;
    }
    tecmo_gameplay_scene_end(scene);

    /* With no controller assigned to the scoring side, use the observed
       125-update launch schedule and reset it for the following attempt. */
    launch.controller_team[1] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    if (!tecmo_gameplay_scene_launch(scene, &launch) ||
        !scene_test_enter_free_throw_sequence(
            scene, TECMO_GAMEPLAY_TEAM_HOME, 2U)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "CPU free-throw timer setup failed");
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    for (frame = 0U;
         frame + 1U <
             TECMO_GAMEPLAY_FREE_THROW_CPU_OBSERVED_LAUNCH_UPDATES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
            scene->state.free_throws.attempts_remaining != 2U ||
            scene->free_throw_frame != frame + 1U ||
            scene->action_serial != 0U || scene->audio_player.sfx_pending ||
            !tecmo_gameplay_state_valid(&scene->state)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "CPU free throw launched before observed schedule");
            return false;
        }
    }
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
        scene->state.free_throws.attempts_remaining != 1U ||
        scene->free_throw_frame != 0U || scene->action_serial != 1U ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_HOME] != 0U ||
        !scene_test_free_throw_lineup_bound(
            scene, 1U, 5U, 0U,
            TECMO_GAMEPLAY_FREE_THROW_ORIENTATION_1_CAMERA_X) ||
        !scene->audio_player.sfx_pending ||
        scene->audio_player.pending_sfx_id != 13U ||
        !tecmo_gameplay_state_valid(&scene->state)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "CPU free throw missed observed launch update");
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene->audio_player, NULL, 1024U);
    if (scene->audio_player.sfx_pending ||
        scene->audio_player.current_sfx_id != 13U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "missed home free-throw mailbox was not side-result 13");
        return false;
    }
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.free_throws.attempts_remaining != 1U ||
        scene->free_throw_frame != 1U || scene->action_serial != 1U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "second CPU free-throw timer did not reset");
        return false;
    }
    for (frame = 1U;
         frame + 1U <
             TECMO_GAMEPLAY_FREE_THROW_CPU_OBSERVED_LAUNCH_UPDATES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
            scene->state.free_throws.attempts_remaining != 1U ||
            scene->free_throw_frame != frame + 1U ||
            scene->action_serial != 1U || scene->audio_player.sfx_pending) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "second CPU free throw launched early");
            return false;
        }
    }
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->state.free_throws.attempts_remaining != 0U ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_HOME] != 0U ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        scene->controlled_actor[0] != scene->ball_holder ||
        scene->free_throw_frame != 0U || scene->action_serial != 2U ||
        !scene_test_free_throw_lineup_unbound(scene) ||
        !scene->audio_player.sfx_pending ||
        scene->audio_player.pending_sfx_id != 13U ||
        scene->audio_player.music == NULL ||
        !scene->audio_player.music->playing ||
        scene->audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY ||
        scene->audio_player.music->track_pending ||
        !tecmo_gameplay_state_valid(&scene->state)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "CPU free-throw settlement failed");
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene->audio_player, NULL, 1024U);
    if (scene->audio_player.sfx_pending ||
        scene->audio_player.current_sfx_id != 13U ||
        !scene->audio_player.music->playing ||
        scene->audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY ||
        scene->audio_player.music->track_pending ||
        !tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->audio_player.sfx_pending ||
        scene->audio_player.current_sfx_id != 13U ||
        scene->audio_player.music->track_pending ||
        scene->audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "final CPU free-throw audio repeated or missing");
        return false;
    }
    tecmo_gameplay_scene_end(scene);
    launch.controller_team[1] = TECMO_GAMEPLAY_TEAM_HOME;
    *launch_input = launch;
    *p1_input = p1;
    *p2_input = p2;
    return true;
}

static bool scene_test_halftime_and_final_result(
    TecmoGameplayScene *scene,
    TecmoGameplaySceneLaunch *launch_input,
    TecmoControlFrame *p1_input,
    TecmoControlFrame *p2_input,
    char *message,
    size_t message_size)
{
    TecmoGameplaySceneLaunch launch = *launch_input;
    TecmoControlFrame p1 = *p1_input;
    TecmoControlFrame p2 = *p2_input;
    TecmoGameplaySceneResult result;
    size_t frame;
    if (!tecmo_gameplay_scene_launch(scene, &launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "halftime gameplay launch rejected");
        return false;
    }
    scene->state.period = 2U;
    scene->state.clock_minutes = 0U;
    scene->state.clock_seconds = 1U;
    scene->state.clock_divider = 1U;
    scene->state.shot_clock = 12U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_state_valid(&scene->state) ||
        !tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_FIXED_WAIT) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "halftime expiry entry failed");
        return false;
    }
    for (frame = 0U; frame < 40U &&
         scene->state.phase ==
             TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_FIXED_WAIT; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "halftime expiry wait failed");
            return false;
        }
    }
    if (scene->state.phase != TECMO_GAMEPLAY_PHASE_HALFTIME_BANNER) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "halftime banner transition failed");
        return false;
    }
    for (frame = 0U; frame < TECMO_GAMEPLAY_HALFTIME_BANNER_FRAMES &&
         scene->state.phase == TECMO_GAMEPLAY_PHASE_HALFTIME_BANNER; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "halftime banner update failed");
            return false;
        }
    }
    if (scene->state.phase != TECMO_GAMEPLAY_PHASE_HALFTIME_SCORE_SCREEN ||
        !scene->audio_player.music->track_pending ||
        scene->audio_player.music->pending_track_id !=
            TECMO_MUSIC_TRACK_PRESENTATION) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "halftime score/music transition failed");
        return false;
    }
    p1.released.shoot = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
        scene->state.banner != TECMO_GAMEPLAY_BANNER_THIRD_PERIOD) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "halftime dismissal failed");
        return false;
    }
    tecmo_gameplay_scene_end(scene);

    if (!tecmo_gameplay_scene_launch(scene, &launch) ||
        !tecmo_gameplay_set_score(&scene->state,
                                  TECMO_GAMEPLAY_TEAM_AWAY, 4U) ||
        !tecmo_gameplay_set_score(&scene->state,
                                  TECMO_GAMEPLAY_TEAM_HOME, 2U)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "final gameplay setup failed");
        return false;
    }
    scene->state.period = 4U;
    scene->state.clock_minutes = 0U;
    scene->state.clock_seconds = 1U;
    scene->state.clock_divider = 1U;
    scene->state.shot_clock = 12U;
    memset(&p1, 0, sizeof(p1));
    if (!tecmo_gameplay_state_valid(&scene->state) ||
        !tecmo_gameplay_scene_update(scene, &p1, &p2)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "final expiry entry failed");
        return false;
    }
    for (frame = 0U; frame < 40U &&
         scene->state.phase ==
             TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_FIXED_WAIT; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "final expiry wait failed");
            return false;
        }
    }
    if (scene->state.phase != TECMO_GAMEPLAY_PHASE_FINAL_SCORE_SCREEN) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "final score transition failed");
        return false;
    }
    p1.released.shoot = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        !tecmo_gameplay_scene_result(scene, &result) ||
        result.source != launch.source ||
        result.game_index != launch.game_index ||
        result.away_team != launch.away_team ||
        result.home_team != launch.home_team ||
        result.away_score != 4U || result.home_score != 2U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "final result handoff failed");
        return false;
    }
    tecmo_gameplay_scene_end(scene);
    *launch_input = launch;
    *p1_input = p1;
    *p2_input = p2;
    return true;
}

bool tecmo_gameplay_scene_test_state_flow(
    TecmoGameplaySceneTestContext *test)
{
    TecmoGameplayScene *scene = test->scene;
    TecmoGameplaySceneLaunch launch = test->launch;
    TecmoControlFrame p1 = test->p1;
    TecmoControlFrame p2 = test->p2;
    char *message = test->message;
    size_t message_size = test->message_size;

    if (!scene_test_violations_and_cpu_offense(
            scene, &launch, &p1, &p2, message, message_size) ||
        !scene_test_period_expiry_and_restart(
            scene, &launch, &p1, &p2, message, message_size) ||
        !scene_test_controller_policy(
            scene, &launch, &p1, &p2, message, message_size) ||
        !scene_test_dribble_policy(
            scene, &launch, &p1, &p2, message, message_size) ||
        !scene_test_cpu_formation_regression(
            scene, &launch, &p1, &p2, message, message_size) ||
        !scene_test_music_and_steal_policy(
            scene, &launch, &p1, &p2, message, message_size) ||
        !scene_test_foul_and_away_free_throws(
            scene, &launch, &p1, &p2, message, message_size) ||
        !scene_test_home_and_cpu_free_throws(
            scene, &launch, &p1, &p2, message, message_size) ||
        !scene_test_halftime_and_final_result(
            scene, &launch, &p1, &p2, message, message_size)) {
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    tecmo_gameplay_scene_destroy(scene);
    test->launch = launch;
    test->p1 = p1;
    test->p2 = p2;
    return true;
}
